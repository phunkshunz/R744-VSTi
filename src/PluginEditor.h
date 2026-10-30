#pragma once
#include "PluginProcessor.h"
#include "Gui.h"
#include "About.h"

// Tärning (samma som i resten av MASKINHALLEN)
class Dice : public juce::Button
{
public:
    explicit Dice (const juce::String& tip) : juce::Button ("dice") { setTooltip (tip); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace ArmColours;
        const float s = (float) juce::jmin (getWidth(), getHeight()) - 2.0f;
        auto r = juce::Rectangle<float> (s, s).withCentre (getLocalBounds().toFloat().getCentre());
        if (down) r = r.reduced (1.0f);
        g.setColour (sodium.withAlpha (over ? 0.3f : 0.16f));
        g.fillRoundedRectangle (r, s * 0.22f);
        g.setColour (sodium);
        g.drawRoundedRectangle (r.reduced (0.6f), s * 0.22f, 1.2f);
        const float d = juce::jmax (2.0f, s * 0.17f);
        for (auto p : { juce::Point<float> (0.28f, 0.28f), { 0.72f, 0.28f }, { 0.5f, 0.5f }, { 0.28f, 0.72f }, { 0.72f, 0.72f } })
            g.fillEllipse (r.getX() + r.getWidth() * p.x - d * 0.5f, r.getY() + r.getHeight() * p.y - d * 0.5f, d, d);
    }
};

class WaveView : public juce::Component
{
public:
    explicit WaveView (R744Processor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    R744Processor& proc;
};

class R744Editor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit R744Editor (R744Processor&);
    ~R744Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void setStatus (const juce::String& s) { status = s; repaint (statusArea); }
    void chooseLoop();
    void chooseFolder();
    void subMenu();

    R744Processor& proc;
    ArmLnF lnf;
    juce::TooltipWindow tips { this, 600 };

    using BoxAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    WaveView wave { proc };
    juce::TextButton loadBtn { "LADDA LOOP" }, folderBtn { "LADDA MAPP" }, randomBtn, sameBtn { "SAMMA MAPP" },
                     prevBtn, nextBtn, subBtn { "MAPPAR" }, ideaBtn, mutateBtn { "MUTERA" }, undoBtn, freezeBtn { "FRYS" };
    juce::ComboBox beatsBox, chordBox, rootBox, phraseBox, presetBox;
    std::unique_ptr<BoxAtt> beatsAtt, chordAtt, rootAtt, phraseAtt;
    std::unique_ptr<BtnAtt> freezeAtt;
    Knob kLoop, kPing, kRes, kRing, kBright, kSpread, kOct, kCloud, kGrain, kDensity, kWander, kTune, kBreath, kWow, kRoom, kSize, kOut;
    Dice diceRes { juce::String::fromUTF8 ("Slumpa resonansen: ackord, grundton, ringning") },
         diceCloud { juce::String::fromUTF8 ("Slumpa molnet") },
         diceAir { juce::String::fromUTF8 ("Slumpa andning och rum") };
    std::unique_ptr<juce::FileChooser> chooser;
    AboutOverlay about { "R-744", juce::String::fromUTF8 ("Klangmaskin · loopar blir ton") };

    juce::String status;
    juce::Rectangle<int> header, loopArea, pLoop, pRes, pCloud, pAir, statusArea, logoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (R744Editor)
};
