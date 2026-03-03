/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

namespace te = tracktion;
using namespace std::literals;

namespace
{
juce::Colour getEditBackground()
{
    return juce::Colour::fromRGB (17, 22, 30);
}

juce::Colour getPanelBackground (bool rightPanel)
{
    return rightPanel ? juce::Colour::fromRGB (23, 31, 43)
                      : juce::Colour::fromRGB (25, 34, 46);
}

juce::Colour getTrackLaneBase (bool oddLane)
{
    return oddLane ? juce::Colour::fromRGB (35, 42, 54)
                   : juce::Colour::fromRGB (28, 35, 46);
}

juce::Colour getSelectionAccent()
{
    return juce::Colour::fromRGB (92, 190, 255);
}

te::TimeDuration getSixteenthDurationAt (const te::Edit& edit, te::TimePosition at)
{
    const auto bps = juce::jmax (1.0e-4, edit.tempoSequence.getBeatsPerSecondAt (at, true));
    return te::TimeDuration::fromSeconds (0.25 / bps);
}

te::TimeDuration getOneBarDurationAt (const te::Edit& edit, te::TimePosition at)
{
    const auto bps = juce::jmax (1.0e-4, edit.tempoSequence.getBeatsPerSecondAt (at, true));
    const auto& sig = edit.tempoSequence.getTimeSigAt (at);
    const auto numerator = juce::jmax (1, sig.numerator.get());
    const auto denominator = juce::jmax (1, sig.denominator.get());
    const double beatsPerBar = (double) numerator * 4.0 / (double) denominator;
    return te::TimeDuration::fromSeconds (beatsPerBar / bps);
}

int getBeatsPerBarInt (const te::Edit& edit, te::TimePosition at)
{
    const auto& sig = edit.tempoSequence.getTimeSigAt (at);
    const auto numerator = juce::jmax (1, sig.numerator.get());
    const auto denominator = juce::jmax (1, sig.denominator.get());
    return juce::jmax (1, juce::roundToInt ((double) numerator * 4.0 / (double) denominator));
}

TimelineEditTool timelineEditTool = TimelineEditTool::select;
std::function<void (te::MidiClip&)> midiClipDoubleClickHandler;
const juce::Identifier laneHeightPropertyId ("sampledexLaneHeight");
constexpr int minLaneHeightPx = 28;
constexpr int maxLaneHeightPx = 320;
constexpr int laneResizeHandleHeightPx = 7;

int getLaneHeightForTrack (te::Track& track, EditViewState& editViewState)
{
    const int globalHeight = juce::jlimit (minLaneHeightPx, maxLaneHeightPx, roundToInt (editViewState.trackHeight.get()));

    if (! track.state.hasProperty (laneHeightPropertyId))
        return globalHeight;

    const int laneHeight = (int) track.state.getProperty (laneHeightPropertyId, globalHeight);
    return juce::jlimit (minLaneHeightPx, maxLaneHeightPx, laneHeight);
}

bool isNearLaneResizeHandle (const juce::Component& component, int localY)
{
    return localY >= component.getHeight() - laneResizeHandleHeightPx;
}

void setTrackLaneHeight (te::Track& track, int heightPx)
{
    const int clamped = juce::jlimit (minLaneHeightPx, maxLaneHeightPx, heightPx);
    if ((int) track.state.getProperty (laneHeightPropertyId, 0) != clamped)
        track.state.setProperty (laneHeightPropertyId, clamped, nullptr);
}
}

TimelineEditTool getTimelineEditTool()
{
    return timelineEditTool;
}

void setTimelineEditTool (TimelineEditTool tool)
{
    timelineEditTool = tool;
}

juce::String getTimelineEditToolName (TimelineEditTool tool)
{
    switch (tool)
    {
        case TimelineEditTool::pencil:   return "Pencil";
        case TimelineEditTool::scissors: return "Scissors";
        case TimelineEditTool::resize:   return "Resize";
        case TimelineEditTool::select:
        default:                         return "Select";
    }
}

void setMidiClipDoubleClickHandler (std::function<void (te::MidiClip&)> handler)
{
    midiClipDoubleClickHandler = std::move (handler);
}

//==============================================================================
class PluginTreeBase
{
public:
    virtual ~PluginTreeBase() = default;
    virtual String getUniqueName() const = 0;

    void addSubItem (PluginTreeBase* itm)   { subitems.add (itm);       }
    int getNumSubItems()                    { return subitems.size();   }
    PluginTreeBase* getSubItem (int idx)    { return subitems[idx];     }

private:
    OwnedArray<PluginTreeBase> subitems;
};

//==============================================================================
class PluginTreeItem : public PluginTreeBase
{
public:
    PluginTreeItem (const PluginDescription&);
    PluginTreeItem (const String& uniqueId, const String& name, const String& xmlType, bool isSynth, bool isPlugin);

    te::Plugin::Ptr create (te::Edit&);

    String getUniqueName() const override
    {
        if (desc.fileOrIdentifier.startsWith (te::RackType::getRackPresetPrefix()))
            return desc.fileOrIdentifier;

        return desc.createIdentifierString();
    }

    PluginDescription desc;
    String xmlType;
    bool isPlugin = true;

    JUCE_LEAK_DETECTOR (PluginTreeItem)
};

//==============================================================================
class PluginTreeGroup : public PluginTreeBase
{
public:
    PluginTreeGroup (te::Edit&, KnownPluginList::PluginTree&, te::Plugin::Type);
    PluginTreeGroup (const String&);

    String getUniqueName() const override           { return name; }

    String name;

private:
    void populateFrom (KnownPluginList::PluginTree&);
    void createBuiltInItems (int& num, te::Plugin::Type);

    JUCE_LEAK_DETECTOR (PluginTreeGroup)
};

//==============================================================================
PluginTreeItem::PluginTreeItem (const juce::PluginDescription& d)
    : desc (d), xmlType (te::ExternalPlugin::xmlTypeName), isPlugin (true)
{
    jassert (xmlType.isNotEmpty());
}

PluginTreeItem::PluginTreeItem (const juce::String& uniqueId, const juce::String& name,
                                const juce::String& xmlType_, bool isSynth, bool isPlugin_)
    : xmlType (xmlType_), isPlugin (isPlugin_)
{
    jassert (xmlType.isNotEmpty());
    desc.name = name;
    desc.fileOrIdentifier = uniqueId;
    desc.pluginFormatName = (uniqueId.endsWith ("_trkbuiltin") || xmlType == te::RackInstance::xmlTypeName)
                                ? juce::String (te::PluginManager::builtInPluginFormatName) : juce::String();
    desc.category = xmlType;
    desc.isInstrument = isSynth;
}

te::Plugin::Ptr PluginTreeItem::create (te::Edit& ed)
{
    return ed.getPluginCache().createNewPlugin (xmlType, desc);
}

//==============================================================================
PluginTreeGroup::PluginTreeGroup (te::Edit& edit, KnownPluginList::PluginTree& tree, te::Plugin::Type types)
    : name ("Plugins")
{
    {
        int num = 1;

        auto builtinFolder = new PluginTreeGroup (TRANS("Builtin Plugins"));
        addSubItem (builtinFolder);
        builtinFolder->createBuiltInItems (num, types);
    }

    {
        auto racksFolder = new PluginTreeGroup (TRANS("Plugin Racks"));
        addSubItem (racksFolder);

        racksFolder->addSubItem (new PluginTreeItem (String (te::RackType::getRackPresetPrefix()) + "-1",
                                                     TRANS("Create New Empty Rack"),
                                                     te::RackInstance::xmlTypeName, false, false));

        int i = 0;
        for (auto rf : edit.getRackList().getTypes())
            racksFolder->addSubItem (new PluginTreeItem ("RACK__" + String (i++), rf->rackName,
                                                         te::RackInstance::xmlTypeName, false, false));
    }

    populateFrom (tree);
}

PluginTreeGroup::PluginTreeGroup (const String& s)  : name (s)
{
    jassert (name.isNotEmpty());
}

void PluginTreeGroup::populateFrom (KnownPluginList::PluginTree& tree)
{
    for (auto subTree : tree.subFolders)
    {
        if (subTree->plugins.size() > 0 || subTree->subFolders.size() > 0)
        {
            auto fs = new PluginTreeGroup (subTree->folder);
            addSubItem (fs);

            fs->populateFrom (*subTree);
        }
    }

    for (const auto& pd : tree.plugins)
        addSubItem (new PluginTreeItem (pd));
}


template<class FilterClass>
void addInternalPlugin (PluginTreeBase& item, int& num, bool synth = false)
{
    item.addSubItem (new PluginTreeItem (String (num++) + "_trkbuiltin",
                                         TRANS (FilterClass::getPluginName()),
                                         FilterClass::xmlTypeName, synth, false));
}

void PluginTreeGroup::createBuiltInItems (int& num, te::Plugin::Type types)
{
    addInternalPlugin<te::VolumeAndPanPlugin> (*this, num);
    addInternalPlugin<te::LevelMeterPlugin> (*this, num);
    addInternalPlugin<te::EqualiserPlugin> (*this, num);
    addInternalPlugin<te::ReverbPlugin> (*this, num);
    addInternalPlugin<te::DelayPlugin> (*this, num);
    addInternalPlugin<te::ChorusPlugin> (*this, num);
    addInternalPlugin<te::PhaserPlugin> (*this, num);
    addInternalPlugin<te::CompressorPlugin> (*this, num);
    addInternalPlugin<te::PitchShiftPlugin> (*this, num);
    addInternalPlugin<te::LowPassPlugin> (*this, num);
    addInternalPlugin<te::MidiModifierPlugin> (*this, num);
    addInternalPlugin<te::MidiPatchBayPlugin> (*this, num);
    addInternalPlugin<te::PatchBayPlugin> (*this, num);
    addInternalPlugin<te::AuxSendPlugin> (*this, num);
    addInternalPlugin<te::AuxReturnPlugin> (*this, num);
    addInternalPlugin<te::TextPlugin> (*this, num);
    addInternalPlugin<te::FreezePointPlugin> (*this, num);

   #if TRACKTION_ENABLE_REWIRE
    addInternalPlugin<te::ReWirePlugin> (*this, num, true);
   #endif

    if (types == te::Plugin::Type::allPlugins)
    {
        addInternalPlugin<te::SamplerPlugin> (*this, num, true);
        addInternalPlugin<te::FourOscPlugin> (*this, num, true);
    }

    addInternalPlugin<te::InsertPlugin> (*this, num);

   #if ENABLE_INTERNAL_PLUGINS
    for (auto& d : PluginTypeBase::getAllPluginDescriptions())
        if (isPluginAuthorised (d))
            addSubItem (new PluginTreeItem (d));
   #endif
}

