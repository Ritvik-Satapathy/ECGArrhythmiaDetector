ECG Arrhythmia Project - Context-Aware Continuous Version
==========================================================

What changed in this version
----------------------------
The previous context-aware version used hard heart-rate ranges. If the measured
heart rate was anywhere inside a person's context-adjusted range, the heart-rate
concern became exactly zero. As a result, changing age, activity, sport type, or
training frequency often produced the same final likelihood score.

This version uses a CONTINUOUS context model.

The program now:
1. Builds a context-specific expected heart-rate CENTER and tolerance.
2. Adjusts that center gradually based on age, activity state, sport type, and
   training days per week.
3. Measures how far the actual ECG-derived heart rate is from that center.
4. Converts that distance smoothly into a 0-1 heart-rate concern value.
5. Combines that with RR irregularity and extreme RR intervals.

The ECG evidence still dominates the score:
- 55% RR irregularity
- 25% extreme RR intervals
- 20% context-aware heart-rate concern

This means context matters continuously, but it cannot completely override the
waveform evidence.

Important scientific limitation
--------------------------------
The displayed percentage is a HEURISTIC ENGINEERING SCORE. It is not yet a
clinically calibrated probability of arrhythmia. A true probability requires
validation/calibration against labeled ECG ground truth (for example MIT-BIH
annotations). The current context adjustments are intentionally simple engineering
heuristics for prototype development.

Files changed
-------------
- ArrhythmiaDetector.cpp
- ArrhythmiaDetector.h
- main.cpp

The ECG reader and current R-peak detector were intentionally left unchanged.
