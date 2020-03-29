#include "latch.h"

#include <atomic>
#include <chrono>
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

static_assert(sizeof(ptrdiff_t) == sizeof(LONG) ||
              sizeof(ptrdiff_t) == sizeof(LONG64));

void latch::count_down(ptrdiff_t update) noexcept(false) {
    if (counter < update)
        throw system_error{EINVAL, system_category(),
                           "update is greater than counter"};
    // if not lock-free, rely on InterLocked operation
    if constexpr (atomic<ptrdiff_t>::is_always_lock_free) {
        counter -= update;
    } else if constexpr (sizeof(ptrdiff_t) == sizeof(LONG)) {
        InterlockedAdd(reinterpret_cast<LONG*>(&counter),
                       static_cast<LONG>(-update));
    } else if constexpr (sizeof(ptrdiff_t) == sizeof(LONG64)) {
        InterlockedAdd64(reinterpret_cast<LONG64*>(&counter),
                         static_cast<LONG64>(-update));
    }
    // counter reached zero
    if (counter == 0)
        WakeByAddressAll(&counter);
}

DWORD _Current_timeout_ms = INFINITE;

void latch::_Set_timeout(unsigned ns) noexcept {
    const auto ms =
        duration_cast<chrono::milliseconds>(chrono::nanoseconds{ns});
    _Current_timeout_ms = static_cast<DWORD>(ms.count());
}
unsigned latch::_Get_timeout() noexcept {
    return static_cast<unsigned>(_Current_timeout_ms);
}

bool latch::try_wait() const noexcept {
    // if counter equals zero, returns immediately
    if (counter == 0)
        return true;
    // blocks on `*this` until a call to count_down that decrements counter to zero
    ptrdiff_t captured = counter;
    if (WaitOnAddress(const_cast<ptrdiff_t*>(&counter), &captured,
                      sizeof(ptrdiff_t), _Current_timeout_ms))
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
