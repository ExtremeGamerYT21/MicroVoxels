# Implementation notes

## Generic raster-buffer provider

`SurfaceSamples` carries final RGB, dimensions, and two visible-sample images. The default images are RGBA32F world XYZ (`w == 0` for background). The experimental depth provider uses two D32 images instead (`depth == 1` for background), plus current/previous inverse VP in the frame uniform. The second image supports one generated frame of world-region LOD history. It is not a persistent geometry representation.

The demo mesh renderer computes animated vertices, shadowing, smooth normals, materials and tone mapping inside the source stage. Its opaque depth test determines the visible sample. The generator receives no triangle IDs, vertex buffers, normals, material records or shadow texture. Source RGB is evaluated once per source fragment, then reused for every footprint contribution. The former triangle-specific generator and its append buffer are removed.

`world_samples.glsl` isolates loading/reconstruction from the voxel filter. Cached XYZ remains the default after profiling. `--depth-source` removes both position targets and the XYZ fragment output; depth is sampled directly in every filter phase. Vulkan NDC depth is `[0,1]`; pixel centres use `2*(pixel+0.5)/extent - 1`. The projection already includes the Vulkan Y flip. Current and previous depth use their corresponding inverse matrices. Precise projection arithmetic avoids contraction differences between key-recovery paths.

The depth variant can cooperatively reconstruct an 8×8 tile with a one-pixel halo into 100 shared positions for footprint/LOD derivatives. `--no-depth-tiles` bypasses this cache. Odd image edges participate in barriers and load background outside the image. Arbitrary hash-owner lookups still reconstruct directly. The available device did not benefit from tiling. Depth rounding changes cells near grid planes, so this is a comparison mode rather than the production default.

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

## Compact unique list and packed draw

Only the invocation that changes a global slot owner from zero appends that slot index to `uniqueSlots[]`. `instanceCount` is the append counter and eventual indirect draw count. Duplicate contributions update RGB/count/closest ID without appending. A GPU atomic maximum maintains `ceil(uniqueCount/256)` for indirect resolve and next-frame clearing. All slot/list writes complete before resolve starts; no CPU dispatch-count readback is added.

`resolve_unique.comp` visits occupied slots only, finalizes average or selected RGB, derives the exact cell from its immutable owner, and writes a packed instance. The old count/prefix/compact pipelines remain selectable with `--legacy-compaction` for comparisons. A permanently full-clear legacy run skips unique-list appending; automatic or touched clearing tracks the list even when a dense frame uses a bulk clear.

Default instances are 16 bytes: two uint32 words encode signed 20-bit XYZ plus three LOD bits, and two uint32 words contain RGBA16F. No shaderInt64 capability is required. Coordinates range from -524,288 through 524,287; out-of-range writes increment drops. At 10 mm this covers approximately ±5.24 km at LOD zero. Half colour preserves more precision than RGBA8, with a maximum 0.0002442 RGB rounding error in `[0,1]`. `--unpacked` retains the old 32-byte centre/size + RGBA path.

The vertex shader derives pitch and centre from cell/LOD. Shared corners still come from integer grid boundaries. Base pitch and cube scale are captured when generating the cloud, so changing settings while frozen cannot move packed cubes. The normal fragment shader remains `outColor = color`. The separate existing cube-light debug pipeline is preserved. Each cube uses 36 procedural vertices and one opaque indirect draw.

Append order is not sorted. Cell keys and integer RGB reductions remain deterministic. Stationary screenshots pass byte-exact comparisons on the test device. Compared with the legacy ordered draw, 21 equal-depth edge pixels select a different stored colour in the matched coarse view; coverage is identical. Half packing can round a final display channel by one code point. The render comparison explicitly bounds these differences.

## Clears, local deduplication and synchronization

Touched clearing dispatches over the previous list and zeroes all 24 bytes of each occupied hash slot, then counters and current root metadata reset. The first generation performs a full table clear. By default a previous unique count below 25% capacity selects touched clearing; dense clouds use a bulk clear because profiling showed scattered clears were slower there. This count is already read for existing HUD statistics. `--touched-clear` and `--full-clear` force either path. No generation tags or slot locks are introduced.

