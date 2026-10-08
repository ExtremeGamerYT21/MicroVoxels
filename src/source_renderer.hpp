#pragma once
#include "source_world.hpp"
#include "surface_samples.hpp"
#include "vulkan_context.hpp"
namespace micro {
// Source visibility provider: raster or distance-field sphere tracing. The
// voxel filter only sees visible XYZ/depth and final shaded RGB.
class SourceRenderer {
    VulkanContext &vk;
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{};
    VkDescriptorSet set{};
    VkRenderPass pass{}, shadowPass{};
    VkPipeline pipeline{}, raymarchPipeline{}, shadowPipeline{}, animatePipeline{};
    VkFramebuffer shadowFramebuffer{};
    std::array<VkFramebuffer, 2> framebuffers{};
    Buffer originalVertices;
    Buffer distanceNodes, distanceTriangles;
    void destroyTargets();

  public:
    Image color, depth, shadow;
    Buffer animatedVertices;
    Buffer traceStats;
    std::array<Image, 2> positions;
    uint32_t width{}, height{}, vertexCount{};
    SourceRenderer(VulkanContext &vk, const SourceWorld &world, Buffer &frame);
    ~SourceRenderer();
    void resize(uint32_t w, uint32_t h);
    void render(int target, const Parameters &parameters, bool raymarch = false);
    SurfaceSamples samples() {
        return {color, positions, width, height};
    }
};
} // namespace micro
