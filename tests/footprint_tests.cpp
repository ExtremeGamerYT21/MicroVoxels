#include "surface_footprint.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    using namespace micro;
    Parameters p{};
    p.extent = {3, 3, 0, 0};
    p.footprint = {1, 27, 0, 0};
    Frame f{};
    f.vp = glm::perspective(glm::radians(55.f), 1.f, .05f, 100.f);
    f.options.z = 1;
    std::array<glm::vec4, 9> positions{};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            positions[y * 3 + x] = {.02f * (x - 1), .02f * (y - 1), -5, 1};
    auto flat = sourceFootprint(positions.data(), 4, p, f);
    require(glm::length(flat.dx - glm::vec3(.02f, 0, 0)) < 1e-6f &&
                glm::length(flat.dy - glm::vec3(0, .02f, 0)) < 1e-6f,
            "A flat raster patch must retain its screen derivatives");
    positions[5] = {.02f, 0, -6, 1};
    auto edge = sourceFootprint(positions.data(), 4, p, f);
    require(edge.rejected == 1 && glm::length(edge.dx - flat.dx) < 1e-6f,
            "A foreground/floor depth jump must use only the continuous side");
    positions[3] = {-.02f, 0, -6, 1};
    positions[1] = positions[7] = glm::vec4(0);
    auto isolated = sourceFootprint(positions.data(), 4, p, f);
    require(glm::length(isolated.dx) == 0 && glm::length(isolated.dy) == 0,
            "An isolated foreground sample must fall back to a point");
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            positions[y * 3 + x] = {.02f * (x - 1), .02f * (y - 1), -5, 1};
    auto small = boundedFootprint(positions.data(), 4, p, f, .005f);
    require(small.clamped && glm::length(small.dx) <= .010001f,
            "A footprint must obey the configured cell-radius limit");
    p.footprint.x = 0;
    require(glm::length(boundedFootprint(positions.data(), 4, p, f, .005f).dx) == 0,
            "Radius zero must produce point splats");
    SurfaceFootprint diagonal;
    diagonal.dx = {2, 2, 0};
    diagonal.dy = {0, 0, .1f};
    require(referenceFootprintBox({0, 0, 0}, diagonal, {0, 0, 0}, 1),
            "A cell intersecting the interior of a footprint must be retained");
    require(!referenceFootprintBox({0, 0, 0}, diagonal, {1, -1, 0}, 1),
            "Overlapping bounds alone must not fill cells outside the footprint plane");
    std::cout << "Raster derivatives, discontinuities, point fallback, radius bounds and footprint "
                 "clipping passed\n";
}
