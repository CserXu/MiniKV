#include "wal.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

std::string trimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return "";
    }
    return value.substr(first);
}

} // namespace

WAL::WAL(std::filesystem::path path)
    : path_(std::move(path)) {
}

void WAL::appendPut(const std::string& key, const std::string& value) const {
    ensureParentDirectory();

    std::ofstream output(path_, std::ios::app);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open WAL file for append: " + path_.string());
    }

    output << "PUT " << key << ' ' << value << '\n';
    output.flush();

    if (!output.good()) {
        throw std::runtime_error("failed to write PUT record to WAL: " + path_.string());
    }
}

void WAL::appendDelete(const std::string& key) const {
    ensureParentDirectory();

    std::ofstream output(path_, std::ios::app);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open WAL file for append: " + path_.string());
    }

    output << "DEL " << key << '\n';
    output.flush();

    if (!output.good()) {
        throw std::runtime_error("failed to write DEL record to WAL: " + path_.string());
    }
}

void WAL::clear() const {
    ensureParentDirectory();

    std::ofstream output(path_, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to clear WAL file: " + path_.string());
    }
}

void WAL::replay(const std::function<void(const Record&)>& apply) const {
    std::ifstream input(path_);
    if (!input.is_open()) {
        if (!std::filesystem::exists(path_)) {
            return;
        }
        throw std::runtime_error("failed to open WAL file for replay: " + path_.string());
    }

    std::string line;
    while (std::getline(input, line)) {
        std::istringstream recordInput(line);
        std::string operation;
        recordInput >> operation;

        if (operation == "PUT") {
            std::string key;
            recordInput >> key;

            std::string value;
            std::getline(recordInput, value);
            value = trimLeft(value);

            if (!key.empty()) {
                apply({OperationType::Put, key, value});
            }
            continue;
        }

        if (operation == "DEL") {
            std::string key;
            recordInput >> key;

            if (!key.empty()) {
                apply({OperationType::Delete, key, ""});
            }
        }
    }
}

void WAL::ensureParentDirectory() const {
    const auto parentPath = path_.parent_path();
    if (!parentPath.empty()) {
        std::filesystem::create_directories(parentPath);
    }
}
