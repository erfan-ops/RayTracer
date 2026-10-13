#include <Interval.hpp>


Interval::Interval() : m_min(+infinity), m_max(-infinity) {} // Default interval is empty

Interval::Interval(float min, float max) : m_min(min), m_max(max) {}

float Interval::size() const {
    return m_max - m_min;
}

bool Interval::contains(float x) const {
    return m_min <= x && x <= m_max;
}

bool Interval::surrounds(float x) const {
    return m_min < x && x < m_max;
}

float Interval::clamp(float x) const {
    return std::clamp(x, m_min, m_max);
}

float Interval::min() const {
    return m_min;
}

float Interval::max() const {
    return m_max;
}

const Interval Interval::empty    = Interval(+infinity, -infinity);
const Interval Interval::universe = Interval(-infinity, +infinity);
