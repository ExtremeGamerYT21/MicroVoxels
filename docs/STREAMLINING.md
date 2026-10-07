# Microvoxel pipeline streamlining

The default replaces three compaction dispatches with append-on-first-insert plus one unique resolve, halves final instances to **16 bytes**, and clears occupied slots when the table is sparse. Cached XYZ, region LOD, footprint coverage, RGB reduction and the unlit cube draw remain intact. Depth reconstruction and shared local dedup are independently selectable experiments.

Measurements below use **llvmpipe (LLVM 20.1.2, 256 bits), software Vulkan on Linux**. They are not AMD/RX 6700 measurements. [streamlining/measurements.json](streamlining/measurements.json) contains raw runs, medians, ranges and correctness results. The parent baseline is `f3f37fffcf5299a10e82ba445fc9582407ab3d8f`, rebuilt with equivalent timestamp instrumentation only.

## Current functions and passes

| Stage | Function/shader | Work |
|---|---|---|
| Source | `SourceRenderer::render()` / `shadeSurface()` | Animate vertices, shadow map, opaque triangle rasterization; output depth, final RGB and optional XYZ |
| Reset | `VisualVoxelizer::generate()` / `clear_touched.comp` | Clear previous unique slots or bulk-clear a dense table; reset counters and current root metadata |
| LOD | `lod.comp` / `lod_resolve.comp` | Existing distance/coverage requests, hysteresis and one consistent nested LOD per world region |
| Generation | `voxelize.comp` / `processPixel()` | Load/reconstruct visible XYZ; unchanged point/footprint quantization |
| Dedup | `emitCell()` / `emitGlobal()` | Optional bounded shared dedup; exact global hash; append slot only on its first insertion |
| Resolve | `resolve_unique.comp` / `storeInstance()` | Visit only unique slots; finalize RGB; write packed key and colour |
| Draw | `VoxelRenderer::draw()` / `cubeVertex()` | Decode cell/LOD, derive centre/size, one indirect cube draw; `outColor = color` |
| Present | HUD / swapchain | Statistics and display |

The core filter has **four compute dispatches**, down from six: two LOD passes, generation and resolve. Sparse clearing adds one dispatch, for five; dense/initial clearing uses a transfer command. Source animation/shadow/rasterization and final graphics are outside this count. There are no source triangles, normals, material evaluation or raycasts in the filter.

`uniqueCount` occupies the indirect draw's instance-count word. Generation also produces the resolve dispatch count entirely on GPU. The compact list survives for next-frame clearing. No CPU count readback was added to schedule resolve or drawing. Existing HUD readback supplies the previous occupancy used by the clear policy.

The default clear policy uses touched slots below **25% table occupancy**, and a bulk clear above it. `--touched-clear` and `--full-clear` force either path. Even a bulk-cleared frame tracks its unique list when automatic clearing is enabled, so a later switch back to sparse clearing is safe. The first generation always clears the whole table.

## Matched coarse-grid profiles

Each variant ran three times, in rotated order, for 30 frames with the first two excluded. Inputs: 640×480 source, fixed time 1 s, 10 mm base pitch, five levels, 4 m first distance boundary, coverage LOD on, footprint radius one, cap 27, source shadows. HUD, verification and image readbacks were off. Isolated markers order timestamps explicitly; ordinary markers can attribute later compute/draw work to an earlier interval on this software driver.

Values are medians of each run's average. Component medians need not sum exactly to the median total.

| GPU timestamp interval | Parent | New default |
|---|---:|---:|
| Source animation + shadow + raster | 7.63 ms | 7.25 ms |
| Hash-table clear | 1.28 ms | 0.46 ms |
| Root/counter reset | 0.30 ms | 0.22 ms |
| LOD selection | 4.52 ms | 5.02 ms |
| LOD resolve | 1.69 ms | 1.77 ms |
| Footprint generation + global dedup | 12.92 ms | 18.68 ms |
| Compact counts | 26.78 ms | Bypassed |
| Compact prefix | 0.39 ms | Bypassed |
| Compact write | 36.77 ms | Bypassed |
| Unique resolve | — | 0.97 ms |
| Total filter generation | **84.51 ms** | **27.29 ms** |
| Cube draw | 41.08 ms | 35.94 ms |
| GPU frame, before present | **134.48 ms** | **72.07 ms** |
| GPU-equivalent FPS (`1000/frame`) | 7.44 | 13.88 |

