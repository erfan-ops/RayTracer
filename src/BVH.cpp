#include <BVH.hpp>

#include <algorithm>

BVHNode::BVHNode(
    HittableList list
) : BVHNode(list.objects, 0, list.objects.size())
{
    // There's a C++ subtlety here. This constructor (without span indices) creates an
    // implicit copy of the hittable list, which we will modify. The lifetime of the copied
    // list only extends until this constructor exits. That's OK, because we only need to
    // persist the resulting bounding volume hierarchy.
}

BVHNode::BVHNode(std::vector<std::shared_ptr<Hittable>>& objects, size_t start, size_t end) {
    // Build the bounding box of the span of source objects.
    m_bbox = AABB::empty;
    for (size_t object_index=start; object_index < end; object_index++)
        m_bbox = AABB(m_bbox, objects[object_index]->bounding_box());

    int axis = m_bbox.longest_axis();

    auto comparator = (axis == 0) ? box_x_compare
                    : (axis == 1) ? box_y_compare
                    : box_z_compare;

    size_t object_span = end - start;

    if (object_span == 1) {
        left = right = objects[start];
    } else if (object_span == 2) {
        left = objects[start];
        right = objects[start+1];
    } else {
        std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);

        auto mid = start + object_span/2;
        left = std::make_shared<BVHNode>(objects, start, mid);
        right = std::make_shared<BVHNode>(objects, mid, end);
    }
}

bool BVHNode::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    if (!m_bbox.hit(r, ray_t))
        return false;

    bool hit_left = left->hit(r, ray_t, rec);
    bool hit_right = right->hit(r, Interval(ray_t.min(), hit_left ? rec.t : ray_t.max()), rec);

    return hit_left || hit_right;
}

AABB BVHNode::bounding_box() const { return m_bbox; }



bool BVHNode::box_compare(const std::shared_ptr<Hittable> a, const std::shared_ptr<Hittable> b, int axis_index) {
    auto a_axis_interval = a->bounding_box().axis_interval(axis_index);
    auto b_axis_interval = b->bounding_box().axis_interval(axis_index);
    return a_axis_interval.min() < b_axis_interval.min();
}

bool BVHNode::box_x_compare (const std::shared_ptr<Hittable> a, const std::shared_ptr<Hittable> b) {
    return box_compare(a, b, 0);
}

bool BVHNode::box_y_compare (const std::shared_ptr<Hittable> a, const std::shared_ptr<Hittable> b) {
    return box_compare(a, b, 1);
}

bool BVHNode::box_z_compare (const std::shared_ptr<Hittable> a, const std::shared_ptr<Hittable> b) {
    return box_compare(a, b, 2);
}