//==============================================================================
class PluginMenu : public PopupMenu
{
public:
    PluginMenu() = default;

    PluginMenu (PluginTreeGroup& node, std::function<void (PluginTreeItem*)> callback = nullptr)
    {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*> (node.getSubItem (i)))
                addSubMenu (subNode->name, PluginMenu (*subNode, callback), true);

        for (int i = 0; i < node.getNumSubItems(); ++i)
        {
            if (auto subType = dynamic_cast<PluginTreeItem*> (node.getSubItem (i)))
            {
                if (callback)
                    addItem (subType->desc.name, [subType, callback] { callback (subType); });
                else
                    addItem (subType->getUniqueName().hashCode(), subType->desc.name, true, false);
            }
        }
    }

    static PluginTreeItem* findType (PluginTreeGroup& node, int hash)
    {
        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto subNode = dynamic_cast<PluginTreeGroup*> (node.getSubItem (i)))
                if (auto* t = findType (*subNode, hash))
                    return t;

        for (int i = 0; i < node.getNumSubItems(); ++i)
            if (auto t = dynamic_cast<PluginTreeItem*> (node.getSubItem (i)))
                if (t->getUniqueName().hashCode() == hash)
                    return t;

        return nullptr;
    }

    PluginTreeItem* runMenu (PluginTreeGroup& node)
    {
        int res = show();

        if (res == 0)
            return nullptr;

        return findType (node, res);
    }
};

//==============================================================================
inline te::Plugin::Ptr showMenuAndCreatePlugin (te::Edit& edit)
{
    if (auto tree = EngineHelpers::createPluginTree (edit.engine))
    {
        PluginTreeGroup root (edit, *tree, te::Plugin::Type::allPlugins);
        PluginMenu m (root);

        if (auto type = m.runMenu (root))
            return type->create (edit);
    }

    return {};
}

inline void showMenuAndCreatePluginAsync (te::Track::Ptr destTrack, int index,
                                          std::function<void (te::Plugin::Ptr)> onInserted)
{
    if (auto tree = std::shared_ptr<juce::KnownPluginList::PluginTree> (EngineHelpers::createPluginTree (destTrack->edit.engine)))
    {
        auto root = std::make_shared<PluginTreeGroup> (destTrack->edit, *tree, te::Plugin::Type::allPlugins);
        PluginMenu m (*root,
                      [tree, root, destTrack, index, onInserted] (PluginTreeItem* selectedItem)
                      {
                          if (auto newPlugin = selectedItem->create (destTrack->edit))
                          {
                              destTrack->pluginList.insertPlugin (newPlugin, index, nullptr);

                              if (onInserted)
                                  onInserted (newPlugin);
                          }
                      });
        m.showMenuAsync({});
    }
}


//==============================================================================
ClipComponent::ClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : editViewState (evs), clip (c)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ClipComponent::paint (Graphics& g)
{
    const auto clipBase = clip->getColour().withSaturation (0.60f).withBrightness (0.80f);
    juce::ColourGradient grad (clipBase.withAlpha (0.86f), 0.0f, 0.0f,
                               clipBase.darker (0.30f).withAlpha (0.92f), 0.0f, (float) getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f, 1.0f), 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawHorizontalLine (2, 4.0f, (float) getWidth() - 4.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f, 1.0f), 3.0f, 1.0f);

    if (editViewState.selectionManager.isSelected (clip.get()))
    {
        g.setColour (juce::Colour::fromRGB (84, 183, 255).withAlpha (0.95f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f, 1.5f), 3.0f, 2.0f);
    }

    if (getWidth() > 44)
    {
        auto textArea = getLocalBounds().reduced (6, 3);
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.drawText (clip->getName(), textArea.translated (1, 1), juce::Justification::topLeft, true);
        g.setColour (juce::Colours::white.withAlpha (0.90f));
        g.drawText (clip->getName(), textArea, juce::Justification::topLeft, true);
    }
}

void ClipComponent::mouseDown (const MouseEvent& e)
{
    editViewState.selectionManager.selectOnly (clip.get());

    const auto clipRange = clip->getEditTimeRange();
    const double clickNorm = juce::jlimit (0.0, 1.0, (double) e.x / juce::jmax (1, getWidth()));
    const auto clickTime = te::TimePosition::fromSeconds (clipRange.getStart().inSeconds() + clipRange.getLength().inSeconds() * clickNorm);
    clip->edit.getTransport().setPosition (clickTime);

    if (! e.mods.isPopupMenu())
    {
        const auto activeTool = getTimelineEditTool();
        const auto splitGuard = juce::jmax (0.01, getSixteenthDurationAt (clip->edit, clickTime).inSeconds() * 0.45);

        if (activeTool == TimelineEditTool::scissors)
        {
            const bool canSplit = clickTime.inSeconds() > clipRange.getStart().inSeconds() + splitGuard
                               && clickTime.inSeconds() < clipRange.getEnd().inSeconds() - splitGuard;

            if (canSplit)
            {
                auto splitSelection = te::splitClips ({ clip.get() }, clickTime);
                if (splitSelection.isNotEmpty())
                    editViewState.selectionManager.select (splitSelection);
            }

            dragMode = DragMode::none;
            updateMouseCursorForLocalX (e.x, e.mods.isShiftDown());
            return;
        }

        if (activeTool == TimelineEditTool::select && e.getNumberOfClicks() > 1)
        {
            juce::AlertWindow w ("Rename Clip", "Enter a new clip name", juce::AlertWindow::NoIcon);
            w.addTextEditor ("name", clip->getName());
            w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            if (w.runModalLoop() == 1)
            {
                const auto newName = w.getTextEditorContents ("name").trim();
                if (newName.isNotEmpty())
                    clip->setName (newName);
            }

            dragMode = DragMode::none;
            updateMouseCursorForLocalX (e.x, e.mods.isShiftDown());
            return;
        }

        dragOriginalStartSeconds = clipRange.getStart().inSeconds();
        dragOriginalEndSeconds = clipRange.getEnd().inSeconds();

        if (activeTool == TimelineEditTool::pencil)
        {
            dragMode = DragMode::resizeEnd;
        }
        else if (activeTool == TimelineEditTool::resize)
        {
            dragMode = isNearClipStartHandle (e.x) ? DragMode::resizeStart : DragMode::resizeEnd;
        }
        else
        {
            if (isNearClipStartHandle (e.x))
                dragMode = DragMode::resizeStart;
            else if (isNearClipEndHandle (e.x))
                dragMode = DragMode::resizeEnd;
            else
                dragMode = DragMode::move;
        }

        updateMouseCursorForLocalX (e.x, e.mods.isShiftDown());
        return;
    }

    auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get());

    PopupMenu m;
    m.addSectionHeader ("Clip");
    m.addItem (1, "Rename...");
    m.addItem (2, "Duplicate");
    m.addItem (3, "Delete");
    m.addItem (4, "Split At Cursor");
    m.addItem (5, "Trim Start To Cursor");
    m.addItem (6, "Trim End To Cursor");
    m.addItem (7, "Nudge Left (1/16)");
    m.addItem (8, "Nudge Right (1/16)");
    m.addSeparator();
    m.addItem (9, "Loop Transport To Clip");
    m.addItem (10, clip->isLooping() ? "Disable Clip Looping" : "Enable Clip Looping", clip->canLoop(), false);

    if (audioClip != nullptr)
    {
        m.addSeparator();
        m.addSectionHeader ("Audio");
        m.addItem (11, "Gain +1 dB");
        m.addItem (12, "Gain -1 dB");
        m.addItem (13, "Fade In (1/16)");
        m.addItem (14, "Fade Out (1/16)");
        m.addItem (15, "Clear Fades");
        m.addItem (16, audioClip->getIsReversed() ? "Reverse: Off" : "Reverse: On");
        m.addItem (17, audioClip->getAutoTempo() ? "Auto Tempo: Off" : "Auto Tempo: On");
        m.addItem (18, audioClip->getWarpTime() ? "Warp: Off" : "Warp: On");
    }

    const int res = m.showMenu (PopupMenu::Options().withTargetComponent (this));
    if (res == 0)
        return;

    if (res == 1)
    {
        juce::AlertWindow w ("Rename Clip", "Enter a new clip name", juce::AlertWindow::NoIcon);
        w.addTextEditor ("name", clip->getName());
        w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        if (w.runModalLoop() == 1)
        {
            const auto newName = w.getTextEditorContents ("name").trim();
            if (newName.isNotEmpty())
                clip->setName (newName);
        }
    }
    else if (res == 2)
    {
        if (auto duplicated = te::duplicateClip (*clip))
        {
            const auto sourcePosition = clip->getPosition();
            duplicated->setPosition ({ { sourcePosition.getEnd(), sourcePosition.getLength() }, sourcePosition.getOffset() });
            editViewState.selectionManager.selectOnly (duplicated.get());
        }
    }
    else if (res == 3)
    {
        if (juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                                "Delete Clip",
                                                "Delete selected clip \"" + clip->getName() + "\"?",
                                                "Delete",
                                                "Cancel",
                                                this))
            clip->removeFromParent();
    }
    else if (res == 4)
    {
        auto splitSelection = te::splitClips ({ clip.get() }, clickTime);
        if (splitSelection.isNotEmpty())
            editViewState.selectionManager.select (splitSelection);
    }
    else if (res == 5)
    {
        if (clickTime > clipRange.getStart() && clickTime < clipRange.getEnd())
            clip->setStart (clickTime, true, false);
    }
    else if (res == 6)
    {
        if (clickTime > clipRange.getStart() && clickTime < clipRange.getEnd())
            clip->setEnd (clickTime, true);
    }
    else if (res == 7 || res == 8)
    {
        const auto position = clip->getPosition();
        const auto step = getSixteenthDurationAt (clip->edit, clickTime);
        const double deltaSeconds = (res == 8 ? 1.0 : -1.0) * step.inSeconds();
        const double newStartSeconds = juce::jmax (0.0, position.getStart().inSeconds() + deltaSeconds);
        clip->setPosition ({ { te::TimePosition::fromSeconds (newStartSeconds), position.getLength() }, position.getOffset() });
    }
    else if (res == 9)
    {
        auto& transport = clip->edit.getTransport();
        transport.setLoopRange (clip->getEditTimeRange());
        transport.looping = true;
    }
    else if (res == 10)
    {
        if (clip->isLooping())
            clip->disableLooping();
        else
            clip->setNumberOfLoops (4);
    }
    else if (audioClip != nullptr && res == 11)
    {
        audioClip->setGainDB (juce::jlimit (-60.0f, 24.0f, audioClip->getGainDB() + 1.0f));
    }
    else if (audioClip != nullptr && res == 12)
    {
        audioClip->setGainDB (juce::jlimit (-60.0f, 24.0f, audioClip->getGainDB() - 1.0f));
    }
    else if (audioClip != nullptr && res == 13)
    {
        audioClip->setFadeIn (getSixteenthDurationAt (clip->edit, clickTime));
    }
    else if (audioClip != nullptr && res == 14)
    {
        audioClip->setFadeOut (getSixteenthDurationAt (clip->edit, clickTime));
    }
    else if (audioClip != nullptr && res == 15)
    {
        audioClip->setFadeIn (te::TimeDuration());
        audioClip->setFadeOut (te::TimeDuration());
    }
    else if (audioClip != nullptr && res == 16)
    {
        audioClip->setIsReversed (! audioClip->getIsReversed());
        audioClip->reverseLoopPoints();
    }
    else if (audioClip != nullptr && res == 17)
    {
        audioClip->setAutoTempo (! audioClip->getAutoTempo());
    }
    else if (audioClip != nullptr && res == 18)
    {
        audioClip->setWarpTime (! audioClip->getWarpTime());
    }
}

