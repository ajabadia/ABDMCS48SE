# Findings & Decisions

## Requirements
- Analyze WAV captures rendered from REAPER for BDMCS48SE-DAW-01; output deterministic JSON alongside the primary WAV evidence.
- Record duration, sample rate, channel count, frame count, RMS, peak, clipping observations, DC offset, non-constant/non-silent counts, active segment and dominant frequency, plus SHA-256.
- Tests cover silence, constant signal, A4 sine, truncated/unreadable file, stereo and DC offset.
- Do not set CPU/dropout thresholds in this analyzer; those are manually reported as observed host metrics.

## Research Findings
- Local runtime: Python 3.14.7; installed/pinned NumPy 2.3.3 and SoundFile 0.13.1 for verification.
- Repo has no existing Python test or packaging conventions; implement standard-library `unittest` with temporary directories and runtime CLI imports.
- `.gitignore` excludes generic `*.bin` and `*.zip`, but not `.py`, requirements text, docs, or tests.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Python CLI + NumPy + SoundFile | Matches requested tools; use fixed pins compatible with current Python 3.14 and document tested runtime |
| JSON report includes `schemaVersion`, `audio`, and `source` | Stable machine-readable basics with source hash; omit manually supplied DAW/performance values because CLI cannot reliably infer them |
| Thresholds only for descriptive classification (`silence`, finite, nonconstant), no acceptance pass/fail or performance threshold | Avoid turning observed measurements into invented product requirements |
| Dominant frequency estimated from non-silent window via Hann + FFT, reported with scope | Provides approximate tone evidence, not a certification of tuning |
| Truncated/unsupported WAV errors surfaced to stderr with nonzero exit | Do not emit a misleading report for invalid evidence |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| `soundfile` absent in local Python | Pinned requirements installed successfully; 8/8 unittest cases passed |

## Resources
- `ROADMAP.md` BDMCS48SE-DAW-01 contract
- `docs/DECISIONS_AND_QUESTIONS.md` DAW evidence choices
