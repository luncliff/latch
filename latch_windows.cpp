#include <atomic>
#include <latch.hpp>
#include <system_error>
#include <type_traits>

// clang-format off
#include <Windows.h>
#include <synchapi.h>
// clang-format on

namespace std {

    static_assert(is_copy_assignable_v<latch> == false);
    static_assert(is_copy_constructible_v<latch> == false);

    latch::latch(ptrdiff_t expected) noexcept(false) : counter{expected}, handle{nullptr} {
        if (expected < 0)
            throw invalid_argument{"expected value should be zero or positive"};

        handle = CreateEventExW(nullptr, nullptr,
            // if expected == 0, start with signaled
            expected == 0 ? CREATE_EVENT_INITIAL_SET : CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE);
        if (handle == NULL)
            throw system_error{static_cast<int>(GetLastError()), system_category(), "CreateEventEx"};
    }

    latch::~latch() {
        CloseHandle(handle);
    }

    void latch::arrive_and_wait(ptrdiff_t update) noexcept(false) {
        this->count_down(update);
        this->wait();
    }

    void latch::count_down(ptrdiff_t update) noexcept(false) {
        if (counter < update)
            throw invalid_argument{"latch's counter can't be negative"};

        if constexpr (std::atomic<ptrdiff_t>::is_always_lock_free) {
            counter -= update;
        } else {
            std::atomic_fetch_sub(reinterpret_cast<std::atomic_ptrdiff_t*>(&counter), update);
        }
        if (counter == 0)
            SetEvent(handle);
    }

    bool latch::try_wait() const noexcept {
        // allow interrupt by APC or I/O works
        switch (WaitForSingleObjectEx(handle, INFINITE, TRUE)) {
        case WAIT_OBJECT_0:
            return counter == 0;
        case WAIT_TIMEOUT: // won't happen
        case WAIT_ABANDONED: // not for the event type handle
        case WAIT_IO_COMPLETION: // expected
        case WAIT_FAILED:
        default:
            return false;
        }
    }

    void latch::wait() const noexcept(false) {
        while (try_wait() == false)
            throw system_error{static_cast<int>(GetLastError()), system_category(), "WaitForSingleObjectEx"};
    }

}
