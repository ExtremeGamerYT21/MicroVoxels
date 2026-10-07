#include "visual_voxelizer.hpp"
namespace micro {
VisualVoxelizer::VisualVoxelizer(VulkanContext &context, Buffer &uniform,
                                 const TriangleSurfaces &triangles)
    : vk(context), frame(uniform), triangleCount(triangles.triangleCount) {
    visibleTriangles = vk.buffer(triangleCount * 4ull, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    candidates = vk.buffer(CandidateCapacity * 32ull,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    slots = vk.buffer(Capacity * 24ull,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    counters = vk.buffer(sizeof(Counters),
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    instances = vk.buffer(Capacity * sizeof(Voxel),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    for (auto &r : roots)
        r = vk.buffer(RootCapacity * 8ull, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (uint32_t i = 0; i <= 15; i++) {
        VkDescriptorType t =
            i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                   : (i == 4 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                             : ((i == 1 || i == 2 || i == 3 || i == 7 || i == 11 || i == 15)
                                    ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                    : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
        bindings.push_back({i, t, 1, VK_SHADER_STAGE_ALL, nullptr});
    }
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = uint32_t(bindings.size());
    ci.pBindings = bindings.data();
    check(vkCreateDescriptorSetLayout(vk.device, &ci, nullptr, &setLayout),
          "voxel descriptor layout");
    layout = vk.pipelineLayout(setLayout);
    for (auto &s : sets)
        s = vk.allocate(setLayout);
    lodPipeline = vk.compute(layout, "lod.comp");
    resolveLodPipeline = vk.compute(layout, "lod_resolve.comp");
    hashPipeline = vk.compute(layout, "voxelize.comp");
    compactPipeline = vk.compute(layout, "compact.comp");
    visibilityPipeline = vk.compute(layout, "triangle_visibility.comp");
    triangleLodPipeline = vk.compute(layout, "triangle_lod.comp");
    triangleEmitPipeline = vk.compute(layout, "triangle_emit.comp");
    triangleHashPipeline = vk.compute(layout, "triangle_hash.comp");
}
void VisualVoxelizer::resize(const SurfaceSamples &source, const TriangleSurfaces &triangles) {
    for (auto &l : lods) {
        vk.destroy(l);
        l = vk.image(source.width, source.height, VK_FORMAT_R32_UINT,
                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }
    for (int i = 0; i < 2; i++) {
        VkDescriptorBufferInfo bufferInfo[16]{};
        VkDescriptorImageInfo imageInfo[16]{};
        VkWriteDescriptorSet writes[16]{};
        bufferInfo[0] = {frame.handle, 0, sizeof(Frame)};
        bufferInfo[5] = {roots[i].handle, 0, roots[i].size};
        bufferInfo[6] = {roots[1 - i].handle, 0, roots[1 - i].size};
        bufferInfo[8] = {slots.handle, 0, slots.size};
        bufferInfo[9] = {counters.handle, 0, counters.size};
        bufferInfo[10] = {instances.handle, 0, instances.size};
        bufferInfo[12] = {triangles.vertices.handle, 0, triangles.vertices.size};
        bufferInfo[13] = {visibleTriangles.handle, 0, visibleTriangles.size};
        bufferInfo[14] = {candidates.handle, 0, candidates.size};
        imageInfo[1] = {vk.sampler, source.positions[i].view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfo[2] = {vk.sampler, source.positions[1 - i].view,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfo[3] = {vk.sampler, lods[1 - i].view, VK_IMAGE_LAYOUT_GENERAL};
        imageInfo[4] = {VK_NULL_HANDLE, lods[i].view, VK_IMAGE_LAYOUT_GENERAL};
        imageInfo[7] = {vk.sampler, source.color.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfo[11] = {vk.sampler, triangles.ids.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        imageInfo[15] = {vk.sampler, triangles.shadow.view,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        for (uint32_t b = 0; b <= 15; b++) {
            auto &w = writes[b];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = sets[i];
            w.dstBinding = b;
            w.descriptorCount = 1;
            if (b == 0 || b == 5 || b == 6 || (b >= 8 && b <= 10) || (b >= 12 && b <= 14)) {
                w.descriptorType =
                    b == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                w.pBufferInfo = &bufferInfo[b];
            } else {
                w.descriptorType = b == 4 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                          : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w.pImageInfo = &imageInfo[b];
            }
        }
        vkUpdateDescriptorSets(vk.device, 16, writes, 0, nullptr);
    }
    vk.immediateBegin();
    for (auto &l : lods)
        vk.imageBarrier(l, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    for (auto &r : roots)
        vkCmdFillBuffer(vk.cmd, r.handle, 0, r.size, 0);
    vk.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vk.immediateEnd();
    current = 0;
    resetHistory = true;
}
void VisualVoxelizer::generate(const Parameters &p, VkQueryPool q, bool triangles) {
    vk.barrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
               VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdFillBuffer(vk.cmd, slots.handle, 0, slots.size, 0);
    if (triangles)
        vkCmdFillBuffer(vk.cmd, visibleTriangles.handle, 0, visibleTriangles.size, 0);
    // Initialize the whole indirect command and statistics in one transfer. A fill
    // followed by an unsynchronized partial update can clear vertexCount again.
    Counters initial{};
    initial.vertexCount = 36;
    vkCmdUpdateBuffer(vk.cmd, counters.handle, 0, sizeof(initial), &initial);
    vkCmdFillBuffer(vk.cmd, roots[current].handle, 0, roots[current].size, 0);
    if (resetHistory) {
        vkCmdFillBuffer(vk.cmd, roots[1 - current].handle, 0, roots[1 - current].size, 0);
        resetHistory = false;
    }
    vk.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindDescriptorSets(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &sets[current], 0,
                            nullptr);
    vkCmdPushConstants(vk.cmd, layout, VK_SHADER_STAGE_ALL, 0, sizeof(p), &p);
    if (triangles) {
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, visibilityPipeline);
        vkCmdDispatch(vk.cmd, (p.extent.x + 7) / 8, (p.extent.y + 7) / 8, 1);
        vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, triangleLodPipeline);
        vkCmdDispatch(vk.cmd, (p.gridExtent.x * p.gridExtent.y * p.gridExtent.z + 255) / 256, 1, 1);
        vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, triangleEmitPipeline);
        vkCmdDispatch(vk.cmd, triangleCount, 1, 1);
        vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        if (q)
            vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 2);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, triangleHashPipeline);
        vkCmdDispatch(vk.cmd, (p.gridExtent.w + 255) / 256, 1, 1);
    } else {
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lodPipeline);
        vkCmdDispatch(vk.cmd, (p.extent.x + 7) / 8, (p.extent.y + 7) / 8, 1);
        vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, resolveLodPipeline);
        vkCmdDispatch(vk.cmd, (p.extent.x + 7) / 8, (p.extent.y + 7) / 8, 1);
        vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        if (q)
            vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 2);
        vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, hashPipeline);
        vkCmdDispatch(vk.cmd, (p.extent.x + 7) / 8, (p.extent.y + 7) / 8, 1);
    }
    vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    if (q)
        vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 3);
    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compactPipeline);
    vkCmdDispatch(vk.cmd, Capacity / 256, 1, 1);
    vk.barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_ACCESS_SHADER_WRITE_BIT,
               VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
                   VK_ACCESS_TRANSFER_READ_BIT);
    if (q)
        vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q, 4);
    cloudReady = true;
}
VisualVoxelizer::~VisualVoxelizer() {
    for (auto &i : lods)
        vk.destroy(i);
    for (auto &r : roots)
        vk.destroy(r);
    vk.destroy(slots);
    vk.destroy(counters);
    vk.destroy(instances);
    vk.destroy(visibleTriangles);
    vk.destroy(candidates);
    for (auto p :
         {lodPipeline, resolveLodPipeline, hashPipeline, compactPipeline, visibilityPipeline,
          triangleLodPipeline, triangleEmitPipeline, triangleHashPipeline})
        vkDestroyPipeline(vk.device, p, nullptr);
    vkDestroyPipelineLayout(vk.device, layout, nullptr);
    vkFreeDescriptorSets(vk.device, vk.descriptorPool, 2, sets.data());
    vkDestroyDescriptorSetLayout(vk.device, setLayout, nullptr);
}
} // namespace micro
