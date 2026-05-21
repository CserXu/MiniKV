#pragma once

#include <cstddef>
#include <filesystem>

struct Config {
    static constexpr std::size_t DefaultFlushThreshold = 5;
    static constexpr std::size_t DefaultCacheSize = 100;
    static constexpr std::size_t DefaultCompactionThreshold = 3;

    std::size_t flushThreshold = DefaultFlushThreshold;
    std::size_t cacheSize = DefaultCacheSize;
    std::size_t compactionThreshold = DefaultCompactionThreshold;

    static Config load(const std::filesystem::path& configPath);
};
