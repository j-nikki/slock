# slock
Simple screen locker utility for X.

Forked from https://git.suckless.org/slock
<br>Based on patch https://tools.suckless.org/slock/patches/blur-pixelated-screen/

## Requirements

You need GCC 12+, CMake 3.20+, and the X extensions XShm, Xinerama, Xrandr.

## Installation

- install:<br>`cmake -B build && sudo cmake --install build`
<br>*note: default prefix (`/usr/local`) can be overridden with `--prefix <dir>`*
- uninstall:<br>`cat build/install_manifest.txt | sudo xargs rm`

You might need to modify `config.h` according to your setup.

## Usage

Run `slock` to lock screen. Type your password and `<ENTER>` to unlock.
