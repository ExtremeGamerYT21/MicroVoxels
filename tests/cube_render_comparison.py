"""Compare actual cube pixels, including frozen near-plane and inside views."""
import json
import sys
from pathlib import Path


def ppm(path):
    magic, dimensions, maximum, pixels = path.read_bytes().split(b"\n", 3)
    w, h = map(int, dimensions.split())
    assert magic == b"P6" and maximum == b"255" and w % 2 == 0
    assert len(pixels) == w * h * 3
    return w, h, pixels


def compare(baseline, candidate, exact):
    old_files = sorted(Path(baseline).glob("frame-*.ppm"))
    new_files = sorted(Path(candidate).glob("frame-*.ppm"))
    assert len(old_files) == len(new_files) == 19, "Missing render exercise frames"
    frames = []
    for old_path, new_path in zip(old_files, new_files):
        w, h, old = ppm(old_path)
        nw, nh, new = ppm(new_path)
        assert (w, h) == (nw, nh)
        half = w // 2
        # The background is specified by VoxelRenderer, even for inside views
        # where the upper-left voxel pixel can be occupied.
        background = bytes((9, 14, 23))
        masks = [[], []]
        changed = 0
        for y in range(h):
            start = y * w * 3
            assert old[start:start + half * 3] == new[start:start + half * 3], "Source changed"
            for x in range(half, w):
                i = start + x * 3
                a, b = old[i:i + 3], new[i:i + 3]
                changed += a != b
                masks[0].append(a != background)
                masks[1].append(b != background)
        occupied = sum(masks[0])
        lost = sum(a and not b for a, b in zip(*masks))
        gained = sum(b and not a for a, b in zip(*masks))
        interior_lost = 0
        for y in range(1, h - 1):
            for x in range(1, half - 1):
                i = y * half + x
                if masks[1][i] or not masks[0][i]:
                    continue
                interior_lost += all(masks[0][(y + dy) * half + x + dx]
                                     for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)))
        assert interior_lost == 0, f"Missing interior pixels: {new_path}: {interior_lost}"
        if exact:
            assert changed == 0, f"Eight-corner indexing changed pixels: {new_path}: {changed}"
        else:
            # Winding reversal can move raster/depth ties by a few edge pixels.
            assert changed <= max(8, occupied * .002), f"RGB changed: {new_path}: {changed}"
            assert lost + gained <= max(4, occupied * .0005), f"Coverage changed: {new_path}: {lost}/{gained}"
        frames.append({"frame": old_path.stem, "occupied": occupied, "changed_rgb": changed,
                       "lost": lost, "gained": gained, "interior_lost": interior_lost})
    for suffix in (".json",):
        a = json.loads(Path(str(baseline) + suffix).read_text())
        b = json.loads(Path(str(candidate) + suffix).read_text())
        for field in ("hits", "unique_voxels", "candidate_voxel_writes", "voxels_per_lod",
                      "render_freeze_checks", "verified_frames"):
            assert a[field] == b[field], f"Changed voxel data: {field}"
        assert b["validation_errors"] == 0 and b["render_freeze_checks"] == 18
    return {"candidate": str(candidate), "exact": exact, "frames": frames}


if __name__ == "__main__":
    result = compare(sys.argv[1], sys.argv[2], "--exact" in sys.argv[4:])
    Path(sys.argv[3]).write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"candidate": result["candidate"], "exact": result["exact"],
                      "frames": len(result["frames"]),
                      "changed_rgb": sum(f["changed_rgb"] for f in result["frames"]),
                      "interior_lost": sum(f["interior_lost"] for f in result["frames"])}))
