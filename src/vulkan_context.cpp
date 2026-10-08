#include "vulkan_context.hpp"
#include "source_world.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace micro {
void check(VkResult r, const char *action) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string(action) + ": VkResult " + std::to_string(r));
}
static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
              const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        static_cast<VulkanContext *>(user)->validationErrors++;
    std::cerr << "Vulkan validation: " << data->pMessage << '\n';
    return VK_FALSE;
}
VulkanContext::VulkanContext(int w, int h, bool hidden, bool validate, bool vsync)
    : validation(validate), vsyncRequested(vsync) {
    glfwSetErrorCallback([](int code, const char *message) {
        std::cerr << "GLFW " << code << ": " << message << '\n';
    });
    if (!glfwInit())
        throw std::runtime_error("GLFW initialization failed: a desktop display is required");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, hidden ? GLFW_FALSE : GLFW_TRUE);
    window = glfwCreateWindow(w, h, "Microvoxels", nullptr, nullptr);
    if (!window)
        throw std::runtime_error("Window creation failed");
    uint32_t count = 0;
    const char **required = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char *> extensions(required, required + count);
    const char *layer = "VK_LAYER_KHRONOS_validation";
    if (validation) {
        uint32_t n = 0;
        vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> layers(n);
        vkEnumerateInstanceLayerProperties(&n, layers.data());
        validation = std::any_of(layers.begin(), layers.end(),
                                 [&](auto &l) { return std::strcmp(l.layerName, layer) == 0; });
        if (!validation)
            throw std::runtime_error(
                "--validation requested but Khronos validation layer is unavailable");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Microvoxels";
    app.apiVersion = VK_API_VERSION_1_1;
    VkDebugUtilsMessengerCreateInfoEXT di{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    di.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    di.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    di.pfnUserCallback = debugCallback;
    di.pUserData = this;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = uint32_t(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
    if (validation) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &layer;
        ci.pNext = &di;
    }
    check(vkCreateInstance(&ci, nullptr, &instance), "create instance");
    if (validation) {
        auto f = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        check(f(instance, &di, nullptr, &debug), "create debug messenger");
    }
    check(glfwCreateWindowSurface(instance, window, nullptr, &surface), "create surface");
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    int best = -1;
    for (auto d : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(d, &features);
        if (!features.fragmentStoresAndAtomics || props.apiVersion < VK_API_VERSION_1_1)
            continue;
        uint32_t n = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &n, nullptr);
        std::vector<VkQueueFamilyProperties> families(n);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &n, families.data());
        for (uint32_t i = 0; i < n; i++) {
            VkBool32 support;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface, &support);
            if (support &&
                (families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
                    (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
                if (score > best) {
                    best = score;
                    physical = d;
                    queueFamily = i;
                    timestampBits = families[i].timestampValidBits;
                    timestampPeriod = props.limits.timestampPeriod;
                    gpuName = props.deviceName;
                    gpuType = props.deviceType;
                }
                break;
            }
        }
    }
    if (!physical)
        throw std::runtime_error("No Vulkan 1.1 graphics+compute device with presentation support");
    float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = queueFamily;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    const char *swapExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkPhysicalDeviceFeatures features{};
    features.fragmentStoresAndAtomics = VK_TRUE;
    VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dc.queueCreateInfoCount = 1;
    dc.pQueueCreateInfos = &qi;
    dc.enabledExtensionCount = 1;
    dc.ppEnabledExtensionNames = &swapExt;
    dc.pEnabledFeatures = &features;
    check(vkCreateDevice(physical, &dc, nullptr, &device), "create device");
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex = queueFamily;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    check(vkCreateCommandPool(device, &pi, nullptr, &pool), "create command pool");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    check(vkAllocateCommandBuffers(device, &ai, &cmd), "allocate command buffer");
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    check(vkCreateFence(device, &fi, nullptr, &fence), "create fence");
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check(vkCreateSemaphore(device, &si, nullptr, &acquired), "create semaphore");
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
                                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
                                    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
                                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128}};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpi.maxSets = 64;
    dpi.poolSizeCount = 4;
    dpi.pPoolSizes = sizes;
    check(vkCreateDescriptorPool(device, &dpi, nullptr, &descriptorPool), "create descriptor pool");
    VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 0;
    check(vkCreateSampler(device, &sci, nullptr, &sampler), "create sampler");
    resizeSwapchain();
    std::cout << "Device: " << gpuName
              << "; Vulkan source visibility -> world samples -> unlit indirect cubes\n";
}
uint32_t VulkanContext::memoryType(uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p;
    vkGetPhysicalDeviceMemoryProperties(physical, &p);
    for (uint32_t i = 0; i < p.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (p.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    throw std::runtime_error("Compatible memory type unavailable");
}
Buffer VulkanContext::buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host) {
    Buffer b{};
    b.size = size;
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device, &ci, nullptr, &b.handle), "create buffer");
    VkMemoryRequirements r;
    vkGetBufferMemoryRequirements(device, b.handle, &r);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = r.size;
    ai.memoryTypeIndex =
        memoryType(r.memoryTypeBits,
                   host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device, &ai, nullptr, &b.memory), "allocate buffer memory");
    check(vkBindBufferMemory(device, b.handle, b.memory, 0), "bind buffer");
    if (host)
        check(vkMapMemory(device, b.memory, 0, size, 0, &b.mapped), "map buffer");
    return b;
}
Image VulkanContext::image(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage,
                           VkImageAspectFlags aspect) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(physical, fmt, &properties);
    VkFormatFeatureFlags required = 0;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        required |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        required |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
        required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    if ((properties.optimalTilingFeatures & required) != required)
        throw std::runtime_error("Required image format unsupported");
    Image i{};
    i.format = fmt;
    i.width = w;
    i.height = h;
    i.aspect = aspect;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateImage(device, &ci, nullptr, &i.handle), "create image");
    VkMemoryRequirements r;
    vkGetImageMemoryRequirements(device, i.handle, &r);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = r.size;
    ai.memoryTypeIndex = memoryType(r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device, &ai, nullptr, &i.memory), "allocate image memory");
    check(vkBindImageMemory(device, i.handle, i.memory, 0), "bind image");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = i.handle;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange = {aspect, 0, 1, 0, 1};
    check(vkCreateImageView(device, &vi, nullptr, &i.view), "create image view");
    return i;
}
void VulkanContext::destroy(Buffer &b) {
    if (b.mapped)
        vkUnmapMemory(device, b.memory);
    if (b.handle)
        vkDestroyBuffer(device, b.handle, nullptr);
    if (b.memory)
        vkFreeMemory(device, b.memory, nullptr);
    b = {};
}
void VulkanContext::destroy(Image &i) {
    if (i.view)
        vkDestroyImageView(device, i.view, nullptr);
    if (i.handle)
        vkDestroyImage(device, i.handle, nullptr);
    if (i.memory)
        vkFreeMemory(device, i.memory, nullptr);
    i = {};
}
VkShaderModule VulkanContext::shader(const std::string &name) {
    std::ifstream f(std::string(MICROVOXELS_SHADER_DIR) + "/" + name + ".spv",
                    std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("Shader not found: " + name + ". Build the shaders target.");
    size_t n = size_t(f.tellg());
    std::vector<uint32_t> code(n / 4);
    f.seekg(0);
    f.read(reinterpret_cast<char *>(code.data()), std::streamsize(n));
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = n;
    ci.pCode = code.data();
    VkShaderModule m;
    check(vkCreateShaderModule(device, &ci, nullptr, &m), "create shader module");
    return m;
}
VkPipelineLayout VulkanContext::pipelineLayout(VkDescriptorSetLayout set) {
    VkPushConstantRange pc{VK_SHADER_STAGE_ALL, 0, sizeof(Parameters)};
    VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &set;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges = &pc;
    VkPipelineLayout result;
    check(vkCreatePipelineLayout(device, &ci, nullptr, &result), "create pipeline layout");
    return result;
}
VkDescriptorSet VulkanContext::allocate(VkDescriptorSetLayout l) {
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &l;
    VkDescriptorSet s;
    check(vkAllocateDescriptorSets(device, &ai, &s), "allocate descriptor set");
    return s;
}
VkPipeline VulkanContext::compute(VkPipelineLayout layout, const std::string &name) {
    VkShaderModule m = shader(name);
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.layout = layout;
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ci.stage.module = m;
    ci.stage.pName = "main";
    VkPipeline p;
    VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &p);
    vkDestroyShaderModule(device, m, nullptr);
    check(r, "create compute pipeline");
    return p;
}
VkPipeline VulkanContext::graphics(VkPipelineLayout layout, VkRenderPass pass,
                                   const std::string &vertex, const std::string &fragment,
                                   int colors, bool depth, bool sourceVertex, bool hudVertex,
                                   VkCullModeFlags cull) {
    VkShaderModule vs = shader(vertex), fs = fragment.empty() ? VK_NULL_HANDLE : shader(fragment);
    VkPipelineShaderStageCreateInfo stages[2]{};
    for (int i = 0; i < 2; i++) {
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
        stages[i].module = i ? fs : vs;
        stages[i].pName = "main";
    }
    VkVertexInputBindingDescription binding{0, uint32_t(sourceVertex ? sizeof(Vertex) : 24),
                                            VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[4] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
                                                  {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
                                                  {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 24},
                                                  {3, 0, VK_FORMAT_R32G32_SFLOAT, 36}};
    if (hudVertex) {
        attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 8};
    }
    VkPipelineVertexInputStateCreateInfo vi{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (sourceVertex || hudVertex) {
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &binding;
        vi.vertexAttributeDescriptionCount = hudVertex ? 2 : 4;
        vi.pVertexAttributeDescriptions = attrs;
        if (sourceVertex && fragment.empty()) {
            attrs[1] = attrs[3];
            vi.vertexAttributeDescriptionCount = 2;
        }
    }
    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo ra{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    ra.polygonMode = VK_POLYGON_MODE_FILL;
    ra.cullMode = cull;
    ra.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    ra.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = ds.depthWriteEnable = depth;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    std::vector<VkPipelineColorBlendAttachmentState> attachments(colors);
    for (auto &a : attachments) {
        a.colorWriteMask = 15;
        if (hudVertex) {
            a.blendEnable = VK_TRUE;
            a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            a.colorBlendOp = VK_BLEND_OP_ADD;
            a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            a.alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }
    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = uint32_t(attachments.size());
    cb.pAttachments = attachments.data();
    VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dynamic;
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.stageCount = fs ? 2 : 1;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &ra;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dy;
    ci.layout = layout;
    ci.renderPass = pass;
    VkPipeline p;
    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &p);
    vkDestroyShaderModule(device, vs, nullptr);
    if (fs)
        vkDestroyShaderModule(device, fs, nullptr);
    check(r, "create graphics pipeline");
    return p;
}
const char *VulkanContext::presentModeName() const {
    switch (presentMode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "immediate";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "mailbox";
    default:
        return "fifo";
    }
}
void VulkanContext::resizeSwapchain() {
    check(vkDeviceWaitIdle(device), "wait before resize");
    for (auto s : presented)
        vkDestroySemaphore(device, s, nullptr);
    presented.clear();
    for (auto v : swapViews)
        vkDestroyImageView(device, v, nullptr);
    swapViews.clear();
    if (swapchain)
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    VkSurfaceCapabilitiesKHR caps;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps),
          "surface capabilities");
    uint32_t n = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &n, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(n);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &n, formats.data());
    auto fmt = formats.front();
    for (auto f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_R8G8B8A8_SRGB) {
            fmt = f;
            break;
        }
    swapFormat = fmt.format;
    if (swapFormat != VK_FORMAT_B8G8R8A8_SRGB && swapFormat != VK_FORMAT_R8G8B8A8_SRGB)
        throw std::runtime_error("An sRGB swapchain is required to preserve linear source colors");
    // FIFO is always supported. Preserve the requested mode across window resizes.
    presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsyncRequested) {
        check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &n, nullptr),
              "count surface present modes");
        std::vector<VkPresentModeKHR> modes(n);
        check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &n, modes.data()),
              "surface present modes");
        for (auto preferred : {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR}) {
            if (std::find(modes.begin(), modes.end(), preferred) != modes.end()) {
                presentMode = preferred;
                break;
            }
        }
    }
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    extent =
        caps.currentExtent.width != UINT32_MAX
            ? caps.currentExtent
            : VkExtent2D{
                  std::clamp(uint32_t(w), caps.minImageExtent.width, caps.maxImageExtent.width),
                  std::clamp(uint32_t(h), caps.minImageExtent.height, caps.maxImageExtent.height)};
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount)
        ci.minImageCount = std::min(ci.minImageCount, caps.maxImageCount);
    ci.imageFormat = fmt.format;
    ci.imageColorSpace = fmt.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    check(vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain), "create swapchain");
    std::cout << "Presentation: " << presentModeName()
              << " (VSync requested: " << (vsyncRequested ? "on" : "off") << ")\n";
    if (!vsyncRequested && presentMode == VK_PRESENT_MODE_FIFO_KHR)
        std::cout << "Uncapped presentation unavailable; using refresh-synchronized FIFO.\n";
    vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr);
    swapImages.resize(n);
    vkGetSwapchainImagesKHR(device, swapchain, &n, swapImages.data());
    for (auto im : swapImages) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = im;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = swapFormat;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView v;
        check(vkCreateImageView(device, &vi, nullptr, &v), "create swap view");
        swapViews.push_back(v);
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore s;
        check(vkCreateSemaphore(device, &si, nullptr, &s), "create present semaphore");
        presented.push_back(s);
    }
}
void VulkanContext::wait() {
    check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "wait for frame");
}
void VulkanContext::begin() {
    wait();
    check(vkResetFences(device, 1, &fence), "reset fence");
    check(vkResetCommandBuffer(cmd, 0), "reset command buffer");
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(cmd, &bi), "begin command buffer");
}
void VulkanContext::submit(uint32_t index) {
    check(vkEndCommandBuffer(cmd), "end command buffer");
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &acquired;
    si.pWaitDstStageMask = &stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &presented[index];
    check(vkQueueSubmit(queue, 1, &si, fence), "submit frame");
}
void VulkanContext::immediateBegin() {
    begin();
}
void VulkanContext::immediateEnd() {
    check(vkEndCommandBuffer(cmd), "end immediate");
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    check(vkQueueSubmit(queue, 1, &si, fence), "submit immediate");
    wait();
}
void VulkanContext::barrier(VkPipelineStageFlags from, VkPipelineStageFlags to, VkAccessFlags write,
                            VkAccessFlags read) {
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    b.srcAccessMask = write;
    b.dstAccessMask = read;
    vkCmdPipelineBarrier(cmd, from, to, 0, 1, &b, 0, nullptr, 0, nullptr);
}
void VulkanContext::imageBarrier(Image &i, VkImageLayout from, VkImageLayout to,
                                 VkPipelineStageFlags src, VkPipelineStageFlags dst,
                                 VkAccessFlags writes, VkAccessFlags reads) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = writes;
    b.dstAccessMask = reads;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = i.handle;
    b.subresourceRange = {i.aspect, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, src, dst, 0, 0, nullptr, 0, nullptr, 1, &b);
}
void VulkanContext::viewport(float x, float y, float w, float h) {
    VkViewport v{x, y, w, h, 0, 1};
    VkRect2D s{{int32_t(x), int32_t(y)}, {uint32_t(w), uint32_t(h)}};
    vkCmdSetViewport(cmd, 0, 1, &v);
    vkCmdSetScissor(cmd, 0, 1, &s);
}
VulkanContext::~VulkanContext() {
    if (device) {
        vkDeviceWaitIdle(device);
        for (auto v : swapViews)
            vkDestroyImageView(device, v, nullptr);
        for (auto s : presented)
            vkDestroySemaphore(device, s, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroySampler(device, sampler, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroySemaphore(device, acquired, nullptr);
        vkDestroyFence(device, fence, nullptr);
        vkDestroyCommandPool(device, pool, nullptr);
        vkDestroyDevice(device, nullptr);
    }
    if (surface)
        vkDestroySurfaceKHR(instance, surface, nullptr);
    if (debug) {
        auto f = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        f(instance, debug, nullptr);
    }
    if (instance)
        vkDestroyInstance(instance, nullptr);
    if (window)
        glfwDestroyWindow(window);
    glfwTerminate();
}
} // namespace micro
