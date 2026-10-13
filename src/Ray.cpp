#include <Ray.hpp>


Ray::Ray() {}

Ray::Ray(const Point3 &origin, const Vec3 &direction) : m_orig(origin), m_dir(direction) {}

const Point3 &Ray::origin() const { return m_orig; }
const Vec3 &Ray::direction() const { return m_dir; }

Point3 Ray::at(float t) const {
    return m_orig + t * m_dir;
}

