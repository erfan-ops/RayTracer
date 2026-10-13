#include <Interval.hpp>


Interval::Interval() : m_min(+infinity), m_max(-infinity) {} // Default interval is empty

Interval::Interval(double min, double max) : m_min(min), m_max(max) {}

double Interval::size() const {
    return m_max - m_min;
}

bool Interval::contains(double x) const {
    return m_min <= x && x <= m_max;
}

bool Interval::surrounds(double x) const {
    return m_min < x && x < m_max;
}

double Interval::clamp(double x) const {
    return std::clamp(x, m_min, m_max);
}

double Interval::min() const {
    return m_min;
}

double Interval::max() const {
    return m_max;
}

const Interval Interval::empty    = Interval(+infinity, -infinity);
const Interval Interval::universe = Interval(-infinity, +infinity);
