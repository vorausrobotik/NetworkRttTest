#include <RTUtils/RtBenchmark.h>
#include <RTUtils/core.h>
#include <CLI/CLI.hpp>
#include <csignal>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <thread>

#include "Benchmark.h"
#include "RSock.h"

using namespace vr::NetworkRttTest;
using namespace std::chrono_literals;

constexpr int LIVE_STATS_OUTPUT_LINES = 10;

std::atomic_bool stopSignal;
std::atomic_bool dumpResults;

void handleSignal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        stopSignal = true;
    }
    else if (signal == SIGUSR1)
    {
        dumpResults = true;
    }
}

namespace
{

void configureCli(CLI::App& app, Options& options)
{
    app.option_defaults()->always_capture_default();

    app.add_option("-i,--interface", options.interface, "Network interface to use")->required();
    app.add_option("-c,--cycletime", options.cycletime, "Cycletime for the benchmark in microseconds");
    app.add_option("-s,--prevent-sleep-states", options.preventSleepStates,
                   "Prevent or allow cpu sleep states by writing 0 to /dev/cpu_dma_latency");
    app.add_option("-m,--lock-memory", options.lockMemory, "use mlockall to lock memory");
    app.add_option("-p,--priority", options.priority,
                   "SCHED_FIFO priority to use for the benchmark thread, -1 means SCHED_OTHER");
    app.add_option("--cpu-affinity", options.cpuAffinity,
                   "Set CPU affinity of the benchmark thread to this cpu, -1 means no affinity");
    app.add_option("-r,--results-path", options.resultsPath,
                   "Write benchmark results to this directory. It is created if it does not exist and must be empty "
                   "unless --create-results-subdir is used.");
    app.add_option(
        "--create-results-subdir", options.createResultsSubdirectory,
        "Create a new subdirectory in the results path for each run, named with date and time, to avoid "
        "overwriting previous results. Without this, the benchmark aborts if the results path is not empty.");
    app.add_option("-f,--stats-frequency", options.statsFrequency,
                   "Duration to wait between printing stats to the console. Supports the suffixed ms, s, m, h for "
                   "milliseconds, seconds, minutes and hours. "
                   "If this is not passed, stats will be printed very frequently but the cursor will be moved up to "
                   "overwrite the previous results.");
    app.add_option("-q,--quiet", options.quiet, "Dont print statistics during the run");
}

void prepareResultsPath(Options& options)
{
    std::filesystem::create_directories(options.resultsPath);

    if (options.createResultsSubdirectory)
    {
        options.resultsPath = vr::RTUtils::createRtBenchmarkBasePath(options.resultsPath);
        return;
    }

    if (!std::filesystem::is_empty(options.resultsPath))
    {
        throw std::runtime_error("Results directory '" + options.resultsPath.string() +
                                 "' is not empty, aborting to avoid overwriting previous results. "
                                 "Use --create-results-subdir to create a new subdirectory for each run.");
    }
}

void setupRealtimeEnvironment(const Options& options)
{
    if (options.lockMemory)
    {
        vr::RTUtils::lockMemory();
    }
    if (options.preventSleepStates)
    {
        vr::RTUtils::preventSleepStates();
    }
}

void applyBenchmarkThreadSettings(const Options& options)
{
    if (options.priority != -1)
    {
        vr::RTUtils::setSchedulingParamsOfCurrentThread(vr::RTUtils::SchedulingPolicy::FIFO, options.priority);
    }
    if (options.cpuAffinity != -1)
    {
        std::vector<bool> affinityMask(options.cpuAffinity + 1, false);
        affinityMask[options.cpuAffinity] = true;
        vr::RTUtils::setCpuAffinityOfCurrentThread(affinityMask);
    }
    vr::RTUtils::setNameOfCurrentThread("NetworkBench");
}

void installSignalHandlers()
{
    std::signal(SIGINT, &handleSignal);
    std::signal(SIGTERM, &handleSignal);
    std::signal(SIGUSR1, &handleSignal);
}

void uninstallSignalHandlers()
{
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    // dont uninstall SIGUSR1 --> keep program from crashing if user tries to dump results
}

void handleDumpResultsSignal(const std::filesystem::path& resultsPath, const Statistics& stats)
{
    if (dumpResults)
    {
        auto stream = std::ofstream(resultsPath / "current_results.json");
        dumpStatisticsToJson(stats, stream);
        dumpResults = false;
    }
}

void printStatsUntilStopped(const std::optional<std::chrono::milliseconds>& statsFrequency,
                            const Statistics& stats,
                            const Options& options)
{
    auto lastPrint = std::chrono::steady_clock::now();

    while (!stopSignal)
    {
        std::this_thread::sleep_for(50ms);
        handleDumpResultsSignal(options.resultsPath, stats);
        if (!options.quiet)
        {
            if (statsFrequency.has_value())
            {
                if (std::chrono::steady_clock::now() - lastPrint < *statsFrequency)
                {
                    continue;
                }
                lastPrint = std::chrono::steady_clock::now();
                std::cout << "\nCurrent NetworkRttTest statistics:\n";
            }
            else
            {
                // move cursor up reserved live-stats lines and clear to end of display
                std::cout << "\r\033[" << LIVE_STATS_OUTPUT_LINES << "A\033[J";
            }
            printStats(stats, std::cout);
            std::cout << std::flush;
        }
    }
    auto finalResultsFile = std::ofstream(options.resultsPath / "final_results.json");
    dumpStatisticsToJson(stats, finalResultsFile);
    std::cout << "\nFinal NetworkRttTest statistics:\n";
    printStats(stats, std::cout);
}

}  // namespace

int main(int argc, char** argv)  // NOLINT(*-exception-escape)
{
    CLI::App app{"A (realtime) benchmark for raw sockets."};
    Options options;
    configureCli(app, options);

    CLI11_PARSE(app, argc, argv);

    try
    {
        std::cout << "NetworkRttTest\n";
        prepareResultsPath(options);

        printOptions(options, std::cout);
        writeOptionsToJson(options, options.resultsPath / "used_options.json");

        auto statsFrequency = parseStatsFrequency(options.statsFrequency);

        setupRealtimeEnvironment(options);

        RSock driver(options.interface);
        driver.setReceiveTimeout(options.cycletime);

        installSignalHandlers();

        Benchmark benchmark(options, driver);

        Statistics stats;
        std::exception_ptr benchmarkException;

        std::jthread benchmarkThread([&]() {
            try
            {
                applyBenchmarkThreadSettings(options);
                std::cout << "Starting benchmark\n\n";
                benchmark.doNetworkBenchmark(stopSignal, stats);
            } catch (...)
            {
                benchmarkException = std::current_exception();
                stopSignal = true;
            }
        });

        std::this_thread::sleep_for(200ms);
        if (!statsFrequency.has_value() && !options.quiet)
        {
            // reserve output lines for live stats; each update rewinds and redraws them
            for (int line = 0; line < LIVE_STATS_OUTPUT_LINES; ++line)
            {
                std::cout << "\n";
            }
        }

        printStatsUntilStopped(statsFrequency, stats, options);

        uninstallSignalHandlers();
        benchmarkThread.join();
        if (benchmarkException)
        {
            std::rethrow_exception(benchmarkException);
        }

    } catch (const std::exception& exception)
    {
        std::cerr << "Critical error: " << exception.what() << "\n" << std::flush;
        return 1;
    }
    return 0;
}
