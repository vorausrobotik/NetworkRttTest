#pragma once
#include <chrono>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>

namespace vr::NetworkRttTest
{
constexpr int DEFAULT_PRIORITY = 49;
constexpr std::chrono::microseconds DEFAULT_CYCLETIME{1000};

struct Options
{
    std::string interface;
    std::chrono::microseconds cycletime{DEFAULT_CYCLETIME};
    bool lockMemory{true};
    bool preventSleepStates{true};
    int priority{DEFAULT_PRIORITY};  // -1 means SCHED_OTHER, otherwise SCHED_FIFO with the given priority
    int cpuAffinity{-1};             // -1 means no affinity, otherwise set affinity to the given cpu
    std::filesystem::path resultsPath = "results";
    bool createResultsSubdirectory{false};
    std::optional<std::string> statsFrequency;
    bool quiet{false};
    // Developer Note: if you add new options here, make sure to also update the following functions:
    // - formatOptions
    // - writeOptionsToJson
};

std::string formatOptions(const Options& options);
void printOptions(const Options& options, std::ostream& out);
std::optional<std::chrono::milliseconds> parseStatsFrequency(const std::optional<std::string>& statsFrequencyOption);
void writeOptionsToJson(const Options& options, const std::filesystem::path& filePath);

}  // namespace vr::NetworkRttTest