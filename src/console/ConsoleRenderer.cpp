#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "../scene/Scene.cpp"
#include "../lights/LaserLight.cpp"
#include "../materials/PrismMaterial.cpp"
#include "../console/ConsoleBuffer.cpp"

class ConsoleRenderer {
private:
    HANDLE hConsole;
    COORD consoleSize;
    SHORT materialListY;
    ConsoleBuffer buffer;

    struct ControlPanel {
        SHORT x;
        SHORT y;
        SHORT width;
        SHORT height;
    } panel;

    void clearArea(SHORT x, SHORT y, SHORT width, SHORT height) {
        for (SHORT dy = 0; dy < height; ++dy) {
            for (SHORT dx = 0; dx < width; ++dx) {
                buffer.setPixel(x + dx, y + dy, Color::Black(), L' ');
            }
        }
    }

    void drawString(SHORT x, SHORT y, const std::wstring& str, const Color& color = Color::White()) {
        for (size_t i = 0; i < str.length(); ++i) {
            buffer.setPixel(x + i, y, color, str[i]);
        }
    }

    void drawControlPanel() {
        std::wstring topBorder = L"┌─────────────────────────┐";
        std::wstring bottomBorder = L"└─────────────────────────┘";
        std::wstring emptyLine = L"│                         │";

        drawString(panel.x, panel.y, topBorder);
        for (SHORT y = panel.y + 1; y < panel.y + panel.height - 1; ++y) {
            drawString(panel.x, y, emptyLine);
        }
        drawString(panel.x, panel.y + panel.height - 1, bottomBorder);

        drawString(panel.x + 8, panel.y + 1, L"Controls:");

        std::vector<std::wstring> controls = {
            L"LMC - Rectangle",
            L"RMC - delete",
            L"L   - point light",
            L"D   - delete light",
            L"S   - spotlight",
            L"+/- - intensity",
            L"C   - clear scene",
        };

        SHORT currentY = panel.y + 3;
        for (const auto& control : controls) {
            drawString(panel.x + 3, currentY++, control);
        }

        currentY++;
        drawString(panel.x + 8, currentY++, L"Material:");

        materialListY = currentY;
    }

    void drawRay(const Vector2D& startPos, const LightSource::Ray& ray, float baseIntensity) {
        if (baseIntensity < 0.1f) return;

        struct RaySegment {
            Vector2D position;
            LightSource::Ray ray;
            float intensity;
            float totalDistance;
            int bounces;

            RaySegment(const Vector2D& pos, const LightSource::Ray& r, float i)
                : position(pos), ray(r), intensity(i), totalDistance(0), bounces(0) {
            }
        };

        std::vector<RaySegment> activeRays = {
            RaySegment(startPos, ray, baseIntensity)
        };
        std::vector<RaySegment> nextRays;
        const int maxReflections = 5;

        float maxLength = 50.0f;
        for (const auto& light : scene.getLights()) {
            if (auto laser = dynamic_cast<const LaserLight*>(light.get())) {
                if ((laser->getPosition() - startPos).lengthSquared() < 0.01f) {
                    maxLength = 300.0f;
                    break;
                }
            }
        }

        while (!activeRays.empty()) {
            nextRays.clear();

            for (const auto& ray : activeRays) {
                if (ray.bounces >= maxReflections || ray.intensity < 0.1f) continue;

                ConsoleShape::RayHit nearestHit;
                const ConsoleShape* hitShape = nullptr;

                for (const auto& border : scene.getBorders()) {
                    auto hit = border->intersectRay(ray.position, ray.ray.direction);
                    if (hit.hit && hit.distance < nearestHit.distance) {
                        nearestHit = hit;
                        hitShape = border.get();
                    }
                }

                for (const auto& shape : scene.getShapes()) {
                    auto hit = shape->intersectRay(ray.position, ray.ray.direction);
                    if (hit.hit && hit.distance < nearestHit.distance) {
                        nearestHit = hit;
                        hitShape = shape.get();
                    }
                }

                float rayLength = nearestHit.hit ? nearestHit.distance : maxLength;
                float step = 0.1f;

                for (float t = 0; t <= rayLength; t += step) {
                    Vector2D currentPos = ray.position + ray.ray.direction * t;
                    SHORT x, y;
                    currentPos.toConsoleCoords(x, y);

                    if (!scene.isInWorkingArea(x, y)) break;

                    float totalDistance = ray.totalDistance + t;
                    float distanceAttenuation = std::max<float>(0.0f,
                        std::min<float>(1.0f, 1.0f - (totalDistance / maxLength)));

                    if (distanceAttenuation <= 0.1f) break;

                    float currentIntensity = ray.intensity * distanceAttenuation;
                    Color rayColor = ray.ray.spectrum.toRGB() * currentIntensity;
                    buffer.setPixel(x, y, rayColor, L'•');
                }

                if (nearestHit.hit && hitShape) {
                    Vector2D normal = hitShape->getNormalAt(nearestHit.position);
                    float totalDistance = ray.totalDistance + nearestHit.distance;
                    float hitAttenuation = std::max<float>(0.0f,
                        std::min<float>(1.0f, 1.0f - (totalDistance / maxLength)));

                    Material::IntensityInfo intensity(
                        ray.intensity,
                        ray.intensity * hitAttenuation
                    );

                    bool isPrism = dynamic_cast<const PrismMaterial*>(hitShape->getMaterial()) != nullptr;
                    auto reflectionResult = hitShape->getMaterial()->calculateReflection(
                        ray.ray.direction,
                        normal,
                        intensity,
                        ray.ray.spectrum,
                        nearestHit.position
                    );

                    if (isPrism) {
                        const ConsoleRectangle* rect = dynamic_cast<const ConsoleRectangle*>(hitShape);
                        if (!rect) continue;

                        Vector2D rectSize(static_cast<float>(rect->getWidth()),
                            static_cast<float>(rect->getHeight()));

                        for (const auto& reflected : reflectionResult.reflectedRays) {
                            Vector2D exitPoint;
                            bool found = findPrismExitPoint(nearestHit.position,
                                reflected.direction,
                                rect->getPosition(),
                                rectSize,
                                exitPoint);

                            if (found) {
                                float newTotalDistance = ray.totalDistance + nearestHit.distance;
                                float newAttenuation = std::max<float>(0.0f, 1.0f - (newTotalDistance / maxLength));

                                if (reflected.intensity >= 0.1f && newAttenuation > 0.0f) {
                                    RaySegment newRay(exitPoint,
                                        LightSource::Ray(reflected.direction, reflected.spectrum),
                                        reflected.intensity
                                    );
                                    newRay.bounces = ray.bounces + 1;
                                    newRay.totalDistance = newTotalDistance;
                                    nextRays.push_back(newRay);
                                }
                            }
                        }
                    }
                    else {
                        for (const auto& point : reflectionResult.sssPoints) {
                            SHORT x, y;
                            point.position.toConsoleCoords(x, y);
                            if (scene.isInWorkingArea(x, y)) {
                                buffer.setPixel(x, y, point.color, point.symbol);
                            }
                        }

                        for (const auto& reflected : reflectionResult.reflectedRays) {
                            float newTotalDistance = ray.totalDistance + nearestHit.distance;
                            float newAttenuation = std::max<float>(0.0f,
                                1.0f - (newTotalDistance / maxLength));

                            if (reflected.intensity >= 0.1f && newAttenuation > 0.0f) {
                                Vector2D newPos = nearestHit.position + reflected.direction * 0.1f;
                                RaySegment newRay(newPos,
                                    LightSource::Ray(reflected.direction, reflected.spectrum),
                                    reflected.intensity
                                );
                                newRay.bounces = ray.bounces + 1;
                                newRay.totalDistance = newTotalDistance;
                                nextRays.push_back(newRay);
                            }
                        }
                    }
                }
            }

            activeRays.swap(nextRays);
        }
    }

