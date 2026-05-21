#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

class MemTable {
public:
    static constexpr const char* TombstoneValue = "__DELETE__";

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    void remove(const std::string& key);
    void clear();
    bool empty() const;
    std::size_t size() const;
    const std::unordered_map<std::string, std::string>& entries() const;

private:
    std::unordered_map<std::string, std::string> data_;
};
