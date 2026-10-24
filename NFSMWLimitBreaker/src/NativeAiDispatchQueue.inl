// Native 8-byte AI dispatch pairs at 00914958. Its fixed 24-element buffer
// ends at 00914A28 (the scrape sound handle), then the spatial-query context.
// A hardware write watch proved native push_back wrote pair 25 there.
// Replace only this instance's allocator vtable; append/consume/order stay native.
constexpr std::uintptr_t kAiDispatchQueue=0x00914958u;
constexpr std::uint32_t kAiDispatchVtable=0x00899248u;
constexpr std::uint32_t kAiDispatchReserve=512;
constexpr std::uint32_t kAiDispatchMaximum=0x0FFFFFFFu;
struct AiDispatchPair {std::uint32_t key,value;};
struct alignas(16) AiDispatchStorage {
 std::array<std::uint32_t,4> before{};
 alignas(16) std::array<AiDispatchPair,kAiDispatchReserve> data{};
 std::array<std::uint32_t,4> after{};
};
static AiDispatchStorage g_aiDispatchStorage;
static std::array<std::uintptr_t,6> g_aiDispatchVtable{};
static bool g_aiDispatchInstalled=false;
static void* __fastcall AiDispatchAllocate(void*,void*,std::uint32_t count,std::uint32_t){
 if(count<=kAiDispatchReserve)return g_aiDispatchStorage.data.data();
 if(count>kAiDispatchMaximum)return nullptr;
 return _aligned_malloc(std::size_t(count)*sizeof(AiDispatchPair),16);
}
static void __fastcall AiDispatchFree(void* self,void*,void* memory,std::uint32_t){
 if(memory&&memory!=g_aiDispatchStorage.data.data()&&memory!=static_cast<BYTE*>(self)+16)_aligned_free(memory);
}
static std::uint32_t __fastcall AiDispatchGrow(void* self,void*,std::uint32_t requested){
 const auto current=*reinterpret_cast<const std::uint32_t*>(static_cast<BYTE*>(self)+8);
 return std::min(kAiDispatchMaximum,std::max({requested,kAiDispatchReserve,current+current/2}));
}
static std::uint32_t __fastcall AiDispatchMax(void*,void*){return kAiDispatchMaximum;}
static void RestoreAiDispatchQueue(){
 if(!g_aiDispatchInstalled)return;
 // Startup transaction rollback only: no live queue is retired here.
 if(At<std::uint32_t>(kAiDispatchQueue)==reinterpret_cast<std::uintptr_t>(g_aiDispatchVtable.data()))
  WriteBytes(kAiDispatchQueue,&kAiDispatchVtable,sizeof(kAiDispatchVtable));
 g_aiDispatchInstalled=false;
}
static bool InstallAiDispatchQueue(){
 const BYTE pushGuard[]={0x56,0x8B,0xF1,0x8B,0x46,0x0C,0x3B,0x46,0x08};
 const std::array<std::uint32_t,6> expected{{0x4F4710,0x4F22E0,0x4F22F0,0x4F22C0,0x4F22D0,0x4E4680}};
 const auto liveData=At<std::uint32_t>(kAiDispatchQueue+4),liveCapacity=At<std::uint32_t>(kAiDispatchQueue+8);
 const bool emptyStorage=(liveData==0&&liveCapacity==0)||(liveData==kAiDispatchQueue+16&&liveCapacity==24);
 if(memcmp(reinterpret_cast<void*>(0x4EA810),pushGuard,sizeof(pushGuard))||
    memcmp(reinterpret_cast<void*>(kAiDispatchVtable),expected.data(),sizeof(expected))||
    At<std::uint32_t>(kAiDispatchQueue)!=kAiDispatchVtable||
    !emptyStorage||At<std::uint32_t>(kAiDispatchQueue+12)!=0){
  Log(L"AI_DISPATCH_QUEUE_INACTIVE reason=identity-or-live-owner-mismatch push_match=%u vtable_match=%u live_vtable=%08lX data=%08lX capacity=%lu count=%lu",
   unsigned(memcmp(reinterpret_cast<void*>(0x4EA810),pushGuard,sizeof(pushGuard))==0),unsigned(memcmp(reinterpret_cast<void*>(kAiDispatchVtable),expected.data(),sizeof(expected))==0),
   static_cast<unsigned long>(At<std::uint32_t>(kAiDispatchQueue)),static_cast<unsigned long>(At<std::uint32_t>(kAiDispatchQueue+4)),
   static_cast<unsigned long>(At<std::uint32_t>(kAiDispatchQueue+8)),static_cast<unsigned long>(At<std::uint32_t>(kAiDispatchQueue+12)));return false;
 }
 for(unsigned i=0;i<6;++i)g_aiDispatchVtable[i]=expected[i];
 g_aiDispatchVtable[1]=reinterpret_cast<std::uintptr_t>(&AiDispatchAllocate);
 g_aiDispatchVtable[2]=reinterpret_cast<std::uintptr_t>(&AiDispatchFree);
 g_aiDispatchVtable[3]=reinterpret_cast<std::uintptr_t>(&AiDispatchGrow);
 g_aiDispatchVtable[4]=reinterpret_cast<std::uintptr_t>(&AiDispatchMax);
 g_aiDispatchStorage.before.fill(kGuardBefore);g_aiDispatchStorage.after.fill(kGuardAfter);
 // Execute the actual native append, including growth from the old 24 limit
 // and beyond the reserved 512 pairs. Check order, copy, alignment and guards.
 struct Fake {std::uintptr_t vtable;AiDispatchPair* data;std::uint32_t capacity,count;
  AiDispatchPair inlinePairs[24];std::uint32_t adjacent[8];};
 Fake fake{};fake.vtable=reinterpret_cast<std::uintptr_t>(g_aiDispatchVtable.data());
 fake.data=fake.inlinePairs;fake.capacity=24;for(auto& value:fake.adjacent)value=0xA514A28u;
 auto append=reinterpret_cast<void(__thiscall*)(void*,const AiDispatchPair*)>(0x4EA810);
 for(unsigned i=0;i<kAiDispatchReserve+17;++i){AiDispatchPair value{i,~i};append(&fake,&value);}
 bool valid=fake.count==kAiDispatchReserve+17&&fake.capacity>=fake.count&&
  (reinterpret_cast<std::uintptr_t>(fake.data)&15)==0;
 for(unsigned i=0;i<fake.count&&valid;++i)valid=fake.data[i].key==i&&fake.data[i].value==~i;
 for(auto value:fake.adjacent)valid=valid&&value==0xA514A28u;
 for(auto value:g_aiDispatchStorage.before)valid=valid&&value==kGuardBefore;
 for(auto value:g_aiDispatchStorage.after)valid=valid&&value==kGuardAfter;
 AiDispatchFree(&fake,nullptr,fake.data,fake.capacity);
 if(!valid){Log(L"AI_DISPATCH_QUEUE_INACTIVE reason=native-growth-self-test-failed");return false;}
 g_aiDispatchStorage.data.fill({});
 if(At<std::uint32_t>(kAiDispatchQueue)!=kAiDispatchVtable||At<std::uint32_t>(kAiDispatchQueue+12)!=0){
  Log(L"AI_DISPATCH_QUEUE_INACTIVE reason=owner-or-count-changed-during-preflight");return false;
 }
 const auto vtable=std::uint32_t(reinterpret_cast<std::uintptr_t>(g_aiDispatchVtable.data()));
 // Preserve the native lazy-empty header. Its first append reserves storage
 // through the checked vtable; warm inline contents migrate on native growth.
 if(!WriteBytes(kAiDispatchQueue,&vtable,4)){
  g_aiDispatchInstalled=true;RestoreAiDispatchQueue();return false;
 }
 g_aiDispatchInstalled=true;
 Log(L"AI_DISPATCH_QUEUE_ACTIVE object=00914958 old_capacity=24 reserve=512 native_growth_self_test=529 order_preserved=1 adjacent_sound_and_spatial_untouched=1");
 return true;
}
