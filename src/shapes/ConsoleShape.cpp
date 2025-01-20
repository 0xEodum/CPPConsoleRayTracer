#pragma once
#include <memory>
#include <string>
#include <windows.h>
#include "../materials/Material.cpp"
#include "../materials/Matte.cpp"
#include "../console/ConsoleBuffer.cpp"
#include "../core/Vector2D.cpp"

class ConsoleShape {
protected:
    struct BorderSymbols {
        wchar_t horizontal;
        wchar_t vertical;
        wchar_t topLeft;
        wchar_t topRight;
        wchar_t bottomLeft;
        wchar_t bottomRight;
    };

    Vector2D position;
    BorderSymbols borders;
    bool isFilled;
    std::shared_ptr<Material> material;

    void drawColoredChar(ConsoleBuffer& buffer, SHORT x, SHORT y, const Color& color, wchar_t symbol) const {
        buffer.setPixel(x, y, color, symbol);
    }

public:
    struct RayHit {
        bool hit;
        Vector2D position;
        float distance;

        RayHit() : hit(false), distance((std::numeric_limits<float>::max)()) {}

        RayHit(bool hit, const Vector2D& pos, float dist)
            : hit(hit), position(pos), distance(dist) {
        }
    };

    virtual RayHit intersectRay(const Vector2D& origin, const Vector2D& direction) const = 0;

    ConsoleShape(SHORT x, SHORT y, bool filled = false)
        : position(Vector2D::fromConsoleCoords(x, y))
        , isFilled(filled)
        , material(std::make_shared<Matte>())
    {
        borders = {
            L'─',
            L'│',
            L'┌',
            L'┐',
            L'└',
            L'┘'
        };
    }

    virtual ~ConsoleShape() = default;

    virtual void draw(ConsoleBuffer& buffer) const = 0;
    virtual bool containsPoint(const Vector2D& point) const = 0;
    virtual std::unique_ptr<ConsoleShape> clone() const = 0;

    virtual Vector2D getNormalAt(const Vector2D& point) const = 0;


    void setDoubleLineStyle() {
        borders = {
            L'═',
            L'║',
            L'╔',
            L'╗',
            L'╚',
            L'╝'
        };
    }

    void setBoldLineStyle() {
        borders = {
            L'━',
            L'┃',
            L'┏',
            L'┓',
            L'┗',
            L'┛'
        };
    }

    void setDashedLineStyle() {
        borders = {
            L'╌',
            L'╎',
            L'┌',
            L'┐',
            L'└',
            L'┘'
        };
    }

    void setMaterial(std::shared_ptr<Material> newMaterial) {
        material = newMaterial;
    }

    const Material* getMaterial() const {
        return material.get();
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

    const Vector2D& getPosition() const { return position; }

    void setPosition(SHORT newX, SHORT newY) {
        position = Vector2D::fromConsoleCoords(newX, newY);
    }

    void setPosition(const Vector2D& newPosition) {
        position = newPosition;
    }

    void setFilled(bool fill) { isFilled = fill; }
    bool getFilled() const { return isFilled; }

protected:
    RayHit calculateRectangleIntersection(
        const Vector2D& origin,
        const Vector2D& direction,
        const Vector2D& rectSize
    ) const {
        RayHit hit;

        SHORT px, py;
        position.toConsoleCoords(px, py);
        SHORT width, height;
        rectSize.toConsoleCoords(width, height);

        float tNear = -(std::numeric_limits<float>::max)();
        float tFar = (std::numeric_limits<float>::max)();

        if (std::abs(direction.getX()) > 1e-6f) {
            float t1 = (px - origin.getX()) / direction.getX();
            float t2 = (px + width - 1 - origin.getX()) / direction.getX();

            tNear = std::min<float>(t1, t2);
            tFar = std::max<float>(t1, t2);
        }

        if (std::abs(direction.getY()) > 1e-6f) {
            float t1 = (py - origin.getY()) / direction.getY();
            float t2 = (py + height - 1 - origin.getY()) / direction.getY();

            tNear = std::max<float>(tNear, std::min<float>(t1, t2));
            tFar = std::min<float>(tFar, std::max<float>(t1, t2));
        }

        if (tNear <= tFar && tNear > 0) {
            float hitX = origin.getX() + tNear * direction.getX();
            float hitY = origin.getY() + tNear * direction.getY();

            if (hitX >= px && hitX < px + width &&
                hitY >= py && hitY < py + height) {
                hit.hit = true;
                hit.position = Vector2D(hitX, hitY);
                hit.distance = tNear;
            }
        }

        return hit;
    }
};