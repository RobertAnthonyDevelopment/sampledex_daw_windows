#include "../BeatMakerNoRecord.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
class ModernLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        const auto fonts = juce::Font::findAllTypefaceNames();
        if (fonts.contains ("SF Pro Text"))
            juce::LookAndFeel::setDefaultSansSerifTypefaceName ("SF Pro Text");
        else if (fonts.contains ("Avenir Next"))
            juce::LookAndFeel::setDefaultSansSerifTypefaceName ("Avenir Next");

        setColourScheme (juce::LookAndFeel_V4::ColourScheme (juce::Colour::fromRGB (9, 13, 21),   // window
                                                             juce::Colour::fromRGB (25, 34, 50),  // widget
                                                             juce::Colour::fromRGB (12, 17, 27),  // menu
                                                             juce::Colour::fromRGB (78, 112, 156),// outline
                                                             juce::Colours::white.withAlpha (0.94f),
                                                             juce::Colour::fromRGB (38, 128, 204),// default fill
                                                             juce::Colours::white.withAlpha (0.98f),
                                                             juce::Colour::fromRGB (248, 156, 63),// highlighted fill
                                                             juce::Colours::white.withAlpha (0.95f)));

        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour::fromRGB (9, 13, 21));
        setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (35, 49, 70));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (54, 146, 224));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.96f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white.withAlpha (0.96f));
        setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (93, 179, 230));
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB (59, 79, 106));
        setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.92f));
        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));
        setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);

        setColour (juce::GroupComponent::outlineColourId, juce::Colour::fromRGB (88, 123, 168).withAlpha (0.54f));
        setColour (juce::GroupComponent::textColourId, juce::Colours::white.withAlpha (0.92f));

        setColour (juce::ComboBox::backgroundColourId, juce::Colour::fromRGB (19, 28, 41));
        setColour (juce::ComboBox::outlineColourId, juce::Colour::fromRGB (84, 119, 162));
        setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.95f));
        setColour (juce::ComboBox::arrowColourId, juce::Colour::fromRGB (242, 173, 91));

        setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (70, 96, 135));
        setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (241, 174, 88));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB (66, 166, 224));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB (46, 60, 82));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB (20, 28, 40));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB (84, 120, 164));
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.95f));

        setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (15, 22, 33).withAlpha (0.88f));
        setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (86, 120, 162).withAlpha (0.74f));
        setColour (juce::TextEditor::textColourId, juce::Colours::white.withAlpha (0.94f));

        setColour (juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB (12, 18, 28).withAlpha (0.98f));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour::fromRGB (54, 133, 204).withAlpha (0.90f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white.withAlpha (0.98f));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white.withAlpha (0.95f));
        setColour (juce::PopupMenu::headerTextColourId, juce::Colour::fromRGB (251, 186, 110));

        setColour (juce::Toolbar::backgroundColourId, juce::Colour::fromRGB (10, 17, 27).withAlpha (0.90f));
        setColour (juce::Toolbar::separatorColourId, juce::Colour::fromRGB (92, 130, 173).withAlpha (0.72f));
        setColour (juce::Toolbar::buttonMouseOverBackgroundColourId, juce::Colour::fromRGB (66, 129, 191).withAlpha (0.46f));
        setColour (juce::Toolbar::buttonMouseDownBackgroundColourId, juce::Colour::fromRGB (78, 167, 224).withAlpha (0.58f));

        setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (77, 144, 214).withAlpha (0.90f));
        setColour (juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB (12, 17, 26).withAlpha (0.74f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour::fromRGB (11, 16, 25).withAlpha (0.98f));
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white.withAlpha (0.96f));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour::fromRGB (90, 126, 171).withAlpha (0.84f));

        setColour (juce::TabbedButtonBar::tabTextColourId, juce::Colours::white.withAlpha (0.82f));
        setColour (juce::TabbedButtonBar::frontTextColourId, juce::Colours::white.withAlpha (0.98f));
        setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colour::fromRGB (78, 113, 156).withAlpha (0.86f));
        setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colour::fromRGB (243, 173, 94).withAlpha (0.96f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions ((float) juce::jmax (11, juce::jmin (13, buttonHeight - 9)),
                                              juce::Font::bold));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        const bool isStatus = label.getComponentID().contains ("status");
        return juce::Font (juce::FontOptions (isStatus ? 13.2f : 13.7f,
                                              isStatus ? juce::Font::plain : juce::Font::bold));
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        const auto bounds = label.getLocalBounds();
        const bool drawPill = label.getBorderSize().getLeft() >= 8 || label.getComponentID().contains ("status");

        if (! drawPill)
        {
            juce::LookAndFeel_V4::drawLabel (g, label);
            return;
        }

        auto panel = bounds.toFloat().reduced (0.5f, 0.5f);
        juce::ColourGradient fill (juce::Colour::fromRGB (24, 34, 50).withAlpha (0.88f),
                                   panel.getX(), panel.getY(),
                                   juce::Colour::fromRGB (13, 19, 28).withAlpha (0.90f),
                                   panel.getX(), panel.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (panel, 6.5f);

        g.setColour (juce::Colour::fromRGB (84, 119, 162).withAlpha (0.78f));
        g.drawRoundedRectangle (panel, 6.5f, 1.0f);

        g.setColour (label.findColour (juce::Label::textColourId).withAlpha (label.isEnabled() ? 0.98f : 0.50f));
        g.setFont (getLabelFont (label));
        g.drawFittedText (label.getText(), bounds.reduced (8, 1), label.getJustificationType(), 1);
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f, 0.5f);
        auto base = backgroundColour;

        if (! button.isEnabled())
            base = base.withMultipliedSaturation (0.20f).withAlpha (0.50f);
        else if (shouldDrawButtonAsDown)
            base = base.brighter (0.22f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.16f);

        juce::ColourGradient fill (base.brighter (0.13f), bounds.getX(), bounds.getY(),
                                   base.darker (0.24f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 7.0f);

        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

        if (button.isEnabled() && shouldDrawButtonAsHighlighted)
        {
            g.setColour (juce::Colour::fromRGB (126, 192, 245).withAlpha (shouldDrawButtonAsDown ? 0.42f : 0.28f));
            g.drawRoundedRectangle (bounds.expanded (0.4f), 7.6f, 1.1f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.20f));
        g.drawLine (bounds.getX() + 5.0f, bounds.getBottom() - 0.9f,
                    bounds.getRight() - 5.0f, bounds.getBottom() - 0.9f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        auto textArea = button.getLocalBounds().reduced (8, 1);
        auto text = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                : juce::TextButton::textColourOffId)
                        .withAlpha (button.isEnabled() ? 0.96f : 0.48f);

        if (button.isEnabled() && shouldDrawButtonAsHighlighted)
            text = text.brighter (shouldDrawButtonAsDown ? 0.22f : 0.14f);

        g.setColour (text);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(), textArea, juce::Justification::centred, 1);
    }

    void drawMenuBarBackground (juce::Graphics& g,
                                int width,
                                int height,
                                bool,
                                juce::MenuBarComponent&) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        juce::ColourGradient fill (juce::Colour::fromRGB (24, 33, 47).withAlpha (0.97f), bounds.getX(), bounds.getY(),
                                   juce::Colour::fromRGB (14, 20, 30).withAlpha (0.96f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 8.0f);

        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    }

    juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override
    {
        return juce::Font (juce::FontOptions (12.0f, juce::Font::bold));
    }

    void drawMenuBarItem (juce::Graphics& g,
                          int width,
                          int height,
                          int,
                          const juce::String& itemText,
                          bool isMouseOverItem,
                          bool isMenuOpen,
                          bool,
                          juce::MenuBarComponent&) override
    {
        auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (3.0f, 2.0f);

        if (isMouseOverItem || isMenuOpen)
        {
            const auto bg = (isMenuOpen ? juce::Colour::fromRGB (63, 126, 203) : juce::Colour::fromRGB (43, 79, 124)).withAlpha (0.86f);
            g.setColour (bg);
            g.fillRoundedRectangle (area, 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (area, 7.0f, 1.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (isMouseOverItem || isMenuOpen ? 0.98f : 0.90f));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (itemText, juce::Rectangle<int> (0, 0, width, height), juce::Justification::centred, false);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (12.3f, juce::Font::plain));
    }

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator,
                            bool isActive,
                            bool isHighlighted,
                            bool isTicked,
                            bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override
    {
        if (isSeparator)
        {
            auto line = area.toFloat().reduced (11.0f, 0.0f);
            line = line.withHeight (1.0f).withCentre (line.getCentre());
            g.setColour (juce::Colour::fromRGB (80, 113, 153).withAlpha (0.55f));
            g.fillRoundedRectangle (line, 0.5f);
            return;
        }

        auto row = area.toFloat().reduced (4.0f, 2.0f);
        if (isHighlighted)
        {
            juce::ColourGradient hi (juce::Colour::fromRGB (68, 152, 225).withAlpha (0.96f),
                                     row.getX(), row.getY(),
                                     juce::Colour::fromRGB (43, 103, 166).withAlpha (0.94f),
                                     row.getX(), row.getBottom(), false);
            g.setGradientFill (hi);
            g.fillRoundedRectangle (row, 6.0f);
            g.setColour (juce::Colour::fromRGB (252, 186, 104).withAlpha (0.92f));
            g.drawRoundedRectangle (row, 6.0f, 1.0f);
        }
        else
        {
            g.setColour (juce::Colour::fromRGB (16, 24, 36).withAlpha (0.44f));
            g.fillRoundedRectangle (row, 6.0f);
        }

        auto content = area.reduced (10, 0);
        const int iconSize = juce::jlimit (10, 15, content.getHeight() - 6);
        auto iconArea = content.removeFromLeft (juce::jmax (18, iconSize + 4));
        auto textArea = content.removeFromLeft (juce::jmax (20, content.getWidth() - 52));
        auto shortcutArea = content.removeFromRight (juce::jmin (96, content.getWidth()));

        if (icon != nullptr)
        {
            icon->drawWithin (g, iconArea.toFloat().withSizeKeepingCentre ((float) iconSize, (float) iconSize),
                              juce::RectanglePlacement::centred, 1.0f);
        }
        else if (isTicked)
        {
            g.setColour (isHighlighted ? juce::Colours::white.withAlpha (0.98f)
                                       : juce::Colour::fromRGB (248, 177, 93).withAlpha (0.95f));
            auto tick = getTickShape ((float) iconSize);
            tick.applyTransform (juce::AffineTransform::translation ((float) iconArea.getX() + 1.0f,
                                                                     (float) iconArea.getY() + 2.0f));
            g.fillPath (tick);
        }

        auto textCol = textColour != nullptr ? *textColour : findColour (juce::PopupMenu::textColourId);
        if (! isActive)
            textCol = textCol.withAlpha (0.45f);
        else if (isHighlighted)
            textCol = findColour (juce::PopupMenu::highlightedTextColourId);

        g.setColour (textCol);
        g.setFont (getPopupMenuFont().withHeight (12.6f));
        g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (textCol.withAlpha (0.72f));
            g.setFont (getPopupMenuFont().withHeight (11.5f));
            g.drawFittedText (shortcutKeyText, shortcutArea, juce::Justification::centredRight, 1);
        }

        if (hasSubMenu)
        {
            juce::Path arrow;
            const float cx = (float) (area.getRight() - 12);
            const float cy = (float) area.getCentreY();
            arrow.startNewSubPath (cx - 3.0f, cy - 4.0f);
            arrow.lineTo (cx + 2.0f, cy);
            arrow.lineTo (cx - 3.0f, cy + 4.0f);
            g.setColour (textCol.withAlpha (0.88f));
            g.strokePath (arrow, juce::PathStrokeType (1.4f));
        }
    }

    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar,
                                       juce::Graphics& g,
                                       int width,
                                       int height) override
    {
        juce::ignoreUnused (bar);
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

        juce::ColourGradient fill (juce::Colour::fromRGB (24, 34, 48).withAlpha (0.72f), bounds.getX(), bounds.getY(),
                                   juce::Colour::fromRGB (15, 21, 31).withAlpha (0.74f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    }

    void drawTabButton (juce::TabBarButton& button,
                        juce::Graphics& g,
                        bool isMouseOver,
                        bool isMouseDown) override
    {
        auto area = button.getActiveArea().toFloat().reduced (1.0f, 2.0f);
        const bool isFront = button.isFrontTab();

        auto base = isFront ? juce::Colour::fromRGB (55, 96, 140)
                            : juce::Colour::fromRGB (31, 45, 62);

        if (isMouseDown)
            base = base.brighter (0.18f);
        else if (isMouseOver)
            base = base.brighter (0.10f);

        juce::ColourGradient fill (base.brighter (0.09f), area.getX(), area.getY(),
                                   base.darker (0.18f), area.getX(), area.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (area, 6.0f);

        g.setColour (button.findColour (isFront ? juce::TabbedButtonBar::frontOutlineColourId
                                                : juce::TabbedButtonBar::tabOutlineColourId));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        if (isFront)
        {
            auto accent = area.removeFromTop (2.0f).reduced (7.0f, 0.0f);
            juce::ColourGradient accentFill (juce::Colour::fromRGB (146, 210, 255).withAlpha (0.88f), accent.getX(), accent.getY(),
                                             juce::Colour::fromRGB (146, 210, 255).withAlpha (0.0f), accent.getRight(), accent.getY(), false);
            g.setGradientFill (accentFill);
            g.fillRoundedRectangle (accent, 1.2f);
        }

        g.setColour (button.findColour (isFront ? juce::TabbedButtonBar::frontTextColourId
                                                : juce::TabbedButtonBar::tabTextColourId)
                        .withAlpha (button.isEnabled() ? 1.0f : 0.45f));
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawFittedText (button.getButtonText(),
                          area.toNearestInt().reduced (10, 1),
                          juce::Justification::centred,
                          1);
    }

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override
    {
        const int textWidth = 28 + juce::roundToInt ((float) button.getButtonText().length() * 7.4f);
        return juce::jlimit (74, juce::jmax (88, tabDepth * 3), textWidth);
    }

    void drawComboBox (juce::Graphics& g,
                       int width,
                       int height,
                       bool,
                       int buttonX,
                       int buttonY,
                       int buttonW,
                       int buttonH,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f, 0.5f);
        juce::ColourGradient fill (box.findColour (juce::ComboBox::backgroundColourId).brighter (0.08f),
                                   bounds.getX(), bounds.getY(),
                                   box.findColour (juce::ComboBox::backgroundColourId).darker (0.20f),
                                   bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 7.0f);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

        auto arrow = juce::Rectangle<float> ((float) buttonX, (float) buttonY, (float) buttonW, (float) buttonH).reduced (6.0f, 6.0f);
        juce::Path p;
        p.startNewSubPath (arrow.getX(), arrow.getY() + 1.0f);
        p.lineTo (arrow.getCentreX(), arrow.getBottom());
        p.lineTo (arrow.getRight(), arrow.getY() + 1.0f);
        p.closeSubPath();

        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (p);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (2, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearVertical)
        {
            const auto trackColour = slider.findColour (juce::Slider::trackColourId).withAlpha (0.92f);
            const auto thumbColour = slider.findColour (juce::Slider::thumbColourId);
            constexpr float thickness = 5.0f;

            if (style == juce::Slider::LinearHorizontal)
            {
                auto track = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - thickness * 0.5f,
                                                     (float) width, thickness);
                g.setColour (trackColour.withAlpha (0.36f));
                g.fillRoundedRectangle (track, 2.8f);

                auto active = track.withWidth (juce::jlimit (0.0f, (float) width, sliderPos - (float) x));
                g.setColour (trackColour);
                g.fillRoundedRectangle (active, 2.8f);

                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.fillEllipse (sliderPos - 5.0f, track.getCentreY() - 4.3f, 10.0f, 10.0f);
                g.setColour (thumbColour);
                g.fillEllipse (sliderPos - 5.0f, track.getCentreY() - 5.2f, 10.0f, 10.0f);
                g.setColour (juce::Colours::white.withAlpha (0.42f));
                g.drawEllipse (sliderPos - 5.0f, track.getCentreY() - 5.2f, 10.0f, 10.0f, 1.0f);
            }
            else
            {
                auto track = juce::Rectangle<float> ((float) x + (float) width * 0.5f - thickness * 0.5f, (float) y,
                                                     thickness, (float) height);
                g.setColour (trackColour.withAlpha (0.36f));
                g.fillRoundedRectangle (track, 2.8f);

                const float top = juce::jlimit ((float) y, (float) (y + height), sliderPos);
                auto active = juce::Rectangle<float> (track.getX(), top, track.getWidth(), track.getBottom() - top);
                g.setColour (trackColour);
                g.fillRoundedRectangle (active, 2.8f);

                g.setColour (juce::Colours::black.withAlpha (0.25f));
                g.fillEllipse (track.getCentreX() - 5.0f, sliderPos - 4.1f, 10.0f, 10.0f);
                g.setColour (thumbColour);
                g.fillEllipse (track.getCentreX() - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f);
                g.setColour (juce::Colours::white.withAlpha (0.42f));
                g.drawEllipse (track.getCentreX() - 5.0f, sliderPos - 5.0f, 10.0f, 10.0f, 1.0f);
            }

            return;
        }

        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId).withAlpha (0.92f));
        g.fillEllipse (bounds);

        auto ring = bounds.reduced (radius * 0.22f);
        g.setColour (slider.findColour (juce::Slider::trackColourId).withAlpha (0.36f));
        g.drawEllipse (ring, 2.8f);

        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, ring.getWidth() * 0.5f, ring.getHeight() * 0.5f, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (0.96f));
        g.strokePath (valueArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path pointer;
        const float pointerLength = radius * 0.56f;
        const float pointerThickness = 2.4f;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, 1.0f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillPath (pointer);

        g.setColour (juce::Colour::fromRGB (7, 11, 18).withAlpha (0.94f));
        g.fillEllipse (juce::Rectangle<float> (cx - 3.1f, cy - 3.1f, 6.2f, 6.2f));
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawEllipse (juce::Rectangle<float> (cx - 3.1f, cy - 3.1f, 6.2f, 6.2f), 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto tickArea = bounds.removeFromLeft (24.0f).reduced (2.0f);
        auto textArea = button.getLocalBounds().withTrimmedLeft (26);

        g.setColour (juce::Colour::fromRGB (30, 40, 54));
        g.fillRoundedRectangle (tickArea, 4.5f);
        g.setColour (juce::Colour::fromRGB (82, 107, 139).withAlpha (0.84f));
        g.drawRoundedRectangle (tickArea, 4.5f, 1.0f);

        if (button.getToggleState())
        {
            g.setColour (button.findColour (juce::ToggleButton::tickColourId));
            auto marker = tickArea.reduced (4.2f);
            g.fillRoundedRectangle (marker, 3.2f);
        }

        g.setColour (button.findColour (juce::ToggleButton::textColourId).withAlpha (button.isEnabled() ? 0.95f : 0.45f));
        g.setFont (12.6f);
        g.drawText (button.getButtonText(), textArea, juce::Justification::centredLeft, false);
    }

    void drawGroupComponentOutline (juce::Graphics& g,
                                    int width,
                                    int height,
                                    const juce::String& text,
                                    const juce::Justification&,
                                    juce::GroupComponent& group) override
    {
        if (text.trim().isEmpty())
            return;

        auto area = juce::Rectangle<float> (1.0f, 8.0f, (float) width - 2.0f, (float) height - 9.0f);
        auto outline = group.findColour (juce::GroupComponent::outlineColourId);
        auto fill = juce::Colour::fromRGB (20, 28, 40).withAlpha (0.54f);

        g.setColour (fill);
        g.fillRoundedRectangle (area, 11.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (area, 11.0f, 1.0f);

        auto topAccent = area.withTrimmedBottom (area.getHeight() - 2.0f).reduced (14.0f, 0.0f);
        juce::ColourGradient accent (outline.withAlpha (0.68f), topAccent.getX(), topAccent.getY(),
                                     outline.withAlpha (0.0f), topAccent.getRight(), topAccent.getY(), false);
        g.setGradientFill (accent);
        g.fillRoundedRectangle (topAccent, 1.0f);

        auto labelWidth = juce::jlimit (44.0f, (float) width - 20.0f, 18.0f + 7.2f * (float) text.length());
        auto labelArea = juce::Rectangle<float> (14.0f, 0.0f, labelWidth, 18.0f);

        g.setColour (juce::Colour::fromRGB (24, 34, 49).withAlpha (0.94f));
        g.fillRoundedRectangle (labelArea, 6.5f);
        g.setColour (outline.withAlpha (0.80f));
        g.drawRoundedRectangle (labelArea, 6.5f, 1.0f);
        g.setColour (group.findColour (juce::GroupComponent::textColourId));
        g.setFont (11.8f);
        g.drawFittedText (text, labelArea.toNearestInt().reduced (7, 1), juce::Justification::centredLeft, 1);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        auto area = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
        g.setColour (editor.findColour (juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle (area, 6.0f, 1.0f);
    }

    void drawTooltip (juce::Graphics& g,
                      const juce::String& text,
                      int width,
                      int height) override
    {
        auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
        juce::ColourGradient fill (findColour (juce::TooltipWindow::backgroundColourId).brighter (0.07f),
                                   bounds.getX(), bounds.getY(),
                                   findColour (juce::TooltipWindow::backgroundColourId).darker (0.20f),
                                   bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (bounds, 6.0f);

        g.setColour (findColour (juce::TooltipWindow::outlineColourId));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        g.setColour (findColour (juce::TooltipWindow::textColourId));
        g.setFont (juce::Font (juce::FontOptions (12.2f, juce::Font::plain)));
        g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (8, 5), juce::Justification::centredLeft, 3);
    }

    void drawScrollbar (juce::Graphics& g,
                        juce::ScrollBar& scrollbar,
                        int x,
                        int y,
                        int width,
                        int height,
                        bool isScrollbarVertical,
                        int thumbStartPosition,
                        int thumbSize,
                        bool isMouseOver,
                        bool isMouseDown) override
    {
        juce::ignoreUnused (scrollbar);

        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        g.setColour (findColour (juce::ScrollBar::backgroundColourId));
        g.fillRoundedRectangle (bounds.reduced (0.5f), 4.5f);

        auto thumb = isScrollbarVertical
                         ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStartPosition, (float) width - 4.0f, (float) thumbSize)
                         : juce::Rectangle<float> ((float) thumbStartPosition, (float) y + 2.0f, (float) thumbSize, (float) height - 4.0f);

        auto thumbColour = findColour (juce::ScrollBar::thumbColourId);
        if (isMouseDown)
            thumbColour = thumbColour.brighter (0.20f);
        else if (isMouseOver)
            thumbColour = thumbColour.brighter (0.10f);

        g.setColour (thumbColour);
        g.fillRoundedRectangle (thumb, 4.5f);
    }
};
}

namespace
{
juce::String createSafeTimestampTag()
{
    return juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
}

bool validateEditXmlFile (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File does not exist: " + file.getFullPathName();
        return false;
    }

    if (file.getSize() <= 0)
    {
        error = "File is empty: " + file.getFileName();
        return false;
    }

    juce::XmlDocument xml (file);
    std::unique_ptr<juce::XmlElement> root (xml.getDocumentElement());
    if (root == nullptr)
    {
        const auto parseError = xml.getLastParseError().trim();
        error = "XML parse error"
                + (parseError.isNotEmpty() ? ": " + parseError : juce::String ("."));
        return false;
    }

    if (root->getTagName().trim().isEmpty())
    {
        error = "Missing XML root element.";
        return false;
    }

    return true;
}

std::vector<juce::File> getSortedBackupFilesForEdit (const juce::File& editFile)
{
    std::vector<juce::File> backups;
    const auto backupDirectory = editFile.getParentDirectory().getChildFile ("Backups");
    if (! backupDirectory.isDirectory())
        return backups;

    juce::Array<juce::File> foundBackups;
    backupDirectory.findChildFiles (foundBackups,
                                    juce::File::findFiles,
                                    false,
                                    editFile.getFileNameWithoutExtension() + "_*.tracktionedit");

    backups.reserve ((size_t) foundBackups.size());
    for (const auto& file : foundBackups)
        backups.push_back (file);

    std::sort (backups.begin(), backups.end(),
               [] (const juce::File& a, const juce::File& b)
               {
                   return a.getLastModificationTime() > b.getLastModificationTime();
               });

    return backups;
}

void pruneEditBackups (const juce::File& editFile, int maxBackupsToKeep)
{
    if (maxBackupsToKeep < 1)
        return;

    auto backups = getSortedBackupFilesForEdit (editFile);
    if ((int) backups.size() <= maxBackupsToKeep)
        return;

    for (int i = maxBackupsToKeep; i < (int) backups.size(); ++i)
        [[maybe_unused]] const bool deleted = backups[(size_t) i].deleteFile();
}

bool restoreFileFromBackupCopy (const juce::File& backupFile, const juce::File& targetFile)
{
    if (! backupFile.existsAsFile())
        return false;

    if (targetFile.existsAsFile() && ! targetFile.deleteFile())
        return false;

    return backupFile.copyFileTo (targetFile);
}

double choosePreferredSampleRate (const juce::Array<double>& sampleRates)
{
    if (sampleRates.isEmpty())
        return 0.0;

    for (const double preferred : { 48000.0, 96000.0, 44100.0 })
        if (sampleRates.contains (preferred))
            return preferred;

    double bestRate = sampleRates.getFirst();
    double bestDistance = std::abs (bestRate - 48000.0);

    for (int i = 1; i < sampleRates.size(); ++i)
    {
        const double candidate = sampleRates.getUnchecked (i);
        const double distance = std::abs (candidate - 48000.0);

        if (distance < bestDistance
            || (std::abs (distance - bestDistance) <= 1.0e-6 && candidate > bestRate))
        {
            bestRate = candidate;
            bestDistance = distance;
        }
    }

    return bestRate;
}

int choosePreferredBufferSize (const juce::Array<int>& bufferSizes)
{
    if (bufferSizes.isEmpty())
        return 0;

    for (const int preferred : { 256, 512, 128 })
        if (bufferSizes.contains (preferred))
            return preferred;

    int bestSize = bufferSizes.getFirst();
    int bestDistance = std::abs (bestSize - 256);

    for (int i = 1; i < bufferSizes.size(); ++i)
    {
        const int candidate = bufferSizes.getUnchecked (i);
        const int distance = std::abs (candidate - 256);

        if (distance < bestDistance || (distance == bestDistance && candidate > bestSize))
        {
            bestSize = candidate;
            bestDistance = distance;
        }
    }

    return bestSize;
}
}

