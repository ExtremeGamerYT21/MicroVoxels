#include "source_world.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
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
SourceWorld::SourceWorld(SourceScene scene) {
    if (scene == SourceScene::Test)
        buildTest();
    else
        buildGarden();
}
void SourceWorld::quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color,
                       float gloss) {
    triangle(a, b, c, color, 0, gloss);
    triangle(a, c, d, color, 0, gloss);
}
void SourceWorld::box(glm::vec3 l, glm::vec3 h, glm::vec3 color, float gloss) {
    quad({l.x, l.y, h.z}, {h.x, l.y, h.z}, {h.x, h.y, h.z}, {l.x, h.y, h.z}, color, gloss);
    quad({l.x, l.y, l.z}, {l.x, h.y, l.z}, {h.x, h.y, l.z}, {h.x, l.y, l.z}, color, gloss);
    quad({l.x, l.y, l.z}, {l.x, l.y, h.z}, {l.x, h.y, h.z}, {l.x, h.y, l.z}, color, gloss);
    quad({h.x, l.y, h.z}, {h.x, l.y, l.z}, {h.x, h.y, l.z}, {h.x, h.y, h.z}, color, gloss);
    quad({l.x, h.y, h.z}, {h.x, h.y, h.z}, {h.x, h.y, l.z}, {l.x, h.y, l.z}, color, gloss);
    quad({l.x, l.y, l.z}, {h.x, l.y, l.z}, {h.x, l.y, h.z}, {l.x, l.y, h.z}, color, gloss);
}
void SourceWorld::buildTest() {
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

namespace {
// All source geometry is deterministic; camera movement never regenerates it.
struct GardenRandom {
    uint32_t state = 0x4d56584cu;
    float next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return float(state >> 8) / 16777216.f;
    }
};
float groundHeight(float x, float z) {
    float hills = .13f * std::sin(x * .67f) * std::cos(z * .52f) +
                  .68f * std::exp(-((x + 4.1f) * (x + 4.1f) + (z + 3.5f) * (z + 3.5f)) / 9.f) +
                  .36f * std::exp(-((x - 4.5f) * (x - 4.5f) + (z - 2.1f) * (z - 2.1f)) / 7.f);
    // Blend a level building pad into the surrounding terrain.
    float dx = std::max(std::abs(x + .25f) - 2.25f, 0.f);
    float dz = std::max(std::abs(z + 1.5f) - 1.95f, 0.f);
    float t = std::clamp(std::sqrt(dx * dx + dz * dz) / 1.1f, 0.f, 1.f);
    return hills * t * t * (3 - 2 * t);
}
float pathCenter(float z) {
    return -.25f + .72f * std::sin(std::max(z, 0.f) * .72f);
}
} // namespace

