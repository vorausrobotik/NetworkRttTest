#pragma once
#include <RTUtils/Histogram.h>
#include <RTUtils/TimeHandler.h>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <variant>

namespace vr::RTUtils
{

/**
 * Class to time realtime or other time sensitive operations in a histogram.
 */
class RtBenchmark
{
   public:
    static constexpr std::chrono::nanoseconds DEFAULT_BIN_WIDTH = std::chrono::nanoseconds(500);
    static constexpr unsigned int DEFAULT_NUMBER_OF_BINS = 5000;

    // outlier detection strategies
    /**
     * Detect outliers based on fixed values.
     */
    struct FixedLimits
    {
        std::optional<std::chrono::nanoseconds> thresholdNegative = std::nullopt;
        std::optional<std::chrono::nanoseconds> thresholdPositive = std::nullopt;
    };
    /**
     * Disables outlier detection.
     */
    struct None
    {
    };

    using OutlierDetectionStrategy = std::variant<FixedLimits, None>;

    struct Settings
    {
        /** The underlying histogram is written to this path on destruction if set. */
        std::optional<std::filesystem::path> dumpOnDestructionPath = std::nullopt;

        /** The minimum value that should be contained in the histogram (@see FixedHistogram). */
        std::chrono::nanoseconds startValue = std::chrono::nanoseconds(0);

        /** The binWidth of the histogram (@see FixedHistogram). */
        std::chrono::nanoseconds binWidth = DEFAULT_BIN_WIDTH;

        /** The number of bins for the histogram (@see FixedHistogram). */
        unsigned int numberOfBins = DEFAULT_NUMBER_OF_BINS;

        /** Strategy of detecting outliers */
        OutlierDetectionStrategy outlierDetectionStrategy = None{};

        /** If an outlier is detected (@see outlierDetectionStrategy), this callback is called.*/
        std::function<void(std::chrono::nanoseconds value, unsigned long cycleNumber, const RtBenchmark& benchmark)>
            outlierNotifyFn = nullptr;

        /** An optional name for the benchmark for printing and identification. */
        std::optional<std::string> name = std::nullopt;
    };

    /**
     * Creates the RtBenchmark.
     * @warning this constructor is deprecated, use RtBenchmark(std::shared_ptr<vr::RTUtils::TimeHandler> timeHandler,
     * Settings settings) instead.
     * @param timeHandler The TimeHandler to use.
     * @param dumpOnDestructionPath The underlying histogram is written to this path on destruction if set.
     * @param startValue The minimum value that should be contained in the histogram (@see FixedHistogram).
     * @param binWidth The binWidth of the histogram (@see FixedHistogram).
     * @param numberOfBins The number of bins for the histogram (@see FixedHistogram).
     */
    [[deprecated("use Settings based constructor instead!")]] RtBenchmark(
        std::shared_ptr<vr::RTUtils::TimeHandler> timeHandler,
        std::optional<std::filesystem::path> dumpOnDestructionPath = std::nullopt,
        std::chrono::nanoseconds startValue = std::chrono::nanoseconds(0),
        std::chrono::nanoseconds binWidth = DEFAULT_BIN_WIDTH,
        unsigned int numberOfBins = 500);  // NOLINT (500 is an old default value for this constructor).

    RtBenchmark(std::shared_ptr<vr::RTUtils::TimeHandler> timeHandler, Settings settings);

    ~RtBenchmark();

    RtBenchmark(const RtBenchmark& other) = default;
    RtBenchmark& operator=(const RtBenchmark& other) = default;

    RtBenchmark(RtBenchmark&& other) noexcept;
    RtBenchmark& operator=(RtBenchmark&& other) noexcept;

    /**
     * Returns the underlying histogram.
     * @return the histogram.
     */
    const FixedHistogram<std::chrono::nanoseconds>& getHistogram() const;

    /**
     * Starts a duration measurement.
     */
    void start();

    /**
     * Stops a duration measurement and records the result in the histogram.
     * This function does nothing if the measurement is not running.
     */
    void stop();

