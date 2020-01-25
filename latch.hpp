#pragma once
#include <cstddef>

namespace std {
    /**
     * @defgroup thread.coord
     * Concepts realted to thread coordination, and defines the coordination types `latch` and `barrier`.
     * These types facilitate concurrent computation performed by a number of threads.
     */

    /**
     * @brief A coordination mechanism that allows any number of threads to block
     * until an expected number of threads arrive at the latch
     * @ingroup thread.coord
     * @see N4835, 1571~1572 p
     * @details
     * A `latch` is a thread coordination mechanism that allows any number of threads to block until an expected number
     * of threads arrive at the `latch` (via the `count_down` function).
     * The expected count is set when the `latch` is created.
     * An individual `latch` is a single-use object;
     * once the expected count has been reached, the `latch` cannot be reused.
     */
    class latch {
    public:
        /**
         * @brief   Initialize `counter` with `expected`
         * @param   expected
         * @pre     `expected >= 0` is true
         */
        constexpr explicit latch(ptrdiff_t expected) noexcept : counter{expected} {}
        /**
         * @details
         * Concurrent invocations of the member functions of `latch` other than its destructor,
         * do not introduce data races
         */
        ~latch() = default;

        latch(const latch&) = delete;
        latch& operator=(const latch&) = delete;

        /**
         * @param   update
         * @pre     `update >= 0` is true, and `update <= counter` is true
         * @post    Atomically decreses `counter` by `update`.
         *          If `counter` is equal to zero, unblocks all threads blocked on `*this`
         * @throw   system_error
         * @details
         * Synchronization:
         * Strongly happens before the returns from all calls that are unblocked.
         *
         * Error Conditions:
         * Any of the error conditions allowed for mutex types (32.5.3.2)
         */
        void count_down(ptrdiff_t update = 1) noexcept(false);
        bool try_wait() const noexcept;
        void wait() const noexcept(false);
        void arrive_and_wait(ptrdiff_t update = 1) noexcept(false);

    private:
        /**
         * @brief A latch maintains an internal counter that is initialized when the latch is created
         * @details Threads can block on the latch object, waiting for counter to be decremented to zero.
         */
        ptrdiff_t counter;
    };
} // namespace std