BeatMakerNoRecord::BeatMakerNoRecord()
{
    modernLookAndFeel = std::make_unique<ModernLookAndFeel>();
    setLookAndFeel (modernLookAndFeel.get());
    setOpaque (true);
    topMenuBar.setModel (this);

    Helpers::addAndMakeVisible (*this, { &sessionGroup, &arrangementGroup, &trackGroup, &clipEditGroup,
                                         &midiEditGroup, &audioEditGroup, &fxGroup, &mixerGroup,
                                         &workspaceSection, &mixerSection, &pianoSection,
                                         &workspaceGroup, &mixerAreaGroup, &pianoRollGroup,
                                         &topMenuBar,
                                         &newEditButton, &openEditButton, &saveButton, &saveAsButton,
                                         &undoButton, &redoButton, &helpButton,
                                         &focusSelectionButton, &centerPlayheadButton, &fitProjectButton,
                                         &playPauseButton, &stopButton, &returnToStartButton, &transportLoopButton,
                                         &setLoopToSelectionButton, &zoomInButton, &zoomOutButton, &zoomResetButton,
                                         &jumpPrevBarButton, &jumpNextBarButton,
                                         &showMarkerTrackButton, &showArrangerTrackButton,
                                         &addMarkerButton, &prevMarkerButton, &nextMarkerButton, &loopMarkersButton,
                                         &addSectionButton, &prevSectionButton, &nextSectionButton, &loopSectionButton,
                                         &addTrackButton, &addMidiTrackButton,
                                         &moveTrackUpButton, &moveTrackDownButton, &duplicateTrackButton, &colorTrackButton,
                                         &renameTrackButton, &addFloatingSynthTrackButton,
                                         &importAudioButton, &importMidiButton, &createMidiClipButton,
                                         &editToolLabel, &editToolSelectButton, &editToolPencilButton, &editToolScissorsButton, &editToolResizeButton,
                                         &leftDockPanelTabs,
                                         &leftDockPanelModeLabel, &leftDockPanelModeBox,
                                         &defaultSynthModeLabel, &defaultSynthModeBox,
                                         &copyButton, &cutButton, &pasteButton, &deleteButton, &duplicateButton,
                                         &splitButton, &trimStartButton, &trimEndButton,
                                         &moveStartToCursorButton, &moveEndToCursorButton,
                                         &nudgeLeftButton, &nudgeRightButton, &slipLeftButton, &slipRightButton,
                                         &moveToPrevButton, &moveToNextButton, &toggleClipLoopButton, &renameClipButton,
                                         &selectAllButton, &deselectAllButton, &splitAllTracksButton, &insertBarButton, &deleteBarButton,
                                         &quantizeButton, &quantizeTypeBox,
                                         &midiTransposeDownButton, &midiTransposeUpButton, &midiOctaveDownButton, &midiOctaveUpButton,
                                         &midiVelocityDownButton, &midiVelocityUpButton,
                                         &midiHumanizeTimingButton, &midiHumanizeVelocityButton, &midiLegatoButton, &midiBounceToAudioButton,
                                         &midiGenerateChordsButton, &midiGenerateArpButton, &midiGenerateBassButton, &midiGenerateDrumsButton,
                                         &midiToolsTabs,
                                         &chordDirectoryRootLabel, &chordDirectoryRootBox,
                                         &chordDirectoryScaleLabel, &chordDirectoryScaleBox,
                                         &chordDirectoryProgressionLabel, &chordDirectoryProgressionBox,
                                         &chordDirectoryBarsLabel, &chordDirectoryBarsBox,
                                         &chordDirectoryTimeSignatureLabel, &chordDirectoryTimeSignatureBox,
                                         &chordDirectoryOctaveLabel, &chordDirectoryOctaveBox,
                                         &chordDirectoryVoicingLabel, &chordDirectoryVoicingBox,
                                         &chordDirectoryDensityLabel, &chordDirectoryDensityBox,
                                         &chordDirectoryPreviewPresetLabel, &chordDirectoryPreviewPresetBox,
                                         &chordDirectoryVelocityLabel, &chordDirectoryVelocitySlider,
                                         &chordDirectorySwingLabel, &chordDirectorySwingSlider,
                                         &chordDirectoryPreviewButton, &chordDirectoryApplyButton,
                                         &chordDirectoryExportMidiButton, &chordDirectoryExportWavButton,
                                         &audioGainDownButton, &audioGainUpButton,
                                         &audioFadeInButton, &audioFadeOutButton, &audioClearFadesButton,
                                         &audioReverseButton, &audioSpeedDownButton, &audioSpeedUpButton,
                                         &audioPitchDownButton, &audioPitchUpButton,
                                         &audioAutoTempoButton, &audioWarpButton,
                                         &audioAlignToBarButton, &audioMake2BarLoopButton, &audioMake4BarLoopButton, &audioFillTransportLoopButton,
                                         &gridLabel, &gridBox,
                                         &fxChainLabel, &fxChainBox, &fxRefreshButton, &fxScanButton, &fxScanSkippedButton, &fxPrepPlaybackButton,
                                         &fxAddSamplerButton, &fxAddSynthButton, &fxAddEQButton, &fxAddCompButton,
                                         &fxAddReverbButton, &fxAddDelayButton,
                                         &fxAddExternalInstrumentButton, &fxAddExternalButton, &fxOpenEditorButton,
                                         &fxMoveUpButton, &fxMoveDownButton, &fxBypassButton, &fxDeleteButton,
                                         &trackMuteButton, &trackSoloButton,
                                         &trackVolumeLabel, &trackPanLabel, &tempoLabel,
                                         &trackVolumeSlider, &trackPanSlider, &tempoSlider,
                                         &editNameLabel, &transportInfoLabel, &workflowStateLabel, &selectedTrackLabel, &statusLabel, &contextHintLabel,
                                         &trackHeightLabel, &trackHeightSlider, &leftDockScrollSlider,
                                         &horizontalScrollSlider, &verticalScrollSlider, &timelineRuler, &trackAreaToolbar, &mixerToolsToolbar, &mixerArea,
                                         &leftDockSplitter, &workspaceMixerSplitter, &workspaceBottomSplitter, &mixerPianoSplitter,
                                         &channelRackGroup, &inspectorGroup, &builtInSynthGroup,
                                         &channelRackTrackLabel, &channelRackTrackBox,
                                         &channelRackPluginLabel, &channelRackPluginBox,
                                         &channelRackAddInstrumentButton, &channelRackAddFxButton, &channelRackOpenPluginButton,
                                         &inspectorTrackNameLabel, &inspectorRouteLabel, &inspectorPluginLabel, &inspectorMeterLabel,
                                         &builtInSynthTargetLabel,
                                         &builtInSynthPresetInitButton, &builtInSynthPresetWarmPadButton,
                                         &builtInSynthPresetPunchBassButton, &builtInSynthPresetBrightPluckButton,
                                         &builtInSynthCutoffLabel, &builtInSynthCutoffSlider,
                                         &builtInSynthResonanceLabel, &builtInSynthResonanceSlider,
                                         &builtInSynthEnvLabel, &builtInSynthEnvSlider,
                                         &builtInSynthDriveLabel, &builtInSynthDriveSlider,
                                         &builtInSynthAttackLabel, &builtInSynthAttackSlider,
                                         &builtInSynthReleaseLabel, &builtInSynthReleaseSlider,
                                         &builtInSynthReverbLabel, &builtInSynthReverbSlider,
                                         &builtInSynthDelayLabel, &builtInSynthDelaySlider,
                                         &builtInSynthOsc1Label, &builtInSynthOsc1Slider,
                                         &builtInSynthOsc2Label, &builtInSynthOsc2Slider,
                                         &builtInSynthOsc3Label, &builtInSynthOsc3Slider,
                                         &builtInSynthOsc4Label, &builtInSynthOsc4Slider,
                                         &builtInSynthUnisonLabel, &builtInSynthUnisonSlider,
                                         &builtInSynthWidthLabel, &builtInSynthWidthSlider,
                                         &builtInSynthChorusLabel, &builtInSynthChorusSlider,
                                         &builtInSynthMasterLabel, &builtInSynthMasterSlider,
                                         &pianoFloatToggleButton, &pianoEnsureInstrumentButton,
                                         &pianoOpenInstrumentButton, &pianoAlwaysOnTopButton, &pianoEditorModeTabs,
                                         &stepSequencerGroup, &stepSequencer, &pianoRollToolbar, &midiPianoRoll });

    addAndMakeVisible (zoomVerticalInButton);
    addAndMakeVisible (zoomVerticalOutButton);
    addAndMakeVisible (zoomVerticalResetButton);

    addAndMakeVisible (commandToolbar);

    workspaceSection.addAndMakeVisible (workspaceGroup);
    workspaceSection.addAndMakeVisible (timelineRuler);
    workspaceSection.addAndMakeVisible (trackAreaToolbar);
    workspaceSection.addAndMakeVisible (horizontalScrollSlider);
    workspaceSection.addAndMakeVisible (verticalScrollSlider);

    mixerSection.addAndMakeVisible (mixerAreaGroup);
    mixerSection.addAndMakeVisible (mixerArea);
    mixerSection.addAndMakeVisible (mixerToolsToolbar);
    mixerSection.addAndMakeVisible (channelRackGroup);
    mixerSection.addAndMakeVisible (channelRackPreview);
    mixerSection.addAndMakeVisible (mixerRackSplitter);
    mixerSection.addAndMakeVisible (rackInspectorSplitter);
    mixerSection.addAndMakeVisible (channelRackControlsSplitter);
    mixerSection.addAndMakeVisible (inspectorGroup);
    mixerSection.addAndMakeVisible (builtInSynthGroup);
    mixerSection.addAndMakeVisible (channelRackTrackLabel);
    mixerSection.addAndMakeVisible (channelRackTrackBox);
    mixerSection.addAndMakeVisible (channelRackPluginLabel);
    mixerSection.addAndMakeVisible (channelRackPluginBox);
    mixerSection.addAndMakeVisible (channelRackAddInstrumentButton);
    mixerSection.addAndMakeVisible (channelRackAddFxButton);
    mixerSection.addAndMakeVisible (channelRackOpenPluginButton);
    mixerSection.addAndMakeVisible (inspectorTrackNameLabel);
    mixerSection.addAndMakeVisible (inspectorRouteLabel);
    mixerSection.addAndMakeVisible (inspectorPluginLabel);
    mixerSection.addAndMakeVisible (inspectorMeterLabel);
    mixerSection.addAndMakeVisible (builtInSynthTargetLabel);
    mixerSection.addAndMakeVisible (builtInSynthPresetInitButton);
    mixerSection.addAndMakeVisible (builtInSynthPresetWarmPadButton);
    mixerSection.addAndMakeVisible (builtInSynthPresetPunchBassButton);
    mixerSection.addAndMakeVisible (builtInSynthPresetBrightPluckButton);
    mixerSection.addAndMakeVisible (builtInSynthCutoffLabel);
    mixerSection.addAndMakeVisible (builtInSynthCutoffSlider);
    mixerSection.addAndMakeVisible (builtInSynthResonanceLabel);
    mixerSection.addAndMakeVisible (builtInSynthResonanceSlider);
    mixerSection.addAndMakeVisible (builtInSynthEnvLabel);
    mixerSection.addAndMakeVisible (builtInSynthEnvSlider);
    mixerSection.addAndMakeVisible (builtInSynthDriveLabel);
    mixerSection.addAndMakeVisible (builtInSynthDriveSlider);
    mixerSection.addAndMakeVisible (builtInSynthAttackLabel);
    mixerSection.addAndMakeVisible (builtInSynthAttackSlider);
    mixerSection.addAndMakeVisible (builtInSynthReleaseLabel);
    mixerSection.addAndMakeVisible (builtInSynthReleaseSlider);
    mixerSection.addAndMakeVisible (builtInSynthReverbLabel);
    mixerSection.addAndMakeVisible (builtInSynthReverbSlider);
    mixerSection.addAndMakeVisible (builtInSynthDelayLabel);
    mixerSection.addAndMakeVisible (builtInSynthDelaySlider);
    mixerSection.addAndMakeVisible (builtInSynthOsc1Label);
    mixerSection.addAndMakeVisible (builtInSynthOsc1Slider);
    mixerSection.addAndMakeVisible (builtInSynthOsc2Label);
    mixerSection.addAndMakeVisible (builtInSynthOsc2Slider);
    mixerSection.addAndMakeVisible (builtInSynthOsc3Label);
    mixerSection.addAndMakeVisible (builtInSynthOsc3Slider);
    mixerSection.addAndMakeVisible (builtInSynthOsc4Label);
    mixerSection.addAndMakeVisible (builtInSynthOsc4Slider);
    mixerSection.addAndMakeVisible (builtInSynthUnisonLabel);
    mixerSection.addAndMakeVisible (builtInSynthUnisonSlider);
    mixerSection.addAndMakeVisible (builtInSynthWidthLabel);
    mixerSection.addAndMakeVisible (builtInSynthWidthSlider);
    mixerSection.addAndMakeVisible (builtInSynthChorusLabel);
    mixerSection.addAndMakeVisible (builtInSynthChorusSlider);
    mixerSection.addAndMakeVisible (builtInSynthMasterLabel);
    mixerSection.addAndMakeVisible (builtInSynthMasterSlider);

    pianoSection.addAndMakeVisible (pianoRollGroup);
    pianoSection.addAndMakeVisible (stepSequencerGroup);
    pianoSection.addAndMakeVisible (pianoFloatToggleButton);
    pianoSection.addAndMakeVisible (pianoEnsureInstrumentButton);
    pianoSection.addAndMakeVisible (pianoOpenInstrumentButton);
    pianoSection.addAndMakeVisible (pianoAlwaysOnTopButton);
    pianoSection.addAndMakeVisible (pianoEditorModeTabs);
    pianoSection.addAndMakeVisible (pianoStepSplitter);
    pianoSection.addAndMakeVisible (stepSequencer);
    pianoSection.addAndMakeVisible (stepSequencerHorizontalScrollBar);
    pianoSection.addAndMakeVisible (stepSequencerToolbar);
    pianoSection.addAndMakeVisible (pianoRollToolbar);
    pianoSection.addAndMakeVisible (midiPianoRoll);
    pianoSection.addAndMakeVisible (pianoRollHorizontalScrollBar);
    pianoSection.addAndMakeVisible (pianoRollVerticalScrollBar);

    for (size_t i = 0; i < detachedPanelWindows.size(); ++i)
    {
        const auto panel = static_cast<DetachedPanel> (i);
        detachedPanelWindows[i].container = std::make_unique<DetachedPanelContainer> (*this, panel);
    }

    for (auto* scrollbar : { &stepSequencerHorizontalScrollBar, &pianoRollHorizontalScrollBar, &pianoRollVerticalScrollBar })
    {
        scrollbar->setAutoHide (false);
        scrollbar->setWantsKeyboardFocus (false);
        scrollbar->setRepaintsOnMouseActivity (true);
        scrollbar->addListener (this);
    }

    configurePlaybackOnlyIO();
    setTimedPluginScanTimeoutMs (5000);
    engine.getPluginManager().knownPluginList.setCustomScanner (createTimedPluginScanCustomScanner());
    outputLevelMeter = engine.getDeviceManager().deviceManager.getOutputLevelGetter();
    setupCallbacks();
    setupSliders();
    setupCommandToolbar();
    setupTrackAreaToolbar();
    setupMixerToolsToolbar();
    setupPianoRollToolbar();
    setupStepSequencerToolbar();
    setTimelineEditToolFromUi (getTimelineEditTool(), false);
    tooltipWindow.setMillisecondsBeforeTipAppears (650);

    selectionManager.addChangeListener (this);
    leftDockPanelTabs.addChangeListener (this);
    midiToolsTabs.addChangeListener (this);
    pianoEditorModeTabs.addChangeListener (this);
    setMidiClipDoubleClickHandler ([this] (te::MidiClip& midiClip)
    {
        // Double-click should open in the docked editor by default for faster workflow.
        openMidiClipInPianoRoll (midiClip, false);
    });
    // Defer startup edit creation until the message loop is running.
    // Avoid modal startup prompts to keep boot robust on all systems.
    juce::Component::SafePointer<BeatMakerNoRecord> safeThis (this);
    juce::Timer::callAfterDelay (180, [safeThis]
    {
        if (auto* owner = safeThis.getComponent())
            owner->createNewEdit (false);
    });

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    if (auto* mappings = commandManager.getKeyMappings())
        mappings->resetToDefaultMappings();

    setWantsKeyboardFocus (true);
    updateButtonsFromState();
    setSize (1420, 900);

    juce::Component::SafePointer<BeatMakerNoRecord> safeThisFloat (this);
    juce::Timer::callAfterDelay (260, [safeThisFloat]
    {
        auto* owner = safeThisFloat.getComponent();
        if (owner == nullptr)
            return;

        auto& properties = owner->engine.getPropertyStorage().getPropertiesFile();
        const bool floatWorkspace = properties.getBoolValue ("windowFloatWorkspace", true);
        const bool floatMixer = properties.getBoolValue ("windowFloatMixer", true);
        const bool floatPiano = properties.getBoolValue ("windowFloatPiano", true);

        if (floatWorkspace)
            owner->windowPanelWorkspaceVisible = true;
        if (floatMixer)
            owner->windowPanelMixerVisible = true;
        if (floatPiano)
            owner->windowPanelPianoVisible = true;

        if (floatWorkspace && ! owner->isSectionFloating (FloatSection::workspace))
            owner->setSectionFloating (FloatSection::workspace, true);
        if (floatMixer && ! owner->isSectionFloating (FloatSection::mixer))
            owner->setSectionFloating (FloatSection::mixer, true);
        if (floatPiano && ! owner->isSectionFloating (FloatSection::piano))
            owner->setSectionFloating (FloatSection::piano, true);

        auto restoreDetachedPanel = [owner, &properties] (DetachedPanel panel, const char* propertyKey, bool& panelVisibleFlag)
        {
            if (! properties.getBoolValue (propertyKey, false))
                return;

            panelVisibleFlag = true;
            if (! owner->isDetachedPanelFloating (panel))
                owner->setDetachedPanelFloating (panel, true);
        };

        restoreDetachedPanel (DetachedPanel::arrangement, "windowFloatPanelArrangement", owner->windowPanelArrangementVisible);
        restoreDetachedPanel (DetachedPanel::tracks, "windowFloatPanelTracks", owner->windowPanelTrackVisible);
        restoreDetachedPanel (DetachedPanel::clip, "windowFloatPanelClip", owner->windowPanelClipVisible);
        restoreDetachedPanel (DetachedPanel::midi, "windowFloatPanelMidi", owner->windowPanelMidiVisible);
        restoreDetachedPanel (DetachedPanel::audio, "windowFloatPanelAudio", owner->windowPanelAudioVisible);
        restoreDetachedPanel (DetachedPanel::fx, "windowFloatPanelFx", owner->windowPanelFxVisible);
        restoreDetachedPanel (DetachedPanel::trackMixer, "windowFloatPanelTrackMixer", owner->windowPanelTrackMixerVisible);
        restoreDetachedPanel (DetachedPanel::mixerArea, "windowFloatPanelMixerArea", owner->windowPanelMixerAreaVisible);
        restoreDetachedPanel (DetachedPanel::channelRack, "windowFloatPanelChannelRack", owner->windowPanelChannelRackVisible);
        restoreDetachedPanel (DetachedPanel::inspector, "windowFloatPanelInspector", owner->windowPanelInspectorVisible);
        restoreDetachedPanel (DetachedPanel::pianoRoll, "windowFloatPanelPianoRoll", owner->windowPanelPianoRollVisible);
        restoreDetachedPanel (DetachedPanel::stepSequencer, "windowFloatPanelStepSequencer", owner->windowPanelStepSequencerVisible);

        owner->resized();
        owner->updateButtonsFromState();
    });
}

