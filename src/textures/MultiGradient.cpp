#include <Texture.hpp>

#include <immintrin.h>

MultiGradientTexture::MultiGradientTexture(
    const std::vector<Color>& colors,
    Vec3 direction,
    double length,
    int flag
) : m_colors(colors)
  , m_direction(direction)
  , m_length(length)
  , m_flag(flag)
{}

Color MultiGradientTexture::value(double u, double v, const Point3& p) const {
    const double d = dot(p, m_direction);

    double t = std::fmod(std::abs(d), m_length) / m_length;

    const double scaled = t * (m_colors.size() - 1);
    const int index = static_cast<int>(scaled);
    const double local_t = scaled - index;

    const Color& c1 = m_colors[static_cast<size_t>(index)];
    const Color& c2 = m_colors[static_cast<size_t>(index) + 1];

    __m256d v1 = _mm256_set_pd(0.0, c1.b(), c1.g(), c1.r());
    __m256d v2 = _mm256_set_pd(0.0, c2.b(), c2.g(), c2.r());
    __m256d tvec = _mm256_set1_pd(local_t);

    __m256d result = _mm256_fmadd_pd(_mm256_sub_pd(v2, v1), tvec, v1);

    double temp[4];
    _mm256_storeu_pd(temp, result);

    Color out(temp[0], temp[1], temp[2]);

    return out;
}