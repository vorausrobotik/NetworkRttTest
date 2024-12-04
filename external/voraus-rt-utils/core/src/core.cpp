#include "RTUtils/core.h"

#include <pthread.h>
#include <iostream>

#include <fcntl.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <climits>
#include <cstring>
#include <fstream>
#include <limits>

namespace vr::RTUtils
{
constexpr auto SLEEP_STATE_LATENCY_FILE = "/dev/cpu_dma_latency";

constexpr size_t MAX_NAME_LENGTH = 100;

/*
 * Developer notice:
 * This helper functions could either be placed in a header file or in this (private) cpp file.
 * Placing this in a header file has the advantage that it can deal with non-existent scheduling options by using
 * preprocessor macros at the costs of having to include pthread.h in the header (slower build).
 * However, conan should take care of different compiler and libc versions anyway, so this is placed in the source file.
 */
static constexpr int schedPolicyEnumToInt(const SchedulingPolicy& policy)
{
    switch (policy)
    {
        case SchedulingPolicy::OTHER:
            return SCHED_OTHER;
        case SchedulingPolicy::IDLE:
            return SCHED_IDLE;
        case SchedulingPolicy::BATCH:
            return SCHED_BATCH;
        case SchedulingPolicy::FIFO:
            return SCHED_FIFO;
        case SchedulingPolicy::RR:
            return SCHED_RR;
        case SchedulingPolicy::DEADLINE:
            return SCHED_DEADLINE;
    }
    throw std::logic_error("no handler for passed scheduling policy! This should never happen.");
}

static constexpr SchedulingPolicy intToSchedPolicyEnum(int policy)
{
    switch (policy)
    {
        case SCHED_OTHER:
            return SchedulingPolicy::OTHER;
        case SCHED_IDLE:
            return SchedulingPolicy::IDLE;
        case SCHED_BATCH:
            return SchedulingPolicy::BATCH;
        case SCHED_FIFO:
            return SchedulingPolicy::FIFO;
        case SCHED_RR:
            return SchedulingPolicy::RR;
        case SCHED_DEADLINE:
            return SchedulingPolicy::DEADLINE;
        default:
            throw std::logic_error("no handler for passed scheduling policy!");
    }
}

static void setSchedulingParamsOfPThread(pthread_t threadHandle, const SchedulingPolicy& schedulingPolicy, int priority)
{
    sched_param schedParam{};
    int targetPolicy = schedPolicyEnumToInt(schedulingPolicy);
    int policy;
    if (pthread_getschedparam(threadHandle, &policy, &schedParam) != 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                "failed to get scheduling parameters! This should never happen.");
    }
    if (schedParam.sched_priority == priority && policy == targetPolicy)
    {  // thread has already the correct policy and priority
        return;
    }
    schedParam.sched_priority = priority;
    if (pthread_setschedparam(threadHandle, targetPolicy, &schedParam) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "failed to set scheduling parameters!");
    }
}

static std::pair<cpu_set_t*, size_t> allocCpuSet(size_t numberOfCpus)
{
    auto* cpuset = CPU_ALLOC(numberOfCpus);
    auto cpusetsize = CPU_ALLOC_SIZE(numberOfCpus);
    if (cpuset == nullptr)
    {
        throw std::system_error(errno, std::generic_category(),
                                "failed to allocate a cpuset of size: " + std::to_string(cpusetsize));
    }
    return {cpuset, cpusetsize};
}

static void setCpuAffinityOfPthread(pthread_t threadHandle, const std::vector<bool>& cpuAffinity)
{
    // "translate" cpuAffinity into a dynamically sized cpu set in pthread format
    auto [cpuset, cpusetsize] = allocCpuSet(cpuAffinity.size());

    CPU_ZERO_S(cpusetsize, cpuset);
    for (size_t i = 0; i < cpuAffinity.size(); ++i)
    {
        if (cpuAffinity[i])
        {
            CPU_SET_S(i, cpusetsize, cpuset);
        }
    }

    if (auto result = pthread_setaffinity_np(threadHandle, cpusetsize, cpuset); result != 0)
    {
        CPU_FREE(cpuset);
        throw std::system_error(result, std::generic_category(), "failed to set cpu affinity!");
    }
    CPU_FREE(cpuset);
}

