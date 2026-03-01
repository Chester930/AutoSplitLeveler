#include "zerocrossing.h"
#include <cmath>
#include <algorithm>

namespace Steinberg {
namespace Vst {

ZeroCrossingAnalyzer::ZeroCrossingAnalyzer(float sampleRate)
    : mSampleRate(sampleRate)
{
}

size_t ZeroCrossingAnalyzer::findNearestZeroCrossing(const float* buffer, size_t numSamples, size_t startIndex, size_t maxSearchSamples) {
    if (numSamples == 0 || startIndex >= numSamples) return startIndex;

    size_t lowerBound = (startIndex > maxSearchSamples) ? startIndex - maxSearchSamples : 0;
    size_t upperBound = std::min(startIndex + maxSearchSamples, numSamples - 1);

    size_t bestIndex = startIndex;
    float currentSample = buffer[startIndex];
    
    // If we are already exactly at 0
    if (currentSample == 0.0f) return startIndex;

    // Search forward and backward simultaneously to find the *nearest* crossing
    size_t searchDist = 1;
    while (true) {
        bool forwardValid = (startIndex + searchDist <= upperBound);
        bool backwardValid = (startIndex >= searchDist && startIndex - searchDist >= lowerBound);

        if (!forwardValid && !backwardValid) {
            break; // Reached search limits
        }

        // Check forward crossing
        if (forwardValid) {
            size_t idx = startIndex + searchDist;
            if ((buffer[idx - 1] > 0.0f && buffer[idx] <= 0.0f) || 
                (buffer[idx - 1] < 0.0f && buffer[idx] >= 0.0f)) {
                return idx;
            }
        }

        // Check backward crossing
        if (backwardValid) {
            size_t idx = startIndex - searchDist;
            if ((buffer[idx] > 0.0f && buffer[idx + 1] <= 0.0f) || 
                (buffer[idx] < 0.0f && buffer[idx + 1] >= 0.0f)) {
                return idx;
            }
        }

        searchDist++;
    }

    // If no zero crossing is found within the limit, return the original index
    return startIndex;
}

void ZeroCrossingAnalyzer::applyMicroFadeIn(float* buffer, size_t numSamples, size_t fadeLengthSamples) {
    if (fadeLengthSamples == 0 || numSamples == 0) return;
    size_t processSamples = std::min(numSamples, fadeLengthSamples);
    
    for (size_t i = 0; i < processSamples; ++i) {
        // Simple linear fade in
        float gain = static_cast<float>(i) / static_cast<float>(processSamples);
        buffer[i] *= gain;
    }
}

void ZeroCrossingAnalyzer::applyMicroFadeOut(float* buffer, size_t numSamples, size_t fadeLengthSamples) {
    if (fadeLengthSamples == 0 || numSamples == 0) return;
    size_t processSamples = std::min(numSamples, fadeLengthSamples);
    
    size_t startIndex = numSamples - processSamples;
    for (size_t i = 0; i < processSamples; ++i) {
        // Simple linear fade out
        float gain = 1.0f - (static_cast<float>(i) / static_cast<float>(processSamples));
        buffer[startIndex + i] *= gain;
    }
}

} // namespace Vst
} // namespace Steinberg
