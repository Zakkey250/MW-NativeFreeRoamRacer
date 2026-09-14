    {
        // Actual D3D9 rendering in an invisible task-owned test window. No game,
        // sound output, hook installation, or user window is accessed.
        HWND window=CreateWindowExW(0,L"STATIC",L"Encounter gauge offline test",WS_POPUP,
            0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);IDirect3DDevice9* device=nullptr;
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth=1280;pp.BackBufferHeight=720;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
        const bool ready=d3d&&window&&SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device));
        expect(ready,"gauge creates isolated hidden D3D9 test device");
        if(ready) {
            void* slot=reinterpret_cast<void*>(Address(0x00982BDC));void* before=nullptr;
            std::memcpy(&before,slot,4);std::memcpy(slot,&device,4);
            for(unsigned mode=0;mode<2;++mode) {
                device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040,1,0);
                device->SetRenderState(D3DRS_FOGENABLE,TRUE);
                g_gaugeVisible=true;g_gaugeLead=mode==0;g_gaugeGap=mode==0?180.0f:270.0f;
                device->BeginScene();DrawEncounterGauge(device);device->EndScene();
                DWORD fog=0;device->GetRenderState(D3DRS_FOGENABLE,&fog);
                expect(fog==TRUE,"gauge restores caller rendering state");
                IDirect3DSurface9 *back=nullptr,*copy=nullptr;
                bool captured=SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) &&
                    SUCCEEDED(device->CreateOffscreenPlainSurface(1280,720,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr)) &&
                    SUCCEEDED(device->GetRenderTargetData(back,copy));
                D3DLOCKED_RECT locked{};
                captured=captured&&SUCCEEDED(copy->LockRect(&locked,nullptr,D3DLOCK_READONLY));
                if(captured) {
                    const auto pixel=*reinterpret_cast<DWORD*>(static_cast<unsigned char*>(locked.pBits)+locked.Pitch*48+40*4);
                    expect(pixel==(mode==0?0xff50e669u:0xfff54b41u),mode==0?"gauge rendered green fill pixel":"gauge rendered red fill pixel");
                    BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
                    file.bfSize=file.bfOffBits+1280*720*4;
                    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=1280;info.biHeight=-720;
                    info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
                    HANDLE output=CreateFileW(mode==0?L"artifacts\\Tests\\gauge-lead.bmp":L"artifacts\\Tests\\gauge-chase.bmp",
                        GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
                    if(output!=INVALID_HANDLE_VALUE) {
                        DWORD written=0;WriteFile(output,&file,sizeof(file),&written,nullptr);WriteFile(output,&info,sizeof(info),&written,nullptr);
                        for(unsigned y=0;y<720;++y) WriteFile(output,static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch,1280*4,&written,nullptr);
                        CloseHandle(output);
                    }
                    copy->UnlockRect();
                }
                expect(captured,"gauge backbuffer captured for visual verification");
                if(copy) copy->Release();if(back) back->Release();
            }
            g_gaugeVisible=false;std::memcpy(slot,&before,4);device->Release();
        }
        if(d3d) d3d->Release();if(window) DestroyWindow(window);
    }
