#pragma once

#include "loudnessanalyzer.h"
#include <vector>

namespace Steinberg {
namespace Vst {

struct AudioSegment {
    size_t startIndex;
    size_t length;
    LoudnessResult loudness;
    float appliedGainDb; // Gain to apply in dB
};

class LevelingEngine {
public:
    // This is essentially an empirical mapping based on the "2 to 4" compressor meter logic.
    // We assume the TubeCompressor gives a GR of -4 to -6 dB for loud words,
    // and we want it to sit at -2 to -4 dB.
    // We will establish an abstract 'Target RMS/LUFS'. 
    LevelingEngine(float targetRmsDb, float maxReductionDb = -6.0f);

    // Analyze segments and calculate the required gain adjustment (Clip Gain) 
    // to bring their RMS to the target RMS.
    void calculateGainAdjustments(std::vector<AudioSegment>& segments);

private:
    float mTargetRmsDb;
    float mMaxReductionDb;
};

} // namespace Vst
} // namespace Steinberg
