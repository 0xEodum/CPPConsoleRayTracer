#pragma once
#include <windows.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>

class Vector2D {
private:
    float x, y;

public:
    Vector2D() : x(0.0f), y(0.0f) {}
    Vector2D(float x, float y) : x(x), y(y) {}

    float getX() const { return x; }
    float getY() const { return y; }

    Vector2D operator+(const Vector2D& other) const {
        return Vector2D(x + other.x, y + other.y);
    }

    Vector2D operator-(const Vector2D& other) const {
        return Vector2D(x - other.x, y - other.y);
    }

    Vector2D operator*(float scalar) const {
        return Vector2D(x * scalar, y * scalar);
    }

    Vector2D operator-() const {
        return Vector2D(-x, -y);
    }

    float dot(const Vector2D& other) const {
        return x * other.x + y * other.y;
    }

    float length() const {
        return std::sqrt(x * x + y * y);
    }

    float lengthSquared() const {
        return x * x + y * y;
    }

    Vector2D normalize() const {
        float len = length();
        if (len > 0) {
            return Vector2D(x / len, y / len);
        }
        return *this;
    }

    Vector2D rotate(float angle) const {
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        return Vector2D(
            x * cos_a - y * sin_a,
            x * sin_a + y * cos_a
        );
    }

    float getAngle() const {
        return std::atan2(y, x);
    }

    static Vector2D fromAngle(float angle) {
        return Vector2D(std::cos(angle), std::sin(angle));
    }

    static Vector2D zero() {
        return Vector2D(0.0f, 0.0f);
    }

    Vector2D reflect(const Vector2D& normal) const {
        float d = this->dot(normal);
        return *this - normal * (2.0f * d);
    }

    bool isNearlyEqual(const Vector2D& other, float tolerance = 1e-6f) const {
        return std::abs(x - other.x) <= tolerance &&
            std::abs(y - other.y) <= tolerance;
    }

    void toConsoleCoords(SHORT& outX, SHORT& outY) const {
        outX = static_cast<SHORT>(std::round(x));
        outY = static_cast<SHORT>(std::round(y));
    }

    static Vector2D fromConsoleCoords(SHORT x, SHORT y) {
        return Vector2D(static_cast<float>(x), static_cast<float>(y));
    }

    bool operator<(const Vector2D& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }

    bool operator>(const Vector2D& other) const {
        return other < *this;
    }

    bool operator<=(const Vector2D& other) const {
        return !(other < *this);
    }

    bool operator>=(const Vector2D& other) const {
        return !(*this < other);
    }

    bool operator==(const Vector2D& other) const {
        return isNearlyEqual(other);
    }

    bool operator!=(const Vector2D& other) const {
        return !(*this == other);
    }
};