Generation improved **67.7%** and GPU frame time improved **46.4%** in this fixture. New wall-frame FPS was 13.76. Append/list/statistic updates increase the generation/hash interval; removing the table-wide compaction work more than compensates. These measurements do not imply that every individual stage became faster.

| Independent variant | Generation | GPU frame | Instance stride |
|---|---:|---:|---:|
| Parent source | 84.51 ms | 134.48 ms | 32 B |
| Append + resolve, full clear, unpacked | 31.50 ms | 76.78 ms | 32 B |
| Packed, full clear | 29.46 ms | 67.79 ms | 16 B |
| Packed, touched clear: sparse default | 27.29 ms | 72.07 ms | 16 B |
| Depth reconstruction with tile | 34.35 ms | 89.12 ms | 16 B |
| Depth reconstruction without tile | 31.83 ms | 83.48 ms | 16 B |
| Cached XYZ + local dedup | 28.60 ms | 72.19 ms | 16 B |

Run-to-run frame ranges overlap, especially for packing/clearing/local dedup. Packing reduced memory and measured draw cost; touched clearing reduced clear cost at low occupancy. Their combined frame-time benefit should not be inferred from one noisy row comparison. The compaction removal is the clear, large improvement.

## Counts and memory

| Metric | Parent / cached XYZ direct | New cached XYZ direct | New cached XYZ local | Depth direct |
|---|---:|---:|---:|---:|
| Source pixels | 307,200 | 307,200 | 307,200 | 307,200 |
| Valid samples | 183,023 | 183,023 | 183,023 | 183,023 |
| Candidate writes | 377,760 | 377,760 | 377,760 | 391,290 |
| Global insert/update attempts | 377,760* | 377,760 | 45,279 | 391,290 |
| Unique cells | 19,739 | 19,739 | 19,739 | 23,692 |
| Table load | 0.941% | 0.941% | 0.941% | 1.130% |
| Contributors per unique cell | 19.14 | 19.14 | 19.14 | 16.52 |
| Instance bytes per voxel | 32 | 16 | 16 | 16 |
| Active instance bytes | 631,648 | 315,824 | 315,824 | 379,072 |
| Instance allocation | 64 MiB | 32 MiB | 32 MiB | 32 MiB |

*The parent has one global attempt per candidate; it lacked a separate attempt counter. Current variants count attempts and probes explicitly. New cached direct has 380,657 probes, approximately 1.008 per attempt: collisions are rare in this coarse view. No cell/root writes were dropped in the measured fixtures.

The new list costs 8 MiB at maximum capacity. Together, list + final instance allocations are 40 MiB versus the old 64 MiB instance allocation. The 48 MiB hash table remains bounded at 2,097,152 slots. Sparse clearing writes 473,736 bytes of occupied slot payload in the coarse fixture instead of the whole 50,331,648-byte table, plus list reads; these are logical payload sizes, not measured memory-controller traffic.

The spatial key stores three signed 20-bit coordinates and three LOD bits in two uint32 words. RGBA16F uses two more words. This fits 16 bytes without shaderInt64 and preserves more colour precision than RGBA8. Position/size are decoded from the key and the cloud's captured base pitch. Packing does not change integer RGB accumulation or nearest-contributor selection.

## Dense clouds and decisions

A second, deliberately undersampled fixture uses 5.7 mm base pitch, five distance levels, an 8.6 m first boundary and coverage LOD off. It emits 1,070,484 candidates and 669,048 unique cells, at 31.9% table load. These are short six-frame runs, four timed frames, so the frame numbers are illustrative rather than repeated medians.

| Dense-cloud variant | Hash clear | Generation | Cube draw | GPU frame |
|---|---:|---:|---:|---:|
| Parent | 1.35 ms | 130.51 ms | 1,052.63 ms | 1,190.28 ms |
| Append, unpacked, touched | 5.75 ms | 96.64 ms | 1,198.09 ms | 1,305.42 ms |
| Packed, forced touched | 6.49 ms | 96.53 ms | 758.98 ms | 863.34 ms |
| Packed, bulk clear | 1.95 ms | 90.37 ms | 803.29 ms | 901.73 ms |
| Packed, local dedup, touched | 6.00 ms | 129.53 ms | 827.01 ms | 963.42 ms |
| Final automatic policy, bulk clear | 1.66 ms | 80.93 ms | 1,089.96 ms | 1,178.07 ms |

The dense clear comparison motivated the automatic bulk fallback. It does not change occupied cells. The 25% threshold is a simple heuristic from the available measurements; use the forced variants to evaluate a physical GPU.

