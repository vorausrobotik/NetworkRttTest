// NOLINTBEGIN(readability-magic-numbers, readability-function-cognitive-complexity)
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <span>
#include <vector>

#include "Benchmark.h"
#include "TestFrame.h"

using Catch::Matchers::ContainsSubstring;

using namespace vr::NetworkRttTest;
using namespace std::chrono_literals;

class MockDriver
{
   public:
    size_t send(std::span<const std::byte> buffer)
    {
        lastSentData_.assign(buffer.begin(), buffer.end());
        return buffer.size();
    }

    size_t receive(std::span<std::byte> buffer)
    {
        if (receiveCallback_)
        {
            return receiveCallback_(buffer);
        }
        return 0;
    }

    void setReceiveCallback(std::function<size_t(std::span<std::byte>)> callback)
    {
        receiveCallback_ = std::move(callback);
    }

    std::span<const std::byte> getLastSentData() const { return lastSentData_; }

   private:
    std::function<size_t(std::span<std::byte>)> receiveCallback_;
    std::vector<std::byte> lastSentData_;
};

inline ReadSlaveTimeFrame extractAndValidateSentFrame(std::span<const std::byte> data)
{
    REQUIRE(data.size() >= sizeof(ReadSlaveTimeFrame));

    ReadSlaveTimeFrame frame;
    std::memcpy(&frame, data.data(), sizeof(frame));

    // Verify the frame looks like a valid ReadSlaveTimeFrame
    CHECK(ntohs(frame.header.h_proto) == ETHERCAT_ETHERTYPE);  // EtherCAT ethertype
    CHECK(frame.ecatFrameHeader.type == 1);                    // EtherCAT frame type
    CHECK(frame.datagramHeader.command == 1);                  // APRD command
    CHECK(frame.datagramHeader.address2 == 0x910);             // slave time register

    return frame;
}

namespace
{
Options makeTestOptions(const std::string& subdir)
{
    auto currentTime = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
    std::filesystem::path subdirPath = std::filesystem::temp_directory_path() /
                                       ("benchmark_test_" + subdir + "_" + std::to_string(timestamp));

    Options opts;
    opts.interface = "mock0";
    opts.cycletime = 200us;
    opts.lockMemory = false;
    opts.preventSleepStates = false;
    opts.priority = -1;
    opts.resultsPath = subdirPath;
    std::filesystem::create_directories(opts.resultsPath);
    return opts;
}
}  // namespace

TEST_CASE("Benchmark: no response is counted as lost frame")
{
    auto options = makeTestOptions("no_response");
    MockDriver driver;

    // receive() always returns 0 → every frame is lost.
    // After MAX_ERROR_COUNT / ERROR_COUNT_INCREMENT cycles the benchmark throws.

    Benchmark<MockDriver> benchmark(options, driver);

    std::atomic_bool stopSignal{false};
    Statistics stats;

    CHECK_THROWS_AS(benchmark.doNetworkBenchmark(stopSignal, stats), std::runtime_error);
    CHECK(stats.lostFrameCount > 3);
    CHECK(stats.cycleCount > 0);
}

TEST_CASE("Benchmark: frame with wrong index is counted as lost frame")
{
    auto options = makeTestOptions("wrong_index");
    MockDriver driver;

    // On odd calls return a frame with an index that never matches the sender,
    // on even calls return 0 bytes so receiveExpectedFrame_ gives up.
    int callCount = 0;
    driver.setReceiveCallback([&](std::span<std::byte> buffer) -> size_t {
        callCount++;
        if (callCount % 2 == 1)
        {
            auto wrongFrame = extractAndValidateSentFrame(driver.getLastSentData());
            wrongFrame.datagramHeader.index += 1;  // mismatch: different index than sent
            auto copySize = std::min(buffer.size(), sizeof(wrongFrame));
            std::memcpy(buffer.data(), &wrongFrame, copySize);
            return sizeof(wrongFrame);
        }
        return 0;  // timeout
    });

    Benchmark<MockDriver> benchmark(options, driver);

    std::atomic_bool stopSignal{false};
    Statistics stats;

    CHECK_THROWS_AS(benchmark.doNetworkBenchmark(stopSignal, stats), std::runtime_error);
    CHECK(stats.lostFrameCount > 3);
    CHECK(stats.cycleCount > 0);
}

TEST_CASE("Benchmark: correct index is accepted, no lost frames")
{
    auto options = makeTestOptions("correct_index");
    MockDriver driver;

    std::atomic_bool stopSignal{false};
    int receiveCount = 0;

    // Echo back the most recently sent frame (same index) → always a match.
    driver.setReceiveCallback([&](std::span<std::byte> buffer) -> size_t {
        auto sentFrame = extractAndValidateSentFrame(driver.getLastSentData());
        auto copySize = std::min(buffer.size(), sizeof(sentFrame));
        std::memcpy(buffer.data(), &sentFrame, copySize);

        receiveCount++;
        if (receiveCount >= 10)
        {
            stopSignal = true;
        }
        return sizeof(sentFrame);
    });

    Benchmark<MockDriver> benchmark(options, driver);
    Statistics stats;

    CHECK_NOTHROW(benchmark.doNetworkBenchmark(stopSignal, stats));
    CHECK(stats.lostFrameCount == 0);
    CHECK(stats.cycleCount >= 10);
}