void ClipComponent::mouseDrag (const MouseEvent& e)
{
    if (dragMode == DragMode::none)
        return;

    auto* parent = getParentComponent();
    const int parentWidth = juce::jmax (1, parent != nullptr ? parent->getWidth() : getWidth());
    const double visibleSeconds = juce::jmax (0.05, (editViewState.viewX2.get() - editViewState.viewX1.get()).inSeconds());
    const double rawDeltaSeconds = ((double) e.getDistanceFromDragStartX() / (double) parentWidth) * visibleSeconds;
    const double snapSeconds = juce::jmax (0.01,
                                           getSixteenthDurationAt (clip->edit,
                                                                   te::TimePosition::fromSeconds (juce::jmax (0.0, dragOriginalStartSeconds)))
                                               .inSeconds());
    const double snappedDeltaSeconds = std::round (rawDeltaSeconds / snapSeconds) * snapSeconds;
    const double minimumLengthSeconds = juce::jmax (0.02, snapSeconds);
    const double originalLengthSeconds = juce::jmax (minimumLengthSeconds, dragOriginalEndSeconds - dragOriginalStartSeconds);

    switch (dragMode)
    {
        case DragMode::move:
        {
            const auto currentPos = clip->getPosition();
            const double movedStartSeconds = juce::jmax (0.0, dragOriginalStartSeconds + snappedDeltaSeconds);
            clip->setPosition ({ { te::TimePosition::fromSeconds (movedStartSeconds),
                                   te::TimeDuration::fromSeconds (originalLengthSeconds) },
                                 currentPos.getOffset() });
            break;
        }

        case DragMode::resizeStart:
        {
            const double latestStart = dragOriginalEndSeconds - minimumLengthSeconds;
            const double resizedStart = juce::jlimit (0.0, latestStart, dragOriginalStartSeconds + snappedDeltaSeconds);
            clip->setStart (te::TimePosition::fromSeconds (resizedStart), true, false);
            break;
        }

        case DragMode::resizeEnd:
        {
            const double earliestEnd = dragOriginalStartSeconds + minimumLengthSeconds;
            const double resizedEnd = juce::jmax (earliestEnd, dragOriginalEndSeconds + snappedDeltaSeconds);
            clip->setEnd (te::TimePosition::fromSeconds (resizedEnd), true);
            break;
        }

        case DragMode::none:
        default:
            break;
    }

    updateMouseCursorForLocalX (e.x, e.mods.isShiftDown());
}

void ClipComponent::mouseUp (const MouseEvent& e)
{
    dragMode = DragMode::none;
    updateMouseCursorForLocalX (e.x, e.mods.isShiftDown());
}

void ClipComponent::mouseMove (const MouseEvent& e)
{
    updateMouseCursorForLocalX (e.x, e.mods.isShiftDown());
}

void ClipComponent::mouseExit (const MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

bool ClipComponent::isNearClipStartHandle (int x) const
{
    const int handleWidth = juce::jlimit (4, 10, juce::jmax (4, getWidth() / 5));
    return x <= handleWidth;
}

bool ClipComponent::isNearClipEndHandle (int x) const
{
    const int handleWidth = juce::jlimit (4, 10, juce::jmax (4, getWidth() / 5));
    return x >= getWidth() - handleWidth;
}

void ClipComponent::updateMouseCursorForLocalX (int x, bool shiftDown)
{
    juce::ignoreUnused (shiftDown);

    juce::MouseCursor mouseCursor = juce::MouseCursor::NormalCursor;
    const auto tool = getTimelineEditTool();

    if (tool == TimelineEditTool::pencil || tool == TimelineEditTool::scissors)
    {
        mouseCursor = juce::MouseCursor::CrosshairCursor;
    }
    else if (tool == TimelineEditTool::resize)
    {
        mouseCursor = juce::MouseCursor::LeftRightResizeCursor;
    }
    else
    {
        mouseCursor = (isNearClipStartHandle (x) || isNearClipEndHandle (x))
                          ? juce::MouseCursor::LeftRightResizeCursor
                          : juce::MouseCursor::DraggingHandCursor;
    }

    setMouseCursor (mouseCursor);
}

//==============================================================================
AudioClipComponent::AudioClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent (evs, c)
{
    updateThumbnail();
}

void AudioClipComponent::paint (Graphics& g)
{
    ClipComponent::paint (g);

    if (editViewState.drawWaveforms && thumbnail != nullptr)
        drawWaveform (g, *getWaveAudioClip(), *thumbnail, Colours::black.withAlpha (0.5f),
                      0, getWidth(), 0, getHeight(), 0);
}

void AudioClipComponent::drawWaveform (Graphics& g, te::AudioClipBase& c, te::SmartThumbnail& thumb, Colour colour,
                                       int left, int right, int y, int h, int xOffset)
{
    auto getTimeRangeForDrawing = [this] (const int l, const int r) -> tracktion::TimeRange
    {
        if (auto p = getParentComponent())
        {
            auto t1 = editViewState.xToTime (l, p->getWidth());
            auto t2 = editViewState.xToTime (r, p->getWidth());

            return { t1, t2 };
        }

        return {};
    };

    jassert (left <= right);
    const auto gain = c.getGain();
    const auto pan = thumb.getNumChannels() == 1 ? 0.0f : c.getPan();

    const float pv = pan * gain;
    const float gainL = (gain - pv);
    const float gainR = (gain + pv);

    const bool usesTimeStretchedProxy = c.usesTimeStretchedProxy();

    const auto clipPos = c.getPosition();
    auto offset = clipPos.getOffset();
    auto speedRatio = c.getSpeedRatio();

    g.setColour (colour);

    if (usesTimeStretchedProxy)
    {
        const Rectangle<int> area (left + xOffset, y, right - left, h);

        if (! thumb.isOutOfDate())
        {
            drawChannels (g, thumb, area,
                          getTimeRangeForDrawing (left, right),
                          c.isLeftChannelActive(), c.isRightChannelActive(),
                          gainL, gainR);
        }
    }
    else if (c.getLoopLength() == 0s)
    {
        auto region = getTimeRangeForDrawing (left, right);

        auto t1 = (region.getStart() + offset) * speedRatio;
        auto t2 = (region.getEnd()   + offset) * speedRatio;

        drawChannels (g, thumb,
                      { left + xOffset, y, right - left, h },
                      { t1, t2 },
                      c.isLeftChannelActive(), c.isRightChannelActive(),
                      gainL, gainR);
    }
}

void AudioClipComponent::drawChannels (Graphics& g, te::SmartThumbnail& thumb, Rectangle<int> area,
                                       te::TimeRange time, bool useLeft, bool useRight,
                                       float leftGain, float rightGain)
{
    if (useLeft && useRight && thumb.getNumChannels() > 1)
    {
        thumb.drawChannel (g, area.removeFromTop (area.getHeight() / 2), time, 0, leftGain);
        thumb.drawChannel (g, area, time, 1, rightGain);
    }
    else if (useLeft)
    {
        thumb.drawChannel (g, area, time, 0, leftGain);
    }
    else if (useRight)
    {
        thumb.drawChannel (g, area, time, 1, rightGain);
    }
}

void AudioClipComponent::updateThumbnail()
{
    if (auto* wac = getWaveAudioClip())
    {
        te::AudioFile af (wac->getAudioFile());

        if (af.getFile().existsAsFile() || (! wac->usesSourceFile()))
        {
            if (af.isValid())
            {
                const te::AudioFile proxy ((wac->hasAnyTakes() && wac->isShowingTakes()) ? wac->getAudioFile() : wac->getPlaybackFile());

                if (thumbnail == nullptr)
                    thumbnail = std::make_unique<te::SmartThumbnail> (wac->edit.engine, proxy, *this, &wac->edit);
                else
                    thumbnail->setNewFile (proxy);
            }
            else
            {
                thumbnail = nullptr;
            }
        }
    }
}

void drawMidiClip (juce::Graphics& g, te::MidiClip& mc, juce::Rectangle<int> r, te::TimeRange tr)
{
    auto timeToX = [width = r.getWidth(), tr] (auto time)
    {
        return juce::roundToInt (((time - tr.getStart()) * width) / (tr.getLength()));
    };

    for (auto n : mc.getSequence().getNotes())
    {
        auto sBeat = mc.getStartBeat() + toDuration (n->getStartBeat());
        auto eBeat = mc.getStartBeat() + toDuration (n->getEndBeat());

        auto s = mc.edit.tempoSequence.toTime (sBeat);
        auto e = mc.edit.tempoSequence.toTime (eBeat);

        auto t1 = (double) timeToX (s) - r.getX();
        auto t2 = (double) timeToX (e) - r.getX();

        double y = (1.0 - double (n->getNoteNumber()) / 127.0) * r.getHeight();

        g.setColour (Colours::white.withAlpha (n->getVelocity() / 127.0f));
        g.drawLine (float (t1), float (y), float (t2), float (y));
    }
}

//==============================================================================
MidiClipComponent::MidiClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent (evs, c)
{
}

void MidiClipComponent::paint (Graphics& g)
{
    ClipComponent::paint (g);

    if (auto mc = getMidiClip())
    {
        auto& seq = mc->getSequence();
        for (auto n : seq.getNotes())
        {
            auto sBeat = mc->getStartBeat() + toDuration (n->getStartBeat());
            auto eBeat = mc->getStartBeat() + toDuration (n->getEndBeat());

            auto s = editViewState.beatToTime (sBeat);
            auto e = editViewState.beatToTime (eBeat);

            if (auto p = getParentComponent())
            {
                auto t1 = (double) editViewState.timeToX (s, p->getWidth()) - getX();
                auto t2 = (double) editViewState.timeToX (e, p->getWidth()) - getX();

                double y = (1.0 - double (n->getNoteNumber()) / 127.0) * getHeight();

                g.setColour (Colours::white.withAlpha (n->getVelocity() / 127.0f));
                g.drawLine (float (t1), float (y), float (t2), float (y));
            }
        }
    }
}

void MidiClipComponent::mouseDown (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu()
        && e.getNumberOfClicks() > 1)
    {
        if (auto* midiClip = getMidiClip())
        {
            editViewState.selectionManager.selectOnly (midiClip);

            const auto clipRange = midiClip->getEditTimeRange();
            const double clickNorm = juce::jlimit (0.0, 1.0, (double) e.x / juce::jmax (1, getWidth()));
            const auto clickTime = te::TimePosition::fromSeconds (clipRange.getStart().inSeconds()
                                                                   + clipRange.getLength().inSeconds() * clickNorm);
            midiClip->edit.getTransport().setPosition (clickTime);

            if (midiClipDoubleClickHandler)
                midiClipDoubleClickHandler (*midiClip);
        }

        return;
    }

    ClipComponent::mouseDown (e);
}

