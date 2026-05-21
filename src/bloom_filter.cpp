#include "bloom_filter.h"

#include <algorithm>
#include <functional>

BloomFilter::BloomFilter(std::size_t bitCount, std::size_t hashFunctionCount)
    : bitCount_(std::max<std::size_t>(bitCount, 1)),
      hashFunctionCount_(std::max<std::size_t>(hashFunctionCount, 1)),
      bits_(bitCount_, 0) {
}

void BloomFilter::add(const std::string& key) {
    for (std::size_t i = 0; i < hashFunctionCount_; ++i) {
        bits_[hash(key, i)] = 1;
    }
}

bool BloomFilter::possiblyContains(const std::string& key) const {
    for (std::size_t i = 0; i < hashFunctionCount_; ++i) {
        if (bits_[hash(key, i)] == 0) {
            return false;
        }
    }

    return true;
}

std::size_t BloomFilter::hash(const std::string& key, std::uint64_t seed) const {
    const std::uint64_t mixedSeed = 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    const auto hashValue = std::hash<std::string>{}(key + "#" + std::to_string(mixedSeed));
    return hashValue % bitCount_;
}
