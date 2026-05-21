#include "db.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

namespace {

namespace fs = std::filesystem;

constexpr int OperationCount = 10000;
constexpr int HotKeyCount = 100;
constexpr int HotQueryPercent = 80;

class NullBuffer : public std::streambuf {
protected:
    int overflow(int ch) override {
        return traits_type::not_eof(ch);
    }
};

class ScopedCoutSilencer {
public:
    ScopedCoutSilencer()
        : originalBuffer_(std::cout.rdbuf(&nullBuffer_)) {
    }

    ~ScopedCoutSilencer() {
        std::cout.rdbuf(originalBuffer_);
    }

private:
    NullBuffer nullBuffer_;
    std::streambuf* originalBuffer_;
};

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

fs::path resolveBenchmarkDataDirectory() {
    return findProjectRoot(fs::current_path()) / "data" / "benchmark";
}

fs::path resolveConfigPath() {
    return findProjectRoot(fs::current_path()) / "data" / "config.txt";
}

double toSeconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double>(duration).count();
}

double tps(int operations, double seconds) {
    if (seconds <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(operations) / seconds;
}

std::string keyFor(int index) {
    return "key_" + std::to_string(index);
}

std::string valueFor(int index) {
    return "value_" + std::to_string(index);
}

} // namespace

int main() {
    const auto dataDirectory = resolveBenchmarkDataDirectory();
    fs::remove_all(dataDirectory);

    DB db(dataDirectory, resolveConfigPath());

    const auto putStart = std::chrono::steady_clock::now();
    for (int i = 0; i < OperationCount; ++i) {
        db.put(keyFor(i), valueFor(i));
    }
    const auto putEnd = std::chrono::steady_clock::now();

    int foundCount = 0;
    std::mt19937 randomEngine(42);
    std::uniform_int_distribution<int> percentDistribution(1, 100);
    std::uniform_int_distribution<int> hotKeyDistribution(0, HotKeyCount - 1);
    std::uniform_int_distribution<int> allKeyDistribution(0, OperationCount - 1);

    const auto getStart = std::chrono::steady_clock::now();
    {
        ScopedCoutSilencer silenceDebugLogs;
        for (int i = 0; i < OperationCount; ++i) {
            const int keyIndex = percentDistribution(randomEngine) <= HotQueryPercent
                                     ? hotKeyDistribution(randomEngine)
                                     : allKeyDistribution(randomEngine);
            const auto value = db.get(keyFor(keyIndex));
            if (value.has_value() && *value == valueFor(keyIndex)) {
                ++foundCount;
            }
        }
    }
    const auto getEnd = std::chrono::steady_clock::now();

    if (foundCount != OperationCount) {
        throw std::runtime_error("benchmark read verification failed");
    }

    const double putSeconds = toSeconds(putEnd - putStart);
    const double getSeconds = toSeconds(getEnd - getStart);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "MiniKV Benchmark\n";
    std::cout << "Operations: " << OperationCount << '\n';
    std::cout << "Put total time: " << putSeconds << " s\n";
    std::cout << "Get total time: " << getSeconds << " s\n";
    std::cout << "Write TPS: " << tps(OperationCount, putSeconds) << '\n';
    std::cout << "Read TPS: " << tps(OperationCount, getSeconds) << '\n';
    std::cout << "Flush count: " << db.flushCount() << '\n';
    std::cout << "SSTable count: " << db.sstableCount() << '\n';
    std::cout << "Cache hit count: " << db.cacheHitCount() << '\n';
    std::cout << "Cache miss count: " << db.cacheMissCount() << '\n';
    std::cout << "Cache hit ratio: " << db.cacheHitRatio() * 100.0 << "%\n";
    std::cout << "Bloom filter skip count: " << db.bloomFilterSkipCount() << '\n';
    std::cout << "Bloom filter false positive count: "
              << db.bloomFilterFalsePositiveCount() << '\n';

    return 0;
}
