#pragma once
#include <windows.h>
#include <algorithm>
#include <memory>
#include "../scene/Scene.cpp"
#include "../console/ConsoleRenderer.cpp"
#include "../materials/Mirror.cpp"
#include "../materials/Absorber.cpp"
#include "../materials/Matte.cpp"
#include "../lights/PointLight.cpp"
#include "../lights/SpotLight.cpp"
#include "../lights/LaserLight.cpp"
#include "../materials/PrismMaterial.cpp"

class InputHandler {
private:
    Scene& scene;
    ConsoleRenderer& renderer;
    HANDLE hStdin;
    COORD currentMousePos;
    int selectedMaterial;

    std::unique_ptr<LaserLight> pendingLaser;
    bool isPlacingLaser;

    std::shared_ptr<Material> createMaterial() const {
        switch (selectedMaterial) {
        case 0: return std::make_shared<Mirror>();
        case 1: return std::make_shared<Absorber>();
        case 2: return std::make_shared<Matte>();
        case 3: return std::make_shared<PrismMaterial>();
        default: return std::make_shared<Matte>();
        }
    }

    void handleMouseEvent(const MOUSE_EVENT_RECORD& event) {
        currentMousePos = event.dwMousePosition;

        if (!scene.isInWorkingArea(currentMousePos.X, currentMousePos.Y)) {
            return;
        }

        bool needsUpdate = false;

        if (event.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
            auto rect = std::make_unique<ConsoleRectangle>(
                currentMousePos.X - 2,
                currentMousePos.Y - 1,
                5,
                3,
                true
            );
            rect->setMaterial(createMaterial());

            if (!scene.addShape(std::move(rect))) {
                Beep(1000, 100);
            }
            else {
                needsUpdate = true;
            }
        }
        else if (event.dwButtonState == RIGHTMOST_BUTTON_PRESSED) {
            if (scene.removeShape(currentMousePos.X, currentMousePos.Y)) {
                needsUpdate = true;
            }
        }

        if (needsUpdate) {
            renderer.render();
        }
    }

    void handleKeyEvent(const KEY_EVENT_RECORD& event) {
        if (!event.bKeyDown) return;

        bool needsUpdate = false;

        switch (event.wVirtualKeyCode) {
        case 'L':
        {
            auto light = std::make_unique<PointLight>(
                currentMousePos.X,
                currentMousePos.Y
            );
            scene.addLight(std::move(light));
            needsUpdate = true;
        }
        break;

        case 'S':
        {
            auto spot = std::make_unique<SpotLight>(
                currentMousePos.X,
                currentMousePos.Y,
                0.0f,
                M_PI / 4
            );
            scene.addLight(std::move(spot));
            needsUpdate = true;
        }
        break;

        case 'D':
            if (scene.removeLight(currentMousePos.X, currentMousePos.Y)) {
                needsUpdate = true;
            }
            break;

        case 'C':
            scene.clear();
            needsUpdate = true;
            break;

        case VK_OEM_PLUS:
        case VK_ADD:
            if (auto light = scene.getLightAt(currentMousePos.X, currentMousePos.Y)) {
                float intensity = light->getIntensity() + 0.1f;
                light->setIntensity(std::min<float>(1.0f, intensity));
                needsUpdate = true;
            }
            break;

        case VK_OEM_MINUS:
        case VK_SUBTRACT:
            if (auto light = scene.getLightAt(currentMousePos.X, currentMousePos.Y)) {
                float intensity = light->getIntensity() - 0.1f;
                light->setIntensity(std::max<float>(0.0f, intensity));
                needsUpdate = true;
            }
            break;

        case VK_UP:
            selectedMaterial = (selectedMaterial - 1 + 4) % 4;
            scene.setSelectedMaterial(selectedMaterial);
            needsUpdate = true;
            break;

        case VK_DOWN:
            selectedMaterial = (selectedMaterial + 1) % 4;
            scene.setSelectedMaterial(selectedMaterial);
            needsUpdate = true;
            break;

        case 'Z':
            if (!isPlacingLaser) {
                pendingLaser = std::make_unique<LaserLight>(
                    currentMousePos.X,
                    currentMousePos.Y
                );
                isPlacingLaser = true;
                scene.addLight(pendingLaser->clone());
                needsUpdate = true;
            }
            else {
                if (auto laser = dynamic_cast<LaserLight*>(
                    scene.getLightAt(pendingLaser->getX(), pendingLaser->getY()))) {
                    laser->setTarget(currentMousePos.X, currentMousePos.Y);
                }
                isPlacingLaser = false;
                pendingLaser.reset();
                needsUpdate = true;
            }
            break;
        }

        if (needsUpdate) {
            renderer.render();
        }
    }

public:
    InputHandler(Scene& scene, ConsoleRenderer& renderer, HANDLE hStdIn)
        : scene(scene)
        , renderer(renderer)
        , hStdin(hStdIn)
        , selectedMaterial(0)
        , currentMousePos{ 0, 0 }
        , isPlacingLaser(false)
    {
        DWORD prevMode;
        GetConsoleMode(hStdin, &prevMode);
        SetConsoleMode(hStdin, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT |
            ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT);

        renderer.updateMaterialSelection(selectedMaterial);
    }

    void processInput() {
        INPUT_RECORD inputRecord;
        DWORD numEvents;

        if (ReadConsoleInput(hStdin, &inputRecord, 1, &numEvents)) {
            switch (inputRecord.EventType) {
            case MOUSE_EVENT:
                handleMouseEvent(inputRecord.Event.MouseEvent);
                break;
            case KEY_EVENT:
                handleKeyEvent(inputRecord.Event.KeyEvent);
                break;
            }
        }
    }

    COORD getCurrentMousePos() const {
        return currentMousePos;
    }
};