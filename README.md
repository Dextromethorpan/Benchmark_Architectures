# Benchmark_Architectures

A headless C++ benchmark that compares two memory management architectures for real-time audio processing.

## What it does

Runs 100 audio blocks through two architectures and measures four memory concepts:

- **Timing Unpredictability** — heap allocations per audio block during steady-state processing
- **Memory Churn** — total `new()` and `delete()` calls during steady-state
- **Memory Fragmentation** — largest contiguous free block before and after the run
- **Concurrency Safety** — two instances running simultaneously, checksum comparison

At the end it prints a comparison table to the console and writes `results.json` for the BenchmarkViewer.

## The two architectures

**Version A — Direct Allocation**
Simulates a naive DSP architecture where effects allocate scratch buffers on every audio block using `new` and `delete`. Represents the current Equalizer codebase style.

**Version B — Marc's Architecture**
Implements Marc Gallo's suggestion: an `AudioSystem` class that owns a `MemoryPool` and exposes an `IEffectFactory` as a service. All memory is allocated once at startup. Zero heap allocations during steady-state processing.

## Requirements

- Windows 10 or later
- Visual Studio 2022
- CMake 3.20 or later
- C++20

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Run

```bash
.\Release\MemoryBenchmark.exe
```

This produces `results.json` in the `Release\` folder. Open it with BenchmarkViewer to see the results visually.

## Output

```
> Version A - Direct Allocation
> Version B - Marc's Architecture

  Timing Unpredictability
  Memory Churn
  Memory Fragmentation
  Concurrency Safety

  Version A (Direct)    2/4 concepts passed   needs attention
  Version B (Marc)      4/4 concepts passed   real-time safe

  Results written to: results.json
```

## Project structure

```
Benchmark_Architectures/
  BenchmarkTypes.hpp     shared types, counters, timer, checksum
  VersionA.hpp           direct allocation architecture
  VersionB.hpp           Marc's AudioSystem + Factory + Pool
  Benchmark.hpp          four measurement functions
  Report.hpp             console output formatter
  JsonExporter.hpp       writes results.json
  main.cpp               entry point
  CMakeLists.txt         build configuration
```

## Related project

[BenchmarkViewer](https://github.com/Dextromethorpan/Benchmark_Viewer) — Qt6 desktop app that reads `results.json` and displays the results in a terminal-style dashboard.
