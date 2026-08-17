#include "ECGReader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// Convert a string to lowercase.
// Example: "CSV" becomes "csv".
static string toLower(string text)
{
    for (char& character : text)
    {
        character = static_cast<char>(tolower(character));
    }

    return text;
}

// Return the file extension.
// Example: "107.csv" returns "csv".
static string getExtension(const string& filename)
{
    size_t dotPosition = filename.find_last_of('.');

    if (dotPosition == string::npos)
    {
        return "";
    }

    return toLower(filename.substr(dotPosition + 1));
}

// Remove the file extension.
// Example: "100.dat" returns "100".
static string removeExtension(const string& filename)
{
    size_t dotPosition = filename.find_last_of('.');

    if (dotPosition == string::npos)
    {
        return filename;
    }

    return filename.substr(0, dotPosition);
}

// Read CSV or TXT ECG data.
static bool readCSVorTXT(
    const string& filename,
    ECGSignal& signal,
    string& errorMessage)
{
    ifstream file(filename);

    if (!file.is_open())
    {
        errorMessage = "Invalid input: file could not be opened.";
        return false;
    }

    vector<vector<double>> rows;
    string line;

    while (getline(file, line))
    {
        // Allow comma, semicolon, tab, and space-separated data.
        for (char& character : line)
        {
            if (character == ',' ||
                character == ';' ||
                character == '\t')
            {
                character = ' ';
            }
        }

        stringstream lineStream(line);
        vector<double> numbers;
        double value;

        while (lineStream >> value)
        {
            numbers.push_back(value);
        }

        // Text headers are skipped because they contain no readable numbers.
        if (!numbers.empty())
        {
            rows.push_back(numbers);
        }
    }

    file.close();

    if (rows.size() < 10)
    {
        errorMessage =
            "Invalid input: the file does not appear to contain enough ECG "
            "waveform samples. Make sure you selected an actual ECG signal "
            "file, not a checksum, documentation, header, or annotation file.";

        return false;
    }

    // Find the largest number of columns found in any row.
    size_t maximumColumns = 0;

    for (const vector<double>& row : rows)
    {
        maximumColumns = max(maximumColumns, row.size());
    }

    if (maximumColumns == 0)
    {
        errorMessage = "Invalid input: no numeric columns were found.";
        return false;
    }

    size_t selectedColumn = 0;

    /*
     * Choose the ECG column based on the file structure.
     *
     * One-column file:
     *     Column 0 = ECG signal
     *
     * Two-column file:
     *     Column 0 = time or sample index
     *     Column 1 = ECG signal
     *
     * Kaggle MIT-BIH three-column file:
     *     Column 0 = sample index
     *     Column 1 = ECG signal 1
     *     Column 2 = ECG signal 2
     *
     * For three or more columns, use column 1 by default.
     */
    if (maximumColumns == 1)
    {
        selectedColumn = 0;
    }
    else
    {
        selectedColumn = 1;
    }

    signal.samples.clear();

    for (const vector<double>& row : rows)
    {
        if (row.size() > selectedColumn)
        {
            signal.samples.push_back(row[selectedColumn]);
        }
    }

    if (signal.samples.size() < 10)
    {
        errorMessage =
            "Invalid input: fewer than 10 usable ECG samples were read "
            "from the selected signal column.";

        return false;
    }

    signal.sourceType = "CSV/TXT";

    return true;
}

// Convert a 12-bit signed number into a regular C++ integer.
static int signExtend12Bit(int value)
{
    if (value >= 2048)
    {
        value -= 4096;
    }

    return value;
}

// Read MIT-BIH 212-format DAT data with a matching HEA file.
static bool readMITBIH212Dat(
    const string& filename,
    ECGSignal& signal,
    string& errorMessage)
{
    string baseFilename = removeExtension(filename);
    string headerFilename = baseFilename + ".hea";

    ifstream headerFile(headerFilename);

    if (!headerFile.is_open())
    {
        errorMessage =
            "Invalid input: matching .hea header file was not found.";

        return false;
    }

    string firstHeaderLine;
    getline(headerFile, firstHeaderLine);

    stringstream firstLineStream(firstHeaderLine);

    string recordName;
    int numberOfSignals = 0;
    double samplingRate = 0.0;

    firstLineStream >>
        recordName >>
        numberOfSignals >>
        samplingRate;

    if (numberOfSignals < 1 || samplingRate <= 0)
    {
        errorMessage =
            "Invalid input: the .hea header file could not be parsed.";

        return false;
    }

    string firstSignalLine;
    getline(headerFile, firstSignalLine);

    headerFile.close();

    // This program currently supports MIT-BIH format 212 only.
    if (firstSignalLine.find("212") == string::npos)
    {
        errorMessage =
            "Unsupported ECG encoding: this program only supports "
            "MIT-BIH 212-format .dat files.";

        return false;
    }

    ifstream dataFile(filename, ios::binary);

    if (!dataFile.is_open())
    {
        errorMessage =
            "Invalid input: the .dat file could not be opened.";

        return false;
    }

    signal.samples.clear();

    unsigned char byte0;
    unsigned char byte1;
    unsigned char byte2;

    while (
        dataFile.read(reinterpret_cast<char*>(&byte0), 1) &&
        dataFile.read(reinterpret_cast<char*>(&byte1), 1) &&
        dataFile.read(reinterpret_cast<char*>(&byte2), 1))
    {
        /*
         * MIT-BIH format 212 stores two 12-bit signal values
         * inside three bytes.
         */
        int sampleChannel1 =
            static_cast<int>(byte0) |
            ((static_cast<int>(byte1) & 0x0F) << 8);

        int sampleChannel2 =
            static_cast<int>(byte2) |
            ((static_cast<int>(byte1) & 0xF0) << 4);

        sampleChannel1 = signExtend12Bit(sampleChannel1);
        sampleChannel2 = signExtend12Bit(sampleChannel2);

        // Use the first ECG channel.
        signal.samples.push_back(sampleChannel1);
    }

    dataFile.close();

    if (signal.samples.size() < 10)
    {
        errorMessage =
            "Invalid input: not enough samples were read from the .dat file.";

        return false;
    }

    signal.samplingRate = samplingRate;
    signal.sourceType = "MIT-BIH 212 DAT/HEA";

    return true;
}

// Main ECG file-reading function.
bool ECGReader::readFile(
    const string& filename,
    ECGSignal& signal,
    string& errorMessage)
{
    // Clear old values before reading a new file.
    signal.samples.clear();
    signal.sourceType.clear();
    errorMessage.clear();

    string extension = getExtension(filename);

    if (extension == "csv" || extension == "txt")
    {
        return readCSVorTXT(filename, signal, errorMessage);
    }

    if (extension == "dat")
    {
        return readMITBIH212Dat(filename, signal, errorMessage);
    }

    errorMessage =
        "Invalid input: unsupported file type. Use .csv, .txt, "
        "or MIT-BIH .dat with a matching .hea file.";

    return false;
}