# Bounded visual voxel persistence

The cache keeps briefly missing world cells rather than rebuilding occupancy
entirely from the current frame. The original cache-only comparison uses 100 ms (`--source raster --no-taa --cache-ms 100`). The current world temporal AA mode uses 200 ms by default; see [world temporal sampling](WORLD_TEMPORAL_AA.md) for its RGB accumulation and missing-sample policy. **K** toggles
it; `--no-voxel-cache` restores frame-local occupancy and the original LOD timing.
`--cache-ms 0..500` sets the hold window. **P** pauses source wind and **F** freezes
the entire cloud.

## Source-buffer-only policy

Current raster samples generate cells and reduce RGB exactly as before. Then one
optional compute pass looks up each previous unique cell in the current hash.
An already present exact cell always wins with its freshly reduced RGB.

A missing cell survives only if its last exact observation is still within the
hold window, its world region still uses the same LOD, its center is in the current
frustum, and a current visible XYZ sample touches its world cell. The support
lookup covers at most 7×7 source pixels around the projected cell center. A 3×3
lookup proved too narrow because quantization moves that center away from the
actual supporting source pixel. World support is bounded to the cell plus a
0.15-cell allowance on each side, rejecting background and distant surfaces.

Retaining a cell never refreshes its last-seen time. Only a fresh exact-cell
contribution does. Cached colors are copied unchanged, and never enter the fresh
average or nearest-contributor reduction. Lighting/color modes, grid settings,
footprint settings, resize, view changes, camera resets/cuts and unfreezing clear
the occupancy history. Freeze leaves both instances and counters unchanged.

Regions also retain their settled LOD for the same short window. The first
cache prototype rejected most old cells because aliased grass derivatives kept
changing region pitch. Each region now stores its last actual pitch-change time.
Its existing distance/coverage/hysteresis request can change the pitch after that
window. All samples in the region share this decision. Cache invalidation bypasses
the hold, and old parent/child cells are rejected when the region does switch.

The pipeline remains source rasterization → world-grid filtering → hash dedup →
ordered compaction → unlit cubes. The cache uses visible source buffers and region
metadata; it has no mesh connectivity, object IDs, normals, ray tests or voxel
lighting. It is temporary visual history, rather than a persistent voxel world.

## Cost and limits

History is bounded to 262,144 cells. The cache adds 10 MiB of device-local storage:
8 MiB for previous 32-byte instances and two 1 MiB last-seen arrays. Region timing
adds another 2 MiB across the two existing region tables. Final instances remain
32 bytes per voxel. Each generation copies only the active previous records,
36 bytes per cell; it does not copy or clear the full history allocation. Existing
statistics already provide the previous count, so no new CPU count readback is
added. The merge is dispatched only over previous unique cells.

Clouds above the history limit bypass reuse for that frame; they still render all
fresh cells. JSON reports record capacity resets, retained/expired/rejected cells,
LOD rejections, copy bytes, and GPU cache resolve time. History-copy cost is included
in `lod_and_clear_ms`; `cache_resolve_ms` times the merge alone. Generation and
frame GPU totals include both. The HUD shows the cache mode, hold window, retained
count and resolve cost.

Wind moves real source geometry through a fixed grid. Short history reduces
disappearance but does not make animated grass immobile. Large LOD transitions,
disocclusion, subpixel coverage, grazing angles and the bounded support lookup
still cause turnover. Longer holds can leave brief motion history within the
support bound; use K to compare and P to isolate sampling from animation. Surface
identity is unavailable in this generic source contract, so the cache cannot
perfectly distinguish nearby surfaces within the same coarse cell.

## Verification and measurement

`--exercise-cache --frames 43` checks three stationary frames, 34 measured small
camera moves, a forced time jump for expiry, freeze/camera movement, an empty source
view, a camera reset and a grid change. Every generated frame is compared against
a CPU source-buffer/history reference, including fresh RGB, cached RGB, TTL,
deduplication and consistent LOD. Unit tests cover neighborhood support,
discontinuities, negative coordinates, frustum rejection and incompatible LODs.
Stationary garden screenshots with caching on/off are byte-identical. Existing
source animation, average/closest color, debug draw, indexed culling and resize
checks pass with the cache enabled. Fixed single-LOD cache checks also pass.

On software Vulkan with the garden at 480×360 and default settings, the matched
motion fixture recorded **98,330 cell removals with caching off and 62,787 with
caching on**, a **36.15% reduction**. The cache retained 15,404 cells over the
sequence and expired 4,031. Both runs passed 41 generated-frame references, two
stationary checks and the frozen-cloud check with zero validation errors. A 1 ms
hold, shorter than the fixture's 16.67 ms timestep, retained zero stale cells and
expired 6,440. These are occupancy measurements, not a measured perceptual shimmer
score or Radeon performance claim.

Two optional custom-pitch verifier cases also fail on the unmodified baseline:
garden footprint counters at 57 mm, and test-scene closest RGB at 21.6 mm. These
pre-existing CPU/GPU reference edge cases are unchanged by this feature.

From Command Prompt after rebuilding:

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File tools\benchmark_voxel_cache.ps1
```

This runs cache off/on in alternating order over three repetitions, using a
visible window, uncapped presentation, the same fixed camera and deterministic
source animation. It uses GPU timestamps with no verification, screenshots or
cloud readbacks. It reports generation, merge, draw and total GPU time plus mean
retained cubes, and saves raw reports. Keep the camera untouched during each run.
GPU frame time excludes presentation and CPU overhead. Software Vulkan timings
do not establish hardware performance; target Radeon overhead remains unmeasured.
