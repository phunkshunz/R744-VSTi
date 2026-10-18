#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "Gen.h"
#include <deque>

class R744Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    R744Processor();
    ~R744Processor() override { stopTimer(); }

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
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- allt nedan: bara från meddelandetråden
    bool loadSample (const juce::File& f);
    void resetSystem();
    const std::vector<float>& peaks() const { return wavePeaks; }
    juce::String sampleName() const { return samplePath.isEmpty() ? juce::String() : juce::File (samplePath).getFileName(); }
    bool hasSample() const { return sample != nullptr; }

    enum Action { Idea, All, Mutate, SecEvap, SecComp, SecCool, SecValve, SecGroove,
                  PatSteps, PatSlices, PatAccents, PatRatchets, Euclid, ShiftL, ShiftR, Mirror, Clear };
    void act (Action a);
    void undo();
    bool canUndo() const { return ! undoStack.empty(); }

    const r7::Pattern& pattern() const { return pat; }
    void editStep (int i, const r7::Step& s, bool withUndo);
    void applyFactory (int cat, int index);
    int currentCat = 0, currentIndex = 0;
    juce::String currentName { "Init" };

    juce::AudioProcessorValueTreeState apvts;
    r7::Engine engine;
    std::atomic<int> uiVersion { 0 };          // ökas när mönster eller preset byts, fönstret ritar om

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void pushUndo();
    void pushPattern();
    void writePatch (const r7::Patch& p);
    r7::Patch readPatch() const;
    static juce::String patternToString (const r7::Pattern&);
    static r7::Pattern patternFromString (const juce::String&);
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    r7::Pattern pat, audioPat;
    juce::SpinLock patLock;
    std::atomic<bool> patFresh { true };

    std::unique_ptr<r7::Sample> sample;
    juce::SpinLock sampleLock;
    std::vector<float> wavePeaks;
    juce::String samplePath;

    std::deque<std::pair<juce::ValueTree, r7::Pattern>> undoStack;
    r7::Dice dice { (uint32_t) juce::Time::getMillisecondCounter() };
    int lastEvolveBar = -1;

    juce::HeapBlock<float> scratchL, scratchR;
    int scratchSize = 0, heldNote = -1;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (R744Processor)
};
