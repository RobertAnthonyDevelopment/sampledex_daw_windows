/*
    Full DAW scaffold state tracker.
    Keeps milestone focus and module readiness in one place.
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class DawScaffold
{
public:
    struct RuntimeSignals
    {
        bool noRecordPolicy = false;
        bool hasEditSession = false;
        bool hasTracks = false;
        bool hasAnyClip = false;
        bool hasAnyMidiClip = false;
        bool hasTimelineTools = false;
        bool hasArrangementTools = false;
        bool hasMixerTools = false;
        bool hasQuantizeTools = false;
        bool hasFileWorkflow = false;
    };

    DawScaffold();

    void update (const RuntimeSignals& signals);

    juce::String getFocusMilestoneCode() const;
    juce::String getFocusMilestoneName() const;
    juce::String buildReport() const;

private:
    struct ModuleState
    {
        juce::String key;
        juce::String title;
        juce::String why;
        bool ready = false;
        juce::String evidence;
    };

    enum class Milestone
    {
        m1NoRecording,
        m2EditingCore,
        m3TimelineTrackArea,
        m4DeepEditing,
        m5ProductionPolish
    };

    std::vector<ModuleState> modules;
    RuntimeSignals state;
    Milestone focusMilestone = Milestone::m1NoRecording;

    void updateModules();
    void updateFocusMilestone();
    static juce::String boolMark (bool value);
};

