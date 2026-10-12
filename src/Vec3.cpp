#include <Vec3.hpp>



Vec3::Vec3() : e{0, 0, 0} {}
Vec3::Vec3(float e0, float e1, float e2) : e{e0, e1, e2} {}

float Vec3::x() const { return e[0]; }
float Vec3::y() const { return e[1]; }
float Vec3::z() const { return e[2]; }

float Vec3::r() const { return e[0]; }
float Vec3::g() const { return e[1]; }
float Vec3::b() const { return e[2]; }

Vec3 Vec3::operator-() const { return Vec3(-e[0], -e[1], -e[2]); }
float Vec3::operator[](int i) const { return e[i]; }
float &Vec3::operator[](int i) { return e[i]; }

Vec3 &Vec3::operator+=(const Vec3 &v) {
    e[0] += v.e[0];
    e[1] += v.e[1];
    e[2] += v.e[2];
    return *this;
}

Vec3 &Vec3::operator*=(float t) {
    e[0] *= t;
    e[1] *= t;
    e[2] *= t;
    return *this;
}

Vec3 &Vec3::operator/=(float t) {
    return *this *= 1 / t;
}

float Vec3::length() const {
    return std::sqrt(length_squared());
}

float Vec3::length_squared() const {
    return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
}