//==============================================================================
RecordingClipComponent::RecordingClipComponent (te::Track::Ptr t, EditViewState& evs)
    : track (t), editViewState (evs)
{
    startTimerHz (10);
    initialiseThumbnailAndPunchTime();
}

void RecordingClipComponent::initialiseThumbnailAndPunchTime()
{
    if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        for (auto idi : at->edit.getEditInputDevices().getDevicesForTargetTrack (*at))
        {
            punchInTime = idi->getPunchInTime (at->itemID);

            if (idi->getRecordingFile (at->itemID).exists())
                thumbnail = at->edit.engine.getRecordingThumbnailManager().getThumbnailFor (idi->getRecordingFile (at->itemID));
        }
    }
}

void RecordingClipComponent::paint (Graphics& g)
{
    g.fillAll (Colours::red.withAlpha (0.5f));
    g.setColour (Colours::black);
    g.drawRect (getLocalBounds());

    if (editViewState.drawWaveforms)
        drawThumbnail (g, Colours::black.withAlpha (0.5f));
}

void RecordingClipComponent::drawThumbnail (Graphics& g, Colour waveformColour) const
{
    if (thumbnail == nullptr)
        return;

    Rectangle<int> bounds;
    tracktion::TimeRange times;
    getBoundsAndTime (bounds, times);
    auto w = bounds.getWidth();

    if (w > 0 && w < 10000)
    {
        g.setColour (waveformColour);
        thumbnail->thumb->drawChannels (g, bounds, times.getStart().inSeconds(), times.getEnd().inSeconds(), 1.0f);
    }
}

bool RecordingClipComponent::getBoundsAndTime (Rectangle<int>& bounds, tracktion::TimeRange& times) const
{
    auto editTimeToX = [this] (te::TimePosition t)
    {
        if (auto p = getParentComponent())
            return editViewState.timeToX (t, p->getWidth()) - getX();

        return 0;
    };

    auto xToEditTime = [this] (int x)
    {
        if (auto p = getParentComponent())
            return editViewState.xToTime (x + getX(), p->getWidth());

        return te::TimePosition();
    };

    bool hasLooped = false;
    auto& edit = track->edit;

    if (auto epc = edit.getTransport().getCurrentPlaybackContext())
    {
        auto localBounds = getLocalBounds();

        auto timeStarted = thumbnail->punchInTime;
        auto unloopedPos = timeStarted + te::TimeDuration::fromSeconds (thumbnail->thumb->getTotalLength());

        auto t1 = timeStarted;
        auto t2 = unloopedPos;

        if (epc->isLooping() && t2 >= epc->getLoopTimes().getEnd())
        {
            hasLooped = true;

            t1 = jmin (t1, epc->getLoopTimes().getStart());
            t2 = epc->getPosition();

            t1 = jmax (editViewState.viewX1.get(), t1);
            t2 = jmin (editViewState.viewX2.get(), t2);
        }
        else if (edit.recordingPunchInOut)
        {
            const auto in  = thumbnail->punchInTime;
            const auto out = edit.getTransport().getLoopRange().getEnd();

            t1 = jlimit (in, out, t1);
            t2 = jlimit (in, out, t2);
        }

        bounds = localBounds.withX (jmax (localBounds.getX(), editTimeToX (t1)))
                 .withRight (jmin (localBounds.getRight(), editTimeToX (t2)));

        auto loopRange = epc->getLoopTimes();
        const auto recordedTime = unloopedPos - toDuration (epc->getLoopTimes().getStart());
        const int numLoops = (int) (recordedTime / loopRange.getLength());

        const tracktion::TimeRange editTimes (xToEditTime (bounds.getX()),
                                              xToEditTime (bounds.getRight()));

        times = (editTimes + (loopRange.getLength() * numLoops)) - toDuration (timeStarted);
    }

    return hasLooped;
}

void RecordingClipComponent::timerCallback()
{
    updatePosition();
}

void RecordingClipComponent::updatePosition()
{
    auto& edit = track->edit;

    if (auto epc = edit.getTransport().getCurrentPlaybackContext())
    {
        auto t1 = punchInTime >= 0s ? punchInTime : edit.getTransport().getTimeWhenStarted();
        auto t2 = jmax (t1, epc->getUnloopedPosition());

        if (epc->isLooping())
        {
            auto loopTimes = epc->getLoopTimes();

            if (t2 >= loopTimes.getEnd())
            {
                t1 = jmin (t1, loopTimes.getStart());
                t2 = loopTimes.getEnd();
            }
        }
        else if (edit.recordingPunchInOut)
        {
            auto mr = edit.getTransport().getLoopRange();
            auto in  = mr.getStart();
            auto out = mr.getEnd();

            t1 = jlimit (in, out, t1);
            t2 = jlimit (in, out, t2);
        }

        t1 = jmax (t1, editViewState.viewX1.get());
        t2 = jmin (t2, editViewState.viewX2.get());

        if (auto p = getParentComponent())
        {
            int x1 = editViewState.timeToX (t1, p->getWidth());
            int x2 = editViewState.timeToX (t2, p->getWidth());

            setBounds (x1, 0, x2 - x1, p->getHeight());
            return;
        }
    }

    setBounds ({});
}

//==============================================================================
TrackHeaderComponent::TrackHeaderComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (t)
{
    Helpers::addAndMakeVisible (*this, { &trackName, &armButton, &muteButton, &soloButton, &inputButton });

    auto styleHeaderButton = [] (juce::TextButton& button, juce::Colour onColour)
    {
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (31, 41, 56));
        button.setColour (juce::TextButton::buttonOnColourId, onColour);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.88f));
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::white.withAlpha (0.95f));
    };

    styleHeaderButton (armButton, juce::Colour::fromRGB (222, 80, 78));
    styleHeaderButton (muteButton, juce::Colour::fromRGB (220, 104, 78));
    styleHeaderButton (soloButton, juce::Colour::fromRGB (84, 188, 103));
    styleHeaderButton (inputButton, juce::Colour::fromRGB (76, 146, 223));

    trackName.setText (t->getName(), dontSendNotification);
    trackName.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    trackName.setJustificationType (juce::Justification::centredLeft);
    trackName.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.93f));
    trackName.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        inputButton.onClick = [this, at]
        {
            PopupMenu m;

            if (EngineHelpers::trackHasInput (*at))
            {
                bool ticked = EngineHelpers::isInputMonitoringEnabled (*at);
                m.addItem (1000, "Input Monitoring", true, ticked);
                m.addSeparator();
            }

            if (editViewState.showWaveDevices)
            {
                int id = 1;
                for (auto instance : at->edit.getAllInputDevices())
                {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                    {
                        bool ticked = instance->getTargets().getFirst() == at->itemID;
                        m.addItem (id++, instance->getInputDevice().getName(), true, ticked);
                    }
                }
            }

            if (editViewState.showMidiDevices)
            {
                m.addSeparator();

                int id = 100;
                for (auto instance : at->edit.getAllInputDevices())
                {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice)
                    {
                        bool ticked = instance->getTargets().getFirst() == at->itemID;
                        m.addItem (id++, instance->getInputDevice().getName(), true, ticked);
                    }
                }
            }

            int res = m.show();

            if (res == 1000)
            {
                EngineHelpers::enableInputMonitoring (*at, ! EngineHelpers::isInputMonitoringEnabled (*at));
            }
            else if (res >= 100)
            {
                int id = 100;
                for (auto instance : at->edit.getAllInputDevices())
                {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice)
                    {
                        if (id == res)
                            [[ maybe_unused ]] auto result = instance->setTarget (at->itemID, true, &at->edit.getUndoManager(), 0);

                        id++;
                    }
                }
            }
            else if (res >= 1)
            {
                int id = 1;
                for (auto instance : at->edit.getAllInputDevices())
                {
                    if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
                    {
                        if (id == res)
                            [[ maybe_unused ]] auto result = instance->setTarget (at->itemID, true, &at->edit.getUndoManager(), 0);

                        id++;
                    }
                }
            }
        };
        armButton.onClick = [this, at]
        {
            EngineHelpers::armTrack (*at, ! EngineHelpers::isTrackArmed (*at));
            armButton.setToggleState (EngineHelpers::isTrackArmed (*at), dontSendNotification);
        };
        muteButton.onClick = [at] { at->setMute (! at->isMuted (false)); };
        soloButton.onClick = [at] { at->setSolo (! at->isSolo (false)); };

        armButton.setToggleState (EngineHelpers::isTrackArmed (*at), dontSendNotification);
    }
    else
    {
        armButton.setVisible (false);
        muteButton.setVisible (false);
        soloButton.setVisible (false);
    }

    track->state.addListener (this);
    inputsState = track->edit.state.getChildWithName (te::IDs::INPUTDEVICES);
    inputsState.addListener (this);

    valueTreePropertyChanged (track->state, te::IDs::mute);
    valueTreePropertyChanged (track->state, te::IDs::solo);
    valueTreePropertyChanged (inputsState, te::IDs::targetIndex);
}

