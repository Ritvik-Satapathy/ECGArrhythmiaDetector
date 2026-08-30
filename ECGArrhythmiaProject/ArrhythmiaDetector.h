#ifndef ARRHYTHMIA_DETECTOR_H
#define ARRHYTHMIA_DETECTOR_H

#include "PatientContext.h"
#include <string>
#include <vector>

struct ArrhythmiaResult {
    bool valid = false;

    double heartRate = 0.0;
    double averageRR = 0.0;
    double irregularityRatio = 0.0;
    double extremeRRPercent = 0.0;

    double expectedHeartRateLow = 0.0;
    double expectedHeartRateHigh = 0.0;
    double expectedHeartRateCenter = 0.0;

    // Context-adjusted RR limits derived from the expected heart-rate model.
    double expectedRRLow = 0.0;
    double expectedRRHigh = 0.0;
    double allowedIrregularity = 0.0;

    // 0-1 internal concern values. These are exposed only so the program can
    // explain how the final heuristic score was formed.
    double heartRateConcern = 0.0;
    double irregularityConcern = 0.0;
    double extremeRRConcern = 0.0;

    // IMPORTANT: this is a heuristic engineering score, not a clinically
    // calibrated probability of disease.
    double arrhythmiaLikelihoodPercent = 0.0;
    std::string likelihoodCategory;
    std::string explanation;
};

class ArrhythmiaDetector {
public:
    static ArrhythmiaResult analyze(const std::vector<int>& rPeaks,
                                    double samplingRate,
                                    const PatientContext& context);
};

#endif
