#pragma once
#include "PluginProcessor.h"
#include "Gui.h"
#include "About.h"

// Tärning, med eller utan text bredvid. Samma som i FUKT.
class DiceButton : public juce::Button
{
public:
    explicit DiceButton (const juce::String& tip = {}, const juce::String& txt = {}, bool hot = false)
        : juce::Button ("dice"), label (txt), loud (hot) { setTooltip (tip); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void roll() { face = juce::Random::getSystemRandom().nextInt (6) + 1; repaint(); }
    int face = 5;
    juce::String label;
    bool loud = false;
};

class SmallButton : public juce::Button
{
public:
    explicit SmallButton (const juce::String& t, const juce::String& tip = {}) : juce::Button (t), text (t) { setTooltip (tip); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String text;
};

// Stegrutnätet: STEG (på/av + vilken slice), ACCENT, RATCHET, BAKLÄNGES
class SeqGrid : public juce::Component
{
public:
    explicit SeqGrid (R744Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    static constexpr int kLabelW = 0;

private:
    int cellAt (juce::Point<int>, int& row) const;
    juce::Rectangle<float> cell (int row, int col) const;
    R744Processor& proc;
    int dragCol = -1, dragStartY = 0, dragStartSlice = 0;
    bool dragged = false;
};

class R744Editor : public juce::AudioProcessorEditor,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
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
    void openFile();
    void refreshPresetBoxes();
    void paintWave (juce::Graphics&, juce::Rectangle<int>);
    void paintPh (juce::Graphics&, juce::Rectangle<int>);
    void paintGauge (juce::Graphics&, juce::Rectangle<int>);
    void paintFreeze (juce::Graphics&, juce::Rectangle<int>);
    void hook (DiceButton&, R744Processor::Action);
    void hook (SmallButton&, R744Processor::Action);
    juce::ComboBox& combo (const char* id);

    R744Processor& proc;
    ArmLnF lnf;
    juce::TooltipWindow tips { this, 600 };

    // rattar
    Knob kEvap, kGrain, kSpread, kGate, kPress, kFold, kCrush, kBass, kTemp, kReso, kHunt,
         kValve, kCirc, kSwing, kDrift, kSteps, kPitch, kHeat, kOut;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtts;
    juce::ComboBox catBox, presetBox;
    SmallButton prevB { "<", utf8 ("Förra preset") }, nextB { ">", utf8 ("Nästa preset") };

    // idéer
    DiceButton ideaB { utf8 ("Ett helt nytt, genomtänkt läge från en slumpad kategori"), utf8 ("IDÉ!"), true };
    DiceButton allB { utf8 ("Vild slump över allt"), "ALLT" };
    DiceButton mutB { utf8 ("Ändra lite här och där"), "MUTERA" };
    SmallButton undoB { utf8 ("ÅNGRA"), utf8 ("Backa senaste ändringen (30 steg)") };
    DiceButton dEvap { utf8 ("Slumpa förångaren") }, dComp { utf8 ("Slumpa kompressorn") }, dCool { utf8 ("Slumpa gaskylaren") },
               dValve { utf8 ("Slumpa ventil och cirkulation") }, dGroove { utf8 ("Slumpa groove: swing, drift, steg, takt") },
               dSteps { utf8 ("Nya steg") }, dSlices { utf8 ("Nya slices") }, dAcc { utf8 ("Nya accenter") }, dRat { utf8 ("Nya ratchets och baklänges") };
    SmallButton euclidB { "EUKLID", utf8 ("Euklidisk rytm") }, shiftLB { "<<", utf8 ("Skifta mönstret åt vänster") },
                shiftRB { ">>", utf8 ("Skifta mönstret åt höger") }, mirrorB { "SPEGLA", utf8 ("Spela mönstret baklänges") },
                clearB { "RENSA", utf8 ("Töm mönstret") };

    juce::TextButton loadB { "LADDA" }, holdB { "HOLD" }, resetB { "RESET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    SeqGrid grid { proc };
    AboutOverlay about { "R-744", "Transcritical Sampler" };

    juce::String msg;
    int frame = 0, seenVersion = -1, seenFlash = 0, flashFrames = 0;
    bool blink = false;
    juce::Rectangle<int> logoArea, waveArea, phArea, seqArea, grooveArea, gaugeArea, panels[5], rowLabels[4];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (R744Editor)
};
