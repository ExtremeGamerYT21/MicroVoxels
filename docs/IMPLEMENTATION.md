# Implementation notes

## Generic raster-buffer provider

`SurfaceSamples` carries final linear RGB, two RGBA32F visible world-position images, and dimensions. Position `.w == 0` means background; opaque samples use `.w == 1`. The second position image supports one generated frame of world-region LOD history. It is not a persistent geometry representation.

The demo mesh renderer computes animated vertices, shadowing, smooth normals, materials and tone mapping inside the source stage. Its opaque depth test determines the visible sample. The generator receives no triangle IDs, vertex buffers, normals, material records or shadow texture. Source RGB is evaluated once per source fragment, then reused for every footprint contribution. The former triangle-specific generator and its append buffer are removed.

The existing cached XYZ buffer is used directly. Verification also reads actual D32 source depth, reconstructs XYZ with inverse VP, and compares it with cached positions. Vulkan NDC depth is `[0,1]`; pixel centres use `2*(pixel+0.5)/extent - 1`. The projection already includes the Vulkan Y flip. The check permits one eighth of a source pixel for raster edge snapping/interpolation and checks world-space reconstruction error separately. Reconstruction is a verification operation.

## Source-pixel footprints

A five-tap cross (centre, left, right, up, down) fits inside a 3×3 source neighborhood. Valid neighbor deltas are oriented in the same positive screen direction. The closer continuous side is used, permitting one-sided derivatives at a silhouette. Background and out-of-image neighbors are ignored.

Continuity rejects world separation beyond `min(0.25 m, 8 * nominalPixelSpan)` and view-depth jumps beyond `max(4 * nominalPixelSpan, 2.5% * viewDepth)`. Nominal span comes from VP projection scale, dimensions and clip W. If accepted direction lengths differ by a ratio above 2, the longer side is rejected. Comparable directions with cosine below 0.5 are both rejected. Missing directions become zero: a line or point fallback.

The sample and derivatives define `p ± dx/2 ± dy/2`. Derivatives are scaled together if the largest world-axis half extent exceeds `radius * selectedPitch`. The default radius is one, configurable from zero to two. The default write cap is 27, configurable from one to 64. The point's own cell is emitted first, preserving the point baseline under clipping/caps.

Candidate bounds use floor quantization and at most `(2*radius+1)^3` cells. A separating-axis test checks the parallelogram against each cell: box axes, patch normal and patch-edge/box-axis cross products. It also handles line/point fallbacks. Coordinates are cell-relative, with a 0.0001-cell tolerance. Zero-width axes retain floor ownership, avoiding a second layer for a grid-aligned plane. These are pixel patches inferred solely from source positions, not source triangles or camera rays.

Each contribution uses the centre pixel's final source RGB. No BRDF or lighting is evaluated at a generated cell. Average mode is source-contribution weighted. Closest mode selects the nearest visible source point, with pixel/offset ID breaking exact-distance ties.

## World grid and LOD

For level `l`, `h = base * 2^l`, `cell = floor(hit/h)`, and centre is `(cell+0.5)*h`. Signed coordinates and level form the exact key. Grids share world origin; camera movement never snaps the grid.

Distance and projected-source-footprint LOD are chosen for a world root region sized like the largest enabled pitch. Samples in a region reduce their requests to the maximum; a resolve dispatch assigns that same level. Distance uses the region centre with 90/110% hysteresis. Coverage uses `1.25 * max(abs(dx)+abs(dy))`, coarsens immediately and refines below the 90% threshold. `C` toggles coverage LOD. Size, level-count, coverage-mode and source-target changes reset history.

With multiple levels, expansion crosses a root boundary only if the target region is registered at the same LOD. It is clipped at unknown/incompatible regions, preventing overlapping parents/children. With one level, unsampled cells are allowed because every region has the same pitch. History is one generated frame; unseen regions lose it. Freeze retains the current cloud.

## Exact deduplication

A hash slot stores an immutable owner ID, integer RGB sums/count and a closest-contributor ID. Owner zero means empty. Nonzero owner encodes `sourcePixel*125 + offsetIndex + 1`, with offsets in `[-2,2]^3`. The full cell key is immediately recoverable from immutable source XYZ, resolved LOD and that offset. There are no partially published multi-word keys, spin locks, float atomics or append-buffer publication races.

Collisions probe and compare the full coordinate/level key. There are 2,097,152 cell slots, 96 cell probes, and 262,144 root slots with 64 root probes. Instance capacity equals cell-table capacity. Capacity/probe failures increment visible statistics; no alternate occupancy path is silently used.

Each source pixel contributes at most once to a cell. Dimensions are capped at 2048, so `2048*2048*1023 < 2^32` bounds 10-bit RGB sums. The 64-write cap bounds candidates below 2^32. Packed owners remain below 2^29. Closest selection uses atomic compare-and-swap.

## Ordered draw and synchronization

Compaction counts occupied slots per 256-slot group. One compute group scans counts into group offsets and writes the indirect instance count. Local prefix scans then place instances in hash-slot order, replacing unordered atomic append. This fixes the observed equal-depth differences in the stationary rendered fixture. Exact cell-sized cube corners come from integer grid boundaries, so neighbors share the same floating-point corner values.

Each instance is only `vec4 centreSize; vec4 rgba`. The normal fragment shader outputs stored RGB directly. The separate cube-light debug pipeline uses procedural face directions without altering the source. Each cube currently uses 36 procedural vertices and one opaque indirect draw. Drawing hundreds of thousands of cubes is measured rather than assumed free.

The first 16 bytes of the 80-byte counters buffer remain `VkDrawIndirectCommand`. One transfer initializes the whole buffer with 36 vertices and zero statistics. Explicit compute dependencies order LOD requests, resolve, footprint/hash writes, counts, prefix and compaction. Final barriers cover indirect/vertex reads and transfer/host readbacks. Render-pass dependencies cover source sampling. Swapchain acquire and layout transitions remain chained at TRANSFER. CI enables synchronization validation.

One frame is in flight. Presentation semaphores are reused after image reacquisition. Freeze skips all generation and preserves instance/counter buffers while source and camera continue. Resize rebuilds source/history/render targets while retaining frozen instances.

## Verification and statistics

References read actual GPU position/RGB/depth/LOD images and instances. They check every exact cell, RGB reduction, lattice centre, alpha, per-level count, indirect header and footprint statistic. Occupancy uses independent double-precision six-plane polygon clipping rather than the GPU SAT. Unit fixtures cover continuous derivatives, foreground/background jumps, isolated samples, bounds and nonintersecting patches with overlapping bounds.

The live fixture fixes source time and albedo, holds one LOD, checks a stationary cloud, then makes small camera changes. Screenshot comparison checks exact stationary RGB bytes, identical source panes, interior holes and source-relative temporal silhouette error. Animation, lighting and LOD changes are excluded from that comparison.

HUD/report fields include resolution and sample count, valid hits, candidate writes, unique cells, writes per valid/all source sample, maximum emitted cells and half extent, clamped samples, rejected neighbors, drops, frame/generation timings and a separately scoped cube-draw timestamp. HUD counters describe the previous generation; frozen counters describe the retained cloud. Verification/readbacks should be disabled for timing.

Remaining changes can come from source visibility, discontinuity fallback, finite radius/write caps, clipping at LOD regions, material/color reduction, and screen rasterization of cube silhouettes. Projected-size LOD helps when a source pixel represents many fine cells. The filter cannot infer surfaces absent from source buffers. Temporal occupancy accumulation and transparent layers are not implemented.
