#include "latch.h"

#include <atomic>
#include <system_error>
#include <type_traits>
// clang-format off

#include <Windows.h>
#include <synchapi.h>
// clang-format on

namespace std {

static_assert(is_copy_assignable_v<latch> == false);
static_assert(is_copy_constructible_v<latch> == false);

void latch::arrive_and_wait(ptrdiff_t update) noexcept(false) {
    this->count_down(update);
    this->wait();
}

void latch::count_down(ptrdiff_t update) noexcept(false) {
    static_assert(is_same_v<ptrdiff_t, LONG64> || is_same_v<ptrdiff_t, LONG>);
    if (counter < update)
        throw system_error{EINVAL, system_category(),
                           "update is greater than counter"};
    // if not lock-free, rely on InterLocked operation
    if constexpr (atomic<ptrdiff_t>::is_always_lock_free) {
        counter -= update;
    } else if constexpr (is_same_v<ptrdiff_t, LONG>) {
        InterlockedAdd(reinterpret_cast<LONG*>(&counter),
                       static_cast<LONG>(-update));
    } else if constexpr (is_same_v<ptrdiff_t, LONG64>) {
        InterlockedAdd64(reinterpret_cast<LONG64*>(&counter),
                         static_cast<LONG64>(-update));
    }
    // counter reached zero
    if (counter == 0)
        WakeByAddressAll(&counter);
}

bool latch::try_wait() const noexcept {
    // if counter equals zero, returns immediately
    if (counter == 0)
        return true;
    // blocks on `*this` until a call to count_down that decrements counter to zero
    ptrdiff_t captured = counter;
    if (WaitOnAddress(const_cast<ptrdiff_t*>(&counter), &captured,
                      sizeof(ptrdiff_t), INFINITE))
        return counter == 0;
    // caller can check `GetLastError` for this case
    return false;
}

void latch::wait() const noexcept(false) {
    while (try_wait() == false) {
        // case: error from WaitOnAddress
        if (const auto ec = GetLastError())
            throw system_error{static_cast<int>(ec), system_category(),
                               "WaitOnAddress"};
        // case: counter != 0. retry
        // ...
    }
}

} // namespace std
