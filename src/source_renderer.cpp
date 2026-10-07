#include "source_renderer.hpp"
#include <algorithm>
#include <cstring>
namespace micro {
SourceRenderer::SourceRenderer(VulkanContext &context, const SourceWorld &world, Buffer &frame)
    : vk(context) {
    vertexCount = uint32_t(world.vertices.size());
    originalVertices =
        vk.buffer(vertexCount * sizeof(SourceVertex), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
    animatedVertices = vk.buffer(originalVertices.size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto *data = static_cast<SourceVertex *>(originalVertices.mapped);
    for (uint32_t i = 0; i < vertexCount; i++) {
        const auto &v = world.vertices[i];
        data[i] = {glm::vec4(v.position, v.material.x), glm::vec4(v.normal, v.material.y),
                   glm::vec4(v.color, 1)};
    }
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
         VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 4;
    ci.pBindings = bindings;
    check(vkCreateDescriptorSetLayout(vk.device, &ci, nullptr, &setLayout),
          "source descriptor layout");
    layout = vk.pipelineLayout(setLayout);
    set = vk.allocate(setLayout);
    shadow = vk.image(1024, 1024, VK_FORMAT_D32_SFLOAT,
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                      VK_IMAGE_ASPECT_DEPTH_BIT);
    VkDescriptorBufferInfo bi{frame.handle, 0, sizeof(Frame)};
    VkDescriptorImageInfo ii{vk.sampler, shadow.view,
                             VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    VkDescriptorBufferInfo originalInfo{originalVertices.handle, 0, originalVertices.size},
        animatedInfo{animatedVertices.handle, 0, animatedVertices.size};
    VkWriteDescriptorSet writes[4]{};
    for (int i = 0; i < 4; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = bindings[i].descriptorType;
    }
    writes[0].pBufferInfo = &bi;
    writes[1].pImageInfo = &ii;
    writes[2].pBufferInfo = &originalInfo;
    writes[3].pBufferInfo = &animatedInfo;
    vkUpdateDescriptorSets(vk.device, 4, writes, 0, nullptr);
    VkAttachmentDescription sa{};
    sa.format = VK_FORMAT_D32_SFLOAT;
    sa.samples = VK_SAMPLE_COUNT_1_BIT;
    sa.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    sa.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    sa.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    sa.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    sa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sa.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAttachmentReference sr{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.pDepthStencilAttachment = &sr;
    VkSubpassDependency deps[2] = {
        {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0},
        {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0}};
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1;
    rp.pAttachments = &sa;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    rp.dependencyCount = 2;
    rp.pDependencies = deps;
    check(vkCreateRenderPass(vk.device, &rp, nullptr, &shadowPass), "shadow render pass");
    VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb.renderPass = shadowPass;
    fb.attachmentCount = 1;
    fb.pAttachments = &shadow.view;
    fb.width = shadow.width;
    fb.height = shadow.height;
    fb.layers = 1;
    check(vkCreateFramebuffer(vk.device, &fb, nullptr, &shadowFramebuffer), "shadow framebuffer");
    VkAttachmentDescription attachments[3]{};
    for (int i = 0; i < 3; i++) {
        auto &a = attachments[i];
        a.format = i == 2 ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
        a.samples = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = i == 2 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    VkAttachmentReference cr[2] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                                   {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}},
                          dr{2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    sub.colorAttachmentCount = 2;
    sub.pColorAttachments = cr;
    sub.pDepthStencilAttachment = &dr;
    VkSubpassDependency sourceDeps[2] = {
        {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0},
        {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0}};
    rp.attachmentCount = 3;
    rp.pAttachments = attachments;
    rp.pDependencies = sourceDeps;
    check(vkCreateRenderPass(vk.device, &rp, nullptr, &pass), "source render pass");
    pipeline = vk.graphics(layout, pass, "source.vert", "source.frag", 2, true);
    shadowPipeline = vk.graphics(layout, shadowPass, "shadow.vert", "", 0, true);
    animatePipeline = vk.compute(layout, "source_animate.comp");
}
void SourceRenderer::resize(uint32_t w, uint32_t h) {
    destroyTargets();
    width = w;
    height = h;
    auto usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    color = vk.image(w, h, VK_FORMAT_R32G32B32A32_SFLOAT, usage);
    depth = vk.image(w, h, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT);
    for (int i = 0; i < 2; i++) {
        positions[i] = vk.image(w, h, VK_FORMAT_R32G32B32A32_SFLOAT, usage);
        VkImageView views[] = {color.view, positions[i].view, depth.view};
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass = pass;
        ci.attachmentCount = 3;
        ci.pAttachments = views;
        ci.width = w;
        ci.height = h;
        ci.layers = 1;
        check(vkCreateFramebuffer(vk.device, &ci, nullptr, &framebuffers[i]), "source framebuffer");
    }
    // Both history positions start valid for sampling; empty roots guarantee they are never
    // interpreted as hits.
    vk.immediateBegin();
    for (auto &p : positions) {
        vk.imageBarrier(p, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                        VK_ACCESS_SHADER_READ_BIT);
    }
    vk.immediateEnd();
}
void SourceRenderer::render(int target, const Parameters &parameters) {
    vk.barrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, animatePipeline);
    vkCmdBindDescriptorSets(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(vk.cmd, layout, VK_SHADER_STAGE_ALL, 0, sizeof(parameters), &parameters);
    vkCmdDispatch(vk.cmd, (vertexCount + 63) / 64, 1, 1);
    vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vkCmdBindDescriptorSets(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0,
                            nullptr);
    vkCmdPushConstants(vk.cmd, layout, VK_SHADER_STAGE_ALL, 0, sizeof(parameters), &parameters);
    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1, 0};
    VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    bi.renderPass = shadowPass;
    bi.framebuffer = shadowFramebuffer;
    bi.renderArea.extent = {shadow.width, shadow.height};
    bi.clearValueCount = 1;
    bi.pClearValues = &shadowClear;
    vkCmdBeginRenderPass(vk.cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    vk.viewport(0, 0, float(shadow.width), float(shadow.height));
    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
    vkCmdDraw(vk.cmd, vertexCount, 1, 0, 0);
    vkCmdEndRenderPass(vk.cmd);
    VkClearValue clears[3]{};
    clears[2].depthStencil = {1, 0};
    bi.renderPass = pass;
    bi.framebuffer = framebuffers[target];
    bi.renderArea.extent = {width, height};
    bi.clearValueCount = 3;
    bi.pClearValues = clears;
    vkCmdBeginRenderPass(vk.cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    vk.viewport(0, 0, float(width), float(height));
    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(vk.cmd, vertexCount, 1, 0, 0);
    vkCmdEndRenderPass(vk.cmd);
}
void SourceRenderer::destroyTargets() {
    for (auto &f : framebuffers) {
        if (f)
            vkDestroyFramebuffer(vk.device, f, nullptr);
        f = {};
    }
    for (auto &i : positions)
        vk.destroy(i);
    vk.destroy(color);
    vk.destroy(depth);
}
SourceRenderer::~SourceRenderer() {
    destroyTargets();
    vk.destroy(originalVertices);
    vk.destroy(animatedVertices);
    vkDestroyFramebuffer(vk.device, shadowFramebuffer, nullptr);
    vk.destroy(shadow);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroyPipeline(vk.device, animatePipeline, nullptr);
    vkDestroyPipeline(vk.device, shadowPipeline, nullptr);
    vkDestroyRenderPass(vk.device, pass, nullptr);
    vkDestroyRenderPass(vk.device, shadowPass, nullptr);
    vkDestroyPipelineLayout(vk.device, layout, nullptr);
    vkFreeDescriptorSets(vk.device, vk.descriptorPool, 1, &set);
    vkDestroyDescriptorSetLayout(vk.device, setLayout, nullptr);
}
} // namespace micro
