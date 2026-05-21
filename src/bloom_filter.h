#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class BloomFilter {
public:
    explicit BloomFilter(std::size_t bitCount = 8192, std::size_t hashFunctionCount = 4);

    void add(const std::string& key);
    bool possiblyContains(const std::string& key) const;

private:
    std::size_t hash(const std::string& key, std::uint64_t seed) const;

    std::size_t bitCount_;
    std::size_t hashFunctionCount_;
    std::vector<std::uint8_t> bits_;
};
