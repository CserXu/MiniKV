#include "db.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace {

namespace fs = std::filesystem;

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return "";
    }
    return value.substr(first);
}

void printHelp() {
    std::cout << "Commands:\n"
              << "  put <key> <value>\n"
              << "  get <key>\n"
              << "  delete <key>\n"
              << "  exit\n";
}

bool isProjectRoot(const fs::path& path) {
    return fs::exists(path / "CMakeLists.txt") && fs::is_directory(path / "src");
}

fs::path findProjectRoot(fs::path start) {
    start = fs::absolute(std::move(start));

    while (!start.empty()) {
        if (isProjectRoot(start)) {
            return start;
        }

        const auto parent = start.parent_path();
        if (parent == start) {
            break;
        }
        start = parent;
    }

    return fs::current_path();
}

fs::path resolveDataDirectory() {
    return findProjectRoot(fs::current_path()) / "data";
}

} // namespace

int main() {
    DB db(resolveDataDirectory());
    std::string line;

    std::cout << "MiniKV started. Type 'help' for commands.\n";

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);
        std::string command;
        input >> command;

        command = toLower(command);
        if (command.empty()) {
            continue;
        }

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "help") {
            printHelp();
            continue;
        }

        if (command == "put") {
            std::string key;
            input >> key;

            std::string value;
            std::getline(input, value);
            value = trimLeft(value);

            if (key.empty() || value.empty()) {
                std::cout << "Usage: put <key> <value>\n";
                continue;
            }

            db.put(key, value);
            std::cout << "OK\n";
            continue;
        }

        if (command == "get") {
            std::string key;
            input >> key;

            if (key.empty()) {
                std::cout << "Usage: get <key>\n";
                continue;
            }

            const auto value = db.get(key);
            if (value.has_value()) {
                std::cout << *value << '\n';
            } else {
                std::cout << "NOT FOUND\n";
            }
            continue;
        }

        if (command == "delete" || command == "remove") {
            std::string key;
            input >> key;

            if (key.empty()) {
                std::cout << "Usage: delete <key>\n";
                continue;
            }

            if (db.remove(key)) {
                std::cout << "OK\n";
            } else {
                std::cout << "NOT FOUND\n";
            }
            continue;
        }

        std::cout << "Unknown command: " << command << '\n';
    }

    std::cout << "MiniKV stopped.\n";
    return 0;
}
