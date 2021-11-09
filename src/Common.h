#pragma once

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#ifndef unlikely
#define unlikely(e) __builtin_expect(!!(e), 0)
#endif

#ifndef likely
#define likely(e) __builtin_expect(!!(e), 1)
#endif

inline bool setAffinity(int core)
{
    if(core < 0) return true;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    int err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if(likely(!err))
        return true;

    fprintf(stderr, "Failed to set affinity to core %d: %d %s\n", core, errno, strerror(errno));
    return false;
}

#define info(fmt, ...) fprintf(stderr, "INFO: " fmt, __VA_ARGS__)
#define warn(fmt, ...) fprintf(stderr, "WARN: " fmt, __VA_ARGS__)
#define error(fmt, ...) fprintf(stderr, "ERROR: " fmt, __VA_ARGS__)

#define fatal(fmt, ...) do { \
    fprintf(stderr, "FATAL: " fmt, __VA_ARGS__); \
    asm("int $3"); \
} while(0)

#define fatal_assert(c, ...) do { \
        if(unlikely(!(c))) fatal(__VA_ARGS__ ); \
} while(0)


namespace Time
{

static const uint64_t usec   = 1000;
static const uint64_t ms   = 1000 * usec;
static const uint64_t msec   = 1000 * usec;
static const uint64_t s = 1000 * msec;
static const uint64_t sec = 1000 * msec;
static const uint64_t second = 1000 * msec;
static const uint64_t minute = 60 * second;
static const uint64_t hour   = 60 * minute;
static const uint64_t day    = 24 * hour;

inline uint64_t now()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (uint64_t)(t.tv_sec * 1000000000ULL) + (uint64_t)t.tv_nsec;
}

}