    bool findPrismExitPoint(const Vector2D& entryPoint,
        const Vector2D& direction,
        const Vector2D& rectPos,
        const Vector2D& rectSize,
        Vector2D& exitPoint) const {
        float tRight = (rectPos.getX() + rectSize.getX() - entryPoint.getX()) / direction.getX();
        float tLeft = (rectPos.getX() - entryPoint.getX()) / direction.getX();
        float tBottom = (rectPos.getY() + rectSize.getY() - entryPoint.getY()) / direction.getY();
        float tTop = (rectPos.getY() - entryPoint.getY()) / direction.getY();

        std::vector<std::pair<float, Vector2D>> intersections;

        auto addIntersection = [&](float t, const Vector2D& point) {
            if (t > 0 && point.getX() >= rectPos.getX() &&
                point.getX() <= rectPos.getX() + rectSize.getX() &&
                point.getY() >= rectPos.getY() &&
                point.getY() <= rectPos.getY() + rectSize.getY()) {
                intersections.emplace_back(t, point);
            }
            };

        if (std::abs(direction.getX()) > 1e-6f) {
            Vector2D rightPoint = entryPoint + direction * tRight;
            Vector2D leftPoint = entryPoint + direction * tLeft;
            addIntersection(tRight, rightPoint);
            addIntersection(tLeft, leftPoint);
        }

        if (std::abs(direction.getY()) > 1e-6f) {
            Vector2D bottomPoint = entryPoint + direction * tBottom;
            Vector2D topPoint = entryPoint + direction * tTop;
            addIntersection(tBottom, bottomPoint);
            addIntersection(tTop, topPoint);
        }

        if (!intersections.empty()) {
            std::sort(intersections.begin(), intersections.end());
            exitPoint = intersections[0].second;
            return true;
        }

        return false;
    }

public:
    ConsoleRenderer(HANDLE console, Scene& scene)
        : hConsole(console)
        , scene(scene)
        , buffer(console, 120, 50)
    {
        DWORD consoleMode;
        GetConsoleMode(hConsole, &consoleMode);
        SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        consoleSize = csbi.dwSize;

        panel.width = 25;
        panel.height = 18;
        panel.x = scene.getX() + scene.getWidth() + 2;
        panel.y = scene.getY();
    }

    void render() {
        clearArea(0, 0, consoleSize.X, consoleSize.Y);
        clearArea(scene.getX(), scene.getY(), scene.getWidth(), scene.getHeight());

        drawControlPanel();
        drawMaterialList(scene.getSelectedMaterial());

        for (const auto& border : scene.getBorders()) {
            border->draw(buffer);
        }

        for (const auto& shape : scene.getShapes()) {
            shape->draw(buffer);
        }

        for (const auto& light : scene.getLights()) {
            std::vector<LightSource::Ray> rays;
            light->generateRays(rays);

            for (const auto& ray : rays) {
                drawRay(light->getPosition(), ray, light->getIntensity());
            }
        }

        for (const auto& light : scene.getLights()) {
            light->draw(buffer);
        }

        buffer.flush();
    }

    void drawMaterialList(int selectedMaterial) {
        SHORT y = materialListY;

        std::vector<std::wstring> materials = {
             L"   Mirror",
             L"   Absorber",
             L"   Matte",
             L"   Prism"
        };

        materials[selectedMaterial] = L">  " + materials[selectedMaterial].substr(3);

        for (const auto& material : materials) {
            drawString(panel.x + 3, y++, material);
        }
    }

    void updateMaterialSelection(int selectedMaterial) {
        render();
    }

private:
    Scene& scene;
};