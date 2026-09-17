    {
        unsigned previous=0;SafeRead(reinterpret_cast<void*>(Address(kGameFlowState)),&previous);
        setGlobal(kGameFlowState,3u);expect(StartupNoticeAllowed(),"alpha59 startup permits notice");
        setGlobal(kGameFlowState,5u);expect(StartupNoticeAllowed(),"alpha59 frontend permits notice");
        setGlobal(kGameFlowState,6u);expect(!StartupNoticeAllowed(),"alpha59 gameplay cancels queued or open notice");
        setGlobal(kGameFlowState,0xFFFFFFFFu);expect(!StartupNoticeAllowed(),"alpha59 invalid flow rejects notice");
        setGlobal(kGameFlowState,previous);
    }
