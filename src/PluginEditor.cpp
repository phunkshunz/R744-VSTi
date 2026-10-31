#include "PluginEditor.h"
#include "Sprak.h"

using namespace ArmColours;

namespace
{
juce::String u8 (const char* s) { return sprak::trU (s); }
void fillBox (juce::ComboBox& b, juce::AudioProcessorValueTreeState& s, const char* id)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) b.addItemList (p->choices, 1);
    b.setJustificationType (juce::Justification::centred);
}
juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }
} // namespace

void WaveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ground);
    g.fillRoundedRectangle (r, 5.0f);
    const auto& pk = proc.peaks();
    if (pk.empty())
    {
        g.setColour (muted);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (u8 ("SLÄPP EN LOOP ELLER EN HEL MAPP HÄR"), getLocalBounds(), juce::Justification::centred);
        return;
    }
    const float mid = r.getCentreY(), h = r.getHeight() * 0.45f;
    const float w = r.getWidth() / (float) pk.size();
    g.setColour (concrete.withAlpha (0.55f));
    for (size_t i = 0; i < pk.size(); ++i)
    {
        const float a = pk[i] / 0.7f * h;
        g.fillRect (r.getX() + w * (float) i, mid - a, juce::jmax (1.0f, w - 0.5f), a * 2.0f);
    }
    // taktslag
    const int beats = proc.loopBeats();
    g.setColour (line2);
    for (int b = 1; b < beats; ++b)
        g.fillRect (r.getX() + r.getWidth() * (float) b / (float) beats, r.getY() + (b % 4 == 0 ? 0.0f : r.getHeight() * 0.85f), 1.0f, b % 4 == 0 ? r.getHeight() : r.getHeight() * 0.15f);
    // molnets position
    const float x = r.getX() + r.getWidth() * proc.scanPos.load();
    g.setColour (cold.withAlpha (0.25f));
    g.fillRect (x - 6.0f, r.getY(), 12.0f, r.getHeight());
    g.setColour (cold);
    g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
}

