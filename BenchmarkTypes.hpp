#pragma once
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>

// ─────────────────────────────────────────────────────────────
// Global allocation counters
// Override new/delete to count every heap operation.
// These are sampled before and after each audio block.
// ─────────────────────────────────────────────────────────────
namespace AllocCounter {
    inline std::atomic<long long> totalAllocs{0};
    inline std::atomic<long long> totalFrees{0};

    inline void reset() {
        totalAllocs.store(0);
        totalFrees.store(0);
    }

    inline long long allocs() { return totalAllocs.load(); }
    inline long long frees()  { return totalFrees.load();  }
}

// Override global new/delete — works on MSVC and MinGW
// without any external tools.
inline void* operator new(std::size_t size) {
    ++AllocCounter::totalAllocs;
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc{};
    return p;
}

inline void operator delete(void* ptr) noexcept {
    if (ptr) {
        ++AllocCounter::totalFrees;
        std::free(ptr);
    }
}

inline void operator delete(void* ptr, std::size_t) noexcept {
    if (ptr) {
        ++AllocCounter::totalFrees;
        std::free(ptr);
    }
}

// ─────────────────────────────────────────────────────────────
// Fragmentation probe
// Attempts to allocate progressively halved blocks to find
// the largest contiguous free region available right now.
// ─────────────────────────────────────────────────────────────
inline std::size_t largestContiguousBlock() {
    std::size_t size = 128ULL * 1024 * 1024; // start at 128 MB
    while (size >= 4096) {
        void* p = std::malloc(size);
        if (p) {
            std::free(p);
            return size;
        }
        size /= 2;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────
// Checksum — simple XOR+rotate over float buffer
// Used to verify two instances produce identical output
// (memory safety / concurrency test).
// ─────────────────────────────────────────────────────────────
inline uint32_t checksum(const float* buf, std::size_t n) {
    uint32_t h = 0x12345678u;
    for (std::size_t i = 0; i < n; ++i) {
        uint32_t bits;
        std::memcpy(&bits, &buf[i], sizeof(bits));
        h ^= bits;
        h = (h << 7) | (h >> 25); // rotate left 7
    }
    return h;
}

// ─────────────────────────────────────────────────────────────
// Timer — nanosecond resolution
// ─────────────────────────────────────────────────────────────
struct Timer {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point t0;

    void start() { t0 = Clock::now(); }

    long long elapsedNs() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - t0).count();
    }
};

// ─────────────────────────────────────────────────────────────
// BenchmarkResult — one row in the final report
// ─────────────────────────────────────────────────────────────
struct BenchmarkResult {
    std::string  architectureName;

    // Timing unpredictability
    long long    allocsPerBlock        = 0;   // avg allocations per audio block
    double       avgBlockLatencyUs     = 0.0; // average block processing time

    // Memory churn
    long long    totalAllocsRuntime    = 0;   // total new() calls during steady-state
    long long    totalFreesRuntime     = 0;   // total delete() calls during steady-state

    // Fragmentation
    std::size_t  fragBefore            = 0;   // largest contiguous block before run
    std::size_t  fragAfter             = 0;   // largest contiguous block after run

    // Concurrency safety
    bool         instancesProduceSameOutput = false;
    int          checksumMismatches    = 0;
};