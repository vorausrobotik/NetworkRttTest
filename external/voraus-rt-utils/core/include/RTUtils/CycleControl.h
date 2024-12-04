#pragma once
#include "RTUtils/TimeHandler.h"

#include <memory>

namespace vr::RTUtils
{
/**
 * This class can be used to do hold a specific cycle.
 * After the normal work the sleepAndStartNextCycle() method can be called.
 * This will calculate the remaining time in the current cycle and calls sleep accordingly.
 * It will also return the time it slept. If the current cycle took to long and the
 * next cycle is already started, a negative value is returned.
 * The caller must act appropriately on this value (e.g. abort the program on repeated violations).
 * If a cycle is missed, CycleControl will try to 'catch up' by default.
 * If this is undesired or not possible, startNextCycleNow() or skipCycle() can be used.
 * It is also possible to sync the cyclecontrol to another clock (adjusting the phase similar to a PLL) by
 * slightly shortening or extending the cycle.
 */
class CycleControl
{
   public:
    /**
     * Creates a CycleControl.
     * @param cycleTime
     * @param timeHandler
     * @param maxAllowedCycleTimeDeviation
     */
    CycleControl(duration_t cycleTime,
                 std::shared_ptr<TimeHandler> timeHandler,
                 duration_t maxAllowedCycleTimeDeviation = std::chrono::nanoseconds(0));

    /**
     * This can be used to adjust the cycle time for one cycle to influence the phase of this CycleControl.
     * The passed value is clipped in order to respect maxAllowedDeviation.
     * @param correction positive value to extending the current cycle and negative value to shortening the current
     * cycle.
     * @return true if the value was clipped to meet the maxAllowedDeviation restriction.
     */
    bool setPhaseCorrection(duration_t correction);

    /**
     * This can be used when it is not possible or not desired to recover from a serious cycle time violation.
     * The new cycle is started at the current time.
     */
    void startNextCycleNow();

    /**
     * Sleeps for the remaining cycle time and starts a new cycle.
     * This does also "consume" the last syncingCorrection.
     * @warning If the return value of this method is not checked and the system violates the cycle time on
     * average, a sleep will never be issued. This can lock up your entire system if the process runs with a high
     * realtime priority. Make sure you handle cycle time violations appropriately. If you don't use this cycle control
     * for at least a few cycles you can call startNextCycleNow() or skip the appropriate amount of cycles using
     * skipCycle(). Then the cycle control won't try to "catch up".
     * @return the "remaining" time of the cycle which was slept.
     * If this is negative, the last cycle was too long and no sleep was issued.
     */
    [[nodiscard]] duration_t sleepAndStartNextCycle();

    /**
     * Skips a cycle.
     * This can be used when it is not possible or not desired to recover from a serious cycle time violation.
     */
    void skipCycle();

    /**
     * Returns the nominal cycle time.
     * @return
     */
    [[nodiscard]] duration_t getNominalCycleTime() const;

    /**
     * Returns the maximum allowed deviation (due to syncing this cycle control to another clock).
     * @return
     */
    [[nodiscard]] duration_t getMaxAllowedCycleTimeDeviation() const;

    /**
     * Returns the current cycle time (including phase correction)
     * @return
     */
    [[nodiscard]] duration_t getCurrentCycleTime() const;

    /**
     * Returns the remaining time of the current cycle.
     */
    [[nodiscard]] duration_t getRemainingTime() const;

    /**
     * Returns the start timestamp of the current cycle.
     */
    [[nodiscard]] timepoint_t getCycleStart() const;

   private:
    timepoint_t cycleStart_{};
    duration_t cycleTime_{};
    duration_t maxAllowedCycleTimeDeviation_{};
    std::shared_ptr<TimeHandler> timeHandler_;
    std::optional<duration_t> phaseCorrection_ = std::nullopt;

    [[nodiscard]] timepoint_t getNextCycleStart_() const;

    void startNextNominalCycle_();
};

struct PllParameter
{
    duration_t cycleTime;
    duration_t shift;
    duration_t maxDeviation;
};

/**
 * This function can be used to calculate phase corrections to synchronize to another time domain.
 * The passed sync Point is expected to be a timepoint in a monotonically increasing time domain.
 * This function calculates a phase correction to minimize the sync error: lastSyncPoint % cycletime + shift.
 *
 * @param lastSyncPoint The last sync point
 * @param pllParameter Pll parameter to use.
 * @return the calculated phase correction.
 */
duration_t calculatePhaseCorrection(duration_t lastSyncPoint, const PllParameter& pllParameter);
}  // namespace vr::RTUtils

namespace [[deprecated]] VR
{
using namespace vr;
}
