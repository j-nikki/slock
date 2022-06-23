#pragma once

#include <chrono>
#include <source_location>

class timer
{
  public:
    timer(bool enabled = true) : enabled_{enabled} {}
    void finish()
    {
        if (!enabled_) [[likely]]
            return;
        const auto dur = (std::chrono::high_resolution_clock::now() - start_).count() / 1000000.;
        printf("%17.3fms %s\n%s", dur, std::source_location::current().file_name(), buf_);
        dit_ = buf_;
    }
    void enable(bool enable = true) { enabled_ = enable; }
    ~timer()
    {
        if (dit_ != buf_) finish();
    }
    auto operator()(const char *name, auto &&f,
                    std::source_location sl = std::source_location::current()) -> decltype(f())
    {
        if constexpr (std::is_void_v<decltype(f())>) {
            return (void)operator()(
                name, [&] { return f(), 0; }, sl);
        } else {
            const auto start   = std::chrono::high_resolution_clock::now();
            decltype(auto) res = f();
            if (!enabled_) [[likely]]
                return res;
            const auto dur = (std::chrono::high_resolution_clock::now() - start).count() / 1000000.;
            dit_ += sprintf(dit_, "%17.3fms %s:%d:%d (%s)\n", dur, sl.file_name(), sl.line(),
                            sl.column(), name ? name : sl.function_name());
            return res;
        }
    }

  private:
    char buf_[2048], *dit_ = buf_;
    const std::chrono::high_resolution_clock::time_point start_ =
        std::chrono::high_resolution_clock::now();
    bool enabled_;
};
