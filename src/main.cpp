#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <hittables/Sphere.hpp>
#include <hittables/Quad.hpp>
#include <hittables/Triangle.hpp>
#include <hittables/ConstantMedium.hpp>
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

    const size_t width = 1280ull;
    const size_t height = 720ull;

    Camera cam(
        20.0,
        Point3(26.0, 3.0, 6.0),
        Point3(0.0, 2.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        800,
        80,
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
    world.add(std::make_shared<Quad>(Point3(343,554,332), Vec3(-130,0,0), Vec3(0,0,-105), light)); // light source
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(555,0,0),  Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555),     Vec3(555,0,0),  Vec3(0,555,0), white));

    std::shared_ptr<Hittable> box1 = box(Point3(0,165,0), Point3(165,330,165), white);
    box1 = std::make_shared<RotateY>(box1, 15);
    box1 = std::make_shared<Translate>(box1, Vec3(265,0,295));
    world.add(box1);

    std::shared_ptr<Hittable> box2 = box(Point3(0,0,0), Point3(165,165,165), white);
    box2 = std::make_shared<RotateY>(box2, -18);
    box2 = std::make_shared<Translate>(box2, Vec3(130,0,65));
    world.add(box2);

    double r = 50;
    world.add(std::make_shared<Sphere>(Point3(165, 165 + r, 165), r, std::make_shared<Dielectric>(1.5)));

    r = 150;
    world.add(std::make_shared<Sphere>(Point3(265+82.5, 247.5, 295+82.5), r, std::make_shared<Dielectric>(1.5)));

    ImageWrite image{};

    const size_t width = 500ull;
    const size_t height = 500ull;

    Camera cam(
        40.0,
        Point3(278, 278, -800),
        Point3(278, 278, 0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        500,
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


void cornell_box2() {
    HittableList world;

    auto red    = std::make_shared<Lambertian>(Color(.75, .05, .05));
    auto purple = std::make_shared<Lambertian>(Color(.4, .05, .7));
    auto white  = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto black  = std::make_shared<Lambertian>(Color(.03, .03, .03));
    auto green  = std::make_shared<Lambertian>(Color(.05, .55, .05));
    auto light  = std::make_shared<DiffuseLight>(Color(40, 20, 15));
    auto minilight  = std::make_shared<DiffuseLight>(Color(200, 90, 50));
    auto backwall = std::make_shared<Metal>(Color(0.2, 0.2, 0.2), 0.0);

    world.add(std::make_shared<Quad>(Point3(555,0,0),     Vec3(0,555,0),  Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(0,555,0),  Vec3(0,0,555), purple));
    world.add(std::make_shared<Quad>(Point3(343,554,332), Vec3(-130,0,0), Vec3(0,0,-105), light, 20)); // light source
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(555,0,0),  Vec3(0,0,555), green));
    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555),     Vec3(555,0,0),  Vec3(0,555,0), backwall));
    // world.add(std::make_shared<Quad>(Point3(-100, -100, -700),     Vec3(700,0,0),  Vec3(0,700,0), white));

    double r = 12;
    world.add(std::make_shared<Sphere>(Point3(16, 350, 277.5), r, minilight));
    world.add(std::make_shared<Sphere>(Point3(555-16, 350, 277.5), r, minilight));

    double lamp_ratio = 2;
    world.add(std::make_shared<Quad>(Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio), Vec3(0,r*4.0*lamp_ratio,0),  Vec3(0,0,r*2.0*lamp_ratio), black));
    world.add(std::make_shared<Triangle>(
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3((r*2.0 + 10) - (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        black
    ));
    world.add(std::make_shared<Triangle>(
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3((r*2.0 + 10) - (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        black
    ));


    world.add(std::make_shared<Quad>(Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio), Vec3(0,r*4.0*lamp_ratio,0),  Vec3(0,0,r*2.0*lamp_ratio), black));
    world.add(std::make_shared<Triangle>(
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3((555 - (r*2.0 + 10)) + (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        black
    ));
    world.add(std::make_shared<Triangle>(
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3((555 - (r*2.0 + 10)) + (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        black
    ));


    auto box1Mat = std::make_shared<Metal>(Color(0.30, 0.30, 0.30), 0.3);
    std::shared_ptr<Hittable> box1 = box(Point3(0,0,0), Point3(165,165,165), box1Mat);
    box1 = std::make_shared<RotateY>(box1, 25);
    box1 = std::make_shared<Translate>(box1, Vec3(265,0,200));
    world.add(box1);

    auto ballMat = std::make_shared<Metal>(Color(0.35, 0.35, 0.35), 0.0);
    Point3 localCenter(82.5, 165.0 + 82.5, 82.5);

    // Rotate the local point by 30 degrees, then translate.
    double angle = degrees_to_radians(30.0);

    Point3 sphereCenter(
        265 + std::cos(angle) * localCenter.x() +
            std::sin(angle) * localCenter.z(),
        localCenter.y(),
        200 - std::sin(angle) * localCenter.x() +
            std::cos(angle) * localCenter.z()
    );
    world.add(std::make_shared<Sphere>(sphereCenter, 82.5, std::make_shared<Dielectric>(1.5)));


    world.add(std::make_shared<Sphere>(Point3(150,70,145), 70, ballMat));
    world.add(std::make_shared<Sphere>(Point3(300,20,100), 20, std::make_shared<Lambertian>(Color(1.0, 1.0, 0.8))));


    ImageWrite image{};

    const size_t width = 400ull;
    const size_t height = 400ull;

    Camera cam(
        45.0,
        Point3(278, 278, -650),
        Point3(278, 278, 500),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        500,
        50,
        0.0,
        100.0,
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


void cornell_smoke() {
    HittableList world;

    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));

    world.add(std::make_shared<Quad>(Point3(555,0,0), Vec3(0,555,0), Vec3(0,0,555), green));
    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(0,555,0), Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(113,554,127), Vec3(330,0,0), Vec3(0,0,305), light));
    world.add(std::make_shared<Quad>(Point3(0,555,0), Vec3(555,0,0), Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(555,0,0), Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555), Vec3(555,0,0), Vec3(0,555,0), white));

    std::shared_ptr<Hittable> box1 = box(Point3(0,0,0), Point3(165,330,165), white);
    box1 = make_shared<RotateY>(box1, 15);
    box1 = make_shared<Translate>(box1, Vec3(265,0,295));

    std::shared_ptr<Hittable> box2 = box(Point3(0,0,0), Point3(165,165,165), white);
    box2 = make_shared<RotateY>(box2, -18);
    box2 = make_shared<Translate>(box2, Vec3(130,0,65));

    world.add(std::make_shared<ConstantMedium>(box1, 0.05, Color(0,0,0)));
    world.add(std::make_shared<ConstantMedium>(box2, 0.05, Color(1,1,1)));

    ImageWrite image{};

    const size_t width = 600ull;
    const size_t height = 600ull;

    Camera cam(
        40.0,
        Point3(278, 278, -800),
        Point3(278, 278, 0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        500,
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


void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    HittableList boxes1;
    auto ground = std::make_shared<Lambertian>(Color(0.48, 0.83, 0.53));

    int boxes_per_side = 9;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -200.0 + i*w;
            auto z0 = -400.0 + j*w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1,101);
            auto z1 = z0 + w;

            boxes1.add(box(Point3(x0,y0,z0), Point3(x1,y1,z1), ground));
        }
    }

    HittableList world;

    world.add(std::make_shared<BVHNode>(boxes1));

    auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));
    world.add(std::make_shared<Quad>(Point3(123,554,147), Vec3(300,0,0), Vec3(0,0,265), light));

    world.add(std::make_shared<Sphere>(Point3(260, 150, 45), 50, std::make_shared<Dielectric>(1.5)));

    ImageWrite image{};

    const size_t width = image_width;
    const size_t height = image_width;

    Camera cam(
        30.0,
        Point3(478, 278, -600),
        Point3(260, 150, 45),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        samples_per_pixel,
        max_depth,
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
    switch (5) {
        case 1: simple_light();               break;
        case 2: cornell_box();                break;
        case 5: cornell_box2();                break;
        case 3: cornell_smoke();              break;
        case 4: final_scene(1080, 1080, 108); break;
    }

    return 0;
}