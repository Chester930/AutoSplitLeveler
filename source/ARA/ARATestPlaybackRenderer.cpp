//------------------------------------------------------------------------------
//! \file       ARATestPlaybackRenderer.cpp
//!             playback renderer implementation for the ARA test plug-in,
//!             customizing the playback renderer base class of the ARA library
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2026, Celemony Software GmbH, All Rights Reserved.
//! \license    Licensed under the Apache License, Version 2.0 (the "License");
//!             you may not use this file except in compliance with the License.
//!             You may obtain a copy of the License at
//!
//!               http://www.apache.org/licenses/LICENSE-2.0
//!
//!             Unless required by applicable law or agreed to in writing, software
//!             distributed under the License is distributed on an "AS IS" BASIS,
//!             WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//!             See the License for the specific language governing permissions and
//!             limitations under the License.
//------------------------------------------------------------------------------

#include "ARATestPlaybackRenderer.h"
#include "ARATestDocumentController.h"
#include "ARATestAudioSource.h"

void ARATestPlaybackRenderer::renderPlaybackRegions (float* const* ppOutput, ARA::ARASamplePosition samplePosition,
                                                     ARA::ARASampleCount samplesToRender, bool isPlayingBack)
{
    // initialize output buffers with silence
    for (auto c { 0 }; c < _channelCount; ++c)
        std::memset (ppOutput[c], 0, sizeof (float) * static_cast<size_t> (samplesToRender));

    if (!isPlayingBack)
        return;

    auto docController { getDocumentController<ARATestDocumentController> () };
    if (docController->rendererWillAccessModelGraph (this))
    {
        const auto sampleEnd { samplePosition + samplesToRender };
        for (const auto& playbackRegion : getPlaybackRegions ())
        {
            const auto audioModification { playbackRegion->getAudioModification () };
            ARA_VALIDATE_API_STATE (!audioModification->isDeactivatedForUndoHistory ());
            const auto audioSource { audioModification->getAudioSource<const ARATestAudioSource> () };
            ARA_VALIDATE_API_STATE (!audioSource->isDeactivatedForUndoHistory ());

            if (!audioSource->isSampleAccessEnabled ())
                continue;

            if (audioSource->getSampleRate () != _sampleRate)
                continue;

            const auto regionStartSample { playbackRegion->getStartInPlaybackSamples (_sampleRate) };
            if (sampleEnd <= regionStartSample)
                continue;

            const auto regionEndSample { playbackRegion->getEndInPlaybackSamples (_sampleRate) };
            if (regionEndSample <= samplePosition)
                continue;

            auto startSongSample { std::max (regionStartSample, samplePosition) };
            auto endSongSample { std::min (regionEndSample, sampleEnd) };

            const auto offsetToPlaybackRegion { playbackRegion->getStartInAudioModificationSamples () - regionStartSample };

            const auto startAvailableSourceSamples { std::max (ARA::ARASamplePosition { 0 }, playbackRegion->getStartInAudioModificationSamples ()) };
            const auto endAvailableSourceSamples { std::min (audioSource->getSampleCount (), playbackRegion->getEndInAudioModificationSamples ()) };

            startSongSample = std::max (startSongSample, startAvailableSourceSamples - offsetToPlaybackRegion);
            endSongSample = std::min (endSongSample, endAvailableSourceSamples - offsetToPlaybackRegion);
            if (endSongSample <= startSongSample)
                continue;

            // cache the note content pointer for gain lookup
            const TestNoteContent* noteContent { audioSource->getNoteContent () };
            const auto sr { _sampleRate };

            const auto sourceChannelCount { audioSource->getChannelCount () };
            for (auto posInSong { startSongSample }; posInSong < endSongSample; ++posInSong)
            {
                const auto posInBuffer { posInSong - samplePosition };
                const auto posInSource { posInSong + offsetToPlaybackRegion };

                // ── Dynamic Spread Scaling ──────────────────────────────
                const float targetPeakDb = _targetPeakDb;
                float gainLinear = 1.0f;

                if (noteContent)
                {
                    const double timeSec { static_cast<double> (posInSource) / sr };
                    for (const auto& note : *noteContent)
                    {
                        if (timeSec >= note._startTime && timeSec < note._startTime + note._duration)
                        {
                            if (note._maxPeak > -90.0f)
                            {
                                // Formula: New_dB = TargetPeak - (MaxPeakAna - Original_dB) * (4.0 / SpreadAna)
                                const float spreadAna = note._maxPeak - note._minPeak;
                                const float scale = (spreadAna > 0.1f) ? (4.0f / spreadAna) : 1.0f;
                                const float maxPeakAna = note._maxPeak;

                                // Helper function to process a single sample value
                                auto processSample = [&](float s) -> float {
                                    if (std::abs(s) < 1e-10f) return 0.0f;
                                    float originalDb = 20.0f * std::log10(std::abs(s));
                                    float newDb = targetPeakDb - (maxPeakAna - originalDb) * scale;
                                    float sign = (s > 0) ? 1.0f : -1.0f;
                                    return std::pow(10.0f, newDb / 20.0f) * sign;
                                };

                                // Apply to current sample
                                if (sourceChannelCount == _channelCount)
                                {
                                    for (auto c { 0 }; c < sourceChannelCount; ++c)
                                    {
                                        float s = audioSource->getRenderSampleCacheForChannel (c)[posInSource];
                                        ppOutput[c][posInBuffer] += processSample(s);
                                    }
                                }
                                else
                                {
                                    float monoSum { 0.0f };
                                    for (auto c { 0 }; c < sourceChannelCount; ++c)
                                        monoSum += audioSource->getRenderSampleCacheForChannel (c)[posInSource];
                                    if (sourceChannelCount > 1)
                                        monoSum /= static_cast<float> (sourceChannelCount);
                                    
                                    float processedSum = processSample(monoSum);
                                    for (auto c { 0 }; c < _channelCount; ++c)
                                        ppOutput[c][posInBuffer] = processedSum;
                                }
                                goto nextSample; // Processed this sample, skip default logic
                            }
                            break;
                        }
                    }
                }
                // Default passthrough if no note found
                if (sourceChannelCount == _channelCount)
                {
                    for (auto c { 0 }; c < sourceChannelCount; ++c)
                        ppOutput[c][posInBuffer] += audioSource->getRenderSampleCacheForChannel (c)[posInSource];
                }
                else
                {
                    float monoSum { 0.0f };
                    for (auto c { 0 }; c < sourceChannelCount; ++c)
                        monoSum += audioSource->getRenderSampleCacheForChannel (c)[posInSource];
                    if (sourceChannelCount > 1)
                        monoSum /= static_cast<float> (sourceChannelCount);
                    for (auto c { 0 }; c < _channelCount; ++c)
                        ppOutput[c][posInBuffer] = monoSum;
                }

            nextSample:;
            }
        }

        docController->rendererDidAccessModelGraph (this);
    }
}


