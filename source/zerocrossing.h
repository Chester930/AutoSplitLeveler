#pragma once

#include <cstddef>
#include <vector>

namespace Steinberg {
namespace Vst {

class ZeroCrossingAnalyzer {
public:
    ZeroCrossingAnalyzer(float sampleRate);

    // Find the nearest zero crossing point from a given startIndex
    // Search within a maximum of maxSearchSamples 
    // Returns the refined sample index
    size_t findNearestZeroCrossing(const float* buffer, size_t numSamples, size_t startIndex, size_t maxSearchSamples = 4410);

    // Apply a micro fade-in to the buffer (alters the buffer directly)
    // For ARA2 edit events, this might be handled by host fade settings,
    // but useful if we need to hard-bake the fade.
    void applyMicroFadeIn(float* buffer, size_t numSamples, size_t fadeLengthSamples);

    // Apply a micro fade-out to the buffer (alters the buffer directly)
    void applyMicroFadeOut(float* buffer, size_t numSamples, size_t fadeLengthSamples);

private:
    float mSampleRate;
};

} // namespace Vst
} // namespace Steinberg
