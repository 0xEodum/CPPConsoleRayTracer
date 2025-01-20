#pragma once
#include <windows.h>
#include <vector>
#include "../core/Color.cpp"

class ConsoleBuffer {
private:
    struct PixelInfo {
        wchar_t symbol;
        Color color;
        bool modified;

        PixelInfo() : symbol(L' '), modified(false) {}
    };

    std::vector<std::vector<PixelInfo>> buffer;
    HANDLE console;
    SHORT width, height;

public:
    ConsoleBuffer(HANDLE console, SHORT width, SHORT height)
        : console(console), width(width), height(height) {
        buffer.resize(height, std::vector<PixelInfo>(width));
    }

    void clear() {
        for (auto& row : buffer) {
            for (auto& pixel : row) {
                pixel.symbol = L' ';
                pixel.modified = false;
            }
        }
    }

    void setPixel(SHORT x, SHORT y, const Color& color, wchar_t symbol) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            auto& pixel = buffer[y][x];
            pixel.symbol = symbol;
            pixel.color = color;
            pixel.modified = true;
        }
    }

    void flush() {
        for (SHORT y = 0; y < height; y++) {
            SHORT startX = -1;
            std::wstring coloredString;

            for (SHORT x = 0; x < width; x++) {
                if (buffer[y][x].modified) {
                    if (startX == -1) {
                        startX = x;
                        SetConsoleCursorPosition(console, { x, y });
                    }

                    coloredString += buffer[y][x].color.getColorCode(false) +
                        buffer[y][x].symbol +
                        L"\x1b[0m";

                    buffer[y][x].modified = false;
                }
                else if (startX != -1) {
                    DWORD written;
                    WriteConsoleW(console, coloredString.c_str(), coloredString.length(), &written, nullptr);
                    coloredString.clear();
                    startX = -1;
                }
            }

            if (!coloredString.empty()) {
                DWORD written;
                WriteConsoleW(console, coloredString.c_str(), coloredString.length(), &written, nullptr);
            }
        }
    }
};