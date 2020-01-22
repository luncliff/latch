#include <atomic>
#include <latch.hpp>
#include <synchapi.h>
#include <system_error>
#include <type_traits>

#include <Windows.h>

namespace std {

    static_assert(is_copy_assignable_v<latch> == false);
    static_assert(is_copy_constructible_v<latch> == false);

    latch::latch(ptrdiff_t delta) noexcept(false) {
        HANDLE ev = CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
        if (ev == NULL)
            throw system_error{GetLastError(), system_category(), "CreateEventEx"};

        std::atomic_uint32_t ref = delta;
    }

    latch::~latch() {
        // CloseHandle(ev);
    }

    void latch::count_down_and_wait() noexcept(false) {
        this->count_down();
        this->wait();
    }

    void latch::count_down(uint32_t n) noexcept(false) {

        if (ref.load(memory_order_acquire) < n)
            throw underflow_error{"latch's count can't be negative"};

        ref.fetch_sub(n, memory_order_release);
        if (ref.load(memory_order_acquire) == 0)
            SetEvent(ev);
    }

    // GSL_SUPPRESS(f .23)
    // GSL_SUPPRESS(con .4)
    bool latch::is_ready() const noexcept {
        if (ref.load(memory_order_acquire) > 0)
            return false;

        if (ev == INVALID_HANDLE_VALUE)
            return true;

        // if it is not closed, test it
        auto ec = WaitForSingleObjectEx(ev, 0, TRUE);
        if (ec == WAIT_OBJECT_0) {
            // WAIT_OBJECT_0 : return by signal
            this->~latch();
            return true;
        }
        return false;
    }

    // GSL_SUPPRESS(es .76)
    void latch::wait() noexcept(false) {
        static_assert(WAIT_OBJECT_0 == 0);

    StartWait:
        if (this->is_ready())
            return;

        // standard interface doesn't define timed wait.
        // This makes APC available. expecially for Overlapped I/O
        if (const auto ec = WaitForSingleObjectEx(ev, 1536, TRUE)) {
            // WAIT_IO_COMPLETION : return because of APC
            if (ec == WAIT_IO_COMPLETION)
                goto StartWait;
            // WAIT_TIMEOUT	: this is expected. try again
            if (ec == WAIT_TIMEOUT)
                goto StartWait;

            // WAIT_FAILED	: use GetLastError in the case
            // WAIT_ABANDONED
            throw system_error{gsl::narrow_cast<int>(GetLastError()), system_category(), "WaitForSingleObjectEx"};
        }
        // WAIT_OBJECT_0 : return by signal
        this->~latch();
        return;
    }

}
