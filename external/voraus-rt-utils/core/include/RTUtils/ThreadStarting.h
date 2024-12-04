#pragma once

#include <functional>
#include <future>

#include "RTUtils/core.h"

namespace vr::RTUtils
{
/**
 * Helper Class to be able to create a std::thread but don't run it right away.
 * This can be passed as first argument to a std::thread. The thread can then be resumed by calling passBarrier().
 * @warning If you pass this by reference and not by value, you have to make sure, that this class does not go
 * out of scope before the std::thread does to avoid returning to the operator() function when the objects is already
 * destroyed.
 */
class StartBarrier
{
   public:
    explicit StartBarrier(std::promise<void>& barrier) { barrierFuture_ = barrier.get_future(); }

    /**
     * Call operator.
     * This function will wait until passBarrier was called and then invoke the actual callable.
     * @note The Callable and the Arguments have the same restrictions as an std::thread and are either moved or copied
     * by value. If you need to pass them by reference, you can do so with a std::ref or std::cref. Beware of the
     * lifetime implications in this case.
     * @tparam Callable Callable type
     * @tparam Args Argument types
     * @param callable the actual callables
     * @param args the actual args.
     */
    template <class Callable, class... Args>
    void operator()(Callable&& callable, Args&&... args) const
    {
        barrierFuture_.wait();
        std::invoke(std::forward<Callable>(callable), std::forward<Args>(args)...);
    }

    /*
     * Developer notice:
     * This must be a static function.
     * Consider the following case:
     * 1.) An std::thread is created with a reference to a StartBarrier (no copy to be able to actually call passBarrier
     * later) 2.) barrier.passBarrier() is called. 3.) barrier goes out of scope. E.g. thread spawning could be done in
     * a helper function.
     * --> When the Callable returns to operator() the actual barrier is already destroyed --> UB.
     * Making this static and pass the promise allows passing the complete Barrier by value.
     */

    /**
     * Provides a value to a barrier future.
     * This should be used to allow passing a StartBarrier.
     * @param barrierPromise promise to the corresponding future that was passed to the constructor of a StartBarrier.
     */
    static void passBarrier(std::promise<void>& barrierPromise) { barrierPromise.set_value(); }

   private:
    std::future<void> barrierFuture_;
};

/**
 * Helper functions which properly starts a std::thread with realtime scheduler.
 * @note The Callable and the Arguments have the same restrictions as an std::thread and are either moved or copied by
 * value. If you need to pass them by reference, you can do so with a std::ref or std::cref. Beware of the lifetime
 * implications in this case.
 * @tparam Callable The type of the Callable
 * @tparam Args The types of the Arguments
 * @param schedParams the scheduling parameters to run with
 * @param callable the actual callable
 * @param args the arguments for the callable.
 * @return the std::thread which runs with the requested callable and scheduling parameters.
 */
template <class Callable, class... Args>
[[nodiscard]] std::thread startThread(const SchedParams& schedParams, Callable&& callable, Args&&... args)
{
    std::promise<void> barrierPromise;
    std::thread thread(StartBarrier(barrierPromise), std::forward<Callable>(callable), std::forward<Args>(args)...);
    setSchedulingParamsOfThread(thread, schedParams.first, schedParams.second);
    StartBarrier::passBarrier(barrierPromise);
    return thread;
}

/**
 * Helper functions which properly starts a std::thread with realtime scheduler.
 * @note The Callable and the Arguments have the same restrictions as an std::thread and are either moved or copied by
 * value. If you need to pass them by reference, you can do so with a std::ref or std::cref. Beware of the lifetime
 * implications in this case.
 * @tparam Callable The type of the Callable
 * @tparam Args The types of the Arguments
 * @param schedParams the scheduling parameters to run with
 * @param name The name of the new thread.
 * @param callable the actual callable
 * @param args the arguments for the callable.
 * @return the std::thread which runs with the requested callable, name and scheduling parameters.
 */
template <class Callable, class... Args>
[[nodiscard]] std::thread startNamedThread(const SchedParams& schedParams,
                                           const std::string& name,
                                           Callable&& callable,
                                           Args&&... args)
{
    std::promise<void> barrierPromise;
    std::thread thread(StartBarrier(barrierPromise), std::forward<Callable>(callable), std::forward<Args>(args)...);
    setSchedulingParamsOfThread(thread, schedParams.first, schedParams.second);
    setNameOfThread(thread, name);
    StartBarrier::passBarrier(barrierPromise);
    return thread;
}

}  // namespace vr::RTUtils

namespace [[deprecated]] VR
{
using namespace vr;
}