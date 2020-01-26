#include "latch_darwin.hpp"

// clang-format off
#include <chrono>
#include <system_error>
#include <cerrno>

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <mach/semaphore.h>
// clang-format on

using std::system_category;
using std::system_error;

/**
 * @brief RAII for `pthread_mutexattr_t`
 * @see pthread_mutexattr_t
 */
struct _Mutex_attr_t final {
    pthread_mutexattr_t impl{};

    _Mutex_attr_t() noexcept(false) {
        if (auto ec = pthread_mutexattr_init(&impl))
            throw system_error{ec, system_category(), "pthread_mutexattr_init"};

        if (auto ec = pthread_mutexattr_settype(&impl, //
                                                PTHREAD_MUTEX_ERRORCHECK))
            throw system_error{ec, system_category(),
                               "pthread_mutexattr_settype"};
// see pthread.h in /ndk/sysroot/usr/include
// general POSIX or NDK high version
#if !defined(__ANDROID_API__) ||                                               \
    (defined(__ANDROID_API__) && __ANDROID_API__ >= 28)
        if (auto ec = pthread_mutexattr_setprotocol(&impl, //
                                                    PTHREAD_PRIO_NONE))
            throw system_error{ec, system_category(),
                               "pthread_mutexattr_setprotocol"};
#endif
    }
    ~_Mutex_attr_t() noexcept(false) {
        if (auto ec = pthread_mutexattr_destroy(&impl))
            throw system_error{ec, system_category(),
                               "pthread_mutexattr_destroy"};
    }

    pthread_mutexattr_t* get() {
        return &impl;
    }
};

/**
 * @brief Acquire current itme in `<chrono>` duration
 * 
 * @return std::chrono::nanoseconds 
 * @see _Time_after
 */
auto _Time_current() noexcept -> std::chrono::nanoseconds {
    return std::chrono::system_clock::now().time_since_epoch();
}

auto _Time_after(std::chrono::nanoseconds d,
                 std::chrono::nanoseconds& tp) noexcept -> timespec {
    using namespace std::chrono;
    tp = _Time_current() + d;
    return timespec{
        .tv_sec = duration_cast<seconds>(tp).count(),
        .tv_nsec = duration_cast<nanoseconds>(tp).count() % 1'000'000,
    };
}

int _Cond_timed_wait(pthread_mutex_t& mtx, pthread_cond_t& cv,
                     const timespec& until) noexcept {
    if (auto ec = pthread_mutex_lock(&mtx))
        return ec;
    auto reason = pthread_cond_timedwait(&cv, &mtx, &until);
    if (auto ec = pthread_mutex_unlock(&mtx))
        return ec;
    // fprintf(stderr, "%p %s %d\n", pthread_self(), "pthread_cond_timedwait",
    //         reason);
    return reason;
}

int _Latch_timed_wait(const ptrdiff_t& counter, //
                      pthread_mutex_t& mtx, pthread_cond_t& cv,
                      std::chrono::nanoseconds sleep_duration) noexcept {
    using namespace std;
    using namespace std::chrono;
    int ec = 0;

    // Calculate the timepoint for `pthread_cond_timedwait`,
    // which uses absolute time(== time point)
    std::chrono::nanoseconds end_time{};
    auto until = _Time_after(sleep_duration, end_time);
    // fprintf(stderr, "sec %zu nsec %zu\n", until.tv_sec, until.tv_nsec);

CheckCurrentCount:
    if (counter == 0)
        return 0;

    // check time since it can be a spurious wakeup
    if (end_time < _Time_current())
        // reached timeout. return error
        return ec;

    ec = _Cond_timed_wait(mtx, cv, until);
    if (ec == ETIMEDOUT) // this might be a spurious wakeup
        goto CheckCurrentCount;
    // reason containes error code at this moment
    return ec;
}

namespace std {

latch::latch(ptrdiff_t expected) noexcept(false)
    : counter{expected}, cv{}, mtx{} {

    // instread of std::mutex, use customized attributes
    _Mutex_attr_t attr{};

    if (auto ec = pthread_mutex_init(&mtx, attr.get()))
        throw system_error{ec, system_category(), "pthread_mutex_init"};
    if (auto ec = pthread_cond_init(&cv, nullptr)) {
        pthread_mutex_destroy(&mtx);
        throw system_error{ec, system_category(), "pthread_cond_init"};
    }
}

latch::~latch() noexcept(false) {
    if (auto ec = pthread_cond_destroy(&cv)) {
        pthread_mutex_destroy(&mtx);
        throw system_error{ec, system_category(), "pthread_cond_destroy"};
    }
    if (auto ec = pthread_mutex_destroy(&mtx))
        throw system_error{ec, system_category(), "pthread_mutex_destroy"};
}

void latch::arrive_and_wait(ptrdiff_t update) noexcept(false) {
    count_down(update);
    wait();
}

void latch::count_down(ptrdiff_t update) noexcept(false) {
    if (counter < update)
        throw system_error{EINVAL, system_category(),
                           "update is greater than counter"};
    counter -= update;
    if (counter > 0)
        return;

    // fprintf(stderr, "%p %s \n", pthread_self(), "pthread_cond_signal");
    if (auto ec = pthread_cond_signal(&cv))
        throw system_error{ec, system_category(), "pthread_cond_signal"};
}

bool latch::try_wait() const noexcept {
    // if counter equals zero, returns immediately
    if (counter == 0)
        return true;

    // The sleep time is random-picked for ease of debugging
    const auto sleep_duration = 1536ms;
    // Since latch doesn't provide interface for waiting with timeout,
    // the code can wait more when the counter reached 0
    if (const auto ec = _Latch_timed_wait(counter, mtx, cv, sleep_duration)) {
        errno = ec;
        return false;
    }
    return counter == 0;
}

void latch::wait() const noexcept(false) {
    while (try_wait() == false) {
        if (errno != ETIMEDOUT)
            throw system_error{errno, system_category(), "_Latch_timed_wait"};
    }
}

} // namespace std