static std::vector<bool> getCpuAffinityOfPthread(pthread_t threadHandle)
{
    // Ultimately, we want to use pthread_getaffinity_np(pthread_t thread, size_t cpusetsize, cpu_set_t *cpuset)
    // for getting the cpu affinity. However, pthread_getaffinity_np fails if the passed cpusetsize is smaller than the
    // size of the affinity mask used by the kernel.
    // Also, apparently, there is no way to know the size of the affinity mask used by the kernel,
    // other than probing with increasingly larger cpu sets.
    // --> do that

    // keep this number always at a multiple of number of bits in a unsigned long,
    // Although the CPU_ALLOC macro allows any number, but the resulting size is rounded up to full unsigned long anyway
    // pthread_getaffinity_np just uses the size later
    constexpr size_t startSize = sizeof(unsigned long) * CHAR_BIT;

    constexpr size_t maxTries = 20;  // try only to double the size maxTries times.

    size_t numberOfCpusToTry = startSize;
    bool success = false;

    auto [cpuset, cpusetsize] = allocCpuSet(numberOfCpusToTry);

    for (size_t i = 0; i < maxTries; ++i)
    {
        auto result = pthread_getaffinity_np(threadHandle, cpusetsize, cpuset);
        if (result == 0)
        {
            success = true;
            break;
        }
        CPU_FREE(cpuset);
        if (result == EINVAL)
        {
            numberOfCpusToTry *= 2;
            std::tie(cpuset, cpusetsize) = allocCpuSet(numberOfCpusToTry);
            continue;
        }
        throw std::system_error(result, std::generic_category(),
                                "Failed to get the cpu affinity with pthread_getaffinity_np!");
    }
    // now we either have a valid cpu set or exceeded the max retries
    if (!success)
    {
        throw std::system_error(EINVAL, std::generic_category(),
                                "Failed to get the cpu affinity with pthread_getaffinity_np, the internal kernel "
                                "representation seems to hold more than " +
                                    std::to_string(numberOfCpusToTry) + "cpus!");
    }

    size_t count = CPU_COUNT_S(cpusetsize, cpuset);

    std::vector<bool> result;
    for (size_t i = 0; i < numberOfCpusToTry; ++i)
    {
        if (CPU_ISSET_S(i, cpusetsize, cpuset))
        {
            result.push_back(true);
            count -= 1;
            if (count == 0)
            {
                break;
            }
        }
        else
        {
            result.push_back(false);
        }
    }
    CPU_FREE(cpuset);
    return result;
}

static experimental::RtStats doGetrusage(int who)
{
    rusage usage{};
    if (getrusage(who, &usage) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "getrusage() failed!");
    }
    return {
        .minorPageFaults = usage.ru_minflt,
        .majorPageFaults = usage.ru_majflt,
        .voluntaryContextSwitches = usage.ru_nvcsw,
        .involuntaryContextSwitches = usage.ru_nivcsw,
    };
}

void setSchedulingParamsOfCurrentThread(const SchedulingPolicy& schedulingPolicy, int priority)
{
    setSchedulingParamsOfPThread(pthread_self(), schedulingPolicy, priority);
}

void setSchedulingParamsOfThread(std::thread& thread, const SchedulingPolicy& schedulingPolicy, int priority)
{
    setSchedulingParamsOfPThread(thread.native_handle(), schedulingPolicy, priority);
}

int getPriorityOfCurrentThread()
{
    return getSchedulingParamsOfCurrentThread().second;
}

int getPriorityOfThread(std::thread& thread)
{
    return getSchedulingParamsOfThread(thread).second;
}

int preventSleepStates()
{
    return setMaxAllowedSleepStateTransitionDelay(0);
}

void lockMemory()
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "failed to execute mlockall!");
    }
}

void allowSleepStates(int fd)  // NOLINT(readability-identifier-length)
{
    close(fd);
}

void unlockMemory()
{
    munlockall();
}

int getMaxAllowedSleepStateTransitionDelay()
{
    std::ifstream dmaLatencyFile(SLEEP_STATE_LATENCY_FILE, std::ios::binary);
    if (!dmaLatencyFile.is_open())
    {
        throw std::system_error(errno, std::generic_category(),
                                std::string("failed to open ") + SLEEP_STATE_LATENCY_FILE);
    }
    int maxTransitionDelay{};
    dmaLatencyFile.read(reinterpret_cast<char*>(&maxTransitionDelay), sizeof(maxTransitionDelay));
    return maxTransitionDelay;
}

static std::pair<SchedulingPolicy, int> getSchedulingParamsOfPThread(pthread_t handle)
{
    sched_param schedParam{};
    int policy;

    if (pthread_getschedparam(handle, &policy, &schedParam) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "failed to set scheduling parameters!");
    }
    return std::make_pair(intToSchedPolicyEnum(policy), schedParam.sched_priority);
}

std::pair<SchedulingPolicy, int> getSchedulingParamsOfCurrentThread()
{
    return getSchedulingParamsOfPThread(pthread_self());
}

std::pair<SchedulingPolicy, int> getSchedulingParamsOfThread(std::thread& thread)
{
    return getSchedulingParamsOfPThread(thread.native_handle());
}

SchedulingPolicy getSchedulingPolicyOfCurrentThread()
{
    return getSchedulingParamsOfCurrentThread().first;
}

