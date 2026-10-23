from __future__ import annotations

import json
import math
import tempfile
import unittest
from pathlib import Path

import numpy as np
import soundfile as sf

from analyze_capture import analyze_capture, main


class AnalyzeCaptureTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.sample_rate = 48_000

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def write_wav(self, name: str, samples: np.ndarray, sample_rate: int | None = None) -> Path:
        path = self.root / name
        sf.write(path, samples, sample_rate or self.sample_rate, subtype="FLOAT")
        return path

    def test_silence_is_finite_constant_and_has_no_frequency(self) -> None:
        report = analyze_capture(self.write_wav("silence.wav", np.zeros(4800)))
        audio = report["audio"]
        self.assertTrue(audio["finite"])
        self.assertFalse(audio["nonConstant"])
        self.assertEqual(audio["nonSilentFrames"], 0)
        self.assertIsNone(audio["dominantFrequencyHz"])
        self.assertEqual(audio["frames"], 4800)

    def test_constant_wave_is_not_marked_nonconstant(self) -> None:
        report = analyze_capture(self.write_wav("constant.wav", np.full(2048, 0.25)))
        self.assertFalse(report["audio"]["nonConstant"])
        self.assertAlmostEqual(report["audio"]["dcOffsetPerChannel"][0], 0.25, places=6)

    def test_a4_sine_reports_frequency_and_active_region(self) -> None:
        frames = self.sample_rate * 2
        t = np.arange(frames, dtype=np.float64) / self.sample_rate
        signal = np.zeros(frames, dtype=np.float64)
        start = self.sample_rate // 2
        end = self.sample_rate + start
        signal[start:end] = 0.5 * np.sin(2.0 * math.pi * 440.0 * t[start:end])
        report = analyze_capture(self.write_wav("a4.wav", signal))
        audio = report["audio"]
        self.assertTrue(audio["finite"])
        self.assertTrue(audio["nonConstant"])
        self.assertIsNotNone(audio["activeSegment"])
        self.assertAlmostEqual(audio["dominantFrequencyHz"], 440.0, delta=1.0)
        self.assertEqual(len(report["source"]["sha256"]), 64)

    def test_truncated_wav_is_rejected(self) -> None:
        path = self.write_wav("truncated.wav", np.sin(np.arange(4096) * 0.1))
        path.write_bytes(path.read_bytes()[:48])
        with self.assertRaises((OSError, RuntimeError, ValueError)):
            analyze_capture(path)

    def test_wav_with_no_finite_samples_is_reported_as_nonfinite(self) -> None:
        samples = np.full(1024, np.nan, dtype=np.float32)
        audio = analyze_capture(self.write_wav("nonfinite.wav", samples))["audio"]
        self.assertFalse(audio["finite"])
        self.assertEqual(audio["peak"], 0.0)
        self.assertEqual(audio["rms"], 0.0)
        self.assertIsNone(audio["dominantFrequencyHz"])

    def test_short_note_is_found_using_windowed_rms(self) -> None:
        samples = np.zeros(48_000, dtype=np.float64)
        t = np.arange(480, dtype=np.float64) / self.sample_rate
        samples[10_000:10_480] = 0.5 * np.sin(2 * math.pi * 440 * t)
        audio = analyze_capture(self.write_wav("short-note.wav", samples))["audio"]
        self.assertIsNotNone(audio["activeSegment"])
        self.assertLessEqual(audio["activeSegment"]["startFrame"], 10_000)
        self.assertGreaterEqual(audio["activeSegment"]["endFrameExclusive"], 10_480)
        self.assertAlmostEqual(audio["dominantFrequencyHz"], 440.0, delta=5.0)

    def test_opposed_stereo_channels_do_not_cancel_frequency_analysis(self) -> None:
        t = np.arange(self.sample_rate, dtype=np.float64) / self.sample_rate
        left = 0.4 * np.sin(2 * math.pi * 440 * t)
        right = -left
        audio = analyze_capture(self.write_wav("opposed-stereo.wav", np.column_stack((left, right))))["audio"]
        self.assertAlmostEqual(audio["dominantFrequencyHz"], 440.0, delta=1.0)

    def test_stereo_channels_are_reported_and_dc_is_per_channel(self) -> None:
        t = np.arange(self.sample_rate, dtype=np.float64) / self.sample_rate
        stereo = np.column_stack((0.4 * np.sin(2 * math.pi * 440 * t) + 0.1,
                                  0.2 * np.sin(2 * math.pi * 440 * t) - 0.05))
        report = analyze_capture(self.write_wav("stereo.wav", stereo))
        audio = report["audio"]
        self.assertEqual(audio["channels"], 2)
        self.assertEqual(len(audio["dcOffsetPerChannel"]), 2)
        self.assertAlmostEqual(audio["dcOffsetPerChannel"][0], 0.1, places=5)
        self.assertAlmostEqual(audio["dcOffsetPerChannel"][1], -0.05, places=5)

    def test_dc_offset_is_reported_without_being_removed_from_rms(self) -> None:
        samples = 0.2 + 0.05 * np.sin(2 * math.pi * 220 * np.arange(4800) / self.sample_rate)
        audio = analyze_capture(self.write_wav("dc.wav", samples))["audio"]
        self.assertAlmostEqual(audio["dcOffsetPerChannel"][0], 0.2, places=5)
        self.assertGreater(audio["rms"], abs(audio["dcOffsetPerChannel"][0]))

    def test_cli_writes_json_report(self) -> None:
        wav_path = self.write_wav("input.wav", np.sin(2 * math.pi * 440 * np.arange(4800) / self.sample_rate))
        output = self.root / "reports" / "capture.json"
        self.assertEqual(main(["--input", str(wav_path), "--output", str(output)]), 0)
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(report["schemaVersion"], 1)
        self.assertEqual(report["audio"]["sampleRate"], self.sample_rate)

    def test_cli_reports_unreadable_input_with_nonzero_status(self) -> None:
        output = self.root / "bad.json"
        self.assertEqual(main(["--input", str(self.root / "missing.wav"), "--output", str(output)]), 1)
        self.assertFalse(output.exists())

    def test_context_records_observed_settings_without_invented_thresholds(self) -> None:
        path = self.write_wav("context.wav", np.sin(np.arange(4800) * 0.1))
        context = {
            "host": {
                "dawVersion": "7.x",
                "osVersion": "Windows build under test",
                "sampleRate": 48_000,
                "bufferSize": 256,
                "instances": 2,
            },
            "performance": {"cpuPercent": 8.5, "dropouts": 0},
        }
        report = analyze_capture(path, context)
        self.assertEqual(report["host"]["daw"], "REAPER")
        self.assertEqual(report["host"]["sampleRate"], 48_000)
        self.assertEqual(report["performance"]["dropouts"], 0)
        self.assertEqual(report["performance"]["threshold"], "not-defined for the DAW-01 baseline")

    def test_invalid_context_is_rejected(self) -> None:
        path = self.write_wav("context-invalid.wav", np.zeros(256))
        with self.assertRaises(ValueError):
            analyze_capture(path, {"performance": {"threshold": 1}})

    def test_empty_context_object_keeps_default_host_settings(self) -> None:
        path = self.write_wav("empty-context.wav", np.zeros(256))
        report = analyze_capture(path, {})
        self.assertEqual(report["host"]["daw"], "REAPER")
        self.assertIsNone(report["host"]["sampleRate"])


if __name__ == "__main__":
    unittest.main()
