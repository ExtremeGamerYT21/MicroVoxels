#pragma once
#include "surface_samples.hpp"
namespace micro {
class VisualVoxelizer {
    VulkanContext &vk;
    Buffer &frame;
    VkPipeline lodPipeline{}, resolveLodPipeline{}, hashPipeline{}, localHashPipeline{},
        compactPipeline{}, compactCountsPipeline{}, compactPrefixPipeline{},
        resolveUniquePipeline{}, clearTouchedPipeline{};
    bool tableInitialized = false;
    bool verifyDepth;

  public:
    static constexpr uint32_t Capacity = 1 << 21, RootCapacity = 1 << 18;
    Buffer slots, counters, instances, compactGroups, uniqueSlots, reconstructedSamples;
    std::array<Buffer, 2> roots;
    std::array<Image, 2> lods;
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{};
    std::array<VkDescriptorSet, 2> sets{};
    bool resetHistory = true, cloudReady = false, cloudTouchedClear = false;
    int current = 0;
    uint32_t instanceStride;
    uint32_t cloudComputePasses = 0;
    float cloudBase = .01f, cloudSplat = 1.f;
    VisualVoxelizer(VulkanContext &vk, Buffer &frame, bool packed, bool verifyDepth = false);
    ~VisualVoxelizer();
    void resize(const SurfaceSamples &source);
    void generate(const Parameters &p, VkQueryPool queries);
    void advance() {
        current = 1 - current;
    }
};
} // namespace micro
