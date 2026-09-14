// Borrow only the observed Engage_Mechanic frame pieces and event-join node.
// EventIcon is borrowed only while owned, with its original texture restored.
using EncounterFindFn = void*(__cdecl*)(void*, std::uint32_t);
using EncounterAnimateFn = void(__cdecl*)(void*, std::uint32_t, bool);
using EncounterTextureFn = void(__cdecl*)(void*,std::uint32_t);
struct EncounterUiApi { EncounterFindFn find; EncounterAnimateFn animate; EncounterTextureFn texture=nullptr; bool iconReady=false; };
struct EncounterFrameNode {
    void* pointer = nullptr;
    std::uint32_t guid = 0, hash = 0, hidden = 0;
};
struct EncounterFrameLease {
    void* widget = nullptr;
    void* root = nullptr;
    void* prompt = nullptr;
    void* icon = nullptr;
    bool independentIcon = false;
    std::uint32_t originalTexture=0;
    std::array<EncounterFrameNode, 24> nodes{};
    unsigned count = 0;
};
EncounterFrameLease g_encounterFrame{};

bool EncounterUiRead(void* object, std::size_t offset, std::uint32_t* value) noexcept {
    return AudioRead(reinterpret_cast<std::uintptr_t>(object), offset, value);
}
bool EncounterFramePath(void* node, void* target, EncounterFrameLease* lease,
                        unsigned depth, unsigned* visited) noexcept {
    if (!node || depth > 8 || ++*visited > 96 || lease->count >= lease->nodes.size()) return false;
    const auto savedCount = lease->count;
    EncounterFrameNode entry{}; entry.pointer = node;
    std::uint32_t flags = 0, type = 0;
    if (!EncounterUiRead(node, 0x0C, &entry.guid) || !EncounterUiRead(node, 0x10, &entry.hash) ||
        !EncounterUiRead(node, 0x18, &type) || !EncounterUiRead(node, 0x1C, &flags) ||
        (flags & 0x40)) return false; // Native explicitly disallows showing this node.
    entry.hidden = flags & 1;
    lease->nodes[lease->count++] = entry;
    if (node == target) return type == 1; // All three selected pieces are images.
    std::uint32_t count = 0, child = 0;
    if (type == 5 && EncounterUiRead(node, 0x60, &count) && count <= 32 &&
        EncounterUiRead(node, 0x64, &child)) {
        for (unsigned i = 0; i < count && child; ++i) {
            if (EncounterFramePath(reinterpret_cast<void*>(child), target, lease, depth + 1, visited)) return true;
            if (!EncounterUiRead(reinterpret_cast<void*>(child), 4, &child)) break;
        }
    }
    lease->count = savedCount;
    return false;
}

