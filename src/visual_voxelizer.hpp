#pragma once
#include "surface_samples.hpp"
namespace micro {
class VisualVoxelizer {
    VulkanContext &vk;
    Buffer &frame;
    VkPipeline lodPipeline{}, resolveLodPipeline{}, hashPipeline{}, compactPipeline{},
        compactCountsPipeline{}, compactPrefixPipeline{};

  public:
    static constexpr uint32_t Capacity = 1 << 21, RootCapacity = 1 << 18;
    Buffer slots, counters, instances, compactGroups;
    std::array<Buffer, 2> roots;
    std::array<Image, 2> lods;
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{};
    std::array<VkDescriptorSet, 2> sets{};
    bool resetHistory = true, cloudReady = false;
    int current = 0;
    VisualVoxelizer(VulkanContext &vk, Buffer &frame);
    ~VisualVoxelizer();
    void resize(const SurfaceSamples &source);
    void generate(const Parameters &p, VkQueryPool queries);
    void advance() {
        current = 1 - current;
    }
};
} // namespace micro
