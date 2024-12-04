#pragma once
#include <chrono>
#include <optional>
#include <ratio>
#include <vector>

namespace vr::RTUtils
{
using timepoint_t = std::chrono::nanoseconds;
using duration_t = std::chrono::nanoseconds;
// these types are capable of covering at least +-292 years
// TODO: static assert this.

/*
 * Developer notice:
 * The TimeHandler concept has multiple purposes:
 * - time getting and sleeping is a little tricky to do correctly in a realtime context.
 * - abstraction
 * - DI for testing
 */

/**
 * Class to abstract time getting and calculating.
 */
class TimeHandler
{
   public:
    /**
     * Gets the current time.
     * @return the current time.
     */
    [[nodiscard]] virtual timepoint_t getTime() const = 0;

    /**
     * Calculates the elapsed time between two time points.
     * @param old The timepoint in the past.
     * @param current The current time.
     * @return current - old.
     */
    [[nodiscard]] virtual duration_t getElapsedTime(const timepoint_t& old, const timepoint_t& current) const = 0;

    /**
     * Calculates the elapsed time between curren time and another time point.
     * @param old The timepoint in the past.
     * @return current - old.
     */
    [[nodiscard]] virtual duration_t getElapsedTime(const timepoint_t& old) const = 0;

    /**
     * sleeps for the passed duration.
     * @param timeToSleep sleep for this duration
     */
    virtual void sleep(const duration_t& timeToSleep) const = 0;

    virtual ~TimeHandler() = default;
};

/**
 * This Class just uses nanosleep and clock_gettime with the systems clock.
 */
class BasicTimeHandler : public TimeHandler
{
   public:
    /**
     * Gets the current time. This will throw if clock_gettime returns an error.
     * @return the current time.
     */
    [[nodiscard]] timepoint_t getTime() const override;

    /**
     * Calculates the elapsed time.
     * If no current time is passed, the current time is determined by TimeHandler.getTime().
     * @param old The timepoint in the past.
     * @param current The current time.
     * @return current - old.
     */
    [[nodiscard]] duration_t getElapsedTime(const timepoint_t& old, const timepoint_t& current) const override;

    /**
     * Calculates the elapsed time between curren time and another time point.
     * @param old The timepoint in the past.
     * @return current - old.
     */
    [[nodiscard]] duration_t getElapsedTime(const timepoint_t& old) const override;

    /**
     * sleeps for the passed duration.
     * @param timeToSleep sleep for this duration
     */
    void sleep(const duration_t& timeToSleep) const override;
};

}  // namespace vr::RTUtils

namespace [[deprecated]] VR
{
using namespace vr;
}