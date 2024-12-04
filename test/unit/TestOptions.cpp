// NOLINTBEGIN(readability-magic-numbers, readability-function-cognitive-complexity)
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Options.h"

using namespace vr::NetworkRttTest;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("formatOptions with default options")
{
    Options options;
    options.interface = "eth0";

    auto result = formatOptions(options);

    CHECK_THAT(result, ContainsSubstring("Options:"));
    CHECK_THAT(result, ContainsSubstring("Interface: eth0"));
    CHECK_THAT(result, ContainsSubstring("Cycletime: 1000 us"));
    CHECK_THAT(result, ContainsSubstring("Lock memory: true"));
    CHECK_THAT(result, ContainsSubstring("Prevent sleep states: true"));
    CHECK_THAT(result, ContainsSubstring("Quiet: false"));
    CHECK_THAT(result, ContainsSubstring("Priority: 49"));
    CHECK_THAT(result, ContainsSubstring("CPU affinity: -1"));
    CHECK_THAT(result, ContainsSubstring("Results path: results"));
    CHECK_THAT(result, ContainsSubstring("Create results subdirectory: false"));
    CHECK_THAT(result, ContainsSubstring("Stats frequency: live mode"));
}

TEST_CASE("formatOptions with custom options")
{
    Options options;
    options.interface = "enp3s0";
    options.cycletime = std::chrono::microseconds{500};
    options.lockMemory = false;
    options.preventSleepStates = false;
    options.quiet = true;
    options.priority = 80;
    options.cpuAffinity = 2;
    options.resultsPath = "/tmp/my_results";
    options.createResultsSubdirectory = true;
    options.statsFrequency = "5s";

    auto result = formatOptions(options);

    CHECK_THAT(result, ContainsSubstring("Interface: enp3s0"));
    CHECK_THAT(result, ContainsSubstring("Cycletime: 500 us"));
    CHECK_THAT(result, ContainsSubstring("Lock memory: false"));
    CHECK_THAT(result, ContainsSubstring("Prevent sleep states: false"));
    CHECK_THAT(result, ContainsSubstring("Quiet: true"));
    CHECK_THAT(result, ContainsSubstring("Priority: 80"));
    CHECK_THAT(result, ContainsSubstring("CPU affinity: 2"));
    CHECK_THAT(result, ContainsSubstring("Results path: /tmp/my_results"));
    CHECK_THAT(result, ContainsSubstring("Create results subdirectory: true"));
    CHECK_THAT(result, ContainsSubstring("Stats frequency: 5s"));
}

TEST_CASE("formatOptions stats frequency live mode when nullopt")
{
    Options options;
    options.interface = "eth0";
    options.statsFrequency = std::nullopt;

    auto result = formatOptions(options);

    CHECK_THAT(result, ContainsSubstring("Stats frequency: live mode"));
}

TEST_CASE("parseStatsFrequency parses valid values")
{
    CHECK(parseStatsFrequency(std::nullopt) == std::nullopt);
    CHECK(parseStatsFrequency(std::optional<std::string>{"250ms"}) == std::chrono::milliseconds{250});
    CHECK(parseStatsFrequency(std::optional<std::string>{"2s"}) == std::chrono::milliseconds{2000});
    CHECK(parseStatsFrequency(std::optional<std::string>{"3m"}) == std::chrono::milliseconds{180000});
    CHECK(parseStatsFrequency(std::optional<std::string>{"1h"}) == std::chrono::milliseconds{3600000});
    CHECK(parseStatsFrequency(std::optional<std::string>{"1.5s"}) == std::chrono::milliseconds{1500});
}

TEST_CASE("parseStatsFrequency rejects invalid values")
{
    CHECK_THROWS_AS(parseStatsFrequency(std::optional<std::string>{"abc"}), std::invalid_argument);
    CHECK_THROWS_AS(parseStatsFrequency(std::optional<std::string>{"10x"}), std::invalid_argument);
}

TEST_CASE("writeOptionsToJson writes configured values")
{
    const auto outputDir = std::filesystem::temp_directory_path() / "network_rtt_test_options_json";
    std::filesystem::create_directories(outputDir);
    const auto outputFile = outputDir / "used_options.json";

    Options options;
    options.interface = "eth0";
    options.cycletime = std::chrono::microseconds{250};
    options.lockMemory = false;
    options.preventSleepStates = true;
    options.quiet = true;
    options.priority = 42;
    options.cpuAffinity = 3;
    options.resultsPath = "/tmp/results path";
    options.createResultsSubdirectory = true;
    options.statsFrequency = "2s";

    writeOptionsToJson(options, outputFile);

    std::ifstream inputFile(outputFile);
    REQUIRE(inputFile.is_open());

    std::ostringstream buffer;
    buffer << inputFile.rdbuf();
    const auto fileContent = buffer.str();

    CHECK_THAT(fileContent, ContainsSubstring("\"interface\": \"eth0\""));
    CHECK_THAT(fileContent, ContainsSubstring("\"cycletime_us\": 250"));
    CHECK_THAT(fileContent, ContainsSubstring("\"lock_memory\": false"));
    CHECK_THAT(fileContent, ContainsSubstring("\"prevent_sleep_states\": true"));
    CHECK_THAT(fileContent, ContainsSubstring("\"quiet\": true"));
    CHECK_THAT(fileContent, ContainsSubstring("\"priority\": 42"));
    CHECK_THAT(fileContent, ContainsSubstring("\"cpu_affinity\": 3"));
    CHECK_THAT(fileContent, ContainsSubstring("\"results_path\": \"/tmp/results path\""));
    CHECK_THAT(fileContent, ContainsSubstring("\"create_results_subdirectory\": true"));
    CHECK_THAT(fileContent, ContainsSubstring("\"stats_frequency\": \"2s\""));

    CHECK(fileContent.find("\"stats_frequency\": \"2s\"") < fileContent.find("\"quiet\": true"));

    std::filesystem::remove(outputFile);
}

TEST_CASE("writeOptionsToJson live mode")
{
    const auto outputDir = std::filesystem::temp_directory_path() / "network_rtt_test_options_json";
    std::filesystem::create_directories(outputDir);
    const auto outputFile = outputDir / "used_options_live_mode.json";

    Options options;
    options.interface = "eth0";
    options.quiet = false;
    options.statsFrequency = std::nullopt;

    writeOptionsToJson(options, outputFile);

    std::ifstream inputFile(outputFile);
    REQUIRE(inputFile.is_open());

    std::ostringstream buffer;
    buffer << inputFile.rdbuf();
    const auto fileContent = buffer.str();

    CHECK_THAT(fileContent, ContainsSubstring("\"stats_frequency\": \"live mode\""));
    CHECK_THAT(fileContent, ContainsSubstring("\"quiet\": false"));
    CHECK(fileContent.find("\"stats_frequency\": \"live mode\"") < fileContent.find("\"quiet\": false"));

    std::filesystem::remove(outputFile);
}

// NOLINTEND(readability-magic-numbers, readability-function-cognitive-complexity)
