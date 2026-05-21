#include "config.h"

#include <fstream>
#include <string>

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parsePositiveSize(const std::string& text, std::size_t& output) {
    try {
        std::size_t parsedLength = 0;
        const auto value = std::stoull(text, &parsedLength);
        if (parsedLength != text.size() || value == 0) {
            return false;
        }

        output = static_cast<std::size_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

Config Config::load(const std::filesystem::path& configPath) {
    Config config;

    std::ifstream input(configPath);
    if (!input.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = trim(line.substr(0, separator));
        const auto valueText = trim(line.substr(separator + 1));

        std::size_t value = 0;
        if (!parsePositiveSize(valueText, value)) {
            continue;
        }

        if (key == "flush_threshold") {
            config.flushThreshold = value;
        } else if (key == "cache_size") {
            config.cacheSize = value;
        } else if (key == "compaction_threshold") {
            config.compactionThreshold = value;
        }
    }

    return config;
}
