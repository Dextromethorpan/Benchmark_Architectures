#pragma once
#include "BenchmarkTypes.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────
// JsonExporter
// Writes BenchmarkResult vector to a JSON file.
// The Qt viewer reads this file — no coupling between the two.
// ─────────────────────────────────────────────────────────────

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else                out += c;
    }
    return out;
}

static void exportJson(const std::vector<BenchmarkResult>& results,
                       const std::string& path = "results.json") {
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot write " + path);

    f << "{\n";
    f << "  \"benchmark\": \"Memory Architecture Comparison\",\n";
    f << "  \"blocks_measured\": " << MEASURED_BLOCKS << ",\n";
    f << "  \"block_size\": "      << BLOCK_SIZE      << ",\n";
    f << "  \"sample_rate\": "     << SAMPLE_RATE     << ",\n";
    f << "  \"results\": [\n";

    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        f << "    {\n";
        f << "      \"name\": \""              << escapeJson(r.architectureName) << "\",\n";

        // Timing unpredictability
        f << "      \"timing\": {\n";
        f << "        \"allocs_per_block\": "  << r.allocsPerBlock        << ",\n";
        f << "        \"avg_latency_us\": "    << r.avgBlockLatencyUs     << ",\n";
        f << "        \"passed\": "            << (r.allocsPerBlock == 0 ? "true" : "false") << "\n";
        f << "      },\n";

        // Memory churn
        f << "      \"churn\": {\n";
        f << "        \"total_allocs\": "      << r.totalAllocsRuntime    << ",\n";
        f << "        \"total_frees\": "       << r.totalFreesRuntime     << ",\n";
        f << "        \"passed\": "            << (r.totalAllocsRuntime < 10 ? "true" : "false") << "\n";
        f << "      },\n";

        // Fragmentation
        long long drop = static_cast<long long>(r.fragBefore)
                       - static_cast<long long>(r.fragAfter);
        f << "      \"fragmentation\": {\n";
        f << "        \"before_mb\": "         << (r.fragBefore / (1024.0 * 1024.0)) << ",\n";
        f << "        \"after_mb\": "          << (r.fragAfter  / (1024.0 * 1024.0)) << ",\n";
        f << "        \"growth_mb\": "         << (drop > 0 ? drop / (1024.0*1024.0) : 0.0) << ",\n";
        f << "        \"passed\": "            << (drop <= 0 ? "true" : "false") << "\n";
        f << "      },\n";

        // Concurrency safety
        f << "      \"concurrency\": {\n";
        f << "        \"checksum_mismatches\": " << r.checksumMismatches << ",\n";
        f << "        \"instances_independent\": "
          << (r.instancesProduceSameOutput ? "true" : "false") << ",\n";
        f << "        \"passed\": "
          << (r.checksumMismatches == 0 ? "true" : "false") << "\n";
        f << "      }\n";

        f << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }

    f << "  ]\n";
    f << "}\n";

    std::cout << "\n  Results written to: " << path << "\n";
}