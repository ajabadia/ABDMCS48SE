#!/usr/bin/env python3
"""Render the real JUCE instrument headlessly, then analyze its WAV capture."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

from analyze_capture import analyze_capture

REPO_ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_ROOT = REPO_ROOT / "docs" / "evidence" / "BDMCS48SE-DAW-01"
DEFAULT_BUILD_DIR = REPO_ROOT / "build-plugin"
DEFAULT_EXECUTABLE = (
    DEFAULT_BUILD_DIR
    / "bdmcs48se_plugin_smoke_artefacts"
    / "Release"
    / "bdmcs48se_plugin_smoke.exe"
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin-smoke", type=Path, help="Existing headless JUCE processor executable")
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD_DIR, help="Existing JUCE-enabled CMake build directory")
    parser.add_argument("--build-if-needed", action="store_true", help="Build bdmcs48se_plugin_smoke in Release before rendering")
    parser.add_argument("--output-wav", type=Path, default=EVIDENCE_ROOT / "headless" / "headless-a4.wav")
    parser.add_argument("--output-json", type=Path, default=EVIDENCE_ROOT / "headless" / "headless-a4.json")
    parser.add_argument("--context", type=Path, help="Optional context JSON; omit it for accurate headless-render metadata")
    parser.add_argument("--sample-rate", type=int, default=48_000)
    parser.add_argument("--duration", type=float, default=6.0)
    parser.add_argument("--note", type=int, default=69, help="MIDI note; 69 is A4")
    parser.add_argument("--note-on", type=float, default=0.5)
    parser.add_argument("--note-off", type=float, default=5.5)
    parser.add_argument("--velocity", type=float, default=0.9)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.sample_rate < 8_000 or args.duration <= 0 or not 0 <= args.note <= 127:
        parser.error("sample rate, duration or MIDI note is outside its valid range")
    if args.note_on < 0 or args.note_off <= args.note_on or args.note_off >= args.duration:
        parser.error("require 0 <= note-on < note-off < duration")
    if not 0 < args.velocity <= 1:
        parser.error("velocity must be greater than 0 and no greater than 1")

    executable = args.plugin_smoke or DEFAULT_EXECUTABLE
    if args.build_if_needed:
        executable = args.build_dir / "bdmcs48se_plugin_smoke_artefacts" / "Release" / "bdmcs48se_plugin_smoke.exe"
        command = [
            "cmake", "--build", str(args.build_dir), "--config", "Release",
            "--target", "bdmcs48se_plugin_smoke",
        ]
        built = subprocess.run(command, cwd=REPO_ROOT, check=False)
        if built.returncode:
            print(f"[ERROR] JUCE headless runner build failed ({built.returncode})", file=sys.stderr)
            return built.returncode

    if not executable.is_file():
        print(f"[ERROR] JUCE headless runner not found: {executable}", file=sys.stderr)
        print("Pass --plugin-smoke <path> or build it with --build-if-needed --build-dir <cmake-dir>.", file=sys.stderr)
        return 1

    args.output_wav.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    render = subprocess.run(
        [
            str(executable), "--render-capture", "--output", str(args.output_wav),
            "--sample-rate", str(args.sample_rate), "--duration", str(args.duration),
            "--note", str(args.note), "--note-on", str(args.note_on),
            "--note-off", str(args.note_off), "--velocity", str(args.velocity),
        ],
        cwd=REPO_ROOT,
        check=False,
    )
    if render.returncode:
        print(f"[ERROR] JUCE capture render failed ({render.returncode}); WAV analysis was not started.", file=sys.stderr)
        return render.returncode

    try:
        context = json.loads(args.context.read_text(encoding="utf-8")) if args.context else {
            "host": {
                "daw": "JUCE headless render (no DAW)",
                "pluginFormat": "JUCE AudioProcessor (headless)",
            }
        }
        report = analyze_capture(args.output_wav, context)
        args.output_json.write_text(
            json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    except (OSError, RuntimeError, ValueError) as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1

    print(f"[OK] WAV capturado: {args.output_wav}")
    print(f"[OK] informe escrito: {args.output_json}")
    print(f"[AUDIO] {report['audio']['sampleRate']} Hz, {report['audio']['channels']} ch, "
          f"{report['audio']['durationSeconds']:.3f} s, "
          f"peak {report['audio']['peakDbfs'] if report['audio']['peakDbfs'] is not None else '-inf'} dBFS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
