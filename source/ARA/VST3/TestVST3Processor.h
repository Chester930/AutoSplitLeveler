//------------------------------------------------------------------------------
//! \file       TestVST3Processor.h
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

#pragma once

#include "ARA_API/ARAVST3.h"
#include "TestVST3Controller.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include "ARA_Library/PlugIn/ARAPlug.h"

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
//  TestVST3Processor
//------------------------------------------------------------------------
class TestVST3Processor : public AudioEffect, public ARA::IPlugInEntryPoint, public ARA::IPlugInEntryPoint2
{
public:
    TestVST3Processor ();
    ~TestVST3Processor () SMTG_OVERRIDE;

    // Class IDs
    static const FUID getClassFUID ()
    {
        return FUID (0xcb347e94, 0xdf4f42cf, 0x902ef651, 0x30cf41db);
    }

    static const FUID getEditControllerClassFUID ()
    {
        return FUID (0xcbe26d00, 0x67da43e8, 0x912c1248, 0xd10e602b);
    }

    // Create functions
    static FUnknown* createInstance (void* /*context*/)
    {
        return static_cast<IAudioProcessor*> (new TestVST3Processor);
    }

    static FUnknown* createEditControllerInstance (void* /*context*/)
    {
        return TestVST3Controller::createInstance (nullptr);
    }

    //------------------------------------------------------------------------
    // AudioEffect overrides:
    //------------------------------------------------------------------------
    /** Called at first after constructor */
    tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;

    /** Called at the end before destructor */
    tresult PLUGIN_API terminate () SMTG_OVERRIDE;

    /** Switch the Plug-in on/off */
    tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;

    /** Will be called before any process call */
    tresult PLUGIN_API setupProcessing (ProcessSetup& newSetup) SMTG_OVERRIDE;

    /** Try to set (host => plug-in) a wanted arrangement for inputs and outputs. */
    tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                      SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE;

    /** Asks if a given sample size is supported see SymbolicSampleSizes. */
    tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) SMTG_OVERRIDE;

    /** Here we go...the process call */
    tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE;


    //------------------------------------------------------------------------
    // ARA::IPlugInEntryPoint2 overrides:
    //------------------------------------------------------------------------

    /** Get associated ARA factory */
    const ARA::ARAFactory* PLUGIN_API getFactory () SMTG_OVERRIDE;

    /** Bind to ARA document controller instance */
    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentController (ARA::ARADocumentControllerRef documentControllerRef) SMTG_OVERRIDE;
    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef,
                                                    ARA::ARAPlugInInstanceRoleFlags knownRoles, ARA::ARAPlugInInstanceRoleFlags assignedRoles) SMTG_OVERRIDE;

    //------------------------------------------------------------------------
    // Interface
    //------------------------------------------------------------------------
    OBJ_METHODS (TestVST3Processor, AudioEffect)
        DEFINE_INTERFACES
        DEF_INTERFACE (IPlugInEntryPoint)
        DEF_INTERFACE (IPlugInEntryPoint2)
        END_DEFINE_INTERFACES (AudioEffect)
        REFCOUNT_METHODS (AudioEffect)

//------------------------------------------------------------------------
protected:
    ARA::PlugIn::PlugInExtension _araPlugInExtension;

    // Auto-Leveler State for DOP / UI
    float m_sampleRate = 44100.0f;
    float m_silenceThreshDb = -45.0f;
    float m_silenceGapMs = 300.0f;
    float m_targetPeakDb = -3.0f;

    float m_rmsWindowPower = 0.0f;
    float m_currentGainDb = 0.0f;
};

} // namespace Vst
} // namespace Steinberg

ARA_DISABLE_VST3_WARNINGS_END
