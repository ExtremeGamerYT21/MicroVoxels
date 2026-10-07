#pragma once
#include "vulkan_context.hpp"
#include <functional>
namespace micro {
struct Settings {
    float base = .01f, distance = 4.f, splat = 1.03f;
    int levels = 5, mode = 2, lighting = 2;
    bool average = true, cubeLight = false, frozen = false, supersampling = false, paused = false,
         visibleHud = true, adaptiveLod = true;
};
struct HudVertex {
    glm::vec2 position;
    glm::vec4 color;
};
class Hud {
    VulkanContext &vk;
    Buffer vertices;
    VkPipeline pipeline{};
    VkPipelineLayout layout{};
    std::vector<HudVertex> mesh;
    void rect(float x, float y, float w, float h, glm::vec4 c);
    void text(float x, float y, const std::string &s, glm::vec4 c = {.83f, .91f, 1, 1});

  public:
    Hud(VulkanContext &vk, VkRenderPass pass, VkDescriptorSetLayout setLayout);
    ~Hud();
    void build(const Settings &s, const Counters &counts, const std::array<double, 6> &times,
               size_t triangles, uint32_t samples, float fps);
    void draw();
    void click(double x, double y, Settings &s);
};
} // namespace micro
