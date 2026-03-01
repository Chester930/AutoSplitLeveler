#pragma once

#include "ARA_Library/PlugIn/ARAPlug.h"
#include "TestAnalysis.h"


// Note: TestNoteContent is defined in TestAnalysis.h as std::vector<TestNote>
// We use ARA's built-in TestNote structure to represent AudioSegments for ARA.
// TestNote has { _frequency, _volume, _startTime, _duration }
// We can use _volume for the calculated Gain Reduction we want to apply to this region.

class AutoSplitLevelerAlgorithm : public TestProcessingAlgorithm
{
public:
    AutoSplitLevelerAlgorithm();

    std::unique_ptr<TestNoteContent> analyzeNoteContent(TestAnalysisCallbacks* analysisCallbacks, int64_t sampleCount, double sampleRate, uint32_t channelCount) const override;

private:
    float mThresholdDb;
    float mMinSilenceSeconds;
};
