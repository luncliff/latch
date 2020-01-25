#include "latch.hpp"

// clang-format off
#include <system_error>

#include <linux/futex.h>
#include <sys/time.h>
// clang-format on

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

        /// @todo need implementation of wake
    }

    bool latch::try_wait() const noexcept {
        if (counter == 0)
            return true;

        /// @todo need implementation of wait
        return false;
    }

    void latch::wait() const noexcept(false) {
        while (try_wait() == false) {
            /// @todo need implementation for spurious wakeup
            if (errno)
                throw system_error{static_cast<int>(errno), system_category(), "WaitOnAddress"};
        }
    }

}
