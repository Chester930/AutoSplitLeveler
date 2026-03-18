#include "TestVST3Controller.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/base/fstrdefs.h"
#include <cstring>

namespace Steinberg {
namespace Vst {

tresult PLUGIN_API TestVST3Controller::initialize(FUnknown* context)
{
    tresult result = EditController::initialize(context);
    if (result == kResultTrue)
    {
        //---Parameters---------
        parameters.addParameter(STR16("Silence Threshold"), STR16("dB"), 0, -45.0f/96.0f + 1.0f, ParameterInfo::kCanAutomate, kSilenceThreshId);
        parameters.addParameter(STR16("Min Silence Duration"), STR16("ms"), 0, 300.0f/2000.0f, ParameterInfo::kCanAutomate, kSilenceGapId);
        parameters.addParameter(STR16("Target Peak"), STR16("dB"), 0, -3.0f/24.0f + 1.0f, ParameterInfo::kCanAutomate, kTargetPeakId);
        
        // Actions (momentary buttons)
        parameters.addParameter(STR16("Trigger Split"), STR16(""), 0, 0, ParameterInfo::kNoFlags, kTriggerSplitId);
        parameters.addParameter(STR16("Trigger Leveling"), STR16(""), 0, 0, ParameterInfo::kNoFlags, kTriggerLevelingId);
    }
    return result;
}

tresult PLUGIN_API TestVST3Controller::terminate()
{
    return EditController::terminate();
}

IPlugView* PLUGIN_API TestVST3Controller::createView(FIDString name)
{
    if (name && strcmp(name, Vst::ViewType::kEditor) == 0)
    {
        // For now, we use a generic XML based VST3Editor if a uidesc is provided.
        // In a real project, we'd load a .uidesc file here.
        auto* editor = new VSTGUI::VST3Editor(this, "view", "autosplitleveler.uidesc");
        return editor;
    }
    return nullptr;
}

tresult PLUGIN_API TestVST3Controller::setParamNormalized(ParamID tag, ParamValue value)
{
    // Handle action triggers
    if (tag == kTriggerSplitId && value > 0.5)
    {
        // Trigger Function 1 logic via processor or ARA extension
    }
    return EditController::setParamNormalized(tag, value);
}

VSTGUI::IController* TestVST3Controller::createSubController(VSTGUI::UTF8StringPtr /*name*/, const VSTGUI::IUIDescription* /*description*/, VSTGUI::VST3Editor* /*editor*/)
{
    return nullptr;
}

} // namespace Vst
} // namespace Steinberg
