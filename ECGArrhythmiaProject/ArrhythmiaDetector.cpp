#include "ArrhythmiaDetector.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace std;

static double clamp01(double value) {
    return max(0.0, min(1.0, value));
}

struct ExpectedHeartRateModel {
    double low = 60.0;
    double high = 100.0;
    double center = 80.0;
    double tolerance = 20.0;
};

// Build a context-dependent expected heart-rate model.
//
// IMPORTANT: These adjustments are intentionally simple engineering heuristics.
// They are NOT clinical reference equations. Their purpose is to make the
// prototype respond continuously to age, activity, sport type, and training
// frequency instead of only changing when a hard boundary is crossed.
static ExpectedHeartRateModel expectedHeartRateModel(const PatientContext& context) {
    ExpectedHeartRateModel model;

    // Start with a broad resting model.
    // For children/teens, shift the center upward gradually.
    if (context.age < 6) {
        model.center = 105.0;
        model.tolerance = 32.0;
    } else if (context.age < 13) {
        model.center = 92.0;
        model.tolerance = 28.0;
    } else if (context.age < 18) {
        model.center = 82.0;
        model.tolerance = 25.0;
    } else {
        // Adult baseline. We keep age influence deliberately small because this
        // is a heuristic prototype rather than a clinical age-risk model.
        model.center = 78.0;
        model.tolerance = 22.0;

        // Continuous age adjustment for adults: very small shift across age.
        // This makes age matter without letting age dominate the ECG evidence.
        double adultAgeShift = (static_cast<double>(context.age) - 30.0) * 0.03;
        adultAgeShift = max(-1.5, min(1.5, adultAgeShift));
        model.center += adultAgeShift;
    }

    // Continuous conditioning adjustment.
    // More training days gradually lower the expected resting center, with a
    // larger effect for endurance-heavy sports.
    if (context.playsSports) {
        double sportFactor = 0.0;

        switch (context.sportType) {
            case SportType::Endurance:
                sportFactor = 1.0;
                break;
            case SportType::RunningIntermittent:
                sportFactor = 0.75;
                break;
            case SportType::StrengthPower:
                sportFactor = 0.40;
                break;
            case SportType::Other:
                sportFactor = 0.30;
                break;
            default:
                sportFactor = 0.0;
                break;
        }

        double trainingFraction = clamp01(context.trainingDaysPerWeek / 7.0);
        double conditioningShift = 14.0 * sportFactor * trainingFraction;
        model.center -= conditioningShift;

        // Trained people are given a slightly wider low-rate tolerance.
        model.tolerance += 3.0 * sportFactor * trainingFraction;
    }

    // Continuous activity-state adjustment.
    switch (context.activityState) {
        case ActivityState::RelaxedRest:
            // Keep the resting center as calculated above.
            break;

        case ActivityState::AwakeRestNotRelaxed:
            model.center += 8.0;
            model.tolerance += 4.0;
            break;

        case ActivityState::ExercisedWithin15Minutes: {
            // Use age to create a simple post-exercise center. This is still a
            // heuristic; it is not intended to predict a clinical target zone.
            double estimatedMax = (context.age >= 18)
                ? 208.0 - 0.7 * context.age
                : 195.0;

            double postExerciseCenter = 0.65 * estimatedMax;
            model.center = max(model.center, postExerciseCenter);
            model.tolerance = max(model.tolerance, 35.0);
            break;
        }

        case ActivityState::Exercised15To60MinutesAgo:
            model.center += 15.0;
            model.tolerance += 10.0;
            break;

        case ActivityState::Unknown:
            // Unknown context should make the model less confident, so widen
            // the tolerance rather than making a strong contextual claim.
            model.tolerance += 12.0;
            break;
    }

    // Keep the model in a sensible numerical range.
    model.center = max(35.0, min(190.0, model.center));
    model.tolerance = max(12.0, min(50.0, model.tolerance));

    model.low = max(25.0, model.center - model.tolerance);
    model.high = min(220.0, model.center + model.tolerance);

    if (model.high <= model.low) {
        model.high = model.low + 20.0;
    }

    return model;
}

// Continuous heart-rate concern.
//
// The old version returned exactly 0 whenever heart rate was anywhere inside a
// context-adjusted range. That meant many different contexts produced the same
// final score. This version instead measures how far the observed heart rate is
// from the context-specific expected center, scaled by the context-specific
// tolerance. Therefore, changing age/activity/training can affect the score even
// when the heart rate is still inside the broad expected range.
static double continuousHeartRateConcern(double heartRate,
                                         const ExpectedHeartRateModel& model) {
    double distanceFromCenter = abs(heartRate - model.center);

    // 0 at the expected center, gradually rising as the measurement moves away.
    // A distance equal to the model tolerance gives concern around 0.35.
    // Very large deviations approach 1.0 smoothly.
    double normalizedDistance = distanceFromCenter / model.tolerance;

    double concern = 1.0 - exp(-0.43 * normalizedDistance * normalizedDistance);
    return clamp01(concern);
}