BeatMakerNoRecord::~BeatMakerNoRecord()
{
    shuttingDown = true;
    pendingUiUpdateFlags.store (pendingUiUpdateNone, std::memory_order_relaxed);
    pendingUiUpdatePosted.store (false, std::memory_order_relaxed);
    closeDetachedPanelWindows();
    closeFloatingWindows();
    topMenuBar.setModel (nullptr);
    setLookAndFeel (nullptr);
    modernLookAndFeel.reset();
    setMidiClipDoubleClickHandler ({});

    selectionManager.removeChangeListener (this);
    leftDockPanelTabs.removeChangeListener (this);
    midiToolsTabs.removeChangeListener (this);
    pianoEditorModeTabs.removeChangeListener (this);
    commandManager.setFirstCommandTarget (nullptr);
    stepSequencerHorizontalScrollBar.removeListener (this);
    pianoRollHorizontalScrollBar.removeListener (this);
    pianoRollVerticalScrollBar.removeListener (this);

    if (edit != nullptr)
        edit->getTransport().removeChangeListener (this);

    engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
}

void BeatMakerNoRecord::setupCallbacks()
{
    newEditButton.onClick = [this] { createNewEdit (false); };
    openEditButton.onClick = [this] { openEdit(); };
    saveButton.onClick = [this] { saveEdit(); };
    saveAsButton.onClick = [this] { saveEditAs(); };
    undoButton.onClick = [this] { if (edit != nullptr) edit->getUndoManager().undo(); };
    redoButton.onClick = [this] { if (edit != nullptr) edit->getUndoManager().redo(); };
    helpButton.onClick = [this] { showShortcutOverlay(); };
    focusSelectionButton.onClick = [this] { focusSelectedClipInView(); };
    centerPlayheadButton.onClick = [this] { centerPlayheadInView(); };
    fitProjectButton.onClick = [this] { fitProjectInView(); };

    playPauseButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            if (! edit->getTransport().isPlaying())
                prepareEditForPluginPlayback (true);

            EngineHelpers::togglePlay (*edit);
        }
    };
    stopButton.onClick = [this]
    {
        if (edit != nullptr)
            edit->getTransport().stop (false, false);
    };
    returnToStartButton.onClick = [this]
    {
        if (edit != nullptr)
            edit->getTransport().setPosition (te::TimePosition::fromSeconds (0.0));
    };
    transportLoopButton.onClick = [this] { toggleTransportLooping(); };
    setLoopToSelectionButton.onClick = [this] { setTransportLoopToSelectedClip(); };
    jumpPrevBarButton.onClick = [this] { jumpByBar (false); };
    jumpNextBarButton.onClick = [this] { jumpByBar (true); };
    zoomInButton.onClick = [this] { zoomTimeline (0.7); };
    zoomOutButton.onClick = [this] { zoomTimeline (1.4); };
    zoomResetButton.onClick = [this] { resetZoom(); };
    zoomVerticalInButton.onClick = [this]
    {
        const double nextHeight = juce::jlimit (trackHeightSlider.getMinimum(),
                                                trackHeightSlider.getMaximum(),
                                                trackHeightSlider.getValue() * 1.12);
        trackHeightSlider.setValue (nextHeight);
    };
    zoomVerticalOutButton.onClick = [this]
    {
        const double nextHeight = juce::jlimit (trackHeightSlider.getMinimum(),
                                                trackHeightSlider.getMaximum(),
                                                trackHeightSlider.getValue() * 0.89);
        trackHeightSlider.setValue (nextHeight);
    };
    zoomVerticalResetButton.onClick = [this]
    {
        trackHeightSlider.setValue (58.0);
    };
    showMarkerTrackButton.onClick = [this] { toggleMarkerTrackVisibility(); };
    showArrangerTrackButton.onClick = [this] { toggleArrangerTrackVisibility(); };
    addMarkerButton.onClick = [this] { addMarkerAtPlayhead(); };
    prevMarkerButton.onClick = [this] { jumpToMarker (false); };
    nextMarkerButton.onClick = [this] { jumpToMarker (true); };
    loopMarkersButton.onClick = [this] { setLoopBetweenNearestMarkers(); };
    addSectionButton.onClick = [this] { addArrangerSectionAtPlayhead(); };
    prevSectionButton.onClick = [this] { jumpToArrangerSection (false); };
    nextSectionButton.onClick = [this] { jumpToArrangerSection (true); };
    loopSectionButton.onClick = [this] { setLoopToCurrentArrangerSection(); };

    addTrackButton.onClick = [this] { addTrack(); };
    addMidiTrackButton.onClick = [this] { addMidiTrack(); };
    moveTrackUpButton.onClick = [this] { moveSelectedTrackVertically (false); };
    moveTrackDownButton.onClick = [this] { moveSelectedTrackVertically (true); };
    duplicateTrackButton.onClick = [this] { duplicateSelectedTrack(); };
    colorTrackButton.onClick = [this] { cycleSelectedTrackColour(); };
    renameTrackButton.onClick = [this] { renameSelectedTrack(); };
    addFloatingSynthTrackButton.onClick = [this] { addFloatingSynthTrack(); };
    importAudioButton.onClick = [this] { importAudioClip(); };
    importMidiButton.onClick = [this] { importMidiClip(); };
    createMidiClipButton.onClick = [this] { createMidiClip(); };
    pianoFloatToggleButton.setClickingTogglesState (true);
    pianoFloatToggleButton.onClick = [this]
    {
        toggleSectionFloating (FloatSection::piano);
        refreshPianoFloatingWindowUi();
    };
    pianoEnsureInstrumentButton.onClick = [this]
    {
        if (edit == nullptr)
            return;

        auto* track = [this] () -> te::AudioTrack*
        {
            if (auto* midiClip = getSelectedMidiClip())
                if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
                    return ownerTrack;

            return getSelectedTrackOrFirst();
        }();

        if (track == nullptr)
        {
            setStatus ("Select a MIDI clip or track first.");
            return;
        }

        const bool changed = ensureTrackHasInstrumentForMidiPlayback (*track);
        if (! changed && trackHasInstrumentPlugin (*track))
            setStatus ("Instrument already ready for MIDI playback on " + track->getName() + ".");

        refreshPianoFloatingWindowUi();
    };
    pianoOpenInstrumentButton.onClick = [this]
    {
        if (edit == nullptr)
            return;

        auto* track = [this] () -> te::AudioTrack*
        {
            if (auto* midiClip = getSelectedMidiClip())
                if (auto* ownerTrack = dynamic_cast<te::AudioTrack*> (midiClip->getTrack()))
                    return ownerTrack;

            return getSelectedTrackOrFirst();
        }();

        if (track == nullptr)
        {
            setStatus ("Select a MIDI clip or track first.");
            return;
        }

        ensureTrackHasInstrumentForMidiPlayback (*track);

        for (auto* plugin : track->pluginList.getPlugins())
        {
            if (plugin != nullptr && plugin->isSynth() && plugin->isEnabled())
            {
                plugin->showWindowExplicitly();
                setStatus ("Opened instrument UI: " + plugin->getName());
                refreshPianoFloatingWindowUi();
                return;
            }
        }

        setStatus ("No instrument UI available on selected track.");
    };
    pianoAlwaysOnTopButton.onClick = [this]
    {
        pianoFloatingAlwaysOnTop = pianoAlwaysOnTopButton.getToggleState();
        engine.getPropertyStorage().getPropertiesFile().setValue ("pianoFloatAlwaysOnTop", pianoFloatingAlwaysOnTop);

        if (pianoFloatingWindow != nullptr)
        {
            pianoFloatingWindow->setAlwaysOnTop (pianoFloatingAlwaysOnTop);
            pianoFloatingWindow->toFront (true);
        }

        refreshPianoFloatingWindowUi();
    };
    editToolSelectButton.setClickingTogglesState (true);
    editToolPencilButton.setClickingTogglesState (true);
    editToolScissorsButton.setClickingTogglesState (true);
    editToolResizeButton.setClickingTogglesState (true);
    editToolSelectButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::select); };
    editToolPencilButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::pencil); };
    editToolScissorsButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::scissors); };
    editToolResizeButton.onClick = [this] { setTimelineEditToolFromUi (TimelineEditTool::resize); };
    defaultSynthModeBox.onChange = [this]
    {
        if (defaultSynthModeBox.getSelectedId() <= 0)
            return;

        const auto selectedMode = getDefaultSynthModeSelection();
        engine.getPropertyStorage().getPropertiesFile().setValue ("defaultSynthMode",
                                                                  getDefaultSynthModeStorageValue (selectedMode));
        setStatus ("Default synth mode set to " + getDefaultSynthModeDisplayName (selectedMode) + ".");
    };
    leftDockPanelModeBox.onChange = [this]
    {
        if (leftDockPanelModeBox.getSelectedId() <= 0)
            return;

        setLeftDockPanelMode (getLeftDockPanelModeSelection(), true, true);
    };

    copyButton.onClick = [this] { copySelection(); };
    cutButton.onClick = [this] { cutSelection(); };
    pasteButton.onClick = [this] { pasteSelection(); };
    deleteButton.onClick = [this] { deleteSelectedItem(); };
    duplicateButton.onClick = [this] { duplicateSelectedClip(); };
    splitButton.onClick = [this] { splitSelectedClipAtPlayhead(); };
    trimStartButton.onClick = [this] { trimSelectedClipStartToPlayhead(); };
    trimEndButton.onClick = [this] { trimSelectedClipEndToPlayhead(); };
    moveStartToCursorButton.onClick = [this] { moveSelectedClipBoundaryToCursor (true); };
    moveEndToCursorButton.onClick = [this] { moveSelectedClipBoundaryToCursor (false); };
    nudgeLeftButton.onClick = [this] { nudgeSelectedClip (false); };
    nudgeRightButton.onClick = [this] { nudgeSelectedClip (true); };
    slipLeftButton.onClick = [this] { slipSelectedClipContent (false); };
    slipRightButton.onClick = [this] { slipSelectedClipContent (true); };
    moveToPrevButton.onClick = [this] { moveSelectedClipToNeighbour (false); };
    moveToNextButton.onClick = [this] { moveSelectedClipToNeighbour (true); };
    toggleClipLoopButton.onClick = [this] { toggleSelectedClipLooping(); };
    renameClipButton.onClick = [this] { renameSelectedClip(); };
    selectAllButton.onClick = [this] { selectAllEditableItems(); };
    deselectAllButton.onClick = [this] { selectionManager.deselectAll(); };
    splitAllTracksButton.onClick = [this] { splitAllTracksAtPlayhead(); };
    insertBarButton.onClick = [this] { insertBarAtPlayhead(); };
    deleteBarButton.onClick = [this] { deleteBarAtPlayhead(); };
    quantizeButton.onClick = [this] { quantizeSelectedMidiClip(); };
    midiTransposeDownButton.onClick = [this] { transposeSelectedMidiNotes (-1); };
    midiTransposeUpButton.onClick = [this] { transposeSelectedMidiNotes (1); };
    midiOctaveDownButton.onClick = [this] { transposeSelectedMidiNotes (-12); };
    midiOctaveUpButton.onClick = [this] { transposeSelectedMidiNotes (12); };
    midiVelocityDownButton.onClick = [this] { adjustSelectedMidiNoteVelocity (-8); };
    midiVelocityUpButton.onClick = [this] { adjustSelectedMidiNoteVelocity (8); };
    midiHumanizeTimingButton.onClick = [this] { humanizeSelectedMidiTiming (0.08); };
    midiHumanizeVelocityButton.onClick = [this] { humanizeSelectedMidiVelocity (10); };
    midiLegatoButton.onClick = [this] { legatoSelectedMidiNotes(); };
    midiBounceToAudioButton.onClick = [this] { bounceSelectedMidiClipToAudio(); };
    midiGenerateChordsButton.onClick = [this] { generateMidiChordProgression(); };
    midiGenerateArpButton.onClick = [this] { generateMidiArpeggioPattern(); };
    midiGenerateBassButton.onClick = [this] { generateMidiBasslinePattern(); };
    midiGenerateDrumsButton.onClick = [this] { generateMidiDrumPattern(); };
    chordDirectoryPreviewButton.onClick = [this] { previewChordScaleDirectoryPattern(); };
    chordDirectoryApplyButton.onClick = [this] { generateMidiChordScaleDirectoryPattern(); };
    chordDirectoryExportMidiButton.onClick = [this] { exportChordScaleDirectorySelectionAsMidi(); };
    chordDirectoryExportWavButton.onClick = [this] { exportChordScaleDirectorySelectionAsWav(); };
    audioGainDownButton.onClick = [this] { adjustSelectedAudioClipGain (-1.0f); };
    audioGainUpButton.onClick = [this] { adjustSelectedAudioClipGain (1.0f); };
    audioFadeInButton.onClick = [this] { setSelectedAudioClipFade (true); };
    audioFadeOutButton.onClick = [this] { setSelectedAudioClipFade (false); };
    audioClearFadesButton.onClick = [this] { clearSelectedAudioClipFades(); };
    audioReverseButton.onClick = [this] { toggleSelectedAudioClipReverse(); };
    audioSpeedDownButton.onClick = [this] { scaleSelectedAudioClipSpeed (0.5); };
    audioSpeedUpButton.onClick = [this] { scaleSelectedAudioClipSpeed (2.0); };
    audioPitchDownButton.onClick = [this] { adjustSelectedAudioClipPitch (-1.0f); };
    audioPitchUpButton.onClick = [this] { adjustSelectedAudioClipPitch (1.0f); };
    audioAutoTempoButton.onClick = [this] { toggleSelectedAudioClipAutoTempo(); };
    audioWarpButton.onClick = [this] { toggleSelectedAudioClipWarp(); };
    audioAlignToBarButton.onClick = [this] { alignSelectedClipToBar(); };
    audioMake2BarLoopButton.onClick = [this] { makeSelectedClipLoop (2); };
    audioMake4BarLoopButton.onClick = [this] { makeSelectedClipLoop (4); };
    audioFillTransportLoopButton.onClick = [this] { fillTransportLoopWithSelectedClip(); };
    fxRefreshButton.onClick = [this] { refreshSelectedTrackPluginList(); };
    fxScanButton.onClick = [this] { openPluginScanDialog(); };
    fxScanSkippedButton.onClick = [this] { scanSkippedPlugins(); };
    fxPrepPlaybackButton.onClick = [this] { prepareEditForPluginPlayback (true); };
    fxAddSamplerButton.onClick = [this] { setStatus ("Built-in plugins are removed from this build."); };
    fxAddSynthButton.onClick = [this] { setStatus ("Built-in plugins are removed from this build."); };
    fxAddEQButton.onClick = [this] { setStatus ("Built-in plugins are removed from this build."); };
    fxAddCompButton.onClick = [this] { setStatus ("Built-in plugins are removed from this build."); };
    fxAddReverbButton.onClick = [this] { setStatus ("Built-in plugins are removed from this build."); };
    fxAddDelayButton.onClick = [this] { setStatus ("Built-in plugins are removed from this build."); };
    fxAddExternalInstrumentButton.onClick = [this] { addExternalInstrumentPluginToSelectedTrack(); };
    fxAddExternalButton.onClick = [this] { addExternalPluginToSelectedTrack(); };
    fxOpenEditorButton.onClick = [this] { openSelectedTrackPluginEditor(); };
    fxMoveUpButton.onClick = [this] { moveSelectedTrackPlugin (false); };
    fxMoveDownButton.onClick = [this] { moveSelectedTrackPlugin (true); };
    fxBypassButton.onClick = [this] { toggleSelectedTrackPluginBypass(); };
    fxDeleteButton.onClick = [this] { deleteSelectedTrackPlugin(); };
    fxChainBox.onChange = [this]
    {
        if (updatingTrackControls)
            return;

        if (auto* plugin = getSelectedTrackPlugin())
            selectionManager.selectOnly (plugin);

        updateButtonsFromState();
    };
    channelRackTrackBox.onChange = [this]
    {
        if (updatingChannelRackControls || edit == nullptr)
            return;

        auto tracks = te::getAudioTracks (*edit);
        const int selectedIndex = channelRackTrackBox.getSelectedId() - 1;
        if (selectedIndex < 0 || selectedIndex >= tracks.size())
            return;

        if (auto* track = tracks.getUnchecked (selectedIndex))
            selectionManager.selectOnly (track);

        updateTrackControlsFromSelection();
        updateButtonsFromState();
    };
    channelRackPluginBox.onChange = [this]
    {
        if (updatingChannelRackControls)
            return;

        auto* track = getSelectedTrackOrFirst();
        if (track == nullptr)
            return;

        auto plugins = track->pluginList.getPlugins();
        const int selectedIndex = channelRackPluginBox.getSelectedId() - 1;
        if (selectedIndex < 0 || selectedIndex >= channelRackPluginIndexMap.size())
            return;

        const int pluginIndex = channelRackPluginIndexMap.getUnchecked (selectedIndex);
        if (pluginIndex < 0 || pluginIndex >= plugins.size())
            return;

        if (auto* plugin = plugins[pluginIndex].get())
        {
            selectionManager.selectOnly (plugin);
            fxChainBox.setSelectedId (pluginIndex + 1, juce::dontSendNotification);
        }

        updateButtonsFromState();
    };
    channelRackAddInstrumentButton.onClick = [this] { addExternalInstrumentPluginToSelectedTrack(); };
    channelRackAddFxButton.onClick = [this] { addExternalPluginToSelectedTrack(); };
    channelRackOpenPluginButton.onClick = [this] { openSelectedTrackPluginEditor(); };
    builtInSynthPresetInitButton.onClick = [this] { applyBuiltInSynthPreset (BuiltInSynthPreset::init); };
    builtInSynthPresetWarmPadButton.onClick = [this] { applyBuiltInSynthPreset (BuiltInSynthPreset::warmPad); };
    builtInSynthPresetPunchBassButton.onClick = [this] { applyBuiltInSynthPreset (BuiltInSynthPreset::punchBass); };
    builtInSynthPresetBrightPluckButton.onClick = [this] { applyBuiltInSynthPreset (BuiltInSynthPreset::brightPluck); };
    builtInSynthCutoffSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthResonanceSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthEnvSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthDriveSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthAttackSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthReleaseSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthReverbSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthDelaySlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthOsc1Slider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthOsc2Slider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthOsc3Slider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthOsc4Slider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthUnisonSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthWidthSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthChorusSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };
    builtInSynthMasterSlider.onValueChange = [this] { applyBuiltInSynthMacroFromUI(); };

    trackMuteButton.onClick = [this] { applyTrackMixerFromUI(); };
    trackSoloButton.onClick = [this] { applyTrackMixerFromUI(); };
    trackVolumeSlider.onValueChange = [this] { applyTrackMixerFromUI(); };
    trackPanSlider.onValueChange = [this] { applyTrackMixerFromUI(); };
    tempoSlider.onValueChange = [this] { applyTempoFromUI(); };
    trackHeightSlider.onValueChange = [this] { applyTrackHeightFromUI(); };
    leftDockScrollSlider.onValueChange = [this] { resized(); };
    horizontalScrollSlider.onValueChange = [this] { applyHorizontalScrollFromUI(); };
    verticalScrollSlider.onValueChange = [this] { applyVerticalScrollFromUI(); };

    auto persistLayoutRatios = [this]
    {
        auto& properties = engine.getPropertyStorage().getPropertiesFile();
        properties.setValue ("layoutLeftDockRatio", leftDockWidthRatio);
        properties.setValue ("layoutWorkspaceMixerRatio", workspaceMixerWidthRatio);
        properties.setValue ("layoutWorkspaceBottomRatio", workspaceBottomHeightRatio);
        properties.setValue ("layoutMixerPianoRatio", mixerPianoHeightRatio);
        properties.setValue ("layoutPianoStepRatio", pianoStepHeightRatio);
        properties.setValue ("layoutMixerRackRatio", mixerRackHeightRatio);
        properties.setValue ("layoutRackInspectorRatio", rackInspectorWidthRatio);
        properties.setValue ("layoutRackControlsRatio", channelRackControlsHeightRatio);
    };

    auto relayoutMixerSection = [this]
    {
        if (mixerSection.isShowing())
            layoutSectionContent (FloatSection::mixer, mixerSection.getLocalBounds());

        if (isSectionFloating (FloatSection::mixer))
            mixerSection.repaint();
        else
            resized();
    };

    leftDockSplitter.onDeltaDrag = [this, persistLayoutRatios] (int delta)
    {
        if (currentBodyWidthForResize <= 0)
            return;

        const int minLeft = 300;
        const int maxLeft = juce::jmax (minLeft, currentBodyWidthForResize - 430);
        const int currentWidth = juce::roundToInt (leftDockWidthRatio * (float) currentBodyWidthForResize);
        const int nextWidth = juce::jlimit (minLeft, maxLeft, currentWidth + delta);
        leftDockWidthRatio = (float) nextWidth / (float) juce::jmax (1, currentBodyWidthForResize);
        persistLayoutRatios();
        resized();
    };

    workspaceMixerSplitter.onDeltaDrag = [this, persistLayoutRatios] (int delta)
    {
        if (currentRightDockWidthForResize <= 0)
            return;

        const int minWorkspaceWidth = 320;
        const int minMixerWidth = 320;
        const int maxWorkspaceWidth = juce::jmax (minWorkspaceWidth, currentRightDockWidthForResize - minMixerWidth);
        const int currentWidth = juce::roundToInt (workspaceMixerWidthRatio * (float) currentRightDockWidthForResize);
        const int nextWidth = juce::jlimit (minWorkspaceWidth, maxWorkspaceWidth, currentWidth + delta);
        workspaceMixerWidthRatio = (float) nextWidth / (float) juce::jmax (1, currentRightDockWidthForResize);
        persistLayoutRatios();
        resized();
    };

    workspaceBottomSplitter.onDeltaDrag = [this, persistLayoutRatios] (int delta)
    {
        if (currentRightDockHeightForResize <= 0)
            return;

        const int minBottom = 180;
        const int maxBottom = juce::jmax (minBottom, currentRightDockHeightForResize - 190);
        const int currentHeight = juce::roundToInt (workspaceBottomHeightRatio * (float) currentRightDockHeightForResize);
        // Bottom pane height should decrease when dragging the splitter downward.
        const int nextHeight = juce::jlimit (minBottom, maxBottom, currentHeight - delta);
        workspaceBottomHeightRatio = (float) nextHeight / (float) juce::jmax (1, currentRightDockHeightForResize);
        persistLayoutRatios();
        resized();
    };

    mixerPianoSplitter.onDeltaDrag = [this, persistLayoutRatios] (int delta)
    {
        if (currentBottomDockHeightForResize <= 0)
            return;

        const int minTop = 100;
        const int maxTop = juce::jmax (minTop, currentBottomDockHeightForResize - 120);
        const int currentHeight = juce::roundToInt (mixerPianoHeightRatio * (float) currentBottomDockHeightForResize);
        const int nextHeight = juce::jlimit (minTop, maxTop, currentHeight + delta);
        mixerPianoHeightRatio = (float) nextHeight / (float) juce::jmax (1, currentBottomDockHeightForResize);
        persistLayoutRatios();
        resized();
    };

    mixerRackSplitter.onDeltaDrag = [this, persistLayoutRatios, relayoutMixerSection] (int delta)
    {
        if (currentMixerSectionHeightForResize <= 0)
            return;

        const int minMixerHeight = 140;
        const int maxMixerHeight = juce::jmax (minMixerHeight, currentMixerSectionHeightForResize - 150);
        const int currentHeight = juce::roundToInt (mixerRackHeightRatio * (float) currentMixerSectionHeightForResize);
        const int nextHeight = juce::jlimit (minMixerHeight, maxMixerHeight, currentHeight + delta);
        mixerRackHeightRatio = (float) nextHeight / (float) juce::jmax (1, currentMixerSectionHeightForResize);
        persistLayoutRatios();
        relayoutMixerSection();
    };

    rackInspectorSplitter.onDeltaDrag = [this, persistLayoutRatios, relayoutMixerSection] (int delta)
    {
        if (currentRackSectionWidthForResize <= 0)
            return;

        const int minRackWidth = juce::jmax (120, currentRackSectionWidthForResize / 4);
        const int maxRackWidth = juce::jmax (minRackWidth, currentRackSectionWidthForResize - juce::jmax (120, currentRackSectionWidthForResize / 4));
        const int currentWidth = juce::roundToInt (rackInspectorWidthRatio * (float) currentRackSectionWidthForResize);
        const int nextWidth = juce::jlimit (minRackWidth, maxRackWidth, currentWidth + delta);
        rackInspectorWidthRatio = (float) nextWidth / (float) juce::jmax (1, currentRackSectionWidthForResize);
        persistLayoutRatios();
        relayoutMixerSection();
    };

    channelRackControlsSplitter.onDeltaDrag = [this, persistLayoutRatios, relayoutMixerSection] (int delta)
    {
        if (currentChannelRackSectionHeightForResize <= 0)
            return;

        const int minControlsHeight = 90;
        const int maxControlsHeight = juce::jmax (minControlsHeight, currentChannelRackSectionHeightForResize - 120);
        const int currentHeight = juce::roundToInt (channelRackControlsHeightRatio * (float) currentChannelRackSectionHeightForResize);
        const int nextHeight = juce::jlimit (minControlsHeight, maxControlsHeight, currentHeight + delta);
        channelRackControlsHeightRatio = (float) nextHeight / (float) juce::jmax (1, currentChannelRackSectionHeightForResize);
        persistLayoutRatios();
        relayoutMixerSection();
    };

    pianoStepSplitter.onDeltaDrag = [this, persistLayoutRatios] (int delta)
    {
        if (currentPianoSectionHeightForResize <= 0)
            return;

        const int minStepHeight = 96;
        const int maxStepHeight = juce::jmax (minStepHeight, currentPianoSectionHeightForResize - 120);
        const int currentHeight = juce::roundToInt (pianoStepHeightRatio * (float) currentPianoSectionHeightForResize);
        const int nextHeight = juce::jlimit (minStepHeight, maxStepHeight, currentHeight + delta);
        pianoStepHeightRatio = (float) nextHeight / (float) juce::jmax (1, currentPianoSectionHeightForResize);
        persistLayoutRatios();

        if (pianoSection.isShowing())
            layoutSectionContent (FloatSection::piano, pianoSection.getLocalBounds());

        if (isSectionFloating (FloatSection::piano))
            pianoSection.repaint();
        else
            resized();
    };
}

