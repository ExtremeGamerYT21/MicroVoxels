#pragma once
#include "surface_types.hpp"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>
namespace micro {
void check(VkResult r, const char *action);
struct Buffer {
    VkBuffer handle{};
    VkDeviceMemory memory{};
    VkDeviceSize size{};
    void *mapped{};
};
struct Image {
    VkImage handle{};
    VkDeviceMemory memory{};
    VkImageView view{};
    VkFormat format{};
    uint32_t width{}, height{};
    VkImageAspectFlags aspect{};
};
struct Voxel {
    glm::vec4 centerSize, rgba;
};
struct Counters {
    uint32_t vertexCount, instanceCount, firstVertex, firstInstance, hits, dropped, rootDropped,
        candidateWrites;
    uint32_t perLod[8];
    uint32_t maxFootprintCells, clampedFootprints, rejectedNeighbors, maxFootprintExtent;
    uint32_t cacheRetained, cacheExpired, cacheRejected, cacheDropped;
    uint32_t cacheLodRejected;
    uint32_t temporalBlends;
};
static_assert(sizeof(Voxel) == 32 && sizeof(Counters) == 104 && sizeof(Parameters) == 112 &&
              sizeof(Frame) == 176);
class VulkanContext {
  public:
    GLFWwindow *window{};
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debug{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkSurfaceKHR surface{};
    VkQueue queue{};
    uint32_t queueFamily{}, timestampBits{};
    float timestampPeriod{};
    std::string gpuName;
    VkPhysicalDeviceType gpuType{};
    uint32_t validationErrors{};
    VkCommandPool pool{};
    VkCommandBuffer cmd{};
    VkFence fence{};
    VkSemaphore acquired{};
    std::vector<VkSemaphore> presented;
    VkSwapchainKHR swapchain{};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkFormat swapFormat{};
    VkExtent2D extent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    VkDescriptorPool descriptorPool{};
    VkSampler sampler{};
    bool validation{};
    bool vsyncRequested = true;
    VulkanContext(int width, int height, bool hidden, bool validate, bool vsync = true);
    ~VulkanContext();
    Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host = false);
    Image image(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage,
                VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void destroy(Buffer &b);
    void destroy(Image &i);
    VkShaderModule shader(const std::string &name);
    VkPipeline compute(VkPipelineLayout layout, const std::string &name);
    VkPipeline graphics(VkPipelineLayout layout, VkRenderPass pass, const std::string &vertex,
                        const std::string &fragment, int colorCount, bool depth,
                        bool sourceVertex = false, bool hudVertex = false,
                        VkCullModeFlags cull = VK_CULL_MODE_NONE);
    VkPipelineLayout pipelineLayout(VkDescriptorSetLayout set);
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);
    void resizeSwapchain();
    const char *presentModeName() const;
    void begin();
    void submit(uint32_t index);
    void wait();
    void immediateBegin();
    void immediateEnd();
    void barrier(VkPipelineStageFlags from, VkPipelineStageFlags to, VkAccessFlags write,
                 VkAccessFlags read);
    void imageBarrier(Image &image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags src,
                      VkPipelineStageFlags dst, VkAccessFlags writes, VkAccessFlags reads);
    void viewport(float x, float y, float w, float h);

  private:
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags);
};
} // namespace micro