void ARATestPlaybackRenderer::enableRendering (ARA::ARASampleRate sampleRate, ARA::ARAChannelCount channelCount, ARA::ARASampleCount maxSamplesToRender, bool apiSupportsToggleRendering) noexcept
{
    // proper plug-ins would use this call to manage the resources which they need for rendering,
    // but our test plug-in caches everything it needs in-memory anyways, so this method is near-empty
    _sampleRate = sampleRate;
    _channelCount = channelCount;
    _maxSamplesToRender = maxSamplesToRender;
#if ARA_VALIDATE_API_CALLS
    _isRenderingEnabled = true;
    _apiSupportsToggleRendering = apiSupportsToggleRendering;
#endif
}

void ARATestPlaybackRenderer::disableRendering () noexcept
{
#if ARA_VALIDATE_API_CALLS
    _isRenderingEnabled = false;
#endif
}

#if ARA_VALIDATE_API_CALLS
void ARATestPlaybackRenderer::willAddPlaybackRegion (ARA::PlugIn::PlaybackRegion* /*playbackRegion*/) noexcept
{
    if (_apiSupportsToggleRendering)
        ARA_VALIDATE_API_STATE (!_isRenderingEnabled);
//  else
//      proper plug-ins would check _isRenderingEnabled here and toggle it off on demand, toggling it back on in didAddPlaybackRegion()
//      this works because hosts using such APIs implicitly guarantee that they do not concurrently render the plug-in while making this call
}

void ARATestPlaybackRenderer::willRemovePlaybackRegion (ARA::PlugIn::PlaybackRegion* /*playbackRegion*/) noexcept
{
    if (_apiSupportsToggleRendering)
        ARA_VALIDATE_API_STATE (!_isRenderingEnabled);
//  else
//      see willAddPlaybackRegion(), same pattern applies here
}
#endif
