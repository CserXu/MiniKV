cmake_minimum_required(VERSION 3.10)

project(MiniKV)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MINIKV_CORE_SOURCES
    src/bloom_filter.h
    src/bloom_filter.cpp
    src/config.h
    src/config.cpp
    src/db.h
    src/db.cpp
    src/lru_cache.h
    src/lru_cache.cpp
    src/memtable.h
    src/memtable.cpp
    src/sstable.h
    src/sstable.cpp
    src/wal.h
    src/wal.cpp
)

add_executable(MiniKV
    src/main.cpp
    ${MINIKV_CORE_SOURCES}
)

add_executable(MiniKVBenchmark
    src/benchmark.cpp
    ${MINIKV_CORE_SOURCES}
)

target_include_directories(MiniKV PRIVATE src)
target_include_directories(MiniKVBenchmark PRIVATE src)

set_property(TARGET MiniKV PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_property(TARGET MiniKVBenchmark PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
