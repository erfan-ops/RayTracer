#include <Ray.hpp>


Ray::Ray() {}

Ray::Ray(const Point3& origin, const Vec3& direction) : m_orig(origin), m_dir(direction) {}
Ray::Ray(const Point3& origin, const Vec3& direction, double time) : m_orig(origin), m_dir(direction), m_time(time) {}

const Point3& Ray::origin() const { return m_orig; }
const Vec3& Ray::direction() const { return m_dir; }

double Ray::time() const { return m_time; }

Point3 Ray::at(double t) const {
    return m_orig + t * m_dir;
}

