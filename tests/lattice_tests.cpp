#include "lattice.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
void require(bool ok, const char *msg) {
    if (!ok) {
        std::cerr << msg << '\n';
        std::exit(1);
    }
}
int main() {
    using namespace micro;
    require(cell({-.001f, 0.f, -.1f}, .01f, 0) == Cell{-1, 0, -10, 0},
            "Negative positions must use floor");
    for (float x : {-1.237f, -.051f, .023f, 2.619f})
        for (int l = 0; l < 5; ++l) {
            auto fine = cell({x, x, x}, .005f, l), coarse = cell({x, x, x}, .005f, l + 1);
            require(int(std::floor(fine[0] / 2.0)) == coarse[0], "Nested cells disagree");
            auto c = center(fine, .005f);
            float h = std::ldexp(.005f, l);
            require(c[0] - h * .5f <= x + .00001f && x < c[0] + h * .5f + .00001f,
                    "Cell does not contain hit");
        }
    require(chooseLod(8.5f, 8.f, 5, 1) == 1, "Coarse LOD must persist inside hysteresis band");
    require(chooseLod(8.5f, 8.f, 5, 0) == 0, "Fine LOD must persist inside hysteresis band");
    require(chooseLod(7.f, 8.f, 5, 1) == 0, "Coarse should refine below lower threshold");
    require(chooseLod(9.f, 8.f, 5, 0) == 1, "Fine should coarsen above upper threshold");
    require(chooseLod(100.f, 8.f, 3, 0) == 2, "Large camera jumps must traverse all thresholds");
    std::cout << "Negative coordinates, nesting, occupancy, hysteresis and camera jumps passed\n";
}
