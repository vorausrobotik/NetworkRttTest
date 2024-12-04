#include <RTUtils/CycleControl.h>
#include <RTUtils/RtBenchmark.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <ostream>
#include <span>
#include <stdexcept>
#include "NetworkDriver.h"
#include "Options.h"
#include "TestFrame.h"

constexpr size_t MTU = 1520;
constexpr unsigned int WARMUP_CYCLES = 1000;
constexpr unsigned int ERROR_COUNT_INCREMENT = 5;
constexpr unsigned int MAX_ERROR_COUNT = ERROR_COUNT_INCREMENT * 100;

namespace vr::NetworkRttTest
{

namespace detail
{
    inline vr::RTUtils::RtBenchmark::Settings makeBenchmarkSettings(const std::filesystem::path& benchmarkBasePath,
                                                                    const std::string& name,
                                                                    std::chrono::nanoseconds startValue)
    {
        return {.dumpOnDestructionPath = benchmarkBasePath / (name + ".json"),
                .startValue = startValue,
                .numberOfBins = 20000};  // bins for 10 ms // NOLINT(*-magic-numbers)
    }

    inline std::chrono::nanoseconds deltaAtEcatDevice(std::chrono::nanoseconds receiveTimestamp,
                                                      std::chrono::nanoseconds cycletime)
    {
        auto delta = receiveTimestamp % cycletime;
        if (delta > (cycletime / 2))
        {
            delta -= cycletime;  // transform from [0, cycletime] to [-cycletime/2, cycletime/2]
        }
        return delta;
    }

    template <typename Rep, typename Period>
    inline auto toMicroseconds(std::chrono::duration<Rep, Period> duration)
    {
        return std::chrono::duration_cast<std::chrono::duration<Rep, std::micro>>(duration).count();
    }

}  // namespace detail

struct Statistics
{
    std::atomic<uint64_t> cycleCount{0};
    std::atomic<uint64_t> lostFrameCount{0};
    std::atomic<uint64_t> skippedCycles{0};
    std::atomic<uint64_t> currentRtt{0};
    std::atomic<float> avgRtt{0};
    std::atomic<uint64_t> maxRtt{0};
    std::atomic<int64_t> minDeltaAtEcatDevice{0};
    std::atomic<int64_t> maxDeltaAtEcatDevice{0};
    std::atomic<int64_t> currentDeltaAtEcatDevice{0};
    std::atomic<float> avgDeltaAtEcatDevice{0};
};

void printStats(const Statistics& stats, std::ostream& out);

void dumpStatisticsToJson(const Statistics& stats, std::ostream& out);

template <NetworkDriver Driver>
struct Benchmark
{
    explicit Benchmark(const Options& options, Driver& driver)
        : driver_(driver),
          timeHandler_(std::make_shared<RTUtils::BasicTimeHandler>()),
          sendMeasurement_(timeHandler_,
                           detail::makeBenchmarkSettings(options.resultsPath, "send", std::chrono::nanoseconds{0})),
          roundTripMeasurement_(
              timeHandler_,
              detail::makeBenchmarkSettings(options.resultsPath, "round_trip", std::chrono::nanoseconds{0})),
          jitterMeasurement_(timeHandler_,
                             detail::makeBenchmarkSettings(options.resultsPath,
                                                           "jitter_at_ecat_device",
                                                           std::chrono::milliseconds{-5})),  // NOLINT(*-magic-numbers)
          options_(options)
    {
        static_assert(sizeof(ReadSlaveTimeFrame) <= MTU, "frame must fit into buffer");
    }

    void doNetworkBenchmark(std::atomic_bool& stopSignal, Statistics& stats)
    {
        using namespace std::chrono_literals;

        lostFrameCount_ = 0;
        errorCounter_ = 0;
        cycleNumber_ = 0;
        integralError_ = std::chrono::nanoseconds{0};

        RTUtils::CycleControl cycleControl(options_.cycletime, timeHandler_, options_.cycletime / 4);

        // we want to measure two things: the round trip time and the jitter at the ethercat device.
        // EtherCAT device can be used to accurately store the timestamp at which the packet reached the device.
        // So we define a timepoint where the frame should reach the device and shorten or lengthen our sleep time
        // to try to reach this timepoint as close as possible.
        // The target timepoint is: time % cycletime == 0

        // send initial frame
        cycleControl.startNextCycleNow();
        sendNext_(false);
        std::ignore = cycleControl.sleepAndStartNextCycle();

        while (!stopSignal)
        {
            runSingleCycle_(cycleControl, stats);

            auto remaining = cycleControl.sleepAndStartNextCycle();
            if (remaining < 0ns)
            {
                for (int i = 0; i < (-remaining / options_.cycletime) + 1; ++i)
                {
                    cycleControl.skipCycle();
                    stats.skippedCycles++;
                }
            }
        }
    }