bool ResolveEncounterFrame(void* widget, EncounterFrameLease* output, const EncounterUiApi& api) noexcept {
    std::uint32_t package = 0, root = 0, hash = 0;
    if (!EncounterWidgetFree(widget) || !EncounterUiRead(widget, 0x10, &package) || !package ||
        !EncounterUiRead(widget, 0x38, &root) || !root ||
        !EncounterUiRead(reinterpret_cast<void*>(root), 0x10, &hash) || hash != 0x3345911D) return false;
    EncounterFrameLease result{}; result.widget = widget; result.root = reinterpret_cast<void*>(root);
    // Optional fourth image inherits the native frame position/scale/animation.
    for (const auto targetHash : {0x0A729B1Bu, 0xD907EF51u, 0xB996A84Cu,0x3B1B624Au}) {
        if(targetHash==0x3B1B624A&&(!api.iconReady||!api.texture))continue;
        void* target = api.find(reinterpret_cast<void*>(package), targetHash);
        if (!target || !EncounterUiRead(target, 0x10, &hash) || hash != targetHash) return false;
        EncounterFrameLease path{}; unsigned visited = 0;
        if (!EncounterFramePath(result.root, target, &path, 0, &visited)) {
            // EventIcon is separately owned by MenuZoneTrigger (+3C), not
            // necessarily under Engage_Mechanic. Native ShowEvent animates it
            // independently (0057BD1C/0057BD5D). Never borrow an arbitrary sibling.
            std::uint32_t ownedIcon=0;
            if(targetHash!=0x3B1B624A||!EncounterUiRead(widget,0x3C,&ownedIcon)||
                reinterpret_cast<void*>(ownedIcon)!=target) return false;
            path={};visited=0;
            if(!EncounterFramePath(target,target,&path,0,&visited))return false;
            result.independentIcon=true;
        }
        for (unsigned i = 0; i < path.count; ++i) {
            bool exists = false;
            for (unsigned j = 0; j < result.count; ++j) exists |= result.nodes[j].pointer == path.nodes[i].pointer;
            if (!exists) {
                if (result.count >= result.nodes.size()) return false;
                result.nodes[result.count++] = path.nodes[i];
            }
        }
        if (targetHash == 0x0A729B1B) result.prompt = target;
        if (targetHash == 0x3B1B624A) {
            result.icon=target;
            if(!EncounterUiRead(target,0x24,&result.originalTexture))return false;
        }
    }
    *output = result;
    return true;
}

bool EncounterFrameLive(void* widget,const EncounterFrameLease& lease) noexcept;
bool RestoreEncounterIcon(void* widget,const EncounterUiApi& api) noexcept {
    if(!g_encounterFrame.icon)return true;
    if(!api.texture||!EncounterFrameLive(widget,g_encounterFrame))return false;
    std::uint32_t texture=0;
    if(!EncounterUiRead(g_encounterFrame.icon,0x24,&texture))return false;
    // A real event may already have installed a new native race icon. Keep it.
    if(texture==kEncounterReadyTexture)api.texture(g_encounterFrame.icon,g_encounterFrame.originalTexture);
    return true;
}

bool EncounterFrameLive(void* widget, const EncounterFrameLease& lease) noexcept {
    std::uint32_t root = 0;
    if (widget != lease.widget || !lease.count || !EncounterUiRead(widget, 0x38, &root) ||
        reinterpret_cast<void*>(root) != lease.root) return false;
    if(lease.independentIcon) {
        std::uint32_t icon=0;
        if(!EncounterUiRead(widget,0x3C,&icon)||reinterpret_cast<void*>(icon)!=lease.icon)return false;
    }
    for (unsigned i = 0; i < lease.count; ++i) {
        std::uint32_t guid = 0, hash = 0;
        if (!EncounterUiRead(lease.nodes[i].pointer, 0x0C, &guid) || guid != lease.nodes[i].guid ||
            !EncounterUiRead(lease.nodes[i].pointer, 0x10, &hash) || hash != lease.nodes[i].hash) return false;
    }
    return true;
}

bool SetEncounterFrameBits(const EncounterFrameLease& lease, bool show) noexcept {
    std::array<std::uint32_t, 24> flags{};
    // Validate the whole small set before any write. No recursive native Show:
    // that would also reveal SMS/other key layouts and the stale central icon.
    for (unsigned i = 0; i < lease.count; ++i) {
        if (!EncounterUiRead(lease.nodes[i].pointer, 0x1C, &flags[i]) || (show && (flags[i] & 0x40))) return false;
        MEMORY_BASIC_INFORMATION page{};
        auto* address = static_cast<std::uint8_t*>(lease.nodes[i].pointer) + 0x1C;
        if (!VirtualQuery(address, &page, sizeof(page)) || page.State != MEM_COMMIT ||
            (page.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            ((page.Protect & 0xff) != PAGE_READWRITE && (page.Protect & 0xff) != PAGE_WRITECOPY &&
             (page.Protect & 0xff) != PAGE_EXECUTE_READWRITE && (page.Protect & 0xff) != PAGE_EXECUTE_WRITECOPY)) return false;
    }
    for (unsigned i = 0; i < lease.count; ++i) {
        const auto next = (flags[i] & ~1u) | (show ? 0u : lease.nodes[i].hidden);
        if (next != flags[i]) {
            // Same hidden/dirty bits as native 00514C70/00514CC0; retain all others.
            *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(lease.nodes[i].pointer) + 0x1C) = next | 0x02400000;
        }
    }
    return true;
}

