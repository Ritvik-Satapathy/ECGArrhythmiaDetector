#ifndef ECG_READER_H
#define ECG_READER_H

#include "ECGSignal.h"
#include <string>

class ECGReader {
public:
    // Reads .csv, .txt, or MIT-BIH .dat with matching .hea file.
    // Returns true if successful. If false, errorMessage explains why.
    static bool readFile(const std::string& filename,
                         ECGSignal& signal,
                         std::string& errorMessage);
};

#endif
