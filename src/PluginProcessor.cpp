#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::String;

namespace
{
String pct (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; }

const std::vector<std::string>& floatIds()
{
    static const std::vector<std::string> ids = { "evap","grain","spread","gate","press","fold","crush","bass",
                                                   "temp","reso","hunt","valve","circ","swing","drift","superheat","out" };
    return ids;
}
const std::vector<std::string>& allIds()
{
    static std::vector<std::string> ids = [] { auto v = floatIds(); for (auto* s : { "rate","slices","ftype","huntRate","echo","steps","pitch" }) v.push_back (s); return v; }();
    return ids;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout R744Processor::createLayout()
{
    using juce::ParameterID;
    using A = juce::AudioParameterFloatAttributes;
    juce::NormalisableRange<float> r (0.0f, 1.0f);
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto f = [&] (const char* id, const char* name, float def, std::function<String (float, int)> fmt = pct)
    {
        v.push_back (std::make_unique<juce::AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, A().withStringFromValueFunction (fmt)));
    };
    auto c = [&] (const char* id, const char* name, juce::StringArray items, int def)
    {
        v.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // FÖRÅNGARE
    f ("evap", "Expansion", 0.0f, [] (float x, int) { return "x" + String (1.0f + x * x * 15.0f, 1); });
    f ("grain", "Grain", 0.4f, [] (float x, int) { return String (juce::roundToInt (12.0f + 300.0f * x * x)) + " ms"; });
    f ("spread", "Spread", 0.0f);
    f ("gate", "Length", 0.4f);
    // KOMPRESSOR
    f ("press", "Pressure", 0.25f, [] (float x, int) { return String (juce::roundToInt (35.0f + 100.0f * x)) + " bar"; });
    f ("fold", "Fold", 0.1f);
    f ("crush", "Crush", 0.0f);
    f ("bass", "Bass Lock", 0.4f, [] (float x, int) { return x < 0.01f ? String ("AV") : String (juce::roundToInt (40.0f + 210.0f * x)) + " Hz"; });
    // GASKYLARE
    f ("temp", "Ambient Temp", 0.2f, [] (float x, int) { return String (juce::roundToInt (12.0f + 33.0f * x)) + String::fromUTF8 (" °C"); });
    f ("reso", "Resonance", 0.35f);
    c ("ftype", "Filter Type", { "LP", "BP", "HP" }, 0);
    f ("hunt", "Hunting", 0.2f);
    c ("huntRate", "Hunting Rate", { "1/16", "1/8", "1/4", "1/2", "1 takt", "2 takter", "4 takter" }, 4);
    // VENTIL och CIRKULATION
    f ("valve", "Pressure Drop", 0.1f);
    f ("circ", "Circulation", 0.25f);
    c ("echo", "Circulation Time", { "1/16", "1/8", "3/16", "1/4", "3/8" }, 2);
    // GROOVE
    c ("rate", "Step Rate", { "1/32", "1/16", "1/16T", "1/8", "1/8T", "1/4" }, 1);
    v.push_back (std::make_unique<juce::AudioParameterInt> (ParameterID { "steps", 1 }, "Steps", 1, 16, 16));
    f ("swing", "Swing", 0.0f);
    f ("drift", "Drift", 0.0f);
    c ("slices", "Slices", { "4", "8", "16", "32" }, 2);
    v.push_back (std::make_unique<juce::AudioParameterInt> (ParameterID { "pitch", 1 }, "Pitch", -24, 24, 0));
    c ("evolve", "Evolve", { "AV", "2 takter", "4 takter", "8 takter", "16 takter" }, 0);
    // MAKRO
    f ("superheat", "Superheat", 0.0f);
    f ("out", "Output", 0.7f);
    v.push_back (std::make_unique<juce::AudioParameterBool> (ParameterID { "hold", 1 }, "Hold", true));
    return { v.begin(), v.end() };
}

R744Processor::R744Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "R744", createLayout())
{
    applyFactory (0, 0);
    undoStack.clear();
    startTimerHz (20);
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
    patFresh.store (true);
}

void R744Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber());
        else if (msg.isNoteOff()) engine.noteOff();
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allOff();
    }

    if (patFresh.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (patLock);
        if (lock.isLocked()) { engine.setPattern (audioPat); patFresh.store (false); }
    }

    r7::Params p;
    p.evap = raw ("evap"); p.grain = raw ("grain"); p.spread = raw ("spread"); p.gate = raw ("gate");
    p.press = raw ("press"); p.fold = raw ("fold"); p.crush = raw ("crush"); p.bass = raw ("bass");
    p.temp = raw ("temp"); p.reso = raw ("reso"); p.hunt = raw ("hunt");
    p.ftype = (int) raw ("ftype"); p.huntRate = (int) raw ("huntRate");
    p.valve = raw ("valve"); p.circ = raw ("circ"); p.echo = (int) raw ("echo");
    p.swing = raw ("swing"); p.drift = raw ("drift");
    p.steps = (int) raw ("steps"); p.rate = (int) raw ("rate"); p.slices = (int) raw ("slices"); p.pitch = (int) raw ("pitch");
    p.superheat = raw ("superheat"); p.out = raw ("out"); p.hold = raw ("hold") > 0.5f;

    r7::Clock clock;
    clock.freeRun = wrapperType == wrapperType_Standalone;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            clock.bpm = pos->getBpm().orFallback (128.0);
            clock.ppq = pos->getPpqPosition().orFallback (0.0);
            clock.playing = pos->getIsPlaying();
        }

    try
    {
        // Ljudtråden väntar aldrig: byts ljudfilen just nu blir blocket utan röster.
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        const r7::Sample* s = lock.isLocked() ? sample.get() : nullptr;
        r7::Clock c = clock;
        const double beatsPerSample = c.bpm / 60.0 / sr;

        for (int done = 0; done < n;)
        {
            const int len = juce::jmin (scratchSize, n - done);
            engine.process (scratchL.get(), scratchR.get(), len, s, p, c);
            c.ppq += len * beatsPerSample;
            if (buffer.getNumChannels() >= 2)
            {
                buffer.copyFrom (0, done, scratchL.get(), len);
                buffer.copyFrom (1, done, scratchR.get(), len);
                for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, done, len);
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

//==============================================================================
void R744Processor::pushPattern()
{
    {
        const juce::SpinLock::ScopedLockType lock (patLock);
        audioPat = pat;
    }
    patFresh.store (true);
    uiVersion.fetch_add (1);
}

void R744Processor::pushUndo()
{
    undoStack.emplace_back (apvts.copyState(), pat);
    while (undoStack.size() > 30) undoStack.pop_front();
}

void R744Processor::undo()
{
    if (undoStack.empty()) return;
    auto [st, p] = undoStack.back();
    undoStack.pop_back();
    const bool hold = raw ("hold") > 0.5f;
    apvts.replaceState (st);
    if (auto* h = apvts.getParameter ("hold")) h->setValueNotifyingHost (hold ? 1.0f : 0.0f);
    pat = p;
    pushPattern();
}

void R744Processor::writePatch (const r7::Patch& p)
{
    for (auto& [id, v] : p)
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

r7::Patch R744Processor::readPatch() const
{
    r7::Patch p;
    for (auto& id : allIds()) p[id] = apvts.getRawParameterValue (id)->load();
    return p;
}

void R744Processor::editStep (int i, const r7::Step& s, bool withUndo)
{
    if (i < 0 || i >= r7::kSteps) return;
    if (withUndo) pushUndo();
    pat.s[i] = s;
    pushPattern();
}

void R744Processor::applyFactory (int cat, int index)
{
    const auto& rc = r7::recipes();
    cat = juce::jlimit (0, (int) rc.size() - 1, cat);
    index = juce::jlimit (0, r7::kPerCat - 1, index);
    pushUndo();
    r7::Dice d ((uint32_t) (cat * 1000 + index * 17 + 5));
    r7::Patch p;
    r7::Pattern np;
    r7::applyRecipe (p, np, rc[(size_t) cat], d);
    writePatch (p);
    pat = np;
    currentCat = cat; currentIndex = index;
    currentName = String::fromUTF8 (r7::presetName (cat, index).c_str());
    pushPattern();
}

void R744Processor::act (Action a)
{
    pushUndo();
    r7::Patch p = readPatch();
    auto& d = dice;
    switch (a)
    {
        case Idea:
        {
            const int cat = d.pick ((int) r7::recipes().size());
            r7::applyRecipe (p, pat, r7::recipes()[(size_t) cat], d);
            currentCat = cat;
            currentName = String::fromUTF8 ("Idé · ") + String::fromUTF8 (r7::recipes()[(size_t) cat].name);
            break;
        }
        case All:       r7::wild (p, pat, d); currentName = String::fromUTF8 ("Slump"); break;
        case Mutate:
            for (auto& id : floatIds())
                if (id != "superheat" && id != "out") p[id] = juce::jlimit (0.0f, 1.0f, p[id] + d.range (-0.08f, 0.08f));
            r7::mutatePattern (pat, d, 2 + d.pick (3));
            break;
        case SecEvap:   r7::diceSection (p, { "evap", "grain", "spread", "gate" }, d); break;
        case SecComp:   r7::diceSection (p, { "press", "fold", "crush", "bass" }, d); break;
        case SecCool:   r7::diceSection (p, { "temp", "reso", "ftype", "hunt", "huntRate" }, d); break;
        case SecValve:  r7::diceSection (p, { "valve", "circ", "echo" }, d); break;
        case SecGroove: r7::diceSection (p, { "swing", "drift", "steps", "rate" }, d); break;
        case PatSteps:  r7::genSteps (pat, d, d.range (0.25f, 0.85f)); break;
        case PatSlices: r7::genSlices (pat, d, d.pick (5)); break;
        case PatAccents: r7::genAccents (pat, d); break;
        case PatRatchets: r7::genRatchets (pat, d, d.range (0.08f, 0.35f)); break;
        case Euclid:    r7::euclid (pat, d.irange (3, 11), 16, d.pick (4)); break;
        case ShiftL:    r7::rotate (pat, -1); break;
        case ShiftR:    r7::rotate (pat, 1); break;
        case Mirror:    r7::mirror (pat); break;
        case Clear:     for (auto& s : pat.s) s = r7::Step(); pat.s[0].on = 1; break;
    }
    writePatch (p);
    pushPattern();
}

// självspelet: muterar mönstret lite var N:e takt, i takt med musiken
void R744Processor::timerCallback()
{
    static const int bars[] = { 0, 2, 4, 8, 16 };
    const int every = bars[juce::jlimit (0, 4, (int) raw ("evolve"))];
    const int bar = engine.uiBar.load();
    if (every == 0 || ! engine.uiRunning.load()) { lastEvolveBar = bar; return; }
    if (bar != lastEvolveBar)
    {
        lastEvolveBar = bar;
        if (bar > 0 && bar % every == 0)
        {
            r7::mutatePattern (pat, dice, 2);
            pushPattern();
        }
    }
}

//==============================================================================
bool R744Processor::loadSample (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    if (rd == nullptr || rd->sampleRate <= 0) return false;

    const auto n = (int) juce::jmin<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 600.0));
    if (n < 256) return false;

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
        for (size_t i = 0; i < (size_t) n; ++i) { s->l[i] *= 0.9f / peak; s->r[i] *= 0.9f / peak; }

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
    // det gamla ljudet (nu i s) frigörs här på meddelandetråden, aldrig i ljudtråden
    wavePeaks = std::move (pk);
    samplePath = f.getFullPathName();
    uiVersion.fetch_add (1);
    return true;
}

void R744Processor::resetSystem()
{
    if (auto* h = apvts.getParameter ("superheat")) h->setValueNotifyingHost (0.0f);
    engine.requestReset();
}

String R744Processor::patternToString (const r7::Pattern& p)
{
    juce::StringArray a;
    for (auto& s : p.s)
        a.add (String (s.on) + "," + String (s.slice) + "," + String (s.accent) + "," + String (s.ratchet) + "," + String (s.rev));
    return a.joinIntoString (";");
}

r7::Pattern R744Processor::patternFromString (const String& str)
{
    r7::Pattern p;
    auto steps = juce::StringArray::fromTokens (str, ";", "");
    for (int i = 0; i < juce::jmin (r7::kSteps, steps.size()); ++i)
    {
        auto f = juce::StringArray::fromTokens (steps[i], ",", "");
        if (f.size() < 5) continue;
        p.s[i] = { (uint8_t) (f[0].getIntValue() != 0), (uint8_t) juce::jlimit (0, 31, f[1].getIntValue()),
                   (uint8_t) (f[2].getIntValue() != 0), (uint8_t) juce::jlimit (1, 8, f[3].getIntValue()), (uint8_t) (f[4].getIntValue() != 0) };
    }
    return p;
}

void R744Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("sample", samplePath, nullptr);
    st.setProperty ("pattern", patternToString (pat), nullptr);
    st.setProperty ("cat", currentCat, nullptr);
    st.setProperty ("index", currentIndex, nullptr);
    st.setProperty ("name", currentName, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void R744Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.isValid()) return;
    const String path = st.getProperty ("sample").toString();
    if (st.hasProperty ("pattern")) pat = patternFromString (st.getProperty ("pattern").toString());
    currentCat = st.getProperty ("cat", 0);
    currentIndex = st.getProperty ("index", 0);
    currentName = st.getProperty ("name", "Init").toString();
    apvts.replaceState (st);
    pushPattern();
    if (path.isNotEmpty() && juce::File (path).existsAsFile())
        loadSample (juce::File (path));
}

juce::AudioProcessorEditor* R744Processor::createEditor() { return new R744Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new R744Processor(); }
