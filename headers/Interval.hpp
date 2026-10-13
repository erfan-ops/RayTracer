#pragma once

#include <Common.hpp>

class Interval {
public:
    Interval();

    Interval(float min, float max);

    float size() const;

    bool contains(float x) const;

    bool surrounds(float x) const;

    float clamp(float x) const;

    float min() const;
    float max() const;

public:
    static const Interval empty;
    static const Interval universe;

private:
    float m_min;
    float m_max;
};