void BeatMakerNoRecord::scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == nullptr || updatingEditorScrollBars)
        return;

    if (scrollBarThatHasMoved == &pianoRollHorizontalScrollBar)
    {
        auto* midiClip = getSelectedMidiClip();
        if (midiClip == nullptr)
            return;

        syncPianoRollViewportToSelection (false);
        const double clipLengthBeats = juce::jmax (1.0 / 16.0, getMidiClipLengthBeats (*midiClip));
        const double viewLengthBeats = juce::jlimit (1.0 / 16.0, clipLengthBeats, pianoRollViewLengthBeats);
        const double maxStartBeat = juce::jmax (0.0, clipLengthBeats - viewLengthBeats);
        pianoRollViewStartBeat = juce::jlimit (0.0, maxStartBeat, newRangeStart);
        updatePianoRollScrollbarsFromViewport();
        midiPianoRoll.repaint();
        return;
    }

    if (scrollBarThatHasMoved == &pianoRollVerticalScrollBar)
    {
        syncPianoRollViewportToSelection (false);
        const auto noteLimits = pianoRollVerticalScrollBar.getRangeLimit();
        const double minLowest = noteLimits.getStart();
        const double maxLowest = juce::jmax (minLowest, noteLimits.getEnd() - (double) juce::jmax (1, pianoRollViewNoteCount));
        pianoRollViewLowestNote = juce::roundToInt (juce::jlimit (minLowest, maxLowest, newRangeStart));
        updatePianoRollScrollbarsFromViewport();
        midiPianoRoll.repaint();
        return;
    }

    if (scrollBarThatHasMoved == &stepSequencerHorizontalScrollBar)
    {
        if (stepSequencerDragMode != StepSequencerDragMode::none || getSelectedMidiClip() == nullptr)
            return;

        setStepSequencerViewportStartBeat (newRangeStart, true);
        updateStepSequencerScrollbarFromPageContext();
        stepSequencer.repaint();
        midiPianoRoll.repaint();
    }
}