TrackHeaderComponent::~TrackHeaderComponent()
{
    track->state.removeListener (this);
}

void TrackHeaderComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (te::TrackList::isTrack (v))
    {
        if (i == te::IDs::mute)
            muteButton.setToggleState ((bool)v[i], dontSendNotification);
        else if (i == te::IDs::solo)
            soloButton.setToggleState ((bool)v[i], dontSendNotification);
    }
    else if (v.hasType (te::IDs::INPUTDEVICES)
             || v.hasType (te::IDs::INPUTDEVICE)
             || v.hasType (te::IDs::INPUTDEVICEDESTINATION))
    {
        if (auto at = dynamic_cast<te::AudioTrack*> (track.get()))
        {
            armButton.setEnabled (EngineHelpers::trackHasInput (*at));
            armButton.setToggleState (EngineHelpers::isTrackArmed (*at), dontSendNotification);
        }
    }
}

void TrackHeaderComponent::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    auto base = juce::Colour::fromRGB (26, 34, 46);

    juce::ColourGradient fill (base.brighter (0.12f), area.getX(), area.getY(),
                               base.darker (0.22f), area.getX(), area.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (track->getColour().withAlpha (0.75f));
    g.fillRoundedRectangle (juce::Rectangle<float> (area.getX() + 2.0f, area.getY() + 3.0f, 3.0f, area.getHeight() - 6.0f), 1.2f);

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    if (editViewState.selectionManager.isSelected (track.get()))
    {
        g.setColour (getSelectionAccent().withAlpha (0.95f));
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.8f);
    }
}

void TrackHeaderComponent::mouseDown (const MouseEvent&)
{
    editViewState.selectionManager.selectOnly (track.get());
}

void TrackHeaderComponent::resized()
{
    auto r = getLocalBounds().reduced (5);
    trackName.setBounds (r.removeFromTop (juce::jmax (16, r.getHeight() / 2 + 1)));
    r.removeFromTop (2);

    const bool isAudioTrack = dynamic_cast<te::AudioTrack*> (track.get()) != nullptr;
    const bool showRecordControls = isAudioTrack && editViewState.showRecordControls.get();

    inputButton.setVisible (showRecordControls);
    armButton.setVisible (showRecordControls);

    const int buttonCount = (int) inputButton.isVisible() + (int) armButton.isVisible() + (int) muteButton.isVisible() + (int) soloButton.isVisible();
    if (buttonCount <= 0)
        return;

    const int gap = 3;
    const int buttonWidth = juce::jmax (16, (r.getWidth() - gap * (buttonCount - 1)) / buttonCount);
    auto setButtonBounds = [&r, buttonWidth] (juce::Button& button)
    {
        button.setBounds (r.removeFromLeft (buttonWidth).reduced (0, 1));
        r.removeFromLeft (3);
    };

    if (inputButton.isVisible())
        setButtonBounds (inputButton);

    if (armButton.isVisible())
        setButtonBounds (armButton);

    if (muteButton.isVisible())
        setButtonBounds (muteButton);

    if (soloButton.isVisible())
        setButtonBounds (soloButton);
}

//==============================================================================
PluginComponent::PluginComponent (EditViewState& evs, te::Plugin::Ptr p)
    : editViewState (evs), plugin (p)
{
    const auto pluginName = plugin->getName();
    setButtonText (pluginName.substring (0, 1).toUpperCase());
    setTooltip (pluginName);
    setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (34, 46, 63));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (64, 131, 209));
    setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.90f));
    setColour (juce::TextButton::textColourOnId, juce::Colours::white.withAlpha (0.95f));
}

PluginComponent::~PluginComponent()
{
}

void PluginComponent::clicked (const ModifierKeys& modifiers)
{
    editViewState.selectionManager.selectOnly (plugin.get());
    if (modifiers.isPopupMenu())
    {
        PopupMenu m;
        m.addItem ("Delete", [this]
        {
            if (juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                                    "Delete Plugin",
                                                    "Delete plugin \"" + plugin->getName() + "\"?",
                                                    "Delete",
                                                    "Cancel",
                                                    this))
                plugin->deleteFromParent();
        });
        m.showAt (this);
    }
    else
    {
        plugin->showWindowExplicitly();
    }
}

//==============================================================================
TrackFooterComponent::TrackFooterComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (t)
{
    addAndMakeVisible (addButton);
    addButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (34, 46, 63));
    addButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (64, 131, 209));
    addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.88f));
    addButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white.withAlpha (0.95f));

    buildPlugins();

    track->state.addListener (this);

    addButton.onClick = [this]
    {
        if (auto plugin = showMenuAndCreatePlugin (track->edit))
            track->pluginList.insertPlugin (plugin, 0, &editViewState.selectionManager);
    };
}

TrackFooterComponent::~TrackFooterComponent()
{
    track->state.removeListener (this);
}

void TrackFooterComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c)
{
    if (c.hasType (te::IDs::PLUGIN))
        markAndUpdate (updatePlugins);
}

void TrackFooterComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int)
{
    if (c.hasType (te::IDs::PLUGIN))
        markAndUpdate (updatePlugins);
}

void TrackFooterComponent::valueTreeChildOrderChanged (juce::ValueTree&, int, int)
{
    markAndUpdate (updatePlugins);
}

void TrackFooterComponent::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto base = juce::Colour::fromRGB (24, 31, 42);

    juce::ColourGradient fill (base.brighter (0.10f), area.getX(), area.getY(),
                               base.darker (0.20f), area.getX(), area.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    if (editViewState.selectionManager.isSelected (track.get()))
    {
        g.setColour (getSelectionAccent().withAlpha (0.90f));
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.8f);
    }
}

void TrackFooterComponent::mouseDown (const MouseEvent&)
{
    editViewState.selectionManager.selectOnly (track.get());
}

void TrackFooterComponent::resized()
{
    auto r = getLocalBounds().reduced (5);
    const int cx = juce::jmax (18, juce::jmin (24, r.getHeight() - 2));

    addButton.setBounds (r.removeFromLeft (cx).withSizeKeepingCentre (cx, cx).reduced (1));
    r.removeFromLeft (6);

    for (auto p : plugins)
    {
        p->setBounds (r.removeFromLeft (cx).withSizeKeepingCentre (cx, cx).reduced (1));
        r.removeFromLeft (3);
    }
}

void TrackFooterComponent::handleAsyncUpdate()
{
    if (compareAndReset (updatePlugins))
        buildPlugins();
}

void TrackFooterComponent::buildPlugins()
{
    plugins.clear();

    for (auto plugin : track->pluginList)
    {
        auto p = new PluginComponent (editViewState, plugin);
        addAndMakeVisible (p);
        plugins.add (p);
    }
    resized();
}

//==============================================================================
TrackComponent::TrackComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (t)
{
    track->state.addListener (this);
    track->edit.getTransport().addChangeListener (this);

    markAndUpdate (updateClips);
}

TrackComponent::~TrackComponent()
{
    track->state.removeListener (this);
    track->edit.getTransport().removeChangeListener (this);
}

