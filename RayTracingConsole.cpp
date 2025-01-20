#include <windows.h>
#include <iostream>
#include "src/scene/Scene.cpp"
#include "src/console/ConsoleRenderer.cpp"
#include "src/console/InputHandler.cpp"
#include "src/utils/Logger.hpp"


void setupConsole(HANDLE& hConsole, HANDLE& hStdin) {

    HWND consoleWindow = GetConsoleWindow();

    LONG style = GetWindowLong(consoleWindow, GWL_STYLE);
    style = style & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX;
    SetWindowLong(consoleWindow, GWL_STYLE, style);

    COORD bufferSize = { 120, 50 };
    SetConsoleScreenBufferSize(hConsole, bufferSize);

    SMALL_RECT windowSize = { 0, 0, 119, 49 };
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);

    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 8;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

int main() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

    if (hConsole == INVALID_HANDLE_VALUE || hStdin == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get console handles\n";
        return 1;
    }

    try {
        setupConsole(hConsole, hStdin);

        Scene scene(5, 3, 80, 40);
        ConsoleRenderer renderer(hConsole, scene);
        InputHandler input(scene, renderer, hStdin);

        renderer.render();

        bool running = true;
        while (running) {
            input.processInput();

            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                running = false;
            }
        }
        Logger::cleanup();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        Logger::cleanup();
        return 1;
    }
}