#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <cmath>

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;

    Vec2(float p_x, float p_y) : x(p_x), y(p_y) {}

    inline Vec2 operator+(const Vec2& rhs) const 
    {
        return Vec2(
                x + rhs.x, y + rhs.y
                );
    }

    inline Vec2 operator-(const Vec2& rhs) const 
    {
        return Vec2(
                x - rhs.x, y - rhs.y
                );
    }

    inline Vec2 operator*(float scalar) const 
    {
        return Vec2(
                x * scalar, y * scalar
                );
    }

    inline Vec2 operator/(float scalar) const 
    {
        return Vec2(
                x / scalar, y / scalar
                );
    }

    inline float length_squared() const {return x * x + y * y;}
    inline float length() const {return std::sqrt(length_squared());}

    inline Vec2 normalized() const 
    {
        float len = length();
        return (len > 0.0001f) ? Vec2(x/len,y/len) : Vec2(0.0f, 0.0f);
    }

    inline float distance_to(const Vec2& other) const 
    {
        return (*this - other).length();
    }
};

struct IntVec2
{
    int x = 0;
    int y = 0;

    IntVec2() = default;
    IntVec2(int p_x, int p_y) : x(p_x), y(p_y) {}

    inline bool operator==(const IntVec2& rhs) const {return x == rhs.x && y == rhs.y;}
};

#endif