TEST_CASE("Benchmark: wrong index followed by correct index in same receive cycle is accepted")
{
    auto options = makeTestOptions("wrong_then_correct");
    MockDriver driver;

    std::atomic_bool stopSignal{false};
    int receiveCount = 0;

    // Odd calls → wrong index, even calls → correct index.
    driver.setReceiveCallback([&](std::span<std::byte> buffer) -> size_t {
        receiveCount++;
        if (receiveCount % 2 == 1)
        {
            auto wrongFrame = extractAndValidateSentFrame(driver.getLastSentData());
            wrongFrame.datagramHeader.index += 1;  // mismatch: different index than sent
            auto copySize = std::min(buffer.size(), sizeof(wrongFrame));
            std::memcpy(buffer.data(), &wrongFrame, copySize);
            return sizeof(wrongFrame);
        }

        auto sentFrame = extractAndValidateSentFrame(driver.getLastSentData());
        auto copySize = std::min(buffer.size(), sizeof(sentFrame));
        std::memcpy(buffer.data(), &sentFrame, copySize);

        if (receiveCount >= 20)  // 10 cycles × 2 receive calls each
        {
            stopSignal = true;
        }
        return sizeof(sentFrame);
    });

    Benchmark<MockDriver> benchmark(options, driver);
    Statistics stats;

    CHECK_NOTHROW(benchmark.doNetworkBenchmark(stopSignal, stats));
    CHECK(stats.lostFrameCount == 0);
    CHECK(stats.cycleCount >= 10);
}

TEST_CASE("printStats during warmup")
{
    Statistics stats;
    stats.cycleCount = 10;  // below WARMUP_CYCLES (1000)
    stats.lostFrameCount = 2;
    stats.skippedCycles = 7;

    std::ostringstream out;
    printStats(stats, out);
    auto result = out.str();

    CHECK_THAT(result, ContainsSubstring("Cycles:"));
    CHECK_THAT(result, ContainsSubstring("10"));
    CHECK_THAT(result, ContainsSubstring("Lost frames:"));
    CHECK_THAT(result, ContainsSubstring("2"));
    CHECK_THAT(result, ContainsSubstring("Skipped cyc:"));
    CHECK_THAT(result, ContainsSubstring("7"));
    CHECK_THAT(result, ContainsSubstring("Act RTT:"));
    CHECK_THAT(result, ContainsSubstring("-"));
    CHECK_THAT(result, ContainsSubstring("Avg RTT:"));
    CHECK_THAT(result, ContainsSubstring("Max Rtt:"));
    CHECK_THAT(result, ContainsSubstring("Act delta:"));
    CHECK_THAT(result, ContainsSubstring("Avg delta:"));
    CHECK_THAT(result, ContainsSubstring("Min delta:"));
    CHECK_THAT(result, ContainsSubstring("Max delta:"));
    // During warmup, no "us" unit should appear for RTT/delta values
    // (only dashes are shown)
}

TEST_CASE("printStats after warmup")
{
    Statistics stats;
    stats.cycleCount = 2000;  // above WARMUP_CYCLES (1000)
    stats.lostFrameCount = 5;
    stats.skippedCycles = 3;
    stats.currentRtt = 120;
    stats.avgRtt = 115.5F;
    stats.maxRtt = 200;
    stats.currentDeltaAtEcatDevice = -3;
    stats.avgDeltaAtEcatDevice = -1.5F;
    stats.minDeltaAtEcatDevice = -10;
    stats.maxDeltaAtEcatDevice = 8;

    std::ostringstream out;
    printStats(stats, out);
    auto result = out.str();

    CHECK_THAT(result, ContainsSubstring("Cycles:"));
    CHECK_THAT(result, ContainsSubstring("2000"));
    CHECK_THAT(result, ContainsSubstring("Lost frames:"));
    CHECK_THAT(result, ContainsSubstring("5"));
    CHECK_THAT(result, ContainsSubstring("Skipped cyc:"));
    CHECK_THAT(result, ContainsSubstring("3"));
    CHECK_THAT(result, ContainsSubstring("120 us"));
    CHECK_THAT(result, ContainsSubstring("115.50 us"));
    CHECK_THAT(result, ContainsSubstring("200 us"));
    CHECK_THAT(result, ContainsSubstring("-3 us"));
    CHECK_THAT(result, ContainsSubstring("-1.50 us"));
    CHECK_THAT(result, ContainsSubstring("-10 us"));
    CHECK_THAT(result, ContainsSubstring("8 us"));
}

