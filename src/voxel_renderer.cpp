#include "voxel_renderer.hpp"
namespace micro {
VoxelRenderer::VoxelRenderer(VulkanContext &context, VisualVoxelizer &v)
    : vk(context), voxelizer(v) {
    VkAttachmentDescription attachments[2]{};
    for (int i = 0; i < 2; i++) {
        auto &a = attachments[i];
        // The format is fixed; no device-specific lighting or ray-tracing extensions.
        a.format = i ? VK_FORMAT_D32_SFLOAT : vk.swapFormat;
        a.samples = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = i ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                          : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }
    VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &cr;
    sub.pDepthStencilAttachment = &dr;
    VkSubpassDependency deps[2] = {
        {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0},
        {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_ACCESS_TRANSFER_READ_BIT, 0}};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 2;
    ci.pAttachments = attachments;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 2;
    ci.pDependencies = deps;
    check(vkCreateRenderPass(vk.device, &ci, nullptr, &pass), "voxel render pass");
    cubes = vk.graphics(v.layout, pass, "voxel.vert", "voxel.frag", 1, true);
    debugCubes = vk.graphics(v.layout, pass, "voxel_debug.vert", "voxel_debug.frag", 1, true);
    fullscreen = vk.graphics(v.layout, pass, "fullscreen.vert", "fullscreen.frag", 1, false);
}
void VoxelRenderer::resize() {
    if (framebuffer)
        vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
    vk.destroy(output);
    vk.destroy(depth);
    output = vk.image(vk.extent.width, vk.extent.height, vk.swapFormat,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    depth = vk.image(vk.extent.width, vk.extent.height, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    VkImageView views[] = {output.view, depth.view};
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = pass;
    ci.attachmentCount = 2;
    ci.pAttachments = views;
    ci.width = vk.extent.width;
    ci.height = vk.extent.height;
    ci.layers = 1;
    check(vkCreateFramebuffer(vk.device, &ci, nullptr, &framebuffer), "voxel framebuffer");
}
void VoxelRenderer::begin(const Parameters &) {
    VkClearValue clear[2]{};
    clear[0].color = {{.035f, .055f, .09f, 1}};
    clear[1].depthStencil = {1, 0};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = pass;
    bi.framebuffer = framebuffer;
    bi.renderArea.extent = vk.extent;
    bi.clearValueCount = 2;
    bi.pClearValues = clear;
    vkCmdBeginRenderPass(vk.cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
}
void VoxelRenderer::draw(const Parameters &p, VkQueryPool queries) {
    vkCmdBindDescriptorSets(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, voxelizer.layout, 0, 1,
                            &voxelizer.sets[voxelizer.current], 0, nullptr);
    vkCmdPushConstants(vk.cmd, voxelizer.layout, VK_SHADER_STAGE_ALL, 0, sizeof(p), &p);
    float w = float(vk.extent.width), h = float(vk.extent.height);
    if (p.flags.w == 0 || p.flags.w == 2) {
        vk.viewport(0, 0, p.flags.w == 2 ? float(vk.extent.width / 2) : w, h);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreen);
        vkCmdDraw(vk.cmd, 3, 1, 0, 0);
    }
    if (queries)
        vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 7);
    if (p.flags.w == 1 || p.flags.w == 2) {
        float half = float(vk.extent.width / 2);
        vk.viewport(p.flags.w == 2 ? half : 0, 0, p.flags.w == 2 ? w - half : w, h);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.flags.z ? debugCubes : cubes);
        vkCmdDrawIndirect(vk.cmd, voxelizer.counters.handle, 0, 1, sizeof(VkDrawIndirectCommand));
    }
    if (queries)
        vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 8);
}
void VoxelRenderer::end() {
    vkCmdEndRenderPass(vk.cmd);
}
VoxelRenderer::~VoxelRenderer() {
    if (framebuffer)
        vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
    vk.destroy(output);
    vk.destroy(depth);
    vkDestroyPipeline(vk.device, cubes, nullptr);
    vkDestroyPipeline(vk.device, debugCubes, nullptr);
    vkDestroyPipeline(vk.device, fullscreen, nullptr);
    vkDestroyRenderPass(vk.device, pass, nullptr);
}
} // namespace micro
