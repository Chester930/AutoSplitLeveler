#include "loudnessanalyzer.h"
#include <cmath>
#include <algorithm>

namespace Steinberg {
namespace Vst {

LoudnessAnalyzer::LoudnessAnalyzer(float sampleRate)
    : mSampleRate(sampleRate)
{
}

float LoudnessAnalyzer::linearToDb(float linear) {
    if (linear <= 0.000001f) {
        return -120.0f; // Silence floor
    }
    return 20.0f * std::log10(linear);
}

LoudnessResult LoudnessAnalyzer::analyzeSegment(const float* buffer, size_t numSamples, size_t startIndex) {
    LoudnessResult result = { -120.0f, -120.0f };
    if (numSamples == 0 || !buffer) return result;

    float sumSquares = 0.0f;
    float peak = 0.0f;

    for (size_t i = 0; i < numSamples; ++i) {
        float sample = buffer[startIndex + i];
        float absSample = std::abs(sample);
        
        if (absSample > peak) {
            peak = absSample;
        }
        
        sumSquares += (sample * sample);
    }

    float meanSquare = sumSquares / static_cast<float>(numSamples);
    float rms = std::sqrt(meanSquare);

    result.peakDb = linearToDb(peak);
    result.rmsDb = linearToDb(rms);

    return result;
}

} // namespace Vst
} // namespace Steinberg
