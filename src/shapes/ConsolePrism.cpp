#pragma once
#include "../shapes/ConsoleShape.cpp"
#include "../materials/PrismMaterial.cpp"

class ConsolePrism : public ConsoleShape {
private:
    Vector2D size;
    float angle;
    Vector2D direction;

public:
    ConsolePrism(SHORT x, SHORT y, SHORT width, SHORT height)
        : ConsoleShape(x, y, false)
        , size(Vector2D::fromConsoleCoords(width, height))
        , angle(M_PI / 3.0f)
        , direction(Vector2D(1.0f, -1.0f).normalize())
    {
        setMaterial(std::make_shared<PrismMaterial>());
    }

    void draw(ConsoleBuffer& buffer) const override {
        Color borderColor = material->getColor();
        SHORT x, y;
        position.toConsoleCoords(x, y);
        SHORT width, height;
        size.toConsoleCoords(width, height);

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

        for (SHORT i = 1; i < height - 1; i++) {
            SHORT diagX = x + width - 1 - i;
            if (diagX > x) {
                drawColoredChar(buffer, diagX, y + i, borderColor, L'\\');
            }
        }
    }

    RayHit intersectRay(const Vector2D& origin, const Vector2D& direction) const override {
        RayHit hit = calculateRectangleIntersection(origin, direction, size);


        return hit;
    }

    Vector2D getNormalAt(const Vector2D& point) const override {
        SHORT px, py;
        point.toConsoleCoords(px, py);
        SHORT x = getX();
        SHORT y = getY();
        SHORT width = getWidth();
        SHORT height = getHeight();

        if (px == x) {
            return Vector2D(-1.0f, 0.0f);
        }
        else if (px == x + width - 1) {
            return Vector2D(cos(angle), sin(angle)).normalize();
        }
        else if (py == y) {
            return Vector2D(0.0f, -1.0f);
        }
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
        return std::make_unique<ConsolePrism>(*this);
    }

    float getAngle() const { return angle; }
    void setAngle(float newAngle) { angle = newAngle; }

    const Vector2D& getDirection() const { return direction; }
    void setDirection(const Vector2D& newDirection) {
        direction = newDirection.normalize();
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