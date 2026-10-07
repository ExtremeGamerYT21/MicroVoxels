"""Run matched optimization variants on the GPU used by the demo executable."""
import argparse
import json
import statistics
import subprocess
from pathlib import Path


VARIANTS = {
    "legacy": ["--legacy-compaction", "--unpacked", "--full-clear"],
    "append": ["--unpacked", "--full-clear"],
    "packed-full-clear": ["--full-clear"],
    "packed-touched-clear": ["--touched-clear"],
    "default": [],
    "depth-tiled": ["--depth-source"],
    "depth-untiled": ["--depth-source", "--no-depth-tiles"],
    "local-dedup": ["--local-dedup"],
}


def median_values(reports):
    result = {}
    for key, value in reports[0].items():
        if isinstance(value, dict):
            result[key] = median_values([report[key] for report in reports])
        elif isinstance(value, (int, float)) and not isinstance(value, bool):
            result[key] = statistics.median(report[key] for report in reports)
        else:
            result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("captures/pipeline-profile"))
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--ordinary-timestamps", action="store_true",
                        help="Allow normal pipeline overlap to measure production throughput")
    parser.add_argument("demo_options", nargs=argparse.REMAINDER,
                        help="Optional shared demo settings after --, excluding variant switches")
    args = parser.parse_args()
    if args.frames < 3 or args.repeats < 1:
        parser.error("At least three frames and one repeat are required")
    args.output.mkdir(parents=True, exist_ok=True)
    extra = args.demo_options
    if extra[:1] == ["--"]:
        extra = extra[1:]
    forbidden = {option for options in VARIANTS.values() for option in options}
    forbidden.update(("--verify", "--validation", "--screenshot", "--capture-sequence",
                      "--exercise", "--exercise-controls", "--exercise-stability"))
    if any(option in forbidden for option in extra):
        parser.error("Use shared scene settings only; omit variant switches, verification and image readbacks")
    common = [str(args.binary.resolve()), "--hidden", "--no-ui", "--time", "1",
              "--frames", str(args.frames), "--width", str(args.width), "--height", str(args.height)]
    if not args.ordinary_timestamps:
        common.append("--profile-stages")
    names = list(VARIANTS)
    reports = {name: [] for name in names}
    for repeat in range(args.repeats):
        # Rotate order to reduce warm-up/load bias across variants.
        for name in names[repeat % len(names):] + names[:repeat % len(names)]:
            path = args.output / f"{name}-{repeat}.json"
            print(f"{repeat + 1}/{args.repeats}: {name}", flush=True)
            subprocess.run(common + extra + VARIANTS[name] + ["--report", str(path)], check=True)
            report = json.loads(path.read_text())
            if report["dropped_hits"] or report["dropped_root_history_samples"]:
                raise RuntimeError(f"{name}: saturated tables invalidate this comparison")
            reports[name].append(report)
    summary = {}
    for name, runs in reports.items():
        summary[name] = {
            "median": median_values(runs),
            "generation_ms_range": [min(run["gpu_generation_ms"] for run in runs),
                                    max(run["gpu_generation_ms"] for run in runs)],
            "frame_ms_range": [min(run["gpu_frame_ms"] for run in runs),
                               max(run["gpu_frame_ms"] for run in runs)],
        }
        median = summary[name]["median"]
        print(f"{name:18s} generation {median['gpu_generation_ms']:.3f} ms; "
              f"frame {median['gpu_frame_ms']:.3f} ms; "
              f"{median['unique_voxels']} cells, {median['instance_bytes_per_voxel']} B/cell")
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
