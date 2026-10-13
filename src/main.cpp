#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <Sphere.hpp>


int main() {
    Image image{};

    const size_t width = 640ull;
    const size_t height = 360ull;

    // Create camera
    Camera cam(
        60.0,
        Point3(-2.0, 2.0, 1.0),
        Point3(0.0, 0.0, -1.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        80,
        10
    );

    // World
    HittableList world;

    auto material_ground = std::make_shared<Lambertian>(Color(0.7, 0.9, 0.0));
    auto material_center = std::make_shared<Lambertian>(Color(0.5, 0.0, 0.8));
    auto material_left   = std::make_shared<Dielectric>(1.5);
    auto material_bubble = std::make_shared<Dielectric>(1.0 / 1.5);
    auto material_right  = std::make_shared<Metal>(Color(0.5, 0.5, 0.6), 0.5);

    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, material_center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, material_left));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.4, material_bubble));
    world.add(std::make_shared<Sphere>(Point3( 1.0,    0.0, -1.0),   0.5, material_right));

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