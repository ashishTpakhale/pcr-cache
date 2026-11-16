#define NOMINMAX
#include <windows.h>
#include <x86intrin.h>
#include "cache.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <new>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bench;
using Clock = std::chrono::steady_clock;
constexpr int warm = 100000, measured = 500000;
inline u64 stamp() {
    _mm_lfence();
    u64 t = __rdtsc();
    _mm_lfence();
    return t;
}

void check(bool ok) {
    if (!ok) throw std::runtime_error("verification failure");
}
void selftest() {
    PCR p;
    struct Ref { int credit{}, epoch{}, birth{}; };
    Ref ref[ways];
    int n = 0, admissions = 0;
    std::mt19937 rng(2026);
    for (int i = 0; i < 100000; ++i) {
        int epoch = admissions / ways;
        if (n && rng()%3) {
            int j = rng()%n;
            p.hit(j);
            if (ref[j].epoch < epoch) {
                ref[j].credit = std::min(3, 1 + std::max(0, ref[j].credit -
                                         (epoch - ref[j].epoch - 1)));
                ref[j].epoch = epoch;
            }
        } else {
            int v = n;
            auto score = [&](const Ref &e) {
                int aged = std::max(0, e.credit - std::max(0, epoch - e.epoch - 1));
                return std::pair(aged + 2*(admissions - e.birth <= 3), e.birth);
            };
            if (n == ways) {
                v = 0;
                for (int j = 1; j < ways; ++j)
                    if (score(ref[j]) < score(ref[v])) v = j;
            }
            check(v == p.admit(n));
            ref[v] = {0, epoch, admissions++};
            n = std::min(ways, n+1);
        }
    }
    SIEVE s;
    AdaptiveClimb a;
    LFU f;
    LRU l;
    for (int i=0; i<ways; ++i) {
        check(s.admit(i)==i && a.admit(i)==i && f.admit(i)==i && l.admit(i)==i);
    }
    s.hit(0); s.hit(2);
    check(s.admit(ways)==1 && s.admit(ways)==3);
    a.hit(0); check(a.id[0]==0 && a.admit(ways)==1);
    f.hit(0); l.hit(0); check(f.admit(ways)==1 && l.admit(ways)==1);
    Set<WTinyLFU> w{};
    for (int i=0; i<ways; ++i) check(!w.access(i,mix(i)).hit);
    w.access(15,mix(15)); // window candidate wins over a singleton
    w.access(16,mix(16));
    check(w.access(15,mix(15)).hit && w.access(16,mix(16)).hit);
}

struct Trace {
    std::vector<u64> keys;
    std::vector<int> samples;
    u64 digest{};
};
Trace generate(int workload, int seed) {
    std::mt19937_64 rng(seed), sampling(seed + 7919);
    std::vector<double> cdf(131072);
    double total = 0;
    for (int i=0; i<int(cdf.size()); ++i)
        cdf[i] = total += std::pow(i+1., -1.05);
    Trace trace;
    for (int i=0; i<warm+measured; ++i) {
        u64 key;
        if (workload==2) {
            // Alternate a looping core and a one-pass scan of unique keys.
            key = i%40000 < 20000 ? (i+seed)%8192 : u64(1'000'000+i);
        } else if (workload==1 && i%20000 < 6000 && rng()%10 < 8) {
            key = 200000 + (i/20000)*4096 + rng()%2048;
        } else {
            double target = (rng() >> 11) * 0x1p-53 * total;
            key = std::lower_bound(cdf.begin(),cdf.end(),target) - cdf.begin();
        }
        trace.keys.push_back(key);
        trace.digest = mix(trace.digest ^ key);
    }
    // One uniformly selected request per 32-request block, in a separate replay.
    for (int i=warm; i+32<=warm+measured; i+=32)
        trace.samples.push_back(i+sampling()%32);
    return trace;
}

