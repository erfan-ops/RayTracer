#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <numbers>

// Constants

const double infinity = std::numeric_limits<double>::infinity();

// Utility Functions

inline double degrees_to_radians(double degrees) {
    return degrees * std::numbers::pi / 180.0f;
}


static std::mt19937 engine{std::random_device{}()};

inline double random_double() {
    // Returns a random real in [0,1).
    return std::uniform_real_distribution<double>(0.0, 1.0)(engine);
}

inline double random_double(double min, double max) {
    // Returns a random real in [min,max).
    return random_double() * (max - min) + min;
}
