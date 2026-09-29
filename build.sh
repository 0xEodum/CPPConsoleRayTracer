#!/usr/bin/env sh
# Configure, build and test in ./build (Release). Extra arguments are passed to CMake.
set -e
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
echo "Done: ./build/raytracer"
