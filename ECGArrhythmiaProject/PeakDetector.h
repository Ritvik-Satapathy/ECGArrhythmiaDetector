#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <vector>

class PeakDetector {
public:
    static std::vector<double> normalize(const std::vector<double>& signal);
    static std::vector<double> smooth(const std::vector<double>& signal, int windowSize);
    static std::vector<int> findRPeaks(const std::vector<double>& signal, double samplingRate);
};

#endif
