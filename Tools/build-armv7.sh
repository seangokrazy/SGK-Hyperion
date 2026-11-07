#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build/armv7 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=Tools/armv7-toolchain.cmake -DBUILD_TESTING=OFF
cmake --build build/armv7 --parallel
