#include <JuceHeader.h>
#include "BeatMakerNoRecord.h"
#include "../common/PluginWindow.h"
#include "Core/BeatMakerNoRecordCore.cpp"
#include "UI/BeatMakerNoRecordUI.cpp"
#include "Editing/BeatMakerNoRecordEditing.cpp"
#include <cstdlib>
#include <iostream>

std::unique_ptr<te::UIBehaviour> createBeatMakerUiBehaviour()
{
    return std::make_unique<ExtendedUIBehaviour>();
}

namespace
{
constexpr int defaultPluginScanTimeoutMs = 5000;
constexpr int minPluginScanTimeoutMs = 500;
constexpr int maxPluginScanTimeoutMs = 120000;
constexpr const char* pluginProbeFlag = "--plugin-probe";
constexpr const char* pluginProbeFormatArg = "--plugin-probe-format";
constexpr const char* pluginProbeIdentifierArg = "--plugin-probe-id-b64";
constexpr const char* pluginProbeResultPrefix = "PLUGIN_PROBE_RESULT_B64=";

std::atomic<int> timedPluginScanTimeoutMs { defaultPluginScanTimeoutMs };
juce::CriticalSection timedOutScanEntriesLock;
juce::StringArray timedOutScanEntries;

void addTimedOutScanEntry (const juce::String& fileOrIdentifier)
{
    if (fileOrIdentifier.isEmpty())
        return;

    const juce::ScopedLock sl (timedOutScanEntriesLock);
    timedOutScanEntries.addIfNotAlreadyThere (fileOrIdentifier);
}

juce::String getArgumentValue (const juce::StringArray& args, const juce::String& key)
{
    for (int i = 0; i < args.size(); ++i)
    {
        const auto arg = args[i];
        if (arg == key)
            return i + 1 < args.size() ? args[i + 1] : juce::String();

        const auto keyEquals = key + "=";
        if (arg.startsWith (keyEquals))
            return arg.fromFirstOccurrenceOf (keyEquals, false, false);
    }

    return {};
}

bool hasArgumentFlag (const juce::StringArray& args, const juce::String& key)
{
    for (const auto& arg : args)
        if (arg == key)
            return true;

    return false;
}

std::unique_ptr<juce::XmlElement> parseProbeResultFromProcessOutput (const juce::String& output)
{
    juce::StringArray lines;
    lines.addLines (output);

    juce::String encodedResult;
    for (int i = lines.size(); --i >= 0;)
    {
        if (lines[i].startsWith (pluginProbeResultPrefix))
        {
            encodedResult = lines[i].fromFirstOccurrenceOf (pluginProbeResultPrefix, false, false).trim();
            break;
        }
    }

    if (encodedResult.isEmpty())
        return {};

    juce::MemoryOutputStream decodedResult;
    if (! juce::Base64::convertFromBase64 (decodedResult, encodedResult))
        return {};

    return juce::parseXML (decodedResult.toString());
}

int runPluginProbeCommandLinePass (const juce::StringArray& args)
{
    const auto pluginFormatName = getArgumentValue (args, pluginProbeFormatArg);
    const auto encodedIdentifier = getArgumentValue (args, pluginProbeIdentifierArg);

    if (pluginFormatName.isEmpty() || encodedIdentifier.isEmpty())
        return 2;

    juce::MemoryOutputStream decodedIdentifier;
    if (! juce::Base64::convertFromBase64 (decodedIdentifier, encodedIdentifier))
        return 3;

    const auto fileOrIdentifier = decodedIdentifier.toString();
    if (fileOrIdentifier.isEmpty())
        return 4;

    juce::AudioPluginFormatManager formatManager;
    juce::addDefaultFormatsToManager (formatManager);

    juce::AudioPluginFormat* formatToUse = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat (i);
        if (format != nullptr && format->getName().equalsIgnoreCase (pluginFormatName))
        {
            formatToUse = format;
            break;
        }
    }

    if (formatToUse == nullptr)
        return 5;

    juce::OwnedArray<juce::PluginDescription> found;
    formatToUse->findAllTypesForFile (found, fileOrIdentifier);

    juce::XmlElement result ("PLUGIN_PROBE_RESULT");
    result.setAttribute ("ok", true);
    result.setAttribute ("format", pluginFormatName);
    result.setAttribute ("identifier", fileOrIdentifier);

    for (auto* description : found)
    {
        if (description == nullptr)
            continue;

        if (auto xml = description->createXml())
            result.addChildElement (xml.release());
    }

    const auto payload = result.toString (juce::XmlElement::TextFormat().withoutHeader().singleLine());
    std::cout << pluginProbeResultPrefix << juce::Base64::toBase64 (payload) << std::endl;
    return 0;
}

int maybeRunPluginProbeMode()
{
    const auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();
    if (! hasArgumentFlag (args, pluginProbeFlag))
        return -1;

    return runPluginProbeCommandLinePass (args);
}

class TimedPluginScanCustomScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override
    {
        if (shouldExit())
            return false;

        juce::StringArray childArgs;
        childArgs.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());
        childArgs.add (pluginProbeFlag);
        childArgs.add (pluginProbeFormatArg);
        childArgs.add (format.getName());
        childArgs.add (pluginProbeIdentifierArg);
        childArgs.add (juce::Base64::toBase64 (fileOrIdentifier));

        juce::ChildProcess childProcess;

        if (! childProcess.start (childArgs, juce::ChildProcess::wantStdOut))
        {
            // If child process launch fails, keep scan functional in-process.
            format.findAllTypesForFile (result, fileOrIdentifier);
            return true;
        }

        const auto startMs = juce::Time::getMillisecondCounter();
        const auto timeoutMs = juce::jlimit (minPluginScanTimeoutMs,
                                             maxPluginScanTimeoutMs,
                                             timedPluginScanTimeoutMs.load());

        for (;;)
        {
            if (shouldExit())
            {
                childProcess.kill();
                return false;
            }

            if (childProcess.waitForProcessToFinish (20))
                break;

            const auto elapsedMs = (int) (juce::Time::getMillisecondCounter() - startMs);
            if (elapsedMs >= timeoutMs)
            {
                childProcess.kill();
                addTimedOutScanEntry (fileOrIdentifier);
                return true;
            }
        }

        const auto processOutput = childProcess.readAllProcessOutput();
        auto xml = parseProbeResultFromProcessOutput (processOutput);
        if (xml == nullptr || ! xml->hasTagName ("PLUGIN_PROBE_RESULT"))
            return false;

        for (auto* pluginXml : xml->getChildIterator())
        {
            juce::PluginDescription description;
            if (! description.loadFromXml (*pluginXml))
                continue;

            description.lastInfoUpdateTime = juce::Time::getCurrentTime();
            result.add (new juce::PluginDescription (description));
        }

        return true;
    }
};

class StartupLoadingComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    explicit StartupLoadingComponent (std::function<void (juce::Component*)> onBootFinishedToUse)
        : onBootFinished (std::move (onBootFinishedToUse))
    {
        setSize (920, 560);
        buildStartupSteps();
        startTimer (120);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient bg (juce::Colour::fromRGB (16, 21, 30), 0.0f, 0.0f,
                                 juce::Colour::fromRGB (8, 12, 18), 0.0f, bounds.getBottom(), false);
        g.setGradientFill (bg);
        g.fillAll();

        auto panel = getLocalBounds().reduced (88, 84).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillRoundedRectangle (panel.translated (0.0f, 2.0f), 12.0f);

        juce::ColourGradient panelFill (juce::Colour::fromRGB (28, 38, 54).withAlpha (0.95f), panel.getX(), panel.getY(),
                                        juce::Colour::fromRGB (20, 28, 41).withAlpha (0.92f), panel.getX(), panel.getBottom(), false);
        g.setGradientFill (panelFill);
        g.fillRoundedRectangle (panel, 12.0f);
        g.setColour (juce::Colour::fromRGB (86, 110, 146).withAlpha (0.80f));
        g.drawRoundedRectangle (panel, 12.0f, 1.0f);

        auto textArea = panel.toNearestInt().reduced (20, 16);
        g.setColour (juce::Colours::white.withAlpha (0.96f));
        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.drawText ("TheSampledexWorkflow", textArea.removeFromTop (30), juce::Justification::centredLeft, false);

        g.setColour (juce::Colour::fromRGB (153, 194, 246).withAlpha (0.92f));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
        g.drawText ("Boot pipeline: preparing DAW systems before launch",
                    textArea.removeFromTop (20),
                    juce::Justification::centredLeft,
                    false);
        g.setColour (juce::Colours::white.withAlpha (0.78f));
        g.setFont (juce::Font (juce::FontOptions (11.4f, juce::Font::plain)));
        g.drawText ("Startup tip: new projects open in MIDI instrument-ready mode. Recording stays disabled.",
                    textArea.removeFromTop (18),
                    juce::Justification::centredLeft,
                    false);

        textArea.removeFromTop (8);
        auto meter = textArea.removeFromTop (16);
        g.setColour (juce::Colour::fromRGB (27, 37, 52).withAlpha (0.95f));
        g.fillRoundedRectangle (meter.toFloat(), 6.0f);

        auto progressFill = meter.withWidth (juce::jmax (2, juce::roundToInt (meter.getWidth() * progress)));
        juce::ColourGradient meterGrad (juce::Colour::fromRGB (88, 171, 255), (float) meter.getX(), (float) meter.getY(),
                                        juce::Colour::fromRGB (53, 118, 212), (float) meter.getRight(), (float) meter.getY(), false);
        g.setGradientFill (meterGrad);
        g.fillRoundedRectangle (progressFill.toFloat(), 6.0f);

        textArea.removeFromTop (10);
        g.setColour (juce::Colours::white.withAlpha (0.86f));
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText ("Current Step: " + currentStep, textArea.removeFromTop (20), juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::plain)));
        g.drawText ("Completed " + juce::String (completedSteps.size()) + " / " + juce::String (startupSteps.size()) + " steps",
                    textArea.removeFromTop (18),
                    juce::Justification::centredLeft,
                    false);

        textArea.removeFromTop (8);
        const int historyCount = juce::jmin (4, completedSteps.size());
        for (int i = 0; i < historyCount; ++i)
        {
            const int index = completedSteps.size() - historyCount + i;
            g.setColour (juce::Colour::fromRGB (129, 210, 155).withAlpha (0.95f));
            g.drawText (juce::String::charToString ((juce_wchar) 0x2022) + " " + completedSteps[index],
                        textArea.removeFromTop (18),
                        juce::Justification::centredLeft,
                        false);
        }
    }

