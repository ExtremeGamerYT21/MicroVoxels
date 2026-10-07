#pragma once
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
struct alignas(16) Frame {
    glm::mat4 vp, lightVP;
    glm::vec4 cameraTime, light, options;
};
struct alignas(16) Parameters {
    glm::ivec4 extent;
    glm::vec4 config;
    glm::ivec4 flags;
    glm::ivec4 gridMin, gridExtent;
};
struct Voxel {
    glm::vec4 centerSize, rgba;
};
struct Counters {
    uint32_t vertexCount, instanceCount, firstVertex, firstInstance, hits, dropped, rootDropped,
        pad;
    uint32_t perLod[8];
};
static_assert(sizeof(Voxel) == 32 && sizeof(Counters) == 64 && sizeof(Parameters) == 80 &&
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
    uint32_t validationErrors{};
    VkCommandPool pool{};
    VkCommandBuffer cmd{};
    VkFence fence{};
    VkSemaphore acquired{};
    std::vector<VkSemaphore> presented;
    VkSwapchainKHR swapchain{};
    VkFormat swapFormat{};
    VkExtent2D extent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    VkDescriptorPool descriptorPool{};
    VkSampler sampler{};
    bool validation{};
    VulkanContext(int width, int height, bool hidden, bool validate);
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
                        bool sourceVertex = false, bool hudVertex = false);
    VkPipelineLayout pipelineLayout(VkDescriptorSetLayout set);
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);
    void resizeSwapchain();
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