void SourceWorld::buildGarden() {
    vertices.reserve(150000);
    GardenRandom random;
    constexpr float edge = 7.f;
    constexpr int divisions = 72;
    auto groundPoint = [](float x, float z) { return glm::vec3(x, groundHeight(x, z), z); };
    auto terrainVertex = [&](float x, float z) {
        constexpr float e = .015f;
        glm::vec3 n = glm::normalize(glm::vec3(
            -(groundHeight(x + e, z) - groundHeight(x - e, z)) / (2 * e), 1,
            -(groundHeight(x, z + e) - groundHeight(x, z - e)) / (2 * e)));
        float variation = .5f + .5f * std::sin(1.8f * x + .7f * z) * std::cos(1.4f * z - .3f * x);
        glm::vec3 color = glm::mix(glm::vec3(.11f, .23f, .055f), glm::vec3(.25f, .36f, .095f),
                                   variation);
        if (z > -.05f && z < 6.8f) {
            float path = 1 - glm::smoothstep(.43f, .66f, std::abs(x - pathCenter(z)));
            color = glm::mix(color, glm::vec3(.40f, .30f, .18f), path);
        }
        return Vertex{groundPoint(x, z), n, color, {0, 96}};
    };
    // A smooth triangle heightfield, with a shallow earth skirt around its finite edge.
    for (int z = 0; z < divisions; z++)
        for (int x = 0; x < divisions; x++) {
            float x0 = -edge + 2 * edge * x / divisions, x1 = -edge + 2 * edge * (x + 1) / divisions;
            float z0 = -edge + 2 * edge * z / divisions, z1 = -edge + 2 * edge * (z + 1) / divisions;
            Vertex p[] = {terrainVertex(x0, z0), terrainVertex(x0, z1),
                          terrainVertex(x1, z1), terrainVertex(x1, z0)};
            for (int i : {0, 1, 2, 0, 2, 3})
                vertices.push_back(p[i]);
        }
    for (int i = 0; i < divisions; i++) {
        float a = -edge + 2 * edge * i / divisions, b = -edge + 2 * edge * (i + 1) / divisions;
        glm::vec3 earth(.25f, .18f, .105f);
        quad(groundPoint(a, edge), {a, -.8f, edge}, {b, -.8f, edge}, groundPoint(b, edge), earth);
        quad(groundPoint(b, -edge), {b, -.8f, -edge}, {a, -.8f, -edge}, groundPoint(a, -edge), earth);
        quad(groundPoint(edge, b), {edge, -.8f, b}, {edge, -.8f, a}, groundPoint(edge, a), earth);
        quad(groundPoint(-edge, a), {-edge, -.8f, a}, {-edge, -.8f, b}, groundPoint(-edge, b), earth);
    }

    glm::vec3 plaster(.78f, .68f, .48f), trim(.88f, .83f, .68f), wood(.30f, .15f, .065f);
    box({-2.05f, -.08f, -3.05f}, {1.55f, .20f, .05f}, {.33f, .31f, .26f});
    box({-2, .20f, -3}, {1.5f, 2.45f, 0}, plaster);
    triangle({-2, 2.45f, 0}, {1.5f, 2.45f, 0}, {-.25f, 3.65f, 0}, plaster);
    triangle({1.5f, 2.45f, -3}, {-2, 2.45f, -3}, {-.25f, 3.65f, -3}, plaster);
    for (float x : {-2.025f, 1.465f})
        box({x, .2f, -.035f}, {x + .065f, 2.46f, .065f}, wood);
    box({-2.05f, .24f, .012f}, {1.55f, .36f, .055f}, wood);

    // Opaque coloured window panes: lighting still happens only in the source pass.
    auto frontWindow = [&](float x, float y) {
        constexpr float w = .66f, h = .72f, f = .055f;
        box({x - w / 2, y - h / 2, .02f}, {x + w / 2, y + h / 2, .05f}, {.065f, .19f, .28f}, 160);
        for (float side : {-1.f, 1.f}) {
            float xx = x + side * w / 2, yy = y + side * h / 2;
            box({xx - f / 2, y - h / 2 - f, .055f}, {xx + f / 2, y + h / 2 + f, .115f}, trim);
            box({x - w / 2 - f, yy - f / 2, .055f}, {x + w / 2 + f, yy + f / 2, .115f}, trim);
        }
        box({x - .018f, y - h / 2, .058f}, {x + .018f, y + h / 2, .10f}, trim);
        box({x - w / 2, y - .018f, .058f}, {x + w / 2, y + .018f, .10f}, trim);
        box({x - .42f, y - h / 2 - .10f, -.01f}, {x + .42f, y - h / 2 - .04f, .18f}, wood);
    };
    frontWindow(-1.35f, 1.45f);
    frontWindow(.88f, 1.45f);
    for (float z : {-1.f, -2.3f}) {
        box({1.52f, 1.02f, z - .35f}, {1.55f, 1.82f, z + .35f}, {.065f, .19f, .28f}, 160);
        for (float side : {-1.f, 1.f}) {
            box({1.56f, 1.00f, z + side * .35f - .035f},
                {1.64f, 1.85f, z + side * .35f + .035f}, trim);
            float y = side < 0 ? 1.02f : 1.82f;
            box({1.56f, y - .035f, z - .39f}, {1.64f, y + .035f, z + .39f}, trim);
        }
        box({1.56f, 1.03f, z - .02f}, {1.61f, 1.81f, z + .02f}, trim);
    }
    for (int i = 0; i < 5; i++) {
        float x = -.61f + i * .144f;
        box({x, .20f, .022f}, {x + .138f, 1.98f, .08f}, wood * (.86f + .06f * i));
    }
    box({-.69f, .19f, .08f}, {-.61f, 2.07f, .14f}, trim);
    box({.11f, .19f, .08f}, {.19f, 2.07f, .14f}, trim);
    box({-.69f, 1.99f, .08f}, {.19f, 2.07f, .14f}, trim);
    sphere({.035f, 1.02f, .13f}, {.033f, .033f, .035f}, {.69f, .45f, .13f}, 3, 6, true, 96);
    box({-.84f, .01f, .03f}, {.34f, .12f, .43f}, {.41f, .39f, .32f});

    // Individual coplanar roof tiles supply material variation without textures.
    for (int side : {-1, 1}) {
        auto roofPoint = [&](float t, float z) {
            return glm::vec3(-.25f + side * 2.02f * t, 3.67f - 1.29f * t, z);
        };
        for (int row = 0; row < 12; row++)
            for (int tile = 0; tile < 18; tile++) {
                float t0 = row / 12.f, t1 = (row + 1) / 12.f;
                float z0 = -3.23f + 3.46f * tile / 18, z1 = -3.23f + 3.46f * (tile + 1) / 18;
                glm::vec3 color = glm::mix(glm::vec3(.34f, .09f, .045f), glm::vec3(.61f, .21f, .08f),
                                           random.next());
                quad(roofPoint(t0, z0), roofPoint(t1, z0), roofPoint(t1, z1), roofPoint(t0, z1),
                     color, 64);
            }
        for (float z : {-.245f - 3.f, .245f})
            cylinder(roofPoint(0, z), roofPoint(1, z), .045f, trim);
        cylinder(roofPoint(1, -3.25f), roofPoint(1, .25f), .045f, wood);
    }
    cylinder({-.25f, 3.68f, -3.24f}, {-.25f, 3.68f, .24f}, .065f, {.42f, .12f, .055f});
    box({.42f, 2.75f, -2.35f}, {.87f, 4.05f, -1.91f}, {.44f, .24f, .16f});
    for (int row = 0; row < 6; row++)
        box({.415f, 2.84f + row * .18f, -2.36f}, {.88f, 2.86f + row * .18f, -1.90f},
            {.60f, .43f, .28f});
    box({.35f, 4.05f, -2.42f}, {.94f, 4.16f, -1.84f}, {.27f, .24f, .20f});
    box({.46f, 4.161f, -2.31f}, {.83f, 4.17f, -1.95f}, {.045f, .04f, .035f});

    for (int i = 0; i < 14; i++) {
        float z = .7f + i * .40f, x = pathCenter(z) + (random.next() - .5f) * .18f;
        float shade = .37f + random.next() * .14f;
        sphere({x, groundHeight(x, z) + .025f, z}, {.30f, .065f, .20f},
               {shade, shade * .96f, shade * .82f}, 3, 8, false, 64);
    }

    // Curved grass ribbons with tapered tips. No alpha test or texture is used.
    constexpr int grassGrid = 60;
    for (int z = 0; z < grassGrid; z++)
        for (int x = 0; x < grassGrid; x++) {
            float px = -6.7f + 13.4f * (x + random.next()) / grassGrid;
            float pz = -6.7f + 13.4f * (z + random.next()) / grassGrid;
            if (random.next() > .60f || (px > -2.25f && px < 1.8f && pz > -3.3f && pz < .6f) ||
                (pz > 0 && std::abs(px - pathCenter(pz)) < .68f) ||
                (px > 2.4f && px < 4.1f && pz > .1f && pz < 2.2f))
                continue;
            for (int blade = 0; blade < 3; blade++) {
                float angle = random.next() * glm::two_pi<float>(), height = .16f + .28f * random.next();
                float width = .022f + .024f * random.next();
                glm::vec3 root = groundPoint(px + (random.next() - .5f) * .08f,
                                             pz + (random.next() - .5f) * .08f);
                glm::vec3 side(std::cos(angle), 0, std::sin(angle));
                glm::vec3 lean(-std::sin(angle), 0, std::cos(angle));
                glm::vec3 a = root - side * width, b = root + side * width;
                glm::vec3 middle = root + glm::vec3(0, height * .52f, 0) + lean * .025f;
                glm::vec3 c = middle + side * width * .52f, d = middle - side * width * .52f;
                glm::vec3 tip = root + glm::vec3(0, height, 0) + lean * .085f;
                glm::vec3 green = glm::mix(glm::vec3(.08f, .21f, .025f), glm::vec3(.30f, .45f, .07f),
                                            random.next());
                auto bladeTriangle = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2,
                                         float t0, float t1, float t2) {
                    glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                    glm::vec3 p[] = {p0, p1, p2};
                    float t[] = {t0, t1, t2};
                    for (int i = 0; i < 3; i++)
                        vertices.push_back({p[i], n, green * (.70f + .35f * t[i]), {t[i] * .20f, 96}});
                };
                bladeTriangle(a, b, c, 0, 0, .52f);
                bladeTriangle(a, c, d, 0, .52f, .52f);
                bladeTriangle(d, c, tip, .52f, .52f, 1);
            }
        }

    // A rear fence and two shorter side sections, following the terrain.
    auto fence = [&](glm::vec2 start, glm::vec2 end, int posts) {
        glm::vec3 previous{};
        for (int i = 0; i < posts; i++) {
            glm::vec2 p = glm::mix(start, end, float(i) / (posts - 1));
            glm::vec3 base = groundPoint(p.x, p.y);
            box(base - glm::vec3(.065f, 0, .065f), base + glm::vec3(.065f, .86f, .065f), wood);
            if (i)
                for (float y : {.30f, .65f})
                    cylinder(previous + glm::vec3(0, y, 0), base + glm::vec3(0, y, 0), .035f,
                             {.47f, .30f, .14f});
            previous = base;
        }
    };
    fence({-4.7f, -4.6f}, {4.7f, -4.6f}, 15);
    fence({-4.7f, -4.6f}, {-4.7f, 1.2f}, 9);
    fence({4.7f, -4.6f}, {4.7f, .8f}, 8);

    for (glm::vec2 p : {glm::vec2(-4.f, -1.3f), glm::vec2(3.5f, -3.4f), glm::vec2(-3.6f, -4.9f)}) {
        glm::vec3 base = groundPoint(p.x, p.y);
        cylinder(base, base + glm::vec3(.10f, 2.6f, .04f), .11f, wood);
        for (int branch = 0; branch < 5; branch++) {
            float a = branch * 2.39996f;
            glm::vec3 end = base + glm::vec3(.65f * std::cos(a), 2.05f + .16f * branch,
                                            .65f * std::sin(a));
            cylinder(base + glm::vec3(0, 1.4f, 0), end, .035f, wood);
            sphere(end + glm::vec3(0, .37f, 0), {.72f, .82f, .67f},
                   {.13f + .015f * branch, .29f + .025f * branch, .045f}, 5, 9, false, 96);
        }
    }
    for (glm::vec2 p : {glm::vec2(-5.5f, -3.8f), glm::vec2(5.2f, -5.4f)}) {
        glm::vec3 base = groundPoint(p.x, p.y);
        cylinder(base, base + glm::vec3(0, 3.2f, 0), .10f, wood);
        for (int layer = 0; layer < 4; layer++) {
            float y = .7f + layer * .65f, r = 1.05f - layer * .20f;
            for (int i = 0; i < 12; i++) {
                float a = glm::two_pi<float>() * i / 12, b = glm::two_pi<float>() * (i + 1) / 12;
                triangle(base + glm::vec3(r * std::cos(a), y, r * std::sin(a)),
                         base + glm::vec3(r * std::cos(b), y, r * std::sin(b)),
                         base + glm::vec3(0, y + 1.25f, 0), {.045f, .20f + .018f * (i % 3), .095f},
                         0, 96);
            }
        }
    }
    for (int i = 0; i < 18; i++) {
        float a = random.next() * glm::two_pi<float>(), r = 4.6f + random.next() * 1.5f;
        float x = std::cos(a) * r, z = std::sin(a) * r, s = .14f + .24f * random.next();
        sphere(groundPoint(x, z) + glm::vec3(0, s * .30f, 0), {s * 1.3f, s * .6f, s},
               {.36f, .37f, .29f}, 4, 7, false, 64);
    }

    // Raised flower bed and a timber bench beside the front path.
    glm::vec3 bed = groundPoint(3.1f, 1.15f);
    box(bed + glm::vec3(-.62f, -.03f, -.8f), bed + glm::vec3(.62f, .11f, .8f), {.25f, .15f, .065f});
    for (float x : {-.67f, .60f})
        box(bed + glm::vec3(x, -.04f, -.87f), bed + glm::vec3(x + .07f, .20f, .87f), wood);
    for (float z : {-.87f, .80f})
        box(bed + glm::vec3(-.67f, -.04f, z), bed + glm::vec3(.67f, .20f, z + .07f), wood);
    for (int i = 0; i < 45; i++) {
        glm::vec3 base = bed + glm::vec3((random.next() - .5f) * 1.1f, .11f,
                                        (random.next() - .5f) * 1.4f);
        glm::vec3 center = base + glm::vec3(0, .22f + random.next() * .20f, 0);
        cylinder(base, center, .009f, {.13f, .30f, .045f});
        glm::vec3 petal = i % 3 == 0 ? glm::vec3(.82f, .75f, .55f)
                                    : (i % 3 == 1 ? glm::vec3(.64f, .16f, .32f)
                                                  : glm::vec3(.46f, .28f, .67f));
        for (int j = 0; j < 8; j++) {
            float a = glm::two_pi<float>() * j / 8, b = a + .28f, c = a - .28f;
            triangle(center, center + glm::vec3(.065f * std::cos(b), -.012f, .065f * std::sin(b)),
                     center + glm::vec3(.065f * std::cos(c), -.012f, .065f * std::sin(c)), petal,
                     0, 96);
        }
        sphere(center + glm::vec3(0, .006f, 0), {.023f, .018f, .023f}, {.84f, .51f, .045f},
               3, 6, true, 96);
    }
    glm::vec3 bench = groundPoint(-3.0f, 2.0f);
    for (float x : {-.65f, .65f})
        for (float z : {-.20f, .20f})
            box(bench + glm::vec3(x - .035f, 0, z - .035f),
                bench + glm::vec3(x + .035f, .48f, z + .035f), wood);
    for (int slat = 0; slat < 4; slat++) {
        float z = -.27f + slat * .14f;
        box(bench + glm::vec3(-.80f, .45f, z), bench + glm::vec3(.80f, .51f, z + .12f),
            {.49f, .29f, .11f});
    }
    for (float x : {-.68f, .68f})
        box(bench + glm::vec3(x - .035f, .45f, -.28f), bench + glm::vec3(x + .035f, .96f, -.21f), wood);
    for (float y : {.68f, .84f})
        box(bench + glm::vec3(-.8f, y, -.31f), bench + glm::vec3(.8f, y + .11f, -.22f),
            {.49f, .29f, .11f});
}
} // namespace micro
