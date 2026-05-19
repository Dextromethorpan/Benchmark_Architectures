#pragma once
#include "BenchmarkTypes.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────
// Prints a formatted comparison report to stdout.
// ─────────────────────────────────────────────────────────────

static std::string mbStr(std::size_t bytes) {
    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << mb << " MB";
    return ss.str();
}

static void printReport(const std::vector<BenchmarkResult>& results) {
    constexpr int W = 42;
    const std::string line(W * 2 + 3, '-');

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          MEMORY ARCHITECTURE BENCHMARK — RESULTS COMPARISON                     ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════╝\n\n";

    // Header row
    std::cout << std::left
              << std::setw(36) << "Metric"
              << std::setw(30) << results[0].architectureName
              << std::setw(30) << results[1].architectureName
              << "  Better?\n";
    std::cout << std::string(96, '-') << "\n";

    // ── Section 1: Timing Unpredictability ───────────────────
    std::cout << "\n  TIMING UNPREDICTABILITY\n";
    std::cout << "  (Should be 0 allocs/block in steady-state for real-time safety)\n\n";

    std::cout << std::left
              << std::setw(36) << "  Avg allocs per audio block"
              << std::setw(30) << results[0].allocsPerBlock
              << std::setw(30) << results[1].allocsPerBlock
              << "  " << (results[0].allocsPerBlock <= results[1].allocsPerBlock
                          ? results[0].architectureName : results[1].architectureName)
              << "\n";

    std::cout << std::left
              << std::setw(36) << "  Avg block latency (µs)"
              << std::setw(30) << std::fixed << std::setprecision(2)
                               << results[0].avgBlockLatencyUs
              << std::setw(30) << results[1].avgBlockLatencyUs
              << "  " << (results[0].avgBlockLatencyUs <= results[1].avgBlockLatencyUs
                          ? results[0].architectureName : results[1].architectureName)
              << "\n";

    // ── Section 2: Memory Churn ───────────────────────────────
    std::cout << "\n  MEMORY CHURN  (" << MEASURED_BLOCKS << " blocks, steady-state only)\n";
    std::cout << "  (Lower = more stable memory landscape during runtime)\n\n";

    std::cout << std::left
              << std::setw(36) << "  Total new() calls"
              << std::setw(30) << results[0].totalAllocsRuntime
              << std::setw(30) << results[1].totalAllocsRuntime
              << "  " << (results[0].totalAllocsRuntime <= results[1].totalAllocsRuntime
                          ? results[0].architectureName : results[1].architectureName)
              << "\n";

    std::cout << std::left
              << std::setw(36) << "  Total delete() calls"
              << std::setw(30) << results[0].totalFreesRuntime
              << std::setw(30) << results[1].totalFreesRuntime
              << "  " << (results[0].totalFreesRuntime <= results[1].totalFreesRuntime
                          ? results[0].architectureName : results[1].architectureName)
              << "\n";

    // ── Section 3: Fragmentation ─────────────────────────────
    std::cout << "\n  MEMORY FRAGMENTATION\n";
    std::cout << "  (Largest contiguous free block — shrinking value = fragmentation growing)\n\n";

    std::cout << std::left
              << std::setw(36) << "  Largest block BEFORE run"
              << std::setw(30) << mbStr(results[0].fragBefore)
              << std::setw(30) << mbStr(results[1].fragBefore)
              << "\n";

    std::cout << std::left
              << std::setw(36) << "  Largest block AFTER run"
              << std::setw(30) << mbStr(results[0].fragAfter)
              << std::setw(30) << mbStr(results[1].fragAfter)
              << "\n";

    long long dropA = static_cast<long long>(results[0].fragBefore)
                    - static_cast<long long>(results[0].fragAfter);
    long long dropB = static_cast<long long>(results[1].fragBefore)
                    - static_cast<long long>(results[1].fragAfter);

    std::cout << std::left
              << std::setw(36) << "  Fragmentation growth"
              << std::setw(30) << mbStr(static_cast<std::size_t>(std::max(0LL, dropA)))
              << std::setw(30) << mbStr(static_cast<std::size_t>(std::max(0LL, dropB)))
              << "  " << (dropA <= dropB
                          ? results[0].architectureName : results[1].architectureName)
              << "\n";

    // ── Section 4: Concurrency Safety ────────────────────────
    std::cout << "\n  CONCURRENCY SAFETY  (two instances, simultaneous threads, 50 rounds)\n";
    std::cout << "  (Mismatch = shared state leaking between instances = memory safety violation)\n\n";

    std::cout << std::left
              << std::setw(36) << "  Checksum mismatches"
              << std::setw(30) << results[0].checksumMismatches
              << std::setw(30) << results[1].checksumMismatches
              << "  " << (results[0].checksumMismatches <= results[1].checksumMismatches
                          ? results[0].architectureName : results[1].architectureName)
              << "\n";

    std::cout << std::left
              << std::setw(36) << "  Instances independent?"
              << std::setw(30) << (results[0].instancesProduceSameOutput ? "YES ✓" : "NO  ✗")
              << std::setw(30) << (results[1].instancesProduceSameOutput ? "YES ✓" : "NO  ✗")
              << "\n";

    // ── Summary ───────────────────────────────────────────────
    std::cout << "\n" << std::string(96, '=') << "\n";
    std::cout << "  SUMMARY\n\n";

    for (auto& r : results) {
        int score = 0;
        if (r.allocsPerBlock    == 0)  ++score;
        if (r.totalAllocsRuntime < 10) ++score;
        if ((static_cast<long long>(r.fragBefore) -
             static_cast<long long>(r.fragAfter)) <= 0) ++score;
        if (r.checksumMismatches == 0) ++score;

        std::cout << "  " << std::left << std::setw(30) << r.architectureName
                  << score << "/4 concepts passed"
                  << (score == 4 ? "  ← real-time safe" : "  ← needs attention")
                  << "\n";
    }

    std::cout << "\n";
}