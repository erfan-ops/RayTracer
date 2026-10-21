#pragma once

#include <Interval.hpp>
#include <Ray.hpp>
#include <Vec3.hpp>

class AABB {
public:
    AABB();

    AABB(const Interval& x, const Interval& y, const Interval& z);

    AABB(const Point3& a, const Point3& b);

    AABB(const AABB& box0, const AABB& box1);

    const Interval& axis_interval(int n) const;

    bool hit(const Ray& r, Interval ray_t) const;

    int longest_axis() const;

private:
    void pad_to_minimums() {
        double delta = 0.0001;
        if (x.size() < delta) x = x.expand(delta);
        if (y.size() < delta) y = y.expand(delta);
        if (z.size() < delta) z = z.expand(delta);
    }

public:
    Interval x, y, z;
    static const AABB empty, universe;
};

AABB operator+(const AABB& bbox, const Vec3& offset);
AABB operator+(const Vec3& offset, const AABB& bbox);
