"""Check matched coverage/RGB when independently toggling filter optimizations."""
import json
import sys
from pathlib import Path
from footprint_comparison import masks


def compare(folder):
    folder = Path(folder)
    reference = masks(folder / "pipeline-legacy.ppm")
    report = json.loads((folder / "pipeline-legacy.json").read_text())
    if report["instance_bytes_per_voxel"] != 32:
        raise AssertionError("The legacy reference must use full instances")
    results = {}
    for name in ("append", "packed", "local"):
        frame = masks(folder / f"pipeline-{name}.ppm")
        current = json.loads((folder / f"pipeline-{name}.json").read_text())
        if current["instance_bytes_per_voxel"] != (32 if name == "append" else 16):
            raise AssertionError(f"{name}: instance packing did not change the actual buffer stride")
        if frame[:4] != reference[:4] or frame[4] != reference[4]:
            raise AssertionError(f"{name}: source pixels or voxel coverage changed")
        for key in ("hits", "candidate_voxel_writes", "unique_voxels", "voxels_per_lod",
                    "max_footprint_cells", "clamped_footprints", "rejected_neighbors"):
            if current[key] != report[key]:
                raise AssertionError(f"{name}: {key} changed")
        if not current["verified_frames"] or current["validation_errors"] or current["dropped_hits"]:
            raise AssertionError(f"{name}: GPU/CPU reference or validation failed")
        if name == "local" and current["global_hash_attempts"] >= current["candidate_voxel_writes"]:
            raise AssertionError("Local deduplication did not reduce global hash traffic")
        errors = [abs(a - b) for a, b in zip(frame[5], reference[5])]
        edge_ties = sum(max(errors[i:i+3]) > 1 for i in range(0, len(errors), 3))
        # RGBA16F can round an 8-bit channel by one code point. Append order can
        # choose a different stored colour at a few exactly equal-depth cube edges.
        if edge_ties > max(2, sum(frame[3]) // 1000) or sum(errors) / len(errors) > .1:
            raise AssertionError(f"{name}: material RGB difference beyond rounding/edge ties")
        results[name] = {
            "source_rgb_identical": True,
            "voxel_coverage_identical": True,
            "mean_rgb_code_error": sum(errors) / len(errors),
            "edge_tie_pixels_above_one_code": edge_ties,
            "bytes_per_voxel": current["instance_bytes_per_voxel"],
        }
    return results


if __name__ == "__main__":
    result = compare(sys.argv[1])
    print(json.dumps(result))
    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(json.dumps(result, indent=2) + "\n")