bool BeatMakerNoRecord::hasAnyClipInEdit() const
{
    if (edit == nullptr)
        return false;

    for (auto* track : te::getAudioTracks (*edit))
        if (! track->getClips().isEmpty())
            return true;

    return false;
}

bool BeatMakerNoRecord::hasAnyMidiClipInEdit() const
{
    if (edit == nullptr)
        return false;

    for (auto* track : te::getAudioTracks (*edit))
        for (auto* clip : track->getClips())
            if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
                return true;

    return false;
}

bool BeatMakerNoRecord::isNoRecordPolicyActive() const
{
    auto& audioDeviceManager = engine.getDeviceManager().deviceManager;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    audioDeviceManager.getAudioDeviceSetup (setup);

    if (setup.inputChannels.countNumberOfSetBits() > 0)
        return false;

    for (auto midiIn : engine.getDeviceManager().getMidiInDevices())
        if (midiIn->isEnabled())
            return false;

    if (editComponent != nullptr && editComponent->getEditViewState().showRecordControls.get())
        return false;

    if (edit != nullptr)
    {
        for (auto input : edit->getAllInputDevices())
            if (! input->getTargets().isEmpty())
                return false;
    }

    return true;
}

void BeatMakerNoRecord::showShortcutOverlay()
{
    juce::String text;
    text << "Global\n";
    text << "  Space: Play/Pause\n";
    text << "  Cmd+S: Save\n";
    text << "  Cmd+Shift+S: Save As\n";
    text << "  Cmd+Z / Cmd+Shift+Z: Undo / Redo\n";
    text << "  Cmd+C / Cmd+X / Cmd+V: Copy / Cut / Paste\n";
    text << "  Cmd+D: Duplicate clip\n";
    text << "  F / C / A: Focus selection / Center playhead / Fit project\n";
    text << "  1 / 2 / 3 / 4: Select / Pencil / Scissors / Resize tool\n";
    text << "  Cmd/Ctrl+1/2/3: UI Density Compact / Comfortable / Accessible\n";
    text << "  Cmd/Ctrl+4/5/6/7: Panels All / Project / Editing / Sound\n";
    text << "  Cmd/Ctrl+Alt+R/F/K/V: Step randomize / 4-on-floor / clear page / vary velocity\n";
    text << "  Cmd/Ctrl+Alt+Left/Right: Shift step page left/right\n";
    text << "  Q / L / B / N: Quantize / Loop toggle / MIDI bounce / Create MIDI clip\n";
    text << "  , / . : MIDI transpose -1 / +1 semitone\n";
    text << "  - / = : MIDI velocity -8 / +8\n";
    text << "  W / M / P: Float/Dock Timeline / Mixer / Piano\n";
    text << "  Alt+1/2/3: Piano editor Split / Piano / Steps\n";
    text << "  Cmd/Ctrl+Shift+W/M/P: Float/Dock Timeline / Mixer / Piano\n";
    text << "  Cmd/Ctrl+Shift+D: Dock all floating panels\n";
    text << "  Delete / Backspace: Delete selected item\n";
    text << "  ?: Show this shortcuts panel\n\n";
    text << "Timeline + Arrangement\n";
    text << "  Shift-drag ruler: Set loop range\n";
    text << "  Alt-drag (or middle-drag) ruler: Pan timeline viewport\n";
    text << "  Click/drag ruler: Move playhead\n";
    text << "  Mouse wheel on ruler: Move playhead by grid\n";
    text << "  Shift+Wheel on ruler/workspace: Pan timeline\n";
    text << "  Cmd/Ctrl+Wheel on ruler/workspace: Zoom timeline around pointer/playhead\n";
    text << "  Double-click ruler: Center around clicked position\n";
    text << "  Left/Right arrows: Move playhead by grid | Shift+Left/Right: jump by bar\n";
    text << "  Cmd/Ctrl+Left/Right or [ ]: Pan timeline window\n";
    text << "  Up/Down: Scroll track area | Shift+Up/Down: Move selected track\n";
    text << "  Home/End: Jump playhead to start/end\n";
    text << "  Right-click ruler: Timeline quick actions\n";
    text << "  Right-click clip/track/empty area: Context actions\n\n";
    text << "Audio Beatmaker\n";
    text << "  Import Audio: drops to next bar and auto-arms loop if sample fits bars\n";
    text << "  Align To Bar: snap selected clip start to nearest bar line\n";
    text << "  Make 2-Bar / 4-Bar Loop: quickly build looped arrangement from selected clip\n";
    text << "  Fill Transport Loop: repeat selected clip across current loop range\n\n";
    text << "MIDI Generation\n";
    text << "  Gen Chords / Arp / Bass / Drums: replace selected MIDI clip notes with generated patterns\n";
    text << "  Use Quantize + Humanize after generation for variation and groove control\n\n";
    text << "Piano Roll\n";
    text << "  Pencil tool: Left-click empty to add note\n";
    text << "  Select tool: Drag note body to move note\n";
    text << "  Resize tool (or drag note edge): Resize note\n";
    text << "  Scissors tool: Click note to split at cursor/grid\n";
    text << "  Shift-drag note: Change velocity\n";
    text << "  Middle-drag (or Cmd/Ctrl-drag): Pan piano-roll time/pitch view\n";
    text << "  Mouse wheel: Scroll pitch | Shift+wheel: scroll time | Cmd/Ctrl+wheel: zoom time | Alt+wheel: zoom pitch\n";
    text << "  Right-click note: Delete note\n";

    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                            "Shortcuts & Gestures",
                                            text,
                                            "Close",
                                            this);
}