R744Panel::R744Panel (R744Processor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    addAndMakeVisible (wave);
    randomBtn.setButtonText (u8 ("SLUMPA LOOP"));
    randomBtn.setColour (juce::TextButton::buttonColourId, sodiumDk);
    randomBtn.setTooltip (u8 ("En ny slumpad loop ur hela mappen"));
    sameBtn.setTooltip (u8 ("En ny slumpad loop ur samma undermapp"));
    prevBtn.setButtonText (u8 ("◀")); nextBtn.setButtonText (u8 ("▶"));
    prevBtn.setTooltip (u8 ("Förra loopen i mappen")); nextBtn.setTooltip (u8 ("Nästa loop i mappen"));
    subBtn.setTooltip (u8 ("Välj undermapp: en slumpad loop därifrån"));
    loadBtn.onClick = [this] { chooseLoop(); };
    folderBtn.onClick = [this] { chooseFolder(); };
    randomBtn.onClick = [this] { setStatus (proc.randomLoop (false)); repaint(); };
    sameBtn.onClick = [this] { setStatus (proc.randomLoop (true)); repaint(); };
    prevBtn.onClick = [this] { setStatus (proc.stepLoop (-1)); repaint(); };
    nextBtn.onClick = [this] { setStatus (proc.stepLoop (1)); repaint(); };
    subBtn.onClick = [this] { subMenu(); };
    for (auto* b : { &loadBtn, &folderBtn, &randomBtn, &sameBtn, &prevBtn, &nextBtn, &subBtn }) addAndMakeVisible (*b);

    ideaBtn.setButtonText (u8 ("IDÉ!"));
    ideaBtn.setColour (juce::TextButton::buttonColourId, sodiumDk);
    ideaBtn.setTooltip (u8 ("Ett nytt genomtänkt läge: ackord, resonans, moln och rum"));
    mutateBtn.setTooltip (u8 ("Ändrar lite här och där, behåller känslan"));
    undoBtn.setButtonText (u8 ("ÅNGRA"));
    ideaBtn.onClick = [this] { setStatus (proc.idea()); };
    mutateBtn.onClick = [this] { setStatus (proc.mutate()); };
    undoBtn.onClick = [this] { proc.undo(); setStatus (u8 ("Ångrat.")); };
    for (auto* b : { &ideaBtn, &mutateBtn, &undoBtn }) addAndMakeVisible (*b);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("PRESET");
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) { proc.setCurrentProgram (id - 1); setStatus (presetBox.getText()); } };
    addAndMakeVisible (presetBox);

    fillBox (beatsBox, s, "beats");  beatsAtt = std::make_unique<BoxAtt> (s, "beats", beatsBox);
    fillBox (chordBox, s, "chord");  chordAtt = std::make_unique<BoxAtt> (s, "chord", chordBox);
    fillBox (rootBox, s, "root");    rootAtt = std::make_unique<BoxAtt> (s, "root", rootBox);
    fillBox (phraseBox, s, "phrase"); phraseAtt = std::make_unique<BoxAtt> (s, "phrase", phraseBox);
    beatsBox.setTooltip (u8 ("Hur lång loopen är. AUTO gissar."));
    chordBox.setTooltip (u8 ("Ackordet som resonatorerna står på. Håll tangenter för att spela egna."));
    rootBox.setTooltip (u8 ("Grundton"));
    for (auto* b : { &beatsBox, &chordBox, &rootBox, &phraseBox }) addAndMakeVisible (*b);

    freezeBtn.setClickingTogglesState (true);
    freezeBtn.setTooltip (u8 ("Molnet står still på ett ställe i loopen"));
    freezeAtt = std::make_unique<BtnAtt> (s, "freeze", freezeBtn);
    addAndMakeVisible (freezeBtn);

    struct K { Knob* k; const char* id; const char* name; juce::Colour c; };
    for (auto& k : { K { &kLoop, "loop", "LOOP", concrete }, K { &kPing, "ping", "SLAG", sodium },
                     K { &kRes, "res", "NIVÅ", sodium }, K { &kRing, "ring", "RINGNING", sodium }, K { &kBright, "bright", "LJUS", cold },
                     K { &kSpread, "spread", "SPRIDNING", cold }, K { &kOct, "octave", "OKTAV", concrete },
                     K { &kCloud, "cloud", "NIVÅ", cold }, K { &kGrain, "grain", "KORN", cold }, K { &kDensity, "density", "TÄTHET", cold },
                     K { &kWander, "wander", "VANDRA", cold }, K { &kTune, "tune", "STÄMNING", sodium },
                     K { &kBreath, "breath", "ANDNING", sodium }, K { &kWow, "wow", "BAND", concrete },
                     K { &kRoom, "room", "RUM", cold }, K { &kSize, "size", "STORLEK", cold }, K { &kOut, "out", "UT", concrete } })
    {
        k.k->attach (s, k.id, sprak::trU (k.name));
        k.k->setAccent (k.c);
        addAndMakeVisible (*k.k);
    }
    kPing.slider.setTooltip (u8 ("Hur mycket tangenterna knackar på resonatorerna, så att de låter även utan loop"));
    kSpread.slider.setTooltip (u8 ("Resonatorerna sprids över oktaver"));
    kTune.slider.setTooltip (u8 ("Andel korn som stäms i ackordet. Noll = loopens egen tonhöjd."));
    kWander.slider.setTooltip (u8 ("Hur långt molnet släpar och vandrar genom loopen"));

    diceRes.onClick = [this] { setStatus (proc.rollSection (0)); };
    diceCloud.onClick = [this] { setStatus (proc.rollSection (1)); };
    diceAir.onClick = [this] { setStatus (proc.rollSection (2)); };
    for (auto* d : { &diceRes, &diceCloud, &diceAir }) addAndMakeVisible (*d);

    addChildComponent (about);
    status = u8 ("Släpp en loop eller en hel mapp. Tryck play i Bitwig. SLUMPA LOOP byter snabbt.");
    setSize (1120, 700);
    startTimerHz (30);
}

R744Panel::~R744Panel() { stopTimer(); setLookAndFeel (nullptr); }

void R744Panel::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    const juce::File f (files[0]);
    setStatus (f.isDirectory() ? proc.loadFolder (f) : (proc.loadSample (f).isEmpty() ? f.getFileNameWithoutExtension() : u8 ("Kunde inte läsa filen.")));
    repaint();
}

void R744Panel::chooseLoop()
{
    chooser = std::make_unique<juce::FileChooser> (u8 ("Välj en loop"), juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        const auto err = proc.loadSample (f);
        setStatus (err.isEmpty() ? f.getFileNameWithoutExtension() : err);
        repaint();
    });
}

void R744Panel::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> (u8 ("Välj en mapp med loopar (undermappar följer med)"), juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        setStatus (proc.loadFolder (f));
        repaint();
    });
}

void R744Panel::subMenu()
{
    const auto subs = proc.subfolders();
    if (subs.isEmpty()) { setStatus (u8 ("Ladda en mapp först.")); return; }
    juce::PopupMenu m;
    m.addSectionHeader (proc.folderName() + "  ·  " + juce::String (proc.folderSize()) + u8 (" loopar"));
    const auto cur = proc.currentSub();
    for (int i = 0; i < subs.size() && i < 400; ++i) m.addItem (i + 1, subs[i], true, subs[i] == cur);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&subBtn), [this] (int r)
    {
        if (r > 0) { setStatus (proc.randomFromSub (r - 1)); repaint(); }
    });
}

