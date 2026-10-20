#include <Perlin.hpp>
#include <Common.hpp>

Perlin::Perlin() {
    for (int i = 0; i < point_count; i++) {
        randvec[i] = unit_vector(Vec3::random(-1,1));
    }

    perlin_generate_perm(perm_x);
    perlin_generate_perm(perm_y);
    perlin_generate_perm(perm_z);
}

double Perlin::noise(const Point3& p) const {
    auto u = p.x() - std::floor(p.x());
    auto v = p.y() - std::floor(p.y());
    auto w = p.z() - std::floor(p.z());

    auto i = int(std::floor(p.x()));
    auto j = int(std::floor(p.y()));
    auto k = int(std::floor(p.z()));
    Vec3 c[2][2][2];

    for (int di=0; di < 2; di++)
        for (int dj=0; dj < 2; dj++)
            for (int dk=0; dk < 2; dk++)
                c[di][dj][dk] = randvec[
                    perm_x[(i+di) & 255] ^
                    perm_y[(j+dj) & 255] ^
                    perm_z[(k+dk) & 255]
                ];

    return perlin_interp(c, u, v, w);
}

double Perlin::turb(const Point3& p, int depth) const {
    auto accum = 0.0;
    auto temp_p = p;
    auto weight = 1.0;

    for (int i = 0; i < depth; i++) {
        accum += weight * noise(temp_p);
        weight *= 0.5;
        temp_p *= 2;
    }

    return std::fabs(accum);
}

void Perlin::perlin_generate_perm(int* p) {
    for (int i = 0; i < point_count; i++)
        p[i] = i;

    permute(p, point_count);
}

void Perlin::permute(int* p, int n) {
    for (int i = n-1; i > 0; i--) {
        int target = random_int(0, i);
        int tmp = p[i];
        p[i] = p[target];
        p[target] = tmp;
    }
}

double Perlin::perlin_interp(const Vec3 c[2][2][2], double u, double v, double w) {
    auto uu = u*u*(3-2*u);
    auto vv = v*v*(3-2*v);
    auto ww = w*w*(3-2*w);
    auto accum = 0.0;

    for (int i=0; i < 2; i++)
        for (int j=0; j < 2; j++)
            for (int k=0; k < 2; k++) {
                Vec3 weight_v(u-i, v-j, w-k);
                accum += (i*uu + (1-i)*(1-uu))
                        * (j*vv + (1-j)*(1-vv))
                        * (k*ww + (1-k)*(1-ww))
                        * dot(c[i][j][k], weight_v);
            }

    return accum;
}
