#pragma once

#include <thread>
#include <memory>

struct threads {
    static const unsigned int nthr;
    alignas(std::jthread) char b_[sizeof(std::jthread) * 64];
    inline std::jthread *begin() { return reinterpret_cast<std::jthread *>(b_); }
    inline std::jthread *end() { return begin() + nthr; }
    inline threads(auto &&f)
    {
        for (auto i = 0u; i < nthr - 1; ++i)
            new (begin() + i) std::jthread{f, i};
        f(nthr - 1);
    }
    inline ~threads() { std::destroy_n(begin(), nthr - 1); }
};
