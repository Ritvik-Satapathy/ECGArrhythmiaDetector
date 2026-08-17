#ifndef ECG_SIGNAL_H
#define ECG_SIGNAL_H

#include <string>
#include <vector>

struct ECGSignal {
    std::vector<double> samples;
    double samplingRate = 360.0;
    std::string sourceType;
};

#endif
