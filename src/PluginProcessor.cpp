#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
const StringArray kRoots { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const StringArray kBeatsModes { "AUTO", "1 TAKT", "2 TAKTER", "4 TAKTER", "8 TAKTER", "16 TAKTER" };
const int kBeatsOf[] = { 0, 4, 8, 16, 32, 64 };
const StringArray kPhrases { "1 TAKT", "2 TAKTER", "4 TAKTER", "8 TAKTER" };
const int kPhraseBars[] = { 1, 2, 4, 8 };
String u8 (const char* s) { return String::fromUTF8 (s); }
} // namespace

AudioProcessorValueTreeState::ParameterLayout R744Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)) + " %"; });
    auto f = [&] (const char* id, const String& name, float def) { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f), def, pct)); };

    f ("loop", "Loop", 0.55f);
    f ("ping", "Slag", 0.5f);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "beats", 1 }, u8 ("Looplängd"), kBeatsModes, 0));

    f ("res", "Resonans", 0.7f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ring", 1 }, "Ringning", NormalisableRange<float> (0.f, 1.f), 0.45f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { const float s = 0.15f * std::pow (40.0f, v); return s < 1 ? String (roundToInt (s * 1000)) + " ms" : String (s, 1) + " s"; })));
    f ("bright", "Ljus", 0.5f);
    f ("spread", "Spridning", 0.5f);
    {
        StringArray chords; for (int i = 0; i < r744::kNumChords; ++i) chords.add (u8 (r744::chordName (i)));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chord", 1 }, "Ackord", chords, 1));
    }
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Grundton", kRoots, 9));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Oktav", -1, 1, 0));

    f ("cloud", "Moln", 0.4f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "grain", 1 }, "Korn", NormalisableRange<float> (0.f, 1.f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (40 + 460 * v * v)) + " ms"; })));
    f ("density", u8 ("Täthet"), 0.5f);
    f ("wander", "Vandra", 0.3f);
    f ("tune", u8 ("Stämning"), 0.7f);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Frys", false));

    f ("breath", "Andning", 0.4f);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "phrase", 1 }, "Fras", kPhrases, 2));
    f ("wow", "Band", 0.2f);
    f ("room", "Rum", 0.35f);
    f ("size", "Storlek", 0.6f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Ut", NormalisableRange<float> (-24.f, 6.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return l;
}

R744Processor::R744Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "R744", createLayout())
{
}

bool R744Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto o = layouts.getMainOutputChannelSet();
    return o == AudioChannelSet::stereo() || o == AudioChannelSet::mono();
}

r744::Params R744Processor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    r744::Params p;
    p.loop = v ("loop"); p.ping = v ("ping");
    p.res = v ("res"); p.ring = v ("ring"); p.bright = v ("bright"); p.spread = v ("spread");
    p.chord = roundToInt (v ("chord")); p.root = roundToInt (v ("root")); p.octave = roundToInt (v ("octave"));
    p.cloud = v ("cloud"); p.grain = v ("grain"); p.density = v ("density"); p.wander = v ("wander"); p.tune = v ("tune");
    p.freeze = v ("freeze") > 0.5f;
    p.breath = v ("breath"); p.phraseBars = kPhraseBars[jlimit (0, 3, roundToInt (v ("phrase")))];
    p.wow = v ("wow"); p.room = v ("room"); p.size = v ("size"); p.outDb = v ("out");
    return p;
}

void R744Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    engine.prepare (sr);
    engine.setSample (audioSample.load());
    numHeld = 0;
}

void R744Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0) return;

    // tangenter: hållna toner stämmer om klangen, ett anslag knackar på resonatorerna
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (numHeld < (int) held.size()) held[(size_t) numHeld++] = m.getNoteNumber();
            engine.ping (m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == m.getNoteNumber()) { held[(size_t) i] = held[(size_t) --numHeld]; break; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) numHeld = 0;
    }
    engine.setHeld (held.data(), numHeld);
    heldCount.store (numHeld);

    bool playing = false; double ppq = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q; else playing = false;
        }
    running.store (playing);

    engine.setSample (audioSample.load (std::memory_order_acquire));
    engine.setParams (readParams());

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float>* none = nullptr; (void) none;
    if (R != nullptr) engine.process (L, R, n, playing, ppq, jlimit (20.0, 400.0, bpm));
    else
    {
        HeapBlock<float> tmp ((size_t) n);
        engine.process (L, tmp.get(), n, playing, ppq, jlimit (20.0, 400.0, bpm));
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + tmp[i]);
    }

    scanPos.store (engine.scanShown);
    int notes[8]; engine.currentNotes (notes);
    for (int k = 0; k < 8; ++k) resNotes[(size_t) k].store (notes[k]);
}

