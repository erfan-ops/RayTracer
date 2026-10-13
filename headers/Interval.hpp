#pragma once

#include <Common.hpp>

class Interval {
public:
    Interval();

    Interval(double min, double max);

    double size() const;

    bool contains(double x) const;

    bool surrounds(double x) const;

    double clamp(double x) const;

    double min() const;
    double max() const;

public:
    static const Interval empty;
    static const Interval universe;

private:
    double m_min;
    double m_max;
};
