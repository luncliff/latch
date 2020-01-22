#pragma once

#include <cstdint>

namespace std {
    class latch {
    public:
        constexpr explicit latch(ptrdiff_t expected);
        ~latch();

        latch(const latch&) = delete;
        latch& operator=(const latch&) = delete;

        void count_down(ptrdiff_t update = 1);
        bool try_wait() const noexcept;
        void wait() const;
        void arrive_and_wait(ptrdiff_t update = 1);

    private:
        ptrdiff_t counter;
        // std::atomic_uint64_t ref{};
        // pthread_cond_t cv{};
        // pthread_mutex_t mtx{};
        };
} // namespace std