Local dedup reduced coarse global attempts **88.0%**, but gave no repeatable frame-time gain. In the dense case it reduced attempts to 741,350 while adding substantial shared-table/key-lookup overhead; bounded fallback remained correct. It stays **off** by default. The mini hash is a separate shader, so disabling it also removes its shared-memory allocation and barriers.

Depth reconstruction saves the XYZ render-target write and both XYZ history allocations. It remained slower in these comparisons. More importantly, finite D32/inverse-projection rounding moves samples around an exact grid plane, particularly the floor at Y = 0, producing additional cells and changed footprints. CPU/GPU inverse reconstruction agrees within micrometres in the depth-only reference, but that does not imply identical floor quantization to interpolated XYZ. Cached XYZ therefore stays **on** by default. Tiling did not help this device; both depth variants remain available for comparison.

LOD selection/resolve retain the same world-region table, nested pitches and hysteresis. Resolve is roughly 1.8 ms in the new coarse profile, a small part of the complete frame; there is no physical-GPU evidence supporting a region-update rewrite. Both passes remain unchanged.

## Correctness and remaining limits

GPU/CPU checks cover exact cells, duplicate detection, RGB weighting/selection, LOD, footprint statistics, packed coordinates, indirect commands, average/closest modes, full/touched clears, local overflow fallback, odd tile edges, 2×2 sampling, empty-cloud recovery, freeze and resize. All 22 shader modules pass SPIR-V validation; three unit suites pass. Windows MSVC and software Vulkan checks run in CI.

Matched legacy/append/packed/local screenshots have **identical source RGB and voxel coverage**. Packed-versus-legacy mean absolute RGB error is **0.00972 of one 8-bit code point**, including 21 equal-depth edge pixels with a different selected cube colour. Half rounding otherwise changes a display channel by at most one code point. Append order is intentionally unsorted; this is a near-identical visual result rather than byte-identical output relative to the old ordered draw.

With fixed camera/time/lighting/LOD, source and voxel RGB screenshots are byte-identical across stationary frames in point, footprint and local-dedup modes. Frozen instance/counter buffers are byte-identical across camera motion. Dense-to-sparse transitions are also reference-checked so automatic clear selection cannot leave stale cells.

The previous footprint results are preserved exactly: 14,181 fine-grid interior void pixels become 12; source-relative temporal silhouette error falls from 6,076 for points to 791 for footprints, an **87.0% reduction**. Static floor-patch turnover remains 2,030 versus 1,614, a **20.5% reduction**. The five-tap cross within 3×3 is still sufficient for this measured improvement. Remaining shimmer comes from source visibility/sample turnover, missing/rejected neighbors, footprint caps, LOD-region clipping and cube-edge rasterization. Coverage LOD remains useful when a source pixel spans many smaller cells.

The fused footprint/hash shader is the largest remaining filter interval. Overall cube drawing dominates this software renderer, particularly with hundreds of thousands of cubes; it is not free here. Source rendering is not the measured bottleneck. The data does **not** establish whether AMD hardware is bandwidth-, atomic- or ALU-limited: low collision rates and the lack of a local-dedup win are insufficient to identify that without hardware counters. No new cube rendering architecture is introduced.

## Repeat on the laptop

From the existing Windows project directory, these commands work in Command Prompt or PowerShell:

```powershell
git pull
cmake --build build-windows --config Release --parallel
.\build-windows\Release\microvoxels.exe --frames 120 --time 1 --no-ui --profile-stages --report captures\new.json
.\build-windows\Release\microvoxels.exe --frames 120 --time 1 --no-ui --profile-stages --legacy-compaction --unpacked --full-clear --report captures\old-path.json
```

The retained old path uses the current shaders/counters, so it includes the new statistics overhead; the original parent rebuild supplies the historical comparison above.

For repeated, rotated-order comparisons, Python is optional:

```powershell
python tools\profile_pipeline.py --binary build-windows\Release\microvoxels.exe --output captures\pipeline-profile --frames 120 --repeats 3
python tools\profile_pipeline.py --binary build-windows\Release\microvoxels.exe --output captures\pipeline-throughput --frames 120 --repeats 3 --ordinary-timestamps
```

The first command isolates passes; the second retains normal overlap for production throughput. JSON reports contain source, LOD, clear, fused generation/dedup, each legacy compaction stage or unique resolve, cube draw, counts, memory and wall/GPU frame times. Verification and image readbacks are disabled by the script. Add shared settings after `--`, for example `-- --voxel-size .0057 --lod-distance 8.6 --no-adaptive-lod`.
