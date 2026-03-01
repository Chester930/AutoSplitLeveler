#include "silencedetector.h"
#include <algorithm>

namespace Steinberg {
namespace Vst {

SilenceDetector::SilenceDetector(float sampleRate)
    : mSampleRate(sampleRate)
    , mThresholdLinear(dbToLinear(-45.0f))
    , mMinSilenceSamples(static_cast<size_t>(0.5f * sampleRate))
    , mCurrentSilenceSamples(0)
    , mInSilence(false)
    , mSilenceStartIndex(0)
{
}

void SilenceDetector::setThresholdDb(float db) {
    mThresholdLinear = dbToLinear(db);
}

void SilenceDetector::setMinimumSilenceDuration(float seconds) {
    mMinSilenceSamples = static_cast<size_t>(seconds * mSampleRate);
}

std::vector<SplitPoint> SilenceDetector::analyze(const float* buffer, size_t numSamples) {
    std::vector<SplitPoint> foundSplits;

    for (size_t i = 0; i < numSamples; ++i) {
        float absSample = std::abs(buffer[i]);

        if (absSample < mThresholdLinear) {
            if (!mInSilence) {
                mInSilence = true;
                mSilenceStartIndex = i;
                mCurrentSilenceSamples = 0;
            }
            mCurrentSilenceSamples++;
        } else {
            if (mInSilence) {
                if (mCurrentSilenceSamples >= mMinSilenceSamples) {
                    // We found a silence block that meets the criteria
                    // A simple macro split logic: split in the middle of the silence
                    size_t splitInSilence = mSilenceStartIndex + (mCurrentSilenceSamples / 2);
                    foundSplits.push_back({ splitInSilence });
                }
                mInSilence = false;
                mCurrentSilenceSamples = 0;
            }
        }
    }

    // Handle case where audio ends during silence
    if (mInSilence && mCurrentSilenceSamples >= mMinSilenceSamples) {
        size_t splitInSilence = mSilenceStartIndex + (mCurrentSilenceSamples / 2);
        foundSplits.push_back({ splitInSilence });
    }

    return foundSplits;
}

} // namespace Vst
} // namespace Steinberg
