"""Check real split-view pixels, not just generated voxel counts."""
import json
import sys
from pathlib import Path


def read_ppm(path):
    magic, dimensions, maximum, pixels = Path(path).read_bytes().split(b"\n", 3)
    width, height = map(int, dimensions.split())
    if magic != b"P6" or maximum != b"255" or width % 2 or len(pixels) != width * height * 3:
        raise ValueError("Expected an even-width RGB screenshot from Microvoxels")
    return width, height, pixels


def compare(before_path, after_path):
    width, height, before = read_ppm(before_path)
    after_width, after_height, after = read_ppm(after_path)
    if (width, height) != (after_width, after_height):
        raise ValueError("Comparison screenshots have different dimensions")
    half = width // 2
    source_backgrounds = (before[:3], after[:3])
    voxel_backgrounds = (before[half * 3:half * 3 + 3], after[half * 3:half * 3 + 3])
    source = []
    for y in range(height):
        for x in range(half):
            offset = (y * width + x) * 3
            source.append(all(image[offset:offset + 3] != background
                              for image, background in zip((before, after), source_backgrounds)))
    covered = 0
    holes = [0, 0]
    for y in range(2, height - 2):
        for x in range(2, half - 2):
            # Exclude silhouettes, where cubic cells legitimately change the outline.
            if not all(source[(y + dy) * half + x + dx]
                       for dy in range(-2, 3) for dx in range(-2, 3)):
                continue
            covered += 1
            offset = (y * width + half + x) * 3
            for i, image in enumerate((before, after)):
                holes[i] += image[offset:offset + 3] == voxel_backgrounds[i]
    if covered < 10000:
        raise AssertionError("Source view is missing or unexpectedly small")
    if holes[1] > max(10, holes[0] // 5):
        raise AssertionError(f"Adaptive LOD did not close sparse-sampling gaps: {holes}")
    return {"width": width, "height": height, "source_interior_pixels": covered,
            "distance_lod_void_pixels": holes[0], "adaptive_lod_void_pixels": holes[1]}


if __name__ == "__main__":
    result = compare(sys.argv[1], sys.argv[2])
    print(json.dumps(result))
    if len(sys.argv) > 3:
        Path(sys.argv[3]).write_text(json.dumps(result, indent=2) + "\n")
