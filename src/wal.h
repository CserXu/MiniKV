#pragma once

#include <filesystem>
#include <functional>
#include <string>

class WAL {
public:
    enum class OperationType {
        Put,
        Delete
    };

    struct Record {
        OperationType type;
        std::string key;
        std::string value;
    };

    explicit WAL(std::filesystem::path path);

    void appendPut(const std::string& key, const std::string& value) const;
    void appendDelete(const std::string& key) const;
    void clear() const;
    void replay(const std::function<void(const Record&)>& apply) const;

private:
    void ensureParentDirectory() const;

    std::filesystem::path path_;
};