struct Stats { int hits{}; u64 checksum{}; };
template<class P, std::size_t budget>
void replay(const Trace &trace, const char *policy, const char *load, int seed, int rep) {
    using Bucket = Set<P>;
    constexpr int sets = budget / sizeof(Bucket);
    long long elapsed{};
    Stats initial;
    std::vector<long long> latency;
    latency.reserve(trace.samples.size());
    std::vector<long long> ticks;
    ticks.reserve(trace.samples.size());
    for (int pass=0; pass<3; ++pass) {
        // An exact-size aligned arena, including unused tail. No resident heap pointers.
        alignas(Bucket) static std::byte arena[budget];
        auto *buckets = ::new (arena) Bucket[sets]{};
        auto access = [&](u64 key) {
            u64 hash = mix(key);
            auto index = ((hash >> 32) * sets) >> 32;
            return buckets[index].access(key,hash);
        };
        for (int i=0; i<warm; ++i) access(trace.keys[i]);
        Stats stats;
        std::size_t sample = 0;
        auto begin = Clock::now();
        for (int i=warm; i<warm+measured; ++i) {
            Result r;
            if (pass && sample<trace.samples.size() && i==trace.samples[sample]) {
                if (pass == 2) {
                    auto start = stamp();
                    r = access(trace.keys[i]);
                    ticks.push_back(stamp()-start);
                } else {
                auto start = Clock::now();
                r = access(trace.keys[i]);
                auto end = Clock::now();
                latency.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count());
                }
                ++sample;
            } else r = access(trace.keys[i]);
            stats.hits += r.hit;
            stats.checksum += r.value;
        }
        auto stop = Clock::now();
        if (!pass) {
            elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(stop-begin).count();
            initial = stats;
        } else check(initial.hits==stats.hits && initial.checksum==stats.checksum);
    }
    u64 expected=0;
    for (int i=warm; i<warm+measured; ++i) expected += trace.keys[i]^0x12345678ULL;
    check(expected==initial.checksum);
    std::sort(latency.begin(), latency.end());
    std::sort(ticks.begin(), ticks.end());
    auto p99ticks = ticks[(ticks.size()*99+99)/100-1];
    auto p99 = latency[(latency.size()*99+99)/100-1];
    std::cout << policy<<','<<load<<','<<seed<<','<<rep<<','<<budget<<','<<sizeof(Bucket)
              <<','<<sets<<','<<warm<<','<<measured<<','<<initial.hits<<','<<elapsed
              <<','<<latency.size()<<','<<p99<<','<<p99ticks<<','<<trace.digest<<'\n';
}

int main() {
    DWORD_PTR allowed{}, system{};
    check(GetProcessAffinityMask(GetCurrentProcess(), &allowed, &system));
    auto core = allowed & (~allowed + 1);
    check(SetThreadAffinityMask(GetCurrentThread(), core) != 0);
    LARGE_INTEGER frequency{};
    check(QueryPerformanceFrequency(&frequency));
    std::cerr << "Affinity mask " << core << ", QPC Hz " << frequency.QuadPart << std::endl;
    selftest();
    using Fn = void(*)(const Trace&,const char*,const char*,int,int);
    Fn run[][6] = {
        {replay<PCR,65536>, replay<LRU,65536>, replay<SIEVE,65536>, replay<LFU,65536>, replay<AdaptiveClimb,65536>, replay<WTinyLFU,65536>},
        {replay<PCR,262144>, replay<LRU,262144>, replay<SIEVE,262144>, replay<LFU,262144>, replay<AdaptiveClimb,262144>, replay<WTinyLFU,262144>}
    };
    const char *names[] = {"PCR","LRU","SIEVE","LFU","AdaptiveClimb","W-TinyLFU"};
    const char *loads[] = {"Zipf","Flash","ScanLoop"};
    std::cout<<"policy,workload,seed,rep,budget,set_bytes,sets,warm,requests,hits,elapsed_ns,samples,p99_ns,p99_ticks,trace_hash\n";
    for (int seed=41; seed<=45; ++seed) {
        for (int w=0; w<3; ++w) {
            auto trace = generate(w,seed);
            for (int rep=0; rep<3; ++rep) {
                std::array<int,6> order{0,1,2,3,4,5};
                std::mt19937 ordering(seed*100+w*10+rep);
                std::shuffle(order.begin(),order.end(),ordering);
                for (int b=0; b<2; ++b)
                    for (int j:order) run[(rep+b)%2][j](trace,names[j],loads[w],seed,rep);
            }
        }
    }
}


