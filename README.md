# Microvoxels

A C++20/Vulkan experiment that turns **rasterized visible world positions and final source RGB** into a sparse shell of **unlit coloured cubes**. The voxel generator reads source buffers only. It has no source triangle topology, material evaluation, or post-process raycasting.

The demo source happens to be 2,860 triangles. A future SDF, procedural renderer or hardware ray tracer can supply the same visible XYZ + final RGB contract. The normal cube fragment shader is literally `outColor = color;`.

## Build and run

Requires CMake 3.24+, a C++20 compiler, Vulkan headers/loader, `glslangValidator`, GLFW 3.3+ and GLM. The demo uses Vulkan 1.1 graphics and compute. No ray-tracing extensions are required.

Ubuntu / Debian:

```sh
sudo apt-get install cmake ninja-build g++ libvulkan-dev libglfw3-dev libglm-dev glslang-tools
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/microvoxels
```

Windows, Visual Studio + vcpkg + Vulkan SDK:

```bat
C:\Users\Extreme\vcpkg\vcpkg.exe install glfw3:x64-windows glm:x64-windows
cmake -S . -B build-windows -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/Users/Extreme/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-windows --config Release
.\build-windows\Release\microvoxels.exe
```

Run commands from the project directory containing `CMakeLists.txt`. Shader compilation is automatic. An existing configured build only needs `cmake --build build-windows --config Release` after updating the source. CI builds with Windows MSVC and verifies rendering with software Vulkan on Linux.

## Compare point and footprint splats

The default **footprint mode** estimates a small parallelogram from the centre pixel and its four immediate axis neighbors, all within a 3×3 neighborhood. It rejects discontinuous neighbors, clamps the patch, and emits cells overlapped by it. Every contribution uses the centre sample's already-shaded RGB. The original point cell is always retained.

Press **T** to compare this with **point mode**, where one valid source sample emits one cell. Both use the same source visibility, world grid, RGB reduction and unlit cube renderer. Default bounds are a **one-cell radius** and **27 candidate writes per valid source sample**. The bounds can be configured independently:

```bat
.\build-windows\Release\microvoxels.exe --point-splats
.\build-windows\Release\microvoxels.exe --footprint-radius 2 --footprint-limit 27
.\build-windows\Release\microvoxels.exe --levels 1 --no-adaptive-lod
```

`--footprint-radius 0` produces point-sized footprints. `--sample-occupancy` remains an alias for point mode. Normal cubes use exact cell-sized geometry, with shared corners derived from integer grid boundaries. Stationary RGB screenshots and frozen clouds are checked in CI.

| Control | Action |
|---|---|
| WASD / Q / E | Fly |
| Hold right mouse | Look around |
| R | Reset the camera and unfreeze |
| T | Point / conservative footprint splats |
| C | Toggle projected-source-footprint LOD |
| Shift | Faster camera |
| Tab | Source / microvoxels / side by side |
| F | Freeze the cloud; camera and source continue |
| G | Separate cube-face-lighting debug pipeline |
| + / - | Base voxel size |
| [ / ] | First distance LOD boundary |
| 1–6 | Number of nested LOD levels |
| O | Average RGB / closest source point |
| X | Toggle 2×2 source sampling |
| H | Albedo / source lighting / source shadows |
| P | Pause source animation |
| F1 | Hide/show HUD |
| Escape | Exit |

Defaults: 10 mm base pitch, 4 m first distance boundary, five levels, ±10% LOD hysteresis, footprint splats and projected-size LOD enabled. Grid origin stays at world `(0,0,0)`.

The HUD shows source resolution/samples, valid hits, candidate writes, unique cubes, writes per hit, maximum footprint extent/count, clamped/rejected footprints, frame/generation/draw times, global hash attempts, table load, contributors per cell and instance memory. Use `--profile-stages` to isolate pass timings; ordinary timestamps allow pipeline overlap.

Freeze and move around to inspect the sampled shell. Newly exposed surfaces are absent until regeneration. Stored RGB retains its original view-dependent source shading. If both panes are empty with `HITS 0`, press **R**. Mouse capture/focus and finite camera-pose guards remain in place.

