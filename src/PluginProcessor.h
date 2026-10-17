#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"

class R744Processor : public juce::AudioProcessor
{
public:
    R744Processor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "R-744"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Bara från meddelandetråden.
    bool loadSample (const juce::File& f);
    void resetSystem();                       // RESET-knappen
    const r7::Sample* uiSample() const { return sample.get(); }   // säkert: bara meddelandetråden byter ut den
    const std::vector<float>& peaks() const { return wavePeaks; }
    juce::String sampleName() const { return samplePath.isEmpty() ? juce::String() : juce::File (samplePath).getFileName(); }

    juce::AudioProcessorValueTreeState apvts;
    r7::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pEvap = nullptr, *pPress = nullptr, *pTemp = nullptr, *pValve = nullptr,
                      * pHeat = nullptr, *pOut = nullptr, *pHold = nullptr;
    std::unique_ptr<r7::Sample> sample;
    juce::SpinLock sampleLock;
    std::vector<float> wavePeaks;
    juce::String samplePath;
    juce::HeapBlock<float> scratchL, scratchR;
    int scratchSize = 0, heldNote = -1;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (R744Processor)
};
