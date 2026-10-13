#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <Sphere.hpp>


int main() {
    Image image{};

    const size_t width = 1280ull;
    const size_t height = 720ull;

    // Create camera
    Camera cam(
        40.0,                           // verticalFOV
        Point3(13.0, 2.0, 3.0),        // lookfrom
        Point3(0.0, 0.0, 0.0),         // lookat
        Vec3(0.0, 1.0, 0.0),           // vup
        width,
        height,
        100,                            // samplesPerPixel
        20                              // maxDepth
    );

    // World
    HittableList world;

    // Ground
    auto material_ground = std::make_shared<Lambertian>(Color(0.5, 0.5, 0.5));
    world.add(std::make_shared<Sphere>(
        Point3(0.0, -1000.0, 0.0),
        1000.0,
        material_ground
    ));

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

            if (choose < 0.7) {
                // Diffuse: multiply two random colors for natural variation.
                material = std::make_shared<Lambertian>(
                    Color(
                        color(rng) * color(rng),
                        color(rng) * color(rng),
                        color(rng) * color(rng)
                    )
                );
            }
            else if (choose < 0.9) {
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

            world.add(std::make_shared<Sphere>(center, r, material));
        }
    }

    // Three large feature spheres
    auto material_center = std::make_shared<Dielectric>(1.5);
    auto material_left   = std::make_shared<Lambertian>(Color(0.4, 0.2, 0.1));
    auto material_right  = std::make_shared<Metal>(Color(0.7, 0.6, 0.5), 0.0);

    world.add(std::make_shared<Sphere>(
        Point3(0.0, 1.0, 0.0), 1.0, material_center
    ));

    world.add(std::make_shared<Sphere>(
        Point3(-4.0, 1.0, 0.0), 1.0, material_left
    ));

    world.add(std::make_shared<Sphere>(
        Point3(4.0, 1.0, 0.0), 1.0, material_right
    ));

    // Render the image
    const unsigned char* data = cam.shot(world);

    // Save the image
    if (Image::isFormatAvailable(Image::Format::PNG)) {
        image.savePNG(data, cam.width(), cam.height(), "out/output.png", Image::PixelFormat::RGB24);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }

    return 0;
}