void R744Panel::mouseDown (const juce::MouseEvent& e)
{
    if (logoArea.contains (e.getPosition())) { about.setBounds (getLocalBounds()); about.setVisible (true); about.toFront (false); }
}

void R744Panel::timerCallback()
{
    wave.repaint();
    undoBtn.setEnabled (proc.canUndo());
    const bool hasFolder = proc.folderSize() > 0;
    for (auto* b : { &randomBtn, &sameBtn, &prevBtn, &nextBtn, &subBtn }) b->setEnabled (hasFolder);
    repaint (pRes);
    repaint (loopArea.removeFromTop (30));
}

void R744Panel::paint (juce::Graphics& g)
{
    g.fillAll (ground);
    // huvud
    g.setColour (sodium);
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold).withHorizontalScale (0.85f));
    g.drawText (sprak::tr ("R-744"), logoArea, juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (u8 ("KLANGMASKIN · MASKINHALLEN · LOOPAR BLIR TON"), header.withTrimmedLeft (150).withWidth (360), juce::Justification::centredLeft);

    drawPanel (g, loopArea, "LOOP", {});
    {
        auto t = loopArea.reduced (12, 8).removeFromTop (16).withTrimmedLeft (60);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (text);
        juce::String info = proc.hasSample() ? proc.sampleName() + "   ·   " + juce::String (proc.loopBeats() / 4) + (proc.loopBeats() == 4 ? " takt" : " takter") : u8 ("ingen loop");
        if (proc.folderSize() > 0) info += u8 ("   ·   mapp ") + proc.folderName() + " / " + proc.currentSub() + "  (" + juce::String (proc.folderSize()) + ")";
        g.drawText (sprak::tr (info), t, juce::Justification::centredLeft, true);
        if (! proc.running.load())
        {
            g.setColour (sodium);
            g.drawText (u8 ("VÄNTAR PÅ PLAY"), t, juce::Justification::centredRight);
        }
    }

    drawPanel (g, pLoop, "LOOP & SLAG", {});
    drawPanel (g, pRes, "RESONANS", {});
    drawPanel (g, pCloud, "MOLN", {});
    drawPanel (g, pAir, "ANDNING & RUM", {});

    // tonerna resonatorerna står på
    {
        juce::StringArray names;
        for (int k = 0; k < 8; ++k) names.addIfNotAlreadyThere (noteName (proc.resNotes[(size_t) k].load()));
        auto t = pRes.reduced (14, 0).withTrimmedTop (pRes.getHeight() - 30).withHeight (20);
        g.setColour (proc.heldCount.load() > 0 ? sodium : cold);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ((proc.heldCount.load() > 0 ? u8 ("TANGENTER: ") : u8 ("TONER: ")) + names.joinIntoString ("  "), t, juce::Justification::centredLeft, true);
    }

    g.setColour (muted);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    auto lbl = [&] (juce::ComboBox& b, const char* t) { g.drawText (u8 (t), b.getBounds().translated (0, -14).withHeight (12), juce::Justification::centredLeft); };
    lbl (chordBox, "ACKORD"); lbl (rootBox, "GRUNDTON"); lbl (phraseBox, "FRAS"); lbl (beatsBox, "LOOPLÄNGD");

    g.setColour (status.isNotEmpty() ? cold : muted);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (sprak::tr (status), statusArea, juce::Justification::centredLeft, true);
}

