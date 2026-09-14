// Exact MW FE layout: type 5 is a GROUP (+64 first child), type 2 is a
// STRING (+64 UTF-16 bString). Never pass a group to the bString setter.
// Native FEPrintf 00525328..00525398 makes this same distinction.
bool EncounterMessageWritable(std::uintptr_t address,std::size_t size) noexcept {
    if(!address||!size||address+size<address) return false;
    const auto end=address+size;
    while(address<end) {
        MEMORY_BASIC_INFORMATION page{};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))||page.State!=MEM_COMMIT||
            (page.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
        const auto protection=page.Protect&0xFF;
        if(protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&
            protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto next=reinterpret_cast<std::uintptr_t>(page.BaseAddress)+page.RegionSize;
        if(next<=address) return false;
        address=next;
    }
    return true;
}

struct EncounterMessageLeaves {std::array<void*,16> values{};std::size_t count=0;};
bool CollectEncounterMessageLeaves(void* root,EncounterMessageLeaves& result) noexcept {
    result={};
    std::array<void*,128> pending{},visited{};
    EncounterMessageLeaves found{};
    std::size_t queued=1,seen=0;pending[0]=root;
    while(queued) {
        void* object=pending[--queued];
        if(!object||seen==visited.size()||
            std::find(visited.begin(),visited.begin()+seen,object)!=visited.begin()+seen) return false;
        visited[seen++]=object;
        auto* bytes=static_cast<unsigned char*>(object);
        unsigned type=0;void* next=nullptr;
        if(!AudioRead(bytes+0x18,&type)||type<1||type>5) return false;
        // The root's siblings belong to other messages. Only walk its subtree.
        if(object!=root) {
            if(!AudioRead(bytes+4,&next)) return false;
            if(next) {if(queued==pending.size()) return false;pending[queued++]=next;}
        }
        if(type==5) {
            void* child=nullptr;
            if(!AudioRead(bytes+0x64,&child)) return false;
            if(child) {if(queued==pending.size()) return false;pending[queued++]=child;}
        } else if(type==2) {
            wchar_t* buffer=nullptr;unsigned capacity=0;
            if(found.count==found.values.size()||
                !EncounterMessageWritable(reinterpret_cast<std::uintptr_t>(object),0x6C)||
                !AudioRead(bytes+0x64,&buffer)||!AudioRead(bytes+0x68,&capacity)||capacity>65536||
                ((buffer==nullptr)!=(capacity==0))||
                (buffer&&!EncounterMessageWritable(reinterpret_cast<std::uintptr_t>(buffer),capacity*sizeof(wchar_t)))) return false;
            found.values[found.count++]=object;
        }
    }
    if(!found.count) return false;
    result=found;return true;
}

template<class Setter>
bool WriteEncounterMessageTree(void* root,const wchar_t* text,Setter setter,std::size_t& written) {
    written=0;EncounterMessageLeaves leaves{};
    // Validate the ENTIRE bounded subtree before modifying even one leaf.
    if(!text||!CollectEncounterMessageLeaves(root,leaves)) return false;
    for(std::size_t i=0;i<leaves.count;++i) {
        auto* bytes=static_cast<unsigned char*>(leaves.values[i]);
        setter(bytes+0x64,text);
        *reinterpret_cast<unsigned*>(bytes+0x1C)|=0x00400002;
        ++written;
    }
    return true;
}
