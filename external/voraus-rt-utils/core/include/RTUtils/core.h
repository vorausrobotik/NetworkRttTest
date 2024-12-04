#pragma once
#include <thread>
#include <utility>
#include <vector>

namespace vr::RTUtils
{
/**
 * Enum for scheduling policies.
 */
enum SchedulingPolicy
{
    OTHER,
    IDLE,
    BATCH,
    FIFO,
    RR,
    DEADLINE,
};

/**
 * Sets the passed scheduling parameters of the current thread.
 * Note: only the realtime scheduler policies (FIFO, DEADLINE...) allow setting a priority other than zero.
 * The "standard" scheduler used by linux is OTHER.
 * @warning use this with care. A Realtime Policy (e.g. FIFO) can crash or even destroy the system when used wrong.
 * @param schedulingPolicy the scheduling policy to use.
 * @param priority the priority to use.
 * @throw std::system_error if setting the passed parameters fails. This can happen due to invalid values (only realtime
 * schedulers allow priorities other than zero), insufficient privileges etc.
 */
void setSchedulingParamsOfCurrentThread(const SchedulingPolicy& schedulingPolicy, int priority);

/**
 * Sets the passed scheduling parameters of a thread.
 * Note: only the realtime scheduler policies (FIFO, DEADLINE...) allow setting a priority other than zero.
 * The "standard" scheduler used by linux is OTHER.
 * @param thread the thread to set the policy on
 * @param schedulingPolicy the scheduling policy to use.
 * @param priority the priority to use.
 * @throw std::system_error if setting the passed parameters fails. This can happen due to invalid values (only realtime
 * schedulers allow priorities other than zero), insufficient privileges etc.
 */
void setSchedulingParamsOfThread(std::thread& thread, const SchedulingPolicy& schedulingPolicy, int priority);

typedef std::pair<SchedulingPolicy, int> SchedParams;
/**
 * Gets the scheduling parameters (policy and priority) of the current thread.
 * @return the scheduling parameters (policy and priority)
 */
SchedParams getSchedulingParamsOfCurrentThread();

/**
 * Gets the scheduling parameters (policy and priority) of the passed thread
 * @return the scheduling parameters (policy and priority)
 */
SchedParams getSchedulingParamsOfThread(std::thread& thread);

/**
 * returns the scheduling priority of the current thread.
 * @return  priority
 */
int getPriorityOfCurrentThread();

/**
 * returns the scheduling priority of a given thread.
 * @return  priority
 */
int getPriorityOfThread(std::thread& thread);

/**
 * returns the scheduling policy of the current thread
 * @return the scheduling policy.
 */
SchedulingPolicy getSchedulingPolicyOfCurrentThread();

/**
 * returns the scheduling policy of a given thread
 * @return the scheduling policy
 */
SchedulingPolicy getSchedulingPolicyOfThread(std::thread& thread);

/**
 * Sets the name of a std::thread.
 * @warning The maximum character limit for a thread name is 15 characters!
 * @throws std::system_error when setting the name fails.
 * @param thread  the thread to set the name of.
 * @param name  the name to set.
 */
void setNameOfThread(std::thread& thread, const std::string& name);

/**
 * Sets the name of the current thread
 * @warning The maximum character limit for a thread name is 15 characters!
 * @throws std::system_error when setting the name fails.
 * @param name the name to set.
 */
void setNameOfCurrentThread(const std::string& name);

/**
 * Returns the name of a thread.
 * @throws std::system_error when getting the name fails.
 * @param thread the thread to get the name of.
 * @return the name.
 */
std::string getNameOfThread(std::thread& thread);

/**
 * Gets the name of the current thread
 * @throws std::system_error when getting the name fails.
 * @return the name of the thread.
 */
std::string getNameOfCurrentThread();

/**
 * returns the currently used, maximum delay limit for power state transitions by reading from /dev/cpu_dma_latency.
 * @return currently used maximum delay limit for power state transitions in µs
 */
int getMaxAllowedSleepStateTransitionDelay();

/**
 * Sets the limit for cpu sleep transition latency by writing to /dev/cpu_dma_latency and keep the file open.
 * This call is undone by calling allowSleepStates() with the returned file descriptor.
 * This function can be called multiple times. The resulting max latency is always the minimum passed delay of the calls
 * (See tests for examples). If this is called with a delay of zero, the cpu will not go to sleep at all (see
 * preventSleepStates())
 * @param delay maximum tolerable latency caused by cpu sleep state transitions.
 * @return the file descriptor of /dev/cpu_dma_latency. Pass this to allowSleepStates() to undo the limit.
 * @throw std::system_error if opening or writing to /dev/cpu_dma_latency is not possible (due to missing permissions or
 * for other reasons)
 */
int setMaxAllowedSleepStateTransitionDelay(int delay);

/**
 * prevent cpu sleep states (and the resulting latencies during switching) by calling
 * setMaxAllowedSleepStateTransitionDelay with zero.
 * @warning This must be used with care as the cpu will no longer enter sleep states resulting in a load average of N
 * (while N is the number of cpus)
 * @return the file descriptor to /dev/cpu_dma_latency
 */
int preventSleepStates();

/**
 * undos the previously set limit by closing the passed file descriptor.
 * @param fd file descriptor (of /dev/cpu_dma_latency)
 */
void allowSleepStates(int fd);  // NOLINT(readability-identifier-length)

/**
 * locks currently used, and future memory in RAM by calling mlockall.
 * @throws std::system_error if the syscall is not successfull.
 */
void lockMemory();

/**
 * unlocks the memory (allows swapping pages of memory out of RAM).
 * if lockMemory() was called multiple times, a single call to unlockMemory() is sufficient to unlock the memory again.
 */
void unlockMemory();

/**
 * Sets the Cpu affinity of the passed thread using pthread_setaffinity_np.
 * @warning the Kernel might silently restrict the passed mask further, depending on available cpus and cpusets.
 * @param thread The thread to modify the affinity mask of.
 * @param affinityMask The affinity mask to set.
 * @throw std::sytem_error if pthread_setaffinity_np fails.
 */
void setCpuAffinityOfThread(std::thread& thread, const std::vector<bool>& affinityMask);

/**
 * Sets the cpu affinity of a passed jthread.
 * @see setCpuAffinityOfThread
 */
void setCpuAffinityOfJthread(std::jthread& thread, const std::vector<bool>& affinityMask);

/**
 * Sets the Cpu affinity of the current thread.
 * @see setCpuAffinityOfThread
 */
void setCpuAffinityOfCurrentThread(const std::vector<bool>& affinityMask);

/**
 * Gets the cpu affinity mask of the passed thread using pthread_getaffinity_np.
 * The resulting vector is only as long as required to hold all "true" values.
 * If the affinity mask is [false, false, true, false, false, ...] on a 16 core machine, the result ist [false, false,
 * true].
 * @param thread The thread to get the affinity mask of.
 * @throw std::sytem_error if pthread_getaffinity_np fails.
 * @return The cpu affinity mask of the thread.
 */
std::vector<bool> getCpuAffinityOfThread(std::thread& thread);

/**
 * Gets the cpu affinity mask of a passed jthread.
 * @see getCpuAffinityOfThread
 */
std::vector<bool> getCpuAffinityOfJthread(std::jthread& thread);

/**
 * Gets the cpu affinity mask of the current thread.
 * @see getCpuAffinityOfThread
 */
std::vector<bool> getCpuAffinityOfCurrentThread();

namespace experimental
{
    /**
     * Return value from getRtStatsOfCurrentThread() and getRtStatsOfCurrentProcess().
     */
    struct RtStats
    {
        /** Number of Page faults without I/O activity*/
        long int minorPageFaults;
        /** Number of Page faults which required I/O activity (e.g. read from SWAP). */
        long int majorPageFaults;

        /** The number of voluntary context switches */
        long int voluntaryContextSwitches;

        /** the number of involuntary context switches */
        long int involuntaryContextSwitches;

        /**
         * Adds the statistics element wise.
         * @param other the operand.
         * @return self + other
         */
        RtStats operator+(const RtStats& other) const;

        /**
         * subtracts the statistics element wise.
         * @param other the operand.
         * @return self - other.
         */
        RtStats operator-(const RtStats& other) const;

        bool operator==(const RtStats&) const = default;

        std::string toString() const;
    };

    /**
     * Returns the RtStats of the current thread.
     * It uses getrusage internally.
     * @throws std::system_error if getrusage fails.
     * @return
     */
    RtStats getRtStatsOfCurrentThread();

    /**
     * Returns the RtStats of the current process.
     * It uses getrusage internally.
     * @throws std::system_error if getrusage fails.
     * @return
     */
    RtStats getRtStatsOfCurrentProcess();

}  // namespace experimental

}  // namespace vr::RTUtils

namespace [[deprecated]] VR
{
using namespace vr;
}