TEST_CASE("printStats at warmup boundary")
{
    Statistics stats;
    stats.cycleCount = WARMUP_CYCLES;  // exactly at boundary — no longer warmup
    stats.lostFrameCount = 0;
    stats.skippedCycles = 1;
    stats.currentRtt = 50;
    stats.avgRtt = 50.0F;
    stats.maxRtt = 50;
    stats.currentDeltaAtEcatDevice = 0;
    stats.avgDeltaAtEcatDevice = 0.0F;
    stats.minDeltaAtEcatDevice = 0;
    stats.maxDeltaAtEcatDevice = 0;

    std::ostringstream out;
    printStats(stats, out);
    auto result = out.str();

    // At exactly WARMUP_CYCLES, warmup is over — should show actual values
    CHECK_THAT(result, ContainsSubstring("50 us"));
    CHECK_THAT(result, ContainsSubstring("Skipped cyc:"));
    CHECK_THAT(result, ContainsSubstring("1"));
}

TEST_CASE("dumpStatisticsToJson during warmup emits null metrics")
{
    Statistics stats;
    stats.cycleCount = 10;  // below WARMUP_CYCLES (1000)
    stats.lostFrameCount = 2;
    stats.skippedCycles = 7;
    stats.currentRtt = 120;
    stats.avgRtt = 115.5F;
    stats.maxRtt = 200;
    stats.currentDeltaAtEcatDevice = -3;
    stats.avgDeltaAtEcatDevice = -1.5F;
    stats.minDeltaAtEcatDevice = -10;
    stats.maxDeltaAtEcatDevice = 8;

    std::ostringstream out;
    dumpStatisticsToJson(stats, out);
    const auto result = out.str();

    CHECK_THAT(result, ContainsSubstring("\"cycle_count\": 10"));
    CHECK_THAT(result, ContainsSubstring("\"lost_frame_count\": 2"));
    CHECK_THAT(result, ContainsSubstring("\"skipped_cycles\": 7"));
    CHECK_THAT(result, ContainsSubstring("\"warmup\": true"));
    CHECK_THAT(result, ContainsSubstring("\"current_rtt_us\": null"));
    CHECK_THAT(result, ContainsSubstring("\"avg_rtt_us\": null"));
    CHECK_THAT(result, ContainsSubstring("\"max_rtt_us\": null"));
    CHECK_THAT(result, ContainsSubstring("\"current_delta_at_ecat_device_us\": null"));
    CHECK_THAT(result, ContainsSubstring("\"avg_delta_at_ecat_device_us\": null"));
    CHECK_THAT(result, ContainsSubstring("\"min_delta_at_ecat_device_us\": null"));
    CHECK_THAT(result, ContainsSubstring("\"max_delta_at_ecat_device_us\": null"));
}

TEST_CASE("dumpStatisticsToJson after warmup emits numeric metrics")
{
    Statistics stats;
    stats.cycleCount = WARMUP_CYCLES;
    stats.lostFrameCount = 5;
    stats.skippedCycles = 3;
    stats.currentRtt = 120;
    stats.avgRtt = 115.5F;
    stats.maxRtt = 200;
    stats.currentDeltaAtEcatDevice = -3;
    stats.avgDeltaAtEcatDevice = -1.5F;
    stats.minDeltaAtEcatDevice = -10;
    stats.maxDeltaAtEcatDevice = 8;

    std::ostringstream out;
    dumpStatisticsToJson(stats, out);
    const auto result = out.str();

    CHECK_THAT(result, ContainsSubstring("\"cycle_count\": 1000"));
    CHECK_THAT(result, ContainsSubstring("\"lost_frame_count\": 5"));
    CHECK_THAT(result, ContainsSubstring("\"skipped_cycles\": 3"));
    CHECK_THAT(result, ContainsSubstring("\"warmup\": false"));
    CHECK_THAT(result, ContainsSubstring("\"current_rtt_us\": 120"));
    CHECK_THAT(result, ContainsSubstring("\"avg_rtt_us\": 115.5"));
    CHECK_THAT(result, ContainsSubstring("\"max_rtt_us\": 200"));
    CHECK_THAT(result, ContainsSubstring("\"current_delta_at_ecat_device_us\": -3"));
    CHECK_THAT(result, ContainsSubstring("\"avg_delta_at_ecat_device_us\": -1.5"));
    CHECK_THAT(result, ContainsSubstring("\"min_delta_at_ecat_device_us\": -10"));
    CHECK_THAT(result, ContainsSubstring("\"max_delta_at_ecat_device_us\": 8"));
}

// NOLINTEND(readability-magic-numbers, readability-function-cognitive-complexity)
