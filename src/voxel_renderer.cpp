#include "voxel_renderer.hpp"
#include "cube_mesh.hpp"
#include <cstddef>
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
        // Final depth is used only inside this pass. Source depth is unchanged.
        a.storeOp = i ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
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
    indexedCubes = vk.graphics(v.layout, pass, "voxel_indexed.vert", "voxel.frag", 1, true);
    indexedDebugCubes = vk.graphics(v.layout, pass, "voxel_faces_debug.vert", "voxel_debug.frag", 1, true);
    culledCubes = vk.graphics(v.layout, pass, "voxel_faces.vert", "voxel.frag", 1, true,
                             false, false, VK_CULL_MODE_BACK_BIT);
    culledDebugCubes = vk.graphics(v.layout, pass, "voxel_faces_debug.vert", "voxel_debug.frag", 1, true,
                                  false, false, VK_CULL_MODE_BACK_BIT);
    fullscreen = vk.graphics(v.layout, pass, "fullscreen.vert", "fullscreen.frag", 1, false);
    indices = vk.buffer(sizeof(CubeCornerIndices) + sizeof(CubeFaceIndices),
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    indexedCommand = vk.buffer(sizeof(VkDrawIndexedIndirectCommand),
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    vk.immediateBegin();
    vkCmdUpdateBuffer(vk.cmd, indices.handle, 0, sizeof(CubeCornerIndices), CubeCornerIndices.data());
    vkCmdUpdateBuffer(vk.cmd, indices.handle, sizeof(CubeCornerIndices), sizeof(CubeFaceIndices),
                      CubeFaceIndices.data());
    VkDrawIndexedIndirectCommand command{36, 0, 0, 0, 0};
    vkCmdUpdateBuffer(vk.cmd, indexedCommand.handle, 0, sizeof(command), &command);
    vk.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    vk.immediateEnd();
}
void VoxelRenderer::prepare(const Parameters &p) {
    if (!p.footprint.w || p.flags.w == 0)
        return;
    // VkDrawIndirectCommand and VkDrawIndexedIndirectCommand have the same
    // instanceCount offset. The other indexed fields are initialized once.
    static_assert(offsetof(Counters, instanceCount) ==
                  offsetof(VkDrawIndexedIndirectCommand, instanceCount));
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = indexedCommand.handle;
    barrier.offset = 0;
    barrier.size = indexedCommand.size;
    vkCmdPipelineBarrier(vk.cmd, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0, 0, nullptr, 1, &barrier, 0, nullptr);
    VkBufferCopy copy{offsetof(Counters, instanceCount),
                      offsetof(VkDrawIndexedIndirectCommand, instanceCount), sizeof(uint32_t)};
    vkCmdCopyBuffer(vk.cmd, voxelizer.counters.handle, indexedCommand.handle, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(vk.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                          0, 0, nullptr, 1, &barrier, 0, nullptr);
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
        bool indexed = p.footprint.w != 0, cull = p.footprint.z != 0, debug = p.flags.z != 0;
        VkPipeline pipeline = cull ? (debug ? culledDebugCubes : culledCubes)
                                   : indexed ? (debug ? indexedDebugCubes : indexedCubes)
                                             : (debug ? debugCubes : cubes);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        if (indexed) {
            vkCmdBindIndexBuffer(vk.cmd, indices.handle, (cull || debug) ? sizeof(CubeCornerIndices) : 0,
                                 VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexedIndirect(vk.cmd, indexedCommand.handle, 0, 1,
                                      sizeof(VkDrawIndexedIndirectCommand));
        } else
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
    vk.destroy(indices);
    vk.destroy(indexedCommand);
    for (auto pipeline : {cubes, debugCubes, indexedCubes, indexedDebugCubes,
                          culledCubes, culledDebugCubes, fullscreen})
        vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroyRenderPass(vk.device, pass, nullptr);
}
} // namespace micro
