#include <iostream>
#include <memory>
#include <vector>
#include "BenchmarkTypes.hpp"
#include "VersionA.hpp"
#include "VersionB.hpp"
#include "Benchmark.hpp"
#include "Report.hpp"
#include "JsonExporter.hpp"

int main() {
    std::cout << "\n";
    std::cout << "======================================================\n";
    std::cout << "  Memory Architecture Benchmark\n";
    std::cout << "  Version A: Direct Allocation (current Equalizer)\n";
    std::cout << "  Version B: Marc's AudioSystem + Factory + Pool\n";
    std::cout << "======================================================\n\n";

    BenchmarkResult resultA, resultB;
    resultA.architectureName = "Version A (Direct)";
    resultB.architectureName = "Version B (Marc)";

    std::cout << "> Version A - Direct Allocation\n";
    {
        VersionA::Equalizer eqA(SAMPLE_RATE);

        Measure::timingUnpredictability(
            "Version A",
            [&](float* buf, std::size_t n){ eqA.processBlock(buf, n); },
            resultA);

        Measure::memoryChurn(
            "Version A",
            [&](float* buf, std::size_t n){ eqA.processBlock(buf, n); },
            resultA);

        Measure::fragmentation(
            "Version A",
            [&](float* buf, std::size_t n){ eqA.processBlock(buf, n); },
            resultA);
    }

    Measure::concurrencySafety(
        "Version A",
        []() {
            return std::make_unique<VersionA::Equalizer>(SAMPLE_RATE);
        },
        [](VersionA::Equalizer& eq, float* buf, std::size_t n){
            eq.processBlock(buf, n);
        },
        resultA);

    std::cout << "\n> Version B - Marc's Architecture\n";
    {
        VersionB::AudioSystem sysB(SAMPLE_RATE, BLOCK_SIZE, 16);
        sysB.registerFactory(
            std::make_unique<VersionB::GainEffectFactory>(6.0f));
        sysB.registerFactory(
            std::make_unique<VersionB::BandPassEffectFactory>(1000.f, SAMPLE_RATE));
        sysB.addEffect("gain");
        sysB.addEffect("bandpass");

        Measure::timingUnpredictability(
            "Version B",
            [&](float* buf, std::size_t n){ sysB.processBlock(buf, n); },
            resultB);

        Measure::memoryChurn(
            "Version B",
            [&](float* buf, std::size_t n){ sysB.processBlock(buf, n); },
            resultB);

        Measure::fragmentation(
            "Version B",
            [&](float* buf, std::size_t n){ sysB.processBlock(buf, n); },
            resultB);
    }

    Measure::concurrencySafety(
        "Version B",
        []() {
            auto sys = std::make_unique<VersionB::AudioSystem>(
                SAMPLE_RATE, BLOCK_SIZE, 16);
            sys->registerFactory(
                std::make_unique<VersionB::GainEffectFactory>(6.0f));
            sys->registerFactory(
                std::make_unique<VersionB::BandPassEffectFactory>(
                    1000.f, SAMPLE_RATE));
            sys->addEffect("gain");
            sys->addEffect("bandpass");
            return sys;
        },
        [](VersionB::AudioSystem& sys, float* buf, std::size_t n){
            sys.processBlock(buf, n);
        },
        resultB);

    // Declare results vector — then pass to both printReport and exportJson
    std::vector<BenchmarkResult> results = { resultA, resultB };
    printReport(results);
    exportJson(results);

    return 0;
}