bool UpgradeEncounterIconWithApi(void* widget,const EncounterUiApi& api) noexcept {
    if(!api.iconReady||!api.texture||g_encounterFrame.icon||!EncounterFrameLive(widget,g_encounterFrame))return false;
    EncounterFrameLease candidate{};
    if(!ResolveEncounterFrame(widget,&candidate,api)||!candidate.icon)return false;
    // The ring is already shown. Preserve its PRE-lease hidden flags, rather
    // than adopting the visible state as its eventual restore state.
    for(unsigned i=0;i<candidate.count;++i)for(unsigned j=0;j<g_encounterFrame.count;++j)
        if(candidate.nodes[i].pointer==g_encounterFrame.nodes[j].pointer&&
           candidate.nodes[i].guid==g_encounterFrame.nodes[j].guid&&candidate.nodes[i].hash==g_encounterFrame.nodes[j].hash)
            candidate.nodes[i].hidden=g_encounterFrame.nodes[j].hidden;
    if(!SetEncounterFrameBits(candidate,true))return false;
    api.texture(candidate.icon,kEncounterReadyTexture);
    api.animate(candidate.icon,0x5079C8F8,true);
    g_encounterFrame=candidate;return true;
}

bool SetEncounterPromptWithApi(void* widget, bool visible, const EncounterUiApi& api) noexcept {
    if (visible) {
        if (!ResolveEncounterFrame(widget, &g_encounterFrame, api)) return false;
        api.animate(g_encounterFrame.prompt, 0x5079C8F8, true);
        if (SetEncounterFrameBits(g_encounterFrame, true)) {
            if(g_encounterFrame.icon) {
                api.texture(g_encounterFrame.icon,kEncounterReadyTexture);
                api.animate(g_encounterFrame.icon,0x5079C8F8,true);
            }
            return true;
        }
        api.animate(g_encounterFrame.prompt, 0x033113AC, true);
        g_encounterFrame = {};
        return false;
    }
    if (!EncounterFrameLive(widget, g_encounterFrame)) { g_encounterFrame = {}; return false; }
    std::uint32_t currentTexture=0;
    if(g_encounterFrame.icon&&EncounterUiRead(g_encounterFrame.icon,0x24,&currentTexture)&&currentTexture==kEncounterReadyTexture)
        api.animate(g_encounterFrame.icon,0x033113AC,true);
    RestoreEncounterIcon(widget,api);
    api.animate(g_encounterFrame.prompt, 0x033113AC, true);
    const bool restored = SetEncounterFrameBits(g_encounterFrame, false);
    g_encounterFrame = {};
    return restored;
}

EncounterUiApi EncounterNativeUiApi() noexcept {
    return {reinterpret_cast<EncounterFindFn>(Address(kEncounterFindObject)),
        reinterpret_cast<EncounterAnimateFn>(Address(kEncounterSetAnimation)),
        reinterpret_cast<EncounterTextureFn>(Address(0x00515B90)),g_promptAssetState==2};
}

bool SetEncounterPromptVisible(void* widget, bool visible) noexcept {
    auto api=EncounterNativeUiApi();
    if(SetEncounterPromptWithApi(widget,visible,api))return true;
    // A different HUD package may omit/lock EventIcon. Retain the accepted ring.
    if(visible&&api.iconReady){api.iconReady=false;return SetEncounterPromptWithApi(widget,true,api);}
    return false;
}