    /**
     * Checks if the measurement is currently running.
     * @return True if start() or recordAndRestart() was called.
     */
    bool isRunning() const;

    /**
     * Stops the measurement and records the result if the measurement was running before and starts it again.
     */
    void recordAndRestart();

    /**
     * Records a value in the underlying histogram.
     * This is useful for e.g. jitter measurements.
     * @param value The value to record.
     */
    void recordValue(std::chrono::nanoseconds value);

    /**
     * Writes the underlying histogram in the voraus gnu gp format on the disk.
     * The path is created if it does not exists.
     * @param path
     */
    void writeToDiskInVorausGpFormat(const std::filesystem::path& path) const;

    /**
     * Write the underlying histogram in JSON format to the disk.
     * The path is created if it does not exists.
     * @param path
     */
    void writeToDiskInJsonFormat(const std::filesystem::path& path) const;

    /**
     * Returns the name of the benchmark.
     *
     * Note that the lifetime of the string_view is limited to the lifetime of the benchmark settings.
     * @return A std::string_view of the benchmark name or std::nullopt if no name is defined.
     */
    std::optional<std::string_view> getName() const;

   private:
    void checkForOutlier_(const std::chrono::nanoseconds& value);

    FixedHistogram<std::chrono::nanoseconds> hist_;
    std::shared_ptr<TimeHandler> timeHandler_;
    std::optional<std::chrono::nanoseconds> startTimestamp_;

    Settings settings_;
    // Developer notice: new members must also be considered in move constructor and move assignment.
};

/**
 * Creates a folder with the current name and and date in the passed path.
 * If the passed path does not exist, it is created.
 * This can be used as a "base path" for RtBenchmarks.
 * @param path the path to use.
 * @param timestamp the timestamp to use. If nullopt is passed, the current time is used.
 * @return The created folder.
 */
std::filesystem::path createRtBenchmarkBasePath(const std::filesystem::path& path,
                                                std::optional<tm> timestamp = std::nullopt);

/**
 * Can be used together with ScopedMeasurement to not immediately start the benchmark.
 * This is usefully if benchmarking should only be done on certain conditions.
 */
struct DeferStartT
{
    DeferStartT() = default;
};
constexpr DeferStartT DEFER_START{};

/**
 * Helper class which can be used in a scope instead of calling benchmark.start() and benchmark.stop() manually.
 * Calls stop() on the benchmark if this class has started the benchmark (either in constructor or by calling start
 * manually when constructed with DeferStart).
 * @tparam BenchmarkT The type of the benchmark to start and stop.
 */
template <typename BenchmarkT>
class ScopedMeasurement
{
   public:
    /**
     * Constructs a ScopedMeasurement and directly starts the benchmark.
     * The benchmark will be stopped on destruction.
     * @param benchmark the benchmark to start.
     */
    explicit ScopedMeasurement(BenchmarkT& benchmark) : benchmark_(benchmark)
    {
        benchmark.start();
        started_ = true;
    }
    /**
     * Constructs a ScopedMeasurement but does not directly start the benchmark.
     * The benchmark can be started later with ScopedMeasurement::start().
     * @warning if the benchmark should be stopped on destruction, it must be started with ScopedMeasurement::start()
     * and not with BenchmarkT::start()!
     * @param benchmark The benchmark.
     */
    ScopedMeasurement(BenchmarkT& benchmark, const DeferStartT& /*unused*/) : benchmark_(benchmark) {}

    ScopedMeasurement(const ScopedMeasurement& other) = delete;
    ScopedMeasurement& operator==(const ScopedMeasurement& other) = delete;

    /**
     * calls start on the benchmark.
     */
    void start()
    {
        benchmark_.start();
        started_ = true;
    }

    /**
     * calls stop on the benchmark.
     */
    void stop()
    {
        benchmark_.stop();
        started_ = false;
    }

    ~ScopedMeasurement()
    {
        if (started_)
        {
            benchmark_.stop();
            started_ = false;
        }
    }

   private:
    BenchmarkT& benchmark_;
    bool started_ = false;
};

}  // namespace vr::RTUtils

namespace [[deprecated]] VR
{
using namespace vr;
}