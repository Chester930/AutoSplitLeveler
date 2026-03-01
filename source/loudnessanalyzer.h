#pragma once

#include <cstddef>

namespace Steinberg {
namespace Vst {

struct LoudnessResult {
    float peakDb;
    float rmsDb;
};

class LoudnessAnalyzer {
public:
    LoudnessAnalyzer(float sampleRate);

    // Calculate RMS and Peak for a specific segment of the buffer
    // Returns results in Decibels (dBFS), where 1.0f is 0 dBFS.
    LoudnessResult analyzeSegment(const float* buffer, size_t numSamples, size_t startIndex = 0);

private:
    float mSampleRate;

    // Helper to convert linear amplitude to Decibels
    float linearToDb(float linear);
};

} // namespace Vst
} // namespace Steinberg
