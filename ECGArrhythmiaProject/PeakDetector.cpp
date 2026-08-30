#include "PeakDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

using namespace std;

vector<double> PeakDetector::normalize(const vector<double>& signal) {
    vector<double> result = signal;
    if (result.empty()) return result;

    double mean = accumulate(result.begin(), result.end(), 0.0) / result.size();

    double var = 0.0;
    for (double x : result) var += (x - mean) * (x - mean);
    var /= result.size();

    double stdDev = sqrt(var);
    if (stdDev == 0.0) return result;

    for (double& x : result) x = (x - mean) / stdDev;
    return result;
}

vector<double> PeakDetector::smooth(const vector<double>& signal, int windowSize) {
    if (windowSize <= 1 || signal.empty()) return signal;

    vector<double> smoothed(signal.size());
    int half = windowSize / 2;

    for (size_t i = 0; i < signal.size(); i++) {
        double sum = 0.0;
        int count = 0;

        int start = max(0, static_cast<int>(i) - half);
        int end = min(static_cast<int>(signal.size()) - 1, static_cast<int>(i) + half);

        for (int j = start; j <= end; j++) {
            sum += signal[j];
            count++;
        }

        smoothed[i] = sum / count;
    }

    return smoothed;
}

static double percentile(vector<double> values, double p) {
    if (values.empty()) return 0.0;
    sort(values.begin(), values.end());

    int index = static_cast<int>((p / 100.0) * (values.size() - 1));
    index = max(0, min(index, static_cast<int>(values.size()) - 1));
    return values[index];
}

static vector<int> findPositivePeaks(const vector<double>& ecg, double samplingRate) {
    vector<int> peaks;

    // Adaptive threshold: use the upper part of the signal instead of one fixed number.
    double threshold = percentile(ecg, 90.0);
    if (threshold < 0.6) threshold = 0.6;

    int minDistance = static_cast<int>(0.25 * samplingRate); // 250 ms refractory period
    if (minDistance < 1) minDistance = 1;

    for (int i = 1; i < static_cast<int>(ecg.size()) - 1; i++) {
        bool isLocalPeak = ecg[i] > ecg[i - 1] && ecg[i] >= ecg[i + 1];
        bool highEnough = ecg[i] > threshold;

        if (isLocalPeak && highEnough) {
            if (peaks.empty() || i - peaks.back() > minDistance) {
                peaks.push_back(i);
            } else {
                // If two peaks are too close, keep the taller one.
                if (ecg[i] > ecg[peaks.back()]) {
                    peaks.back() = i;
                }
            }
        }
    }

    return peaks;
}

vector<int> PeakDetector::findRPeaks(const vector<double>& signal, double samplingRate) {
    vector<double> normalized = normalize(signal);
    vector<double> smoothed = smooth(normalized, 5);

    vector<int> peaks = findPositivePeaks(smoothed, samplingRate);

    // Some ECG leads can be inverted. If we barely found peaks, try flipping the signal.
    if (peaks.size() < 3) {
        vector<double> inverted = smoothed;
        for (double& x : inverted) x = -x;

        vector<int> invertedPeaks = findPositivePeaks(inverted, samplingRate);
        if (invertedPeaks.size() > peaks.size()) peaks = invertedPeaks;
    }

    return peaks;
}