`--local-dedup` selects a separate shader with a 256-slot shared mini hash per 8×8 source workgroup and 16 local probes. It publishes immutable owners with one-word CAS, adds integer RGB sums/count, and selects the same nearest contributor. After a barrier, local aggregates merge globally. A full local table falls back to immediate global insertion, preserving all contributors. Threads outside odd-sized images still participate in barriers. The direct shader allocates no mini hash. Shared storage is 6 KiB for this table, plus 1.6 KiB when depth tiling is active.

The first 16 bytes of the 104-byte counters buffer remain `VkDrawIndirectCommand`; the final 12 bytes are `VkDispatchIndirectCommand`. One transfer initializes the entire buffer, including 36 vertices and dispatch Y/Z = 1. Dependencies cover previous indirect/list reads before reset, touched writes before new insertions, LOD requests/resolution, generation publication, resolve, and indirect/vertex/transfer reads. Source depth/color dependencies include their raster writes. Swapchain acquire and layout transitions remain chained at TRANSFER. CI enables synchronization validation.

Four filter compute passes remain: LOD request, LOD resolve, generation, unique resolve. Touched clearing adds a fifth; bulk clearing is a transfer command. The source animation compute pass and source/final graphics passes are separate. `--profile-stages` inserts execution barriers after timestamps to isolate command costs; the final render pass declares the required self dependency for its profiling markers. Ordinary mode retains overlap. Frozen frames write every timestamp in chronological order while skipping generation.

One frame is in flight. Presentation semaphores are reused after image reacquisition. Freeze skips all generation and preserves instance/counter buffers while source and camera continue. Resize rebuilds source/history/render targets while retaining frozen instances.

## Verification and statistics

References read actual GPU position/RGB/depth/LOD images and instances. They check every exact cell, RGB reduction, lattice centre, alpha, per-level count, indirect header and footprint statistic. Cached XYZ is independently checked against inverse-projected D32 depth with a one-eighth-pixel raster tolerance and a separate world-error bound. In depth mode a verification-only buffer records the GPU reconstructions used for quantization, avoiding CPU/GPU grid-boundary rounding differences; CPU inverse projection checks those positions separately. This buffer is absent from timed generation apart from a 16-byte inactive descriptor placeholder.

Closest-colour verification normally checks the exact CPU nearest sample. If CPU/GPU dot/sqrt arithmetic resolves a numeric near tie differently, only an actual contributor within four float epsilons of the minimum distance and with matching source RGB is accepted. Such cases are counted in `closest_numeric_reference_ties`; the shader selection algorithm is unchanged. Occupancy uses independent double-precision six-plane polygon clipping rather than the GPU SAT. Unit fixtures also cover packed signed boundaries, word layout, world centres and colour precision.

The live fixture fixes source time and albedo, holds one LOD, checks a stationary cloud, then makes small camera changes. Screenshot comparison checks exact stationary RGB bytes, identical source panes, interior holes and source-relative temporal silhouette error. Animation, lighting and LOD changes are excluded from that comparison.

HUD/report fields include resolution/samples, valid hits, candidate writes, global hash attempts/probes, unique cells, table load, contributors per unique cell, instance stride/active/capacity bytes, footprint limits/rejections, source/frame/generation/draw times and individual pass intervals. Voxelization and dedup are fused, so their combined interval is reported rather than inventing separate GPU times. JSON also identifies each optimization/provider/clear policy and wall frame time/FPS. GPU frame timestamps end before presentation; wall time includes presentation and CPU overhead. HUD counters describe the previous generation; frozen counters describe the retained cloud. Disable verification and image readbacks for timing.

Remaining changes can come from source visibility, discontinuity fallback, finite radius/write caps, clipping at LOD regions, material/color reduction, and screen rasterization of cube silhouettes. Projected-size LOD helps when a source pixel represents many fine cells. The filter cannot infer surfaces absent from source buffers. Temporal occupancy accumulation and transparent layers are not implemented.
