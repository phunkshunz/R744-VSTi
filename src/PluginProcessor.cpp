#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::String;

namespace
{
juce::AudioParameterFloatAttributes fmt (std::function<String (float)> f)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
}
String pct (float v) { return String (juce::roundToInt (v * 100.0f)) + " %"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout R744Processor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using juce::ParameterID;
    juce::NormalisableRange<float> r (0.0f, 1.0f);
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    v.push_back (std::make_unique<P> (ParameterID { "evap", 1 }, "Evaporator", r, 0.3f,
        fmt ([] (float x) { return "x" + String (1.0f + x * x * 15.0f, 1); })));
    v.push_back (std::make_unique<P> (ParameterID { "press", 1 }, "Compressor", r, 0.2f,
        fmt ([] (float x) { return String (juce::roundToInt (35.0f + 100.0f * x)) + " bar"; })));
    v.push_back (std::make_unique<P> (ParameterID { "temp", 1 }, "Gas Cooler", r, 0.15f,
        fmt ([] (float x) { return String (juce::roundToInt (12.0f + 33.0f * x)) + String::fromUTF8 (" °C"); })));
    v.push_back (std::make_unique<P> (ParameterID { "valve", 1 }, "Expansion Valve", r, 0.0f, fmt (pct)));
    v.push_back (std::make_unique<P> (ParameterID { "superheat", 1 }, "Superheat", r, 0.0f, fmt (pct)));
    v.push_back (std::make_unique<P> (ParameterID { "out", 1 }, "Output", r, 0.7f, fmt (pct)));
    v.push_back (std::make_unique<juce::AudioParameterBool> (ParameterID { "hold", 1 }, "Hold", false));
    return { v.begin(), v.end() };
}

R744Processor::R744Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "R744", createLayout())
{
    pEvap  = apvts.getRawParameterValue ("evap");
    pPress = apvts.getRawParameterValue ("press");
    pTemp  = apvts.getRawParameterValue ("temp");
    pValve = apvts.getRawParameterValue ("valve");
    pHeat  = apvts.getRawParameterValue ("superheat");
    pOut   = apvts.getRawParameterValue ("out");
    pHold  = apvts.getRawParameterValue ("hold");
}

bool R744Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void R744Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    scratchSize = juce::jmax (64, samplesPerBlock);
    scratchL.allocate ((size_t) scratchSize, true);
    scratchR.allocate ((size_t) scratchSize, true);
    engine.prepare (sampleRate);
}

void R744Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();

    // MIDI: senaste tonen styr tonhöjd och startar om molnet (monofon, rått)
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) { heldNote = msg.getNoteNumber(); engine.noteOn (heldNote); }
        else if (msg.isNoteOff() && msg.getNoteNumber() == heldNote) { heldNote = -1; engine.noteOff(); }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { heldNote = -1; engine.allOff(); }
    }

    r7::Params p;
    p.evap = pEvap->load(); p.press = pPress->load(); p.temp = pTemp->load(); p.valve = pValve->load();
    p.superheat = pHeat->load(); p.out = pOut->load(); p.hold = pHold->load() > 0.5f;

    try
    {
        // Ljudtråden väntar aldrig: är ljudet mitt i ett byte blir blocket tyst.
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        const r7::Sample* s = lock.isLocked() ? sample.get() : nullptr;

        for (int done = 0; done < n;)
        {
            const int len = juce::jmin (scratchSize, n - done);
            engine.process (scratchL.get(), scratchR.get(), len, s, p);
            if (buffer.getNumChannels() >= 2)
            {
                buffer.copyFrom (0, done, scratchL.get(), len);
                buffer.copyFrom (1, done, scratchR.get(), len);
                for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, done, len);
            }
            else if (buffer.getNumChannels() == 1)
            {
                buffer.copyFrom (0, done, scratchL.get(), len, 0.5f);
                buffer.addFrom (0, done, scratchR.get(), len, 0.5f);
            }
            done += len;
        }
    }
    catch (...)
    {
        buffer.clear();
        engine.markError();
    }

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    engine.reportLoad ((float) (secs / juce::jmax (1e-6, n / sr)));
}

bool R744Processor::loadSample (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr || rd->sampleRate <= 0) return false;

    const auto n = (int) juce::jmin<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 600.0));
    if (n < 64) return false;

    juce::AudioBuffer<float> tmp (2, n);
    tmp.clear();
    rd->read (&tmp, 0, n, 0, true, true);
    if (rd->numChannels == 1) tmp.copyFrom (1, 0, tmp, 0, 0, n);

    auto s = std::make_unique<r7::Sample>();
    s->sr = rd->sampleRate;
    s->l.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + n);
    s->r.assign (tmp.getReadPointer (1), tmp.getReadPointer (1) + n);
    const float peak = tmp.getMagnitude (0, n);
    if (peak > 1e-6f)
        for (int i = 0; i < n; ++i) { s->l[(size_t) i] *= 0.9f / peak; s->r[(size_t) i] *= 0.9f / peak; }

    std::vector<float> pk (600, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        auto& v = pk[(size_t) ((juce::int64) i * 600 / n)];
        v = juce::jmax (v, std::abs (s->l[(size_t) i]), std::abs (s->r[(size_t) i]));
    }

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        std::swap (sample, s);
    }
    // det gamla ljudet (nu i s) frigörs här, på meddelandetråden, aldrig i ljudtråden
    wavePeaks = std::move (pk);
    samplePath = f.getFullPathName();
    return true;
}

void R744Processor::resetSystem()
{
    if (auto* h = apvts.getParameter ("superheat")) h->setValueNotifyingHost (0.0f);
    engine.requestReset();
}

void R744Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("sample", samplePath, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void R744Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto st = juce::ValueTree::fromXml (*xml);
        if (! st.isValid()) return;
        const String path = st.getProperty ("sample").toString();
        apvts.replaceState (st);
        if (path.isNotEmpty() && juce::File (path).existsAsFile())
            loadSample (juce::File (path));
    }
}

juce::AudioProcessorEditor* R744Processor::createEditor() { return new R744Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new R744Processor(); }
