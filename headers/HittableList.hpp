#pragma once

#include <Hittable.hpp>
#include <Interval.hpp>

#include <memory>
#include <vector>


class HittableList : public Hittable {
public:
    HittableList();
    HittableList(std::shared_ptr<Hittable> object);

    void clear();

    void add(std::shared_ptr<Hittable> object);

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

public:
    std::vector<std::shared_ptr<Hittable>> objects;
};
