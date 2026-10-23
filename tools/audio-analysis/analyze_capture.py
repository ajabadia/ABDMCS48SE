#!/usr/bin/env python3
"""Analyze a rendered WAV capture and write a reproducible JSON report."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import sys
from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf

SCHEMA_VERSION = 1
SILENCE_THRESHOLD_DBFS = -90.0
ACTIVE_THRESHOLD_DBFS = -60.0


def _dbfs(value: float) -> float | None:
    if value <= 0.0:
        return None
    return 20.0 * math.log10(value)


def _dominant_frequency(samples: np.ndarray, sample_rate: int) -> float | None:
    """Estimate the strongest non-DC spectral component across active channels."""
    if samples.shape[0] < 2:
        return None
    centered = samples - np.mean(samples, axis=0, dtype=np.float64)
    window = np.hanning(samples.shape[0])[:, np.newaxis]
    channel_spectra = np.abs(np.fft.rfft(centered * window, axis=0))
    spectrum = np.sum(np.square(channel_spectra), axis=1, dtype=np.float64)
    if spectrum.size <= 1:
        return None
    spectrum[0] = 0.0
    peak_bin = int(np.argmax(spectrum))
    if peak_bin == 0 or spectrum[peak_bin] <= np.finfo(np.float64).eps:
        return None

    # Parabolic interpolation around the maximum improves resolution between
    # FFT bins without implying accuracy beyond the captured window's limits.
    if 0 < peak_bin < spectrum.size - 1:
        left, center, right = np.log(np.maximum(spectrum[peak_bin - 1 : peak_bin + 2], 1e-300))
        denominator = left - 2.0 * center + right
        offset = 0.5 * (left - right) / denominator if denominator != 0.0 else 0.0
    else:
        offset = 0.0
    return float((peak_bin + offset) * sample_rate / samples.shape[0])


def _active_frames(samples: np.ndarray, threshold: float, sample_rate: int) -> np.ndarray:
    """Mark frames whose centered 10 ms window RMS exceeds a descriptive threshold."""
    frame_magnitudes = np.max(np.abs(samples), axis=1)
    window_frames = max(1, int(round(sample_rate * 0.010)))
    left_frames = window_frames // 2
    right_frames = window_frames - left_frames
    indices = np.arange(frame_magnitudes.size)
    starts = np.maximum(indices - left_frames, 0)
    ends = np.minimum(indices + right_frames, frame_magnitudes.size)
    cumulative_energy = np.concatenate(([0.0], np.cumsum(np.square(frame_magnitudes), dtype=np.float64)))
    window_counts = ends - starts
    window_energy = cumulative_energy[ends] - cumulative_energy[starts]
    window_rms = np.sqrt(window_energy / window_counts)
    return window_rms > threshold


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _context_sections(context: dict[str, Any] | None) -> tuple[dict[str, Any], dict[str, Any]]:
    allowed_sections = {"host", "performance"}
    supplied = {} if context is None else context
    if not isinstance(supplied, dict) or set(supplied) - allowed_sections:
        raise ValueError("context JSON may contain only 'host' and 'performance' objects")

    host = {
        "daw": "REAPER",
        "dawVersion": None,
        "os": platform.system(),
        "osVersion": None,
        "pluginFormat": "VST3",
        "pluginVersion": None,
        "sampleRate": None,
        "bufferSize": None,
        "instances": None,
    }
    performance = {
        "cpuPercent": None,
        "dropouts": None,
        "threshold": "not-defined for the DAW-01 baseline",
    }

    supplied_host = supplied.get("host", {})
    supplied_performance = supplied.get("performance", {})
    if not isinstance(supplied_host, dict) or not isinstance(supplied_performance, dict):
        raise ValueError("context 'host' and 'performance' values must be JSON objects")
    if set(supplied_host) - set(host):
        raise ValueError("context host contains an unknown field")
    if set(supplied_performance) - {"cpuPercent", "dropouts"}:
        raise ValueError("context performance may contain only cpuPercent and dropouts")

    for field in ("daw", "dawVersion", "os", "osVersion", "pluginFormat", "pluginVersion"):
        value = supplied_host.get(field)
        if value is not None and not isinstance(value, str):
            raise ValueError(f"context host.{field} must be a string or null")
    for field in ("sampleRate", "bufferSize", "instances"):
        value = supplied_host.get(field)
        if value is not None and (not isinstance(value, int) or isinstance(value, bool) or value <= 0):
            raise ValueError(f"context host.{field} must be a positive integer or null")
    cpu_percent = supplied_performance.get("cpuPercent")
    if cpu_percent is not None and (
        not isinstance(cpu_percent, (int, float))
        or isinstance(cpu_percent, bool)
        or not math.isfinite(cpu_percent)
        or cpu_percent < 0
    ):
        raise ValueError("context performance.cpuPercent must be a finite non-negative number or null")
    dropouts = supplied_performance.get("dropouts")
    if dropouts is not None and (
        not isinstance(dropouts, int) or isinstance(dropouts, bool) or dropouts < 0
    ):
        raise ValueError("context performance.dropouts must be a non-negative integer or null")

    for field in ("daw", "dawVersion", "os", "osVersion", "pluginFormat", "pluginVersion"):
        if field in supplied_host:
            host[field] = supplied_host[field]
    for field in ("sampleRate", "bufferSize", "instances"):
        if field in supplied_host:
            host[field] = supplied_host[field]
    for field in ("cpuPercent", "dropouts"):
        if field in supplied_performance:
            performance[field] = supplied_performance[field]
    return host, performance


def analyze_capture(input_path: Path, context: dict[str, Any] | None = None) -> dict[str, Any]:
    """Analyze a readable WAV file; raises OSError/ValueError on invalid input."""
    info = sf.info(str(input_path))
    if info.frames <= 0 or info.samplerate <= 0 or info.channels <= 0:
        raise ValueError("WAV must contain at least one frame and valid audio metadata")

    samples, sample_rate = sf.read(str(input_path), dtype="float64", always_2d=True)
    if samples.shape[0] != info.frames:
        raise ValueError("WAV frame count changed or file is truncated while reading")
    if samples.size == 0:
        raise ValueError("WAV contains no samples")

    finite_mask = np.isfinite(samples)
    finite = bool(np.all(finite_mask))
    finite_samples = samples[finite_mask]

    # Metrics use the audio after replacing non-finite values only for safe
    # arithmetic; the report still marks the source as non-finite.
    safe_samples = np.where(finite_mask, samples, 0.0)
    peak = float(np.max(np.abs(finite_samples))) if finite_samples.size else 0.0
    rms = float(np.sqrt(np.mean(np.square(safe_samples), dtype=np.float64)))
    dc_offset = np.mean(safe_samples, axis=0, dtype=np.float64)
    silence_linear = 10.0 ** (SILENCE_THRESHOLD_DBFS / 20.0)
    active_linear = 10.0 ** (ACTIVE_THRESHOLD_DBFS / 20.0)
    frame_magnitudes = np.max(np.abs(safe_samples), axis=1)
    active_frames = _active_frames(safe_samples, active_linear, sample_rate)
    active_indices = np.flatnonzero(active_frames)

    # Frequency estimation uses the first contiguous active segment. This
    # avoids combining leading/trailing silence with unrelated later events.
    active_segment: dict[str, Any] | None = None
    dominant_frequency: float | None = None
    if active_indices.size:
        start = int(active_indices[0])
        end = start + 1
        while end < info.frames and active_frames[end]:
            end += 1
        segment = safe_samples[start:end]
        active_segment = {
            "startFrame": start,
            "endFrameExclusive": end,
            "durationSeconds": (end - start) / sample_rate,
            "frames": end - start,
        }
        dominant_frequency = _dominant_frequency(segment, sample_rate)
        active_segment["frequencyEstimateValid"] = dominant_frequency is not None

    non_constant = bool(np.any(safe_samples != safe_samples[0]))
    clipped_samples = int(np.count_nonzero(np.abs(finite_samples) >= 1.0))
    digest = _sha256_file(input_path)
    host, performance = _context_sections(context)

    return {
        "schemaVersion": SCHEMA_VERSION,
        "source": {
            "fileName": input_path.name,
            "sha256": digest,
            "subtype": info.subtype,
            "format": info.format,
        },
        "host": host,
        "performance": performance,
        "audio": {
            "finite": finite,
            "nonConstant": non_constant,
            "durationSeconds": info.frames / sample_rate,
            "sampleRate": int(sample_rate),
            "channels": int(info.channels),
            "frames": int(info.frames),
            "rms": rms,
            "rmsDbfs": _dbfs(rms),
            "peak": peak,
            "peakDbfs": _dbfs(peak),
            "truePeak": None,
            "truePeakDbfs": None,
            "truePeakNote": "Not estimated; peak is the maximum absolute decoded sample.",
            "clippedSamples": clipped_samples,
            "clippingObserved": clipped_samples > 0,
            "dcOffsetPerChannel": [float(value) for value in dc_offset],
            "nonSilentFrames": int(np.count_nonzero(frame_magnitudes > silence_linear)),
            "nonSilentSamples": int(np.count_nonzero(np.abs(safe_samples) > silence_linear)),
            "activeSegment": active_segment,
            "activeThresholdDbfs": ACTIVE_THRESHOLD_DBFS,
            "activeWindowMs": 10,
            "dominantFrequencyHz": dominant_frequency,
            "frequencyEstimateNote": (
                "Estimate from the first active segment above -60 dBFS using a 10 ms centered RMS window; "
                "FFT resolution depends on segment duration."
                if dominant_frequency is not None
                else "No estimable active segment above -60 dBFS; frequency not estimated."
            ),
        },
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Input WAV capture")
    parser.add_argument("--output", required=True, type=Path, help="Output JSON report")
    parser.add_argument(
        "--context",
        type=Path,
        help="Optional JSON file with observed host settings, CPU and dropout readings",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        context = json.loads(args.context.read_text(encoding="utf-8")) if args.context else None
        report = analyze_capture(args.input, context)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    except (OSError, RuntimeError, ValueError) as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1
    print(f"[OK] informe escrito en {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