bool BeatMakerNoRecord::confirmDestructiveAction (const juce::String& title, const juce::String& message)
{
    return juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                                title,
                                                message + "\n\nYou can undo this with Cmd+Z if available.",
                                                "Delete",
                                                "Cancel",
                                                this);
}

void BeatMakerNoRecord::refreshScaffoldState (bool force)
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    if (! force && (nowMs - lastScaffoldRefreshMs) < 750.0)
        return;

    lastScaffoldRefreshMs = nowMs;

    DawScaffold::RuntimeSignals signals;
    signals.noRecordPolicy = isNoRecordPolicyActive();
    signals.hasEditSession = (edit != nullptr);
    signals.hasTracks = (edit != nullptr) && ! te::getAudioTracks (*edit).isEmpty();
    signals.hasAnyClip = hasAnyClipInEdit();
    signals.hasAnyMidiClip = hasAnyMidiClipInEdit();
    signals.hasTimelineTools = (editComponent != nullptr);
    signals.hasArrangementTools = addTrackButton.isEnabled() && splitAllTracksButton.isEnabled();
    signals.hasMixerTools = trackVolumeSlider.isEnabled() && trackPanSlider.isEnabled() && trackMuteButton.isEnabled();
    signals.hasQuantizeTools = (quantizeTypeBox.getNumItems() > 0);
    signals.hasFileWorkflow = newEditButton.isEnabled() && openEditButton.isEnabled() && saveButton.isEnabled();
    dawScaffold.update (signals);
}

