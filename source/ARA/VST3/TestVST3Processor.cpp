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
#include <cmath>
ARA_DISABLE_VST3_WARNINGS_BEGIN
    #include "pluginterfaces/vst/ivstprocesscontext.h"
ARA_DISABLE_VST3_WARNINGS_END

using namespace Steinberg;

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
    //--- Here you have to implement your processing

    if (!data.outputs || !data.outputs[0].numChannels)
        return kResultTrue;

    ARA_VALIDATE_API_CONDITION (data.outputs[0].numChannels == getAudioBusChannelCount (audioOutputs[0]));
    ARA_VALIDATE_API_CONDITION (data.numSamples <= processSetup.maxSamplesPerBlock);

    // Internal Auto-Leveler for DOP / Insert
    const float targetRmsDb = -18.0f; // Target output RMS
    const float noiseGateDb = -45.0f; // Silence threshold
    const float maxGainDb = 12.0f;
    const float minGainDb = -12.0f;

    // 50ms RMS window smoothing
    const float rmsWindowSec = 0.050f;
    const float rmsAlpha = 1.0f - std::exp(-1.0f / (m_sampleRate * rmsWindowSec));

    // Attack/Release smoothing (10ms attack for peaks, 200ms release for quiet recovery)
    const float attackSec = 0.010f; 
    const float releaseSec = 0.200f;
    const float attackAlpha = 1.0f - std::exp(-1.0f / (m_sampleRate * attackSec));
    const float releaseAlpha = 1.0f - std::exp(-1.0f / (m_sampleRate * releaseSec));

    for (int32 sampleIndex = 0; sampleIndex < data.numSamples; ++sampleIndex)
    {
        // Calculate mono instantaneous power
        float currentPower = 0.0f;
        for (int32 c = 0; c < data.outputs[0].numChannels; ++c)
        {
            float s = data.inputs[0].channelBuffers32[c][sampleIndex];
            currentPower += s * s;
        }
        if (data.outputs[0].numChannels > 0)
            currentPower /= (float)data.outputs[0].numChannels;

        // RMS smoothing state
        m_rmsWindowPower = m_rmsWindowPower * (1.0f - rmsAlpha) + currentPower * rmsAlpha;

        float powerDb = -100.0f;
        if (m_rmsWindowPower > 1e-10f)
            powerDb = 10.0f * std::log10(m_rmsWindowPower);

        float targetGainDb = 0.0f;
        if (powerDb > noiseGateDb)
        {
            // Calculate gain to hit target RMS
            targetGainDb = targetRmsDb - powerDb;

            // Clamp to safe bounds
            if (targetGainDb > maxGainDb) targetGainDb = maxGainDb;
            if (targetGainDb < minGainDb) targetGainDb = minGainDb;
        }

        // Smooth current gain towards target gain based on attack/release
        if (targetGainDb < m_currentGainDb)
        {
            // Gain is decreasing (Compressing a peak -> Fast Attack)
            m_currentGainDb = m_currentGainDb * (1.0f - attackAlpha) + targetGainDb * attackAlpha;
        }
        else
        {
            // Gain is increasing (Recovering a quiet part -> Slow Release)
            m_currentGainDb = m_currentGainDb * (1.0f - releaseAlpha) + targetGainDb * releaseAlpha;
        }

        // Convert back to linear format and apply to output samples
        float linearGain = std::pow(10.0f, m_currentGainDb / 20.0f);
        
        for (int32 c = 0; c < data.outputs[0].numChannels; ++c)
        {
            data.outputs[0].channelBuffers32[c][sampleIndex] = 
                data.inputs[0].channelBuffers32[c][sampleIndex] * linearGain;
        }
    }

    // if we are an ARA editor renderer, we now would add out preview signal to the output, but
    // our test implementation does not support editing and thus never generates any preview signal.
//  if (auto editorRenderer = _araPlugInExtension.getEditorRenderer<ARATestEditorRenderer*> ())
//      editorRenderer->addEditorSignal (...);

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
