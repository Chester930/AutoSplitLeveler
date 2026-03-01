#include "AutoSplitLevelerAlgorithm.h"
#include <vector>
#include <cmath>

AutoSplitLevelerAlgorithm::AutoSplitLevelerAlgorithm()
    : TestProcessingAlgorithm("AutoSplitLeveler", "com.vst3.autosplitleveler.algorithm"),
      mThresholdDb(-45.0f),
      mMinSilenceSeconds(0.150f)
{
}

namespace {

struct AudioSegment {
    int64_t start;
    int64_t end;
};

float peak_dbfs(const float* samples, int64_t count)
{
    if (count == 0) return -100.0f;
    float peak = 0.0f;
    for (int64_t i = 0; i < count; ++i)
    {
        float absVal = std::abs(samples[i]);
        if (absVal > peak) peak = absVal;
    }
    if (peak < 1e-10f) return -100.0f;
    return 20.0f * std::log10(peak);
}

std::vector<AudioSegment> find_segments(const float* mono, int64_t totalSamples, double sr, float threshDb, int minSilenceMs)
{
    float thresh = std::pow(10.0f, threshDb / 20.0f);
    int64_t minSil = static_cast<int64_t>(minSilenceMs * sr / 1000.0);

    std::vector<AudioSegment> segments;
    bool inSpeech = false;
    int64_t speechStart = 0;
    int64_t silenceCount = 0;

    for (int64_t i = 0; i < totalSamples; ++i)
    {
        bool silent = std::abs(mono[i]) < thresh;
        if (!silent)
        {
            if (!inSpeech)
            {
                inSpeech = true;
                speechStart = i;
            }
            silenceCount = 0;
        }
        else
        {
            if (inSpeech)
            {
                silenceCount++;
                if (silenceCount >= minSil)
                {
                    segments.push_back({speechStart, i - silenceCount});
                    inSpeech = false;
                    silenceCount = 0;
                }
            }
        }
    }
    if (inSpeech)
    {
        segments.push_back({speechStart, totalSamples});
    }
    return segments;
}

} // namespace

std::unique_ptr<TestNoteContent> AutoSplitLevelerAlgorithm::analyzeNoteContent(TestAnalysisCallbacks* analysisCallbacks, int64_t sampleCount, double sampleRate, uint32_t channelCount) const
{
    analysisCallbacks->notifyAnalysisProgressStarted();

    // 1. Read all samples into a buffer (simplified mono mixdown)
    std::vector<float> mixdownBuffer(sampleCount, 0.0f);
    
    constexpr auto blockSize = 2048U;
    std::vector<float> blockBuffer(channelCount * blockSize);
    std::vector<void*> dataPointers(channelCount);
    for (auto c = 0U; c < channelCount; ++c)
        dataPointers[c] = &blockBuffer[c * blockSize];

    int64_t samplesRead = 0;
    while (samplesRead < sampleCount)
    {
        if (analysisCallbacks->shouldCancel())
        {
            analysisCallbacks->notifyAnalysisProgressCompleted();
            return {};
        }

        const auto count = std::min(static_cast<int64_t>(blockSize), sampleCount - samplesRead);
        analysisCallbacks->readAudioSamples(samplesRead, count, dataPointers.data());

        // Simple mono mixdown for analysis
        for (int64_t i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (uint32_t c = 0; c < channelCount; ++c)
                sum += blockBuffer[i + c * blockSize];
            
            mixdownBuffer[samplesRead + i] = sum / static_cast<float>(channelCount);
        }

        samplesRead += count;
        analysisCallbacks->notifyAnalysisProgressUpdated(0.5f * static_cast<float>(samplesRead) / static_cast<float>(sampleCount));
    }

    // ── Two-Stage Peak Normalization Logic ─────────────────────────────────

    const float SILENCE_THRESH_DB = -45.0f;
    const int MACRO_SILENCE_MS = 1000;
    const int MICRO_SILENCE_MS = 150;
    const float TARGET_PEAK_DB = -3.0f;
    const float MAX_INTRA_DROP_DB = 6.0f;

    auto macroSegments = find_segments(mixdownBuffer.data(), mixdownBuffer.size(), sampleRate, SILENCE_THRESH_DB, MACRO_SILENCE_MS);

    std::vector<TestNote> foundNotes;
    const float minAllowedPeak = TARGET_PEAK_DB - MAX_INTRA_DROP_DB;

    for (const auto& macroSeg : macroSegments)
    {
        int64_t macroLength = macroSeg.end - macroSeg.start;
        float macroPeak = peak_dbfs(mixdownBuffer.data() + macroSeg.start, macroLength);
        
        float macroGainDb = TARGET_PEAK_DB - macroPeak;

        // Find micro segments within this macro segment
        auto microSegments = find_segments(mixdownBuffer.data() + macroSeg.start, macroLength, sampleRate, SILENCE_THRESH_DB, MICRO_SILENCE_MS);

        int64_t currentPos = macroSeg.start;
        for (const auto& uSeg : microSegments)
        {
            int64_t absUStart = macroSeg.start + uSeg.start;
            int64_t absUEnd = macroSeg.start + uSeg.end;

            // Space before this micro segment gets standard macro gain
            if (absUStart > currentPos)
            {
                TestNote gapNote;
                gapNote._frequency = ARA::kARAInvalidFrequency;
                gapNote._volume = macroGainDb;
                gapNote._startTime = static_cast<double>(currentPos) / sampleRate;
                gapNote._duration = static_cast<double>(absUStart - currentPos) / sampleRate;
                foundNotes.push_back(gapNote);
            }

            // The micro segment itself
            float microPeakOrig = peak_dbfs(mixdownBuffer.data() + absUStart, absUEnd - absUStart);
            float microPeakWithMacro = microPeakOrig + macroGainDb;
            float finalMicroGainDb = macroGainDb;

            // If the micro peak (after macro boost) is too quiet, pull it up to the minimum allowed
            if (microPeakWithMacro < minAllowedPeak && microPeakOrig > -90.0f)
            {
                float extraGainDb = minAllowedPeak - microPeakWithMacro;
                finalMicroGainDb += extraGainDb;
            }

            TestNote microNote;
            microNote._frequency = ARA::kARAInvalidFrequency;
            microNote._volume = finalMicroGainDb;
            microNote._startTime = static_cast<double>(absUStart) / sampleRate;
            microNote._duration = static_cast<double>(absUEnd - absUStart) / sampleRate;
            foundNotes.push_back(microNote);

            currentPos = absUEnd;
        }

        // Space after the last micro segment (remaining tail of the macro segment)
        if (macroSeg.end > currentPos)
        {
            TestNote gapNote;
            gapNote._frequency = ARA::kARAInvalidFrequency;
            gapNote._volume = macroGainDb;
            gapNote._startTime = static_cast<double>(currentPos) / sampleRate;
            gapNote._duration = static_cast<double>(macroSeg.end - currentPos) / sampleRate;
            foundNotes.push_back(gapNote);
        }
    }

    analysisCallbacks->notifyAnalysisProgressCompleted();
    return std::make_unique<TestNoteContent>(foundNotes);
}
