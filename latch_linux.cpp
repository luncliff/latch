#include "latch.h"

#include <atomic>
#include <climits>
#include <system_error>

// clang-format off
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <linux/futex.h>
// clang-format on

/**
 * @param target    On all platforms, futexes are four-byte integers
 * @param expected
 * @param timeout   Timeout for the kernel space blocking
 * @return int      `0` if the caller was woken up (this can be spurious wake-up). 
 *                  `errno` if something went wrong. (`EAGAIN` if under race, `EINTR` for spurious )
 * @link http://manpages.ubuntu.com/manpages/bionic/man2/futex.2.html
 */
int futex_wait_on_address(int32_t* target, int32_t expected, //
                          const struct timespec* timeout = nullptr) {
    if (syscall(SYS_futex, target, FUTEX_WAIT_PRIVATE, expected, timeout, NULL,
                0) != 0)
        return errno;
    return 0;
}

/**
 * This operation wakes at most val of the waiters that are waiting (e.g.,
 * inside FUTEX_WAIT)
 * @param target    On all platforms, futexes are four-byte integers
 * @param num_waiters The number of waiters to awake
 * @return int      `errno` after `syscall(SYS_futex, ...)`
 * @link http://manpages.ubuntu.com/manpages/bionic/man2/futex.2.html
 */
int futex_wake_by_address(int32_t* target, int32_t num_waiters = INT_MAX) {
    syscall(SYS_futex, target, FUTEX_WAKE, num_waiters, NULL, NULL, 0);
    return errno;
}

namespace std {

static_assert(is_copy_assignable_v<latch> == false);
static_assert(is_copy_constructible_v<latch> == false);

void latch::arrive_and_wait(ptrdiff_t update) noexcept(false) {
    this->count_down(update);
    this->wait();
}

void latch::count_down(ptrdiff_t update) noexcept(false) {
    atomic_int32_t* ctr = reinterpret_cast<atomic_int32_t*>(&counter);
    int32_t change = static_cast<int32_t>(update);

    if (ctr->load() < change)
        throw system_error{EINVAL, system_category(),
                           "update is greater than counter"};

    ctr->fetch_sub(change);
    if (ctr->load() != 0)
        return;

    if (const auto ec = futex_wake_by_address(reinterpret_cast<int32_t*>(ctr)))
        throw system_error{ec, system_category(), "futex_wake_by_address"};
}

bool latch::try_wait() const noexcept {
    atomic_int32_t* ctr =
        reinterpret_cast<atomic_int32_t*>(const_cast<ptrdiff_t*>(&counter));
    if (ctr->load() == 0)
        return true;

    const timespec timeout{.tv_sec = 3};
    futex_wait_on_address(reinterpret_cast<int32_t*>(ctr), ctr->load(),
                          &timeout);
    return counter == 0;
}

void latch::wait() const noexcept(false) {
    while (try_wait() == false) {
        /// @todo need implementation for spurious wakeup
        switch (errno) {
        case 0:
        case ETIMEDOUT: // timeout reached
        case EINTR:     // spurious wake up
        case EAGAIN:    // under race
            continue;   // ... try again
        default:
            throw system_error{errno, system_category(),
                               "futex_wait_on_address"};
        }
    }
}

} // namespace std
