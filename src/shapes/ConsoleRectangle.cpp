#pragma once
#include "../shapes/ConsoleShape.cpp"

class ConsoleRectangle : public ConsoleShape {
private:
    Vector2D size;

    Vector2D determineCornerNormal(const Vector2D& point,
        bool isNearLeft, bool isNearRight,
        bool isNearTop, bool isNearBottom) const {
        if (isNearLeft && isNearTop) {
            return Vector2D(-1.0f, -1.0f).normalize();
        }
        if (isNearRight && isNearTop) {
            return Vector2D(1.0f, -1.0f).normalize();
        }
        if (isNearLeft && isNearBottom) {
            return Vector2D(-1.0f, 1.0f).normalize();
        }
        if (isNearRight && isNearBottom) {
            return Vector2D(1.0f, 1.0f).normalize();
        }

        return Vector2D(0.0f, 1.0f);
    }

public:
    ConsoleRectangle(SHORT x, SHORT y, SHORT width, SHORT height, bool filled = false)
        : ConsoleShape(x, y, filled)
        , size(Vector2D::fromConsoleCoords(width, height))
    {
    }

    void draw(ConsoleBuffer& buffer) const override {
        Color materialColor = material->getColor();
        Color borderColor = materialColor.darker(0.8f);
        SHORT x, y;
        position.toConsoleCoords(x, y);
        SHORT width, height;
        size.toConsoleCoords(width, height);

        if (isFilled) {
            wchar_t fillSymbol = material->getFillSymbol();
            for (SHORT dy = 1; dy < height - 1; dy++) {
                for (SHORT dx = 1; dx < width - 1; dx++) {
                    drawColoredChar(buffer, x + dx, y + dy, materialColor, fillSymbol);
                }
            }
        }

        for (SHORT dx = 1; dx < width - 1; dx++) {
            drawColoredChar(buffer, x + dx, y, borderColor, borders.horizontal);
            drawColoredChar(buffer, x + dx, y + height - 1, borderColor, borders.horizontal);
        }

        for (SHORT dy = 1; dy < height - 1; dy++) {
            drawColoredChar(buffer, x, y + dy, borderColor, borders.vertical);
            drawColoredChar(buffer, x + width - 1, y + dy, borderColor, borders.vertical);
        }

        drawColoredChar(buffer, x, y, borderColor, borders.topLeft);
        drawColoredChar(buffer, x + width - 1, y, borderColor, borders.topRight);
        drawColoredChar(buffer, x, y + height - 1, borderColor, borders.bottomLeft);
        drawColoredChar(buffer, x + width - 1, y + height - 1, borderColor, borders.bottomRight);
    }

    RayHit intersectRay(const Vector2D& origin, const Vector2D& direction) const override {
        return calculateRectangleIntersection(origin, direction, size);
    }

    bool intersectsWith(const ConsoleRectangle* other) const {
        SHORT thisLeft = getX();
        SHORT thisRight = thisLeft + getWidth();
        SHORT thisTop = getY();
        SHORT thisBottom = thisTop + getHeight();

        SHORT otherLeft = other->getX();
        SHORT otherRight = otherLeft + other->getWidth();
        SHORT otherTop = other->getY();
        SHORT otherBottom = otherTop + other->getHeight();

        return !(thisRight < otherLeft ||
            thisLeft > otherRight ||
            thisBottom < otherTop ||
            thisTop > otherBottom);
    }

    Vector2D getNormalAt(const Vector2D& point) const override {
        SHORT px, py;
        point.toConsoleCoords(px, py);
        SHORT x = getX();
        SHORT y = getY();
        SHORT width = getWidth();
        SHORT height = getHeight();

        float distLeft = std::abs(px - x);
        float distRight = std::abs(px - (x + width - 1));
        float distTop = std::abs(py - y);
        float distBottom = std::abs(py - (y + height - 1));

        const float CORNER_THRESHOLD = 0.1f;
        bool isNearLeft = distLeft < CORNER_THRESHOLD;
        bool isNearRight = distRight < CORNER_THRESHOLD;
        bool isNearTop = distTop < CORNER_THRESHOLD;
        bool isNearBottom = distBottom < CORNER_THRESHOLD;

        if ((isNearLeft || isNearRight) && (isNearTop || isNearBottom)) {

            return determineCornerNormal(point, isNearLeft, isNearRight, isNearTop, isNearBottom);
        }

        float minDist = std::min<float>({ distLeft, distRight, distTop, distBottom });

        if (minDist == distLeft) return Vector2D(-1.0f, 0.0f);
        if (minDist == distRight) return Vector2D(1.0f, 0.0f);
        if (minDist == distTop) return Vector2D(0.0f, -1.0f);
        return Vector2D(0.0f, 1.0f);
    }

    bool containsPoint(const Vector2D& point) const override {
        SHORT px, py;
        point.toConsoleCoords(px, py);
        SHORT x = getX();
        SHORT y = getY();
        SHORT width = getWidth();
        SHORT height = getHeight();

        return px >= x && px < x + width && py >= y && py < y + height;
    }

    std::unique_ptr<ConsoleShape> clone() const override {
        return std::make_unique<ConsoleRectangle>(*this);
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

    const Vector2D& getSize() const { return size; }
};