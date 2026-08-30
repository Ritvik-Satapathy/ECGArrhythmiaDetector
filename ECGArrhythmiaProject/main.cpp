#include "ArrhythmiaDetector.h"
#include "ECGReader.h"
#include "ECGSignal.h"
#include "PatientContext.h"
#include "PeakDetector.h"

#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;

static int readIntInRange(const string& prompt, int minimum, int maximum) {
    while (true) {
        cout << prompt;
        int value;
        cin >> value;

        if (!cin.fail() && value >= minimum && value <= maximum) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }

        cout << "Invalid input. Please enter a number from "
             << minimum << " to " << maximum << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

static bool readYesNo(const string& prompt) {
    while (true) {
        cout << prompt << " (y/n): ";
        string answer;
        getline(cin, answer);

        if (answer == "y" || answer == "Y" || answer == "yes" || answer == "Yes") {
            return true;
        }
        if (answer == "n" || answer == "N" || answer == "no" || answer == "No") {
            return false;
        }

        cout << "Invalid input. Please enter y or n.\n";
    }
}

static PatientContext collectPatientContext() {
    PatientContext context;

    cout << "\n==========================================\n";
    cout << "              User Context\n";
    cout << "==========================================\n";
    cout << "These questions help the program interpret heart rate in context.\n\n";

    context.age = readIntInRange("Age: ", 1, 120);

    cout << "\nWhat best describes the person when this ECG was recorded?\n";
    cout << "  1. Relaxed/resting\n";
    cout << "  2. Resting, but stressed/excited/not fully relaxed\n";
    cout << "  3. Exercised within the last 15 minutes\n";
    cout << "  4. Exercised 15-60 minutes ago\n";
    cout << "  5. Unknown\n";

    int activityChoice = readIntInRange("Choose 1-5: ", 1, 5);
    context.activityState = static_cast<ActivityState>(activityChoice);

    context.playsSports = readYesNo("Does the person regularly play sports or train?");

    if (context.playsSports) {
        cout << "\nWhich type best describes the main sport/training?\n";
        cout << "  1. Endurance (distance running, cycling, swimming, rowing, etc.)\n";
        cout << "  2. Running/intermittent (soccer, basketball, lacrosse, tennis, etc.)\n";
        cout << "  3. Strength/power (weightlifting, throwing, sprint/power focused, etc.)\n";
        cout << "  4. Other\n";

        int sportChoice = readIntInRange("Choose 1-4: ", 1, 4);
        context.sportType = static_cast<SportType>(sportChoice);
        context.trainingDaysPerWeek = readIntInRange("Training days per week (0-7): ", 0, 7);
    } else {
        context.sportType = SportType::None;
        context.trainingDaysPerWeek = 0;
    }

    return context;
}

int main() {
    cout << "==========================================\n";
    cout << "   Context-Aware ECG Arrhythmia Analyzer\n";
    cout << "==========================================\n\n";

    cout << "Supported inputs:\n";
    cout << "  1. .csv or .txt ECG signal files\n";
    cout << "  2. MIT-BIH .dat files with matching .hea files\n\n";

    cout << "Enter ECG data filename: ";
    string filename;
    getline(cin, filename);

    if (filename.empty()) {
        cout << "\nInvalid input: no filename was entered.\n";
        return 0;
    }

    ECGSignal signal;
    string errorMessage;

    if (!ECGReader::readFile(filename, signal, errorMessage)) {
        cout << "\n" << errorMessage << "\n";
        return 0;
    }

    // CSV/TXT files often do not include sampling-rate metadata, so the user
    // may provide it. MIT-BIH DAT files get it automatically from the HEA file.
    if (signal.sourceType == "CSV/TXT") {
        cout << "Enter sampling rate in Hz, or type 0 to use 360 Hz: ";
        double userRate;
        cin >> userRate;

        if (cin.fail()) {
            cout << "\nInvalid input: sampling rate must be a number.\n";
            return 0;
        }

        if (userRate > 0) signal.samplingRate = userRate;
        else signal.samplingRate = 360.0;

        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    if (signal.samplingRate <= 0 || signal.samples.empty()) {
        cout << "\nInvalid input: ECG signal or sampling rate is unusable.\n";
        return 0;
    }

    PatientContext context = collectPatientContext();

    vector<int> rPeaks = PeakDetector::findRPeaks(signal.samples, signal.samplingRate);
    ArrhythmiaResult result = ArrhythmiaDetector::analyze(rPeaks, signal.samplingRate, context);

    if (!result.valid) {
        cout << "\nUnable to analyze this ECG file.\n";
        cout << "Reason: " << result.explanation << "\n";
        cout << "The recording may be too short, noisy, or may not contain enough detectable heartbeats.\n";
        return 0;
    }

    cout << "\n==========================================\n";
    cout << "              ECG Analysis\n";
    cout << "==========================================\n";
    cout << "Input type: " << signal.sourceType << "\n";
    cout << "Samples read: " << signal.samples.size() << "\n";
    cout << "Sampling rate: " << signal.samplingRate << " Hz\n";
    cout << "R-peaks detected: " << rPeaks.size() << "\n";
    cout << "Estimated heart rate: " << result.heartRate << " bpm\n";
    cout << "Average RR interval: " << result.averageRR << " seconds\n";
    cout << "RR irregularity ratio: " << result.irregularityRatio << "\n";
    cout << "Extreme RR intervals: " << result.extremeRRPercent << "%\n";

    cout << "\nContext used:\n";
    cout << "  Age: " << context.age << "\n";
    cout << "  Recording state: " << activityStateName(context.activityState) << "\n";
    cout << "  Sports/training: " << (context.playsSports ? "yes" : "no") << "\n";
    if (context.playsSports) {
        cout << "  Sport type: " << sportTypeName(context.sportType) << "\n";
        cout << "  Training days/week: " << context.trainingDaysPerWeek << "\n";
    }

    cout << "\nContext-adjusted expected heart-rate model:\n";
    cout << "  Expected center: " << result.expectedHeartRateCenter << " bpm\n";
    cout << "  Broad range: " << result.expectedHeartRateLow << "-"
         << result.expectedHeartRateHigh << " bpm\n";
    cout << "  Heart-rate concern contribution: "
         << (result.heartRateConcern * 100.0) << "% of maximum HR concern\n";

    cout << "\nContext-adjusted RR thresholds:\n";
    cout << "  Expected RR range: "
         << result.expectedRRLow << "-"
         << result.expectedRRHigh << " seconds\n";

    cout << "  Allowed RR irregularity before concern: "
         << result.allowedIrregularity << "\n";

    cout << "\nEstimated arrhythmia likelihood score: "
         << result.arrhythmiaLikelihoodPercent << "%\n";
    cout << "Likelihood category: " << result.likelihoodCategory << "\n";
    cout << "Explanation: " << result.explanation << "\n";

    cout << "\nIMPORTANT: The percentage above is a heuristic engineering score, not a\n";
    cout << "clinically calibrated probability or medical diagnosis. It becomes a true\n";
    cout << "probability only after the model is validated/calibrated against labeled data.\n";

    return 0;
}