ArrhythmiaResult ArrhythmiaDetector::analyze(const vector<int>& rPeaks,
                                              double samplingRate,
                                              const PatientContext& context) {
    ArrhythmiaResult result;

    if (samplingRate <= 0) {
        result.explanation = "Invalid sampling rate.";
        return result;
    }

    if (rPeaks.size() < 3) {
        result.explanation = "Not enough R-peaks were detected to analyze rhythm.";
        return result;
    }

    vector<double> rrIntervals;
    rrIntervals.reserve(rPeaks.size() - 1);

    for (size_t i = 1; i < rPeaks.size(); i++) {
        double rr = (rPeaks[i] - rPeaks[i - 1]) / samplingRate;
        if (rr > 0.0) rrIntervals.push_back(rr);
    }

    if (rrIntervals.size() < 2) {
        result.explanation = "Not enough valid RR intervals were calculated.";
        return result;
    }

    double sumRR = 0.0;
    for (double rr : rrIntervals) sumRR += rr;
    double avgRR = sumRR / rrIntervals.size();

    if (avgRR <= 0.0) {
        result.explanation = "Invalid RR interval calculation.";
        return result;
    }

    double heartRate = 60.0 / avgRR;

    double variance = 0.0;
    for (double rr : rrIntervals) {
        variance += (rr - avgRR) * (rr - avgRR);
    }
    variance /= rrIntervals.size();

    double stdRR = sqrt(variance);
    double irregularityRatio = stdRR / avgRR;

    ExpectedHeartRateModel hrModel = expectedHeartRateModel(context);

    // Convert the context-adjusted expected heart-rate range into RR-interval
    // limits. Because RR = 60 / HR, a higher expected heart rate after exercise
    // automatically creates shorter expected RR intervals.
    double expectedRRLow = 60.0 / hrModel.high;
    double expectedRRHigh = 60.0 / hrModel.low;

    size_t extremeCount = 0;
    for (double rr : rrIntervals) {
        if (rr < expectedRRLow || rr > expectedRRHigh) {
            extremeCount++;
        }
    }

    double extremeFraction =
        static_cast<double>(extremeCount) / rrIntervals.size();

    double extremePercent = extremeFraction * 100.0;

    // Allow slightly more RR variability when the person has recently exercised
    // or is not fully relaxed. This is a simple context-aware engineering heuristic,
    // not a clinically validated threshold.
    double allowedIrregularity = 0.05;

    switch (context.activityState) {
        case ActivityState::RelaxedRest:
            allowedIrregularity = 0.05;
            break;

        case ActivityState::AwakeRestNotRelaxed:
            allowedIrregularity = 0.07;
            break;

        case ActivityState::ExercisedWithin15Minutes:
            allowedIrregularity = 0.10;
            break;

        case ActivityState::Exercised15To60MinutesAgo:
            allowedIrregularity = 0.08;
            break;

        case ActivityState::Unknown:
            allowedIrregularity = 0.08;
            break;
    }

    // ECG evidence remains dominant. Context now affects both heart-rate concern
    // and the RR thresholds used to interpret the waveform.
    double irregularityConcern =
        clamp01((irregularityRatio - allowedIrregularity) / 0.30);

    double extremeRRConcern =
        clamp01(extremeFraction / 0.10);

    double hrConcern =
        continuousHeartRateConcern(heartRate, hrModel);

    double combinedConcern =
        0.55 * irregularityConcern +
        0.25 * extremeRRConcern +
        0.20 * hrConcern;

    // Keep output away from artificial 0% and 100% certainty.
    // This remains a heuristic likelihood SCORE until it is calibrated against
    // labeled ECG ground truth.
    double likelihood = 5.0 + 90.0 * clamp01(combinedConcern);

    string category;
    if (likelihood < 25.0) category = "Low";
    else if (likelihood < 50.0) category = "Moderate";
    else if (likelihood < 75.0) category = "Elevated";
    else category = "High";

    stringstream reason;
    reason << fixed << setprecision(1);

    reason << "Context-adjusted expected heart-rate center is "
           << hrModel.center << " bpm (broad range "
           << hrModel.low << "-" << hrModel.high << " bpm). ";

    reason << "This gives a context-adjusted expected RR range of "
       << expectedRRLow << "-" << expectedRRHigh << " seconds. ";

    reason << "The measured heart rate contributes "
           << (hrConcern * 100.0)
           << "% of the maximum heart-rate concern for this context. ";

    if (irregularityRatio < 0.10) {
        reason << "RR intervals are fairly regular. ";
    } else if (irregularityRatio < 0.18) {
        reason << "RR intervals show some variability. ";
    } else {
        reason << "RR intervals show substantial variability. ";
    }

    if (extremeCount > 0) {
        reason << extremePercent
               << "% of RR intervals fell outside the context-adjusted RR range. ";
    } else {
        reason << "No RR intervals fell outside the context-adjusted RR range. ";
    }

    result.valid = true;
    result.heartRate = heartRate;
    result.averageRR = avgRR;
    result.irregularityRatio = irregularityRatio;
    result.extremeRRPercent = extremePercent;

    result.expectedHeartRateLow = hrModel.low;
    result.expectedHeartRateHigh = hrModel.high;
    result.expectedHeartRateCenter = hrModel.center;

    result.expectedRRLow = expectedRRLow;
    result.expectedRRHigh = expectedRRHigh;
    result.allowedIrregularity = allowedIrregularity;

    result.heartRateConcern = hrConcern;
    result.irregularityConcern = irregularityConcern;
    result.extremeRRConcern = extremeRRConcern;

    result.arrhythmiaLikelihoodPercent = likelihood;
    result.likelihoodCategory = category;
    result.explanation = reason.str();

    return result;
}
