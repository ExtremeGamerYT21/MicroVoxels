#pragma once
#include "visual_voxelizer.hpp"
namespace micro {
// Only consumes colored sample cubes. No SourceWorld dependency or material evaluation.
class VoxelRenderer {
    VulkanContext &vk;
    VisualVoxelizer &voxelizer;
    VkPipeline cubes{}, debugCubes{}, fullscreen{};
    VkPipeline indexedCubes{}, indexedDebugCubes{}, culledCubes{}, culledDebugCubes{};
    Buffer indices, indexedCommand;

  public:
    VkRenderPass pass{};
    Image output, depth;
    VkFramebuffer framebuffer{};
    VoxelRenderer(VulkanContext &vk, VisualVoxelizer &voxelizer);
    ~VoxelRenderer();
    void resize();
    // Convert only the instance count; generation retains its original command.
    void prepare(const Parameters &p);
    void begin(const Parameters &p);
    void draw(const Parameters &p, VkQueryPool queries);
    void end();
};
} // namespace micro
