#pragma once

#include <thread>

const int nthr = std::max(1u, std::thread::hardware_concurrency() / 2);
struct threads {
    alignas(std::jthread) char b_[sizeof(std::jthread) * 64];
    std::jthread *begin() { return reinterpret_cast<std::jthread *>(b_); }
    std::jthread *end() { return begin() + nthr; }
    threads(auto &&f)
    {
        for (int i = 0; i < nthr - 1; ++i)
            new (begin() + i) std::jthread{f, i};
        f(nthr - 1);
    }
    ~threads() { std::destroy_n(begin(), nthr - 1); }
};