void BeatMakerNoRecord::configurePlaybackOnlyIO()
{
    auto& audioDeviceManager = engine.getDeviceManager().deviceManager;
    juce::StringArray warnings;

    const auto initResult = audioDeviceManager.initialise (0, 2, nullptr, true);
    if (initResult.isNotEmpty())
        warnings.add (initResult);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    audioDeviceManager.getAudioDeviceSetup (setup);
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();

    if (auto* currentDevice = audioDeviceManager.getCurrentAudioDevice())
    {
        const auto preferredRate = choosePreferredSampleRate (currentDevice->getAvailableSampleRates());
        if (preferredRate > 0.0)
            setup.sampleRate = preferredRate;

        const auto preferredBuffer = choosePreferredBufferSize (currentDevice->getAvailableBufferSizes());
        if (preferredBuffer > 0)
            setup.bufferSize = preferredBuffer;
    }

    const auto setupResult = audioDeviceManager.setAudioDeviceSetup (setup, true);
    if (setupResult.isNotEmpty())
        warnings.add (setupResult);

    for (auto midiIn : engine.getDeviceManager().getMidiInDevices())
        midiIn->setEnabled (false);

    if (! warnings.isEmpty())
        setStatus ("Audio device warning: " + warnings.joinIntoString (" | "));
}

void BeatMakerNoRecord::applyHighQualitySettingsToAudioClip (te::AudioClipBase& clip)
{
    clip.setUsesProxy (true);
    clip.setResamplingQuality (te::ResamplingQuality::sincBest);

    const auto preferredMode = te::TimeStretcher::checkModeIsAvailable (te::TimeStretcher::defaultMode);
    if (preferredMode != te::TimeStretcher::disabled)
        clip.setTimeStretchMode (preferredMode);
}

int BeatMakerNoRecord::applyHighQualitySettingsToEdit()
{
    if (edit == nullptr)
        return 0;

    int updatedClips = 0;

    for (auto* track : te::getAudioTracks (*edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clip : track->getClips())
        {
            if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
            {
                applyHighQualitySettingsToAudioClip (*audioClip);
                ++updatedClips;
            }
        }
    }

    return updatedClips;
}

void BeatMakerNoRecord::applyHighQualityAudioMode()
{
    configurePlaybackOnlyIO();
    const int clipsUpdated = applyHighQualitySettingsToEdit();

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    engine.getDeviceManager().deviceManager.getAudioDeviceSetup (setup);

    setStatus ("High-quality audio mode applied: "
               + juce::String ((int) std::round (setup.sampleRate)) + " Hz, "
               + juce::String (setup.bufferSize) + " sample buffer, "
               + juce::String (clipsUpdated) + " audio clips updated. Recording remains disabled.");
}

void BeatMakerNoRecord::applyNoRecordPolicyToEdit()
{
    if (edit == nullptr)
        return;

    for (auto input : edit->getAllInputDevices())
    {
        input->getInputDevice().setMonitorMode (te::InputDevice::MonitorMode::off);

        for (auto target : input->getTargets())
        {
            input->setRecordingEnabled (target, false);
            [[maybe_unused]] auto result = input->removeTarget (target, &edit->getUndoManager());
        }
    }
}

juce::File BeatMakerNoRecord::getProjectsRootDirectory()
{
    auto& storage = engine.getPropertyStorage();
    auto root = storage.getDefaultLoadSaveDirectory ("beatMakerProjects");

    auto ensureWritableDirectory = [] (const juce::File& candidate) -> juce::File
    {
        if (candidate == juce::File())
            return {};

        auto resolved = candidate;
        if (! resolved.isDirectory())
            [[maybe_unused]] const bool created = resolved.createDirectory();

        if (resolved.isDirectory() && resolved.hasWriteAccess())
            return resolved;

        return {};
    };

    root = ensureWritableDirectory (root);

    if (root == juce::File())
    {
        const std::array<juce::File, 3> fallbackRoots
        {{
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("TheSampledexWorkflow Projects"),
            juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("TheSampledexWorkflow")
                .getChildFile ("Projects"),
            juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("TheSampledexWorkflow Projects")
        }};

        for (const auto& candidate : fallbackRoots)
        {
            root = ensureWritableDirectory (candidate);
            if (root != juce::File())
                break;
        }
    }

    if (root == juce::File())
        root = juce::File::getSpecialLocation (juce::File::tempDirectory);

    storage.setDefaultLoadSaveDirectory ("beatMakerProjects", root);
    return root;
}

void BeatMakerNoRecord::ensureProjectDirectoryLayout (const juce::File& editFile) const
{
    const auto projectDirectory = editFile.getParentDirectory();
    if (! projectDirectory.isDirectory())
        [[maybe_unused]] const bool created = projectDirectory.createDirectory();

    [[maybe_unused]] const bool audioOk = projectDirectory.getChildFile ("Audio Files").createDirectory();
    [[maybe_unused]] const bool midiOk = projectDirectory.getChildFile ("MIDI Files").createDirectory();
    [[maybe_unused]] const bool exportOk = projectDirectory.getChildFile ("Exports").createDirectory();
    [[maybe_unused]] const bool backupOk = projectDirectory.getChildFile ("Backups").createDirectory();
}

bool BeatMakerNoRecord::saveEditToPath (const juce::File& requestedFile, juce::String& resultMessage, bool updateCurrentFile)
{
    constexpr int maxProjectBackupsToKeep = 40;

    if (edit == nullptr)
    {
        resultMessage = "No active edit session.";
        return false;
    }

    if (requestedFile == juce::File())
    {
        resultMessage = "Invalid save location.";
        return false;
    }

    auto targetFile = requestedFile;
    if (! targetFile.hasFileExtension (".tracktionedit"))
        targetFile = targetFile.withFileExtension (".tracktionedit");

    const auto projectDirectory = targetFile.getParentDirectory();
    if (! projectDirectory.isDirectory() && ! projectDirectory.createDirectory())
    {
        resultMessage = "Could not create project directory: " + projectDirectory.getFullPathName();
        return false;
    }

    if (projectDirectory.isDirectory() && ! projectDirectory.hasWriteAccess())
    {
        resultMessage = "No write access to project directory: " + projectDirectory.getFullPathName();
        return false;
    }

    if (targetFile.existsAsFile() && ! targetFile.hasWriteAccess())
    {
        resultMessage = "Project file is read-only: " + targetFile.getFullPathName();
        return false;
    }

    ensureProjectDirectoryLayout (targetFile);

    const auto backupDirectory = projectDirectory.getChildFile ("Backups");
    const bool hadExistingFile = targetFile.existsAsFile();
    bool backupWritten = false;
    juce::File backupFile;
    if (hadExistingFile)
    {
        const auto backupName = targetFile.getFileNameWithoutExtension() + "_" + createSafeTimestampTag();
        backupFile = backupDirectory.getNonexistentChildFile (backupName, ".tracktionedit", false);
        backupWritten = targetFile.copyFileTo (backupFile);
    }

    const bool ok = te::EditFileOperations (*edit).saveAs (targetFile, true);
    if (! ok)
    {
        if (backupWritten && restoreFileFromBackupCopy (backupFile, targetFile))
            resultMessage = "Save failed while writing project file. Rolled back from backup.";
        else
            resultMessage = "Save failed while writing project file."
                            + (hadExistingFile ? " Existing project file was not updated safely." : juce::String());

        if (backupFile.existsAsFile())
            resultMessage << " Backup: " << backupFile.getFileName();

        return false;
    }

    juce::String validationError;
    if (! validateEditXmlFile (targetFile, validationError))
    {
        if (backupWritten && restoreFileFromBackupCopy (backupFile, targetFile))
            resultMessage = "Saved file failed validation (" + validationError + "). Restored previous backup copy.";
        else
            resultMessage = "Saved file failed validation: " + validationError;

        if (backupFile.existsAsFile())
            resultMessage << " Backup: " << backupFile.getFileName();

        return false;
    }

    edit->editFileRetriever = [targetFile] { return targetFile; };
    if (updateCurrentFile || currentEditFile != targetFile)
    {
        currentEditFile = targetFile;
        editNameLabel.setText (targetFile.getFileName(), juce::dontSendNotification);
    }

    engine.getPropertyStorage().setDefaultLoadSaveDirectory ("beatMakerProjects", targetFile.getParentDirectory());
    engine.getPropertyStorage().getPropertiesFile().saveIfNeeded();
    pruneEditBackups (targetFile, maxProjectBackupsToKeep);

    resultMessage = backupWritten ? "Saved project (backup created)." : "Saved project.";
    if (backupWritten)
        resultMessage << " Backup: " << backupFile.getFileName();

    return true;
}

void BeatMakerNoRecord::createNewEdit (bool promptForTemplateChooser)
{
    juce::ignoreUnused (promptForTemplateChooser);
    ProjectStartTemplate startTemplate = ProjectStartTemplate::defaultSynth;

    auto projectsRoot = getProjectsRootDirectory();
    if (! projectsRoot.isDirectory() && ! projectsRoot.createDirectory())
    {
        setStatus ("Failed to create projects root: " + projectsRoot.getFullPathName());
        return;
    }

    const auto projectName = "Project_" + createSafeTimestampTag();
    auto projectDirectory = projectsRoot.getNonexistentChildFile (projectName, {}, true);
    if (! projectDirectory.createDirectory())
    {
        setStatus ("Failed to create project directory.");
        return;
    }

    auto editFile = projectDirectory.getChildFile (projectName + ".tracktionedit");
    auto newEdit = te::createEmptyEdit (engine, editFile);
    setCurrentEdit (std::move (newEdit), editFile, "Created new project: " + projectName);
    applyProjectStartTemplate (startTemplate);

    juce::String saveMessage;
    if (saveEditToPath (editFile, saveMessage, true))
        setStatus ("Created new project: " + projectName + " | Default startup: MIDI instrument-ready. Recording is disabled.");
    else
        setStatus ("Created project but initial save failed: " + saveMessage);
}

void BeatMakerNoRecord::openEdit()
{
    auto startDirectory = currentEditFile.existsAsFile() ? currentEditFile.getParentDirectory() : getProjectsRootDirectory();
    if (! startDirectory.isDirectory())
        startDirectory = getProjectsRootDirectory();

    juce::FileChooser chooser ("Open Project Edit", startDirectory, "*.tracktionedit");
    if (! chooser.browseForFileToOpen())
        return;

    auto file = chooser.getResult();
    if (! file.existsAsFile())
        return;

    juce::String validationError;
    if (! validateEditXmlFile (file, validationError))
    {
        setStatus ("Failed to open edit \"" + file.getFileName() + "\": " + validationError);
        return;
    }

    auto loaded = te::loadEditFromFile (engine, file);
    if (loaded == nullptr)
    {
        auto backups = getSortedBackupFilesForEdit (file);
        juce::String backupLoadError = "No usable backup found.";

        for (const auto& backup : backups)
        {
            juce::String backupValidationError;
            if (! validateEditXmlFile (backup, backupValidationError))
            {
                backupLoadError = "Backup validation failed (" + backup.getFileName() + "): " + backupValidationError;
                continue;
            }

            auto recoveredEdit = te::loadEditFromFile (engine, backup);
            if (recoveredEdit == nullptr)
            {
                backupLoadError = "Backup load failed (" + backup.getFileName() + ").";
                continue;
            }

            engine.getPropertyStorage().setDefaultLoadSaveDirectory ("beatMakerProjects", file.getParentDirectory());
            setCurrentEdit (std::move (recoveredEdit),
                            file,
                            "Recovered from backup: " + backup.getFileName() + " (source file was unreadable)");
            return;
        }

        setStatus ("Failed to open edit file: " + file.getFileName() + ". " + backupLoadError);
        return;
    }

    engine.getPropertyStorage().setDefaultLoadSaveDirectory ("beatMakerProjects", file.getParentDirectory());
    setCurrentEdit (std::move (loaded), file, "Opened edit: " + file.getFileName());
}

