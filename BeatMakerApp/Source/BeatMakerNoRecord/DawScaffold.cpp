#include "DawScaffold.h"

namespace
{
juce::String getFocusGoal (int focusIndex)
{
    switch (focusIndex)
    {
        case 0: return "Ensure no recording path is active at runtime and in UI.";
        case 1: return "Strengthen core editing workflow coverage and reliability.";
        case 2: return "Improve timeline precision and track area navigation.";
        case 3: return "Add deeper MIDI/audio editing capabilities.";
        default: return "Polish workflow speed, feedback, and stability for daily production use.";
    }
}

}

DawScaffold::DawScaffold()
{
    modules =
    {
        { "session", "Edit Session Foundation", "Edit lifecycle must be stable before deeper features.", false, "Waiting for edit creation/load state." },
        { "no_record", "No-Recording Enforcement", "Beatmaker must never enable microphone/recording paths.", false, "Waiting for runtime no-record verification." },
        { "arrangement", "Arrangement Editing", "Track-level arrangement actions drive beat structure.", false, "Waiting for tracks and arrangement tools." },
        { "clip", "Clip Editing", "Clip edit operations are core to beat chopping and placement.", false, "Waiting for clips and clip tool coverage." },
        { "timeline", "Timeline + Track Area", "Navigation precision controls productivity on dense edits.", false, "Waiting for timeline and viewport tools." },
        { "mixer", "Mixer Controls", "Basic mixer controls are needed for rough balancing inside arrangement.", false, "Waiting for track mixer controls." },
        { "midi", "MIDI Editing Depth", "MIDI editing quality is essential for beat programming.", false, "Waiting for MIDI clips and quantize tooling." },
        { "file_io", "File Workflow", "Open/save flow must be stable for iterative production.", false, "Waiting for save/open workflow confirmation." }
    };
}

void DawScaffold::update (const RuntimeSignals& signals)
{
    state = signals;
    updateModules();
    updateFocusMilestone();
}

juce::String DawScaffold::getFocusMilestoneCode() const
{
    switch (focusMilestone)
    {
        case Milestone::m1NoRecording:      return "M1";
        case Milestone::m2EditingCore:      return "M2";
        case Milestone::m3TimelineTrackArea:return "M3";
        case Milestone::m4DeepEditing:      return "M4";
        case Milestone::m5ProductionPolish: return "M5";
    }

    return "M1";
}

juce::String DawScaffold::getFocusMilestoneName() const
{
    switch (focusMilestone)
    {
        case Milestone::m1NoRecording:       return "Playback-only policy";
        case Milestone::m2EditingCore:       return "Core editing capabilities";
        case Milestone::m3TimelineTrackArea: return "Timeline and track area";
        case Milestone::m4DeepEditing:       return "Deep MIDI/audio editing";
        case Milestone::m5ProductionPolish:  return "Production polish";
    }

    return "Playback-only policy";
}

juce::String DawScaffold::buildReport() const
{
    juce::String report;
    report << "Focus milestone: " << getFocusMilestoneCode() << " " << getFocusMilestoneName() << "\n";
    report << "Goal: " << getFocusGoal ((int) focusMilestone) << "\n\n";
    report << "Module readiness\n";

    for (const auto& module : modules)
    {
        report << "- [" << boolMark (module.ready) << "] " << module.title << "\n";
        report << "  Why: " << module.why << "\n";
        report << "  Evidence: " << module.evidence << "\n";
    }

    report << "\nRuntime signals\n";
    report << "- No-recording policy: " << boolMark (state.noRecordPolicy) << "\n";
    report << "- Edit session: " << boolMark (state.hasEditSession) << "\n";
    report << "- Tracks available: " << boolMark (state.hasTracks) << "\n";
    report << "- Any clip present: " << boolMark (state.hasAnyClip) << "\n";
    report << "- Any MIDI clip present: " << boolMark (state.hasAnyMidiClip) << "\n";
    report << "- Timeline tools: " << boolMark (state.hasTimelineTools) << "\n";
    report << "- Arrangement tools: " << boolMark (state.hasArrangementTools) << "\n";
    report << "- Mixer tools: " << boolMark (state.hasMixerTools) << "\n";
    report << "- Quantize tools: " << boolMark (state.hasQuantizeTools) << "\n";
    report << "- File workflow: " << boolMark (state.hasFileWorkflow) << "\n";

    return report;
}

void DawScaffold::updateModules()
{
    for (auto& module : modules)
    {
        if (module.key == "session")
        {
            module.ready = state.hasEditSession;
            module.evidence = state.hasEditSession ? "Edit object is active." : "No active edit.";
            continue;
        }

        if (module.key == "no_record")
        {
            module.ready = state.noRecordPolicy;
            module.evidence = state.noRecordPolicy ? "Input channels, MIDI input, and record UI are disabled." : "No-record guard failed.";
            continue;
        }

        if (module.key == "arrangement")
        {
            module.ready = state.hasEditSession && state.hasTracks && state.hasArrangementTools;
            module.evidence = module.ready ? "Tracks exist and arrangement tools are available." : "Need edit session, tracks, and arrangement tools.";
            continue;
        }

        if (module.key == "clip")
        {
            module.ready = state.hasEditSession && state.hasAnyClip;
            module.evidence = module.ready ? "Project has clip content for clip-edit workflows." : "Create/import clips to activate clip workflows.";
            continue;
        }

        if (module.key == "timeline")
        {
            module.ready = state.hasEditSession && state.hasTimelineTools;
            module.evidence = module.ready ? "Timeline ruler, grid, zoom, and scroll controls are active." : "Timeline/viewport tools not ready.";
            continue;
        }

        if (module.key == "mixer")
        {
            module.ready = state.hasEditSession && state.hasTracks && state.hasMixerTools;
            module.evidence = module.ready ? "Track mute/solo/volume/pan controls are active." : "Need edit session and track mixer controls.";
            continue;
        }

        if (module.key == "midi")
        {
            module.ready = state.hasEditSession && state.hasAnyMidiClip && state.hasQuantizeTools;
            module.evidence = module.ready ? "MIDI clips exist and quantize tooling is active." : "Need MIDI clip content plus quantize tooling.";
            continue;
        }

        if (module.key == "file_io")
        {
            module.ready = state.hasFileWorkflow;
            module.evidence = state.hasFileWorkflow ? "New/Open/Save controls are available." : "File workflow controls are not ready.";
        }
    }
}

void DawScaffold::updateFocusMilestone()
{
    const auto isReady = [this] (const juce::String& key)
    {
        for (const auto& module : modules)
            if (module.key == key)
                return module.ready;

        return false;
    };

    if (! isReady ("no_record"))
    {
        focusMilestone = Milestone::m1NoRecording;
        return;
    }

    if (! (isReady ("session")
        && isReady ("arrangement")
        && isReady ("clip")
        && isReady ("mixer")
        && isReady ("file_io")))
    {
        focusMilestone = Milestone::m2EditingCore;
        return;
    }

    if (! isReady ("timeline"))
    {
        focusMilestone = Milestone::m3TimelineTrackArea;
        return;
    }

    if (! isReady ("midi"))
    {
        focusMilestone = Milestone::m4DeepEditing;
        return;
    }

    focusMilestone = Milestone::m5ProductionPolish;
}

juce::String DawScaffold::boolMark (bool value)
{
    return value ? "OK" : "TODO";
}