// ======================= LOOPEN =======================
void R744Processor::applyBeatsMode()
{
    if (! current) return;
    const int mode = jlimit (0, 5, roundToInt (apvts.getRawParameterValue ("beats")->load()));
    current->beats = mode == 0 ? r744::guessBeats (current->length() / current->rate) : kBeatsOf[mode];
}

String R744Processor::loadSample (const File& file)
{
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) return u8 ("Kunde inte läsa ljudfilen.");
    const int len = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 60.0));
    if (len < 256) return u8 ("Ljudfilen är för kort.");
    AudioBuffer<float> buf ((int) jmin (2u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);

    auto s = std::make_shared<r744::Sample>();
    s->rate = reader->sampleRate;
    s->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (buf.getNumChannels() > 1) s->R.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
    // normalisera mjukt till -3 dB så att alla loopar driver klangen lika mycket
    float pk = 0.0f; for (float v : s->L) pk = std::max (pk, std::fabs (v)); for (float v : s->R) pk = std::max (pk, std::fabs (v));
    if (pk > 1.0e-4f) { const float g = 0.7f / pk; for (auto& v : s->L) v *= g; for (auto& v : s->R) v *= g; }

    // vågform för panelen
    wavePeaks.assign (400, 0.0f);
    for (int i = 0; i < len; ++i) { auto& p = wavePeaks[(size_t) ((int64) i * 400 / len)]; p = std::max (p, std::fabs (s->L[(size_t) i])); }

    previous = current;          // den gamla får leva kvar en stund, audio-tråden kan fortfarande läsa den
    current = s;
    applyBeatsMode();
    audioSample.store (current.get(), std::memory_order_release);
    samplePath = file.getFullPathName();
    return {};
}

