#include "sstable.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

bool isDigits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

std::size_t bloomBitCountForKeyCount(std::size_t keyCount) {
    return std::max<std::size_t>(8192, keyCount * 16);
}

} // namespace

SSTable::SSTable(std::filesystem::path dataDirectory, std::size_t compactionThreshold)
    : dataDirectory_(std::move(dataDirectory)),
      bloomFilterSkipCount_(0),
      bloomFilterFalsePositiveCount_(0),
      compactionThreshold_(std::max<std::size_t>(compactionThreshold, 1)),
      nextFileIndex_(1) {
    loadExistingFiles();
    compactIfNeeded();
}

void SSTable::flush(const std::unordered_map<std::string, std::string>& entries) {
    if (entries.empty()) {
        return;
    }

    const auto filePath = filePathForIndex(nextFileIndex_);
    writeEntriesToFile(filePath, entries);

    files_.push_back({filePath, buildBloomFilter(entries)});
    ++nextFileIndex_;
    compactIfNeeded();
}

std::optional<std::string> SSTable::get(const std::string& key) const {
    if (compactFile_.has_value()) {
        const auto compactValue = getFromTableFile(*compactFile_, key);
        if (compactValue.has_value()) {
            const auto newerValue = getFromRegularFiles(key);
            if (newerValue.has_value()) {
                return newerValue;
            }

            return compactValue;
        }
    }

    return getFromRegularFiles(key);
}

std::size_t SSTable::fileCount() const {
    return files_.size() + (compactFile_.has_value() ? 1 : 0);
}

std::size_t SSTable::bloomFilterSkipCount() const {
    return bloomFilterSkipCount_;
}

std::size_t SSTable::bloomFilterFalsePositiveCount() const {
    return bloomFilterFalsePositiveCount_;
}

void SSTable::loadExistingFiles() {
    files_.clear();
    compactFile_.reset();
    nextFileIndex_ = 1;

    if (!std::filesystem::exists(dataDirectory_)) {
        return;
    }

    std::vector<std::pair<int, std::filesystem::path>> indexedFiles;
    for (const auto& entry : std::filesystem::directory_iterator(dataDirectory_)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().filename().string() == CompactFileName) {
            compactFile_ = TableFile{entry.path(), buildBloomFilterFromFile(entry.path())};
            continue;
        }

        const auto index = parseFileIndex(entry.path());
        if (index.has_value()) {
            indexedFiles.emplace_back(*index, entry.path());
        }
    }

    std::sort(indexedFiles.begin(), indexedFiles.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& indexedFile : indexedFiles) {
        files_.push_back({indexedFile.second, buildBloomFilterFromFile(indexedFile.second)});
        nextFileIndex_ = std::max(nextFileIndex_, indexedFile.first + 1);
    }
}

void SSTable::compactIfNeeded() {
    if (files_.size() >= compactionThreshold_) {
        compact();
    }
}

void SSTable::compact() {
    std::unordered_map<std::string, std::string> merged;

    if (compactFile_.has_value()) {
        mergeFileInto(compactFile_->path, merged);
    }

    for (const auto& file : files_) {
        mergeFileInto(file.path, merged);
    }

    for (auto it = merged.begin(); it != merged.end();) {
        if (it->second == TombstoneValue) {
            it = merged.erase(it);
        } else {
            ++it;
        }
    }

    const auto outputPath = compactFilePath();
    writeEntriesToFile(outputPath, merged);

    for (const auto& file : files_) {
        std::filesystem::remove(file.path);
    }

    files_.clear();
    compactFile_ = TableFile{outputPath, buildBloomFilter(merged)};
    nextFileIndex_ = 1;
}

std::filesystem::path SSTable::filePathForIndex(int index) const {
    return dataDirectory_ / ("sst_" + std::to_string(index) + ".data");
}

std::filesystem::path SSTable::compactFilePath() const {
    return dataDirectory_ / CompactFileName;
}

std::optional<int> SSTable::parseFileIndex(const std::filesystem::path& path) {
    const std::string filename = path.filename().string();
    const std::string prefix = "sst_";
    const std::string suffix = ".data";

    if (filename.size() <= prefix.size() + suffix.size()) {
        return std::nullopt;
    }

    if (filename.compare(0, prefix.size(), prefix) != 0) {
        return std::nullopt;
    }

    if (filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return std::nullopt;
    }

    const auto indexText = filename.substr(prefix.size(),
                                           filename.size() - prefix.size() - suffix.size());
    if (!isDigits(indexText)) {
        return std::nullopt;
    }

    return std::stoi(indexText);
}

std::optional<std::string> SSTable::getFromFile(const std::filesystem::path& path,
                                                const std::string& key) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open SSTable file for read: " + path.string());
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        const auto currentKey = line.substr(0, separator);
        if (currentKey == key) {
            return line.substr(separator + 1);
        }
    }

    return std::nullopt;
}

void SSTable::mergeFileInto(const std::filesystem::path& path,
                            std::unordered_map<std::string, std::string>& merged) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open SSTable file for compaction: " + path.string());
    }

    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        merged[line.substr(0, separator)] = line.substr(separator + 1);
    }
}

void SSTable::writeEntriesToFile(const std::filesystem::path& path,
                                 const std::unordered_map<std::string, std::string>& entries) {
    const auto parentPath = path.parent_path();
    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open SSTable file for write: " + path.string());
    }

    const std::map<std::string, std::string> sortedEntries(entries.begin(), entries.end());
    for (const auto& entry : sortedEntries) {
        output << entry.first << ':' << entry.second << '\n';
    }
    output.flush();

    if (!output.good()) {
        throw std::runtime_error("failed to write SSTable file: " + path.string());
    }
}

BloomFilter SSTable::buildBloomFilter(const std::unordered_map<std::string, std::string>& entries) {
    BloomFilter bloomFilter(bloomBitCountForKeyCount(entries.size()));
    for (const auto& entry : entries) {
        bloomFilter.add(entry.first);
    }
    return bloomFilter;
}

BloomFilter SSTable::buildBloomFilterFromFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open SSTable file for Bloom Filter build: " + path.string());
    }

    std::vector<std::string> keys;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        keys.push_back(line.substr(0, separator));
    }

    BloomFilter bloomFilter(bloomBitCountForKeyCount(keys.size()));
    for (const auto& key : keys) {
        bloomFilter.add(key);
    }

    return bloomFilter;
}

std::optional<std::string> SSTable::getFromRegularFiles(const std::string& key) const {
    for (auto it = files_.rbegin(); it != files_.rend(); ++it) {
        const auto value = getFromTableFile(*it, key);
        if (value.has_value()) {
            return value;
        }
    }

    return std::nullopt;
}

std::optional<std::string> SSTable::getFromTableFile(const TableFile& tableFile,
                                                     const std::string& key) const {
    if (!tableFile.bloomFilter.possiblyContains(key)) {
        ++bloomFilterSkipCount_;
        return std::nullopt;
    }

    const auto value = getFromFile(tableFile.path, key);
    if (!value.has_value()) {
        ++bloomFilterFalsePositiveCount_;
    }

    return value;
}
