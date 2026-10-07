#pragma once
#include "surface_samples.hpp"
namespace micro {
class VisualVoxelizer {
    VulkanContext &vk;
    Buffer &frame;
    VkPipeline lodPipeline{}, resolveLodPipeline{}, hashPipeline{}, compactPipeline{};
    VkPipeline visibilityPipeline{}, triangleLodPipeline{}, triangleEmitPipeline{},
        triangleHashPipeline{};
    uint32_t triangleCount{};

  public:
    static constexpr uint32_t Capacity = 1 << 22, CandidateCapacity = 1 << 22,
                              RootCapacity = 1 << 18;
    Buffer slots, counters, instances, visibleTriangles, candidates;
    std::array<Buffer, 2> roots;
    std::array<Image, 2> lods;
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{};
    std::array<VkDescriptorSet, 2> sets{};
    bool resetHistory = true, cloudReady = false;
    int current = 0;
    VisualVoxelizer(VulkanContext &vk, Buffer &frame, const TriangleSurfaces &triangles);
    ~VisualVoxelizer();
    void resize(const SurfaceSamples &source, const TriangleSurfaces &triangles);
    void generate(const Parameters &p, VkQueryPool queries, bool triangles);
    void advance() {
        current = 1 - current;
    }
};
} // namespace micro