void TrackComponent::paint (Graphics& g)
{
    const bool selected = editViewState.selectionManager.isSelected (track.get());
    bool oddLane = false;
    if (auto* parent = getParentComponent())
        oddLane = (parent->getIndexOfChildComponent (this) % 2) != 0;
    auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());

    auto laneBase = getTrackLaneBase (oddLane);
    if (selected)
        laneBase = laneBase.brighter (0.18f);

    juce::ColourGradient laneGrad (laneBase.brighter (0.08f).withAlpha (0.95f), 0.0f, 0.0f,
                                   laneBase.darker (0.14f).withAlpha (0.95f), 0.0f, (float) getHeight(), false);
    g.setGradientFill (laneGrad);
    g.fillRect (getLocalBounds());

    if (audioTrack != nullptr)
    {
        if (audioTrack->isMuted (false))
        {
            g.setColour (juce::Colour::fromRGB (64, 36, 36).withAlpha (0.26f));
            g.fillRect (getLocalBounds());
        }
        else if (audioTrack->isSolo (false))
        {
            g.setColour (juce::Colour::fromRGB (236, 193, 84).withAlpha (0.16f));
            g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), 5));
        }
    }

    g.setColour (track->getColour().withAlpha (selected ? 0.95f : 0.72f));
    g.fillRect (juce::Rectangle<float> (0.0f, 1.0f, 2.5f, (float) getHeight() - 2.0f));

    if (getWidth() > 1 && getHeight() > 1)
    {
        const auto viewStart = editViewState.viewX1.get();
        const auto viewEnd = editViewState.viewX2.get();
        const int startBeat = (int) std::floor (track->edit.tempoSequence.toBeats (viewStart).inBeats());
        const int endBeat = (int) std::ceil (track->edit.tempoSequence.toBeats (viewEnd).inBeats()) + 1;
        const int beatCount = juce::jmax (0, endBeat - startBeat);

        const int beatsPerBar = getBeatsPerBarInt (track->edit, viewStart);

        for (int beat = startBeat; beat <= endBeat; ++beat)
        {
            const auto beatTime = track->edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) beat));
            const int x = editViewState.timeToX (beatTime, getWidth());
            if (x < 0 || x > getWidth())
                continue;

            const bool isBar = (beat % beatsPerBar) == 0;
            g.setColour (juce::Colours::white.withAlpha (isBar ? 0.18f : 0.065f));
            g.drawVerticalLine (x, 0.0f, (float) getHeight());
        }

        const bool drawSubBeats = beatCount < 220 && (viewEnd - viewStart).inSeconds() <= 28.0;
        if (drawSubBeats)
        {
            g.setColour (juce::Colours::white.withAlpha (0.035f));
            for (int beat = startBeat; beat <= endBeat; ++beat)
            {
                for (int quarter = 1; quarter < 4; ++quarter)
                {
                    const auto subBeat = te::BeatPosition::fromBeats ((double) beat + (double) quarter * 0.25);
                    const auto subTime = track->edit.tempoSequence.toTime (subBeat);
                    const int x = editViewState.timeToX (subTime, getWidth());
                    if (x > 0 && x < getWidth())
                        g.drawVerticalLine (x, 0.0f, (float) getHeight());
                }
            }
        }

        if (track->edit.getTransport().looping)
        {
            const auto loopRange = track->edit.getTransport().getLoopRange();
            if (! loopRange.isEmpty())
            {
                const int loopX1 = editViewState.timeToX (loopRange.getStart(), getWidth());
                const int loopX2 = editViewState.timeToX (loopRange.getEnd(), getWidth());
                const int x1 = juce::jlimit (0, getWidth(), juce::jmin (loopX1, loopX2));
                const int x2 = juce::jlimit (0, getWidth(), juce::jmax (loopX1, loopX2));
                if (x2 > x1)
                {
                    g.setColour (juce::Colour::fromRGB (89, 196, 116).withAlpha (0.12f));
                    g.fillRect (juce::Rectangle<int> (x1, 0, x2 - x1, getHeight()));
                    g.setColour (juce::Colour::fromRGB (112, 212, 138).withAlpha (0.40f));
                    g.drawVerticalLine (x1, 0.0f, (float) getHeight());
                    g.drawVerticalLine (x2, 0.0f, (float) getHeight());
                }
            }
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    auto handleArea = getLocalBounds().removeFromBottom (laneResizeHandleHeightPx);
    const bool resizing = resizingLaneHeight;
    const bool handleHover = isMouseOverOrDragging() && isNearLaneResizeHandle (*this, getMouseXYRelative().y);
    const auto handleColour = (resizing || handleHover)
                                ? juce::Colour::fromRGB (98, 173, 236).withAlpha (0.80f)
                                : juce::Colours::white.withAlpha (0.14f);
    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillRect (handleArea.withTrimmedTop (2));
    g.setColour (handleColour);
    g.fillRect (juce::Rectangle<int> (handleArea.getX() + 6, handleArea.getCentreY() - 1,
                                      juce::jmax (24, handleArea.getWidth() - 12), 2));

    if (selected)
    {
        g.setColour (getSelectionAccent().withAlpha (0.95f));

        auto rc = getLocalBounds();
        if (editViewState.showHeaders) rc = rc.withTrimmedLeft (-4);
        if (editViewState.showFooters) rc = rc.withTrimmedRight (-4);

        g.drawRect (rc, 2);
    }
}

void TrackComponent::mouseDown (const MouseEvent& e)
{
    editViewState.selectionManager.selectOnly (track.get());

    if (! e.mods.isPopupMenu() && e.mods.isLeftButtonDown() && isNearLaneResizeHandle (*this, e.y))
    {
        resizingLaneHeight = true;
        resizeDragStartScreenY = e.getScreenY();
        resizeDragStartHeight = getHeight();
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        return;
    }

    const int cursorX = juce::jlimit (0, juce::jmax (1, getWidth()), e.x);
    const auto cursorTime = editViewState.xToTime (cursorX, juce::jmax (1, getWidth()));
    track->edit.getTransport().setPosition (cursorTime);

    auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());
    const auto activeTool = getTimelineEditTool();

    if (! e.mods.isPopupMenu())
    {
        if (audioTrack != nullptr && activeTool == TimelineEditTool::pencil)
        {
            const auto oneBar = getOneBarDurationAt (track->edit, cursorTime);
            if (auto clip = audioTrack->insertMIDIClip ("Pattern", { cursorTime, oneBar }, &editViewState.selectionManager))
                editViewState.selectionManager.selectOnly (clip.get());
            return;
        }

        if (audioTrack != nullptr && activeTool == TimelineEditTool::select && e.getNumberOfClicks() > 1)
        {
            const auto oneBar = getOneBarDurationAt (track->edit, cursorTime);
            if (auto clip = audioTrack->insertMIDIClip ("Pattern", { cursorTime, oneBar }, &editViewState.selectionManager))
                editViewState.selectionManager.selectOnly (clip.get());
        }

        return;
    }

    PopupMenu m;
    m.addSectionHeader (track->getName());
    m.addItem (1, "Add MIDI Clip At Cursor", audioTrack != nullptr);
    m.addItem (2, "Import Audio File At Cursor", audioTrack != nullptr);
    m.addItem (3, "Split Track At Cursor");
    m.addSeparator();
    m.addItem (4, audioTrack != nullptr && audioTrack->isMuted (false) ? "Unmute Track" : "Mute Track",
               audioTrack != nullptr, false);
    m.addItem (5, audioTrack != nullptr && audioTrack->isSolo (false) ? "Unsolo Track" : "Solo Track",
               audioTrack != nullptr, false);
    m.addItem (6, "Rename Track");
    m.addItem (7, "Cycle Track Colour", audioTrack != nullptr);
    m.addSeparator();
    m.addItem (8, "Duplicate Track");

    const bool canDelete = ! (track->isMasterTrack() || track->isTempoTrack() || track->isMarkerTrack() || track->isChordTrack());
    m.addItem (9, "Delete Track", canDelete, false);

    const int res = m.showMenu (PopupMenu::Options().withTargetComponent (this));
    if (res == 0)
        return;

    if (res == 1 && audioTrack != nullptr)
    {
        const auto oneBar = getOneBarDurationAt (track->edit, cursorTime);
        if (auto clip = audioTrack->insertMIDIClip ("Pattern", { cursorTime, oneBar }, &editViewState.selectionManager))
            editViewState.selectionManager.selectOnly (clip.get());
    }
    else if (res == 2)
    {
        auto chooser = juce::FileChooser ("Import Audio File",
                                          track->edit.engine.getPropertyStorage().getDefaultLoadSaveDirectory ("trackAreaImportAudio"),
                                          track->edit.engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats());

        if (audioTrack != nullptr && chooser.browseForFileToOpen())
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile())
            {
                track->edit.engine.getPropertyStorage().setDefaultLoadSaveDirectory ("trackAreaImportAudio", file.getParentDirectory());
                te::AudioFile audioFile (track->edit.engine, file);

                if (audioFile.isValid())
                {
                    const auto clipLength = te::TimeDuration::fromSeconds (audioFile.getLength());
                    const te::ClipPosition position { { cursorTime, clipLength }, {} };
                    if (auto clip = audioTrack->insertWaveClip (file.getFileNameWithoutExtension(), file, position, false))
                        editViewState.selectionManager.selectOnly (clip.get());
                }
            }
        }
    }
    else if (res == 3)
    {
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
            clipTrack->splitAt (cursorTime);
    }
    else if (res == 4 && audioTrack != nullptr)
    {
        audioTrack->setMute (! audioTrack->isMuted (false));
    }
    else if (res == 5 && audioTrack != nullptr)
    {
        audioTrack->setSolo (! audioTrack->isSolo (false));
    }
    else if (res == 6)
    {
        juce::AlertWindow w ("Rename Track", "Enter a new track name", juce::AlertWindow::NoIcon);
        w.addTextEditor ("name", track->getName());
        w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w.addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        if (w.runModalLoop() == 1)
        {
            const auto newName = w.getTextEditorContents ("name").trim();
            if (newName.isNotEmpty())
                track->setName (newName);
        }
    }
    else if (res == 7 && audioTrack != nullptr)
    {
        const juce::Colour palette[] =
        {
            juce::Colour::fromRGB (242, 99, 90),
            juce::Colour::fromRGB (244, 158, 76),
            juce::Colour::fromRGB (242, 215, 84),
            juce::Colour::fromRGB (88, 198, 122),
            juce::Colour::fromRGB (77, 189, 206),
            juce::Colour::fromRGB (90, 144, 234),
            juce::Colour::fromRGB (222, 112, 190)
        };

        constexpr int paletteSize = (int) (sizeof (palette) / sizeof (palette[0]));
        int nearestIndex = 0;
        float nearestDistance = 1.0e9f;
        const auto current = track->getColour();

        for (int i = 0; i < paletteSize; ++i)
        {
            const auto c = palette[i];
            const float dr = (float) c.getFloatRed()   - (float) current.getFloatRed();
            const float dg = (float) c.getFloatGreen() - (float) current.getFloatGreen();
            const float db = (float) c.getFloatBlue()  - (float) current.getFloatBlue();
            const float distance = dr * dr + dg * dg + db * db;

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestIndex = i;
            }
        }

        track->setColour (palette[(nearestIndex + 1) % paletteSize]);
    }
    else if (res == 8)
    {
        te::Track::Ptr sourceTrack (track);
        auto duplicated = track->edit.copyTrack (sourceTrack, te::TrackInsertPoint (*track, false));
        if (duplicated != nullptr)
        {
            duplicated->setName (track->getName() + " Copy");
            editViewState.selectionManager.selectOnly (duplicated.get());
        }
    }
    else if (res == 9 && canDelete)
    {
        if (juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                                "Delete Track",
                                                "Delete track \"" + track->getName() + "\" and all its clips?",
                                                "Delete",
                                                "Cancel",
                                                this))
            track->edit.deleteTrack (track.get());
    }
}