void R744Panel::resized()
{
    auto r = getLocalBounds().reduced (14);
    header = r.removeFromTop (56);
    logoArea = header.withWidth (140);
    {
        auto h = header.withTrimmedLeft (520);
        presetBox.setBounds (h.removeFromRight (200).withSizeKeepingCentre (200, 30));
        h.removeFromRight (10);
        undoBtn.setBounds (h.removeFromRight (80).withSizeKeepingCentre (80, 32));
        h.removeFromRight (6);
        mutateBtn.setBounds (h.removeFromRight (90).withSizeKeepingCentre (90, 32));
        h.removeFromRight (6);
        ideaBtn.setBounds (h.removeFromRight (110).withSizeKeepingCentre (110, 40));
    }
    r.removeFromTop (6);
    statusArea = r.removeFromBottom (22);
    r.removeFromBottom (6);

    loopArea = r.removeFromTop (190);
    {
        auto a = loopArea.reduced (12, 10);
        a.removeFromTop (24);
        auto right = a.removeFromRight (300);
        a.removeFromRight (10);
        wave.setBounds (a);
        auto row = [&right] { auto x = right.removeFromTop (34); right.removeFromTop (5); return x; };
        auto r1 = row(); loadBtn.setBounds (r1.removeFromLeft (145)); r1.removeFromLeft (10); folderBtn.setBounds (r1);
        auto r2 = row(); randomBtn.setBounds (r2.removeFromLeft (190)); r2.removeFromLeft (10); subBtn.setBounds (r2);
        auto r3 = row(); sameBtn.setBounds (r3.removeFromLeft (120)); r3.removeFromLeft (8); prevBtn.setBounds (r3.removeFromLeft (40)); r3.removeFromLeft (4); nextBtn.setBounds (r3.removeFromLeft (40)); r3.removeFromLeft (8);
        beatsBox.setBounds (r3.withTrimmedTop (12));
    }
    r.removeFromTop (10);

    const int gap = 10;
    const int w1 = 170, w2 = 340, w3 = 300;
    pLoop = r.removeFromLeft (w1); r.removeFromLeft (gap);
    pRes = r.removeFromLeft (w2); r.removeFromLeft (gap);
    pCloud = r.removeFromLeft (w3); r.removeFromLeft (gap);
    pAir = r;

    auto knobRow = [] (juce::Rectangle<int> a, std::initializer_list<Knob*> ks, int h)
    {
        const int w = a.getWidth() / (int) ks.size();
        for (auto* k : ks) k->setBounds (a.removeFromLeft (w).withHeight (h).reduced (2, 0));
    };
    {
        auto a = pLoop.reduced (10, 10).withTrimmedTop (26);
        knobRow (a.removeFromTop (110), { &kLoop }, 110);
        a.removeFromTop (10);
        knobRow (a.removeFromTop (110), { &kPing }, 110);
    }
    {
        auto a = pRes.reduced (12, 10).withTrimmedTop (26);
        diceRes.setBounds (pRes.getRight() - 32, pRes.getY() + 6, 22, 22);
        auto boxes = a.removeFromTop (44).withTrimmedTop (14);
        chordBox.setBounds (boxes.removeFromLeft (150)); boxes.removeFromLeft (8);
        rootBox.setBounds (boxes.removeFromLeft (80));
        a.removeFromTop (10);
        knobRow (a.removeFromTop (100), { &kRes, &kRing, &kBright }, 100);
        a.removeFromTop (6);
        knobRow (a.removeFromTop (100), { &kSpread, &kOct }, 100);
    }
    {
        auto a = pCloud.reduced (12, 10).withTrimmedTop (26);
        diceCloud.setBounds (pCloud.getRight() - 32, pCloud.getY() + 6, 22, 22);
        knobRow (a.removeFromTop (100), { &kCloud, &kGrain, &kDensity }, 100);
        a.removeFromTop (14);
        knobRow (a.removeFromTop (100), { &kWander, &kTune }, 100);
        a.removeFromTop (12);
        freezeBtn.setBounds (a.removeFromTop (30).withSizeKeepingCentre (140, 30));
    }
    {
        auto a = pAir.reduced (12, 10).withTrimmedTop (26);
        diceAir.setBounds (pAir.getRight() - 32, pAir.getY() + 6, 22, 22);
        knobRow (a.removeFromTop (100), { &kBreath, &kWow, &kOut }, 100);
        a.removeFromTop (26);
        phraseBox.setBounds (a.removeFromTop (26).withWidth (110));
        a.removeFromTop (8);
        knobRow (a.removeFromTop (100), { &kRoom, &kSize }, 100);
    }
    about.setBounds (getLocalBounds());
}

//==============================================================================
R744Editor::R744Editor (R744Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    sprak::laddaOm();
    byggPanel();
    sprakKnapp.accent = sodium;
    sprakKnapp.onClick = [this]
    {
        sprak::sattEngelska (! sprak::engelska());
        // bygg om panelen efter klicket, aldrig mitt i det
        juce::Component::SafePointer<R744Editor> self (this);
        juce::MessageManager::callAsync ([self] { if (self != nullptr) self->byggPanel(); });
    };
    addAndMakeVisible (sprakKnapp);
    setSize (1120, 700);
}

R744Editor::~R744Editor() = default;

void R744Editor::byggPanel()
{
    panel.reset();
    panel = std::make_unique<R744Panel> (proc);
    sprak::oversattAllt (*panel);
    addAndMakeVisible (*panel);
    sprakKnapp.toFront (false);
    sprakKnapp.repaint();
    if (getWidth() > 0) resized();
}

void R744Editor::resized()
{
    if (panel != nullptr) panel->setBounds (getLocalBounds());
    sprakKnapp.setBounds (14 + 150, 14 + 4, 38, 14);
}
