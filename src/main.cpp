#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <Sphere.hpp>
#include <BVH.hpp>
#include <Texture.hpp>


void bouncing_spheres() {
    ImageWrite image{};

    const size_t width = 400ull;
    const size_t height = 225ull;

    // Create camera
    Camera cam(
        30.0,
        Point3(13.0, 2.0, 3.0),
        Point3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        300,
        50,
        0.6,
        10.0
    );

    // World
    HittableList world;

    // Ground
    auto checker = std::make_shared<CheckerTexture>(0.32, Color(.2, .3, .1), Color(.9, .9, .9));
    world.add(std::make_shared<Sphere>(Point3(0,-1000,0), 1000, std::make_shared<Lambertian>(checker)));

    // Random number generation
    static std::random_device rd;
    static std::mt19937 rng(rd());

    static std::uniform_real_distribution<double> position(-1.0, 1.0);
    static std::uniform_real_distribution<double> radius(0.15, 0.35);
    static std::uniform_real_distribution<double> color(0.0, 1.0);
    static std::uniform_real_distribution<double> materialChoice(0.0, 1.0);

    // Generate a grid of spheres
    constexpr int gridSize = 11;

    for (int a = -gridSize; a < gridSize; ++a) {
        for (int b = -gridSize; b < gridSize; ++b) {
            const double choose = materialChoice(rng);

            const Point3 center(
                static_cast<double>(a) + 0.9 * position(rng),
                0.2,
                static_cast<double>(b) + 0.9 * position(rng)
            );

            // Keep space around the three large spheres.
            if ((center - Point3(4.0, 0.2, 0.0)).length() < 0.9 ||
                (center - Point3(0.0, 0.2, 0.0)).length() < 0.9 ||
                (center - Point3(-4.0, 0.2, 0.0)).length() < 0.9) {
                continue;
            }

            const double r = radius(rng);

            std::shared_ptr<Material> material;

            if (choose < 0.6) {
                // Diffuse: multiply two random colors for natural variation.
                material = std::make_shared<Lambertian>(
                    Color(
                        color(rng) * color(rng),
                        color(rng) * color(rng),
                        color(rng) * color(rng)
                    )
                );
            }
            else if (choose < 0.8) {
                // Metal: bright, reflective surfaces with varying roughness.
                material = std::make_shared<Metal>(
                    Color(
                        0.5 + 0.5 * color(rng),
                        0.5 + 0.5 * color(rng),
                        0.5 + 0.5 * color(rng)
                    ),
                    color(rng) * 0.5
                );
            }
            else {
                // Glass
                material = std::make_shared<Dielectric>(1.5);
            }

            auto center2 = center + Vec3(0.0, r*0.5, 0.0);

            world.add(std::make_shared<Sphere>(center, center2, r, material));
        }
    }

    // Three large feature spheres
    auto material_center = std::make_shared<Dielectric>(1.5);
    auto material_left   = std::make_shared<Lambertian>(Color(0.4, 0.2, 0.1));
    auto material_right  = std::make_shared<Metal>(Color(0.7, 0.7, 0.7), 0.0);

    world.add(std::make_shared<Sphere>(
        Point3(0.0, 1.0, 0.0), 1.0, material_center
    ));

    world.add(std::make_shared<Sphere>(
        Point3(-4.0, 1.0, 0.0), 1.0, material_left
    ));

    world.add(std::make_shared<Sphere>(
        Point3(4.0, 1.0, 0.0), 1.0, material_right
    ));

    world = HittableList(std::make_shared<BVHNode>(world));

    // Render the image
    const unsigned char* data = cam.shot(world);

    // Save the image
    if (ImageWrite::isFormatAvailable(ImageWrite::Format::PNG)) {
        image.savePNG(data, cam.width(), cam.height(), "out/output.png", ImageWrite::PixelFormat::RGB24);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }
}


void checkered_spheres() {
    ImageWrite image{};

    const size_t width = 400ull;
    const size_t height = 225ull;

    HittableList world;

    auto checker1 = std::make_shared<CheckerTexture>(0.62, Color(.2, .3, .1), Color(.9, .9, .9));

    std::vector<Color> colors;
    colors.reserve(4);
    colors.emplace_back(0.0, 0.0, 0.0);
    colors.emplace_back(1.0, 0.0, 0.0);
    colors.emplace_back(0.0, 0.0, 1.0);
    colors.emplace_back(1.0, 1.0, 1.0);

    auto gradient = std::make_shared<MultiGradientTexture>(colors, Vec3(1.0, 0.2, 0.5), 4.0, 1);

    double a = 10.0;
    world.add(std::make_shared<Sphere>(Point3(0,-a, 0), a, std::make_shared<Lambertian>(checker1)));
    world.add(std::make_shared<Sphere>(Point3(0, a, 0), a, std::make_shared<Lambertian>(checker1)));

    world.add(std::make_shared<Sphere>(Point3(0.0, 0.0, std::sqrt(3.0) * a), a, std::make_shared<Lambertian>(gradient)));
    world.add(std::make_shared<Sphere>(Point3(0.0, 0.0, -std::sqrt(3.0) * a), a, std::make_shared<Lambertian>(gradient)));

    Camera cam(
        50.0,
        Point3(38.0, 2.0, 3.0),
        Point3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        300,
        50,
        0.0,
        10.0
    );

    const unsigned char* data = cam.shot(world);

    // Save the image
    if (ImageWrite::isFormatAvailable(ImageWrite::Format::PNG)) {
        image.savePNG(data, cam.width(), cam.height(), "out/output.png", ImageWrite::PixelFormat::RGB24);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }
}


void earth() {
    auto earth_texture = std::make_shared<ImageTexture>("images/0F9GANogspBRfY3sR-scaled.jpg");
    auto earth_surface = std::make_shared<Lambertian>(earth_texture);
    auto globe = std::make_shared<Sphere>(Point3(0,0,0), 2, earth_surface);

    ImageWrite image{};

    const size_t width = 400ull;
    const size_t height = 225ull;

    Camera cam(
        20.0,
        Point3(0.0, 0.0, 12.0),
        Point3(0.0, 0.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        300,
        50,
        0.0,
        10.0
    );

    HittableList world;
    world.add(globe);

    const unsigned char* data = cam.shot(world);

    // Save the image
    if (ImageWrite::isFormatAvailable(ImageWrite::Format::PNG)) {
        image.savePNG(data, cam.width(), cam.height(), "out/output.png", ImageWrite::PixelFormat::RGB24);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }
}


int main() {
    switch (3) {
        case 1: bouncing_spheres();  break;
        case 2: checkered_spheres(); break;
        case 3: earth();             break;
    }

    return 0;
}