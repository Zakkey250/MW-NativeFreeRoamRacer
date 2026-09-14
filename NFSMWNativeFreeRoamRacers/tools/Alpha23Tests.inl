    {
        auto* handler = AddVectoredExceptionHandler(1, CountDiagnosticExceptions);
        expect(handler != nullptr, "audio first-chance exception observer installed");
        std::uint32_t value = 0x12345678;
        const auto initialAv = diagnosticAvCount;
        SafeRead(reinterpret_cast<void*>(1), &value);
        expect(diagnosticAvCount == initialAv + 1, "audio negative control proves SEH still raises first-chance AV");
        const auto baselineAv = diagnosticAvCount;
        bool rejected = true;
        for (unsigned i = 0; i < 1000; ++i) {
            for (const auto base : {1u, 0xAAAAAAAAu, 0xCDCDCDCDu, 0xDDDDDDDDu, 0xFEEEFEEEu})
                rejected &= !AudioRead(base, 0x38, &value);
        }
        expect(rejected && diagnosticAvCount == baselineAv && value == 0x12345678,
            "audio invalid pointers rejected 5000 times without AV or output mutation");
        auto* pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        expect(pages != nullptr, "audio page boundary fixture allocated");
        if (pages) {
            DWORD previous = 0;
            expect(VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &previous) != 0,
                "audio inaccessible second page configured");
            expect(!AudioRead(reinterpret_cast<std::uintptr_t>(pages), 4094, &value),
                "audio cross-page read rejects inaccessible trailing bytes");
            expect(VirtualProtect(pages, 4096, PAGE_READWRITE | PAGE_GUARD, &previous) != 0,
                "audio guard page configured");
            expect(!AudioRead(reinterpret_cast<std::uintptr_t>(pages), 0, &value) && diagnosticGuardCount == 0,
                "audio guard page not touched or consumed");
            MEMORY_BASIC_INFORMATION mbi{}; VirtualQuery(pages, &mbi, sizeof(mbi));
            expect((mbi.Protect & PAGE_GUARD) != 0, "audio read preserves guard protection");
            VirtualFree(pages, 0, MEM_RELEASE);
        }
        auto* reserved = VirtualAlloc(nullptr, 4096, MEM_RESERVE, PAGE_NOACCESS);
        expect(reserved && !AudioRead(reinterpret_cast<std::uintptr_t>(reserved), 0, &value),
            "audio reserved uncommitted memory rejected");
        if (reserved) VirtualFree(reserved, 0, MEM_RELEASE);
        expect(!AudioReadableRange(0xfffffff0u, 64) && !AudioReadableRange(0x10000, 0),
            "audio whole range overflow and zero size rejected");
        auto* upper = VirtualAlloc(reinterpret_cast<void*>(0x90000000u), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        expect(upper != nullptr, "audio LAA address above 2 GiB available in test executable");
        if (upper) {
            *static_cast<std::uint32_t*>(upper) = 123;
            expect(AudioRead(reinterpret_cast<std::uintptr_t>(upper), 0, &value) && value == 123,
                "audio valid LAA upper-half address remains readable");
            VirtualFree(upper, 0, MEM_RELEASE);
        }
        put(producer, 0, Address(0x008974AC)); put(producer, 0x28, 0xAAAAAAAAu);
        AudioSourceIdentity alternate{};
        expect(ReadAudioSourceIdentity(1, producerAddress, &alternate), "audio crashing route B variant identity reproduced");
        AudioSourceValues alternateValues{};
        ReadAudioSourceBefore(alternate, &alternateValues);
        expect((alternateValues.valid & 0x55) == 0x55 && !(alternateValues.valid & 128) && alternateValues.context == 0,
            "audio route B retains feedback but never treats optional field as context pointer");
        const auto savedSourceB = g_originalAudioSource[1];
        g_originalAudioSource[1] = reinterpret_cast<AudioSourceFn>(&FakeAudioSource);
        g_audioSourceRecords = {};
        const auto beforeSourceCalls = sourceNativeCalls;
        const auto alternateResult = AudioSourceBHook(producer.data(), nullptr, 0xDEADBEEF);
        expect(sourceNativeCalls == beforeSourceCalls + 1 && alternateResult == (0xDEADBEEFu ^ producerAddress) &&
            g_audioSourceRecords[0].values.stateAfter == 0.5f,
            "audio crashing B variant still forwards native playback once with unchanged argument and return");
        g_originalAudioSource[1] = savedSourceB;
        put(producer, 0, Address(0x00896BF0));
        ReadAudioSourceIdentity(0, producerAddress, &alternate);
        alternateValues = {}; ReadAudioSourceBefore(alternate, &alternateValues);
        expect(!(alternateValues.valid & 128) && alternateValues.context == 0xAAAAAAAA,
            "audio route A rejects uninitialized optional context too");
        expect(diagnosticAvCount == baselineAv && diagnosticGuardCount == 0,
            "audio production probes generated zero first-chance exceptions");
        if (handler) RemoveVectoredExceptionHandler(handler);
    }
    {
        frameFixture = {}; frameAnimations = 0;
        const std::uint32_t hashes[] = {0x3345911D, 0x35236DBE, 0x0A729B1B, 0xD907EF51, 0xB996A84C, 0x3B1B624A, 0xA206A0B4};
        for (unsigned i = 0; i < frameFixture.size(); ++i) {
            put(frameFixture[i], 0x0C, i + 100); put(frameFixture[i], 0x10, hashes[i]);
            put(frameFixture[i], 0x18, i < 2 ? 5u : 1u); put(frameFixture[i], 0x1C, 0x40000001u);
            put(frameFixture[i], 0x24, 0xABCDEF00u + i);
        }
        put(frameFixture[0], 0x60, 5u); put(frameFixture[0], 0x64, reinterpret_cast<std::uintptr_t>(frameFixture[1].data()));
        put(frameFixture[1], 4, reinterpret_cast<std::uintptr_t>(frameFixture[3].data()));
        for (unsigned i = 3; i < 6; ++i) put(frameFixture[i], 4, reinterpret_cast<std::uintptr_t>(frameFixture[i+1].data()));
        put(frameFixture[1], 0x60, 1u); put(frameFixture[1], 0x64, reinterpret_cast<std::uintptr_t>(frameFixture[2].data()));
        std::array<std::uint8_t, 0x60> widget{};
        put(widget, 0, Address(kEncounterMenuVtable)); put(widget, 0x10, 1u);
        put(widget, 0x38, reinterpret_cast<std::uintptr_t>(frameFixture[0].data()));
        const auto savedWidget = widget; const auto savedIcon = frameFixture[5]; const auto savedSms = frameFixture[6];
        const EncounterUiApi api{FindFrameFixture, AnimateFrameFixture};
        expect(SetEncounterPromptWithApi(widget.data(), true, api) && g_encounterFrame.count == 5,
            "HUD frame acquires selected images and hidden ancestor path only");
        bool visible = true;
        for (unsigned i = 0; i < 5; ++i) {std::uint32_t flags = 0; EncounterUiRead(frameFixture[i].data(), 0x1C, &flags); visible &= !(flags & 1);}
        expect(visible && frameAnimations == 1 && frameAnimation == 0x5079C8F8 && frameAnimatedNode == frameFixture[2].data(),
            "HUD frame reveals ring and parents with one APPEAR on native join node");
        for (unsigned i = 0; i < 1000; ++i) SetEncounterFrameBits(g_encounterFrame, true);
        expect(frameAnimations == 1, "HUD frame repeated upkeep never restarts animation");
        expect(widget == savedWidget && frameFixture[5] == savedIcon && frameFixture[6] == savedSms,
            "HUD frame leaves native event metadata central icon and SMS untouched");
        expect(SetEncounterPromptWithApi(widget.data(), false, api) && frameAnimation == 0x033113AC && !g_encounterFrame.count,
            "HUD frame release uses LEAVE and relinquishes ownership");
        bool hidden = true;
        for (unsigned i = 0; i < 5; ++i) {std::uint32_t flags = 0; EncounterUiRead(frameFixture[i].data(), 0x1C, &flags); hidden &= (flags & 1) != 0;}
        expect(hidden, "HUD frame restores borrowed hidden bits including ancestors");
        put(frameFixture[0], 0x1C, 0x40000000u);
        SetEncounterPromptWithApi(widget.data(), true, api); SetEncounterPromptWithApi(widget.data(), false, api);
        std::uint32_t rootFlags = 0; EncounterUiRead(frameFixture[0].data(), 0x1C, &rootFlags);
        expect(!(rootFlags & 1), "HUD frame release preserves pre-existing visible native parent");
        put(widget, 0x48, 42u);
        const auto beforePriority = frameFixture;
        expect(!SetEncounterPromptWithApi(widget.data(), true, api) && frameFixture == beforePriority,
            "HUD frame native event priority rejects all visual writes");
        put(widget, 0x48, 0u); widget[0x4C] = 1;
        expect(!SetEncounterPromptWithApi(widget.data(), true, api), "HUD frame pending SMS blocks acquisition");
        widget[0x4C] = 0; put(frameFixture[3], 0x10, 42u);
        expect(!SetEncounterPromptWithApi(widget.data(), true, api), "HUD frame missing ring component fails closed");
        put(frameFixture[3], 0x10, hashes[3]); put(frameFixture[1], 0x1C, 0x40000041u);
        expect(!SetEncounterPromptWithApi(widget.data(), true, api), "HUD frame native hidden-lock on parent honored");
        put(frameFixture[1], 0x1C, 0x40000001u);
        SetEncounterPromptWithApi(widget.data(), true, api);
        put(frameFixture[2], 0x0C, 9999u);
        const auto beforeRecycle = frameFixture;
        expect(!SetEncounterPromptWithApi(widget.data(), false, api) && frameFixture == beforeRecycle,
            "HUD frame recycled node GUID rejects stale-pointer cleanup");
        put(frameFixture[2], 0x0C, 102u);
        SetEncounterPromptWithApi(widget.data(), true, api);
        g_encounterOwnedWidget = widget.data();
        const auto previousMenu = g_encounterOriginalMenu;
        g_encounterOriginalMenu = reinterpret_cast<EncounterMenuFn>(&FrameNativePriority);
        const auto beforeNative = frameFixture;
        EncounterMenuHook(widget.data(), nullptr, nullptr);
        expect(!g_encounterOwnedWidget && !g_encounterFrame.count && frameFixture == beforeNative,
            "HUD frame native event raised inside original callback wins without being hidden");
        g_encounterOriginalMenu = previousMenu;
    }
