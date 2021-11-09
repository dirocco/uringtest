#include "Common.h"

#include <stdio.h>
#include <vector>
#include <algorithm>

template<unsigned samples>
class PercentileTimer
{
    std::vector<int64_t> times;
    int64_t lastTick = 0;
    const char* name;

public:
    PercentileTimer(const char* name)
    : name(name)
    {
        times.reserve(samples);
    }

    void tick(int64_t t = Time::now())
    {
        lastTick = t;
    }

    void tock(int64_t t = Time::now())
    {
        times.push_back(t - lastTick);
        if(unlikely(times.size() >= samples))
        {
            summary();
            times.clear();
            times.reserve(samples);
        }
    }

    void summary()
    {
        std::sort(times.begin(), times.end());

#define pct(f) times[times.size() * f] / (double)Time::usec
        printf("%s %lu p25: %.3f p50: %.3f p90: %.3f p95: %.3f p99: %.3f p999: %.3f\n",
                name, times.size(), pct(0.25), pct(0.5), pct(0.90), pct(0.95), pct(0.99), pct(0.999));
#undef pct
    }
};
