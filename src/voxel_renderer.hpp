#pragma once
#include "visual_voxelizer.hpp"
namespace micro {
// Only consumes colored sample cubes. No SourceWorld dependency or material evaluation.
class VoxelRenderer {
    VulkanContext &vk;
    VisualVoxelizer &voxelizer;
    VkPipeline cubes{}, debugCubes{}, fullscreen{};

  public:
    VkRenderPass pass{};
    Image output, depth;
    VkFramebuffer framebuffer{};
    VoxelRenderer(VulkanContext &vk, VisualVoxelizer &voxelizer);
    ~VoxelRenderer();
    void resize();
    void begin(const Parameters &p);
    void draw(const Parameters &p);
    void end();
};
} // namespace micro
