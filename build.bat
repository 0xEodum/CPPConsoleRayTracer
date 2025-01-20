@echo off
setlocal enabledelayedexpansion

if not exist "bin" mkdir bin

set CFLAGS=-std=c++17 -O2 -Wall -D_USE_MATH_DEFINES

set INCLUDES=-I./include -I./src

g++ %CFLAGS% %INCLUDES% ^
    src/core/Vector2D.cpp ^
    src/core/Color.cpp ^
    src/core/Spectrum.cpp ^
    src/materials/Material.cpp ^
    src/materials/Absorber.cpp ^
    src/materials/Mirror.cpp ^
    src/materials/Matte.cpp ^
    src/materials/PrismMaterial.cpp ^
    src/lights/LightSource.cpp ^
    src/lights/PointLight.cpp ^
    src/lights/SpotLight.cpp ^
    src/lights/LaserLight.cpp ^
    src/console/ConsoleBuffer.cpp ^
    src/console/ConsoleRenderer.cpp ^
    src/console/InputHandler.cpp ^
    src/shapes/ConsoleShape.cpp ^
    src/shapes/ConsoleRectangle.cpp ^
    src/shapes/ConsolePrism.cpp ^
    src/utils/Logger.cpp ^
    src/scene/Scene.cpp ^
    RayTracingConsole.cpp ^
    -o bin/RayTracer.exe

if %ERRORLEVEL% == 0 (
    echo Build successful! Executable created at bin/RayTracer.exe
) else (
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

exit /b 0