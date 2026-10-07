# Cube draw hardware experiments

The generator, source buffers, world grid, LOD, ordered compaction, RGB reduction,
32-byte instances and unlit fragment shader are unchanged. This change targets
the final graphics draw. Legacy two-sided rendering remains the default until
measurements on the target GPU justify selecting another path.

## Draw paths

| Mesh | Culling | Distinct vertex IDs per cube | Submitted triangles | Purpose |
|---|---|---:|---:|---|
| legacy | off | 36 | 12 | Original baseline |
| indexed | off | 8 | 12 | Reuse the eight transformed corners |
| legacy | back | 36 | 12 | Isolate culling and its shader safeguard |
| indexed | back | 24 | 12 | Share four vertices within each face and cull backfaces |

The eight-corner path uses one immutable 72-byte index mesh for every instance.
It preserves the original triangle order, diagonals and integer-grid boundaries.
Indexed draws let the GPU reuse vertex results; the table counts distinct IDs,
not measured shader invocations or a guaranteed speedup. Instancing and indirect
drawing remain in place. A separate 20-byte indexed command is initialized once;
each indexed frame copies only its four-byte instance count from the original
generator command. There is no CPU count readback added to the draw path and no
additional compute pass. A second 72-byte index mesh serves face-based rendering.

Culling is fixed-function `VK_CULL_MODE_BACK_BIT`. Original +X, +Y and -Z faces
had reversed winding, which is corrected in the culling path. The normal fragment
shader continues to output only stored RGB. Face directions in the vertex helper
describe cube geometry and culling; they introduce no voxel lighting.

Simply enabling backface culling would change near-plane and inside-cube views.
A clipped entry face exposes an exit face in the old two-sided renderer. The new
path tests the aligned bounds against the Vulkan near plane (clip Z = 0), and
reverses away-facing quads of cubes that touch that plane. This conservatively
retains their exit faces, including camera movement into a frozen cube. No source
topology, visibility ray or neighbor-volume lookup is involved. This needs
independent face vertices, so culling uses 24 rather than eight shared corners.
Debug face lighting also uses the 24-vertex indexed mesh.

The final depth attachment is discarded after its render pass; it is never read
later. The source depth attachment and its verification are unchanged.

## Measure on Windows

After rebuilding, from Command Prompt in the repository:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\benchmark_voxel_draw.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\benchmark_voxel_draw.ps1 -Live
```

The first command builds a cloud for eight frames, freezes it, and times the four
draw configurations over 232 frames, three times each. The source renderer still
runs. This removes generation variation from the draw comparison. The second
command measures the same configurations with generation active. Source time,
camera, scene, window size, voxel settings and samples are matched. Timing uses
GPU timestamps without validation, screenshots, cloud readbacks or extra shader
counters. Run order rotates, and the summary reports medians of the run averages.
It also checks matching counts and rejects empty or overflowing clouds.

The benchmark requests `--no-vsync` to avoid the normal FIFO presentation cap.
The demo prefers immediate presentation, then mailbox, with FIFO as the supported
fallback. Reports retain the actual present mode; the script flags a FIFO fallback.
Normal launches keep VSync enabled. For manual FPS comparisons, add `--no-vsync`
to the launch command. Immediate mode can tear; driver or desktop settings can
still impose a frame limit.

`RenderMs` includes indexed-command preparation as well as the draw and render
pass setup. `CubeMs` isolates the draw. `FrameMs` is timed GPU pipeline work; it
excludes presentation and CPU overhead and should not be treated as application
FPS. Raw JSON reports and `benchmark-summary.json` retain the actual device name
and identify CPU/software Vulkan. Software CI measurements establish correctness,
not Radeon performance.

Use `-Scene test`, `-Width 2560 -Height 1440`, `-VoxelSize .0057`, `-Levels 1`,
`-NoAdaptiveLod` or `-PointSplats` to match a workload. Very fine settings can fill
the bounded hash table; choose a setting without drops for the comparison.

For manual comparisons, freeze with **F**, then toggle **I** for indexing and **B**
for culling; neither rebuilds or changes the cloud. The HUD displays both flags.
Command-line equivalents:

```bat
.\build-windows\Release\microvoxels.exe --cube-mesh indexed --cube-culling off
.\build-windows\Release\microvoxels.exe --cube-mesh indexed --cube-culling back
```

## Correctness checks

`--exercise-render --frames 19 --voxel-size .12 --levels 1 --no-adaptive-lod`
captures an original view, then six outside, six near-plane and six inside views
around a cube of the frozen cloud. It checks byte-identical frozen instance data
and counters, plus the source-buffer CPU reference. CI compares actual RGB and
coverage against legacy rendering in both source scenes. The eight-corner path
must match pixels exactly. Culling permits only small raster/depth-tie differences
at edges, with no newly missing interior pixels. Normal freeze, closest color,
debug lighting, reset, resize and input checks also exercise the new paths.

## What profiling must decide next

Eight-corner indexing reduces repeated transforms and instance loads. Backface
culling reduces rasterized triangles but does not by itself reduce vertex work;
its bound tests and 24-vertex mesh may cost more than they save for tiny cubes.
Keep the configurations independent and select the fastest measured option.

Instances are already device-local and reused unchanged while frozen. Caching
clip-space corners across camera movement would invalidate them. Graphics
pipelines are created once; a disk pipeline cache would improve startup rather
than steady-state draw time. The renderer already has early depth testing, no
fragment discard, no blending, no fragment depth writes and a trivial RGB shader.

Hash-slot order gives deterministic equal-depth ties but poor front-to-back
locality. A depth/spatial sort might improve depth rejection and instance locality,
but adds passes and risks reintroducing tie flicker. Use hardware pipeline/AMD RGP
counters to distinguish vertex/primitive throughput, fragment overdraw and memory
stalls before adding that work. No spatial sorting, neighbor-face removal, real
voxel volume, greedy meshing, raycasting or mesh shader rewrite is introduced.

No physical GPU timings are available from this execution environment. Target
hardware improvements and the new bottleneck remain unmeasured until the Windows
benchmark is run on that device.

The initial software correctness run compared 19 views for each of the four paths
in each scene. Eight-corner indexing changed zero RGB pixels. Each culled path
changed three RGB pixels across the test-scene sequence and one across the garden
sequence, with no coverage changes or interior holes. All 18 frozen-frame buffer
checks per run and source-buffer references passed with zero validation errors.
