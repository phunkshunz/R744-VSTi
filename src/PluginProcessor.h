#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Klang.h"
#include <deque>
#include <memory>

struct KlangPreset
{
    const char* name;
    const char* group;
    std::vector<std::pair<const char*, float>> values; // parameter-id -> verkligt värde
};

class R744Processor : public juce::AudioProcessor
{
public:
    R744Processor();
    ~R744Processor() override = default;

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
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); } // aldrig sövas av DAW:en

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-tråden ----
    juce::String loadSample (const juce::File& f);       // tom sträng = OK
    bool hasSample() const { return current != nullptr; }
    juce::String sampleName() const { return samplePath.isEmpty() ? juce::String() : juce::File (samplePath).getFileNameWithoutExtension(); }
    const std::vector<float>& peaks() const { return wavePeaks; }
    int loopBeats() const { return current ? current->beats : 0; }

    // ---- MAPP: ladda en hel mapp med loopar (med undermappar) och byt snabbt ----
    juce::String loadFolder (const juce::File& dir);
    juce::String randomLoop (bool sameFolder);   // slumpa ur hela mappen, eller ur samma undermapp
    juce::String stepLoop (int dir);             // nästa / förra i listan
    juce::String randomFromSub (int subIndex);   // slumpa ur en viss undermapp
    juce::StringArray subfolders() const;        // undermapparna (relativt rotmappen)
    int folderSize() const { return folderFiles.size(); }
    juce::String folderName() const { return folderRoot.getFileName(); }
    juce::String currentSub() const;

    juce::String idea();      // IDÉ!: ett nytt genomtänkt läge
    juce::String mutate();    // MUTERA: lite här och där
    juce::String rollSection (int section); // tärning: 0 RESONANS, 1 MOLN, 2 ANDNING/RUM
    void undo();
    bool canUndo() const { return ! undoStack.empty(); }

    static const std::vector<KlangPreset>& presets();

    juce::AudioProcessorValueTreeState apvts;
    r744::Engine engine;
    std::atomic<float> scanPos { 0.0f };
    std::atomic<bool> running { false };
    std::array<std::atomic<int>, 8> resNotes {};
    std::atomic<int> heldCount { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    r744::Params readParams() const;
    void setParam (const char* id, float plain);
    void pushUndo();
    void applyBeatsMode();

    std::shared_ptr<r744::Sample> current, previous;
    juce::File folderRoot;
    juce::Array<juce::File> folderFiles;
    int folderIndex = -1;
    std::shared_ptr<int> alive = std::make_shared<int> (0);
    std::atomic<const r744::Sample*> audioSample { nullptr };
    juce::String samplePath;
    std::vector<float> wavePeaks;
    std::deque<juce::ValueTree> undoStack;
    int currentProgram = 0;
    double sr = 48000.0;

    std::array<int, 16> held {};
    int numHeld = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (R744Processor)
};
