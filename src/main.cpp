#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <Sphere.hpp>
#include <Quad.hpp>
#include <BVH.hpp>
#include <Texture.hpp>


void simple_light() {
    HittableList world;

    auto pertext = std::make_shared<NoiseTexture>(4);
    world.add(std::make_shared<Sphere>(Point3(0,-1000,0), 1000, std::make_shared<Lambertian>(pertext)));
    world.add(std::make_shared<Sphere>(Point3(0,2,0), 2, std::make_shared<Lambertian>(pertext)));

    auto difflight = std::make_shared<DiffuseLight>(Color(4,4,4));
    world.add(std::make_shared<Sphere>(Point3(0.0, 7.0, 0.0), 2.0, difflight));
    world.add(std::make_shared<Quad>(Point3(3,1,-2), Vec3(2,0,0), Vec3(0,2,0), difflight));

    ImageWrite image{};

    const size_t width = 400ull;
    const size_t height = 225ull;

    Camera cam(
        20.0,
        Point3(26.0, 3.0, 6.0),
        Point3(0.0, 2.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        100,
        50,
        0.0,
        10.0,
        Color(0.0, 0.0, 0.0)
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


void cornell_box() {
    HittableList world;

    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));

    world.add(std::make_shared<Quad>(Point3(555,0,0),     Vec3(0,555,0),  Vec3(0,0,555), green));
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(0,555,0),  Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(343,554,332), Vec3(-130,0,0), Vec3(0,0,-105), light));
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(555,0,0),  Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555),     Vec3(555,0,0),  Vec3(0,555,0), white));

    world.add(box(Point3(130, 0, 65), Point3(295, 165, 230), white));
    world.add(box(Point3(265, 0, 295), Point3(430, 330, 460), white));

    ImageWrite image{};

    const size_t width = 300ull;
    const size_t height = 300ull;

    Camera cam(
        40.0,
        Point3(278, 278, -800),
        Point3(278, 278, 0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        1200,
        50,
        0.0,
        10.0,
        Color(0.0, 0.0, 0.0)
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


int main() {
    switch (2) {
        case 1: simple_light(); break;
        case 2: cornell_box();  break;
    }

    return 0;
}