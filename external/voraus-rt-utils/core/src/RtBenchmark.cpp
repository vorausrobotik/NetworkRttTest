#include <RTUtils/RtBenchmark.h>
#include <fstream>

namespace vr::RTUtils
{
namespace fs = std::filesystem;

RtBenchmark::RtBenchmark(std::shared_ptr<vr::RTUtils::TimeHandler> timeHandler,
                         std::optional<std::filesystem::path> dumpOnDestructionPath,
                         std::chrono::nanoseconds startValue,
                         std::chrono::nanoseconds binWidth,
                         unsigned int numberOfBins)
    : RtBenchmark(std::move(timeHandler),
                  Settings{.dumpOnDestructionPath = std::move(dumpOnDestructionPath),
                           .startValue = startValue,
                           .binWidth = binWidth,
                           .numberOfBins = numberOfBins})
{
}

const FixedHistogram<std::chrono::nanoseconds>& RtBenchmark::getHistogram() const
{
    return hist_;
}

std::optional<std::string_view> RtBenchmark::getName() const
{
    return settings_.name;
}

void RtBenchmark::start()
{
    startTimestamp_ = timeHandler_->getTime();
}

void RtBenchmark::stop()
{
    if (!startTimestamp_.has_value())
    {
        return;
    }
    auto elapsed = timeHandler_->getElapsedTime(startTimestamp_.value());
    recordValue(elapsed);
    startTimestamp_ = std::nullopt;
}

bool RtBenchmark::isRunning() const
{
    return startTimestamp_.has_value();
}

void RtBenchmark::recordAndRestart()
{
    auto now = timeHandler_->getTime();
    if (startTimestamp_.has_value())
    {
        recordValue(now - startTimestamp_.value());
    }
    startTimestamp_ = now;
}

void RtBenchmark::writeToDiskInVorausGpFormat(const std::filesystem::path& path) const
{
    std::string histAsString = inVorausGpFormat(hist_);
    std::ofstream file(path);
    file << histAsString;
    file.close();
}

void RtBenchmark::writeToDiskInJsonFormat(const std::filesystem::path& path) const
{
    std::string histAsString = inJsonFormat(hist_);
    std::ofstream file(path);
    file << histAsString;
    file.close();
}

void RtBenchmark::recordValue(std::chrono::nanoseconds value)
{
    checkForOutlier_(value);
    hist_.recordValue(value);
}

RtBenchmark::RtBenchmark(RtBenchmark&& other) noexcept
    : hist_(std::move(other.hist_)),
      timeHandler_(std::move(other.timeHandler_)),
      startTimestamp_(other.startTimestamp_),
      settings_(std::move(other.settings_))
{
    other.settings_.dumpOnDestructionPath = std::nullopt;
}

RtBenchmark& RtBenchmark::operator=(RtBenchmark&& other) noexcept
{
    hist_ = std::move(other.hist_);
    timeHandler_ = std::move(other.timeHandler_);
    startTimestamp_ = other.startTimestamp_;
    settings_ = std::move(other.settings_);

    other.settings_.dumpOnDestructionPath = std::nullopt;
    return *this;
}

RtBenchmark::~RtBenchmark()
{
    if (settings_.dumpOnDestructionPath.has_value())
    {
        try
        {
            if (settings_.dumpOnDestructionPath->extension() == ".gp")
            {
                writeToDiskInVorausGpFormat(settings_.dumpOnDestructionPath.value());
            }
            else
            {
                writeToDiskInJsonFormat(settings_.dumpOnDestructionPath.value());
            }
        } catch (const std::exception&)  // NOLINT(bugprone-empty-catch)
        {
            // TODO: log exception with optional logging.
        }
    }
}
RtBenchmark::RtBenchmark(std::shared_ptr<vr::RTUtils::TimeHandler> timeHandler, RtBenchmark::Settings settings)
    : hist_(settings.startValue, settings.binWidth, settings.numberOfBins),
      timeHandler_(std::move(timeHandler)),
      settings_(std::move(settings))

{
}

void RtBenchmark::checkForOutlier_(const std::chrono::nanoseconds& value)
{
    bool outlierDetected = false;
    if (std::holds_alternative<None>(settings_.outlierDetectionStrategy))
    {
        return;
    }
    if (std::holds_alternative<FixedLimits>(settings_.outlierDetectionStrategy))
    {
        auto fixedValues = std::get<FixedLimits>(settings_.outlierDetectionStrategy);
        if ((fixedValues.thresholdPositive.has_value() && value > fixedValues.thresholdPositive.value()) ||
            (fixedValues.thresholdNegative.has_value() && value < fixedValues.thresholdNegative.value()))
        {
            outlierDetected = true;
        }
    }

    // we could write the outliers to an outlier table here.
    if (outlierDetected && settings_.outlierNotifyFn)
    {
        settings_.outlierNotifyFn(value, hist_.count(), *this);
    }
}

std::filesystem::path createRtBenchmarkBasePath(const std::filesystem::path& path, std::optional<tm> timestamp)
{
    if (!timestamp.has_value())
    {
        tm tmNow{};
        // get the current date and time
        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        localtime_r(&now, &tmNow);
        timestamp = tmNow;
    }
    char buffer[50];  // NOLINT(readability-magic-numbers)
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d--%H-%M-%S", &timestamp.value());
    std::string dateString(buffer);

    auto result = path / dateString;
    fs::create_directories(result);
    return result;
}
}  // namespace vr::RTUtils