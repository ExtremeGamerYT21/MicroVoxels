#pragma once
#include <glm/glm.hpp>
#include <vector>
namespace micro {
enum class SourceScene { Garden, Test };
struct SourceVertex {
    glm::vec4 position, normal, albedo;
};
static_assert(sizeof(SourceVertex) == 48);
struct Vertex {
    glm::vec3 position, normal, color;
    glm::vec2 material;
};
// Ordinary triangles only. This class has no voxel/grid dependencies.
class SourceWorld {
  public:
    std::vector<Vertex> vertices;
    explicit SourceWorld(SourceScene scene = SourceScene::Garden);
    size_t triangleCount() const {
        return vertices.size() / 3;
    }

  private:
    void buildTest();
    void buildGarden();
    void quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color,
              float gloss = 48);
    void box(glm::vec3 low, glm::vec3 high, glm::vec3 color, float gloss = 48);
    void triangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float bend = 0,
                  float gloss = 24);
    void sphere(glm::vec3 center, glm::vec3 scale, glm::vec3 color, int rings, int sectors,
                bool smooth, float gloss);
    void cylinder(glm::vec3 a, glm::vec3 b, float radius, glm::vec3 color, float bend = 0);
};
} // namespace micro
