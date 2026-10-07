# Implementation notes

## Provider boundary

`SurfaceSamples` carries an RGBA32F colour texture, two RGBA32F world-position textures, and their dimensions.
Position `.w == 0` means no visible source surface. Valid opaque samples use `.w == 1` and colour alpha `1`.
The source shaders interpolate ordinary mesh normals, animate positions/normals, and compute the final RGB.
Tone mapping occurs in that source shading stage. These linear display colours are converted to sRGB once
by the final sRGB colour attachment. No normals or source material records enter the normal cube draw.

Both position textures are retained only to identify visible world regions across the immediately previous
generated frame. They are not a persistent geometry representation. Freezing deliberately retains one cloud.

## Nested grid and hysteresis

For level `l`, `h = base * 2^l` and `cell = floor(hit / h)`. The centre is `(cell + 0.5) * h`.
Negative coordinates use floor, and every grid shares the world origin. A colour reduction key includes
all three signed cell coordinates and the LOD, not just a truncated hash.

LOD is selected for a **world-space root region** whose size equals the largest enabled voxel size.
All visible samples inside that region use the same level. This prevents a large parent and smaller
children occupying the same region because individual pixels happened to fall on opposite distance thresholds.
The camera-to-region-centre distance determines the level; it approximates per-hit distance.

Boundary `l → l+1` is at `firstDistance * 2^l`. With history, refine below 90% of that boundary and coarsen
above 110%. Each region looks up its previous visible level in a sparse one-frame metadata hash. A newly
visible region selects its level directly. Regions unseen for a frame lose that history, so returning to them
can pop. Grid/LOD-count changes reset history. Large camera jumps can cross several boundaries in one frame.
Colour is averaged directly from the current source samples at the selected size. It is a screen-sample-weighted
average rather than a precomputed full-object mip hierarchy.

## Deduplication without publication races

An empty hash slot contains owner ID `0`. A compute invocation claims it atomically with `sampleID + 1`.
The owner ID refers to immutable source position and already-produced LOD images, so competing invocations
can derive the entire exact key immediately. There is no lock, partially published multi-word cell key,
float-atomic extension, or cross-workgroup spin-wait on an owner initializing its key.

Hash collisions are resolved by probing and comparing that exact key. Each accepted sample accumulates
10-bit RGB using 32-bit integer atomics and increments a count. RGB sum overflow is avoided by capping each
source image dimension at 2048: `2048 * 2048 * 1023 < 2^32`. Closest mode chooses minimum camera distance,
with source sample ID as a deterministic exact-depth tie-breaker, using an atomic compare-and-swap loop.

After the accumulation dispatch finishes, compaction visits occupied slots, averages RGB or selects the
closest sample colour, writes one `Voxel { vec4 centerSize; vec4 rgba; }`, and updates the indirect draw/counts.
Output capacity equals hash capacity, so compaction cannot write beyond the instance buffer.

The table has 1,048,576 slots and a 96-probe budget. The world-region history table has 262,144 slots and
a 64-probe budget. Exceeding either budget increments a visible statistic. A full visual table can create
holes; dropped history only loses hysteresis for those regions. The prototype reports these limits rather
than silently claiming full coverage. Use coarser cells if the visual table saturates.

## Draw and synchronization

The normal cube pipeline outputs stored RGB with a flat qualifier. It has no light uniforms in its fragment
shader, no cube-face shading, no AO, no shadow pass, and no BRDF. The debug pipeline alone receives cube-face
normals and deliberately modulates the captured colour by a directional cube light.

Each cube is currently generated procedurally as 36 non-indexed vertices. The first 16 bytes of the counters
buffer are a `VkDrawIndirectCommand`. One transfer initializes the entire counter buffer with 36 vertices
and zero instances/statistics; compaction atomically increments the instance count. Clearing the buffer and
then partially updating it would require a transfer-to-transfer dependency, since overlapping transfer writes
are not implicitly ordered. There is one indirect draw for the entire cloud. `--verify` also checks the
indirect command's vertex count and starting offsets, and profile reports expose those fields.

The prototype uses one graphics/compute/present queue and one frame in flight. Render-pass dependencies make
source colour/position writes visible to sampling; explicit barriers order clears, LOD image writes, colour
reduction, compaction, vertex reads, indirect-command reads, and transfer/host readbacks. Presentation uses
a semaphore per swapchain image; its reuse follows reacquisition of that image. The swapchain layout
transition uses the transfer stage in both scopes to chain it after the acquire semaphore's transfer-stage
wait. Validation is optional and
fails the run if an error is reported. CI additionally enables synchronization validation for its GPU checks.

Freeze skips all three generation dispatches and preserves instance/counter buffers. Source animation,
source shading and the camera continue. Resize recreates source images/history and presentation targets,
while retaining a frozen cloud. Unfreezing resamples the current camera view.

## Current compromises

- Raster sampling reveals only the nearest opaque source layer. Cubes cannot recover surfaces never sampled.
- A cell crossing a silhouette is a whole cube. It can protrude or partially cover a foreground feature.
- Screen sampling can miss small cells; 2×2 sampling and 3% enlarged splats reduce gaps without voxelizing volumes.
- At grazing angles and very fine sizes, holes remain. There is no temporal occupancy accumulation yet.
- Averaging a cell that contains several source surfaces mixes their already-shaded colours. Closest mode
  avoids that mix but can cause stronger colour popping.
- Sharp material boundaries and distance changes can produce visible popping. That is part of the experiment.
- Root-based LOD removes parent/child duplication but does not provide blended transitions between levels.
- Single-frame synchronization, full-table clears/scans, non-indexed cube geometry, and disabled face culling
  favour a small understandable prototype over peak throughput.

Reference APIs: [Khronos compute tutorial](https://github.khronos.org/Vulkan-Site/tutorial/latest/11_Compute_Shader.html),
[indirect drawing](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDrawIndirect.html), and
[timestamp queries](https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html).
