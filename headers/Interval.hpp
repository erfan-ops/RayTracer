#pragma once

#include <Common.hpp>

class Interval {
public:
    Interval();

    Interval(double min, double max);

    Interval(const Interval& a, const Interval& b);

    double size() const;

    bool contains(double x) const;

    bool surrounds(double x) const;

    double clamp(double x) const;

    Interval expand(double delta) const;

    double min() const;
    double max() const;

    void set_min(double m);
    void set_max(double m);

public:
    static const Interval empty;
    static const Interval universe;

private:
    double m_min;
    double m_max;
};

Interval operator+(const Interval& ival, double displacement);
Interval operator+(double displacement, const Interval& ival);