void TrackComponent::mouseDrag (const MouseEvent& e)
{
    if (! resizingLaneHeight)
        return;

    const int delta = e.getScreenY() - resizeDragStartScreenY;
    setTrackLaneHeight (*track, resizeDragStartHeight + delta);

    if (auto* parent = getParentComponent())
    {
        parent->resized();
        parent->repaint();
    }
}

void TrackComponent::mouseUp (const MouseEvent& e)
{
    if (resizingLaneHeight)
    {
        resizingLaneHeight = false;
        mouseMove (e);
    }
}

void TrackComponent::mouseMove (const MouseEvent& e)
{
    juce::MouseCursor mouseCursor = juce::MouseCursor::NormalCursor;
    if (resizingLaneHeight || isNearLaneResizeHandle (*this, e.y))
    {
        mouseCursor = juce::MouseCursor::UpDownResizeCursor;
    }
    else
    {
        const auto activeTool = getTimelineEditTool();

        if (activeTool == TimelineEditTool::pencil || activeTool == TimelineEditTool::scissors)
            mouseCursor = juce::MouseCursor::CrosshairCursor;
        else if (activeTool == TimelineEditTool::resize)
            mouseCursor = juce::MouseCursor::LeftRightResizeCursor;
    }

    setMouseCursor (mouseCursor);
}

void TrackComponent::mouseExit (const MouseEvent&)
{
    if (! resizingLaneHeight)
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TrackComponent::changeListenerCallback (ChangeBroadcaster*)
{
    markAndUpdate (updateRecordClips);
}

void TrackComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (te::Clip::isClipState (v))
    {
        if (i == te::IDs::start
            || i == te::IDs::length)
        {
            markAndUpdate (updatePositions);
        }
    }
}

void TrackComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c)
{
    if (te::Clip::isClipState (c))
        markAndUpdate (updateClips);
}

void TrackComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int)
{
    if (te::Clip::isClipState (c))
        markAndUpdate (updateClips);
}

void TrackComponent::valueTreeChildOrderChanged (juce::ValueTree& v, int a, int b)
{
    if (te::Clip::isClipState (v.getChild (a)))
        markAndUpdate (updatePositions);
    else if (te::Clip::isClipState (v.getChild (b)))
        markAndUpdate (updatePositions);
}

void TrackComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateClips))
        buildClips();
    if (compareAndReset (updatePositions))
        resized();
    if (compareAndReset (updateRecordClips))
        buildRecordClips();
}

void TrackComponent::resized()
{
    const int clipHeight = juce::jmax (1, getHeight() - laneResizeHandleHeightPx);
    for (auto cc : clips)
    {
        auto& c = cc->getClip();
        auto pos = c.getPosition();
        int x1 = editViewState.timeToX (pos.getStart(), getWidth());
        int x2 = editViewState.timeToX (pos.getEnd(), getWidth());

        cc->setBounds (x1, 0, x2 - x1, clipHeight);
    }
}

void TrackComponent::buildClips()
{
    clips.clear();

    if (auto ct = dynamic_cast<te::ClipTrack*> (track.get()))
    {
        for (auto c : ct->getClips())
        {
            ClipComponent* cc = nullptr;

            if (dynamic_cast<te::WaveAudioClip*> (c))
                cc = new AudioClipComponent (editViewState, c);
            else if (dynamic_cast<te::MidiClip*> (c))
                cc = new MidiClipComponent (editViewState, c);
            else
                cc = new ClipComponent (editViewState, c);

            clips.add (cc);
            addAndMakeVisible (cc);
        }
    }

    resized();
}

void TrackComponent::buildRecordClips()
{
    bool needed = false;

    if (track->edit.getTransport().isRecording())
    {
        for (auto in : track->edit.getAllInputDevices())
        {
            if (in->isRecordingActive() && track->itemID == in->getTargets().getFirst())
            {
                needed = true;
                break;
            }
        }
    }

    if (needed)
    {
        recordingClip = std::make_unique<RecordingClipComponent> (track, editViewState);
        addAndMakeVisible (*recordingClip);
    }
    else
    {
        recordingClip = nullptr;
    }
}

//==============================================================================
PlayheadComponent::PlayheadComponent (te::Edit& e , EditViewState& evs)
    : edit (e), editViewState (evs)
{
    startTimerHz (30);
}

void PlayheadComponent::paint (Graphics& g)
{
    const auto x = (float) xPosition;

    g.setColour (juce::Colour::fromRGB (255, 216, 96).withAlpha (0.24f));
    g.fillRect (juce::Rectangle<float> (x - 2.0f, 0.0f, 6.0f, (float) getHeight()));

    g.setColour (juce::Colour::fromRGB (255, 236, 153).withAlpha (0.95f));
    g.fillRect (juce::Rectangle<float> (x, 0.0f, 2.0f, (float) getHeight()));

    juce::Path marker;
    marker.startNewSubPath (x - 5.0f, 0.0f);
    marker.lineTo (x + 7.0f, 0.0f);
    marker.lineTo (x + 1.0f, 8.0f);
    marker.closeSubPath();
    g.fillPath (marker);
}

bool PlayheadComponent::hitTest (int x, int)
{
    if (std::abs (x - xPosition) <= 3)
        return true;

    return false;
}

void PlayheadComponent::mouseDown (const MouseEvent&)
{
    edit.getTransport().setUserDragging (true);
}

void PlayheadComponent::mouseUp (const MouseEvent&)
{
    edit.getTransport().setUserDragging (false);
}

void PlayheadComponent::mouseDrag (const MouseEvent& e)
{
    auto t = editViewState.xToTime (e.x, getWidth());
    edit.getTransport().setPosition (t);
    timerCallback();
}

void PlayheadComponent::timerCallback()
{
    if (firstTimer)
    {
        // On Linux, don't set the mouse cursor until after the Component has appeared
        firstTimer = false;
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
    }

    int newX = editViewState.timeToX (edit.getTransport().getPosition(), getWidth());
    if (newX != xPosition)
    {
        repaint (jmin (newX, xPosition) - 1, 0, jmax (newX, xPosition) - jmin (newX, xPosition) + 3, getHeight());
        xPosition = newX;
    }
}

//==============================================================================
EditComponent::EditComponent (te::Edit& e, te::SelectionManager& sm)
    : edit (e), editViewState (e, sm)
{
    edit.state.addListener (this);
    editViewState.selectionManager.addChangeListener (this);

    addAndMakeVisible (playhead);

    markAndUpdate (updateTracks);
}

EditComponent::~EditComponent()
{
    editViewState.selectionManager.removeChangeListener (this);
    edit.state.removeListener (this);
}

void EditComponent::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v.hasType (IDs::EDITVIEWSTATE))
    {
        if (i == IDs::viewX1
            || i == IDs::viewX2
            || i == IDs::viewY
            || i == IDs::trackHeight)
        {
            markAndUpdate (updateZoom);
        }
        else if (i == IDs::showHeaders
                 || i == IDs::showFooters
                 || i == IDs::showRecordControls)
        {
            markAndUpdate (updateZoom);
        }
        else if (i == IDs::drawWaveforms)
        {
            repaint();
        }
    }
    else if (te::TrackList::isTrack (v) && i == laneHeightPropertyId)
    {
        markAndUpdate (updateZoom);
    }
}

void EditComponent::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c)
{
    if (te::TrackList::isTrack (c))
        markAndUpdate (updateTracks);
}

void EditComponent::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int)
{
    if (te::TrackList::isTrack (c))
        markAndUpdate (updateTracks);
}

void EditComponent::valueTreeChildOrderChanged (juce::ValueTree& v, int a, int b)
{
    if (te::TrackList::isTrack (v.getChild (a)))
        markAndUpdate (updateTracks);
    else if (te::TrackList::isTrack (v.getChild (b)))
        markAndUpdate (updateTracks);
}

void EditComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateTracks))
        buildTracks();
    if (compareAndReset (updateZoom))
        resized();
}

