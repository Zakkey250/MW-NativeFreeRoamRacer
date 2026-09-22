namespace {
struct SolidFixture {
    alignas(16) std::array<unsigned char,0xC0> chunk{};
    std::array<std::uintptr_t,2> head{};
    alignas(16) std::array<std::array<unsigned char,0x90>,3> packs{};
    std::array<std::array<SolidKeyPointer,3>,3> tables{};
    std::uintptr_t solid=0;std::uint32_t key=0;
    template<class T> void Put(unsigned p,unsigned offset,T value) {
        std::memcpy(packs[p].data()+offset,&value,sizeof(value));
    }
    void Reset(std::uint32_t value=0x3817D39A) {
        chunk={};packs={};tables={};key=value;
        SolidKeyPointer top{0x80134010,0xA8},child{0x00134011,0xA0};
        std::memcpy(chunk.data(),&top,8);std::memcpy(chunk.data()+8,&child,8);
        solid=reinterpret_cast<std::uintptr_t>(chunk.data()+16);
        std::memcpy(chunk.data()+32,&key,4);
        head={reinterpret_cast<std::uintptr_t>(packs[0].data()),reinterpret_cast<std::uintptr_t>(packs[2].data())};
        for(unsigned p=0;p<3;++p) {
            Put(p,0,p==2?Head():reinterpret_cast<std::uintptr_t>(packs[p+1].data()));
            Put(p,4,p==0?Head():reinterpret_cast<std::uintptr_t>(packs[p-1].data()));
            Put(p,0xC,3u);Put(p,0x74,reinterpret_cast<std::uintptr_t>(tables[p].data()));
            Put(p,0x7C,std::uint16_t{3});Put(p,0x7E,std::uint16_t{7});
            tables[p]={SolidKeyPointer{key-1,0x1111},SolidKeyPointer{key,solid},SolidKeyPointer{key+1,0x3333}};
        }
        tables[2][1].pointer=solid+64; // Same key, different live allocation.
    }
    std::uintptr_t Head(){return reinterpret_cast<std::uintptr_t>(head.data());}
    std::uintptr_t Owner(){return reinterpret_cast<std::uintptr_t>(packs[0].data());}
    bool Detach(unsigned* count){return DetachSolidAliases(Head(),reinterpret_cast<std::uintptr_t>(chunk.data()),Owner(),count);}
    std::uint16_t Dirty(unsigned p){std::uint16_t value=0;std::memcpy(&value,packs[p].data()+0x7C,2);return value;}
};
SolidFixture* g_solidFixture=nullptr;
unsigned g_solidForwarded=0;bool g_solidForwardCorrect=false;
void __cdecl FakeSolidUnload(void* chunk,void* owner) {
    ++g_solidForwarded;
    g_solidForwardCorrect=chunk==g_solidFixture->chunk.data()&&
        owner==g_solidFixture->packs[0].data()&&g_solidFixture->tables[1][1].pointer==0&&
        g_solidFixture->tables[0][1].pointer==g_solidFixture->solid;
}
}
template<class Expect>
void RunAlpha88Tests(Expect expect) {
    SolidFixture f;unsigned count=0;
    f.Reset();expect(f.Detach(&count)&&count==1,"alpha88 exact shared alias detached before free");
    expect(f.tables[0][1].pointer==f.solid&&f.Dirty(0)==3,"alpha88 native owner left for original lifecycle");
    expect(f.tables[1][1].pointer==0&&f.Dirty(1)==0xFFFF,"alpha88 alias table invalidated and dependencies marked for native rebuild");
    expect(f.tables[2][1].pointer==f.solid+64&&f.Dirty(2)==3,"alpha88 same key different live solid preserved");
    expect(f.tables[1][0].pointer==0x1111&&f.tables[1][2].pointer==0x3333,"alpha88 unrelated keys unchanged");
    expect(f.Detach(&count)&&count==0,"alpha88 repeated cleanup idempotent");
    f.tables[1][1].pointer=f.solid+80;
    expect(f.Detach(&count)&&count==0&&f.tables[1][1].pointer==f.solid+80,
        "alpha88 later same-key replacement is not detached by old allocation identity");
    f.Reset();f.head[0]=reinterpret_cast<std::uintptr_t>(f.packs[1].data());f.Put(1,4,f.Head());
    expect(f.Detach(&count)&&count==1,"alpha88 owner pack already unlinked from loaded list still clears surviving alias");
    for(unsigned i=1;i<=128;++i) {
        f.Reset(i*7919);expect(f.Detach(&count)&&count==1,"alpha88 arbitrary vehicle or part IDs supported without blacklist");
    }
    f.Reset();f.tables[1][2]={f.key,f.solid};
    expect(f.Detach(&count)&&count==2,"alpha88 all equal-key exact aliases cleared within one pack");
    f.Reset();f.tables[1][1].pointer=0;
    expect(f.Detach(&count)&&count==0&&f.Dirty(1)==3,"alpha88 already unloaded slots unchanged");
    f.Reset();f.Put(2,0,std::uintptr_t{1});const auto before=f.tables;
    expect(!f.Detach(&count)&&f.tables==before&&f.Dirty(1)==3,"alpha88 full list preflight prevents partial repair on malformed tail");
    f.Reset();f.Put(1,0xC,65537u);
    expect(!f.Detach(&count)&&f.tables[1][1].pointer==f.solid,"alpha88 excessive table count fails bounded preflight");
    f.Reset();f.Put(1,4,std::uintptr_t{0});
    expect(!f.Detach(&count),"alpha88 broken backlink rejected");
    f.Reset();std::uint32_t huge=0xFFFFFFFF;std::memcpy(f.chunk.data()+12,&huge,4);
    expect(!f.Detach(&count)&&f.tables[1][1].pointer==f.solid,"alpha88 malformed chunk length causes no mutation");
    f.Reset();std::uint32_t other=0;std::memcpy(f.chunk.data(),&other,4);
    expect(f.Detach(&count)&&count==0,"alpha88 other chunk types preserve native no-op");
    f.Reset();const auto oldBase=g_base;const auto oldOriginal=g_originalSolidUnload;const bool oldFault=g_faulted;
    g_base=f.Head()-(kSolidPackHead-kPreferredBase);g_solidFixture=&f;
    g_originalSolidUnload=&FakeSolidUnload;g_solidForwarded=0;g_solidForwardCorrect=false;
    SolidUnloadHook(f.chunk.data(),f.packs[0].data());
    expect(g_solidForwarded==1&&g_solidForwardCorrect,"alpha88 production hook cleans before forwarding original once with unchanged arguments");
    f.Reset();f.Put(1,4,std::uintptr_t{0});g_faulted=false;
    SolidUnloadHook(f.chunk.data(),f.packs[0].data());
    expect(g_solidForwarded==2&&g_faulted,"alpha88 unsupported data stops new spawns but never suppresses native destructor");
    g_base=oldBase;g_originalSolidUnload=oldOriginal;g_faulted=oldFault;g_solidFixture=nullptr;
}
