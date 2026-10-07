#pragma once
#include <array>
#include <utility>
#include <vector>
namespace micro {
using Point3 = std::array<double, 3>;
// Independent CPU reference: clip the pixel footprint polygon against all six box planes.
// Inputs use cell-relative coordinates, matching a geometric unit cell.
inline bool clippedFootprintBox(std::array<Point3, 4> footprint, double half = .5001) {
    std::vector<Point3> polygon(footprint.begin(), footprint.end());
    for (int axis = 0; axis < 3; axis++)
        for (int sign : {-1, 1}) {
            if (polygon.empty())
                return false;
            std::vector<Point3> next;
            Point3 previous = polygon.back();
            double pd = half - sign * previous[axis];
            for (auto current : polygon) {
                double cd = half - sign * current[axis];
                if ((pd >= 0) != (cd >= 0)) {
                    double t = pd / (pd - cd);
                    Point3 crossing{};
                    for (int a = 0; a < 3; a++)
                        crossing[a] = previous[a] + t * (current[a] - previous[a]);
                    next.push_back(crossing);
                }
                if (cd >= 0)
                    next.push_back(current);
                previous = current;
                pd = cd;
            }
            polygon = std::move(next);
        }
    return !polygon.empty();
}
} // namespace micro
