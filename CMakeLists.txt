cmake_minimum_required(VERSION 3.20)
project(MemoryBenchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only main.cpp is required — CMake does not need headers to configure.
# Headers listed via target_sources appear in the VS solution explorer.
add_executable(MemoryBenchmark main.cpp)

target_sources(MemoryBenchmark PRIVATE
    BenchmarkTypes.hpp
    VersionA.hpp
    VersionB.hpp
    Benchmark.hpp
    Report.hpp
    JsonExporter.hpp
)

# MSVC: enable standard exception handling
if(MSVC)
    target_compile_options(MemoryBenchmark PRIVATE /EHsc)
endif()

# GCC/MinGW: link pthreads for std::thread
if(NOT MSVC)
    target_link_libraries(MemoryBenchmark PRIVATE -lpthread)
endif()