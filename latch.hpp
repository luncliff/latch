#pragma once
#include <cstdint>

namespace std {

    class latch {
    public:
        /*constexpr*/ explicit latch(ptrdiff_t expected) noexcept(false);
        ~latch();

        latch(const latch&) = delete;
        latch& operator=(const latch&) = delete;

        void count_down(ptrdiff_t update = 1) noexcept(false);
        bool try_wait() const noexcept;
        void wait() const noexcept(false);
        void arrive_and_wait(ptrdiff_t update = 1) noexcept(false);

    private:
        ptrdiff_t counter;
        void* handle;
    };

} // namespace std
