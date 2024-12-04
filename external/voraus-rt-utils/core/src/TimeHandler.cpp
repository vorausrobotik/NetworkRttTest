#include "RTUtils/TimeHandler.h"
#include <ctime>
#include <system_error>

namespace vr::RTUtils
{
timepoint_t BasicTimeHandler::getTime() const
{
    timespec currentTime{};
    int result = clock_gettime(CLOCK_MONOTONIC, &currentTime);
    if (result != 0)
    {
        throw std::system_error(errno, std::generic_category(), "clock_gettime failed!");
    }
    return std::chrono::seconds{currentTime.tv_sec} + std::chrono::nanoseconds{currentTime.tv_nsec};
}

duration_t BasicTimeHandler::getElapsedTime(const timepoint_t& old, const timepoint_t& current) const
{
    return current - old;
}
duration_t BasicTimeHandler::getElapsedTime(const timepoint_t& old) const
{
    const auto current = getTime();
    return getElapsedTime(old, current);
}
void BasicTimeHandler::sleep(const duration_t& timeToSleep) const
{
    duration_t current = BasicTimeHandler::getTime();  // we don't want to use the virtual getTime here.
    duration_t target = current + timeToSleep;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(target);
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(target - seconds);
    timespec targetTimespec{seconds.count(), nanoseconds.count()};

    int sleepResult = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &targetTimespec, nullptr);
    while (sleepResult == EINTR)  // sleep can be interrupted by signal --> start sleeping again after interruption.
    {
        sleepResult = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &targetTimespec, nullptr);
    }
    if (sleepResult != 0)
    {  // last return value should be 0 on no error.
        throw std::system_error(std::error_code(sleepResult, std::generic_category()), "clock_nanosleep failed!");
    }
}
}  // namespace vr::RTUtils