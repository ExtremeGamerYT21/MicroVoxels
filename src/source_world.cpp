#include "source_world.hpp"
#include <cmath>
#include <glm/gtc/constants.hpp>
namespace micro {
void SourceWorld::triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float bend,
                           float gloss) {
    glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    for (auto p : {a, b, c})
        vertices.push_back({p, n, color, {bend, gloss}});
}
void SourceWorld::sphere(glm::vec3 center, glm::vec3 scale, glm::vec3 color, int rings, int sectors,
                         bool smooth, float gloss) {
    auto point = [&](int y, int x) {
        float a = glm::pi<float>() * y / rings, b = glm::two_pi<float>() * x / sectors;
        return glm::vec3(std::sin(a) * std::cos(b), std::cos(a), std::sin(a) * std::sin(b));
    };
    for (int y = 0; y < rings; y++)
        for (int x = 0; x < sectors; x++)
            for (int t = 0; t < 2; t++) {
                glm::vec3 a = point(y, x), b = point(y + 1, x), c = point(y + 1, x + 1),
                          d = point(y, x + 1);
                glm::vec3 p[3];
                if (t == 0) {
                    p[0] = a;
                    p[1] = c;
                    p[2] = b;
                } else {
                    p[0] = a;
                    p[1] = d;
                    p[2] = c;
                }
                if (glm::length(glm::cross(p[1] - p[0], p[2] - p[0])) < .000001f)
                    continue;
                glm::vec3 face =
                    glm::normalize(glm::cross(scale * (p[1] - p[0]), scale * (p[2] - p[0])));
                for (auto q : p)
                    vertices.push_back({center + scale * q,
                                        smooth ? glm::normalize(q / scale) : face,
                                        color,
                                        {0, gloss}});
            }
}
void SourceWorld::cylinder(glm::vec3 a, glm::vec3 b, float radius, glm::vec3 color, float bend) {
    glm::vec3 axis = glm::normalize(b - a),
              u = glm::normalize(glm::cross(axis, std::abs(axis.y) > .9f ? glm::vec3(1, 0, 0)
                                                                         : glm::vec3(0, 1, 0))),
              v = glm::cross(axis, u);
    for (int i = 0; i < 10; i++) {
        float x = glm::two_pi<float>() * i / 10, y = glm::two_pi<float>() * (i + 1) / 10;
        glm::vec3 p = radius * (u * std::cos(x) + v * std::sin(x)),
                  q = radius * (u * std::cos(y) + v * std::sin(y));
        triangle(a + p, b + p, b + q, color, bend);
        triangle(a + p, b + q, a + q, color, bend);
        triangle(b, b + p, b + q, color, bend);
        triangle(a, a + q, a + p, color, bend);
    }
}
SourceWorld::SourceWorld() {
    triangle({-7, 0, -7}, {7, 0, 7}, {7, 0, -7}, {.31, .36, .40}, 0, -1);
    triangle({-7, 0, -7}, {-7, 0, 7}, {7, 0, 7}, {.31, .36, .40}, 0, -1);
    sphere({-1.6, 1.05, 0}, {1, 1, 1}, {.78, .22, .09}, 24, 48, true, 100);
    sphere({1.1, 1.05, -.8}, {.8, 1.1, .8}, {.08, .50, .72}, 5, 7, false, 64);
    sphere({-.6, .30, 2}, {.65, .32, .44}, {.37, .36, .33}, 5, 9, false, 16);
    sphere({2.4, .28, 1.9}, {.47, .30, .62}, {.38, .44, .40}, 4, 7, false, 16);
    sphere({-3, .25, -1.9}, {.46, .28, .39}, {.34, .34, .39}, 4, 8, false, 16);
    glm::vec3 origin = {2.65, 0, -2.0};
    cylinder(origin, origin + glm::vec3(0, 2.2, 0), .14, {.30, .16, .07}, 1);
    for (int j = 0; j < 7; j++) {
        float a = j * 2.39996f;
        float y = .85f + j * .19f;
        glm::vec3 start = origin + glm::vec3(0, y, 0),
                  end = origin + glm::vec3(.8f * std::cos(a), y + .65f, .8f * std::sin(a));
        cylinder(start, end, .04, {.32, .20, .09}, 1);
        glm::vec3 side = {.45f * std::cos(a + 1.57f), 0, .45f * std::sin(a + 1.57f)};
        // Curved leaf strip with smooth interpolated normals; all motion occurs in source shading.
        for (int k = 0; k < 8; k++) {
            float t = k / 8.f, next = (k + 1) / 8.f;
            auto point = [&](float s, float sign) {
                return start + (end - start) * (s * 1.5f) +
                       side * (std::sin(s * glm::pi<float>()) * sign) +
                       glm::vec3(0, .22f * std::sin(s * glm::pi<float>()), 0);
            };
            glm::vec3 p[4] = {point(t, -1), point(t, 1), point(next, 1), point(next, -1)};
            for (int index : {0, 1, 2, 0, 2, 3}) {
                float s = index < 2 ? t : next;
                glm::vec3 normal = glm::normalize(
                    glm::vec3(.15f * std::cos(a), 1, .35f * std::sin(s * glm::pi<float>())));
                glm::vec3 green = glm::mix(glm::vec3(.04, .20, .025), glm::vec3(.22, .57, .06), s);
                vertices.push_back({p[index], normal, green, {1, 36}});
            }
        }
    }
}
} // namespace micro
