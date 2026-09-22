// Session-only retry isolation. No model names, provider assumptions, native
// pointers or persistent calibration edits. A mismatch is NEVER accepted.
struct PerformanceRetry {
    struct Entry { std::uint32_t key=0; std::uint64_t since=0; bool used=false; };
    std::array<Entry,16> entries{};
    static constexpr std::uint64_t delayMs=60000;

    bool Cooling(std::uint32_t key,std::uint64_t now) noexcept {
        bool found=false;
        for(auto& e:entries) {
            if(e.used && (now<e.since || now-e.since>=delayMs)) e={};
            if(e.used && e.key==key) found=true;
        }
        return found;
    }
    void Failed(std::uint32_t key,std::uint64_t now) noexcept {
        Cooling(key,now);
        Entry* target=nullptr;
        for(auto& e:entries) if(e.used&&e.key==key) {target=&e;break;}
        if(!target) for(auto& e:entries) if(!e.used) {target=&e;break;}
        if(!target) target=&*std::min_element(entries.begin(),entries.end(),
            [](const Entry& a,const Entry& b){return a.since<b.since;});
        *target={key,now,true};
    }
};
PerformanceRetry g_performanceRetry;

bool RecordPerformanceVerification(std::uint32_t key,const PerformanceLevels& expected,
                                   const PerformanceLevels& actual,std::uint64_t now) noexcept {
    if(expected==actual) return true;
    g_performanceRetry.Failed(key,now);
    Log(LogLevel::Warning,"PERFORMANCE_RETRY key=%08X delayMs=60000 expected=%u,%u,%u,%u,%u,%u,%u actual=%u,%u,%u,%u,%u,%u,%u validationRelaxed=0 persistentExclusion=0",
        key,expected[0],expected[1],expected[2],expected[3],expected[4],expected[5],expected[6],
        actual[0],actual[1],actual[2],actual[3],actual[4],actual[5],actual[6]);
    return false;
}
