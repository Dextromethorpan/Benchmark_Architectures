#pragma once
#include "BenchmarkTypes.hpp"
#include "VersionA.hpp"
#include "VersionB.hpp"
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>

// ─────────────────────────────────────────────────────────────
// Benchmark configuration
// ─────────────────────────────────────────────────────────────
static constexpr std::size_t BLOCK_SIZE      = 4096;  // samples per audio block
static constexpr int         WARMUP_BLOCKS   = 5;     // ignored — not steady-state
static constexpr int         MEASURED_BLOCKS = 100;   // steady-state measurement
static constexpr float       SAMPLE_RATE     = 44100.f;

// ─────────────────────────────────────────────────────────────
// Helpers — fill a test buffer with a 440 Hz sine wave
// ─────────────────────────────────────────────────────────────
static void fillTestBuffer(float* buf, std::size_t n, float sr) {
    for (std::size_t i = 0; i < n; ++i)
        buf[i] = std::sin(2.f * 3.14159f * 440.f *
                          static_cast<float>(i) / sr);
}

// ─────────────────────────────────────────────────────────────
// MEASUREMENT 1 — Timing Unpredictability
// Counts allocations per block and measures block latency.
// A well-designed audio architecture = 0 allocs per block
// during steady-state.
// ─────────────────────────────────────────────────────────────
namespace Measure {

template<typename ProcessFn>
void timingUnpredictability(const std::string& label,
                             ProcessFn processBlock,
                             BenchmarkResult& result) {
    std::cout << "  [Timing] Running " << label << "...\n";

    std::vector<float> buffer(BLOCK_SIZE);
    std::vector<long long> allocsPerBlock;
    std::vector<long long> latenciesNs;
    Timer timer;

    // Warm-up — not measured
    for (int i = 0; i < WARMUP_BLOCKS; ++i) {
        fillTestBuffer(buffer.data(), BLOCK_SIZE, SAMPLE_RATE);
        processBlock(buffer.data(), BLOCK_SIZE);
    }

    // Steady-state measurement
    for (int i = 0; i < MEASURED_BLOCKS; ++i) {
        fillTestBuffer(buffer.data(), BLOCK_SIZE, SAMPLE_RATE);

        long long before = AllocCounter::allocs();
        timer.start();
        processBlock(buffer.data(), BLOCK_SIZE);
        long long elapsed = timer.elapsedNs();

        long long after = AllocCounter::allocs();

        allocsPerBlock.push_back(after - before);
        latenciesNs.push_back(elapsed);
    }

    // Summarise
    result.allocsPerBlock = static_cast<long long>(
        std::accumulate(allocsPerBlock.begin(), allocsPerBlock.end(), 0LL)
        / static_cast<long long>(MEASURED_BLOCKS));

    double totalNs = static_cast<double>(
        std::accumulate(latenciesNs.begin(), latenciesNs.end(), 0LL));
    result.avgBlockLatencyUs = (totalNs / MEASURED_BLOCKS) / 1000.0;
}

// ─────────────────────────────────────────────────────────────
// MEASUREMENT 2 — Memory Churn
// Total malloc/free calls during the entire steady-state run.
// Version B should be near zero after construction.
// ─────────────────────────────────────────────────────────────
template<typename ProcessFn>
void memoryChurn(const std::string& label,
                 ProcessFn processBlock,
                 BenchmarkResult& result) {
    std::cout << "  [Churn]  Running " << label << "...\n";

    std::vector<float> buffer(BLOCK_SIZE);

    // Warm-up
    for (int i = 0; i < WARMUP_BLOCKS; ++i) {
        fillTestBuffer(buffer.data(), BLOCK_SIZE, SAMPLE_RATE);
        processBlock(buffer.data(), BLOCK_SIZE);
    }

    // Reset counters — measure only steady-state
    AllocCounter::reset();

    for (int i = 0; i < MEASURED_BLOCKS; ++i) {
        fillTestBuffer(buffer.data(), BLOCK_SIZE, SAMPLE_RATE);
        processBlock(buffer.data(), BLOCK_SIZE);
    }

    result.totalAllocsRuntime = AllocCounter::allocs();
    result.totalFreesRuntime  = AllocCounter::frees();
}

// ─────────────────────────────────────────────────────────────
// MEASUREMENT 3 — Memory Fragmentation
// Probes the largest available contiguous block before and
// after the run. A shrinking value means fragmentation is
// accumulating — dangerous in long sessions.
// ─────────────────────────────────────────────────────────────
template<typename ProcessFn>
void fragmentation(const std::string& label,
                   ProcessFn processBlock,
                   BenchmarkResult& result) {
    std::cout << "  [Frag]   Running " << label << "...\n";

    std::vector<float> buffer(BLOCK_SIZE);

    // Probe BEFORE
    result.fragBefore = largestContiguousBlock();

    // Run steady-state
    for (int i = 0; i < MEASURED_BLOCKS; ++i) {
        fillTestBuffer(buffer.data(), BLOCK_SIZE, SAMPLE_RATE);
        processBlock(buffer.data(), BLOCK_SIZE);
    }

    // Probe AFTER
    result.fragAfter = largestContiguousBlock();
}

// ─────────────────────────────────────────────────────────────
// MEASUREMENT 4 — Concurrency Safety
// Creates TWO instances and runs them simultaneously in
// separate threads with identical input.
// Their outputs MUST produce the same checksum — if they
// differ, shared state is leaking between instances.
// ─────────────────────────────────────────────────────────────
template<typename MakeInstance, typename ProcessFn>
void concurrencySafety(const std::string& label,
                        MakeInstance makeInstance,
                        ProcessFn processBlock,
                        BenchmarkResult& result) {
    std::cout << "  [Conc]   Running " << label << "...\n";

    int mismatches = 0;
    constexpr int ROUNDS = 50;

    for (int round = 0; round < ROUNDS; ++round) {
        // Two independent instances
        auto instanceA = makeInstance();
        auto instanceB = makeInstance();

        std::vector<float> bufA(BLOCK_SIZE);
        std::vector<float> bufB(BLOCK_SIZE);

        // Identical input
        fillTestBuffer(bufA.data(), BLOCK_SIZE, SAMPLE_RATE);
        fillTestBuffer(bufB.data(), BLOCK_SIZE, SAMPLE_RATE);

        // Run both simultaneously in separate threads
        std::thread tA([&]{ processBlock(*instanceA, bufA.data(), BLOCK_SIZE); });
        std::thread tB([&]{ processBlock(*instanceB, bufB.data(), BLOCK_SIZE); });
        tA.join();
        tB.join();

        // Checksums must match — same input, same architecture, same result
        uint32_t csA = checksum(bufA.data(), BLOCK_SIZE);
        uint32_t csB = checksum(bufB.data(), BLOCK_SIZE);

        if (csA != csB) {
            ++mismatches;
            std::cout << "    !! MISMATCH at round " << round
                      << "  csA=" << std::hex << csA
                      << "  csB=" << csB << std::dec << "\n";
        }
    }

    result.checksumMismatches         = mismatches;
    result.instancesProduceSameOutput = (mismatches == 0);
}

} // namespace Measure