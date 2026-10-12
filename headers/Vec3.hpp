#pragma once

#include <cmath>
#include <iostream>

class Vec3 {
public:
    Vec3();
    Vec3(float e0, float e1, float e2);

    float x() const;
    float y() const;
    float z() const;

    float r() const;
    float g() const;
    float b() const;

    Vec3 operator-() const;
    float operator[](int i) const;
    float &operator[](int i);

    Vec3 &operator+=(const Vec3 &v);
    Vec3 &operator*=(float t);
    Vec3 &operator/=(float t);

    float length() const;
    float length_squared() const;

public:
    float e[3];
};

// point3 is just an alias for Vec3, but useful for geometric clarity in the code.
using Point3 = Vec3;

// Vector Utility Functions

inline std::ostream &operator<<(std::ostream &out, const Vec3 &v) {
    return out << v.e[0] << ' ' << v.e[1] << ' ' << v.e[2];
}

inline Vec3 operator+(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[0] + v.e[0], u.e[1] + v.e[1], u.e[2] + v.e[2]);
}

inline Vec3 operator-(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[0] - v.e[0], u.e[1] - v.e[1], u.e[2] - v.e[2]);
}

inline Vec3 operator*(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[0] * v.e[0], u.e[1] * v.e[1], u.e[2] * v.e[2]);
}

inline Vec3 operator*(float t, const Vec3 &v) {
    return Vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

inline Vec3 operator*(const Vec3 &v, float t) {
    return t * v;
}

inline Vec3 operator/(const Vec3 &v, float t) {
    return (1 / t) * v;
}

inline float dot(const Vec3 &u, const Vec3 &v) {
    return u.e[0] * v.e[0] + u.e[1] * v.e[1] + u.e[2] * v.e[2];
}

inline Vec3 cross(const Vec3 &u, const Vec3 &v) {
    return Vec3(u.e[1] * v.e[2] - u.e[2] * v.e[1],
                u.e[2] * v.e[0] - u.e[0] * v.e[2],
                u.e[0] * v.e[1] - u.e[1] * v.e[0]);
}

inline Vec3 unit_vector(const Vec3 &v) {
    return v / v.length();
}
