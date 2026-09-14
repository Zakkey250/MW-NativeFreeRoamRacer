// Texture-free D3D9 overlay: no game FE clones, fonts, pools or retained GPU resources.
std::atomic<bool> g_gaugeVisible{false},g_gaugeLead{false};
std::atomic<float> g_gaugeGap{0};
using EncounterEndSceneFn=HRESULT(WINAPI*)(IDirect3DDevice9*);
EncounterEndSceneFn g_encounterEndScene=nullptr;
bool g_gaugeHookAttempted=false;
struct GaugeVertex { float x,y,z,rhw; D3DCOLOR color; };
struct GaugeGeometry {
    std::array<GaugeVertex,8192> vertices{};unsigned count=0;
    void Rect(float x,float y,float width,float height,D3DCOLOR c) {
        if(width<=0||height<=0||count+6>vertices.size()) return;
        const GaugeVertex a{x-.5f,y-.5f,0,1,c},b{x+width-.5f,y-.5f,0,1,c};
        const GaugeVertex d{x-.5f,y+height-.5f,0,1,c},e{x+width-.5f,y+height-.5f,0,1,c};
        for(auto v:{a,b,d,b,e,d}) vertices[count++]=v;
    }
    void Text(float x,float y,float scale,const char* text,D3DCOLOR c) {
        // Original 3x5 bitmap glyphs. Only the gauge labels use this tiny font;
        // localized notifications use the game's native Unicode text renderer.
        static constexpr char alphabet[]="0123456789m LEADCHS/-";
        static constexpr unsigned glyph[]={0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF,
            0x057D,0,0x4927,0x79E7,0x7BED,0x6B6E,0x7927,0x5BED,0x79CF,0x1248,0x01C0};
        for(const char* p=text;*p;++p,x+=4*scale) {
            const char* found=std::strchr(alphabet,*p);if(!found) continue;
            const auto bits=glyph[found-alphabet];
            for(unsigned row=0;row<5;++row) for(unsigned col=0;col<3;++col)
                if(bits&(1u<<(14-row*3-col))) Rect(x+col*scale,y+row*scale,scale,scale,c);
        }
    }
};

void DrawEncounterGauge(IDirect3DDevice9* device) {
    if(!g_gaugeVisible.load(std::memory_order_acquire)) return;
    IDirect3DDevice9* current=nullptr;
    if(!SafeRead(reinterpret_cast<void*>(Address(0x00982BDC)),&current)||current!=device) return;
    IDirect3DSurface9 *target=nullptr,*back=nullptr;
    if(FAILED(device->GetRenderTarget(0,&target))) return;
    const bool backbuffer=SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) && target==back;
    if(back) back->Release();target->Release();if(!backbuffer) return;
    D3DVIEWPORT9 viewport{};if(FAILED(device->GetViewport(&viewport))||viewport.Height<200) return;
    IDirect3DStateBlock9* saved=nullptr;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved))) return;
    if(FAILED(saved->Capture())) {saved->Release();return;}
    const float s=static_cast<float>(viewport.Height)/900.0f;
    const float x=28*s+viewport.X,y=28*s+viewport.Y,w=300*s;
    const auto gap=std::clamp(g_gaugeGap.load(),0.0f,300.0f);
    const auto color=g_gaugeLead.load()?D3DCOLOR_ARGB(255,80,230,105):D3DCOLOR_ARGB(255,245,75,65);
    GaugeGeometry geometry;
    geometry.Rect(x-8*s,y-8*s,w+16*s,78*s,D3DCOLOR_ARGB(190,8,12,20));
    geometry.Rect(x,y+22*s,w,20*s,D3DCOLOR_ARGB(255,145,150,160));
    geometry.Rect(x+2*s,y+24*s,w-4*s,16*s,D3DCOLOR_ARGB(255,22,26,32));
    geometry.Rect(x+2*s,y+24*s,(w-4*s)*gap/300.0f,16*s,color);
    for(unsigned i=1;i<6;++i) geometry.Rect(x+w*i/6,y+38*s,s,4*s,D3DCOLOR_ARGB(255,220,225,230));
    const auto white=D3DCOLOR_ARGB(255,240,245,250);
    geometry.Text(x,y,2*s,g_gaugeLead.load()?"LEAD":"CHASE",color);
    char label[24]{};sprintf_s(label,"%um / 300m",unsigned(gap));
    geometry.Text(x+158*s,y,2*s,label,white);
    geometry.Text(x,y+48*s,2*s,"0m",white);geometry.Text(x+w-32*s,y+48*s,2*s,"300m",white);
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);device->SetTexture(0,nullptr);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,geometry.count/3,geometry.vertices.data(),sizeof(GaugeVertex));
    saved->Apply();saved->Release();
}
HRESULT WINAPI EncounterEndSceneHook(IDirect3DDevice9* device) {
    DrawEncounterGauge(device);
    return g_encounterEndScene(device);
}
void EnsureEncounterGauge() noexcept {
    if(g_gaugeHookAttempted) return;
    void* device=nullptr;void** table=nullptr;
    if(!SafeRead(reinterpret_cast<void*>(Address(0x00982BDC)),&device)||!device||!SafeRead(device,&table)||!table) return;
    void* end=nullptr;if(!SafeRead(table+42,&end)||!end) return;
    g_gaugeHookAttempted=true;
    const bool created=MH_CreateHook(end,&EncounterEndSceneHook,reinterpret_cast<void**>(&g_encounterEndScene))==MH_OK;
    const bool enabled=created&&MH_EnableHook(end)==MH_OK;
    if(created&&!enabled) MH_RemoveHook(end);
    Log(LogLevel::Info,"ENCOUNTER_GAUGE installed=%u position=top-left scale=0-300m green=lead red=chase textures=0",unsigned(enabled));
}
