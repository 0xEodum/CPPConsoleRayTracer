@echo off
rem Configure, build and test in .\build (Release). Extra arguments are passed to CMake.
cmake -S . -B build %*
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 exit /b 1
echo Done: build\Release\raytracer.exe (or build\raytracer.exe for single-config generators)
