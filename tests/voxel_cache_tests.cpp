#include "voxel_cache.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace micro;
int main() {
    try {
        Parameters p{};
        p.extent = {5, 5, 0, 0};
        p.flags.x = 1;
        Frame frame{};
        frame.vp = glm::mat4(1);
        std::array<glm::vec4, 25> positions{};
        std::map<std::array<int32_t, 3>, int> regions;
        Cell key{0, 0, 5, 0};
        glm::vec3 center(.05f, .05f, .55f);
        auto require = [](bool ok, const char *message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        require(!cacheSupported(key, center, .1f, p, frame, positions.data(), regions),
                "An empty source view must not retain a cached surface");
        positions[13] = glm::vec4(center + glm::vec3(.01f, 0, 0), 1);
        require(cacheSupported(key, center, .1f, p, frame, positions.data(), regions),
                "A neighbor must support a temporarily missing center sample");
        positions[13].z = .2f;
        require(!cacheSupported(key, center, .1f, p, frame, positions.data(), regions),
                "A different foreground depth must not support old geometry");
        positions[13] = glm::vec4(center, 1);
        require(!cacheSupported(key, {2, .05f, .55f}, .1f, p, frame, positions.data(), regions),
                "An offscreen cell must not persist");
        frame.vp[3][3] = -1;
        require(!cacheSupported(key, center, .1f, p, frame, positions.data(), regions),
                "A cell behind the camera must not persist");
        frame.vp = glm::mat4(1);
        p.flags.x = 2;
        regions[{0, 0, 2}] = 1;
        require(!cacheSupported(key, center, .1f, p, frame, positions.data(), regions),
                "An old child LOD must not overlap a current parent LOD");
        require(
            cacheSupported({0, 0, 2, 1}, {.1f, .1f, .5f}, .2f, p, frame, positions.data(), regions),
            "The current parent LOD must remain eligible");
        center = {-.05f, -.05f, .55f};
        positions[13] = glm::vec4(center, 1);
        regions.clear();
        regions[{-1, -1, 2}] = 0;
        require(cacheSupported({-1, -1, 5, 0}, center, .1f, p, frame, positions.data(), regions),
                "Negative world coordinates must use floor-aligned regions");
        regions.clear();
        require(!cacheSupported({-1, -1, 5, 0}, center, .1f, p, frame, positions.data(), regions),
                "An unsampled LOD region must not retain old geometry");
        p.temporal.x = .2f;
        require(cacheSupported({-1, -1, 5, 0}, center, .1f, p, frame, positions.data(), regions),
                "An unsampled jitter phase must retain eligible temporal history");
        p.flags.x = 1;
        frame.vp[2][3] = 1; // positive view depth depends on world z
        for (auto &sample : positions)
            sample = glm::vec4(0, 0, .1f, 1);
        require(!cacheSupported({0, 0, 5, 0}, {.05f, .05f, .55f}, .1f, p, frame, positions.data(),
                                regions),
                "A complete foreground neighborhood must reject occluded temporal history");
        positions[12] = glm::vec4(0);
        require(cacheSupported({0, 0, 5, 0}, {.05f, .05f, .55f}, .1f, p, frame, positions.data(),
                               regions),
                "A missing subpixel sample must not erase temporal history");
        std::cout
            << "Voxel cache: neighboring support, discontinuities, frustum and nested LOD passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