void EditComponent::paint (Graphics& g)
{
    g.fillAll (getEditBackground());

    const int headerWidth = editViewState.showHeaders ? 150 : 0;
    const int footerWidth = editViewState.showFooters ? 150 : 0;
    auto bounds = getLocalBounds();

    if (headerWidth > 0)
    {
        auto headerArea = juce::Rectangle<int> (bounds.getX(), bounds.getY(), headerWidth, bounds.getHeight());
        const auto headerF = headerArea.toFloat();
        juce::ColourGradient fill (getPanelBackground (false).brighter (0.08f), headerF.getX(), headerF.getY(),
                                   getPanelBackground (false).darker (0.16f), headerF.getX(), headerF.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRect (headerArea);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine (headerArea.getRight() - 1, 0.0f, (float) headerArea.getBottom());
    }

    if (footerWidth > 0)
    {
        auto footerArea = juce::Rectangle<int> (bounds.getRight() - footerWidth, bounds.getY(), footerWidth, bounds.getHeight());
        const auto footerF = footerArea.toFloat();
        juce::ColourGradient fill (getPanelBackground (true).brighter (0.06f), footerF.getX(), footerF.getY(),
                                   getPanelBackground (true).darker (0.18f), footerF.getX(), footerF.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRect (footerArea);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine (footerArea.getX(), 0.0f, (float) footerArea.getBottom());
    }

    auto timelineArea = bounds.withTrimmedLeft (headerWidth).withTrimmedRight (footerWidth);

    if (timelineArea.getWidth() <= 1)
        return;

    auto background = timelineArea.toFloat();
    juce::ColourGradient fill (juce::Colour::fromRGB (20, 27, 38), background.getX(), background.getY(),
                               juce::Colour::fromRGB (11, 16, 23), background.getX(), background.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRect (timelineArea);

    const auto viewStart = editViewState.viewX1.get();
    const auto viewEnd = editViewState.viewX2.get();
    const int beatsPerBar = getBeatsPerBarInt (edit, viewStart);
    const int startBeat = (int) std::floor (edit.tempoSequence.toBeats (viewStart).inBeats());
    const int endBeat = (int) std::ceil (edit.tempoSequence.toBeats (viewEnd).inBeats()) + 1;
    const int beatCount = juce::jmax (0, endBeat - startBeat);

    for (int laneIndex = 0; laneIndex < tracks.size(); ++laneIndex)
    {
        auto laneBounds = tracks[laneIndex]->getBounds().getIntersection (timelineArea);
        if (laneBounds.isEmpty())
            continue;

        if ((laneIndex % 2) != 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.018f));
            g.fillRect (laneBounds);
        }

        g.setColour (juce::Colours::black.withAlpha (0.19f));
        g.drawHorizontalLine (laneBounds.getY(), (float) timelineArea.getX(), (float) timelineArea.getRight());
    }

    if (edit.getTransport().looping)
    {
        const auto loopRange = edit.getTransport().getLoopRange();
        if (! loopRange.isEmpty())
        {
            const int loopX1 = timelineArea.getX() + editViewState.timeToX (loopRange.getStart(), timelineArea.getWidth());
            const int loopX2 = timelineArea.getX() + editViewState.timeToX (loopRange.getEnd(), timelineArea.getWidth());
            const int x1 = juce::jlimit (timelineArea.getX(), timelineArea.getRight(), juce::jmin (loopX1, loopX2));
            const int x2 = juce::jlimit (timelineArea.getX(), timelineArea.getRight(), juce::jmax (loopX1, loopX2));

            if (x2 > x1)
            {
                g.setColour (juce::Colour::fromRGB (89, 196, 116).withAlpha (0.11f));
                g.fillRect (juce::Rectangle<int> (x1, timelineArea.getY(), x2 - x1, timelineArea.getHeight()));
                g.setColour (juce::Colour::fromRGB (122, 214, 145).withAlpha (0.44f));
                g.drawVerticalLine (x1, (float) timelineArea.getY(), (float) timelineArea.getBottom());
                g.drawVerticalLine (x2, (float) timelineArea.getY(), (float) timelineArea.getBottom());
            }
        }
    }

    int lastBarLabelX = -1000000;
    for (int beat = startBeat; beat <= endBeat; ++beat)
    {
        const auto beatTime = edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) beat));
        const int x = timelineArea.getX() + editViewState.timeToX (beatTime, timelineArea.getWidth());
        if (x < timelineArea.getX() || x > timelineArea.getRight())
            continue;

        const bool isBar = (beat % beatsPerBar) == 0;
        g.setColour (juce::Colours::white.withAlpha (isBar ? 0.20f : 0.07f));
        g.drawVerticalLine (x, (float) timelineArea.getY(), (float) timelineArea.getBottom());

        if (isBar && x - lastBarLabelX >= 32)
        {
            const int bar = juce::jmax (1, (beat / beatsPerBar) + 1);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawText (juce::String (bar), x + 3, timelineArea.getY() + 2, 36, 12, juce::Justification::topLeft, false);
            lastBarLabelX = x;
        }
    }

    const bool drawSubBeats = beatCount < 260 && (viewEnd - viewStart).inSeconds() <= 30.0;
    if (drawSubBeats)
    {
        g.setColour (juce::Colours::white.withAlpha (0.03f));
        for (int beat = startBeat; beat <= endBeat; ++beat)
        {
            for (int quarter = 1; quarter < 4; ++quarter)
            {
                const auto subBeat = te::BeatPosition::fromBeats ((double) beat + (double) quarter * 0.25);
                const auto subTime = edit.tempoSequence.toTime (subBeat);
                const int x = timelineArea.getX() + editViewState.timeToX (subTime, timelineArea.getWidth());
                if (x > timelineArea.getX() && x < timelineArea.getRight())
                    g.drawVerticalLine (x, (float) timelineArea.getY(), (float) timelineArea.getBottom());
            }
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.11f));
    g.drawRect (timelineArea);
}

void EditComponent::resized()
{
    jassert (headers.size() == tracks.size());

    const int trackGap = 2;
    const int headerWidth = editViewState.showHeaders ? 150 : 0;
    const int footerWidth = editViewState.showFooters ? 150 : 0;

    playhead.setBounds (getLocalBounds().withTrimmedLeft (headerWidth).withTrimmedRight (footerWidth));

    int y = roundToInt (editViewState.viewY.get());
    for (int i = 0; i < jmin (headers.size(), tracks.size()); i++)
    {
        auto h = headers[i];
        auto t = tracks[i];
        auto f = footers[i];
        const int laneHeight = t->getTrack() != nullptr
                                 ? getLaneHeightForTrack (*t->getTrack(), editViewState)
                                 : juce::jlimit (minLaneHeightPx, maxLaneHeightPx, roundToInt (editViewState.trackHeight.get()));

        h->setBounds (0, y, headerWidth, laneHeight);
        t->setBounds (headerWidth, y, getWidth() - headerWidth - footerWidth, laneHeight);
        f->setBounds (getWidth() - footerWidth, y, footerWidth, laneHeight);

        y += laneHeight + trackGap;
    }

    for (auto t : tracks)
        t->resized();
}

void EditComponent::mouseDown (const MouseEvent& e)
{
    const int headerWidth = editViewState.showHeaders ? 150 : 0;
    const int footerWidth = editViewState.showFooters ? 150 : 0;
    const int contentWidth = juce::jmax (1, getWidth() - headerWidth - footerWidth);
    const int localX = juce::jlimit (0, contentWidth, e.x - headerWidth);
    const auto cursorTime = editViewState.xToTime (localX, contentWidth);

    if (! e.mods.isPopupMenu())
    {
        if (! e.mods.isAnyModifierKeyDown())
        {
            edit.getTransport().setPosition (cursorTime);
            editViewState.selectionManager.deselectAll();
        }

        return;
    }

    edit.getTransport().setPosition (cursorTime);

    PopupMenu m;
    m.addItem (1, "Add Track");
    m.addItem (2, "Add Marker At Cursor");
    m.addItem (3, "Paste At Cursor");
    m.addSeparator();
    m.addItem (4, "Insert 1 Bar At Cursor");
    m.addItem (5, "Delete 1 Bar At Cursor");
    m.addSeparator();
    m.addItem (6, "Zoom To Full Arrangement");
    m.addItem (7, "Show Headers", true, editViewState.showHeaders);
    m.addItem (8, "Show Footers", true, editViewState.showFooters);

    const int res = m.showMenu (PopupMenu::Options().withTargetComponent (this));
    if (res == 0)
        return;

    if (res == 1)
    {
        const int previousTrackCount = te::getAudioTracks (edit).size();
        edit.ensureNumberOfAudioTracks (previousTrackCount + 1);
    }
    else if (res == 2)
    {
        auto& markerManager = edit.getMarkerManager();
        const int markerID = markerManager.getNextUniqueID();
        if (auto marker = markerManager.createMarker (markerID,
                                                      cursorTime,
                                                      getOneBarDurationAt (edit, cursorTime),
                                                      &editViewState.selectionManager))
        {
            marker->setName ("Marker " + juce::String (markerID));
            editViewState.selectionManager.selectOnly (marker.get());
        }
    }
    else if (res == 3)
    {
        editViewState.selectionManager.pasteSelected();
    }
    else if (res == 4)
    {
        te::insertSpaceIntoEdit (edit, { cursorTime, getOneBarDurationAt (edit, cursorTime) });
    }
    else if (res == 5)
    {
        if (juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                                "Delete 1 Bar",
                                                "Delete one bar at the current cursor and close the gap?",
                                                "Delete",
                                                "Cancel",
                                                this))
        {
            te::deleteRegionOfTracks (edit,
                                      { cursorTime, getOneBarDurationAt (edit, cursorTime) },
                                      false,
                                      te::CloseGap::yes,
                                      &editViewState.selectionManager);
        }
    }
    else if (res == 6)
    {
        editViewState.viewX1 = te::TimePosition::fromSeconds (0.0);
        editViewState.viewX2 = te::TimePosition::fromSeconds (16.0);
    }
    else if (res == 7)
    {
        editViewState.showHeaders = ! editViewState.showHeaders.get();
    }
    else if (res == 8)
    {
        editViewState.showFooters = ! editViewState.showFooters.get();
    }
}

void EditComponent::buildTracks()
{
    tracks.clear();
    headers.clear();
    footers.clear();

    for (auto t : getAllTracks (edit))
    {
        TrackComponent* c = nullptr;

        if (t->isMasterTrack())
        {
            if (editViewState.showMasterTrack)
                c = new TrackComponent (editViewState, t);
        }
        else if (t->isTempoTrack())
        {
            if (editViewState.showGlobalTrack)
                c = new TrackComponent (editViewState, t);
        }
        else if (t->isMarkerTrack())
        {
            if (editViewState.showMarkerTrack)
                c = new TrackComponent (editViewState, t);
        }
        else if (t->isChordTrack())
        {
            if (editViewState.showChordTrack)
                c = new TrackComponent (editViewState, t);
        }
        else if (t->isArrangerTrack())
        {
            if (editViewState.showArrangerTrack)
                c = new TrackComponent (editViewState, t);
        }
        else
        {
            c = new TrackComponent (editViewState, t);
        }

        if (c != nullptr)
        {
            tracks.add (c);
            addAndMakeVisible (c);

            auto h = new TrackHeaderComponent (editViewState, t);
            headers.add (h);
            addAndMakeVisible (h);

            auto f = new TrackFooterComponent (editViewState, t);
            footers.add (f);
            addAndMakeVisible (f);
        }
    }

    playhead.toFront (false);
    resized();
}
