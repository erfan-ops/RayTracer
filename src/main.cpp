#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <Sphere.hpp>


int main() {
    Image image{};

    const size_t width = 320;
    const size_t height = 180;

    // Create camera
    Camera cam(1.0, Point3(0.0, 0.0, 0.2), width, height, 100, 15);

    // World
    HittableList world;

    auto material_ground = std::make_shared<Lambertian>(Color(0.7, 0.9, 0.0));
    auto material_center = std::make_shared<Lambertian>(Color(0.5, 0.0, 0.8));
    auto material_left   = std::make_shared<Metal>(Color(0.75, 0.75, 0.75), 0.0);
    auto material_right  = std::make_shared<Metal>(Color(0.5, 0.5, 0.6), 0.5);

    world.add(std::make_shared<Sphere>(Point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(std::make_shared<Sphere>(Point3( 0.0,    0.0, -1.2),   0.5, material_center));
    world.add(std::make_shared<Sphere>(Point3(-1.0,    0.0, -1.0),   0.5, material_left));
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