int setMaxAllowedSleepStateTransitionDelay(int delay)
{
    // this can not be done with an ofstream because the file must NOT be closed.
    int fd = open(SLEEP_STATE_LATENCY_FILE, O_WRONLY);  // NOLINT(readability-identifier-length)
    if (fd < 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                (std::string("failed to open ") + SLEEP_STATE_LATENCY_FILE).c_str());
    }
    if (delay > std::numeric_limits<int32_t>::max())
    {
        throw std::invalid_argument("latency must not be greater than " +
                                    std::to_string(std::numeric_limits<int32_t>::max()));
    }
    int32_t latency = static_cast<int32_t>(delay);
    auto result = write(fd, &latency, sizeof(latency));
    if (result < 0)
    {
        throw std::system_error(errno, std::generic_category(),
                                (std::string("failed to write ") + SLEEP_STATE_LATENCY_FILE).c_str());
    }
    if (result != sizeof(latency))
    {
        throw std::runtime_error((std::string("could not write all bytes to ") + SLEEP_STATE_LATENCY_FILE).c_str());
    }
    // file is intentionally left open
    return fd;
}

SchedulingPolicy getSchedulingPolicyOfThread(std::thread& thread)
{
    return getSchedulingParamsOfThread(thread).first;
}

static void setNameOfPThread(pthread_t handle, const std::string& name)
{
    int result = pthread_setname_np(handle, name.c_str());
    if (result != 0)
    {
        throw std::system_error(result, std::generic_category(), "failed to set thread name!");
    }
}

void setNameOfThread(std::thread& thread, const std::string& name)
{
    setNameOfPThread(thread.native_handle(), name);
}

void setNameOfCurrentThread(const std::string& name)
{
    setNameOfPThread(pthread_self(), name);
}

static std::string getNameOfPThread(pthread_t handle)
{
    char nameBuffer[MAX_NAME_LENGTH]{};
    int result = pthread_getname_np(handle, nameBuffer, sizeof(nameBuffer));
    if (result != 0)
    {
        throw std::system_error(result, std::generic_category(), "could not get thread name");
    }
    return {nameBuffer};
}

std::string getNameOfThread(std::thread& thread)
{
    return getNameOfPThread(thread.native_handle());
}

std::string getNameOfCurrentThread()
{
    return getNameOfPThread(pthread_self());
}

void setCpuAffinityOfThread(std::thread& thread, const std::vector<bool>& affinityMask)
{
    setCpuAffinityOfPthread(thread.native_handle(), affinityMask);
}

void setCpuAffinityOfJthread(std::jthread& thread, const std::vector<bool>& affinityMask)
{
    setCpuAffinityOfPthread(thread.native_handle(), affinityMask);
}

void setCpuAffinityOfCurrentThread(const std::vector<bool>& affinityMask)
{
    setCpuAffinityOfPthread(pthread_self(), affinityMask);
}

std::vector<bool> getCpuAffinityOfThread(std::thread& thread)
{
    return getCpuAffinityOfPthread(thread.native_handle());
}

std::vector<bool> getCpuAffinityOfJthread(std::jthread& thread)
{
    return getCpuAffinityOfPthread(thread.native_handle());
}

std::vector<bool> getCpuAffinityOfCurrentThread()
{
    return getCpuAffinityOfPthread(pthread_self());
}

experimental::RtStats experimental::RtStats::operator+(const experimental::RtStats& other) const
{
    return {.minorPageFaults = this->minorPageFaults + other.minorPageFaults,
            .majorPageFaults = this->majorPageFaults + other.majorPageFaults,
            .voluntaryContextSwitches = this->voluntaryContextSwitches + other.voluntaryContextSwitches,
            .involuntaryContextSwitches = this->involuntaryContextSwitches + other.involuntaryContextSwitches};
}

experimental::RtStats experimental::RtStats::operator-(const experimental::RtStats& other) const
{
    return {.minorPageFaults = this->minorPageFaults - other.minorPageFaults,
            .majorPageFaults = this->majorPageFaults - other.majorPageFaults,
            .voluntaryContextSwitches = this->voluntaryContextSwitches - other.voluntaryContextSwitches,
            .involuntaryContextSwitches = this->involuntaryContextSwitches - other.involuntaryContextSwitches};
}

std::string experimental::RtStats::toString() const
{
    return "{minorPageFaults: " + std::to_string(minorPageFaults) +
           ", majorPageFaults: " + std::to_string(majorPageFaults) +
           ", voluntaryContextSwitches: " + std::to_string(voluntaryContextSwitches) +
           ", involuntaryContextSwitches: " + std::to_string(involuntaryContextSwitches) + "}";
}

experimental::RtStats experimental::getRtStatsOfCurrentThread()
{
    return doGetrusage(RUSAGE_THREAD);
}

experimental::RtStats experimental::getRtStatsOfCurrentProcess()
{
    return doGetrusage(RUSAGE_SELF);
}
}  // namespace vr::RTUtils