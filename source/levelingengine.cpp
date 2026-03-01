#include "levelingengine.h"
#include <algorithm>

namespace Steinberg {
namespace Vst {

LevelingEngine::LevelingEngine(float targetRmsDb, float maxReductionDb)
    : mTargetRmsDb(targetRmsDb), mMaxReductionDb(maxReductionDb)
{
}

void LevelingEngine::calculateGainAdjustments(std::vector<AudioSegment>& segments) {
    for (auto& seg : segments) {
        // We only want to REDUCE loud segments (Micro Split logic).
        // If it's already quieter than the target, we might leave it alone to preserve natural dynamics,
        // or optionally boost it. According to the user's diagram, we are bringing 4-6 down to 2-4.
        
        float diff = mTargetRmsDb - seg.loudness.rmsDb;

        if (diff < 0.0f) {
            // Segment is louder than the target, apply negative gain
            // clamp the maximum reduction
            seg.appliedGainDb = std::max(diff, mMaxReductionDb);
        } else {
            // Segment is quieter than target, usually we don't boost, or boost mildly
            // user request: 將觸發指針 4~6 的字元降至 2~4 的標準 -> imply reduction only.
            seg.appliedGainDb = 0.0f;
        }
    }
}

} // namespace Vst
} // namespace Steinberg
