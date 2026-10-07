#include "hud.hpp"
#include "lattice.hpp"
#include "source_renderer.hpp"
#include "voxel_renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace micro;
struct Options {
    int width = 1100, height = 720, frames = 0;
    bool hidden = false, verify = false, validation = false, exercise = false, noUi = false;
    float fixedTime = -1;
    std::string screenshot, report;
    Settings settings;
};
Options parse(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto value = [&]() {
            if (++i >= argc)
                throw std::runtime_error("Missing value for " + a);
            return std::string(argv[i]);
        };
        if (a == "--width")
            o.width = std::stoi(value());
        else if (a == "--height")
            o.height = std::stoi(value());
        else if (a == "--frames")
            o.frames = std::stoi(value());
        else if (a == "--hidden")
            o.hidden = true;
        else if (a == "--verify")
            o.verify = true;
        else if (a == "--validation")
            o.validation = true;
        else if (a == "--exercise")
            o.exercise = true;
        else if (a == "--no-ui")
            o.noUi = true;
        else if (a == "--time")
            o.fixedTime = std::stof(value());
        else if (a == "--screenshot")
            o.screenshot = value();
        else if (a == "--report")
            o.report = value();
        else if (a == "--voxel-size")
            o.settings.base = std::stof(value());
        else if (a == "--lod-distance")
            o.settings.distance = std::stof(value());
        else if (a == "--levels")
            o.settings.levels = std::stoi(value());
        else if (a == "--mode")
            o.settings.mode = std::stoi(value());
        else if (a == "--lighting")
            o.settings.lighting = std::stoi(value());
        else if (a == "--debug-cubes")
            o.settings.cubeLight = true;
        else if (a == "--supersampling")
            o.settings.supersampling = true;
        else if (a == "--closest")
            o.settings.average = false;
        else if (a == "--help") {
            std::cout
                << "Microvoxels: Vulkan triangle samples -> unlit cubic splats\n"
                << "--width N --height N --frames N --hidden --validation --verify --exercise\n"
                << "--voxel-size METERS --lod-distance METERS --levels 1..6 --mode 0|1|2\n"
                << "--lighting 0|1|2 --debug-cubes --closest --supersampling --time SECONDS\n"
                << "--no-ui --screenshot FILE.ppm --report FILE.json\n"
                << "WASD/QE fly; right mouse look; Tab view; F freeze; G cube-light debug;\n"
                << "O average/closest; X supersampling; H lighting; P pause; +/- voxel size;\n"
                << "[/] LOD distance; 1..6 LOD count; F1 HUD; Esc exit.\n";
            std::exit(0);
        } else
            throw std::runtime_error("Unknown option: " + a);
    }
    if (o.width < 160 || o.width > 4096 || o.height < 120 || o.height > 2160 || o.frames < 0)
        throw std::runtime_error("Invalid window dimensions or frame count");
    if (o.settings.base < .005f || o.settings.base > .15f || o.settings.distance < 1 ||
        o.settings.distance > 12 || o.settings.levels < 1 || o.settings.levels > 6 ||
        o.settings.mode < 0 || o.settings.mode > 2 || o.settings.lighting < 0 ||
        o.settings.lighting > 2)
        throw std::runtime_error("Settings out of range; see --help");
    if (o.exercise && o.frames < 8)
        throw std::runtime_error("--exercise requires --frames 8 or more");
    o.settings.visibleHud = !o.noUi;
    return o;
}
struct Input {
    Settings *settings;
    Hud *hud;
    double lastX{}, lastY{};
    bool looking = false;
    float yaw = -2.14f, pitch = -.27f;
};
void keyCallback(GLFWwindow *w, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;
    auto &s = *static_cast<Input *>(glfwGetWindowUserPointer(w))->settings;
    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, 1);
    if (key == GLFW_KEY_TAB)
        s.mode = (s.mode + 1) % 3;
    if (key == GLFW_KEY_F)
        s.frozen = !s.frozen;
    if (key == GLFW_KEY_G)
        s.cubeLight = !s.cubeLight;
    if (key == GLFW_KEY_O)
        s.average = !s.average;
    if (key == GLFW_KEY_X)
        s.supersampling = !s.supersampling;
    if (key == GLFW_KEY_H)
        s.lighting = (s.lighting + 1) % 3;
    if (key == GLFW_KEY_P)
        s.paused = !s.paused;
    if (key == GLFW_KEY_F1)
        s.visibleHud = !s.visibleHud;
    if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD)
        s.base = std::min(.15f, s.base * 1.12f);
    if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT)
        s.base = std::max(.005f, s.base / 1.12f);
    if (key == GLFW_KEY_LEFT_BRACKET)
        s.distance = std::max(1.f, s.distance - .25f);
    if (key == GLFW_KEY_RIGHT_BRACKET)
        s.distance = std::min(12.f, s.distance + .25f);
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6)
        s.levels = key - GLFW_KEY_0;
}
void mouseButton(GLFWwindow *w, int button, int action, int) {
    auto &in = *static_cast<Input *>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        in.looking = action == GLFW_PRESS;
        glfwGetCursorPos(w, &in.lastX, &in.lastY);
        glfwSetInputMode(w, GLFW_CURSOR, in.looking ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !in.looking) {
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        int fw, fh, ww, wh;
        glfwGetFramebufferSize(w, &fw, &fh);
        glfwGetWindowSize(w, &ww, &wh);
        in.hud->click(x * fw / ww, y * fh / wh, *in.settings);
    }
}
void mouseMove(GLFWwindow *w, double x, double y) {
    auto &in = *static_cast<Input *>(glfwGetWindowUserPointer(w));
    if (in.looking) {
        in.yaw += float(x - in.lastX) * .003f;
        in.pitch = std::clamp(in.pitch - float(y - in.lastY) * .003f, -1.5f, 1.5f);
    }
    in.lastX = x;
    in.lastY = y;
}
void copyImage(VulkanContext &vk, Image &im, Buffer &buffer, VkImageLayout layout) {
    vk.imageBarrier(im, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy c{};
    c.imageSubresource = {im.aspect, 0, 0, 1};
    c.imageExtent = {im.width, im.height, 1};
    vkCmdCopyImageToBuffer(vk.cmd, im.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.handle,
                           1, &c);
    vk.imageBarrier(im, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
}
struct Readback {
    VulkanContext &vk;
    Buffer counters, instances, positions, colors, lods, pixels;
    Readback(VulkanContext &context, bool verify) : vk(context) {
        counters = vk.buffer(sizeof(Counters), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
        if (verify)
            instances = vk.buffer(VisualVoxelizer::Capacity * sizeof(Voxel),
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    void resize(uint32_t w, uint32_t h, VkExtent2D display, bool verify, bool screenshot) {
        for (auto b : {&positions, &colors, &lods, &pixels})
            vk.destroy(*b);
        if (verify) {
            positions = vk.buffer(w * uint64_t(h) * 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
            colors = vk.buffer(positions.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
            lods = vk.buffer(w * uint64_t(h) * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
        }
        if (screenshot)
            pixels = vk.buffer(display.width * uint64_t(display.height) * 4,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    ~Readback() {
        for (auto b : {&counters, &instances, &positions, &colors, &lods, &pixels})
            vk.destroy(*b);
    }
};
struct ReferenceCell {
    uint64_t r{}, g{}, b{}, count{};
    uint32_t nearest = UINT32_MAX;
    float nearestDepth = 1e30f;
};
using RootHistory = std::map<std::array<int32_t, 3>, int>;
void verify(const Parameters &p, const Frame &f, const Readback &readback, const Counters &c,
            RootHistory &history) {
    if (c.vertexCount != 36 || c.firstVertex != 0 || c.firstInstance != 0)
        throw std::runtime_error("Invalid GPU indirect cube draw command");
    auto *positions = static_cast<const glm::vec4 *>(readback.positions.mapped);
    auto *colors = static_cast<const glm::vec4 *>(readback.colors.mapped);
    auto *lods = static_cast<const uint32_t *>(readback.lods.mapped);
    auto *instances = static_cast<const Voxel *>(readback.instances.mapped);
    if (c.dropped || c.rootDropped)
        throw std::runtime_error("Verification requires an unsaturated hash table");
    std::map<Cell, ReferenceCell> expected;
    RootHistory current;
    uint32_t hits = 0;
    std::array<uint32_t, 8> expectedLods{};
    for (uint32_t i = 0; i < uint32_t(p.extent.x * p.extent.y); i++)
        if (positions[i].w != 0) {
            glm::vec3 pos = glm::vec3(positions[i]);
            auto root4 = cell({pos.x, pos.y, pos.z}, p.config.x, p.flags.x - 1);
            std::array<int32_t, 3> root = {root4[0], root4[1], root4[2]};
            auto centerArray = center(root4, p.config.x);
            glm::vec3 rc(centerArray[0], centerArray[1], centerArray[2]);
            auto old = history.find(root);
            int expectedLod =
                chooseLod(glm::distance(rc, glm::vec3(f.cameraTime)), p.config.y, p.flags.x,
                          old == history.end() ? -1 : old->second, p.config.z);
            if (lods[i] != uint32_t(expectedLod))
                throw std::runtime_error(
                    "GPU LOD differs from world-root CPU hysteresis reference");
            current[root] = expectedLod;
            auto key = cell({pos.x, pos.y, pos.z}, p.config.x, int(lods[i]));
            auto &ref = expected[key];
            hits++;
            ref.count++;
            ref.r += uint32_t(std::round(std::clamp(colors[i].r, 0.f, 1.f) * 1023));
            ref.g += uint32_t(std::round(std::clamp(colors[i].g, 0.f, 1.f) * 1023));
            ref.b += uint32_t(std::round(std::clamp(colors[i].b, 0.f, 1.f) * 1023));
            float d = glm::distance(pos, glm::vec3(f.cameraTime));
            if (d < ref.nearestDepth || (d == ref.nearestDepth && i < ref.nearest)) {
                ref.nearestDepth = d;
                ref.nearest = i;
            }
        }
    if (hits != c.hits || expected.size() != c.instanceCount)
        throw std::runtime_error("GPU hit or unique-cell count differs from CPU reference");
    for (uint32_t i = 0; i < c.instanceCount; i++) {
        auto &v = instances[i];
        int lod = int(std::round(std::log2(v.centerSize.w / (p.config.x * p.config.w))));
        auto key = cell({v.centerSize.x, v.centerSize.y, v.centerSize.z}, p.config.x, lod);
        auto found = expected.find(key);
        if (found == expected.end())
            throw std::runtime_error("Duplicate or unexpected GPU cell");
        auto &ref = found->second;
        auto cen = center(key, p.config.x);
        for (int axis = 0; axis < 3; axis++)
            if (std::abs(v.centerSize[axis] - cen[axis]) > .00005f)
                throw std::runtime_error("Voxel lattice centre mismatch");
        glm::vec3 wanted = p.flags.y ? glm::vec3(ref.r, ref.g, ref.b) / (1023.f * float(ref.count))
                                     : glm::vec3(colors[ref.nearest]);
        for (int axis = 0; axis < 3; axis++)
            if (std::abs(v.rgba[axis] - wanted[axis]) > .0021f)
                throw std::runtime_error("Voxel RGB differs from source-sample reduction");
        if (v.rgba.w != 1)
            throw std::runtime_error("Unexpected opacity");
        expectedLods[lod]++;
        expected.erase(found);
    }
    for (int i = 0; i < 8; i++)
        if (expectedLods[i] != c.perLod[i])
            throw std::runtime_error("LOD counters mismatch");
    history = std::move(current);
}
void screenshot(const std::string &path, const VulkanContext &vk, const Readback &r) {
    if (path.empty())
        return;
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot write screenshot");
    f << "P6\n" << vk.extent.width << ' ' << vk.extent.height << "\n255\n";
    auto *p = static_cast<const uint8_t *>(r.pixels.mapped);
    bool bgra = vk.swapFormat == VK_FORMAT_B8G8R8A8_SRGB;
    for (uint32_t i = 0; i < vk.extent.width * vk.extent.height; i++) {
        char rgb[3] = {char(p[i * 4 + (bgra ? 2 : 0)]), char(p[i * 4 + 1]),
                       char(p[i * 4 + (bgra ? 0 : 2)])};
        f.write(rgb, 3);
    }
}
int main(int argc, char **argv) {
    try {
        Options opt = parse(argc, argv);
        VulkanContext vk(opt.width, opt.height, opt.hidden, opt.validation);
        Buffer frameBuffer = vk.buffer(sizeof(Frame), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
        {
            SourceWorld world;
            SourceRenderer source(vk, world, frameBuffer);
            VisualVoxelizer voxelizer(vk, frameBuffer);
            VoxelRenderer renderer(vk, voxelizer);
            Hud hud(vk, renderer.pass, voxelizer.setLayout);
            Readback readback(vk, opt.verify || opt.exercise);
            Settings &s = opt.settings;
            Input input{&s, &hud};
            glfwSetWindowUserPointer(vk.window, &input);
            glfwSetKeyCallback(vk.window, keyCallback);
            glfwSetMouseButtonCallback(vk.window, mouseButton);
            glfwSetCursorPosCallback(vk.window, mouseMove);
            VkQueryPool queries{};
            if (vk.timestampBits) {
                VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
                ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
                ci.queryCount = 7;
                check(vkCreateQueryPool(vk.device, &ci, nullptr, &queries), "timestamp query pool");
            }
            bool resize = true;
            Settings previous = s;
            Counters counts{};
            std::array<double, 6> times{}, totals{};
            int timedFrames = 0;
            RootHistory history;
            std::vector<Voxel> frozenSnapshot;
            Counters frozenCounts{};
            int frozenChecks = 0, verifiedFrames = 0;
            glm::vec3 camera(5.8f, 3.6f, 8.4f);
            auto last = std::chrono::steady_clock::now();
            float animation = 0, fps = 60;
            int frameNumber = 0;
            Parameters parameters{};
            while (!glfwWindowShouldClose(vk.window) &&
                   (opt.frames == 0 || frameNumber < opt.frames)) {
                glfwPollEvents();
                int fw, fh;
                glfwGetFramebufferSize(vk.window, &fw, &fh);
                if (fw == 0 || fh == 0) {
                    glfwWaitEventsTimeout(.1);
                    continue;
                }
                vk.wait();
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - last).count();
                last = now;
                if (frameNumber > 0)
                    fps = frameNumber == 1 ? 1.f / std::max(dt, .0001f)
                                           : fps * .9f + .1f / std::max(dt, .0001f);
                float moveDt = std::min(dt, .05f);
                if (opt.exercise) {
                    if (frameNumber == 2)
                        s.frozen = true;
                    if (frameNumber == 3) {
                        camera += glm::vec3(-2, 0, -1);
                        input.yaw -= .45f;
                    }
                    if (frameNumber == 5)
                        s.frozen = false;
                    if (frameNumber == 6)
                        s.average = false;
                    if (frameNumber == 7)
                        s.cubeLight = true;
                }
                if (s.supersampling != previous.supersampling)
                    resize = true;
                if (s.base != previous.base || s.levels != previous.levels) {
                    voxelizer.resetHistory = true;
                    history.clear();
                }
                if (uint32_t(fw) != vk.extent.width || uint32_t(fh) != vk.extent.height) {
                    vk.resizeSwapchain();
                    resize = true;
                }
                if (resize) {
                    float scale = s.supersampling ? 2.f : 1.f;
                    scale = std::min(scale, 2048.f / float(std::max(fw, fh)));
                    uint32_t w = std::max(1u, uint32_t(fw * scale)),
                             h = std::max(1u, uint32_t(fh * scale));
                    source.resize(w, h);
                    voxelizer.resize(source.samples());
                    renderer.resize();
                    readback.resize(w, h, vk.extent, opt.verify, !opt.screenshot.empty());
                    history.clear();
                    resize = false;
                }
                glm::vec3 forward(std::cos(input.pitch) * std::cos(input.yaw),
                                  std::sin(input.pitch),
                                  std::cos(input.pitch) * std::sin(input.yaw));
                glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                float speed = glfwGetKey(vk.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 8.f : 3.f;
                auto pressed = [&](int k) { return glfwGetKey(vk.window, k) == GLFW_PRESS; };
                camera += speed * moveDt *
                          (forward * float(pressed(GLFW_KEY_W) - pressed(GLFW_KEY_S)) +
                           right * float(pressed(GLFW_KEY_D) - pressed(GLFW_KEY_A)) +
                           glm::vec3(0, 1, 0) * float(pressed(GLFW_KEY_E) - pressed(GLFW_KEY_Q)));
                if (!s.paused)
                    animation += opt.frames ? 1.f / 60 : moveDt;
                float time = opt.fixedTime >= 0 ? opt.fixedTime : animation;
                Frame frame{};
                float aspect = float(s.mode == 2 ? vk.extent.width / 2 : vk.extent.width) /
                               float(vk.extent.height);
                glm::mat4 projection = glm::perspective(glm::radians(55.f), aspect, .05f, 100.f);
                projection[1][1] *= -1;
                frame.vp = projection * glm::lookAt(camera, camera + forward, glm::vec3(0, 1, 0));
                glm::vec3 light = glm::normalize(glm::vec3(.5, .9, .3));
                glm::mat4 lp = glm::ortho(-9.f, 9.f, -9.f, 9.f, .1f, 35.f);
                lp[1][1] *= -1;
                frame.lightVP = lp * glm::lookAt(glm::vec3(0, 1, 0) + light * 13.f,
                                                 glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
                frame.cameraTime = glm::vec4(camera, time);
                frame.light = glm::vec4(light, 1.15f);
                frame.options = glm::vec4(float(s.lighting), 0, 0, 0);
                std::memcpy(frameBuffer.mapped, &frame, sizeof(frame));
                parameters.extent = {int(source.width), int(source.height),
                                     int(VisualVoxelizer::Capacity),
                                     int(VisualVoxelizer::Capacity)};
                parameters.config = {s.base, s.distance, .1f, s.splat};
                parameters.flags = {s.levels, s.average ? 1 : 0, s.cubeLight ? 1 : 0, s.mode};
                bool generating = !s.frozen || !voxelizer.cloudReady;
                bool referenceReset = voxelizer.resetHistory;
                if (referenceReset)
                    history.clear();
                hud.build(s, counts, times, world.triangleCount(), source.width * source.height,
                          fps);
                uint32_t swapIndex;
                VkResult acquired = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                                                          vk.acquired, VK_NULL_HANDLE, &swapIndex);
                if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
                    vk.resizeSwapchain();
                    resize = true;
                    continue;
                }
                if (acquired != VK_SUBOPTIMAL_KHR)
                    check(acquired, "acquire image");
                vk.begin();
                if (queries) {
                    vkCmdResetQueryPool(vk.cmd, queries, 0, 7);
                    vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
                }
                source.render(voxelizer.current, parameters);
                if (queries)
                    vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
                if (generating)
                    voxelizer.generate(parameters, queries);
                else if (queries)
                    for (uint32_t i = 2; i <= 4; i++)
                        vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries,
                                            i);
                renderer.begin(parameters);
                renderer.draw(parameters);
                if (queries)
                    vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 5);
                hud.draw();
                renderer.end();
                if (queries)
                    vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 6);
                vk.barrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                VkBufferCopy counterCopy{0, 0, sizeof(Counters)};
                vkCmdCopyBuffer(vk.cmd, voxelizer.counters.handle, readback.counters.handle, 1,
                                &counterCopy);
                if (opt.verify || opt.exercise) {
                    VkBufferCopy c{0, 0, voxelizer.instances.size};
                    vkCmdCopyBuffer(vk.cmd, voxelizer.instances.handle, readback.instances.handle,
                                    1, &c);
                }
                if (opt.verify && generating) {
                    copyImage(vk, source.positions[voxelizer.current], readback.positions,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    copyImage(vk, source.color, readback.colors,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    copyImage(vk, voxelizer.lods[voxelizer.current], readback.lods,
                              VK_IMAGE_LAYOUT_GENERAL);
                }
                if (!opt.screenshot.empty())
                    copyImage(vk, renderer.output, readback.pixels,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkImageMemoryBarrier sb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                sb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                sb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                sb.srcQueueFamilyIndex = sb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                sb.image = vk.swapImages[swapIndex];
                sb.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                sb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(vk.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &sb);
                VkImageCopy imageCopy{};
                imageCopy.srcSubresource =
                    imageCopy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                imageCopy.extent = {vk.extent.width, vk.extent.height, 1};
                vkCmdCopyImage(vk.cmd, renderer.output.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               vk.swapImages[swapIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &imageCopy);
                sb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                sb.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                sb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sb.dstAccessMask = 0;
                vkCmdPipelineBarrier(vk.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                                     nullptr, 1, &sb);
                vk.barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                           VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
                vk.submit(swapIndex);
                VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
                present.waitSemaphoreCount = 1;
                present.pWaitSemaphores = &vk.presented[swapIndex];
                present.swapchainCount = 1;
                present.pSwapchains = &vk.swapchain;
                present.pImageIndices = &swapIndex;
                VkResult pr = vkQueuePresentKHR(vk.queue, &present);
                if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
                    resize = true;
                else
                    check(pr, "present");
                vk.wait();
                std::memcpy(&counts, readback.counters.mapped, sizeof(counts));
                if (queries) {
                    std::array<uint64_t, 7> stamps{};
                    check(vkGetQueryPoolResults(vk.device, queries, 0, 7, sizeof(stamps),
                                                stamps.data(), 8,
                                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
                          "read timestamps");
                    uint64_t mask = vk.timestampBits == 64
                                        ? UINT64_MAX
                                        : ((uint64_t(1) << vk.timestampBits) - 1);
                    for (int i = 0; i < 6; i++)
                        times[i] =
                            double((stamps[i + 1] - stamps[i]) & mask) * vk.timestampPeriod / 1e6;
                    if (frameNumber >= 2 && generating) {
                        for (int i = 0; i < 6; i++)
                            totals[i] += times[i];
                        timedFrames++;
                    }
                }
                if (opt.verify && generating) {
                    verify(parameters, frame, readback, counts, history);
                    verifiedFrames++;
                }
                if (opt.exercise) {
                    auto *data = static_cast<const Voxel *>(readback.instances.mapped);
                    if (frameNumber == 1) {
                        frozenSnapshot.assign(data, data + counts.instanceCount);
                        frozenCounts = counts;
                    }
                    if (frameNumber >= 2 && frameNumber <= 4) {
                        if (std::memcmp(&counts, &frozenCounts, sizeof(counts)) != 0 ||
                            std::memcmp(data, frozenSnapshot.data(),
                                        frozenSnapshot.size() * sizeof(Voxel)) != 0)
                            throw std::runtime_error("Freeze changed cloud contents or counters");
                        frozenChecks++;
                    }
                }
                if (generating)
                    voxelizer.advance();
                previous = s;
                frameNumber++;
                std::string title = "Microvoxels | " + std::to_string(counts.instanceCount) +
                                    " unlit cubes | " + std::to_string(counts.hits) + " hits | " +
                                    std::to_string(int(fps)) + " FPS" +
                                    (s.frozen ? " | FROZEN" : "");
                glfwSetWindowTitle(vk.window, title.c_str());
            }
            vkDeviceWaitIdle(vk.device);
            if (frameNumber > 0)
                screenshot(opt.screenshot, vk, readback);
            std::cout << "Frames: " << frameNumber << "; hits: " << counts.hits
                      << "; unique cubes: " << counts.instanceCount
                      << "; dropped: " << counts.dropped
                      << "; roots dropped: " << counts.rootDropped << '\n';
            if (opt.verify)
                std::cout << "GPU/CPU reference verified " << verifiedFrames
                          << " generated frames\n";
            if (opt.exercise)
                std::cout << "Frozen camera-motion checks: " << frozenChecks
                          << "; closest colour and cube-light debug exercised\n";
            if (timedFrames)
                for (int i = 0; i < 6; i++)
                    totals[i] /= timedFrames;
            const char *labels[] = {"source_and_shadow_ms", "lod_and_clear_ms", "hash_ms",
                                    "compact_ms",           "final_render_ms",  "hud_ms"};
            if (!opt.report.empty()) {
                auto parent = std::filesystem::path(opt.report).parent_path();
                if (!parent.empty())
                    std::filesystem::create_directories(parent);
                std::ofstream f(opt.report);
                if (!f)
                    throw std::runtime_error("Cannot write profile report");
                f << std::setprecision(6) << "{\n  \"device\": \"" << vk.gpuName
                  << "\",\n  \"source_backend\": \"Vulkan raster G-buffer\",\n  \"frames\": "
                  << frameNumber << ",\n  \"timed_frames_after_warmup\": " << timedFrames
                  << ",\n  \"triangles\": " << world.triangleCount()
                  << ",\n  \"samples\": " << source.width * source.height
                  << ",\n  \"hits\": " << counts.hits
                  << ",\n  \"unique_voxels\": " << counts.instanceCount
                  << ",\n  \"indirect_vertex_count\": " << counts.vertexCount
                  << ",\n  \"indirect_first_vertex\": " << counts.firstVertex
                  << ",\n  \"indirect_first_instance\": " << counts.firstInstance
                  << ",\n  \"dropped_hits\": " << counts.dropped
                  << ",\n  \"dropped_root_history_samples\": " << counts.rootDropped
                  << ",\n  \"verified_frames\": " << verifiedFrames
                  << ",\n  \"freeze_checks\": " << frozenChecks
                  << ",\n  \"validation_errors\": " << vk.validationErrors
                  << ",\n  \"timestamp_supported\": " << (queries ? "true" : "false")
                  << ",\n  \"voxels_per_lod\": [";
                for (int i = 0; i < s.levels; i++)
                    f << (i ? ", " : "") << counts.perLod[i];
                f << "],\n  \"average_gpu_timings\": {";
                for (int i = 0; i < 6; i++)
                    f << (i ? ", " : "") << "\"" << labels[i] << "\": " << totals[i];
                f << "}\n}\n";
            }
            if (queries)
                vkDestroyQueryPool(vk.device, queries, nullptr);
            if (vk.validationErrors)
                throw std::runtime_error("Vulkan validation reported errors");
        }
        vk.destroy(frameBuffer);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Microvoxels: " << e.what() << '\n';
        return 1;
    }
}
