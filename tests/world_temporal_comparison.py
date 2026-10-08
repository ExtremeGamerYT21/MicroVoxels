"""Matched eight-phase source sampling with and without world-cell history."""
import json
import sys
from pathlib import Path

off, on = [json.loads(Path(path).read_text()) for path in sys.argv[1:3]]
for r in (off, on):
    assert r['validation_errors'] == 0
    assert r['verified_frames'] == 30
    assert r['temporal_freeze_checks'] == 1 and r['temporal_reset_checks'] == 5
    assert r['dropped_hits'] == r['dropped_root_history_samples'] == r['cache_dropped_voxels'] == 0
    assert r['cache_capacity_resets'] == 0
    assert r['source_ray_count'] == r['samples'] > 0
    assert r['source_raymarch_steps_total'] > 0
    assert r['source_raymarch_exhausted_total'] == 0
assert on['temporal_rgb_blends_total'] > 0 == off['temporal_rgb_blends_total']
assert on['temporal_cell_removals'] < off['temporal_cell_removals']
assert on['temporal_mean_rgb_change'] < off['temporal_mean_rgb_change']
print(json.dumps({
    'cell_removals_without_history': off['temporal_cell_removals'],
    'cell_removals_with_history': on['temporal_cell_removals'],
    'rgb_change_without_history': off['temporal_mean_rgb_change'],
    'rgb_change_with_history': on['temporal_mean_rgb_change'],
    'trace_budget_exhausted_total': on['source_raymarch_exhausted_total'],
}, indent=2))
