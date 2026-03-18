//------------------------------------------------------------------------------
//! \file       TestVST3Controller.h
//!             Custom EditController for AutoSplitLeveler
//------------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace Steinberg {
namespace Vst {

enum {
    kSilenceThreshId = 100,
    kSilenceGapId = 101,
    kTargetPeakId = 102,
    kTriggerSplitId = 103,
    kTriggerLevelingId = 104
};

class TestVST3Controller : public EditController, public VSTGUI::VST3EditorDelegate
{
public:
    tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
    tresult PLUGIN_API terminate () SMTG_OVERRIDE;

    IPlugView* PLUGIN_API createView (FIDString name) SMTG_OVERRIDE;
    tresult PLUGIN_API setParamNormalized (ParamID tag, ParamValue value) SMTG_OVERRIDE;

    static FUnknown* createInstance (void* /*context*/)
    {
        return static_cast<IEditController*> (new TestVST3Controller);
    }

    // VSTGUI::VST3EditorDelegate
    VSTGUI::IController* createSubController (VSTGUI::UTF8StringPtr name, const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;
};

} // namespace Vst
} // namespace Steinberg
