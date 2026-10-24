// Native recolouring caches marker-pixel indices globally, without recording
// the source dimensions. Retail DUMMY_SKIN1..4 are 512px; 5..12 are 256px.
// An extended allocator can therefore replay 512px indices into a 256px skin.
// Keep the original algorithm, and invalidate both its flag and four counts
// when dimensions change. No texture/resource replacement or file edits.
bool (__cdecl* g_nativeSkinRecolour)(void*)=nullptr;
std::mutex g_skinRecolourMutex;
unsigned g_skinRecolourWidth=0,g_skinRecolourHeight=0;
bool SkinRecolourSize(void* job,unsigned& width,unsigned& height){
 __try{
  auto texture=job?*static_cast<BYTE**>(job):nullptr;
  if(!texture||texture[0x4A]!=32)return false;
  width=*reinterpret_cast<unsigned short*>(texture+0x44);
  height=*reinterpret_cast<unsigned short*>(texture+0x46);
  return width&&height&&width<=4096&&height<=4096;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool __cdecl SkinRecolour(void* job){
 // The native cache itself is shared. Serialize its scan and replay together,
 // including callers from other threads, rather than only protecting our key.
 std::lock_guard<std::mutex> guard(g_skinRecolourMutex);
 unsigned width=0,height=0;
 if(SkinRecolourSize(job,width,height)&&
    (width!=g_skinRecolourWidth||height!=g_skinRecolourHeight)){
  *reinterpret_cast<unsigned*>(0x9B0A18)=0;
  memset(reinterpret_cast<void*>(0x9B0A1C),0,4*sizeof(unsigned));
  g_skinRecolourWidth=width;g_skinRecolourHeight=height;
  if(g_loggingEnabled)Log(L"SKIN_RECOLOUR_CACHE width=%u height=%u native_indices_rebuilt=1 resource_files_modified=0",width,height);
 }
 return g_nativeSkinRecolour(job);
}
bool InstallSkinRecolourCache(){
 const BYTE prologue[]={0x83,0xEC,0x44,0x8B,0x44,0x24,0x48};
 const BYTE flag[]={0xA1,0x18,0x0A,0x9B,0};
 const BYTE replay[]={0x8B,0x11,0x8B,0x1E,0x89,0x5C,0x95,0};
 if(memcmp(reinterpret_cast<void*>(0x73B010),prologue,sizeof(prologue))||
    memcmp(reinterpret_cast<void*>(0x73B061),flag,sizeof(flag))||
    memcmp(reinterpret_cast<void*>(0x73B320),replay,sizeof(replay))){
  Log(L"SKIN_RECOLOUR_CACHE inactive=unsupported-code native_preserved=1");return false;
 }
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
 auto site=reinterpret_cast<void*>(0x73B010);
 if(MH_CreateHook(site,SkinRecolour,reinterpret_cast<void**>(&g_nativeSkinRecolour))!=MH_OK)return false;
 if(MH_EnableHook(site)!=MH_OK){MH_RemoveHook(site);return false;}
 Log(L"SKIN_RECOLOUR_CACHE installed=1 mixed_native_resolutions=1 original_recolour_retained=1");return true;
}
