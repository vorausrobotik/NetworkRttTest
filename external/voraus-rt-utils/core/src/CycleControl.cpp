#include <utility>

#include <iostream>
#include "RTUtils/CycleControl.h"

namespace vr::RTUtils
{
CycleControl::CycleControl(duration_t cycleTime,
                           std::shared_ptr<TimeHandler> timeHandler,
                           duration_t maxAllowedCycleTimeDeviation)
    : cycleTime_(cycleTime),
      maxAllowedCycleTimeDeviation_(maxAllowedCycleTimeDeviation),
      timeHandler_(std::move(timeHandler))
{
    startNextCycleNow();
}

duration_t CycleControl::sleepAndStartNextCycle()
{
    duration_t remaining = getRemainingTime();
    timeHandler_->sleep(remaining);
    startNextNominalCycle_();
    return remaining;
}

void CycleControl::startNextCycleNow()
{
    cycleStart_ = timeHandler_->getTime();
    phaseCorrection_ = std::nullopt;
}

bool CycleControl::setPhaseCorrection(duration_t correction)
{
    bool clamped = false;
    if (correction > duration_t(0) && correction > maxAllowedCycleTimeDeviation_)
    {
        correction = maxAllowedCycleTimeDeviation_;
        clamped = true;
    }
    else if (correction < duration_t(0) && correction < maxAllowedCycleTimeDeviation_ * -1)
    {
        correction = -1 * maxAllowedCycleTimeDeviation_;
        clamped = true;
    }
    phaseCorrection_ = correction;
    return clamped;
}
duration_t CycleControl::getNominalCycleTime() const
{
    return cycleTime_;
}
duration_t CycleControl::getMaxAllowedCycleTimeDeviation() const
{
    return maxAllowedCycleTimeDeviation_;
}
duration_t CycleControl::getRemainingTime() const
{
    return getNextCycleStart_() - timeHandler_->getTime();
}
duration_t CycleControl::getCurrentCycleTime() const
{
    return cycleTime_ + phaseCorrection_.value_or(duration_t(0));
}

void CycleControl::skipCycle()
{
    cycleStart_ += getNominalCycleTime();
}

timepoint_t CycleControl::getCycleStart() const
{
    return cycleStart_;
}

timepoint_t CycleControl::getNextCycleStart_() const
{
    return cycleStart_ + getCurrentCycleTime();
}

void CycleControl::startNextNominalCycle_()
{
    cycleStart_ = getNextCycleStart_();
    phaseCorrection_ = std::nullopt;
}

duration_t calculatePhaseCorrection(duration_t lastSyncPoint, const PllParameter& pllParameter)
{
    duration_t syncError = (lastSyncPoint - pllParameter.shift) % pllParameter.cycleTime;
    if (syncError > (pllParameter.cycleTime / 2))
    {
        syncError -= pllParameter.cycleTime;
    }
    // for now just use a P part (and no I part)
    // phase correction is calculated using the syncError to the target time domain
    // missed sync point --> slightly shorter cycle
    // before sync point --> slightly longer cycle
    double correctionFactor = std::chrono::duration<double>(-syncError).count() /
                              std::chrono::duration<double>((pllParameter.cycleTime / 2)).count();
    return std::chrono::duration_cast<duration_t>(correctionFactor * pllParameter.maxDeviation);
}
}  // namespace vr::RTUtils