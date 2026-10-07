# Raster-buffer footprint comparison

The voxel generator now consumes visible cached XYZ and final RGB only. The source renderer owns depth visibility, animation, materials, normals, lights and shadows. There is no post-process raycast and no source triangle topology in generation. Final cubes output stored RGB directly.

## Reconstruction and stability

The source already had a world-position buffer, so normal generation reuses it. Verification reconstructs world positions independently from the actual Vulkan D32 depth, pixel centres and inverse VP. Across the 12-frame moving-camera fixture, maximum cached/reconstructed XYZ disagreement was **1.345 mm**. Every generated frame passed the depth/projection checks. This measures finite-precision agreement at the sampled positions; it does not claim that different camera pixels observe the same point.

Both point and footprint modes produced byte-identical source and voxel RGB screenshots during three stationary frames, with fixed animation, lighting and LOD. GPU cell/RGB sets also matched exactly. Frozen instance/counter buffers remained byte-identical across camera motion. Shared cube corners and ordered hash-slot compaction removed the stationary equal-depth draw-order changes observed during development.

## Coverage and camera motion

Two matched experiments use a 640×480 source and side-by-side output. Both hold animation time at 1 second and use the same camera. Source RGB panes match exactly between modes.

The fine-cell coverage view uses 5.7 mm base pitch, five distance LOD levels, an 8.6 m first boundary, source shadows, and projected-size LOD disabled so the undersampling problem is visible. Interior pixels are source foreground after a 5×5 erosion; a void is a background voxel pixel inside that interior.

| Fine-cell view | Point splats | Footprint splats |
|---|---:|---:|
| Valid source samples | 183,023 | 183,023 |
| Candidate cell writes | 183,023 | 1,070,484 |
| Writes per valid sample | 1.00 | 5.85 |
| Unique cubes | 174,752 | 669,048 |
| Maximum writes from one sample | 1 | 16 |
| Interior void pixels, out of 88,081 | 14,181 | 12 |
| Dropped writes / root samples | 0 / 0 | 0 / 0 |

Candidates increased **5.85×**, or **485%**, and unique cubes increased **3.83×**. The radius was one cell and the write limit was 27. The measured footprint half extent never exceeded one cell.

The motion fixture holds a single 10 mm LOD and albedo lighting. After three stationary frames it makes nine camera steps of 2 mm in X and 0.00025 radians in yaw. The silhouette metric sums changes in `voxelMask - sourceMask` within a two-pixel source/background edge band. Subtracting the source mask removes the ordinary motion of the reference silhouette from this error measure.

| Moving-camera fixture | Point splats | Footprint splats |
|---|---:|---:|
| Temporal silhouette error | 6,076 | 791 |
| Added + removed cells in a static floor patch | 2,030 | 1,614 |

Measured silhouette error fell **87.0%** and floor-patch cell turnover fell **20.5%**. Footprints improve continuity without making occupied cells invariant under camera motion. Remaining changes come from visible-sample appearance/disappearance, rejected or missing neighbors, footprint clamps, and screen rasterization of cube edges. LOD and source shading can add changes in normal interactive use; both were held fixed in the motion comparison.

## Is 3×3 enough? Is projected-size LOD useful?

The five-tap cross inside 3×3 was enough to achieve the measured improvement. It supports one-sided derivatives and point/line fallback without source normals. It cannot infer a surface that has no valid neighboring samples. Continuity tests reject substantial depth/world-position jumps and incoherent directions; bounded patches limit the consequences of ambiguous nearby surfaces. Larger neighborhoods are not required for this first version.

Projected-size LOD is useful for grazing surfaces and distant fine grids. In the deliberately fine coverage view, **176,643 of 183,023 samples (96.5%)** needed radius or write-limit clipping. Expanding those samples indefinitely would increase cost rather than recover reliable detail. Projected footprint LOD is enabled by default, retains power-of-two pitches and hysteresis, and can be toggled with `C`. Multiple-LOD footprints also stop at incompatible or unknown root regions to prevent parent/child overlap.

## Cost and the previous triangle approach

Timings below are Vulkan GPU timestamps on **llvmpipe (LLVM 20.1.2), Mesa 25.2.8, Linux**. They are software-renderer measurements, not RX 6700 timings. Verification/readbacks and HUD were disabled while profiling; only the final frame was captured. The current modes ran 12 frames with the first two excluded. The previous triangle revision `a01e50cf689352396d15f796b660111a992876bc` ran eight frames with the first two excluded, using the same fine-cell camera/source settings.

| Fine-cell profile | Current points | Current footprints | Previous visible triangles |
|---|---:|---:|---:|
| Contributions / candidate writes | 183,023 | 1,070,484 | 2,689,565 |
| Unique cubes | 174,752 | 669,048 | 2,638,885 |
| Source shading + shadows | 7.43 ms | 7.07 ms | 5.42 ms |
| LOD + occupancy + hash + compaction | 106.74 ms | 132.23 ms | 320.08 ms |
| Cube draw alone | 311.69 ms | 1,208.85 ms | Not separately timed |
| Source comparison pane + cube render pass | 346.24 ms | 1,237.15 ms | 2,892.12 ms |

Current ordered compaction takes about 50–56 ms on this software device; it scans the fixed cell table and performs prefix sums to produce stable ordering. The old unordered atomic compaction was much cheaper there. Footprint generation nevertheless measured **24% slower than current points** and **59% faster than the previous triangle generator**. The latter emits a different, larger shell, including portions of visible triangles not individually represented by raster samples; these outputs do not provide equal coverage. Table capacities also differ.

The pipeline is cleaner for the requested generic filter: generation has no mesh-specific resources, per-triangle visibility mask or triangle append buffer. It is cheaper than the previous triangle path in this fixture, but footprint coverage costs more than point splats. **Final cube drawing is not essentially free in the available environment**: it dominates source shading, especially with 669,048 cubes and 36 vertices per cube. Physical-GPU profiling is still needed to measure the RX 6700 result.

## Reproduce and verify

The workflow runs Windows MSVC builds/tests and Linux software Vulkan with synchronization validation. It checks full GPU/CPU cell and RGB agreement, average/closest colour, freeze, camera/settings/resize controls, 2×2 source sampling, radius/write limits, both stationary/moving modes and the coverage comparison. The CPU overlap reference uses six-plane double-precision polygon clipping, independently of the GPU separating-axis test. All 18 shader modules pass SPIR-V validation.

Run the matched fixtures with the commands in [the workflow](../.github/workflows/build.yml). `tests/footprint_comparison.py` consumes their screenshots and reports. [footprints/measurements.json](footprints/measurements.json) records the local comparison and profile measurements; no hardware timings are inferred from them. [PROFILING.md](PROFILING.md) retains the earlier historical pixel-backend measurements.
