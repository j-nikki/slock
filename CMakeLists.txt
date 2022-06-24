cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_FLAGS "-Wpedantic -Wall -Wextra" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-march=native -Ofast -fno-exceptions -DNDEBUG" CACHE STRING "" FORCE)

project(slock)

set(SLOCK_VERSION 1.4)

add_subdirectory(src)
