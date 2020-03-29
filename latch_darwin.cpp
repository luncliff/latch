#include "latch_darwin.h"

#include <cerrno>
#include <chrono>
#include <mutex>
#include <system_error>

#include <sys/time.h>

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

    pthread_mutexattr_t* native() {
        return &impl;
    }
};

_Pthread_mutex_t::_Pthread_mutex_t() noexcept(false) : impl{} {
    _Mutex_attr_t attr{};
    if (auto ec = pthread_mutex_init(&impl, attr.native()))
        throw system_error{ec, system_category(), "pthread_mutex_init"};
}
_Pthread_mutex_t::~_Pthread_mutex_t() noexcept(false) {
    if (auto ec = pthread_mutex_destroy(&impl))
        throw system_error{ec, system_category(), "pthread_mutex_destroy"};
}
void _Pthread_mutex_t::lock() noexcept(false) {
    if (auto ec = pthread_mutex_lock(&impl))
        throw system_error{ec, system_category(), "pthread_mutex_lock"};
}
void _Pthread_mutex_t::unlock() noexcept(false) {
    if (auto ec = pthread_mutex_unlock(&impl))
        throw system_error{ec, system_category(), "pthread_mutex_unlock"};
}
bool _Pthread_mutex_t::try_lock() noexcept {
    return pthread_mutex_trylock(&impl) == 0;
}

_Pthread_cond_t::_Pthread_cond_t() noexcept(false) : impl{} {
    if (auto ec = pthread_cond_init(&impl, nullptr))
        throw system_error{ec, system_category(), "pthread_cond_init"};
}
_Pthread_cond_t::~_Pthread_cond_t() noexcept(false) {
    if (auto ec = pthread_cond_destroy(&impl))
        throw system_error{ec, system_category(), "pthread_cond_destroy"};
}
int32_t _Pthread_cond_t::notify_one() noexcept {
    return pthread_cond_signal(&impl);
}
int32_t _Pthread_cond_t::notify_all() noexcept {
    return pthread_cond_broadcast(&impl);
}
int32_t _Pthread_cond_t::wait(_Pthread_mutex_t& mtx) {
    std::lock_guard lck{mtx};
    return pthread_cond_wait(&impl, mtx.native());
}
int32_t _Pthread_cond_t::wait_for(_Pthread_mutex_t& mtx,
                                  const timespec& until) {
    std::lock_guard lck{mtx};
    return pthread_cond_timedwait(&impl, mtx.native(), &until);
}

namespace std {

latch::latch(ptrdiff_t expected) noexcept(false)
    : counter{expected}, mtx{}, cv{} {
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

    if (const auto ec = cv.notify_all())
        throw system_error{ec, system_category(), "pthread_cond_broadcast"};
}

bool latch::try_wait() const noexcept {
    using namespace std::chrono;
    // if counter equals zero, returns immediately
    if (counter == 0)
        return true;

    // The sleep time is random-picked for ease of debugging
    const auto sleep_time = 1000ms;
    // pthread_cond_timedwait requires abs-time
    const auto end_time = system_clock::now().time_since_epoch() + sleep_time;
    const timespec until{
        .tv_sec = duration_cast<seconds>(end_time).count(),
        .tv_nsec = duration_cast<nanoseconds>(end_time).count() % 1'000'000,
    };
    if (const auto ec = cv.wait_for(mtx, until))
        errno = ec;
    return counter == 0;
}

void latch::wait() const noexcept(false) {
    // Since latch doesn't provide interface for waiting with timeout,
    // the code can wait more when the counter reached 0
    while (try_wait() == false) {
        if (errno == ETIMEDOUT) // this might be a spurious wakeup
            continue;
        throw system_error{errno, system_category(), "pthread_cond_timedwait"};
    }
}

} // namespace std