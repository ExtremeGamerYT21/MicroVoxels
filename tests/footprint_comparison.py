"""Measure rendered coverage and temporal silhouette error in matched raster views."""
import json
import sys
from pathlib import Path
from render_coverage import read_ppm


def masks(path):
    width, height, rgb = read_ppm(path)
    half = width // 2
    source_bg, voxel_bg = rgb[:3], rgb[half * 3:half * 3 + 3]
    source, voxels = bytearray(), bytearray()
    source_rgb, voxel_rgb = bytearray(), bytearray()
    for y in range(height):
        offset = y * width * 3
        row = rgb[offset:offset + half * 3]
        other = rgb[offset + half * 3:offset + width * 3]
        source_rgb.extend(row)
        voxel_rgb.extend(other)
        source.extend(row[i:i+3] != source_bg for i in range(0, len(row), 3))
        voxels.extend(other[i:i+3] != voxel_bg for i in range(0, len(other), 3))
    return half, height, source, voxels, source_rgb, voxel_rgb


def interior_holes(frame):
    w, h, source, voxels, _, _ = frame
    interior = holes = 0
    for y in range(2, h-2):
        for x in range(2, w-2):
            if all(source[(y+dy)*w+x+dx] for dy in range(-2,3) for dx in range(-2,3)):
                interior += 1
                holes += not voxels[y*w+x]
    return interior, holes


def sequence(folder):
    files = sorted(Path(folder).glob("frame-*.ppm"))
    if len(files) < 12:
        raise AssertionError("Expected three stationary frames and nine moving frames")
    frames = [masks(f) for f in files]
    stationary = frames[:3]
    if any(f[4] != stationary[0][4] or f[5] != stationary[0][5] for f in stationary[1:]):
        changes = [sum(a != b for a,b in zip(stationary[0][5], f[5]))
                   for f in stationary[1:]]
        raise AssertionError(f"Stationary RGB output changed: {changes} colour channels")
    energy = 0
    for previous, current in zip(frames[2:-1],frames[3:]):
        w,h,source,voxels,_,_ = current
        old_source, old_voxels = previous[2:4]
        for y in range(2,h-2):
            for x in range(2,w-2):
                i=y*w+x
                # Only a two-pixel band around the source/background silhouette.
                edge=any(source[(y+dy)*w+x+dx]!=source[i] or
                         old_source[(y+dy)*w+x+dx]!=old_source[i]
                         for dy in range(-2,3) for dx in range(-2,3))
                if edge:
                    error=int(voxels[i])-int(source[i])
                    old_error=int(old_voxels[i])-int(old_source[i])
                    energy+=abs(error-old_error)
    return frames,energy


def compare(point_dir,footprint_dir,point_report,footprint_report,before_image,after_image):
    points,point_energy=sequence(point_dir)
    footprints,footprint_energy=sequence(footprint_dir)
    if len(points)!=len(footprints) or any(a[4]!=b[4] for a,b in zip(points,footprints)):
        raise AssertionError("Point and footprint modes changed the source scene")
    if point_energy == 0 or footprint_energy >= point_energy:
        raise AssertionError("Footprints did not reduce camera-motion silhouette error")
    before,after=masks(before_image),masks(after_image)
    if before[4]!=after[4]:
        raise AssertionError("Coverage profiles used different source images")
    pixels,before_holes=interior_holes(before)
    _,after_holes=interior_holes(after)
    if pixels<10000 or after_holes>=before_holes:
        raise AssertionError("Footprints did not improve sparse-sampling coverage")
    a=json.loads(Path(point_report).read_text())
    b=json.loads(Path(footprint_report).read_text())
    before_changes=a["motion_patch_cell_additions"]+a["motion_patch_cell_removals"]
    after_changes=b["motion_patch_cell_additions"]+b["motion_patch_cell_removals"]
    return {
        "stationary_rgb_frames_identical": True,
        "matched_source_frames_identical": True,
        "source_interior_pixels": pixels,
        "point_void_pixels": before_holes,
        "footprint_void_pixels": after_holes,
        "point_silhouette_temporal_error": point_energy,
        "footprint_silhouette_temporal_error": footprint_energy,
        "silhouette_temporal_error_reduction_percent":
            100*(1-footprint_energy/point_energy) if point_energy else 0,
        "point_floor_patch_cell_changes":before_changes,
        "footprint_floor_patch_cell_changes":after_changes,
        "floor_patch_cell_change_reduction_percent":
            100*(1-after_changes/before_changes) if before_changes else 0,
    }


if __name__ == "__main__":
    result=compare(*sys.argv[1:7])
    print(json.dumps(result))
    if len(sys.argv)>7:
        Path(sys.argv[7]).write_text(json.dumps(result,indent=2)+"\n")
