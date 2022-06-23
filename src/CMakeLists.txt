cmake_minimum_required(VERSION 3.20)

add_custom_command(
    OUTPUT ../config.h
    COMMAND sh -c '[ -f ../config.h ] || cp config.def.h ../config.h'
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src")

add_library(ebz explicit_bzero.c)
add_library(image image.c)
target_link_options(image PRIVATE -L/usr/X11R6/lib -lX11)

add_executable(slock slock.cpp ../config.h)
target_link_libraries(slock ebz image)
target_compile_options(slock PUBLIC -std=gnu++2b
    -D_DEFAULT_SOURCE -DVERSION=${SLOCK_VERSION}
    -march=native -Ofast -fwhole-program -fno-exceptions
    -Wpedantic -Wall -Wextra)
target_link_options(slock PRIVATE -s -L/usr/lib -lc -lcrypt -L/usr/X11R6/lib -lX11 -lXinerama -lXft -lXext -lXrandr)

install(
    FILES "${PROJECT_BINARY_DIR}/src/slock"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE SETUID SETGID
    DESTINATION bin)
