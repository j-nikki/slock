#include "threads.h"

#include <algorithm>

const unsigned int threads::nthr = std::max(1u, std::thread::hardware_concurrency() / 2);
