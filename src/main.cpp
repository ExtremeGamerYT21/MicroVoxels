#include "hud.hpp"
#include "lattice.hpp"
#include "source_renderer.hpp"
#include "surface_footprint.hpp"
#include "voxel_cache.hpp"
#include "voxel_renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
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
    bool hidden = false, verify = false, validation = false, exercise = false, noUi = false,
         exerciseControls = false, exerciseStability = false, exerciseRender = false,
         exerciseCache = false;
    bool vsync = true;
    int freezeAfter = -1;
    float fixedTime = -1;
    SourceScene scene = SourceScene::Garden;
    std::string screenshot, report, captureSequence;
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
        if (a == "--scene") {
            auto scene = value();
            if (scene == "garden")
                o.scene = SourceScene::Garden;
            else if (scene == "test")
                o.scene = SourceScene::Test;
            else
                throw std::runtime_error("Unknown scene; use --scene garden or --scene test");
        } else if (a == "--width")
            o.width = std::stoi(value());
        else if (a == "--height")
            o.height = std::stoi(value());
        else if (a == "--frames")
            o.frames = std::stoi(value());
        else if (a == "--hidden")
            o.hidden = true;
        else if (a == "--no-vsync")
            o.vsync = false;
        else if (a == "--vsync")
            o.vsync = true;
        else if (a == "--verify")
            o.verify = true;
        else if (a == "--validation")
            o.validation = true;
        else if (a == "--exercise")
            o.exercise = true;
        else if (a == "--exercise-controls")
            o.exerciseControls = true;
        else if (a == "--exercise-stability")
            o.exerciseStability = true;
        else if (a == "--exercise-render")
            o.exerciseRender = true;
        else if (a == "--exercise-cache")
            o.exerciseCache = true;
        else if (a == "--voxel-cache")
            o.settings.voxelCache = true;
        else if (a == "--no-voxel-cache")
            o.settings.voxelCache = false;
        else if (a == "--cache-ms")
            o.settings.cacheMs = std::stof(value());
        else if (a == "--freeze-after")
            o.freezeAfter = std::stoi(value());
        else if (a == "--cube-mesh") {
            auto mesh = value();
            if (mesh != "legacy" && mesh != "indexed")
                throw std::runtime_error("Use --cube-mesh legacy|indexed");
            o.settings.indexedCubes = mesh == "indexed";
        } else if (a == "--cube-culling") {
            auto cull = value();
            if (cull != "off" && cull != "back")
                throw std::runtime_error("Use --cube-culling off|back");
            o.settings.cullCubes = cull == "back";
        } else if (a == "--sample-occupancy" || a == "--point-splats")
            o.settings.footprintSplats = false;
        else if (a == "--footprint-splats")
            o.settings.footprintSplats = true;
        else if (a == "--footprint-radius")
            o.settings.footprintRadius = std::stoi(value());
        else if (a == "--footprint-limit")
            o.settings.footprintLimit = std::stoi(value());
        else if (a == "--capture-sequence")
            o.captureSequence = value();
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
        else if (a == "--no-adaptive-lod")
            o.settings.adaptiveLod = false;
        else if (a == "--help") {
            std::cout
                << "Microvoxels: Vulkan triangle samples -> unlit cubic splats\n"
                << "--scene garden|test (default: garden)\n"
                << "--width N --height N --frames N --hidden --validation --verify --exercise\n"
                << "--voxel-size METERS --lod-distance METERS --levels 1..6 --mode 0|1|2\n"
                << "--lighting 0|1|2 --debug-cubes --closest --supersampling --time SECONDS\n"
                << "--no-ui --screenshot FILE.ppm --report FILE.json\n"
                << "--no-vsync (uncapped presentation) --vsync (default)\n"
                << "--voxel-cache --no-voxel-cache --cache-ms 0..500 (default: 100)\n"
                << "--exercise-cache (43 frames; static, motion, expiry, freeze, reset)\n"
                << "--point-splats --footprint-splats --footprint-radius 0..2 --footprint-limit "
                   "1..64\n"
                << "--capture-sequence DIRECTORY --no-adaptive-lod --exercise-controls (16 frames) "
                   "--exercise-stability (12 frames)\n"
                << "--cube-mesh legacy|indexed --cube-culling off|back (defaults: legacy/off)\n"
                << "--freeze-after N (finite-run draw benchmark) --exercise-render (19 frames)\n"
                << "WASD/QE fly; right mouse look; Tab view; F freeze; G cube-light debug;\n"
                << "O average/closest; X supersampling; H lighting; P pause; +/- voxel size;\n"
                << "[/] LOD distance; 1..6 LOD count; R reset view; C adaptive LOD; F1 HUD; Esc "
                   "exit; T point/footprint splats; I indexed cubes; B safe backface culling; "
                   "K voxel cache.\n";
            std::exit(0);
        } else
            throw std::runtime_error("Unknown option: " + a);
    }
    if (o.width < 160 || o.width > 4096 || o.height < 120 || o.height > 2160 || o.frames < 0)
        throw std::runtime_error("Invalid window dimensions or frame count");
    if (o.settings.base < .005f || o.settings.base > .15f || o.settings.distance < 1 ||
        o.settings.distance > 12 || o.settings.levels < 1 || o.settings.levels > 6 ||
        o.settings.mode < 0 || o.settings.mode > 2 || o.settings.lighting < 0 ||
        o.settings.lighting > 2 || o.settings.footprintRadius < 0 ||
        o.settings.footprintRadius > 2 || o.settings.footprintLimit < 1 ||
        o.settings.footprintLimit > 64 || !std::isfinite(o.settings.cacheMs) ||
        o.settings.cacheMs < 0 || o.settings.cacheMs > 500)
        throw std::runtime_error("Settings out of range; see --help");
    if (o.exercise && o.frames < 8)
        throw std::runtime_error("--exercise requires --frames 8 or more");
    if (o.exerciseControls && (o.frames < 16 || o.exercise))
        throw std::runtime_error(
            "--exercise-controls requires 16 frames and cannot combine with --exercise");
    if (o.exerciseStability && (o.frames < 12 || o.exercise || o.exerciseControls))
        throw std::runtime_error("--exercise-stability requires 12 frames and no other exercise");
    if (o.exerciseStability) {
        o.verify = true;
        o.fixedTime = 1;
    }
    if (o.exerciseRender) {
        if (o.frames < 19 || o.exercise || o.exerciseControls || o.exerciseStability)
            throw std::runtime_error("--exercise-render needs 19 frames and no other exercise");
        o.verify = true;
        o.fixedTime = 1;
    }
    if (o.exerciseCache) {
        if (o.frames < 43 || o.exercise || o.exerciseControls || o.exerciseStability ||
            o.exerciseRender)
            throw std::runtime_error("--exercise-cache needs 43 frames and no other exercise");
        o.verify = true;
        o.fixedTime = 1;
    }
    if (o.freezeAfter < -1 ||
        (o.freezeAfter >= 0 && (o.frames <= o.freezeAfter || o.exercise || o.exerciseControls ||
                                o.exerciseStability || o.exerciseRender || o.exerciseCache)))
        throw std::runtime_error("--freeze-after needs more frames than N and no exercise");
    o.settings.visibleHud = !o.noUi;
    return o;
}
struct Input {
    Settings *settings;
    Hud *hud;
    glm::vec3 *camera;
    double lastX{}, lastY{};
    bool looking = false, anchored = false;
    bool resetCacheRequested = true;
    float yaw = -2.14f, pitch = -.27f;
    glm::vec3 homeCamera{5.8f, 3.6f, 8.4f};
    float homeYaw = -2.14f, homePitch = -.27f;
};
void stopLooking(GLFWwindow *w, Input &in) {
    in.looking = in.anchored = false;
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}
void resetCamera(GLFWwindow *w, Input &in) {
    stopLooking(w, in);
    *in.camera = in.homeCamera;
    in.yaw = in.homeYaw;
    in.pitch = in.homePitch;
    in.settings->frozen = false;
    in.resetCacheRequested = true;
}
void focusCallback(GLFWwindow *w, int focused) {
    if (!focused)
        stopLooking(w, *static_cast<Input *>(glfwGetWindowUserPointer(w)));
}
void keyCallback(GLFWwindow *w, int key, int, int action, int) {
    bool adjustment = key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD || key == GLFW_KEY_MINUS ||
                      key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_LEFT_BRACKET ||
                      key == GLFW_KEY_RIGHT_BRACKET;
    if (action != GLFW_PRESS && !(action == GLFW_REPEAT && adjustment))
        return;
    auto &in = *static_cast<Input *>(glfwGetWindowUserPointer(w));
    auto &s = *in.settings;
    if (key == GLFW_KEY_R)
        resetCamera(w, in);
    if (key == GLFW_KEY_T)
        s.footprintSplats = !s.footprintSplats;
    if (key == GLFW_KEY_C)
        s.adaptiveLod = !s.adaptiveLod;
    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, 1);
    if (key == GLFW_KEY_TAB)
        s.mode = (s.mode + 1) % 3;
    if (key == GLFW_KEY_F)
        s.frozen = !s.frozen;
    if (key == GLFW_KEY_G)
        s.cubeLight = !s.cubeLight;
    if (key == GLFW_KEY_I)
        s.indexedCubes = !s.indexedCubes;
    if (key == GLFW_KEY_B)
        s.cullCubes = !s.cullCubes;
    if (key == GLFW_KEY_K)
        s.voxelCache = !s.voxelCache;
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
        // Cursor-mode changes can emit a queued recenter event. Rebase before
        // accepting relative motion, including after an Alt-Tab/focus change.
        in.looking = in.anchored = false;
        glfwSetInputMode(w, GLFW_CURSOR,
                         action == GLFW_PRESS ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (action == GLFW_PRESS && glfwRawMouseMotionSupported())
            glfwSetInputMode(w, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwGetCursorPos(w, &in.lastX, &in.lastY);
        in.looking = action == GLFW_PRESS;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !in.looking) {
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        int fw, fh, ww, wh;
        glfwGetFramebufferSize(w, &fw, &fh);
        glfwGetWindowSize(w, &ww, &wh);
        if (fw > 0 && fh > 0 && ww > 0 && wh > 0)
            in.hud->click(x * fw / ww, y * fh / wh, *in.settings);
    }
}
void mouseMove(GLFWwindow *w, double x, double y) {
    auto &in = *static_cast<Input *>(glfwGetWindowUserPointer(w));
    if (!std::isfinite(x) || !std::isfinite(y)) {
        in.anchored = false;
        return;
    }
    if (in.looking && in.anchored) {
        int ww, wh;
        glfwGetWindowSize(w, &ww, &wh);
        double dx = x - in.lastX, dy = y - in.lastY;
        if (std::abs(dx) <= std::max(ww, 1) && std::abs(dy) <= std::max(wh, 1)) {
            in.yaw = std::remainder(in.yaw + float(dx) * .003f, 6.2831853f);
            in.pitch = std::clamp(in.pitch - float(dy) * .003f, -1.5f, 1.5f);
        }
    }
    in.lastX = x;
    in.lastY = y;
    in.anchored = in.looking;
}
void exerciseControls(int frame, GLFWwindow *w, Input &in) {
    auto &s = *in.settings;
    auto press = [&](int key) { keyCallback(w, key, 0, GLFW_PRESS, 0); };
    if (frame == 0)
        press(GLFW_KEY_R);
    if (frame == 1) {
        float yaw = in.yaw, pitch = in.pitch;
        mouseButton(w, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
        mouseMove(w, 1e6, -1e6); // first motion must only establish a new baseline
        mouseMove(w, 0, 0);      // an implausible cursor warp must not rotate the view
        if (in.yaw != yaw || in.pitch != pitch)
            throw std::runtime_error("Mouse capture changed the camera orientation");
        mouseMove(w, 10, 5);
        if (in.yaw == yaw || in.pitch == pitch)
            throw std::runtime_error("Relative mouse look did not move the camera");
        focusCallback(w, GLFW_FALSE);
        yaw = in.yaw;
        mouseMove(w, 1000, 1000);
        if (in.looking || in.yaw != yaw)
            throw std::runtime_error("Focus loss left mouse look active");
        press(GLFW_KEY_R);
    }
    if (frame == 2) {
        *in.camera = {50, 50, 50};
        in.yaw = .7f;
        in.pitch = .5f;
    }
    if (frame == 3)
        s.frozen = true; // freeze an empty cloud, then recover it with the normal R handler
    if (frame == 4) {
        press(GLFW_KEY_R);
        if (s.frozen || in.looking)
            throw std::runtime_error("Camera reset did not resume sampling");
    }
    if (frame == 5) {
        keyCallback(w, GLFW_KEY_F, 0, GLFW_REPEAT, 0);
        if (s.frozen)
            throw std::runtime_error("A repeated key toggled freeze");
        press(GLFW_KEY_T);
        press(GLFW_KEY_C);
    }
    if (frame == 6) {
        if (!s.footprintSplats)
            press(GLFW_KEY_T);
        s.base = .0216f;
        s.distance = 8.6f;
        s.levels = 5;
    }
    if (frame == 7)
        press(GLFW_KEY_X);
    if (frame == 8) {
        press(GLFW_KEY_X);
        press(GLFW_KEY_TAB);
    }
    if (frame == 9) {
        press(GLFW_KEY_TAB);
        press(GLFW_KEY_1);
        s.base = .15f;
    }
    if (frame == 10) {
        press(GLFW_KEY_TAB);
        press(GLFW_KEY_6);
        press(GLFW_KEY_C);
        s.base = .0057f;
        int width, height;
        glfwGetWindowSize(w, &width, &height);
        glfwSetWindowSize(w, width + 64, height + 48);
    }
    if (frame == 11) {
        press(GLFW_KEY_1);
        s.base = .0216f;
    }
    if (frame == 12) {
        press(GLFW_KEY_6);
        s.base = .0057f;
    }
    if (frame == 13) {
        mouseButton(w, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
        mouseMove(w, 100, 100);
        mouseMove(w, 101, 101);
        mouseButton(w, GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, 0);
        mouseButton(w, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
        float yaw = in.yaw;
        mouseMove(w, 2000, -2000);
        if (in.yaw != yaw)
            throw std::runtime_error("Recapturing the mouse changed the view");
        focusCallback(w, GLFW_FALSE);
    }
    if (frame == 14) {
        mouseMove(w, std::nan(""), 0);
        in.yaw = std::nanf(""); // finite-pose guard must restore a usable camera
    }
    if (frame == 15)
        press(GLFW_KEY_R);
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
    Buffer counters, instances, positions, colors, lods, pixels, depths;
    Readback(VulkanContext &context, bool verify) : vk(context) {
        counters = vk.buffer(sizeof(Counters), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
        if (verify)
            instances = vk.buffer(VisualVoxelizer::Capacity * sizeof(Voxel),
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    void resize(uint32_t w, uint32_t h, VkExtent2D display, bool verify, bool capture) {
        for (auto b : {&positions, &colors, &lods, &pixels, &depths})
            vk.destroy(*b);
        if (verify) {
            positions = vk.buffer(w * uint64_t(h) * 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
            colors = vk.buffer(positions.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
            lods = vk.buffer(w * uint64_t(h) * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
            depths = vk.buffer(lods.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
        }
        if (capture)
            pixels = vk.buffer(display.width * uint64_t(display.height) * 4,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    }
    ~Readback() {
        for (auto b : {&counters, &instances, &positions, &colors, &lods, &pixels, &depths})
            vk.destroy(*b);
    }
};
struct ReferenceCell {
    uint64_t r{}, g{}, b{}, count{};
    uint32_t nearest = UINT32_MAX;
    float nearestDepth = 1e30f;
};
using RootHistory = std::map<std::array<int32_t, 3>, int>;
using LodChangeHistory = std::map<std::array<int32_t, 3>, uint32_t>;
struct CachedReference {
    Voxel voxel;
    float lastSeen;
};
using CacheHistory = std::map<Cell, CachedReference>;
void verify(const Parameters &p, const Frame &f, const Readback &readback, const Counters &c,
            RootHistory &history, LodChangeHistory &lodChanges, CacheHistory &cacheHistory,
            float &reconstructionError) {
    if (c.vertexCount != 36 || c.firstVertex != 0 || c.firstInstance != 0)
        throw std::runtime_error("Invalid GPU indirect cube draw command");
    auto *positions = static_cast<const glm::vec4 *>(readback.positions.mapped);
    auto *colors = static_cast<const glm::vec4 *>(readback.colors.mapped);
    auto *lods = static_cast<const uint32_t *>(readback.lods.mapped);
    auto *instances = static_cast<const Voxel *>(readback.instances.mapped);
    auto *depths = static_cast<const float *>(readback.depths.mapped);
    auto inverse = glm::inverse(f.vp);
    // Raster edge snapping and attribute interpolation are not exact inverse projection.
    // Allow an eighth of a source pixel, while checking world/depth error separately.
    glm::vec3 projectionTolerance(.25f / p.extent.x, .25f / p.extent.y, .00001f);
    if (c.dropped || c.rootDropped || c.cacheDropped)
        throw std::runtime_error("Verification requires an unsaturated hash table");
    std::map<Cell, ReferenceCell> expected;
    RootHistory current;
    // All samples in a world region must use its coarsest requested LOD.
    for (uint32_t i = 0; i < uint32_t(p.extent.x * p.extent.y); i++)
        if (positions[i].w != 0) {
            glm::vec3 pos(positions[i]);
            auto root4 = cell({pos.x, pos.y, pos.z}, p.config.x, p.flags.x - 1);
            std::array<int32_t, 3> root = {root4[0], root4[1], root4[2]};
            auto rc = center(root4, p.config.x);
            auto old = history.find(root);
            int previous = old == history.end() ? -1 : old->second;
            int lod =
                chooseLod(glm::distance(glm::vec3(rc[0], rc[1], rc[2]), glm::vec3(f.cameraTime)),
                          p.config.y, p.flags.x, previous, p.config.z);
            if (f.options.y != 0) {
                auto patch = sourceFootprint(positions, i, p, f);
                auto span = glm::abs(patch.dx) + glm::abs(patch.dy);
                float footprint = 1.25f * std::max({span.x, span.y, span.z});
                lod = std::max(
                    lod, chooseCoverageLod(footprint, p.config.x, p.flags.x, previous, p.config.z));
            }
            if (p.cache.y > 0 && p.cache.w != 0 && previous >= 0 &&
                uint32_t(p.cache.x * 1000) - lodChanges.at(root) < uint32_t(p.cache.y * 1000))
                lod = previous;
            auto [region, inserted] = current.emplace(root, lod);
            if (!inserted)
                region->second = std::max(region->second, lod);
        }
    uint32_t hits = 0, candidates = 0, maxCells = 0, clamped = 0, rejected = 0, maxExtent = 0;
    std::array<uint32_t, 8> expectedLods{};
    for (uint32_t i = 0; i < uint32_t(p.extent.x * p.extent.y); i++)
        if (positions[i].w != 0) {
            glm::vec3 pos(positions[i]);
            int x = int(i % uint32_t(p.extent.x)), y = int(i / uint32_t(p.extent.x));
            glm::vec4 ndc(2 * (x + .5f) / p.extent.x - 1, 2 * (y + .5f) / p.extent.y - 1, depths[i],
                          1);
            auto reconstructed = inverse * ndc;
            auto projected = f.vp * glm::vec4(pos, 1);
            float error = glm::distance(pos, glm::vec3(reconstructed) / reconstructed.w);
            reconstructionError = std::max(reconstructionError, error);
            if (error > .0005f * std::max(1.f, glm::distance(pos, glm::vec3(f.cameraTime))) ||
                glm::any(
                    glm::greaterThan(glm::abs(glm::vec3(projected) / projected.w - glm::vec3(ndc)),
                                     projectionTolerance)))
                throw std::runtime_error(
                    "Cached world position does not agree with Vulkan depth: pixel " +
                    std::to_string(i) + " error " + std::to_string(error) + " depth " +
                    std::to_string(depths[i]) + " expected " +
                    std::to_string(projected.z / projected.w) + " xy " +
                    std::to_string(projected.x / projected.w - ndc.x) + "," +
                    std::to_string(projected.y / projected.w - ndc.y));
            auto root4 = cell({pos.x, pos.y, pos.z}, p.config.x, p.flags.x - 1);
            std::array<int32_t, 3> root{root4[0], root4[1], root4[2]};
            int lod = current.at(root);
            if (lods[i] != uint32_t(lod))
                throw std::runtime_error("GPU LOD differs from CPU coverage/hysteresis reference");
            auto key = cell({pos.x, pos.y, pos.z}, p.config.x, lod);
            float h = std::ldexp(p.config.x, lod);
            auto patch = boundedFootprint(positions, i, p, f, h);
            auto extent = .5f * (glm::abs(patch.dx) + glm::abs(patch.dy));
            maxExtent =
                std::max(maxExtent,
                         uint32_t(std::round(std::max({extent.x, extent.y, extent.z}) / h * 1024)));
            rejected += patch.rejected;
            uint32_t emitted = 0;
            auto contribute = [&](const Cell &cellKey) {
                auto &ref = expected[cellKey];
                candidates++;
                emitted++;
                ref.count++;
                ref.r += uint32_t(std::round(std::clamp(colors[i].r, 0.f, 1.f) * 1023));
                ref.g += uint32_t(std::round(std::clamp(colors[i].g, 0.f, 1.f) * 1023));
                ref.b += uint32_t(std::round(std::clamp(colors[i].b, 0.f, 1.f) * 1023));
                float d = glm::distance(pos, glm::vec3(f.cameraTime));
                if (d < ref.nearestDepth || (d == ref.nearestDepth && i < ref.nearest)) {
                    ref.nearestDepth = d;
                    ref.nearest = i;
                }
            };
            hits++;
            contribute(key);
            glm::ivec3 own(key[0], key[1], key[2]), radius(p.footprint.x);
            auto first = glm::max(glm::ivec3(glm::floor((pos - extent) / h)) - own, -radius);
            auto last = glm::min(glm::ivec3(glm::floor((pos + extent) / h)) - own, radius);
            for (int z = first.z; z <= last.z; z++)
                for (int y = first.y; y <= last.y; y++)
                    for (int x = first.x; x <= last.x; x++) {
                        if (x == 0 && y == 0 && z == 0)
                            continue;
                        Cell other{key[0] + x, key[1] + y, key[2] + z, lod};
                        float span = float(1 << (p.flags.x - 1 - lod));
                        std::array<int32_t, 3> target{int32_t(std::floor(other[0] / span)),
                                                      int32_t(std::floor(other[1] / span)),
                                                      int32_t(std::floor(other[2] / span))};
                        auto region = current.find(target);
                        if (p.flags.x > 1 && (region == current.end() || region->second != lod))
                            continue;
                        auto c = center(other, p.config.x);
                        if (!referenceFootprintBox(pos, patch, {c[0], c[1], c[2]}, h))
                            continue;
                        if (emitted >= uint32_t(p.footprint.y)) {
                            patch.clamped = true;
                            continue;
                        }
                        contribute(other);
                    }
            maxCells = std::max(maxCells, emitted);
            if (patch.clamped)
                clamped++;
        }
    if (candidates != c.candidateWrites || maxCells != c.maxFootprintCells ||
        clamped != c.clampedFootprints || rejected != c.rejectedNeighbors ||
        std::abs(int64_t(maxExtent) - c.maxFootprintExtent) > 1)
        throw std::runtime_error(
            "GPU footprint candidate/limit statistics differ from CPU reference");
    if (p.cache.z == 0)
        cacheHistory.clear();
    CacheHistory retained, nextCache;
    uint32_t expired = 0, rejectedCache = 0;
    for (const auto &[key, old] : cacheHistory) {
        if (expected.contains(key))
            continue;
        if (p.cache.x - old.lastSeen > p.cache.y) {
            expired++;
            continue;
        }
        float pitch = std::ldexp(p.config.x, key[3]);
        if (!cacheSupported(key, glm::vec3(old.voxel.centerSize), pitch, p, f, positions,
                            current)) {
            rejectedCache++;
            continue;
        }
        retained.emplace(key, old);
    }
    if (c.cacheRetained != retained.size() || c.cacheExpired != expired ||
        c.cacheRejected != rejectedCache)
        throw std::runtime_error(
            "GPU voxel-cache retention/expiry differs from CPU source-buffer reference");
    if (hits != c.hits || expected.size() + retained.size() != c.instanceCount)
        throw std::runtime_error("GPU hit or unique-cell count differs from CPU reference");
    for (uint32_t i = 0; i < c.instanceCount; i++) {
        auto &v = instances[i];
        int lod = int(std::round(std::log2(v.centerSize.w / (p.config.x * p.config.w))));
        auto key = cell({v.centerSize.x, v.centerSize.y, v.centerSize.z}, p.config.x, lod);
        auto found = expected.find(key);
        if (found == expected.end()) {
            auto cached = retained.find(key);
            if (cached == retained.end())
                throw std::runtime_error("Duplicate or unexpected GPU cell");
            if (std::memcmp(&v, &cached->second.voxel, sizeof(Voxel)))
                throw std::runtime_error("Cached voxel changed its world cell or source RGB");
            nextCache.emplace(key, cached->second);
            retained.erase(cached);
            expectedLods[lod]++;
            continue;
        }
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
        nextCache.emplace(key, CachedReference{v, p.cache.x});
        expected.erase(found);
    }
    for (int i = 0; i < 8; i++)
        if (expectedLods[i] != c.perLod[i])
            throw std::runtime_error("LOD counters mismatch");
    LodChangeHistory nextChanges;
    for (const auto &[root, lod] : current) {
        auto old = history.find(root);
        nextChanges[root] = old != history.end() && old->second == lod ? lodChanges.at(root)
                                                                       : uint32_t(p.cache.x * 1000);
    }
    lodChanges = std::move(nextChanges);
    history = std::move(current);
    if (p.cache.y > 0 && c.instanceCount <= VisualVoxelizer::CacheCapacity)
        cacheHistory = std::move(nextCache);
    else
        cacheHistory.clear();
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
struct BufferCleanup {
    VulkanContext &vk;
    Buffer &buffer;
    ~BufferCleanup() {
        if (buffer.handle) {
            vkDeviceWaitIdle(vk.device);
            vk.destroy(buffer);
        }
    }
};
struct QueryCleanup {
    VulkanContext &vk;
    VkQueryPool &query;
    ~QueryCleanup() {
        if (query) {
            vkDeviceWaitIdle(vk.device);
            vkDestroyQueryPool(vk.device, query, nullptr);
        }
    }
};
int main(int argc, char **argv) {
    try {
        Options opt = parse(argc, argv);
        VulkanContext vk(opt.width, opt.height, opt.hidden, opt.validation, opt.vsync);
        Buffer frameBuffer = vk.buffer(sizeof(Frame), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true);
        BufferCleanup frameCleanup{vk, frameBuffer};
        {
            SourceWorld world(opt.scene);
            SourceRenderer source(vk, world, frameBuffer);
            VisualVoxelizer voxelizer(vk, frameBuffer);
            VoxelRenderer renderer(vk, voxelizer);
            Hud hud(vk, renderer.pass, voxelizer.setLayout);
            Readback readback(vk, opt.verify || opt.exercise);
            Settings &s = opt.settings;
            glm::vec3 camera = opt.scene == SourceScene::Garden ? glm::vec3(7.8f, 5.4f, 11.f)
                                                               : glm::vec3(5.8f, 3.6f, 8.4f);
            Input input{&s, &hud, &camera};
            input.homeCamera = camera;
            input.homePitch = opt.scene == SourceScene::Garden ? -.30f : -.27f;
            input.pitch = input.homePitch;
            glfwSetWindowUserPointer(vk.window, &input);
            glfwSetKeyCallback(vk.window, keyCallback);
            glfwSetMouseButtonCallback(vk.window, mouseButton);
            glfwSetCursorPosCallback(vk.window, mouseMove);
            glfwSetWindowFocusCallback(vk.window, focusCallback);
            VkQueryPool queries{};
            QueryCleanup queryCleanup{vk, queries};
            if (vk.timestampBits) {
                VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
                ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
                ci.queryCount = 11;
                check(vkCreateQueryPool(vk.device, &ci, nullptr, &queries), "timestamp query pool");
            }
            bool resize = true;
            Settings previous = s;
            Counters counts{};
            std::array<double, 7> times{}, totals{};
            int timedFrames = 0;
            double cubeDrawTime = 0, cubeDrawTotal = 0;
            RootHistory history;
            LodChangeHistory lodChanges;
            CacheHistory cacheHistory;
            std::vector<Voxel> frozenSnapshot;
            Counters frozenCounts{};
            int frozenChecks = 0, verifiedFrames = 0, controlChecks = 0, stabilityChecks = 0;
            int renderFreezeChecks = 0;
            int cacheStabilityChecks = 0, cacheFreezeChecks = 0;
            uint64_t cacheMotionAdded = 0, cacheMotionRemoved = 0;
            uint64_t cacheRetainedTotal = 0, cacheExpiredTotal = 0, cacheRejectedTotal = 0;
            uint64_t cacheLodRejectedTotal = 0;
            uint64_t cacheCopyBytes = 0, cacheCapacityResets = 0;
            Voxel renderProbe{};
            uint64_t motionAdded = 0, motionRemoved = 0;
            float reconstructionError = 0;
            std::map<Cell, glm::vec4> stableCells;
            std::map<Cell, glm::vec4> stationaryCloud;
            std::map<Cell, glm::vec4> lastCacheCloud, stationaryCacheCloud;
            auto last = std::chrono::steady_clock::now();
            float animation = 0, fps = 60;
            float cacheClock = 0;
            glm::vec3 lastSampleCamera = camera, lastSampleForward{};
            int frameNumber = 0;
            Parameters parameters{};
            while (!glfwWindowShouldClose(vk.window) &&
                   (opt.frames == 0 || frameNumber < opt.frames)) {
                glfwPollEvents();
                if (opt.exerciseControls && frameNumber < 16)
                    exerciseControls(frameNumber, vk.window, input);
                if (opt.exerciseCache) {
                    if (frameNumber == 0 || frameNumber == 41)
                        resetCamera(vk.window, input);
                    if (frameNumber >= 3 && frameNumber <= 37) {
                        camera.x += .003f;
                        input.yaw += .0004f;
                    }
                    if (frameNumber == 38)
                        s.frozen = true;
                    if (frameNumber == 39) {
                        camera += glm::vec3(-2, 0, -1);
                        input.yaw -= .45f;
                    }
                    if (frameNumber == 40) {
                        s.frozen = false;
                        camera = {50, 50, 50};
                        input.yaw = .7f;
                        input.pitch = .5f;
                    }
                    if (frameNumber == 42)
                        s.levels = s.levels == 1 ? 2 : 1;
                }
                if (opt.freezeAfter >= 0 && frameNumber >= opt.freezeAfter)
                    s.frozen = true;
                if (opt.exerciseRender && frameNumber > 0) {
                    s.frozen = true;
                    const glm::vec3 axes[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                               {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                    glm::vec3 axis = glm::normalize(axes[(frameNumber - 1) % 6] +
                                                    glm::vec3(.013f, .017f, .019f));
                    float size = renderProbe.centerSize.w;
                    float distance = frameNumber <= 6 ? size * .5f + .1f
                                     : frameNumber <= 12 ? size * .5f + .025f : size * .1f;
                    camera = glm::vec3(renderProbe.centerSize) + axis * distance;
                    glm::vec3 direction = -axis;
                    input.yaw = std::atan2(direction.z, direction.x);
                    input.pitch = std::asin(direction.y);
                }
                if (opt.exerciseStability && frameNumber < 12) {
                    s.frozen = false;
                    s.levels = 1;
                    s.lighting = 0;
                    s.adaptiveLod = false;
                    if (frameNumber == 0)
                        resetCamera(vk.window, input);
                    if (frameNumber >= 3) {
                        camera.x += .002f;
                        input.yaw += .00025f;
                    }
                }
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
                cacheClock += opt.frames ? 1.f / 60 : std::max(dt, 0.f);
                if (opt.exerciseCache && frameNumber == 37)
                    cacheClock += s.cacheMs * .001f + .01f; // exercise hard expiry after a hitch
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
                if (s.base != previous.base || s.levels != previous.levels ||
                    s.distance != previous.distance || s.splat != previous.splat ||
                    s.adaptiveLod != previous.adaptiveLod || s.lighting != previous.lighting ||
                    s.average != previous.average || s.mode != previous.mode ||
                    s.footprintSplats != previous.footprintSplats ||
                    s.footprintRadius != previous.footprintRadius ||
                    s.footprintLimit != previous.footprintLimit ||
                    s.voxelCache != previous.voxelCache || s.cacheMs != previous.cacheMs ||
                    (previous.frozen && !s.frozen))
                    voxelizer.resetCache = true;
                if (s.base != previous.base || s.levels != previous.levels ||
                    s.adaptiveLod != previous.adaptiveLod) {
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
                    readback.resize(w, h, vk.extent, opt.verify,
                                    !opt.screenshot.empty() || !opt.captureSequence.empty());
                    history.clear();
                    resize = false;
                }
                if (!std::isfinite(camera.x) || !std::isfinite(camera.y) ||
                    !std::isfinite(camera.z) || !std::isfinite(input.yaw) ||
                    !std::isfinite(input.pitch))
                    resetCamera(vk.window, input);
                glm::vec3 forward(std::cos(input.pitch) * std::cos(input.yaw),
                                  std::sin(input.pitch),
                                  std::cos(input.pitch) * std::sin(input.yaw));
                glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                float speed = glfwGetKey(vk.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 8.f : 3.f;
                auto pressed = [&](int k) {
                    return glfwGetWindowAttrib(vk.window, GLFW_FOCUSED) &&
                           glfwGetKey(vk.window, k) == GLFW_PRESS;
                };
                camera += speed * moveDt *
                          (forward * float(pressed(GLFW_KEY_W) - pressed(GLFW_KEY_S)) +
                           right * float(pressed(GLFW_KEY_D) - pressed(GLFW_KEY_A)) +
                           glm::vec3(0, 1, 0) * float(pressed(GLFW_KEY_E) - pressed(GLFW_KEY_Q)));
                if (input.resetCacheRequested || glm::distance(camera, lastSampleCamera) > .5f ||
                    (voxelizer.cloudReady && glm::dot(forward, lastSampleForward) < .96f))
                    voxelizer.resetCache = true;
                input.resetCacheRequested = false;
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
                frame.options =
                    glm::vec4(float(s.lighting), s.adaptiveLod ? 1.f : 0.f,
                              s.footprintSplats ? 1.f : 0.f, float(world.triangleCount()));
                std::memcpy(frameBuffer.mapped, &frame, sizeof(frame));
                parameters.extent = {int(source.width), int(source.height),
                                     int(VisualVoxelizer::Capacity),
                                     int(VisualVoxelizer::Capacity)};
                parameters.config = {s.base, s.distance, .1f, s.splat};
                parameters.flags = {s.levels, s.average ? 1 : 0, s.cubeLight ? 1 : 0, s.mode};
                parameters.footprint = {s.footprintRadius, s.footprintLimit,
                                        s.cullCubes ? 1 : 0, s.indexedCubes ? 1 : 0};
                bool caching = s.voxelCache && s.cacheMs > 0;
                bool canReuse = caching && voxelizer.cacheReady && !voxelizer.resetCache &&
                                counts.instanceCount <= VisualVoxelizer::CacheCapacity;
                parameters.cache = {cacheClock, caching ? s.cacheMs * .001f : 0.f,
                                    canReuse ? float(counts.instanceCount) : 0.f,
                                    canReuse ? 1.f : 0.f};
                bool generating = !s.frozen || !voxelizer.cloudReady;
                if (generating && caching && counts.instanceCount > VisualVoxelizer::CacheCapacity)
                    cacheCapacityResets++;
                bool referenceReset = voxelizer.resetHistory;
                if (referenceReset)
                    history.clear();
                hud.build(s, counts, times, world.triangleCount(), source.width, source.height, fps,
                          cubeDrawTime);
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
                    vkCmdResetQueryPool(vk.cmd, queries, 0, 11);
                    vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
                }
                source.render(voxelizer.current, parameters);
                if (queries)
                    vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
                if (generating)
                    voxelizer.generate(parameters, queries);
                else if (queries) {
                    for (uint32_t i : {2u, 3u, 9u, 10u, 4u})
                        vkCmdWriteTimestamp(vk.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries,
                                            i);
                }
                renderer.prepare(parameters);
                renderer.begin(parameters);
                renderer.draw(parameters, queries);
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
                    copyImage(vk, source.depth, readback.depths,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                }
                if (!opt.screenshot.empty() || !opt.captureSequence.empty())
                    copyImage(vk, renderer.output, readback.pixels,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkImageMemoryBarrier sb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                sb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                sb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                sb.srcQueueFamilyIndex = sb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                sb.image = vk.swapImages[swapIndex];
                sb.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                sb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                // Match the acquire semaphore's TRANSFER wait stage so the layout
                // transition cannot race the presentation engine's previous read.
                vkCmdPipelineBarrier(vk.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
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
                if (generating) {
                    cacheRetainedTotal += counts.cacheRetained;
                    cacheExpiredTotal += counts.cacheExpired;
                    cacheRejectedTotal += counts.cacheRejected;
                    cacheLodRejectedTotal += counts.cacheLodRejected;
                    cacheCopyBytes +=
                        uint64_t(parameters.cache.z) * (sizeof(Voxel) + sizeof(float));
                }
                if (opt.exerciseControls && frameNumber < 16) {
                    bool empty = frameNumber == 2 || frameNumber == 3;
                    if ((counts.hits == 0) != empty || (!empty && counts.instanceCount == 0))
                        throw std::runtime_error(
                            "Source visibility did not survive camera/settings changes");
                    controlChecks++;
                }
                if (queries) {
                    std::array<uint64_t, 11> stamps{};
                    check(vkGetQueryPoolResults(vk.device, queries, 0, 11, sizeof(stamps),
                                                stamps.data(), 8,
                                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
                          "read timestamps");
                    uint64_t mask = vk.timestampBits == 64
                                        ? UINT64_MAX
                                        : ((uint64_t(1) << vk.timestampBits) - 1);
                    for (int i = 0; i < 6; i++)
                        times[i] =
                            double((stamps[i + 1] - stamps[i]) & mask) * vk.timestampPeriod / 1e6;
                    times[6] = double((stamps[10] - stamps[9]) & mask) * vk.timestampPeriod / 1e6;
                    times[3] = std::max(0.0, times[3] - times[6]);
                    cubeDrawTime =
                        double((stamps[8] - stamps[7]) & mask) * vk.timestampPeriod / 1e6;
                    bool timed = opt.freezeAfter >= 0
                                     ? frameNumber >= opt.freezeAfter && s.frozen : generating;
                    if (frameNumber >= 2 && timed) {
                        for (int i = 0; i < 7; i++)
                            totals[i] += times[i];
                        cubeDrawTotal += cubeDrawTime;
                        timedFrames++;
                    }
                }
                if (!opt.captureSequence.empty()) {
                    std::ostringstream name;
                    name << opt.captureSequence << "/frame-" << std::setfill('0') << std::setw(3)
                         << frameNumber << ".ppm";
                    screenshot(name.str(), vk, readback);
                }
                if (opt.verify && generating) {
                    verify(parameters, frame, readback, counts, history, lodChanges, cacheHistory,
                           reconstructionError);
                    verifiedFrames++;
                }
                if (opt.exerciseRender) {
                    auto *data = static_cast<const Voxel *>(readback.instances.mapped);
                    if (frameNumber == 0) {
                        if (!counts.instanceCount)
                            throw std::runtime_error("Render exercise needs a nonempty cloud");
                        frozenSnapshot.assign(data, data + counts.instanceCount);
                        frozenCounts = counts;
                        for (uint32_t i = 0; i < counts.instanceCount; ++i)
                            if (data[i].centerSize.w > renderProbe.centerSize.w ||
                                (data[i].centerSize.w == renderProbe.centerSize.w &&
                                 data[i].centerSize.y > renderProbe.centerSize.y))
                                renderProbe = data[i];
                        if (renderProbe.centerSize.w < .1f)
                            throw std::runtime_error("Render exercise needs a cube >= .1 m; "
                                                     "use --voxel-size .12 --levels 1");
                    } else {
                        if (std::memcmp(&counts, &frozenCounts, sizeof(counts)) ||
                            std::memcmp(data, frozenSnapshot.data(), frozenSnapshot.size() * sizeof(Voxel)))
                            throw std::runtime_error("Render exercise changed the frozen cloud");
                        ++renderFreezeChecks;
                    }
                }
                if (opt.exerciseCache) {
                    auto *data = static_cast<const Voxel *>(readback.instances.mapped);
                    std::map<Cell, glm::vec4> cloud;
                    for (uint32_t i = 0; i < counts.instanceCount; ++i) {
                        const auto &v = data[i];
                        int lod = int(std::round(std::log2(v.centerSize.w / (s.base * s.splat))));
                        auto key =
                            cell({v.centerSize.x, v.centerSize.y, v.centerSize.z}, s.base, lod);
                        if (!cloud.emplace(key, v.rgba).second)
                            throw std::runtime_error("Cache exercise found duplicate cubes");
                    }
                    if (frameNumber == 0)
                        stationaryCacheCloud = cloud;
                    if (frameNumber == 1 || frameNumber == 2) {
                        if (cloud != stationaryCacheCloud)
                            throw std::runtime_error(
                                "Cache changed stationary world cells or source RGB");
                        cacheStabilityChecks++;
                    }
                    if (frameNumber >= 3 && frameNumber <= 36) {
                        for (const auto &[key, rgb] : cloud)
                            if (!lastCacheCloud.contains(key))
                                cacheMotionAdded++;
                        for (const auto &[key, rgb] : lastCacheCloud)
                            if (!cloud.contains(key))
                                cacheMotionRemoved++;
                    }
                    if (frameNumber == 38) {
                        frozenSnapshot.assign(data, data + counts.instanceCount);
                        frozenCounts = counts;
                    }
                    if (frameNumber == 39) {
                        if (std::memcmp(&counts, &frozenCounts, sizeof(counts)) ||
                            std::memcmp(data, frozenSnapshot.data(),
                                        frozenSnapshot.size() * sizeof(Voxel)))
                            throw std::runtime_error(
                                "Cache changed a frozen cloud after camera movement");
                        cacheFreezeChecks++;
                    }
                    if (frameNumber == 40 && (counts.hits || counts.instanceCount))
                        throw std::runtime_error(
                            "Cache left cubes after moving into an empty source view");
                    if (frameNumber == 41 || frameNumber == 42) {
                        if (!counts.hits || !counts.instanceCount || counts.cacheRetained)
                            throw std::runtime_error(
                                "Cache reset retained old cells after camera/grid reset");
                    }
                    lastCacheCloud = std::move(cloud);
                }
                if (opt.exerciseStability && frameNumber < 12) {
                    if (!generating)
                        throw std::runtime_error("Stability test froze generation");
                    std::map<Cell, glm::vec4> cloud, patch;
                    auto *data = static_cast<const Voxel *>(readback.instances.mapped);
                    for (uint32_t i = 0; i < counts.instanceCount; i++) {
                        const auto &v = data[i];
                        auto key =
                            cell({v.centerSize.x, v.centerSize.y, v.centerSize.z}, s.base, 0);
                        cloud.emplace(key, v.rgba);
                        if (v.centerSize.x >= -4.3f && v.centerSize.x <= -3.7f &&
                            std::abs(v.centerSize.z) <= .3f && std::abs(v.centerSize.y) < s.base)
                            patch.emplace(key, v.rgba);
                    }
                    if (frameNumber == 0)
                        stationaryCloud = cloud;
                    if (frameNumber == 1 || frameNumber == 2) {
                        if (cloud != stationaryCloud)
                            throw std::runtime_error(
                                "Stationary raster cloud changed cell keys or RGB");
                        stabilityChecks++;
                    }
                    if (patch.size() < 25)
                        throw std::runtime_error("Motion test floor patch is not visible");
                    if (frameNumber >= 3) {
                        for (const auto &[key, color] : patch)
                            if (!stableCells.contains(key))
                                motionAdded++;
                        for (const auto &[key, color] : stableCells)
                            if (!patch.contains(key))
                                motionRemoved++;
                    }
                    stableCells = std::move(patch);
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
                if (generating) {
                    lastSampleCamera = camera;
                    lastSampleForward = forward;
                    voxelizer.advance();
                }
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
            if (opt.exerciseControls)
                std::cout << "Camera reset, mouse capture/focus, settings and resize checks: "
                          << controlChecks << '\n';
            if (opt.exerciseRender)
                std::cout << "Frozen cube render checks: " << renderFreezeChecks
                          << " (six outside, six near-plane, six inside views)\n";
            if (opt.exerciseStability)
                std::cout << "Static-camera checks: " << stabilityChecks
                          << "; moving floor patch: " << motionAdded << " cell additions, "
                          << motionRemoved << " removals\n";
            if (timedFrames)
                for (int i = 0; i < 7; i++)
                    totals[i] /= timedFrames;
            if (timedFrames)
                cubeDrawTotal /= timedFrames;
            const char *labels[] = {"source_and_shadow_ms", "lod_and_clear_ms", "hash_ms",
                                    "compact_ms",           "final_render_ms",  "hud_ms",
                                    "cache_resolve_ms"};
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
                  << ",\n  \"scene\": \"" << (opt.scene == SourceScene::Garden ? "garden" : "test")
                  << "\""
                  << ",\n  \"vsync_requested\": " << (vk.vsyncRequested ? "true" : "false")
                  << ",\n  \"present_mode\": \"" << vk.presentModeName() << "\""
                  << ",\n  \"device_type\": \""
                  << (vk.gpuType == VK_PHYSICAL_DEVICE_TYPE_CPU              ? "cpu"
                      : vk.gpuType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   ? "discrete"
                      : vk.gpuType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated"
                                                                             : "other")
                  << "\""
                  << ",\n  \"cube_mesh\": \"" << (s.indexedCubes ? "indexed" : "legacy") << "\""
                  << ",\n  \"cube_backface_culling\": " << (s.cullCubes ? "true" : "false")
                  << ",\n  \"cube_distinct_vertex_ids\": "
                  << (s.indexedCubes ? (s.cullCubes || s.cubeLight ? 24 : 8) : 36)
                  << ",\n  \"instance_bytes_per_voxel\": " << sizeof(Voxel)
                  << ",\n  \"used_instance_bytes\": "
                  << uint64_t(counts.instanceCount) * sizeof(Voxel)
                  << ",\n  \"allocated_instance_bytes\": " << voxelizer.instances.size
                  << ",\n  \"voxel_cache\": " << (s.voxelCache && s.cacheMs > 0 ? "true" : "false")
                  << ",\n  \"cache_hold_ms\": " << s.cacheMs
                  << ",\n  \"cache_capacity_voxels\": " << VisualVoxelizer::CacheCapacity
                  << ",\n  \"cache_allocated_bytes\": "
                  << uint64_t(VisualVoxelizer::CacheCapacity) * (sizeof(Voxel) + 2 * sizeof(float))
                  << ",\n  \"cache_retained_voxels\": " << counts.cacheRetained
                  << ",\n  \"cache_expired_voxels\": " << counts.cacheExpired
                  << ",\n  \"cache_rejected_voxels\": " << counts.cacheRejected
                  << ",\n  \"cache_dropped_voxels\": " << counts.cacheDropped
                  << ",\n  \"cache_retained_total\": " << cacheRetainedTotal
                  << ",\n  \"cache_expired_total\": " << cacheExpiredTotal
                  << ",\n  \"cache_rejected_total\": " << cacheRejectedTotal
                  << ",\n  \"cache_lod_rejected_total\": " << cacheLodRejectedTotal
                  << ",\n  \"cache_history_copy_bytes_total\": " << cacheCopyBytes
                  << ",\n  \"cache_capacity_resets\": " << cacheCapacityResets
                  << ",\n  \"cache_motion_cell_additions\": " << cacheMotionAdded
                  << ",\n  \"cache_motion_cell_removals\": " << cacheMotionRemoved
                  << ",\n  \"cache_stability_checks\": " << cacheStabilityChecks
                  << ",\n  \"cache_freeze_checks\": " << cacheFreezeChecks
                  << ",\n  \"freeze_after\": " << opt.freezeAfter << ",\n  \"timing_scope\": \""
                  << (opt.freezeAfter >= 0 ? "frozen" : "generating") << "\""
                  << ",\n  \"render_freeze_checks\": " << renderFreezeChecks
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
                  << ",\n  \"control_checks\": " << controlChecks << ",\n  \"camera_position\": ["
                  << camera.x << ", " << camera.y << ", " << camera.z << "]"
                  << ",\n  \"camera_yaw\": " << input.yaw
                  << ",\n  \"camera_pitch\": " << input.pitch << ",\n  \"occupancy_backend\": \""
                  << (s.footprintSplats ? "raster-footprints" : "raster-points") << "\""
                  << ",\n  \"candidate_voxel_writes\": " << counts.candidateWrites
                  << ",\n  \"writes_per_valid_sample\": "
                  << (counts.hits ? double(counts.candidateWrites) / counts.hits : 0)
                  << ",\n  \"writes_per_source_sample\": "
                  << double(counts.candidateWrites) / (source.width * source.height)
                  << ",\n  \"max_footprint_cells\": " << counts.maxFootprintCells
                  << ",\n  \"max_footprint_extent_cells\": "
                  << double(counts.maxFootprintExtent) / 1024
                  << ",\n  \"clamped_footprints\": " << counts.clampedFootprints
                  << ",\n  \"rejected_neighbors\": " << counts.rejectedNeighbors
                  << ",\n  \"footprint_radius_cells\": " << s.footprintRadius
                  << ",\n  \"footprint_write_limit\": " << s.footprintLimit
                  << ",\n  \"source_width\": " << source.width
                  << ",\n  \"source_height\": " << source.height
                  << ",\n  \"depth_reconstruction_max_error_m\": "
                  << (opt.verify ? std::to_string(reconstructionError) : "null")
                  << ",\n  \"motion_patch_cell_additions\": " << motionAdded
                  << ",\n  \"motion_patch_cell_removals\": " << motionRemoved
                  << ",\n  \"stationary_cloud_cells_checked\": " << stationaryCloud.size()
                  << ",\n  \"gpu_generation_ms\": " << totals[1] + totals[2] + totals[3] + totals[6]
                  << ",\n  \"final_voxel_draw_ms\": " << cubeDrawTotal << ",\n  \"gpu_frame_ms\": "
                  << totals[0] + totals[1] + totals[2] + totals[3] + totals[4] + totals[5] +
                         totals[6]
                  << ",\n  \"stability_checks\": " << stabilityChecks
                  << ",\n  \"stable_cells_checked\": " << stableCells.size()
                  << ",\n  \"adaptive_lod\": " << (s.adaptiveLod ? "true" : "false")
                  << ",\n  \"validation_errors\": " << vk.validationErrors
                  << ",\n  \"timestamp_supported\": " << (queries ? "true" : "false")
                  << ",\n  \"voxels_per_lod\": [";
                for (int i = 0; i < s.levels; i++)
                    f << (i ? ", " : "") << counts.perLod[i];
                f << "],\n  \"average_gpu_timings\": {";
                for (int i = 0; i < 7; i++)
                    f << (i ? ", " : "") << "\"" << labels[i] << "\": " << totals[i];
                f << "}\n}\n";
            }
            if (queries) {
                vkDestroyQueryPool(vk.device, queries, nullptr);
                queries = {};
            }
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