## Pipeline

1. **SourceRenderer** renders source depth, final RGB and cached XYZ. A benchmark option produces depth + RGB alone.
2. **VisualVoxelizer** clears previous occupied slots, resets counters/root metadata, and runs the existing world-region LOD request/resolve passes.
3. One generation pass loads/reconstructs XYZ, estimates the same bounded footprints, deduplicates exact cells and appends each newly occupied hash slot to `uniqueSlots[]`.
4. **resolve_unique.comp** visits that compact list, finalizes RGB and writes 16-byte cell-key + RGBA16F instances. GPU counters drive both its dispatch and the cube draw.
5. **VoxelRenderer** decodes signed XYZ + LOD, reconstructs centre/size and draws stored RGB directly.

The default keeps cached XYZ because the measured depth variant was slower and changed quantization around exact grid planes. `--depth-source` removes the XYZ targets and reconstructs Vulkan `[0,1]` depth using inverse VP; `--no-depth-tiles` disables its shared 8×8 tile/halo. Verification checks actual source depth independently. All lighting, normals, shadows, materials and tone mapping remain in the source stage. There are no ray–box tests, second source visibility pass, triangle–cell voxelization or persistent voxel volume.

The core filter uses **four compute dispatches**: two existing LOD passes, generation, and unique resolve. Sparse touched-slot clearing adds one dispatch. The first frame and clouds occupying at least a quarter of the table use a bulk clear; a previous-frame count already available to the HUD selects this fallback. `--touched-clear` / `--full-clear` force either path.

Independent comparison switches are `--legacy-compaction`, `--unpacked`, `--full-clear`, `--touched-clear`, `--depth-source`, `--no-depth-tiles`, and `--local-dedup`. Local dedup uses a bounded shared mini hash and falls back to global insertion when full. It stays off by default because it reduced atomic traffic without a repeatable speedup on the available device.

## Verification and measurements

```sh
ctest --test-dir build --output-on-failure
./build/microvoxels --validation --verify --exercise --frames 8 --width 480 --height 360
./build/microvoxels --validation --verify --exercise-controls --frames 16 --width 480 --height 360
./build/microvoxels --validation --exercise-stability --frames 12 --width 640 --height 480 --capture-sequence captures/motion
./build/microvoxels --frames 120 --time 1 --no-ui --report profile.json
./build/microvoxels --frames 120 --time 1 --no-ui --profile-stages --report stages.json
python3 tools/profile_pipeline.py --binary build/microvoxels --output captures/pipeline-profile
```

`--verify` checks every GPU cell, LOD, RGB reduction, indirect command, footprint limit/counter, and XYZ against source depth. Footprint occupancy uses independent double-precision polygon clipping on the CPU. Depth mode adds a verification-only GPU reconstruction readback so CPU arithmetic near a grid boundary cannot change the reference cell; CPU inverse projection still checks reconstruction independently. Verification and screenshots add readbacks; leave them off for performance measurements.

`--exercise` checks byte-identical frozen buffers across camera motion, then average/closest RGB and cube-light debug. `--exercise-controls` checks camera loss/reset, mouse capture/focus, empty freeze recovery, settings, mode switches and resize. `--exercise-stability` uses three stationary frames followed by nine small camera movements, with fixed source time, albedo lighting and one LOD. It verifies identical stationary cell/RGB sets and records turnover in a static floor patch. `tests/footprint_comparison.py` checks exact stationary RGB screenshots and measures matched motion silhouettes/coverage.

Source sampling is capped at 2048 pixels per dimension. Tables are bounded; the HUD reports drops. Very fine cells, grazing angles, silhouettes, discontinuity fallback and footprint caps can still leave gaps or changing cells. Projected-size LOD is useful when source pixels cover many smaller cells.

Read [the implementation notes](docs/IMPLEMENTATION.md), [the streamlining measurements](docs/STREAMLINING.md) and [the earlier footprint comparison](docs/FOOTPRINTS.md). Opaque geometry only; transparency and temporal occupancy accumulation are not implemented.
