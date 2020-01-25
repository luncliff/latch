#include "latch.hpp"

// clang-format off
#include <system_error>

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/time.h>
// clang-format on

/**
 * @link http://manpages.ubuntu.com/manpages/bionic/man2/futex.2.html
 */
int futex_wait_on_address(ptrdiff_t* target, ptrdiff_t value, const struct timespec *timeout)
{
    // EAGAIN?
    return syscall(SYS_futex, target, FUTEX_WAIT_PRIVATE, value,
                   timeout, NULL, 0);
}

/**
 * @link http://manpages.ubuntu.com/manpages/bionic/man2/futex.2.html
 */
int futex_wake_by_address(ptrdiff_t* target)
{
    return syscall(SYS_futex, target, FUTEX_WAKE, 1,
                       NULL, NULL, 0);
}

namespace std {

    static_assert(is_copy_assignable_v<latch> == false);
    static_assert(is_copy_constructible_v<latch> == false);

    void latch::arrive_and_wait(ptrdiff_t update) noexcept(false) {
        this->count_down(update);
        this->wait();
    }

    void latch::count_down(ptrdiff_t update) noexcept(false) {
        // On all platforms, futexes are four-byte integers 
        // that must be aligned on a four-byte boundary
        static_assert(is_same_v<ptrdiff_t, int64_t> || is_same_v<ptrdiff_t, int32_t>);
        if (counter < update)
            throw system_error{EINVAL, system_category(), "update is greater than counter"};
        /// @todo perform atomic operation
        counter -= update;
        /// @todo need implementation of wake
        if(counter == 0)
            if(futex_wake_by_address(&counter)!=0)
                throw system_error{errno, system_category(), "futex_wake_by_address"};
    }

    bool latch::try_wait() const noexcept {
        if (counter == 0)
            return true;

        /// @todo need implementation of wait
        if(futex_wait_on_address(const_cast<ptrdiff_t*>(&counter), 0, nullptr))
            return counter == 0;
        return false;
    }

    void latch::wait() const noexcept(false) {
        while (try_wait() == false) {
            /// @todo need implementation for spurious wakeup
            if (errno)
                throw system_error{errno, system_category(), "futex_wait_on_address"};
        }
    }

}
