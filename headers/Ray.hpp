#pragma once

#include <Vec3.hpp>

class Ray {
public:
    Ray();
    Ray(const Point3& origin, const Vec3& direction);
    Ray(const Point3& origin, const Vec3& direction, double time);

    const Point3& origin() const;
    const Vec3& direction() const;

    double time() const;

    Point3 at(double t) const;

private:
    Point3 m_orig;
    Vec3 m_dir;
    double m_time;
};
