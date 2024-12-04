#include "Options.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace vr::NetworkRttTest
{

std::string formatOptions(const Options& options)
{
    std::string result = std::format(
        "Options:\n"
        "  Interface: {}\n"
        "  Cycletime: {} us\n"
        "  Lock memory: {}\n"
        "  Prevent sleep states: {}\n"
        "  Priority: {}\n"
        "  CPU affinity: {}\n"
        "  Results path: {}\n"
        "  Create results subdirectory: {}\n",
        options.interface, options.cycletime.count(), options.lockMemory ? "true" : "false",
        options.preventSleepStates ? "true" : "false", options.priority, options.cpuAffinity,
        options.resultsPath.string(), options.createResultsSubdirectory ? "true" : "false");
    if (options.statsFrequency.has_value())
    {
        result += std::format("  Stats frequency: {}\n", options.statsFrequency.value());
    }
    else
    {
        result += "  Stats frequency: live mode\n";
    }
    result += std::format("  Quiet: {}\n", options.quiet ? "true" : "false");
    return result;
}

void printOptions(const Options& options, std::ostream& out)
{
    out << formatOptions(options);
}

std::optional<std::chrono::milliseconds> parseStatsFrequency(const std::optional<std::string>& statsFrequencyOption)
{
    if (!statsFrequencyOption.has_value())
    {
        return std::nullopt;
    }
    const auto& value = statsFrequencyOption.value();

    size_t parsedChars = 0;
    double amount = 0.0;
    try
    {
        amount = std::stod(value, &parsedChars);
    } catch (const std::exception&)
    {
        throw std::invalid_argument("Invalid value for stats frequency: " + value);
    }

    std::string_view suffix(value.c_str() + parsedChars, value.size() - parsedChars);
    constexpr std::array<std::pair<std::string_view, double>, 4> suffixMultipliers{{
        {"ms", 1.0},
        {"s", 1000.0},
        {"m", 60.0 * 1000.0},
        {"h", 60.0 * 60.0 * 1000.0},
    }};

    const auto* multiplierIt = std::find_if(suffixMultipliers.begin(), suffixMultipliers.end(),
                                            [&](const auto& item) { return item.first == suffix; });

    if (multiplierIt == suffixMultipliers.end())
    {
        throw std::invalid_argument("Invalid suffix for stats frequency: " + std::string(suffix) +
                                    ". Valid suffixes are ms, s, m, h.");
    }

    return std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(amount * multiplierIt->second));
}

void writeOptionsToJson(const Options& options, const std::filesystem::path& filePath)
{
    std::ofstream out(filePath);
    if (!out.is_open())
    {
        throw std::runtime_error("Failed to open options json file for writing: " + filePath.string());
    }

    out << "{\n";
    out << "  \"interface\": \"" << options.interface << "\",\n";
    out << "  \"cycletime_us\": " << options.cycletime.count() << ",\n";
    out << "  \"lock_memory\": " << (options.lockMemory ? "true" : "false") << ",\n";
    out << "  \"prevent_sleep_states\": " << (options.preventSleepStates ? "true" : "false") << ",\n";
    out << "  \"priority\": " << options.priority << ",\n";
    out << "  \"cpu_affinity\": " << options.cpuAffinity << ",\n";
    out << "  \"results_path\": \"" << options.resultsPath.string() << "\",\n";
    out << "  \"create_results_subdirectory\": " << (options.createResultsSubdirectory ? "true" : "false") << ",\n";
    out << "  \"stats_frequency\": ";
    if (options.statsFrequency.has_value())
    {
        out << "\"" << options.statsFrequency.value() << "\",\n";
    }
    else
    {
        out << "\"live mode\",\n";
    }
    out << "  \"quiet\": " << (options.quiet ? "true" : "false") << "\n";
    out << "}\n";

    if (!out)
    {
        throw std::runtime_error("Failed to write options json file: " + filePath.string());
    }
}

}  // namespace vr::NetworkRttTest