   private:
    void runSingleCycle_(RTUtils::CycleControl& cycleControl, Statistics& stats)
    {
        cycleNumber_++;
        bool warmup = cycleNumber_ < WARMUP_CYCLES;

        auto startTime = timeHandler_->getTime();
        sendNext_(!warmup);

        if (receiveExpectedFrame_())
        {
            auto rtt = timeHandler_->getTime() - startTime;
            auto delta = detail::deltaAtEcatDevice(std::chrono::nanoseconds(receiveFrame_.timestamp),
                                                   options_.cycletime);
            integralError_ += delta;
            auto correction = -((delta / 8) + (integralError_ / 256));  // PI controller // NOLINT(*-magic-numbers)
            cycleControl.setPhaseCorrection(correction);

            if (!warmup)
            {
                roundTripMeasurement_.recordValue(rtt);
                jitterMeasurement_.recordValue(delta);
            }

            stats.currentRtt = detail::toMicroseconds(rtt);
            stats.currentDeltaAtEcatDevice = detail::toMicroseconds(delta);
            errorCounter_ = std::max<int64_t>(0, errorCounter_ - 1);
        }
        else
        {
            lostFrameCount_++;
            errorCounter_ += ERROR_COUNT_INCREMENT;
            if (errorCounter_ >= MAX_ERROR_COUNT)
            {
                throw std::runtime_error("Lost too many packets, aborting at cycle " + std::to_string(cycleNumber_));
            }
        }

        stats.cycleCount = cycleNumber_;
        stats.lostFrameCount = lostFrameCount_;
        updateStats_(stats);
    }

    void sendNext_(bool doMeasurement)
    {
        sendFrame_.datagramHeader.index += 1;
        std::memcpy(sendBuffer_.data(), &sendFrame_, sizeof(sendFrame_));

        RTUtils::ScopedMeasurement measurement(sendMeasurement_, RTUtils::DEFER_START);
        if (doMeasurement)
        {
            measurement.start();
        }
        driver_.send(std::span<const std::byte>(sendBuffer_.data(), sizeof(sendFrame_)));
    }

    bool receiveExpectedFrame_()
    {
        while (true)
        {
            size_t receivedBytes = driver_.receive(std::span<std::byte>(receiveBuffer_.data(), receiveBuffer_.size()));
            if (receivedBytes == 0)
            {
                return false;
            }
            if (receivedBytes < sizeof(receiveFrame_))
            {
                continue;
            }
            std::memcpy(&receiveFrame_, receiveBuffer_.data(), sizeof(receiveFrame_));
            if (receiveFrame_.datagramHeader.index == sendFrame_.datagramHeader.index)
            {
                return true;
            }
        }
    }

    void updateStats_(Statistics& stats)
    {
        using namespace std::chrono_literals;
        auto rttHist = roundTripMeasurement_.getHistogram();
        auto jitterHist = jitterMeasurement_.getHistogram();

        stats.maxRtt = detail::toMicroseconds(rttHist.max().value_or(0ns));
        stats.avgRtt = static_cast<float>(detail::toMicroseconds(rttHist.avg().value_or(0ns)));
        stats.minDeltaAtEcatDevice = detail::toMicroseconds(jitterHist.min().value_or(0ns));
        stats.maxDeltaAtEcatDevice = detail::toMicroseconds(jitterHist.max().value_or(0ns));
        stats.avgDeltaAtEcatDevice = static_cast<float>(detail::toMicroseconds(jitterHist.avg().value_or(0ns)));
    }

    Driver& driver_;
    std::shared_ptr<RTUtils::BasicTimeHandler> timeHandler_;
    RTUtils::RtBenchmark sendMeasurement_;
    RTUtils::RtBenchmark roundTripMeasurement_;
    RTUtils::RtBenchmark jitterMeasurement_;
    std::array<std::byte, MTU> sendBuffer_{};
    std::array<std::byte, MTU> receiveBuffer_{};
    ReadSlaveTimeFrame sendFrame_;
    ReadSlaveTimeFrame receiveFrame_;
    Options options_;
    uint64_t lostFrameCount_ = 0;
    int64_t errorCounter_ = 0;
    uint64_t cycleNumber_ = 0;
    std::chrono::nanoseconds integralError_{0};
};

}  // namespace vr::NetworkRttTest