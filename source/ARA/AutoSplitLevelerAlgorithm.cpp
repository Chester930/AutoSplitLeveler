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

std::vector<int64_t> find_midpoint_splits(const float* mono, int64_t totalSamples, double sr, float threshDb, int minSilenceMs)
{
    float thresh = std::pow(10.0f, threshDb / 20.0f);
    int64_t minSilSamples = static_cast<int64_t>(minSilenceMs * sr / 1000.0);

    std::vector<int64_t> splitPoints;
    int64_t silenceStart = -1;
    int64_t silenceCount = 0;

    for (int64_t i = 0; i < totalSamples; ++i)
    {
        bool isSilent = std::abs(mono[i]) < thresh;
        if (isSilent)
        {
            if (silenceStart == -1) silenceStart = i;
            silenceCount++;
        }
        else
        {
            if (silenceStart != -1 && silenceCount >= minSilSamples)
            {
                // Found a qualified silence period. Split at the midpoint.
                int64_t midpoint = silenceStart + (silenceCount / 2);
                splitPoints.push_back(midpoint);
            }
            silenceStart = -1;
            silenceCount = 0;
        }
    }

    // Check trailing silence
    if (silenceStart != -1 && silenceCount >= minSilSamples)
    {
        int64_t midpoint = silenceStart + (silenceCount / 2);
        splitPoints.push_back(midpoint);
    }

    return splitPoints;
}

} // namespace

std::unique_ptr<TestNoteContent> AutoSplitLevelerAlgorithm::analyzeNoteContent(TestAnalysisCallbacks* analysisCallbacks, int64_t sampleCount, double sampleRate, uint32_t channelCount) const
{
    analysisCallbacks->notifyAnalysisProgressStarted();

    // 1. Read all samples into a mono mixdown buffer
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

    // ── Function 1: Midpoint Split Logic ───────────────────────────────────
    
    const float SILENCE_THRESH_DB = analysisCallbacks->getSilenceThresholdDb();
    const int MIN_SILENCE_DURATION_MS = static_cast<int>(analysisCallbacks->getSilenceGapMs());

    auto splitPoints = find_midpoint_splits(mixdownBuffer.data(), mixdownBuffer.size(), sampleRate, SILENCE_THRESH_DB, MIN_SILENCE_DURATION_MS);

    std::vector<TestNote> foundNotes;
    int64_t currentStart = 0;

    auto createNote = [&](int64_t start, int64_t end) {
        if (end <= start) return;
        
        // Find peaks in this region to calculate dynamic spread
        float maxPeakDb = -100.0f;
        float minPeakDb = 0.0f; 
        bool foundAnyPeak = false;

        // Peak detection with a small window (e.g. 20ms) to find "meaningful" peaks
        int64_t windowSize = static_cast<int64_t>(0.020 * sampleRate);
        if (windowSize < 1) windowSize = 1;

        for (int64_t i = start; i < end; i += windowSize)
        {
            int64_t next = std::min(i + windowSize, end);
            float p = peak_dbfs(mixdownBuffer.data() + i, next - i);
            
            if (p > -45.0f) // Only consider non-silent peaks
            {
                if (!foundAnyPeak)
                {
                    maxPeakDb = p;
                    minPeakDb = p;
                    foundAnyPeak = true;
                }
                else
                {
                    if (p > maxPeakDb) maxPeakDb = p;
                    if (p < minPeakDb) minPeakDb = p;
                }
            }
        }

        TestNote note;
        note._frequency = ARA::kARAInvalidFrequency;
        note._volume = 0.0f; 
        note._startTime = static_cast<double>(start) / sampleRate;
        note._duration = static_cast<double>(end - start) / sampleRate;
        note._maxPeak = foundAnyPeak ? maxPeakDb : -100.0f;
        note._minPeak = foundAnyPeak ? minPeakDb : -100.0f;
        foundNotes.push_back(note);
    };

    for (int64_t split : splitPoints)
    {
        createNote(currentStart, split);
        currentStart = split;
    }
    createNote(currentStart, sampleCount);

    analysisCallbacks->notifyAnalysisProgressCompleted();
    return std::make_unique<TestNoteContent>(foundNotes);
}
