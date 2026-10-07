#include "triangle_overlap.hpp"
#include <cstdlib>
#include <iostream>
void require(bool ok, const char *message) {
    if (!ok) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
int main() {
    using namespace micro;
    require(clippedTriangleBox({Point3{-2, 0, -2}, Point3{2, 0, -2}, Point3{0, 0, 2}}),
            "Large triangle must fill an interior cell with no vertex inside it");
    require(!clippedTriangleBox({Point3{.65, .45, 0}, Point3{.45, .65, 0}, Point3{.8, .8, 0}}, .5),
            "Overlapping bounds alone must not imply triangle overlap");
    require(clippedTriangleBox({Point3{-.2, .5, -.2}, Point3{.2, .5, -.2}, Point3{0, .5, .2}}, .5),
            "Triangle touching a box face must intersect");
    require(
        !clippedTriangleBox({Point3{-.2, .51, -.2}, Point3{.2, .51, -.2}, Point3{0, .51, .2}}, .5),
        "Separated plane must not intersect");
    require(clippedTriangleBox(
                {Point3{-5, -.001, -.001}, Point3{5, .001, .001}, Point3{5, .002, .001}}, .5),
            "Thin diagonal must survive clipping");
    require(
        clippedTriangleBox({Point3{-.5, -.5, -.5}, Point3{-1, -.5, -.5}, Point3{-.5, -1, -.5}}, .5),
        "Negative corner contact must intersect");
    std::cout
        << "Triangle interior, bounds rejection, face/corner contacts and thin surfaces passed\n";
}
