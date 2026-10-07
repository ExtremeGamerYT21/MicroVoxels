# Microvoxels

A small C++20/Vulkan rendering experiment: ordinary triangle surfaces are shaded continuously,
then their **visible world-space samples** become a sparse shell of **unlit coloured cubes**.

The world is triangles. There is no voxel terrain, stored object volume, or persistent voxel world.
The default cube fragment shader is literally `outColor = color;`. Cube lighting has a separate,
explicitly selected debug pipeline.

![Source triangles, left; unlit microvoxels, right](docs/comparison.png)

## Build and run

Requires CMake 3.24+, a C++20 compiler, Vulkan headers/loader, `glslangValidator`, GLFW 3.3+, and GLM.
No ray-tracing hardware or vendor-specific extensions are required. Vulkan 1.1 graphics + compute,
an sRGB desktop swapchain, and the image formats checked at startup are used.

Ubuntu / Debian:

```sh
sudo apt-get install cmake ninja-build g++ libvulkan-dev libglfw3-dev libglm-dev glslang-tools
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/microvoxels
```

Install a current AMD Vulkan driver on your own machine. On Linux this is commonly Mesa RADV;
on Windows use the AMD driver and the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home).

Windows, Visual Studio 2022 + vcpkg + Vulkan SDK:

```powershell
vcpkg install glfw3:x64-windows glm:x64-windows
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
.\build\Release\microvoxels.exe
```

The Windows build recipe is provided for the user's AMD desktop; this run was built and tested on Linux.
Shaders are compiled automatically and loaded from the build's shader directory. Build locally before running.

## Try the experiment

Start in side-by-side mode. The orange sphere uses interpolated smooth normals and a glossy source highlight.
The blue crystal uses flat source normals. The tree and its curved leaves bend continuously in the source
vertex shader. Watch the leaves while changing voxel size: their occupied world cells pop in and out.

Click the HUD's buttons and sliders, or use these keys:

| Control | Action |
|---|---|
| WASD / Q / E | Fly forward/backward/sideways/down/up |
| Hold right mouse | Look around |
| Shift | Faster camera |
| Tab | Source → microvoxels → side by side |
| F | Freeze/unfreeze the generated cloud; camera and source animation continue |
| G | Compare with separately lit cube faces |
| + / - | Increase/decrease the base voxel size |
| [ / ] | Decrease/increase the first LOD distance |
| 1–6 | Number of nested LOD levels |
| O | Average RGB / closest source sample |
| X | Toggle 2×2 source sampling; each image dimension is capped at 2048 |
| H | Albedo only / smooth lighting / smooth lighting with source shadow map |
| P | Pause source animation |
| F1 | Hide/show the HUD |
| Escape | Exit |

Freeze, then walk around a sphere or plant. Unseen sides are missing by design. The frozen colours
also retain their original view-dependent source shading, including the captured glossy highlight.
Use the left pane to see the current continuous scene while the right pane displays that frozen shell.

Defaults: base size 10 mm; first LOD boundary 4 m; five levels; ±10% hysteresis; 1.03× cube size.
The grid origin stays at world `(0,0,0)` through camera movement. The slight cube enlargement helps
coverage; set `Settings::splat` to `1.0` for exact cell-sized cubes.

## Pipeline

1. **SourceWorld** makes 2,860 ordinary triangles: ground, sphere, crystal, rocks, trunk, branches, leaves.
2. **SourceRenderer** draws a source shadow map and a G-buffer of valid world positions and already-shaded
   linear RGB. Lighting, normals, procedural checker colour, gloss, and shadows all belong to this stage.
3. **VisualVoxelizer** chooses a nested world-grid LOD, hashes exact cell keys, reduces samples, and compacts
   the occupied cells into GPU instances and a GPU-written indirect draw command.
4. **VoxelRenderer** depth-tests those instances and writes their stored colours. All faces of one normal
   voxel have identical RGB. Source comparison, debug cube lighting, and HUD are separate draw pipelines.

`SurfaceSamples` is the provider contract. A future triangle ray-query renderer, SDF, or parametric source
can supply equivalent positions/RGB without changing the voxel stages. This milestone uses the explicitly
permitted raster source path; it does **not** implement KHR ray queries or acceleration structures yet.

Read [the implementation notes](docs/IMPLEMENTATION.md) for exact key handling, LOD history, synchronization,
capacity limits, and known artefacts. Read [the measured profile](docs/PROFILING.md) for results and next steps.

## Verification and profiling

```sh
ctest --test-dir build --output-on-failure
./build/microvoxels --validation --verify --exercise --frames 8 --width 480 --height 360
./build/microvoxels --frames 120 --width 1100 --height 720 --no-ui --report profile.json
./build/microvoxels --frames 1 --time 1.2 --screenshot comparison.ppm
```

`--validation` requires `VK_LAYER_KHRONOS_validation`. `--verify` performs expensive readbacks and CPU
reference checks; leave it off when measuring interactive performance. `--exercise` freezes the cloud,
moves the camera, checks byte-identical instances/counters, unfreezes, and exercises closest RGB and debug
cube lighting. `--hidden` hides a GLFW window; it still needs a display. In CI, use `xvfb-run -a`.

JSON reports contain source triangle count, sampling resolution, visible hits, unique voxels, per-LOD counts,
dropped samples, validation errors, and per-stage Vulkan timestamp durations after two warm-up frames.
Timings are unavailable if the queue has no timestamp support. HUD statistics describe the previous completed
frame; while frozen, hit/voxel counters describe the last cloud generation.

This version is opaque only. Reflections, transparent source layers, hardware ray tracing, temporal colour
accumulation, and more sophisticated hole filling are intentionally left for later experiments.
