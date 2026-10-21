#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <hittables/Sphere.hpp>
#include <hittables/Quad.hpp>
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

    std::shared_ptr<Hittable> box1 = box(Point3(0,0,0), Point3(165,330,165), white);
    box1 = std::make_shared<RotateY>(box1, 15);
    box1 = std::make_shared<Translate>(box1, Vec3(265,0,295));
    world.add(box1);

    std::shared_ptr<Hittable> box2 = box(Point3(0,0,0), Point3(165,165,165), white);
    box2 = std::make_shared<RotateY>(box2, -18);
    box2 = std::make_shared<Translate>(box2, Vec3(130,0,65));
    world.add(box2);

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
        200,
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

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i*w;
            auto z0 = -1000.0 + j*w;
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

    auto center1 = Point3(400, 400, 200);
    auto center2 = center1 + Vec3(30,0,0);
    auto sphere_material = std::make_shared<Lambertian>(Color(0.7, 0.3, 0.1));
    world.add(std::make_shared<Sphere>(center1, center2, 50, sphere_material));

    world.add(std::make_shared<Sphere>(Point3(260, 150, 45), 50, std::make_shared<Dielectric>(1.5)));
    world.add(std::make_shared<Sphere>(
        Point3(0, 150, 145), 50, std::make_shared<Metal>(Color(0.8, 0.8, 0.9), 1.0)
    ));

    auto boundary = std::make_shared<Sphere>(Point3(360,150,145), 70, std::make_shared<Dielectric>(1.5));
    world.add(boundary);
    world.add(std::make_shared<ConstantMedium>(boundary, 0.2, Color(0.2, 0.4, 0.9)));
    boundary = std::make_shared<Sphere>(Point3(0,0,0), 5000, std::make_shared<Dielectric>(1.5));
    world.add(std::make_shared<ConstantMedium>(boundary, .0001, Color(1,1,1)));

    auto emat = std::make_shared<Lambertian>(std::make_shared<ImageTexture>("images/earthmap.jpg"));
    world.add(std::make_shared<Sphere>(Point3(400,200,400), 100, emat));
    auto pertext = std::make_shared<NoiseTexture>(0.2);
    world.add(std::make_shared<Sphere>(Point3(220,280,300), 80, std::make_shared<Lambertian>(pertext)));

    HittableList boxes2;
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(std::make_shared<Sphere>(Point3::random(0,165), 10, white));
    }

    world.add(std::make_shared<Translate>(
        std::make_shared<RotateY>(
            std::make_shared<BVHNode>(boxes2), 15),
            Vec3(-100,270,395)
        )
    );

    ImageWrite image{};

    const size_t width = image_width;
    const size_t height = image_width;

    Camera cam(
        40.0,
        Point3(478, 278, -600),
        Point3(278, 278, 0),
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
    switch (4) {
        case 1: simple_light();                break;
        case 2: cornell_box();                 break;
        case 3: cornell_smoke();               break;
        case 4: final_scene(800, 10000, 50);   break;
    }

    return 0;
}