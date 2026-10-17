#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Utseende: samma färger som BETONG — fuktig betong, natriumorange och kall cyan.
namespace ArmColours
{
const juce::Colour ground   { 0xff121314 };
const juce::Colour panel    { 0xff1a1c1e };
const juce::Colour raised   { 0xff232527 };
const juce::Colour line     { 0xff2c2f32 };
const juce::Colour line2    { 0xff34383b };
const juce::Colour text     { 0xffd9d6cf };
const juce::Colour muted    { 0xff8b8e91 };
const juce::Colour sodium   { 0xffff8a1f };
const juce::Colour sodiumDk { 0xffc96a16 };
const juce::Colour cold     { 0xff4fd1e0 };
const juce::Colour coldDk   { 0xff2ea7b5 };
const juce::Colour concrete { 0xffb9b6ae };

inline juce::Colour lane (int i)
{
    static const juce::Colour c[] = { sodium, cold, coldDk, concrete };
    return c[(size_t) juce::jlimit (0, 3, i)];
}
} // namespace ArmColours

inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

class ArmLnF : public juce::LookAndFeel_V4
{
public:
    ArmLnF()
    {
        using namespace ArmColours;
        setColour (juce::ResizableWindow::backgroundColourId, ground);
        setColour (juce::TextButton::buttonColourId, raised);
        setColour (juce::TextButton::buttonOnColourId, sodium);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, ground);
        setColour (juce::ComboBox::backgroundColourId, raised);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, line2);
        setColour (juce::ComboBox::arrowColourId, sodium);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, sodium);
        setColour (juce::PopupMenu::highlightedTextColourId, ground);
        setColour (juce::PopupMenu::headerTextColourId, muted);
        setColour (juce::Slider::rotarySliderFillColourId, sodium);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, raised);
        setColour (juce::Slider::textBoxOutlineColourId, line2);
        setColour (juce::Label::textColourId, text);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::ScrollBar::thumbColourId, line2);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, sodium);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace ArmColours;
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto r = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
        const float thick = juce::jmax (2.5f, size * 0.09f);
        const auto arcR = r.reduced (thick * 0.5f);
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0, startAngle, endAngle, true);
        g.setColour (line);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // bipolära rattar (min < 0) fylls från mitten
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path val;
        val.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() * 0.5f, arcR.getHeight() * 0.5f, 0,
                           juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isEnabled() ? accent : muted);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        auto body = r.reduced (thick * 1.7f);
        g.setColour (raised);
        g.fillEllipse (body);
        g.setColour (line2);
        g.drawEllipse (body, 1.0f);

        juce::Path ptr;
        const float pl = body.getWidth() * 0.32f;
        ptr.addRoundedRectangle (-1.5f, -body.getHeight() * 0.5f + 3.0f, 3.0f, pl, 1.5f);
        g.setColour (juce::Colour (0xfff2efe8));
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (body.getCentreX(), body.getCentreY()));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        using namespace ArmColours;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
        if (down) c = c.brighter (0.15f);
        else if (over) c = c.brighter (0.06f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (b.getToggleState() ? c : (over ? sodium.withAlpha (0.6f) : line2));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::FontOptions (juce::jmin (13.0f, h * 0.5f), juce::Font::bold); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace ArmColours;
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (raised); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (true) ? sodium : line2); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (sodium); g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};

// Ratt med namn under och värde som visas när man skruvar.
class Knob : public juce::Component
{
public:
    Knob()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.onValueChange = [this] { repaint(); };
        slider.setDoubleClickReturnValue (false, 0.0);
        addAndMakeVisible (slider);
    }

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label)
    {
        attachment.reset();
        name = label;
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
        if (auto* p = state.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        repaint();
    }

    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); }

    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        auto r = getLocalBounds();
        auto bottom = r.removeFromBottom (26);
        g.setColour (muted);
        g.setFont (juce::FontOptions (9.5f));
        g.drawText (name, bottom.removeFromTop (12), juce::Justification::centred, true);
        g.setColour (text);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (slider.getTextFromValue (slider.getValue()), bottom, juce::Justification::centred, true);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (26)); }

    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Rubrik + mörk ruta, återanvänds för alla paneler.
inline void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& sub = {})
{
    using namespace ArmColours;
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        auto t = r.reduced (12, 8).removeFromTop (16);
        g.setColour (text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, t, juce::Justification::centredLeft, true);
        if (sub.isNotEmpty())
        {
            g.setColour (muted);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (sub, t, juce::Justification::centredRight, true);
        }
    }
}
