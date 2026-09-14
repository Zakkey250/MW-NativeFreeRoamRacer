    {
        struct Node {alignas(4) std::array<unsigned char,0x80> bytes{};};
        Node group,nested,a,b,image,outside;
        std::array<wchar_t,256> aText{},bText{},outsideText{};
        auto field=[](Node& n,std::size_t at,auto value){std::memcpy(n.bytes.data()+at,&value,sizeof(value));};
        auto node=[](Node& n)->void*{return n.bytes.data();};
        field(group,0x18,5u);field(nested,0x18,5u);field(image,0x18,1u);
        for(auto* n:{&a,&b,&outside}) {field(*n,0x18,2u);field(*n,0x68,256u);}
        field(a,0x64,aText.data());field(b,0x64,bText.data());field(outside,0x64,outsideText.data());
        field(group,0x64,node(nested));field(group,4,node(outside));
        field(nested,0x64,node(a));field(nested,4,node(image));field(a,4,node(b));
        const auto groupBefore=group.bytes,nestedBefore=nested.bytes,imageBefore=image.bytes,outsideBefore=outside.bytes;
        std::size_t calls=0,written=0;
        auto setter=[&](void* string,const wchar_t* value) {
            wchar_t* buffer=nullptr;std::memcpy(&buffer,string,4);++calls;
            wcscpy_s(buffer,256,value);
        };
        expect(WriteEncounterMessageTree(node(group),encounter_text::Get(encounter_text::Id::Start,false),setter,written)&&written==2&&calls==2,
            "alpha28 group resolves nested type2 text leaves only");
        expect(group.bytes==groupBefore&&nested.bytes==nestedBefore&&image.bytes==imageBefore&&outside.bytes==outsideBefore&&outsideText[0]==0,
            "alpha28 preserves all group fields, images and root siblings byte-for-byte");
        expect(wcscmp(aText.data(),encounter_text::Get(encounter_text::Id::Start,false))==0&&wcscmp(aText.data(),bText.data())==0,
            "alpha28 exact Japanese message reaches both text layers");
        unsigned flags=0;std::memcpy(&flags,a.bytes.data()+0x1C,4);
        expect((flags&0x00400002)==0x00400002,"alpha28 dirty flag applied to text only");
        calls=0;field(b,4,node(a));
        expect(!WriteEncounterMessageTree(node(group),L"cycle",setter,written)&&calls==0&&written==0,
            "alpha28 sibling cycle rejected before any text mutation");
        field(b,4,static_cast<void*>(nullptr));field(image,0x18,0x958B30EBu);
        expect(!WriteEncounterMessageTree(node(group),L"corrupt",setter,written)&&calls==0,
            "alpha28 crash-signature type corruption rejects whole subtree");
        field(image,0x18,1u);field(nested,0x64,reinterpret_cast<void*>(0xEEEEEEEE));
        expect(!WriteEncounterMessageTree(node(group),L"unreadable",setter,written)&&calls==0,
            "alpha28 unreadable child rejected without native setter");
        field(nested,0x64,node(a));field(b,0x68,0x28A041B8u);
        expect(!WriteEncounterMessageTree(node(group),L"invalid-capacity",setter,written)&&calls==0,
            "alpha28 invalid bString capacity fails before partial writes");
        field(b,0x68,256u);
        expect(WriteEncounterMessageTree(node(a),L"direct leaf",setter,written)&&written==1&&calls==1,
            "alpha28 direct text root does not alter its sibling");
        calls=0;field(nested,0x64,static_cast<void*>(nullptr));
        expect(!WriteEncounterMessageTree(node(group),L"empty",setter,written)&&calls==0,
            "alpha28 group with no text refuses bString writes");
        field(nested,0x64,node(a));
        for(unsigned i=0;i<5;++i) for(bool english:{false,true}) {
            const auto* value=encounter_text::Get(static_cast<encounter_text::Id>(i),english);
            expect(WriteEncounterMessageTree(node(group),value,setter,written)&&wcscmp(aText.data(),value)==0&&wcscmp(bText.data(),value)==0,
                "alpha28 each battle message and language traverses safely");
        }
        expect(group.bytes==groupBefore&&nested.bytes==nestedBefore,"alpha28 repeated notifications preserve child links");
        std::array<Node,129> chain{};
        for(std::size_t i=0;i<chain.size();++i) {
            field(chain[i],0x18,5u);
            if(i+1<chain.size()) field(chain[i],0x64,node(chain[i+1]));
        }
        calls=0;
        expect(!WriteEncounterMessageTree(node(chain[0]),L"deep",setter,written)&&calls==0,"alpha28 bounded node budget rejects excessive nesting");
    }
