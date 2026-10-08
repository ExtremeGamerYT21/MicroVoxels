#include "source_bvh.hpp"
#include "temporal_samples.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace micro;
int main() {
    try {
        auto require = [](bool valid, const char *message) {
            if (!valid)
                throw std::runtime_error(message);
        };
        for (auto scene : {SourceScene::Test, SourceScene::Garden}) {
            SourceWorld world(scene);
            SourceDistanceBvh tree(world);
            std::set<uint32_t> visited;
            for (const auto &node : tree.nodes) {
                if (node.links.w == 0) {
                    for (uint32_t child : {node.links.x, node.links.y}) {
                        require(
                            glm::all(glm::lessThanEqual(node.low, tree.nodes[child].low)) &&
                                glm::all(glm::greaterThanEqual(node.high, tree.nodes[child].high)),
                            "A parent distance bound must contain its children");
                    }
                    continue;
                }
                for (uint32_t i = 0; i < node.links.w; i++) {
                    uint32_t triangle = tree.triangles[node.links.z + i];
                    require(visited.insert(triangle).second,
                            "Triangle duplicated in distance tree");
                    for (int k = 0; k < 3; k++) {
                        const auto &v = world.vertices[triangle * 3 + k];
                        for (int pose = 0; pose < 24; pose++) {
                            float t = pose * .73f, y = std::max(v.position.y - .5f, 0.f);
                            glm::vec3 p =
                                v.position +
                                glm::vec3(.13f * v.material.x * y * y * std::sin(t * 1.4f), 0,
                                          .055f * v.material.x * y * y * std::sin(t * 1.1f + .6f));
                            require(glm::all(glm::greaterThanEqual(p, glm::vec3(node.low))) &&
                                        glm::all(glm::lessThanEqual(p, glm::vec3(node.high))),
                                    "Wind moved a source vertex outside its distance bound");
                        }
                    }
                }
            }
            require(visited.size() == world.triangleCount(), "Distance tree lost triangles");
        }
        glm::vec2 mean(0);
        for (uint32_t i = 0; i < 8; i++) {
            auto p = sourceJitter(i);
            mean += p;
            require(glm::all(glm::lessThan(glm::abs(p), glm::vec2(.5f))),
                    "Source jitter left its pixel");
            require(p == sourceJitter(i + 8), "Source jitter must repeat deterministically");
        }
        require(glm::length(mean) == 0, "Temporal samples must have no net directional bias");
        glm::vec3 old(.3f), fresh(.35f);
        auto blended = temporalColor(old, fresh, .2f);
        require(glm::length(blended - glm::vec3(.31f)) < .001f,
                "Temporal RGB must average small changes");
        require(glm::length(temporalColor(old, glm::vec3(.9f), .2f) - glm::vec3(.9f)) < .001f,
                "A large material change must reject stale RGB");
        std::cout << "Distance-tree animation bounds and world temporal sampling passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
