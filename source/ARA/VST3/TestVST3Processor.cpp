//------------------------------------------------------------------------------
//! \file       TestVST3Processor.cpp
//!             VST3 audio effect class for the ARA test plug-in,
//!             originally created using the VST project generator from the Steinberg VST3 SDK
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2026, Celemony Software GmbH, All Rights Reserved.
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

#include "TestVST3Processor.h"
#include "ARATestPlaybackRenderer.h"
#include "ARATestDocumentController.h"

#include "ARA_Library/PlugIn/ARAPlug.h"
#include "ARA_Library/Debug/ARADebug.h"
#include "ARATestAudioSource.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cmath>
ARA_DISABLE_VST3_WARNINGS_BEGIN
    #include "pluginterfaces/vst/ivstprocesscontext.h"
ARA_DISABLE_VST3_WARNINGS_END

namespace Steinberg {
namespace Vst {

// helper to improve readability
static int32 getAudioBusChannelCount (const IPtr<Vst::Bus>& bus)
{
    return Vst::SpeakerArr::getChannelCount (FCast<Vst::AudioBus> (bus.get ())->getArrangement ());
}

//-----------------------------------------------------------------------------
TestVST3Processor::TestVST3Processor ()
{
    setControllerClass (getEditControllerClassFUID ());

#if VST_VERSION >= 0x030700   /* 3.7.0 */
    processContextRequirements.needTransportState ();
#endif
}

//------------------------------------------------------------------------
TestVST3Processor::~TestVST3Processor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::initialize (FUnknown* context)
{
    // Here the Plug-in will be instantiated

    //---always initialize the parent-------
    tresult result = AudioEffect::initialize (context);
    // if everything Ok, continue
    if (result != kResultOk)
    {
        return result;
    }

    //--- create Audio IO ------
    addAudioInput (STR16 ("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::terminate ()
{
    // Here the Plug-in will be de-instantiated, last possibility to remove some memory!

    //---do not forget to call parent ------
    return AudioEffect::terminate ();
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::setBusArrangements (Vst::SpeakerArrangement* inputs, int32 numIns, Vst::SpeakerArrangement* outputs, int32 numOuts)
{
    // we only support one in and output bus and these buses must have the same number of non-zero channels
    if ((numIns == 1) && (numOuts == 1) &&
        (inputs[0] == outputs[0]) &&
        (Vst::SpeakerArr::getChannelCount (outputs[0]) != 0))
    {
        return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
    }

    return kResultFalse;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::setActive (TBool state)
{
    //--- called when the Plug-in is enable/disable (On/Off) -----
    if (state)
    {
        m_rmsWindowPower = 0.0f;
        m_currentGainDb = 0.0f;
    }

    if (ARATestPlaybackRenderer* playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ())
    {
        if (state)
            playbackRenderer->enableRendering (processSetup.sampleRate, getAudioBusChannelCount (audioOutputs[0]), processSetup.maxSamplesPerBlock, true);
        else
            playbackRenderer->disableRendering ();
    }

    return AudioEffect::setActive (state);
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::process (Vst::ProcessData& data)
{
    //--- Handle Parameter Changes
    if (data.inputParameterChanges)
    {
        int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParamsChanged; i++)
        {
            auto* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;
            int32 numPoints = queue->getPointCount();
            if (numPoints <= 0) continue;

            ParamValue value;
            int32 sampleOffset;
            if (queue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
            {
                switch (queue->getParameterId())
                {
                    case kSilenceThreshId: 
                        m_silenceThreshDb = (float)(value * 96.0 - 96.0); 
                        if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ()) {
                            for (auto region : playbackRenderer->getPlaybackRegions())
                                if (auto source = region->getAudioModification()->getAudioSource<ARATestAudioSource>())
                                    source->setSilenceThresholdDb(m_silenceThreshDb);
                        }
                        break;
                    case kSilenceGapId:
                        m_silenceGapMs = (float)(value * 2000.0); 
                        if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ()) {
                            for (auto region : playbackRenderer->getPlaybackRegions())
                                if (auto source = region->getAudioModification()->getAudioSource<ARATestAudioSource>())
                                    source->setSilenceGapMs(m_silenceGapMs);
                        }
                        break;
                    case kTargetPeakId:
                        m_targetPeakDb = (float)(value * 24.0 - 24.0); 
                        if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ()) {
                            playbackRenderer->setTargetPeakDb(m_targetPeakDb);
                        }
                        break;
                    case kTriggerSplitId:
                        if (value > 0.5) 
                        {
                            // Trigger ARA re-analysis (Function 1)
                            if (auto docController = _araPlugInExtension.getDocumentController())
                            {
                                // We need to trigger analysis for all sources in this instance
                                if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ()) {
                                    for (auto region : playbackRenderer->getPlaybackRegions())
                                        if (auto source = region->getAudioModification()->getAudioSource<ARATestAudioSource>())
                                            static_cast<ARATestDocumentController*>(docController)->startOrScheduleAnalysisOfAudioSource(source);
                                }
                            }
                        }
                        break;
                }
            }
        }
    }

    if (!data.outputs || !data.outputs[0].numChannels)
        return kResultTrue;

    ARA_VALIDATE_API_CONDITION (data.outputs[0].numChannels == getAudioBusChannelCount (audioOutputs[0]));
    ARA_VALIDATE_API_CONDITION (data.numSamples <= processSetup.maxSamplesPerBlock);

    // Function 2: Dynamic Spread Matching
    const float targetPeakDb = m_targetPeakDb; 

    // Fetch ARA data for the current region
    const TestNote* activeNote { nullptr };
    if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ())
    {
        // For DOP and standard processing, we need to know where we are in the audio source.
        // If the host provides project time, we use it. Otherwise, we might need to track it.
        int64_t currentSourcePos = 0;
        if (data.processContext && (data.processContext->state & Vst::ProcessContext::kProjectTimeMusicValid))
        {
            // Simplified: Use project time samples if available
            currentSourcePos = data.processContext->projectTimeSamples;
        }

        for (const auto& playbackRegion : playbackRenderer->getPlaybackRegions ())
        {
            const auto audioSource = playbackRegion->getAudioModification()->getAudioSource<const ARATestAudioSource>();
            const auto noteContent = audioSource->getNoteContent();
            if (noteContent)
            {
                const double timeSec = static_cast<double>(currentSourcePos) / processSetup.sampleRate;
                for (const auto& note : *noteContent)
                {
                    if (timeSec >= note._startTime && timeSec < note._startTime + note._duration)
                    {
                        activeNote = &note;
                        break;
                    }
                }
            }
            if (activeNote) break;
        }
    }

    if (!activeNote || activeNote->_maxPeak <= -90.0f)
    {
        // Fallback: No ARA analysis found, just pass through or use basic gain
        return kResultTrue;
    }

    const float spreadAna = activeNote->_maxPeak - activeNote->_minPeak;
    const float scale = (spreadAna > 0.1f) ? (4.0f / spreadAna) : 1.0f;
    const float maxPeakAna = activeNote->_maxPeak;

    for (int32 sampleIndex = 0; sampleIndex < data.numSamples; ++sampleIndex)
    {
        for (int32 c = 0; c < data.outputs[0].numChannels; ++c)
        {
            float s = data.inputs[0].channelBuffers32[c][sampleIndex];
            if (std::abs(s) < 1e-10f) 
            {
                data.outputs[0].channelBuffers32[c][sampleIndex] = 0.0f;
                continue;
            }

            float originalDb = 20.0f * std::log10(std::abs(s));
            
            // Core Formula: New_dB = TargetPeak - (MaxPeakAna - Original_dB) * (4.0 / SpreadAna)
            float newDb = targetPeakDb - (maxPeakAna - originalDb) * scale;
            
            float linearGain = std::pow(10.0f, (newDb - originalDb) / 20.0f);
            
            // Maintain original sign
            float sign = (s > 0) ? 1.0f : -1.0f;
            data.outputs[0].channelBuffers32[c][sampleIndex] = std::pow(10.0f, newDb / 20.0f) * sign;
        }
    }

    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    //--- called before any processing ----
    m_sampleRate = (float)newSetup.sampleRate;
    if (m_sampleRate <= 0.0f) m_sampleRate = 44100.0f;
    
    return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    // by default kSample32 is supported
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;

    // disable the following comment if your processing support kSample64
    /* if (symbolicSampleSize == Vst::kSample64)
        return kResultTrue; */

    return kResultFalse;
}

//-----------------------------------------------------------------------------
const ARA::ARAFactory* PLUGIN_API TestVST3Processor::getFactory ()
{
    return ARATestDocumentController::getARAFactory ();
}

//-----------------------------------------------------------------------------
const ARA::ARAPlugInExtensionInstance* PLUGIN_API TestVST3Processor::bindToDocumentController (ARA::ARADocumentControllerRef /*documentControllerRef*/)
{
    ARA_VALIDATE_API_STATE (false && "call is deprecated in ARA 2, host must not call this");
    return nullptr;
}

//-----------------------------------------------------------------------------
const ARA::ARAPlugInExtensionInstance* PLUGIN_API TestVST3Processor::bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef,
                                                                        ARA::ARAPlugInInstanceRoleFlags knownRoles, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
{
    return _araPlugInExtension.bindToARA (documentControllerRef, knownRoles, assignedRoles);
}

} // namespace Vst
} // namespace Steinberg
