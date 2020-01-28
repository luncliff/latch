#pragma once
#if !defined(__APPLE__)
#error "this header is only for Darwin(macOS) platform"
#endif

#include <atomic>
#include <pthread.h>

class _Pthread_mutex_t final {
    pthread_mutex_t impl;

  private:
    _Pthread_mutex_t(const _Pthread_mutex_t&) = delete;
    _Pthread_mutex_t& operator=(const _Pthread_mutex_t&) = delete;
    _Pthread_mutex_t(_Pthread_mutex_t&&) = delete;
    _Pthread_mutex_t& operator=(_Pthread_mutex_t&&) = delete;

  public:
    _Pthread_mutex_t() noexcept(false);
    ~_Pthread_mutex_t() noexcept(false);

    pthread_mutex_t* native() noexcept {
        return &impl;
    }
    void lock() noexcept(false);
    void unlock() noexcept(false);
    bool try_lock() noexcept;
};

class _Pthread_cond_t final {
    pthread_cond_t impl;

  private:
    _Pthread_cond_t(const _Pthread_cond_t&) = delete;
    _Pthread_cond_t& operator=(const _Pthread_cond_t&) = delete;
    _Pthread_cond_t(_Pthread_cond_t&&) = delete;
    _Pthread_cond_t& operator=(_Pthread_cond_t&&) = delete;

  public:
    _Pthread_cond_t() noexcept(false);
    ~_Pthread_cond_t() noexcept(false);

    int32_t notify_one() noexcept;
    int32_t notify_all() noexcept;
    int32_t wait(_Pthread_mutex_t& lock);
    int32_t wait_for(_Pthread_mutex_t& lock, const timespec& until);
};

namespace std {

class latch {
  public:
    static constexpr ptrdiff_t max() noexcept {
        return 32;
    }

  public:
    explicit latch(ptrdiff_t expected) noexcept(false);
    ~latch() noexcept(false) = default;

    latch(const latch&) = delete;
    latch& operator=(const latch&) = delete;

    void count_down(ptrdiff_t update = 1) noexcept(false);
    bool try_wait() const noexcept;
    void wait() const noexcept(false);
    void arrive_and_wait(ptrdiff_t update = 1) noexcept(false);

  private:
    /// @todo member alignment

    std::atomic<ptrdiff_t> counter;
    mutable _Pthread_mutex_t mtx;
    mutable _Pthread_cond_t cv;
};
static_assert(sizeof(latch) <= 128, "the expected size is near 120");

} // namespace std
