#pragma once
#include "PluginProcessor.h"
#include "Gui.h"
#include "About.h"

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
    void paintWave (juce::Graphics&, juce::Rectangle<int>);
    void paintGauge (juce::Graphics&, juce::Rectangle<int>);

    R744Processor& proc;
    ArmLnF lnf;
    Knob evap, press, temp, valve, heat, out;
    juce::TextButton loadB { "LADDA" }, holdB { "HOLD" }, resetB { "RESET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    AboutOverlay about { "R-744", "Transcritical Sampler" };
    juce::String msg;
    bool blink = false;
    int frame = 0;

    juce::Rectangle<int> logoArea, waveArea, gaugeArea, panels[5];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (R744Editor)
};
