#include "hud.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
namespace micro {
// Tiny built-in 5x7 font avoids an external UI/asset dependency.
static const std::map<char, std::array<uint8_t, 7>> font = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 14}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},      {':', {0, 12, 12, 0, 12, 12, 0}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},       {'/', {1, 2, 2, 4, 8, 8, 16}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},       {'[', {14, 8, 8, 8, 8, 8, 14}},
    {']', {14, 2, 2, 2, 2, 2, 14}},      {'=', {0, 0, 31, 0, 31, 0, 0}},
    {'%', {25, 25, 2, 4, 8, 19, 19}}};
Hud::Hud(VulkanContext &context, VkRenderPass pass, VkDescriptorSetLayout set) : vk(context) {
    vertices = vk.buffer(4 * 1024 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);
    layout = vk.pipelineLayout(set);
    pipeline = vk.graphics(layout, pass, "hud.vert", "hud.frag", 1, false, false, true);
}
Hud::~Hud() {
    vk.destroy(vertices);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    vkDestroyPipelineLayout(vk.device, layout, nullptr);
}
void Hud::rect(float x, float y, float w, float h, glm::vec4 c) {
    glm::vec2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    for (int i : {0, 1, 2, 0, 2, 3})
        mesh.push_back(
            {{p[i].x / float(vk.extent.width) * 2 - 1, p[i].y / float(vk.extent.height) * 2 - 1},
             c});
}
void Hud::text(float x, float y, const std::string &s, glm::vec4 c) {
    for (char letter : s) {
        char ch = char(std::toupper(static_cast<unsigned char>(letter)));
        auto f = font.find(ch);
        if (f != font.end())
            for (int r = 0; r < 7; r++)
                for (int col = 0; col < 5; col++)
                    if (f->second[r] & (1 << (4 - col)))
                        rect(x + col * 1.25f, y + r * 1.25f, 1.25f, 1.25f, c);
        x += 7.5f;
    }
}
static std::string number(double v, int precision = 2) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(precision) << v;
    return s.str();
}
void Hud::build(const Settings &s, const Counters &c, const std::array<double, 6> &t, size_t tris,
                uint32_t width, uint32_t height, float fps, double cubeDrawTime, uint32_t stride,
                const std::array<double, 8> &stages, bool isolated) {
    mesh.clear();
    if (!s.visibleHud)
        return;
    rect(8, 8, 430, 476, {.01f, .018f, .03f, .90f});
    text(18, 18, "MICROVOXELS - CONTINUOUS SOURCE / UNLIT CUBES", {.30f, .88f, .70f, 1});
    for (int i = 0; i < 3; i++) {
        rect(18 + 140 * i, 37, 130, 21,
             s.mode == i ? glm::vec4(.07, .24, .25, 1) : glm::vec4(.07, .10, .14, 1));
        text(25 + 140 * i, 43, i == 0 ? "SOURCE" : (i == 1 ? "MICROVOXELS" : "SIDE BY SIDE"));
    }
    rect(18, 64, 200, 21, s.frozen ? glm::vec4(.32, .14, .06, 1) : glm::vec4(.07, .10, .14, 1));
    text(26, 70, s.frozen ? "F: UNFREEZE CLOUD" : "F: FREEZE CLOUD");
    text(238, 70, s.cubeLight ? "G: CUBE LIGHT ON" : "G: CUBE LIGHT OFF");
    text(18, 94, "BASE " + number(s.base * 1000, 1) + " MM");
    rect(208, 94, 210, 9, {.08, .13, .18, 1});
    rect(208, 94, 210 * (s.base - .005f) / .145f, 9, {.15, .60, .45, 1});
    text(18, 113, "LOD START " + number(s.distance, 1) + " M");
    rect(208, 113, 210, 9, {.08, .13, .18, 1});
    rect(208, 113, 210 * (s.distance - 1) / 11, 9, {.15, .60, .45, 1});
    text(18, 133, "LEVELS " + std::to_string(s.levels) + "   1-6");
    text(208, 133, s.average ? "O: AVERAGE RGB" : "O: CLOSEST RGB");
    text(18, 151,
         std::string("X: ") + (s.supersampling ? "2X2 SAMPLES" : "1X SAMPLES") + "   H: LIGHT " +
             std::to_string(s.lighting) + "   P: PAUSE");
    text(18, 173,
         "SOURCE " + std::to_string(width) + " X " + std::to_string(height) + "  TRI " +
             std::to_string(tris));
    text(18, 189, "SAMPLES " + std::to_string(width * height) + "  HITS " + std::to_string(c.hits));
    text(18, 205,
         "WRITES " + std::to_string(c.candidateWrites) + "  CUBES " +
             std::to_string(c.instanceCount));
    text(18, 221,
         "WRITES/HIT " + number(c.hits ? double(c.candidateWrites) / c.hits : 0) + "  MAX CELLS " +
             std::to_string(c.maxFootprintCells));
    text(18, 237,
         "RADIUS " + std::to_string(s.footprintRadius) + "  LIMIT " +
             std::to_string(s.footprintLimit) + "  EXTENT " +
             number(double(c.maxFootprintExtent) / 1024));
    std::string lod = "LOD";
    for (int i = 0; i < s.levels; i++)
        lod += " " + std::to_string(i) + ":" + std::to_string(c.perLod[i]);
    text(18, 253, lod);
    text(18, 269,
         "FPS " + number(fps, 1) + "  FRAME " + number(1000 / std::max(fps, .01f)) + " MS");
    text(18, 285,
         "SOURCE " + number(t[0]) + "  GEN " + number(t[1] + t[2] + t[3]) +
             (isolated ? "  DRAW " : "  RENDER ") + number(isolated ? cubeDrawTime : t[4]) + " MS");
    text(18, 301,
         "DROPPED " + std::to_string(c.dropped) + "  CLAMPED " +
             std::to_string(c.clampedFootprints));
    text(18, 317, "REJECTED NEIGHBORS " + std::to_string(c.rejectedNeighbors));
    text(18, 337, s.footprintSplats ? "T: CONSERVATIVE FOOTPRINT SPLATS" : "T: POINT SPLATS");
    text(18, 355, std::string("C: PROJECTED-SIZE LOD ") + (s.adaptiveLod ? "ON" : "OFF"));
    text(18, 373,
         c.hits ? "WASD QE / RIGHT MOUSE / R RESET / F1 HUD" : "NO SOURCE HITS - R: RESET VIEW");
    text(18, 395, "GLOBAL HASH ATTEMPTS " + std::to_string(c.globalHashAttempts));
    text(18, 411,
         "LOAD " + number(100.0 * c.instanceCount / (1 << 21), 2) + "%  CONTRIBUTORS/CELL " +
             number(c.instanceCount ? double(c.candidateWrites) / c.instanceCount : 0));
    text(18, 427,
         "INSTANCE " + std::to_string(stride) + " B  ACTIVE " +
             number(double(c.instanceCount) * stride / 1048576, 2) + " MIB");
    if (isolated) {
        text(18, 443,
             "CLEAR " + number(stages[0]) + "  LOD " + number(stages[2] + stages[3]) + " MS");
        text(18, 459,
             "HASH " + number(stages[4]) + "  RESOLVE " +
                 number(stages[5] + stages[6] + stages[7]) + " MS");
    } else {
        text(18, 443, "PASS TIMES: --PROFILE-STAGES");
        double frame = 0;
        for (double interval : t)
            frame += interval;
        text(18, 459, "GPU FRAME " + number(frame) + " MS");
    }
    if (mesh.size() * sizeof(HudVertex) > vertices.size)
        throw std::runtime_error("HUD buffer too small");
    std::memcpy(vertices.mapped, mesh.data(), mesh.size() * sizeof(HudVertex));
}
void Hud::draw() {
    if (mesh.empty())
        return;
    vk.viewport(0, 0, float(vk.extent.width), float(vk.extent.height));
    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize zero = 0;
    vkCmdBindVertexBuffers(vk.cmd, 0, 1, &vertices.handle, &zero);
    vkCmdDraw(vk.cmd, uint32_t(mesh.size()), 1, 0, 0);
}
void Hud::click(double x, double y, Settings &s) {
    if (!s.visibleHud)
        return;
    if (y >= 37 && y <= 58 && x >= 18 && x <= 438) {
        s.mode = std::clamp(int((x - 18) / 140), 0, 2);
        return;
    }
    if (y >= 64 && y <= 85) {
        if (x >= 18 && x <= 218)
            s.frozen = !s.frozen;
        else if (x >= 238 && x <= 430)
            s.cubeLight = !s.cubeLight;
    }
    if (y >= 333 && y <= 350 && x >= 18 && x <= 430)
        s.footprintSplats = !s.footprintSplats;
    if (y >= 351 && y <= 369 && x >= 18 && x <= 430)
        s.adaptiveLod = !s.adaptiveLod;
    if (x >= 208 && x <= 418) {
        if (y >= 89 && y <= 107)
            s.base = .005f + .145f * float((x - 208) / 210);
        if (y >= 108 && y <= 127)
            s.distance = 1 + 11 * float((x - 208) / 210);
    }
    if (y >= 130 && y <= 145) {
        if (x < 208)
            s.levels = s.levels % 6 + 1;
        else
            s.average = !s.average;
    }
    if (y >= 147 && y <= 162) {
        if (x < 200)
            s.supersampling = !s.supersampling;
        else if (x < 330)
            s.lighting = (s.lighting + 1) % 3;
        else
            s.paused = !s.paused;
    }
}
} // namespace micro
