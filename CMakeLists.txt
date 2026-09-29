cmake_minimum_required(VERSION 3.16)
project(ConsoleRayTracer VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CRT_BUILD_TESTS "Build the unit tests" ON)
option(CRT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CRT_NATIVE "Optimise for the build machine's CPU (-march=native)" OFF)

find_package(Threads REQUIRED)

# ------------------------------------------------------------------------------------------
# Shared compiler settings
# ------------------------------------------------------------------------------------------
add_library(crt_options INTERFACE)
target_compile_definitions(crt_options INTERFACE CRT_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(crt_options INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
    target_compile_definitions(crt_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    if(CRT_WARNINGS_AS_ERRORS)
        target_compile_options(crt_options INTERFACE /WX)
    endif()
else()
    target_compile_options(crt_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    if(CRT_WARNINGS_AS_ERRORS)
        target_compile_options(crt_options INTERFACE -Werror)
    endif()
    if(CRT_NATIVE)
        target_compile_options(crt_options INTERFACE -march=native)
    endif()
endif()
if(MINGW)
    # Self-contained executables: no libstdc++ / winpthread DLLs needed next to them.
    target_link_options(crt_options INTERFACE -static)
endif()

# ------------------------------------------------------------------------------------------
# Engine library: everything except main(), so the tests can link against it
# ------------------------------------------------------------------------------------------
add_library(crt_engine STATIC
    src/core/Image.cpp
    src/core/Spectrum.cpp
    src/core/ThreadPool.cpp

    src/optics/Light.cpp
    src/optics/LightTracer.cpp
    src/optics/Material.cpp
    src/optics/Overlay.cpp
    src/optics/Presets.cpp
    src/optics/Scene.cpp
    src/optics/Shape.cpp

    src/rt/Bvh.cpp
    src/rt/Camera.cpp
    src/rt/PathTracer.cpp
    src/rt/Presets.cpp
    src/rt/Primitive.cpp
    src/rt/Scene.cpp

    src/term/Canvas.cpp
    src/term/InputParser.cpp
    src/term/Presenter.cpp
    src/term/TerminalPosix.cpp
    src/term/TerminalWindows.cpp

    src/ui/Widgets.cpp

    src/app/App.cpp
    src/app/Cli.cpp
    src/app/Files.cpp
    src/app/OpticsMode.cpp
    src/app/PathTraceMode.cpp
)
target_include_directories(crt_engine PUBLIC src)
target_link_libraries(crt_engine PUBLIC Threads::Threads PRIVATE crt_options)

add_executable(raytracer src/main.cpp)
target_link_libraries(raytracer PRIVATE crt_engine crt_options)

install(TARGETS raytracer RUNTIME DESTINATION bin)

if(CRT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
