# Task Plan: DAW Capture Analyzer

## Goal
Deliver `tools/audio-analysis/analyze_capture.py`, pinned analysis dependencies, synthetic-WAV tests and REAPER capture instructions for BDMCS48SE-DAW-01.

## Next Step
No further task step; all analyzer implementation and final checks are complete.

## Current Phase
Phase 3

## Phases

### Phase 1: Requirements & Discovery
- [x] Recover project state and inspect repository conventions
- [x] Confirm REAPER on Windows, observed negotiated audio settings, recorded CPU/dropouts without thresholds, and WAV plus script evidence
- [x] Note the user's proposed report fields and synthetic test cases
- **Status:** complete

### Phase 2: Contract & Implementation
- [x] Choose a small JSON report shape and metric semantics
- [x] Pin `numpy` and `soundfile` versions appropriate to Python 3.14.7
- [x] Implement CLI analyzer and synthetic WAV tests
- [x] Document REAPER capture workflow and metric definitions
- **Status:** complete

### Phase 3: Verification & Review
- [x] Run analyzer test suite, including malformed/truncated WAV path (14/14 pass after final changes)
- [x] Run CLI against mono and stereo synthetic captures with context JSON; inspected sample rate, channels, frequency, hash and threshold fields (stereo run: 48 kHz, ~440 Hz)
- [x] Review analyzer files, documentation references and `git diff --check`
- **Status:** complete

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Python CLI using `soundfile` and `numpy` | User requested these tools; both installed and tested on local Python 3.14.7 |
| WAV is primary evidence; JSON is derived report | User requested reproducible reports accompanying captures |
| No DAW CPU/dropout thresholds | User explicitly chose to record observed readings only in the first pass |
| Report exposes sample peak, not an unimplemented true-peak estimate | Avoid labeling sample peak as true peak; truePeak fields are null with an explanation |
| Frequency is an estimate on the first active segment | It is informative evidence but does not promise a fundamental or a DAW acceptance threshold |

## Errors Encountered
| Error | Resolution |
|-------|------------|
| `soundfile` was not installed locally | Installed pinned requirements; all analyzer tests passed |
