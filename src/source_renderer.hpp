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
    void destroyTargets();

  public:
    Image color, shadow;
    std::array<Image, 2> depths;
    bool depthOnly;
    Buffer animatedVertices;
    std::array<Image, 2> positions;
    uint32_t width{}, height{}, vertexCount{};
    SourceRenderer(VulkanContext &vk, const SourceWorld &world, Buffer &frame,
                   bool depthOnly = false);
    ~SourceRenderer();
    void resize(uint32_t w, uint32_t h);
    void render(int target, const Parameters &parameters);
    Image &depthFor(int target) {
        return depths[depthOnly ? target : 0];
    }
    SurfaceSamples samples() {
        return {color, depthOnly ? depths : positions, depthOnly, width, height};
    }
};
} // namespace micro
