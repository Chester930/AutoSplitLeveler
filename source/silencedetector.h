#pragma once

#include <vector>
#include <cstddef>
#include <cmath>

namespace Steinberg {
namespace Vst {

struct SplitPoint {
    size_t sampleIndex;
};

class SilenceDetector {
public:
    SilenceDetector(float sampleRate);

    // Set parameters
    void setThresholdDb(float db);
    void setMinimumSilenceDuration(float seconds);

    // Process a block of audio and return split points
    // Note: In ARA, we usually analyze the entire clip offline. 
    // This function assumes processing a continuous stream or full buffer.
    std::vector<SplitPoint> analyze(const float* buffer, size_t numSamples);

private:
    float mSampleRate;
    float mThresholdLinear;
    size_t mMinSilenceSamples;

    size_t mCurrentSilenceSamples;
    bool mInSilence;
    size_t mSilenceStartIndex;
    
    std::vector<SplitPoint> mPendingSplits;

    // Helper to convert dB to linear
    static float dbToLinear(float db) {
        return std::pow(10.0f, db / 20.0f);
    }
};

} // namespace Vst
} // namespace Steinberg