// ======================= MAPPEN =======================
String R744Processor::loadFolder (const File& dir)
{
    if (! dir.isDirectory()) return u8 ("Det är ingen mapp.");
    Array<File> found;
    for (const auto& e : RangedDirectoryIterator (dir, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", File::findFiles))
    {
        found.add (e.getFile());
        if (found.size() >= 20000) break;
    }
    if (found.isEmpty()) return u8 ("Hittade inga ljudfiler i mappen.");
    struct ByPath { static int compareElements (const File& a, const File& b) { return a.getFullPathName().compareNatural (b.getFullPathName()); } };
    ByPath cmp; found.sort (cmp);
    folderRoot = dir;
    folderFiles = found;
    folderIndex = -1;
    const auto r = randomLoop (false);
    return String (found.size()) + u8 (" loopar i ") + String (subfolders().size()) + u8 (" mappar. ") + r;
}

StringArray R744Processor::subfolders() const
{
    StringArray out;
    for (const auto& f : folderFiles)
    {
        const auto rel = f.getParentDirectory().getRelativePathFrom (folderRoot);
        out.addIfNotAlreadyThere (rel == "." ? String ("(rotmappen)") : rel);
    }
    return out;
}

String R744Processor::currentSub() const
{
    if (! isPositiveAndBelow (folderIndex, folderFiles.size())) return {};
    const auto rel = folderFiles[folderIndex].getParentDirectory().getRelativePathFrom (folderRoot);
    return rel == "." ? String ("(rotmappen)") : rel;
}

String R744Processor::randomLoop (bool sameFolder)
{
    if (folderFiles.isEmpty()) return u8 ("Ladda en mapp först.");
    auto& r = Random::getSystemRandom();
    Array<int> pool;
    const File dir = isPositiveAndBelow (folderIndex, folderFiles.size()) ? folderFiles[folderIndex].getParentDirectory() : File();
    for (int i = 0; i < folderFiles.size(); ++i)
        if (i != folderIndex && (! sameFolder || folderFiles[i].getParentDirectory() == dir)) pool.add (i);
    if (pool.isEmpty()) return u8 ("Inga fler loopar där.");
    // försök några gånger om en fil inte går att läsa
    for (int tries = 0; tries < 6 && ! pool.isEmpty(); ++tries)
    {
        const int k = r.nextInt (pool.size());
        const int idx = pool[k];
        if (loadSample (folderFiles[idx]).isEmpty()) { folderIndex = idx; return sampleName() + "  ·  " + currentSub(); }
        pool.remove (k);
    }
    return u8 ("Kunde inte läsa looparna.");
}

String R744Processor::stepLoop (int dir)
{
    if (folderFiles.isEmpty()) return u8 ("Ladda en mapp först.");
    for (int tries = 0; tries < 8; ++tries)
    {
        folderIndex = ((folderIndex + dir) % folderFiles.size() + folderFiles.size()) % folderFiles.size();
        if (loadSample (folderFiles[folderIndex]).isEmpty()) return sampleName() + "  ·  " + currentSub();
    }
    return u8 ("Kunde inte läsa looparna.");
}

String R744Processor::randomFromSub (int subIndex)
{
    const auto subs = subfolders();
    if (! isPositiveAndBelow (subIndex, subs.size())) return {};
    const auto name = subs[subIndex];
    const File dir = name == "(rotmappen)" ? folderRoot : folderRoot.getChildFile (name);
    for (int i = 0; i < folderFiles.size(); ++i)
        if (folderFiles[i].getParentDirectory() == dir) { folderIndex = i; break; }
    return randomLoop (true);
}

// ======================= IDÉER =======================
void R744Processor::setParam (const char* id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void R744Processor::pushUndo()
{
    undoStack.push_back (apvts.copyState());
    while (undoStack.size() > 30) undoStack.pop_front();
}

void R744Processor::undo()
{
    if (undoStack.empty()) return;
    apvts.replaceState (undoStack.back());
    undoStack.pop_back();
    applyBeatsMode();
}

String R744Processor::rollSection (int section)
{
    pushUndo();
    auto& r = Random::getSystemRandom();
    if (section == 0)
    {
        setParam ("chord", (float) r.nextInt (r744::kNumChords));
        static const int roots[] = { 9, 7, 5, 2, 0, 4, 10, 3 };
        setParam ("root", (float) roots[r.nextInt (8)]);
        setParam ("ring", 0.2f + r.nextFloat() * 0.7f);
        setParam ("bright", r.nextFloat());
        setParam ("spread", r.nextFloat());
        return u8 ("RESONANS: ") + u8 (r744::chordName (roundToInt (apvts.getRawParameterValue ("chord")->load())));
    }
    if (section == 1)
    {
        setParam ("grain", r.nextFloat());
        setParam ("density", 0.2f + r.nextFloat() * 0.7f);
        setParam ("wander", r.nextFloat() * 0.8f);
        setParam ("tune", 0.3f + r.nextFloat() * 0.7f);
        return u8 ("MOLN slumpat.");
    }
    setParam ("breath", r.nextFloat() * 0.8f);
    setParam ("phrase", (float) r.nextInt (4));
    setParam ("wow", r.nextFloat() * 0.6f);
    setParam ("room", 0.1f + r.nextFloat() * 0.6f);
    setParam ("size", r.nextFloat());
    return u8 ("ANDNING och RUM slumpat.");
}

// IDÉ!: ett av sex recept, så att det alltid blir något användbart
String R744Processor::idea()
{
    pushUndo();
    auto& r = Random::getSystemRandom();
    auto rr = [&] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    static const char* names[] = { "RINGANDE TRUMMOR", "FRUSET MOLN", "DUBKAMMARE", "GLASKLOCKOR", "DRÖNARE", "PULS I ACKORD" };
    const int k = r.nextInt (6);
    static const int roots[] = { 9, 7, 5, 2, 0, 4, 10, 3 };
    setParam ("root", (float) roots[r.nextInt (8)]);
    setParam ("chord", (float) r.nextInt (r744::kNumChords));
    setParam ("freeze", 0.0f);
    switch (k)
    {
        case 0: setParam ("loop", rr (0.4f, 0.8f)); setParam ("res", rr (0.6f, 0.9f)); setParam ("ring", rr (0.25f, 0.55f)); setParam ("cloud", rr (0.0f, 0.25f)); setParam ("room", rr (0.1f, 0.3f)); setParam ("breath", rr (0.0f, 0.3f)); break;
        case 1: setParam ("loop", rr (0.0f, 0.25f)); setParam ("res", rr (0.2f, 0.5f)); setParam ("cloud", rr (0.7f, 1.0f)); setParam ("grain", rr (0.6f, 1.0f)); setParam ("density", rr (0.5f, 0.9f)); setParam ("tune", rr (0.6f, 1.0f)); setParam ("room", rr (0.4f, 0.7f)); break;
        case 2: setParam ("loop", rr (0.3f, 0.6f)); setParam ("res", rr (0.4f, 0.7f)); setParam ("ring", rr (0.5f, 0.75f)); setParam ("cloud", rr (0.2f, 0.5f)); setParam ("breath", rr (0.4f, 0.8f)); setParam ("wow", rr (0.3f, 0.6f)); setParam ("room", rr (0.4f, 0.65f)); setParam ("size", rr (0.6f, 1.0f)); break;
        case 3: setParam ("loop", rr (0.2f, 0.5f)); setParam ("res", rr (0.7f, 1.0f)); setParam ("ring", rr (0.6f, 0.9f)); setParam ("bright", rr (0.7f, 1.0f)); setParam ("octave", 1.0f); setParam ("cloud", rr (0.1f, 0.4f)); setParam ("room", rr (0.3f, 0.6f)); break;
        case 4: setParam ("loop", rr (0.0f, 0.2f)); setParam ("res", rr (0.6f, 0.9f)); setParam ("ring", rr (0.8f, 1.0f)); setParam ("cloud", rr (0.5f, 0.8f)); setParam ("grain", rr (0.7f, 1.0f)); setParam ("octave", -1.0f); setParam ("breath", rr (0.3f, 0.7f)); setParam ("room", rr (0.5f, 0.8f)); break;
        default: setParam ("loop", rr (0.5f, 0.8f)); setParam ("res", rr (0.5f, 0.8f)); setParam ("ring", rr (0.1f, 0.35f)); setParam ("bright", rr (0.3f, 0.8f)); setParam ("cloud", rr (0.0f, 0.3f)); setParam ("breath", rr (0.3f, 0.7f)); setParam ("phrase", (float) r.nextInt (3)); break;
    }
    setParam ("spread", r.nextFloat());
    setParam ("wander", r.nextFloat() * 0.6f);
    return u8 ("IDÉ: ") + u8 (names[k]) + u8 (" i ") + kRoots[roundToInt (apvts.getRawParameterValue ("root")->load())] + " "
         + u8 (r744::chordName (roundToInt (apvts.getRawParameterValue ("chord")->load())));
}

String R744Processor::mutate()
{
    pushUndo();
    auto& r = Random::getSystemRandom();
    for (auto* id : { "res", "ring", "bright", "spread", "cloud", "grain", "density", "wander", "tune", "breath", "wow", "room", "size" })
        if (auto* p = apvts.getParameter (id))
        {
            const float v = jlimit (0.0f, 1.0f, p->getValue() + (r.nextFloat() - 0.5f) * 0.16f);
            p->beginChangeGesture(); p->setValueNotifyingHost (v); p->endChangeGesture();
        }
    return u8 ("MUTERAT: lite här och där.");
}

// ======================= PRESETS =======================
const std::vector<KlangPreset>& R744Processor::presets()
{
    static const std::vector<KlangPreset> p = {
        { "Ringande Golv", "RESONANS", { { "loop", .65f }, { "res", .8f }, { "ring", .35f }, { "chord", 1 }, { "root", 9 }, { "octave", 0 }, { "cloud", .1f }, { "room", .2f }, { "breath", .2f } } },
        { "Kvintpuls", "RESONANS", { { "loop", .7f }, { "res", .7f }, { "ring", .25f }, { "chord", 4 }, { "root", 7 }, { "octave", -1 }, { "cloud", 0 }, { "room", .15f }, { "breath", .45f }, { "phrase", 1 } } },
        { "Glasklockor", "RESONANS", { { "loop", .3f }, { "res", .9f }, { "ring", .75f }, { "bright", .9f }, { "chord", 2 }, { "root", 2 }, { "octave", 1 }, { "cloud", .2f }, { "room", .45f } } },
        { "Dorisk Hall", "RESONANS", { { "loop", .5f }, { "res", .75f }, { "ring", .55f }, { "chord", 7 }, { "root", 4 }, { "spread", .8f }, { "cloud", .25f }, { "room", .5f }, { "size", .8f } } },
        { "Fruset Moln", "MOLN", { { "loop", .1f }, { "res", .3f }, { "cloud", .9f }, { "grain", .85f }, { "density", .7f }, { "tune", .9f }, { "wander", .2f }, { "room", .55f }, { "breath", .3f } } },
        { "Kornregn", "MOLN", { { "loop", .3f }, { "res", .2f }, { "cloud", .8f }, { "grain", .25f }, { "density", .85f }, { "tune", .6f }, { "wander", .6f }, { "room", .35f } } },
        { "Isbit", "MOLN", { { "loop", 0 }, { "res", .5f }, { "ring", .8f }, { "cloud", .7f }, { "grain", 1 }, { "density", .45f }, { "freeze", 1 }, { "room", .6f }, { "size", .9f } } },
        { "Dubkammare", "DUB", { { "loop", .45f }, { "res", .55f }, { "ring", .6f }, { "chord", 1 }, { "root", 5 }, { "cloud", .35f }, { "breath", .6f }, { "wow", .45f }, { "room", .55f }, { "size", .85f } } },
        { "Basic Kanal", "DUB", { { "loop", .55f }, { "res", .6f }, { "ring", .5f }, { "chord", 0 }, { "root", 7 }, { "breath", .7f }, { "phrase", 3 }, { "wow", .3f }, { "room", .45f } } },
        { "Kylrumsdrönare", "DRÖNARE", { { "loop", 0 }, { "res", .85f }, { "ring", 1 }, { "chord", 8 }, { "root", 9 }, { "octave", -1 }, { "cloud", .6f }, { "grain", .9f }, { "room", .7f }, { "breath", .5f } } },
        { "Tritonus i Tunneln", "DRÖNARE", { { "loop", .15f }, { "res", .8f }, { "ring", .9f }, { "chord", 9 }, { "root", 1 }, { "cloud", .5f }, { "room", .65f }, { "wow", .5f } } },
        { "Sus i Natten", "DRÖNARE", { { "loop", .2f }, { "res", .7f }, { "ring", .85f }, { "chord", 3 }, { "root", 0 }, { "cloud", .55f }, { "tune", 1 }, { "room", .6f }, { "breath", .4f } } },
    };
    return p;
}

int R744Processor::getNumPrograms() { return (int) presets().size(); }
const String R744Processor::getProgramName (int i) { return isPositiveAndBelow (i, (int) presets().size()) ? u8 (presets()[(size_t) i].name) : String(); }

void R744Processor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    // först standardvärden, sedan presetens egna
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (p))
            if (r->paramID != "out" && r->paramID != "beats") { r->beginChangeGesture(); r->setValueNotifyingHost (r->getDefaultValue()); r->endChangeGesture(); }
    for (auto& [id, v] : presets()[(size_t) index].values) setParam (id, v);
}

// ======================= SPARA / LADDA =======================
void R744Processor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("sample", samplePath, nullptr);
    state.setProperty ("folder", folderRoot.getFullPathName(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void R744Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = ValueTree::fromXml (*xml);
    const String path = tree.getProperty ("sample").toString();
    const String folder = tree.getProperty ("folder").toString();
    currentProgram = (int) tree.getProperty ("program", 0);
    apvts.replaceState (tree);
    std::weak_ptr<int> token = alive;
    MessageManager::callAsync ([this, token, path, folder]
    {
        if (token.expired()) return;
        if (folder.isNotEmpty() && File (folder).isDirectory()) loadFolder (File (folder));
        if (path.isNotEmpty() && File (path).existsAsFile())
        {
            loadSample (File (path));
            for (int i = 0; i < folderFiles.size(); ++i) if (folderFiles[i].getFullPathName() == path) folderIndex = i;
        }
    });
}

AudioProcessorEditor* R744Processor::createEditor() { return new R744Editor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new R744Processor(); }
