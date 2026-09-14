float ParseEncounterPowerScale(const wchar_t* text,float fallback) noexcept {
    if(!text) return fallback;
    wchar_t* end=nullptr;const float value=wcstof(text,&end);
    if(end==text||!std::isfinite(value)) return fallback;
    while(*end==L' '||*end==L'\t'||*end==L'\r'||*end==L'\n') ++end;
    // Allow INI comments, but not partially parsed junk (e.g. 1,50 / 1.5oops).
    if(*end&&*end!=L';'&&*end!=L'#') return fallback;
    return std::clamp(value,1.0f,2.0f);
}
void LoadEncounterPowerSettings(const wchar_t* path) noexcept {
    const wchar_t* keys[]={L"PowerScale0To100",L"PowerScale100To200",L"PowerScale200To300"};
    const float defaults[]={1.50f,1.75f,2.00f};
    for(unsigned i=0;i<3;++i) {
        wchar_t text[128]{};
        const auto count=GetPrivateProfileStringW(L"Encounter",keys[i],L"",text,128,path);
        g_settings.encounterPowerScales[i]=count>=127?defaults[i]:ParseEncounterPowerScale(text,defaults[i]);
    }
    Log(LogLevel::Info,"ENCOUNTER_POWER_CONFIG tiers=%.3f/%.3f/%.3f range=1.00..2.00 restartRequired=1",
        g_settings.encounterPowerScales[0],g_settings.encounterPowerScales[1],g_settings.encounterPowerScales[2]);
}
