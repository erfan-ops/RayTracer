#include <Interval.hpp>


Interval::Interval() : m_min(+infinity), m_max(-infinity) {} // Default interval is empty

Interval::Interval(double min, double max) : m_min(min), m_max(max) {}

Interval::Interval(const Interval& a, const Interval& b) {
    m_min = a.min() <= b.min() ? a.min() : b.min();
    m_max = a.max() >= b.max() ? a.max() : b.max();
}

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

Interval Interval::expand(double delta) const {
    double padding = delta * 0.5;
    return Interval(m_min - padding, m_max + padding);
}

double Interval::min() const {
    return m_min;
}

double Interval::max() const {
    return m_max;
}

void Interval::set_min(double m) {
    m_min = m;
}

void Interval::set_max(double m) {
    m_max = m;
}

const Interval Interval::empty    = Interval(+infinity, -infinity);
const Interval Interval::universe = Interval(-infinity, +infinity);
