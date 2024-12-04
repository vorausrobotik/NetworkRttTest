#include "Benchmark.h"

#include <format>

namespace vr::NetworkRttTest
{

constexpr int STATS_COLUMN_WIDTH = 12;

void printStats(const Statistics& stats, std::ostream& out)
{
    bool warmup = stats.cycleCount.load() < WARMUP_CYCLES;
    out << std::format("Cycles:      {:>{}}\n", stats.cycleCount.load(), STATS_COLUMN_WIDTH);
    out << std::format("Lost frames: {:>{}}\n", stats.lostFrameCount.load(), STATS_COLUMN_WIDTH);
    out << std::format("Skipped cyc: {:>{}}\n", stats.skippedCycles.load(), STATS_COLUMN_WIDTH);
    if (warmup)
    {
        out << std::format("Act RTT:     {:>{}}\n", "-", STATS_COLUMN_WIDTH);
        out << std::format("Avg RTT:     {:>{}}\n", "-", STATS_COLUMN_WIDTH);
        out << std::format("Max Rtt:     {:>{}}\n", "-", STATS_COLUMN_WIDTH);
        out << std::format("Act delta:   {:>{}}\n", "-", STATS_COLUMN_WIDTH);
        out << std::format("Avg delta:   {:>{}}\n", "-", STATS_COLUMN_WIDTH);
        out << std::format("Min delta:   {:>{}}\n", "-", STATS_COLUMN_WIDTH);
        out << std::format("Max delta:   {:>{}}\n", "-", STATS_COLUMN_WIDTH);
    }
    else
    {
        out << std::format("Act RTT:     {:>{}} us\n", stats.currentRtt.load(), STATS_COLUMN_WIDTH);
        out << std::format("Avg RTT:     {:>{}.2f} us\n", stats.avgRtt.load(), STATS_COLUMN_WIDTH);
        out << std::format("Max Rtt:     {:>{}} us\n", stats.maxRtt.load(), STATS_COLUMN_WIDTH);
        out << std::format("Act delta:   {:>{}} us\n", stats.currentDeltaAtEcatDevice.load(), STATS_COLUMN_WIDTH);
        out << std::format("Avg delta:   {:>{}.2f} us\n", stats.avgDeltaAtEcatDevice.load(), STATS_COLUMN_WIDTH);
        out << std::format("Min delta:   {:>{}} us\n", stats.minDeltaAtEcatDevice.load(), STATS_COLUMN_WIDTH);
        out << std::format("Max delta:   {:>{}} us\n", stats.maxDeltaAtEcatDevice.load(), STATS_COLUMN_WIDTH);
    }
}

void dumpStatisticsToJson(const Statistics& stats, std::ostream& out)
{
    const bool warmup = stats.cycleCount.load() < WARMUP_CYCLES;
    const auto valueOrNull = [warmup](auto value) { return warmup ? std::string("null") : std::format("{}", value); };

    out << "{\n";
    out << std::format("  \"cycle_count\": {},\n", stats.cycleCount.load());
    out << std::format("  \"lost_frame_count\": {},\n", stats.lostFrameCount.load());
    out << std::format("  \"skipped_cycles\": {},\n", stats.skippedCycles.load());
    out << std::format("  \"warmup\": {},\n", warmup ? "true" : "false");
    out << std::format("  \"current_rtt_us\": {},\n", valueOrNull(stats.currentRtt.load()));
    out << std::format("  \"avg_rtt_us\": {},\n", valueOrNull(stats.avgRtt.load()));
    out << std::format("  \"max_rtt_us\": {},\n", valueOrNull(stats.maxRtt.load()));
    out << std::format("  \"current_delta_at_ecat_device_us\": {},\n",
                       valueOrNull(stats.currentDeltaAtEcatDevice.load()));
    out << std::format("  \"avg_delta_at_ecat_device_us\": {},\n", valueOrNull(stats.avgDeltaAtEcatDevice.load()));
    out << std::format("  \"min_delta_at_ecat_device_us\": {},\n", valueOrNull(stats.minDeltaAtEcatDevice.load()));
    out << std::format("  \"max_delta_at_ecat_device_us\": {}\n", valueOrNull(stats.maxDeltaAtEcatDevice.load()));
    out << "}\n";
}

}  // namespace vr::NetworkRttTest
