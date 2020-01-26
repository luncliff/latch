#pragma once
#include <cstddef>

#if !defined(__APPLE__)
#error "this header is only for Darwin(macOS) platform"
#endif

#include <atomic>
#include <pthread.h>

namespace std {
class latch {
  public:
    latch(ptrdiff_t expected) noexcept(false);
    ~latch() noexcept(false);

    latch(const latch&) = delete;
    latch& operator=(const latch&) = delete;

    void count_down(ptrdiff_t update = 1) noexcept(false);
    bool try_wait() const noexcept;
    void wait() const noexcept(false);
    void arrive_and_wait(ptrdiff_t update = 1) noexcept(false);

  private:
    std::atomic<ptrdiff_t> counter;
    mutable pthread_cond_t cv;
    mutable pthread_mutex_t mtx;
};
} // namespace std