private:
    struct StartupStep
    {
        juce::String name;
        std::function<void()> action;
    };

    void timerCallback() override
    {
        if (nextStepIndex < startupSteps.size())
        {
            const auto& step = startupSteps[nextStepIndex];
            currentStep = step.name;
            repaint();

            step.action();
            completedSteps.add (step.name);

            ++nextStepIndex;
            progress = (float) nextStepIndex / (float) juce::jmax (1, (int) startupSteps.size());
            repaint();
            return;
        }

        stopTimer();

        if (dawComponent == nullptr)
            dawComponent = std::make_unique<BeatMakerNoRecord>();

        auto callback = onBootFinished;
        auto* componentToShow = dawComponent.release();
        juce::MessageManager::callAsync ([callback, componentToShow]
        {
            if (callback != nullptr)
                callback (componentToShow);
        });
    }

    void buildStartupSteps()
    {
        // Keep this list in sync with startup-critical behavior whenever DAW initialization changes.
        startupSteps.push_back ({
            "Validating startup directories",
            []
            {
                auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                   .getChildFile ("TheSampledexWorkflow");
                appData.createDirectory();
            }
        });

        startupSteps.push_back ({
            "Preparing plugin host formats (AU/VST3)",
            [this]
            {
                juce::StringArray formats;
               #if JUCE_PLUGINHOST_AU
                formats.add ("AU");
               #endif
               #if JUCE_PLUGINHOST_VST3
                formats.add ("VST3");
               #endif

                availablePluginFormats = formats.isEmpty() ? "None enabled" : formats.joinIntoString (", ");
                currentStep = "Preparing plugin host formats (" + availablePluginFormats + ")";
            }
        });

        startupSteps.push_back ({
            "Loading core DAW systems",
            [this]
            {
                dawComponent = std::make_unique<BeatMakerNoRecord>();
            }
        });
    }

    std::function<void (juce::Component*)> onBootFinished;
    std::vector<StartupStep> startupSteps;
    juce::StringArray completedSteps;
    std::unique_ptr<BeatMakerNoRecord> dawComponent;
    size_t nextStepIndex = 0;
    float progress = 0.0f;
    juce::String currentStep { "Booting..." };
    juce::String availablePluginFormats;
};
}

std::unique_ptr<juce::KnownPluginList::CustomScanner> createTimedPluginScanCustomScanner()
{
    return std::make_unique<TimedPluginScanCustomScanner>();
}

void setTimedPluginScanTimeoutMs (int timeoutMs)
{
    timedPluginScanTimeoutMs.store (juce::jlimit (minPluginScanTimeoutMs,
                                                  maxPluginScanTimeoutMs,
                                                  timeoutMs));
}

juce::StringArray consumeTimedOutPluginScanEntries()
{
    const juce::ScopedLock sl (timedOutScanEntriesLock);
    auto entries = timedOutScanEntries;
    timedOutScanEntries.clear();
    return entries;
}

class BeatMakerNoRecordApplication : public juce::JUCEApplication
{
public:
    BeatMakerNoRecordApplication() = default;

    const juce::String getApplicationName() override      { return "TheSampledexWorkflow"; }
    const juce::String getApplicationVersion() override   { return "0.1.0"; }

    void initialise (const juce::String&) override
    {
        if (const int probeExitCode = maybeRunPluginProbeMode(); probeExitCode >= 0)
        {
            std::exit (probeExitCode);
        }

        mainWindow.reset (new MainWindow ("TheSampledexWorkflow", *this));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::JUCEApplication& appToUse)
            : juce::DocumentWindow (name,
                                    juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                                    juce::DocumentWindow::allButtons),
              app (appToUse)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new StartupLoadingComponent ([this] (juce::Component* dawComponent)
                                                          {
                                                              launchDaw (dawComponent);
                                                          }),
                             true);

           #if JUCE_ANDROID || JUCE_IOS
            setFullScreen (true);
           #else
            setResizable (true, true);
            setResizeLimits (960, 620, 4096, 4096);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void launchDaw (juce::Component* dawComponent)
        {
            if (dawComponent == nullptr)
                return;

            setContentOwned (dawComponent, true);
            toFront (true);
        }

        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }

    private:
        juce::JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (BeatMakerNoRecordApplication)
