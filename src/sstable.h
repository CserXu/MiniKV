#pragma once

#include "bloom_filter.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class SSTable {
public:
    explicit SSTable(std::filesystem::path dataDirectory, std::size_t compactionThreshold = 3);

    void flush(const std::unordered_map<std::string, std::string>& entries);
    std::optional<std::string> get(const std::string& key) const;
    std::size_t fileCount() const;
    std::size_t bloomFilterSkipCount() const;
    std::size_t bloomFilterFalsePositiveCount() const;

private:
    static constexpr const char* CompactFileName = "compact_1.data";
    static constexpr const char* TombstoneValue = "__DELETE__";

    struct TableFile {
        std::filesystem::path path;
        BloomFilter bloomFilter;
    };

    void loadExistingFiles();
    void compactIfNeeded();
    void compact();
    std::filesystem::path filePathForIndex(int index) const;
    std::filesystem::path compactFilePath() const;
    static std::optional<int> parseFileIndex(const std::filesystem::path& path);
    static std::optional<std::string> getFromFile(const std::filesystem::path& path,
                                                  const std::string& key);
    static void mergeFileInto(const std::filesystem::path& path,
                              std::unordered_map<std::string, std::string>& merged);
    static void writeEntriesToFile(const std::filesystem::path& path,
                                   const std::unordered_map<std::string, std::string>& entries);
    static BloomFilter buildBloomFilter(const std::unordered_map<std::string, std::string>& entries);
    static BloomFilter buildBloomFilterFromFile(const std::filesystem::path& path);

    std::optional<std::string> getFromRegularFiles(const std::string& key) const;
    std::optional<std::string> getFromTableFile(const TableFile& tableFile,
                                                const std::string& key) const;

    std::filesystem::path dataDirectory_;
    std::vector<TableFile> files_;
    std::optional<TableFile> compactFile_;
    mutable std::size_t bloomFilterSkipCount_;
    mutable std::size_t bloomFilterFalsePositiveCount_;
    std::size_t compactionThreshold_;
    int nextFileIndex_;
};
