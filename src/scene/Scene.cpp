#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "../shapes/ConsoleShape.cpp"
#include "../lights/LightSource.cpp"
#include "../materials/Material.cpp"
#include "../materials/Absorber.cpp"
#include "../shapes/ConsoleRectangle.cpp"
#include "../core/Vector2D.cpp"

class Scene {
private:
    int selectedMaterial = 0;

    struct WorkingArea {
        Vector2D position;
        Vector2D size;

        WorkingArea(SHORT x, SHORT y, SHORT width, SHORT height)
            : position(Vector2D::fromConsoleCoords(x, y))
            , size(Vector2D::fromConsoleCoords(width, height)) {
        }

        SHORT getX() const {
            SHORT x, y;
            position.toConsoleCoords(x, y);
            return x;
        }

        SHORT getY() const {
            SHORT x, y;
            position.toConsoleCoords(x, y);
            return y;
        }

        SHORT getWidth() const {
            SHORT width, height;
            size.toConsoleCoords(width, height);
            return width;
        }

        SHORT getHeight() const {
            SHORT width, height;
            size.toConsoleCoords(width, height);
            return height;
        }
    } area;

    std::vector<std::unique_ptr<ConsoleRectangle>> borders;
    std::vector<std::unique_ptr<ConsoleShape>> shapes;
    std::vector<std::unique_ptr<LightSource>> lights;

    void initializeBorders() {
        auto absorber = std::make_shared<Absorber>();

        auto createBorder = [absorber](SHORT x, SHORT y, SHORT width, SHORT height) {
            auto border = std::make_unique<ConsoleRectangle>(x, y, width, height, true);
            border->setMaterial(absorber);
            border->setDoubleLineStyle();
            return border;
            };

        borders.push_back(createBorder(
            area.getX(), area.getY() - 1,
            area.getWidth(), 1
        ));

        borders.push_back(createBorder(
            area.getX(), area.getY() + area.getHeight(),
            area.getWidth(), 1
        ));

        borders.push_back(createBorder(
            area.getX() - 1, area.getY() - 1,
            1, area.getHeight() + 2
        ));

        borders.push_back(createBorder(
            area.getX() + area.getWidth(), area.getY() - 1,
            1, area.getHeight() + 2
        ));
    }

public:
    Scene(SHORT x, SHORT y, SHORT width, SHORT height)
        : area(x, y, width, height)
    {
        initializeBorders();
    }

    bool addShape(std::unique_ptr<ConsoleShape> shape) {
        auto rect = dynamic_cast<ConsoleRectangle*>(shape.get());
        if (rect) {
            Vector2D pos(rect->getPosition());
            Vector2D size(Vector2D::fromConsoleCoords(rect->getWidth(), rect->getHeight()));

            if (pos.getX() <= area.position.getX() ||
                pos.getY() <= area.position.getY() ||
                pos.getX() + size.getX() >= area.position.getX() + area.size.getX() ||
                pos.getY() + size.getY() >= area.position.getY() + area.size.getY()) {
                return false;
            }

            for (const auto& existingShape : shapes) {
                if (auto existingRect = dynamic_cast<ConsoleRectangle*>(existingShape.get())) {
                    if (rect->intersectsWith(existingRect)) {
                        return false;
                    }
                }
            }
        }

        shapes.push_back(std::move(shape));
        return true;
    }

    bool addLight(std::unique_ptr<LightSource> light) {
        const Vector2D& pos = light->getPosition();
        SHORT x, y;
        pos.toConsoleCoords(x, y);

        if (!isInWorkingArea(x, y)) {
            return false;
        }

        lights.push_back(std::move(light));
        return true;
    }

    bool removeShape(SHORT x, SHORT y) {
        Vector2D point = Vector2D::fromConsoleCoords(x, y);
        auto it = std::find_if(shapes.begin(), shapes.end(),
            [&point](const auto& shape) {
                return shape->containsPoint(point);
            });

        if (it != shapes.end()) {
            shapes.erase(it);
            return true;
        }
        return false;
    }

    bool removeLight(SHORT x, SHORT y) {
        Vector2D point = Vector2D::fromConsoleCoords(x, y);
        auto it = std::find_if(lights.begin(), lights.end(),
            [point](const auto& light) {
                return (light->getPosition() - point).lengthSquared() < 9.0f;
            });

        if (it != lights.end()) {
            lights.erase(it);
            return true;
        }
        return false;
    }

    LightSource* getLightAt(SHORT x, SHORT y) {
        Vector2D point = Vector2D::fromConsoleCoords(x, y);
        auto it = std::find_if(lights.begin(), lights.end(),
            [point](const auto& light) {
                return (light->getPosition() - point).lengthSquared() < 9.0f;
            });

        return it != lights.end() ? it->get() : nullptr;
    }

    bool isInWorkingArea(SHORT x, SHORT y) const {
        return x >= area.getX() &&
            x < area.getX() + area.getWidth() &&
            y >= area.getY() &&
            y < area.getY() + area.getHeight();
    }

    bool isInWorkingArea(const Vector2D& point) const {
        SHORT x, y;
        point.toConsoleCoords(x, y);
        return isInWorkingArea(x, y);
    }

    void clear() {
        shapes.clear();
        lights.clear();
        initializeBorders();
    }

    SHORT getX() const { return area.getX(); }
    SHORT getY() const { return area.getY(); }
    SHORT getWidth() const { return area.getWidth(); }
    SHORT getHeight() const { return area.getHeight(); }

    const Vector2D& getPosition() const { return area.position; }
    const Vector2D& getSize() const { return area.size; }

    const std::vector<std::unique_ptr<ConsoleShape>>& getShapes() const { return shapes; }
    const std::vector<std::unique_ptr<LightSource>>& getLights() const { return lights; }
    const std::vector<std::unique_ptr<ConsoleRectangle>>& getBorders() const { return borders; }

    int getSelectedMaterial() const { return selectedMaterial; }
    void setSelectedMaterial(int material) { selectedMaterial = material; }
};