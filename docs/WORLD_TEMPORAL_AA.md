# World-grid temporal sampling

The primary source is now a GPU sphere tracer. Its unsigned distance field is the
minimum distance to the existing animated source triangles, queried through a
median-split BVH. Open grass ribbons work without converting the scene to a solid
SDF or changing its materials. Immutable BVH bounds include the full wind motion;
animated vertices are still computed on the GPU. No hardware RT extension is
needed. Source shading and the rasterized source shadow map remain unchanged.

`--source sphere` and `--source raymarch` select sphere tracing; `--source raster`
selects the original source pass. M toggles them. The tracer advances by 90% of
surface distance, with a 0.1 mm contact tolerance and a default 512-step budget.
`--trace-steps 64..2048` changes the budget. JSON exposes rays, steps, hits and
exhausted rays; exhausting the budget is a miss, not an invented surface.

## Accumulation belongs to world cells

J toggles `--taa`/`--no-taa`. Only source sample coordinates jitter, through eight
fixed offsets inside half a pixel. The camera matrix and final cube transforms do
not jitter. Source XYZ is quantized into the same world cell + power-of-two LOD
key. Fresh contributors first undergo the existing average/closest spatial color
reduction. The history pass then averages that result with the previous RGB of
that exact key. The default current weight is 0.2 (`--taa-alpha .2`), an exponential
moving average. Strong color changes react immediately rather than preserving
unrelated material or old illumination.

Matching fresh cells renew their observation time. Missing cells retain their
stored center/size and RGB without renewing that time. In temporal mode a missing
jitter phase is allowed to survive the default 200 ms window. Current known
incompatible LODs, frustum exclusion, expiry, camera cuts, source/settings changes,
resize and unfreeze reject/reset history. An unknown source region can retain its
previous LOD because it contains no fresh parent/child cells. When the scene or
camera moves, a complete 3x3 foreground depth neighborhood can reject occluded
history. When both are stationary, jitter alone does not invalidate visibility;
the eight phases can accumulate without repeatedly rejecting one another.

K or `--no-voxel-cache` disables occupancy/color history while retaining the jitter
setting for a controlled comparison. `--no-taa` disables jitter and temporal color
averaging and uses the earlier current-neighbor support policy. The original
raster/cache behavior is reproduced with `--source raster --no-taa --cache-ms 100`.

A hash slot's existing RGB words temporarily hold finalized float RGB when its
nearest field's high bit marks a temporal result. This happens after fresh
hashing; unique previous keys have one writer each. Compaction decodes that RGB
and retains fresh ownership/timestamps. The ordinary spatial RGB path and
closest contributor IDs remain unchanged when temporal AA is disabled.

## Cost and limits

There is no final-image TAA pass, screen-space blur, voxel visibility raymarch or
additional per-cell history allocation. The existing history/compaction passes
and 32-byte center/size + RGBA instances remain. The source adds an immutable
48-byte-node BVH, 4-byte triangle indices, a 16-byte trace counter, and a fullscreen
sphere-tracing draw instead of primary triangle rasterization. Generation still
reads only source XYZ/RGB buffers and has no source geometry bindings.

Sphere tracing can be substantially more expensive than rasterization. On the
available software Vulkan device, a 160x120 test-scene source comparison measured
about 251 ms for tracing versus 4.4 ms for rasterization, including the unchanged
shadow/source animation work. These are software measurements, not RX 6700 FPS.
The no-jitter captures had identical foreground hit counts (10,605), only 41 RGB
pixels differed, and mean absolute RGB difference was 0.015 on an 8-bit scale.
M keeps the raster source available; the same world temporal filter works with it.

History is bounded to 262,144 cells; capacity overflow bypasses history and is
reported. Low frame rates or short holds may expire a cell before its jitter phase
returns. Subpixel surfaces can still remain unsampled, and actual motion,
disocclusion and nested LOD changes still change occupancy. Longer history can
leave brief motion trails. Pause wind with P or freeze the whole cloud with F.

## Checks

`--exercise-temporal --frames 32` fixes source time and uses one 80 mm LOD. It
measures cell turnover and RGB changes after an eight-frame warmup, then exercises
freeze/camera movement, unfreeze, source changes, temporal toggles and an empty-view
camera cut. Every generated frame is checked against the CPU source-buffer and
history reference. Run with/without `--no-voxel-cache`; compare reports using
`tests/world_temporal_comparison.py`. CI additionally preserves the full raster
reference, footprint, nested LOD, freeze and draw-path suite with explicit legacy
settings. Unit tests check all source triangles are indexed exactly once, parent
bounds, 24 wind poses, deterministic unbiased jitter, color averaging and rejection.

The stationary garden fixture at 160x120, with a fixed 80 mm LOD and identical
jitter in both runs, measured 115,536 cell disappearances without history versus
86 with history after warmup (99.93% fewer). Mean RGB change across matching cells
fell from 0.0530 to 0.00955 (81.99% lower). Both runs passed 30 generated-frame
references, a byte-exact frozen-cloud check, five history-reset checks, and Vulkan
synchronization validation, with no exhausted rays. This is a controlled temporal
stability comparison, not a target-GPU performance or moving-camera quality claim.

From CMD, compare target-GPU source and temporal costs:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\benchmark_world_temporal.ps1
```
