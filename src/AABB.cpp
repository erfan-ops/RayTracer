#include <AABB.hpp>
#include <Interval.hpp>


const AABB AABB::empty    = AABB(Interval::empty,    Interval::empty,    Interval::empty);
const AABB AABB::universe = AABB(Interval::universe, Interval::universe, Interval::universe);


AABB::AABB() {}

AABB::AABB(
    const Interval& x,
    const Interval& y,
    const Interval& z
) : x(x), y(y), z(z)
{
    pad_to_minimums();
}

AABB::AABB(const Point3& a, const Point3& b) {
    // Treat the two points a and b as extrema for the bounding box, so we don't require a
    // particular minimum/maximum coordinate order.

    x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
    y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);
    z = (a[2] <= b[2]) ? Interval(a[2], b[2]) : Interval(b[2], a[2]);

    pad_to_minimums();
}

AABB::AABB(const AABB& box0, const AABB& box1) {
    x = Interval(box0.x, box1.x);
    y = Interval(box0.y, box1.y);
    z = Interval(box0.z, box1.z);
}

const Interval& AABB::axis_interval(int n) const {
    if (n == 1) return y;
    if (n == 2) return z;
    return x;
}

bool AABB::hit(const Ray& r, Interval ray_t) const {
    const Point3& ray_orig = r.origin();
    const Vec3&   ray_dir  = r.direction();

    for (int axis = 0; axis < 3; axis++) {
        const Interval& ax = axis_interval(axis);
        const double adinv = 1.0 / ray_dir[axis];

        auto t0 = (ax.min() - ray_orig[axis]) * adinv;
        auto t1 = (ax.max() - ray_orig[axis]) * adinv;

        if (t0 < t1) {
            if (t0 > ray_t.min()) ray_t.set_min(t0);
            if (t1 < ray_t.max()) ray_t.set_max(t1);
        } else {
            if (t1 > ray_t.min()) ray_t.set_min(t1);
            if (t0 < ray_t.max()) ray_t.set_max(t0);
        }

        if (ray_t.max() <= ray_t.min())
            return false;
    }
    return true;
}

int AABB::longest_axis() const {
    // Returns the index of the longest axis of the bounding box.

    if (x.size() > y.size())
        return x.size() > z.size() ? 0 : 2;
    else
        return y.size() > z.size() ? 1 : 2;
}

AABB operator+(const AABB& bbox, const Vec3& offset) {
    return AABB(bbox.x + offset.x(), bbox.y + offset.y(), bbox.z + offset.z());
}

AABB operator+(const Vec3& offset, const AABB& bbox) {
    return bbox + offset;
}
