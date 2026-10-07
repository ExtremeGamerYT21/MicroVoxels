"""Compare matched GPU cache exercises; this measures cell turnover, not FPS."""
import json
import sys
from pathlib import Path

off, on, expiry = [json.loads(Path(path).read_text()) for path in sys.argv[1:4]]
for report in (off, on, expiry):
    assert report["validation_errors"] == 0
    assert report["verified_frames"] == 41
    assert report["cache_stability_checks"] == 2
    assert report["cache_freeze_checks"] == 1
    assert report["dropped_hits"] == report["dropped_root_history_samples"] == 0
    assert report["cache_dropped_voxels"] == report["cache_capacity_resets"] == 0
for key in ("frames", "scene", "samples", "triangles", "hits", "candidate_voxel_writes"):
    assert off[key] == on[key], f"Unmatched final source input: {key}"
assert off["voxel_cache"] is False and on["voxel_cache"] is True
assert off["cache_retained_total"] == 0 < on["cache_retained_total"]
assert on["cache_expired_total"] > 0
assert expiry["cache_hold_ms"] == 1
assert expiry["cache_retained_total"] == 0 < expiry["cache_expired_total"]
before, after = off["cache_motion_cell_removals"], on["cache_motion_cell_removals"]
assert 0 < after < before, "Cache failed to reduce cell disappearance in the motion fixture"
print(json.dumps({"cell_removals_off": before, "cell_removals_on": after,
                  "reduction_percent": round(100 * (1 - after / before), 2),
                  "retained_cells_total": on["cache_retained_total"],
                  "expired_cells_total": on["cache_expired_total"]}, indent=2))
