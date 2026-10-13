#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <random>

// Constants

const float infinity = std::numeric_limits<float>::infinity();
const float pi = 3.1415926535897932385;

// Utility Functions

inline float degrees_to_radians(float degrees) {
    return degrees * pi / 180.0f;
}


static std::mt19937 engine{std::random_device{}()};

inline float random_float() {
    // Returns a random real in [0,1).
    return std::uniform_real_distribution<float>(0.0, 1.0)(engine);
}

inline float random_float(float min, float max) {
    // Returns a random real in [min,max).
    return std::uniform_real_distribution<float>(min, max)(engine);
}

// Common Headers

#include <Ray.hpp>
#include <Vec3.hpp>
