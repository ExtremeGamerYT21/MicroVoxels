#pragma once
#include "source_world.hpp"
#include "surface_samples.hpp"
#include "vulkan_context.hpp"
namespace micro {
// Surface-sample provider: position + final shaded RGB. May be replaced by a KHR ray-query
// provider.
class SourceRenderer {
    VulkanContext &vk;
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{};
    VkDescriptorSet set{};
    VkRenderPass pass{}, shadowPass{};
    VkPipeline pipeline{}, shadowPipeline{}, animatePipeline{};
    VkFramebuffer shadowFramebuffer{};
    std::array<VkFramebuffer, 2> framebuffers{};
    Buffer originalVertices;
    glm::vec3 lower{}, upper{};
    void destroyTargets();

  public:
    Image color, depth, shadow, triangleIds;
    Buffer animatedVertices;
    std::array<Image, 2> positions;
    uint32_t width{}, height{}, vertexCount{};
    SourceRenderer(VulkanContext &vk, const SourceWorld &world, Buffer &frame);
    ~SourceRenderer();
    void resize(uint32_t w, uint32_t h);
    void render(int target, const Parameters &parameters);
    SurfaceSamples samples() {
        return {color, positions, width, height};
    }
    TriangleSurfaces triangles() {
        return {triangleIds, shadow, animatedVertices, vertexCount / 3, lower, upper};
    }
};
} // namespace micro