void BeatMakerNoRecord::setCurrentEdit (std::unique_ptr<te::Edit> newEdit, const juce::File& file, const juce::String& message)
{
    if (newEdit == nullptr)
        return;

    if (edit != nullptr)
        edit->getTransport().removeChangeListener (this);

    selectionManager.deselectAll();
    editComponent = nullptr;
    edit = std::move (newEdit);
    currentEditFile = file;
    outputMeterSmoothed = 0.0f;
    activeMidiClipID = {};
    pianoRollViewportClipID = {};
    clearStepSequencerViewportOverride();

    edit->editFileRetriever = [file] { return file; };
    if (file.getParentDirectory().isDirectory())
        engine.getPropertyStorage().setDefaultLoadSaveDirectory ("beatMakerProjects", file.getParentDirectory());

    edit->playInStopEnabled = true;
    edit->getTransport().addChangeListener (this);
    edit->getTransport().ensureContextAllocated();
    lastTransportPlaying = edit->getTransport().isPlaying();
    playbackSafetyWasPlaying = lastTransportPlaying;
    playbackSafetyLastPosition = edit->getTransport().getPosition();
    playbackSafetyActiveClipID = {};
    playbackSafetyLastActiveClipBeat = -1.0;

    if (te::getAudioTracks (*edit).isEmpty())
    {
        edit->ensureNumberOfAudioTracks (1);
        if (auto tracks = te::getAudioTracks (*edit); ! tracks.isEmpty())
            configureTrackRoleAsAudio (*tracks.getFirst(), 1);
    }

    applyNoRecordPolicyToEdit();
    const int updatedAudioClips = applyHighQualitySettingsToEdit();
    markPlaybackRoutingNeedsPreparation();
    prepareEditForPluginPlayback (true);

    buildEditComponent();
    editNameLabel.setText (currentEditFile.getFileName(), juce::dontSendNotification);

    updateTrackControlsFromSelection();
    updateButtonsFromState();
    updatePlayButtonText();
    updateTransportLoopButton();

    auto statusMessage = message + " Recording is disabled.";
    if (updatedAudioClips > 0)
        statusMessage << " High-quality processing active on " << updatedAudioClips << " audio clips.";

    setStatus (statusMessage);
    resized();
}

bool BeatMakerNoRecord::chooseNewProjectTemplate (ProjectStartTemplate& outTemplate, bool forcePrompt, bool allowPromptDialog)
{
    juce::ignoreUnused (forcePrompt, allowPromptDialog);
    outTemplate = ProjectStartTemplate::defaultSynth;
    return true;
}

void BeatMakerNoRecord::applyProjectStartTemplate (ProjectStartTemplate startTemplate)
{
    if (edit == nullptr)
        return;

    auto tracks = te::getAudioTracks (*edit);
    while (! tracks.isEmpty())
    {
        edit->deleteTrack (tracks.getLast());
        tracks = te::getAudioTracks (*edit);
    }

    edit->ensureNumberOfAudioTracks (1);
    tracks = te::getAudioTracks (*edit);
    auto* track = tracks.isEmpty() ? nullptr : tracks.getFirst();
    if (track == nullptr)
        return;

    const bool templateHasMidiTrack = startTemplate != ProjectStartTemplate::audioTrack;

    if (startTemplate == ProjectStartTemplate::defaultSynth)
    {
        configureTrackRoleAsMidi (*track, 1, true);
        setStatus ("Created MIDI Beatmaker project.");
    }
    else if (startTemplate == ProjectStartTemplate::audioTrack)
    {
        configureTrackRoleAsAudio (*track, 1);
        setStatus ("Created Audio Beatmaker project.");
    }
    else
    {
        configureTrackRoleAsMidi (*track, 1, true);
        edit->ensureNumberOfAudioTracks (2);
        tracks = te::getAudioTracks (*edit);

        if (tracks.size() > 1 && tracks[1] != nullptr)
            configureTrackRoleAsAudio (*tracks[1], 2);

        setStatus ("Created Audio + MIDI Beatmaker project.");
    }

    selectionManager.selectOnly (track);

    if (templateHasMidiTrack)
    {
        // Beat-ready default: immediately expose both editors and seed a starter pattern.
        setPianoEditorLayoutMode (PianoEditorLayoutMode::split, false, false);
        createMidiClip();
    }

    updateTrackControlsFromSelection();
    updateButtonsFromState();
}

void BeatMakerNoRecord::markPlaybackRoutingNeedsPreparation()
{
    playbackRoutingNeedsPreparation = true;

    juce::Component::SafePointer<BeatMakerNoRecord> safeThis (this);
    juce::MessageManager::callAsync ([safeThis]
    {
        auto* owner = safeThis.getComponent();
        if (owner == nullptr || owner->edit == nullptr)
            return;

        if (! owner->playbackRoutingNeedsPreparation)
            return;

        if (owner->edit->getTransport().isPlaying())
            return;

        owner->prepareEditForPluginPlayback (false);
    });
}

void BeatMakerNoRecord::buildEditComponent()
{
    if (edit == nullptr)
        return;

    editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
    auto& viewState = editComponent->getEditViewState();
    viewState.showHeaders = true;
    viewState.showFooters = false;
    viewState.showMasterTrack = false;
    viewState.showGlobalTrack = false;
    viewState.showMarkerTrack = false;
    viewState.showChordTrack = false;
    viewState.showArrangerTrack = false;
    viewState.showMidiDevices = false;
    viewState.showWaveDevices = false;
    viewState.showRecordControls = false;
    viewState.trackHeight = trackHeightSlider.getValue();

    if (viewState.viewX2.get() <= viewState.viewX1.get())
    {
        viewState.viewX1 = te::TimePosition::fromSeconds (0.0);
        viewState.viewX2 = te::TimePosition::fromSeconds (16.0);
    }

    workspaceSection.addAndMakeVisible (*editComponent);
}

void BeatMakerNoRecord::saveEdit()
{
    if (edit == nullptr)
        return;

    if (currentEditFile == juce::File())
    {
        saveEditAs();
        return;
    }

    juce::String saveMessage;
    if (saveEditToPath (currentEditFile, saveMessage, true))
        setStatus (saveMessage + " " + currentEditFile.getFileName());
    else
        setStatus ("Save failed: " + saveMessage);
}

void BeatMakerNoRecord::saveEditAs()
{
    if (edit == nullptr)
        return;

    auto startFile = currentEditFile;
    if (startFile == juce::File())
        startFile = getProjectsRootDirectory().getChildFile ("Project.tracktionedit");

    juce::FileChooser chooser ("Save Project As", startFile, "*.tracktionedit");
    if (! chooser.browseForFileToSave (true))
        return;

    auto file = chooser.getResult();
    juce::String saveMessage;
    if (saveEditToPath (file, saveMessage, true))
        setStatus ("Saved project as: " + currentEditFile.getFileName());
    else
        setStatus ("Save As failed: " + saveMessage);
}

void BeatMakerNoRecord::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (edit != nullptr && source == &edit->getTransport())
    {
        pendingUiUpdateFlags.fetch_or (pendingUiUpdateTransport, std::memory_order_relaxed);
        postPendingUiUpdatesAsync();
        return;
    }

    if (source == &selectionManager)
    {
        pendingUiUpdateFlags.fetch_or (pendingUiUpdateSelection, std::memory_order_relaxed);
        postPendingUiUpdatesAsync();
        return;
    }

    if (source == &leftDockPanelTabs)
    {
        const int tabIndex = leftDockPanelTabs.getCurrentTabIndex();
        const auto mode = getLeftDockPanelModeForComboId (tabIndex + 1);
        setLeftDockPanelMode (mode, true, false);
        return;
    }

    if (source == &midiToolsTabs)
    {
        resized();
        updateButtonsFromState();
        return;
    }

    if (source == &pianoEditorModeTabs)
    {
        setPianoEditorLayoutMode (getPianoEditorLayoutModeSelection(), true, false);
    }
}

void BeatMakerNoRecord::postPendingUiUpdatesAsync()
{
    bool expected = false;
    if (! pendingUiUpdatePosted.compare_exchange_strong (expected, true, std::memory_order_acq_rel))
        return;

    juce::Component::SafePointer<BeatMakerNoRecord> safeThis (this);
    juce::MessageManager::callAsync ([safeThis]
    {
        if (safeThis != nullptr)
            safeThis->drainPendingUiUpdatesAsync();
    });
}

void BeatMakerNoRecord::drainPendingUiUpdatesAsync()
{
    for (;;)
    {
        const int pending = pendingUiUpdateFlags.exchange (pendingUiUpdateNone, std::memory_order_acq_rel);
        if (pending == pendingUiUpdateNone)
            break;

        if ((pending & pendingUiUpdateTransport) != 0)
            processTransportChangeForUi();

        if ((pending & pendingUiUpdateSelection) != 0)
            processSelectionChangeForUi();
    }

    pendingUiUpdatePosted.store (false, std::memory_order_release);

    if (pendingUiUpdateFlags.load (std::memory_order_acquire) != pendingUiUpdateNone)
        postPendingUiUpdatesAsync();
}

void BeatMakerNoRecord::processTransportChangeForUi()
{
    if (edit == nullptr)
        return;

    runTransportPlaybackSafetyCheck();
    updatePlayButtonText();
    updateTransportLoopButton();

    const bool nowPlaying = edit->getTransport().isPlaying();
    const bool transportStateChanged = nowPlaying != lastTransportPlaying;
    lastTransportPlaying = nowPlaying;

    if (! nowPlaying && playbackRoutingNeedsPreparation)
        markPlaybackRoutingNeedsPreparation();

    if (getSelectedMidiClip() != nullptr
        && ! stepSequencerManualPageOverrideActive
        && stepSequencerDragMode == StepSequencerDragMode::none)
    {
        updateStepSequencerScrollbarFromPageContext();
    }

    // During playback, dedicated animation timers drive ruler/midi/step repaints.
    // Only force repaint when transport state flips or when playback stops.
    if (! nowPlaying || transportStateChanged)
    {
        timelineRuler.repaint();
        mixerArea.repaint();
        stepSequencer.repaint();
        midiPianoRoll.repaint();
        updateTransportInfoLabel();
    }
}

void BeatMakerNoRecord::runTransportPlaybackSafetyCheck()
{
    if (edit == nullptr)
    {
        playbackSafetyWasPlaying = false;
        playbackSafetyLastPosition = te::TimePosition::fromSeconds (0.0);
        playbackSafetyActiveClipID = {};
        playbackSafetyLastActiveClipBeat = -1.0;
        return;
    }

    auto& transport = edit->getTransport();
    const bool nowPlaying = transport.isPlaying();
    const auto currentPosition = transport.getPosition();

    auto resetClipTracking = [this]
    {
        playbackSafetyActiveClipID = {};
        playbackSafetyLastActiveClipBeat = -1.0;
    };

    if (nowPlaying && playbackSafetyWasPlaying && transport.looping)
        if (currentPosition.inSeconds() + 1.0e-3 < playbackSafetyLastPosition.inSeconds())
            te::midiPanic (*edit, false);

    if (! nowPlaying && playbackSafetyWasPlaying)
        te::midiPanic (*edit, false);

    if (nowPlaying)
    {
        if (auto* midiClip = getSelectedMidiClip())
        {
            const double clipLengthBeats = juce::jmax (1.0 / 64.0, getMidiClipLengthBeats (*midiClip));
            const double clipBeat = midiClip->getContentBeatAtTime (currentPosition).inBeats();
            const bool beatLooksValid = std::isfinite (clipBeat)
                                     && clipBeat >= -(1.0 / 32.0)
                                     && clipBeat <= clipLengthBeats + (1.0 / 32.0);

            if (playbackSafetyActiveClipID != midiClip->itemID || ! beatLooksValid)
            {
                playbackSafetyActiveClipID = midiClip->itemID;
                playbackSafetyLastActiveClipBeat = beatLooksValid ? clipBeat : -1.0;
            }
            else
            {
                if (playbackSafetyLastActiveClipBeat >= 0.0
                    && clipBeat + (1.0 / 32.0) < playbackSafetyLastActiveClipBeat)
                {
                    te::midiPanic (*edit, false);
                }

                playbackSafetyLastActiveClipBeat = clipBeat;
            }
        }
        else
        {
            resetClipTracking();
        }
    }
    else
    {
        resetClipTracking();
    }

    playbackSafetyWasPlaying = nowPlaying;
    playbackSafetyLastPosition = currentPosition;
}

void BeatMakerNoRecord::processSelectionChangeForUi()
{
    // Selection changes can happen mid-gesture (e.g. plugin/track click while dragging notes),
    // so cancel editor interactions before rebuilding UI state.
    clearPianoRollNavigationInteraction();
    resetPianoRollNoteDragState();
    resetStepSequencerDragState();

    if (getSelectedMidiClip() != nullptr)
    {
        syncPianoRollViewportToSelection (false);
    }
    else
    {
        clearStepSequencerViewportOverride();
        updatePianoRollScrollbarsFromViewport();
        updateStepSequencerScrollbarFromPageContext();
    }

    updateTrackControlsFromSelection();
    updateButtonsFromState();
    timelineRuler.repaint();
    mixerArea.repaint();
    stepSequencer.repaint();
    midiPianoRoll.repaint();
}
