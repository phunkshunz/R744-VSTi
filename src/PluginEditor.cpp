#include "PluginEditor.h"

using namespace ArmColours;
using juce::String;
using Action = R744Processor::Action;

namespace
{
const juce::Colour red { 0xffe5432f };
constexpr int W = 1100, H = 736;

// kall cyan -> natriumorange -> rött, som ett kylsystem som går varmt
juce::Colour heatColour (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    return v < 0.5f ? cold.interpolatedWith (sodium, v * 2.0f) : sodium.interpolatedWith (red, (v - 0.5f) * 2.0f);
}
}

//==============================================================================
void DiceButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (label.isNotEmpty())
    {
        auto bg = loud ? sodium : raised;
        g.setColour (down ? bg.brighter (0.15f) : (over ? bg.brighter (0.08f) : bg));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (loud ? sodium : (over ? sodium : line2));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (loud ? ground : ArmColours::text);
        g.setFont (juce::FontOptions (loud ? 16.0f : 12.5f, juce::Font::bold));
        g.drawText (label, r.withTrimmedLeft (r.getHeight()), juce::Justification::centred);
        r = r.removeFromLeft (r.getHeight()).reduced (5.0f);
    }
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    auto d = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
    const auto body = loud ? ground : (down ? sodium.brighter (0.2f) : (over ? sodium : sodiumDk));
    const auto pipC = loud ? sodium : ground;
    g.setColour (body);
    g.fillRoundedRectangle (d, s * 0.22f);
    g.setColour (pipC);
    const float pr = s * 0.09f;
    auto pip = [&] (float fx, float fy) { g.fillEllipse (d.getX() + d.getWidth() * fx - pr, d.getY() + d.getHeight() * fy - pr, pr * 2, pr * 2); };
    if (face % 2 == 1) pip (0.5f, 0.5f);
    if (face >= 2) { pip (0.27f, 0.27f); pip (0.73f, 0.73f); }
    if (face >= 4) { pip (0.73f, 0.27f); pip (0.27f, 0.73f); }
    if (face == 6) { pip (0.27f, 0.5f); pip (0.73f, 0.5f); }
}

void SmallButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? raised.brighter (0.15f) : (over ? raised.brighter (0.06f) : raised));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (over ? sodium : line2);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (ArmColours::text);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (text, r, juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<float> SeqGrid::cell (int row, int col) const
{
    const float cw = (float) getWidth() / r7::kSteps, rh = (float) getHeight() / 4.0f;
    return { col * cw + 2.0f + ((col % 4) == 0 ? 2.0f : 0.0f), row * rh + 2.0f, cw - 4.0f - ((col % 4) == 0 ? 2.0f : 0.0f), rh - 4.0f };
}

int SeqGrid::cellAt (juce::Point<int> p, int& row) const
{
    row = juce::jlimit (0, 3, (int) (p.y * 4 / juce::jmax (1, getHeight())));
    return juce::jlimit (0, r7::kSteps - 1, (int) (p.x * r7::kSteps / juce::jmax (1, getWidth())));
}

void SeqGrid::paint (juce::Graphics& g)
{
    const auto& pat = proc.pattern();
    const int steps = (int) proc.apvts.getRawParameterValue ("steps")->load();
    const int cur = proc.engine.uiRunning.load() ? proc.engine.uiStep.load() : -1;
    const int nSlices = r7::kSliceCounts[juce::jlimit (0, 3, (int) proc.apvts.getRawParameterValue ("slices")->load())];

    for (int c = 0; c < r7::kSteps; ++c)
    {
        const auto& s = pat.s[c];
        const float a = c < steps ? 1.0f : 0.3f;

        auto r0 = cell (0, c);
        g.setColour ((s.on ? (s.accent ? sodium : sodiumDk) : raised).withMultipliedAlpha (a));
        g.fillRoundedRectangle (r0, 3.0f);
        g.setColour (s.on ? ground : muted.withMultipliedAlpha (a));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (String (s.slice % nSlices + 1), r0, juce::Justification::centred);

        auto r1 = cell (1, c);
        g.setColour (raised.withMultipliedAlpha (a));
        g.fillRoundedRectangle (r1, 3.0f);
        if (s.accent) { g.setColour (sodium.withMultipliedAlpha (a)); g.fillRoundedRectangle (r1.reduced (r1.getWidth() * 0.28f, r1.getHeight() * 0.25f), 2.0f); }

        auto r2 = cell (2, c);
        g.setColour ((s.ratchet > 1 ? cold.withAlpha (0.35f) : raised).withMultipliedAlpha (a));
        g.fillRoundedRectangle (r2, 3.0f);
        g.setColour ((s.ratchet > 1 ? cold : muted).withMultipliedAlpha (a));
        g.drawText (s.ratchet > 1 ? "x" + String (s.ratchet) : String ("-"), r2, juce::Justification::centred);

        auto r3 = cell (3, c);
        g.setColour (raised.withMultipliedAlpha (a));
        g.fillRoundedRectangle (r3, 3.0f);
        if (s.rev)
        {
            juce::Path t;
            auto rr = r3.reduced (r3.getWidth() * 0.3f, r3.getHeight() * 0.25f);
            t.addTriangle (rr.getRight(), rr.getY(), rr.getRight(), rr.getBottom(), rr.getX(), rr.getCentreY());
            g.setColour (cold.withMultipliedAlpha (a));
            g.fillPath (t);
        }

        if (c == cur)
        {
            g.setColour (ArmColours::text);
            g.drawRoundedRectangle (cell (0, c).getUnion (cell (3, c)).expanded (1.5f), 4.0f, 1.5f);
        }
    }
}

void SeqGrid::mouseDown (const juce::MouseEvent& e)
{
    int row; const int c = cellAt (e.getPosition(), row);
    dragged = false;
    dragCol = row == 0 ? c : -1;
    dragStartY = e.y;
    dragStartSlice = proc.pattern().s[c].slice;
    if (row == 0) return;   // steg-raden: avgörs i mouseUp (klick) eller mouseDrag (slice)
    auto s = proc.pattern().s[c];
    if (row == 1) s.accent = ! s.accent;
    else if (row == 2) s.ratchet = (uint8_t) (s.ratchet >= 4 ? 1 : s.ratchet + 1);
    else s.rev = ! s.rev;
    proc.editStep (c, s, true);
    repaint();
}

void SeqGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCol < 0) return;
    const int delta = (dragStartY - e.y) / 6;
    if (delta == 0 && ! dragged) return;
    auto s = proc.pattern().s[dragCol];
    const int v = juce::jlimit (0, 31, dragStartSlice + delta);
    if (v == s.slice) return;
    s.slice = (uint8_t) v;
    s.on = 1;
    proc.editStep (dragCol, s, ! dragged);
    dragged = true;
    repaint();
}

void SeqGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragCol >= 0 && ! dragged)
    {
        auto s = proc.pattern().s[dragCol];
        s.on = ! s.on;
        proc.editStep (dragCol, s, true);
        repaint();
    }
    dragCol = -1;
}

void SeqGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int row; const int c = cellAt (e.getPosition(), row);
    if (row != 0) return;
    auto s = proc.pattern().s[c];
    s.slice = (uint8_t) juce::jlimit (0, 31, s.slice + (w.deltaY > 0 ? 1 : -1));
    proc.editStep (c, s, false);
    repaint();
}

//==============================================================================
juce::ComboBox& R744Editor::combo (const char* id)
{
    auto* b = combos.add (new juce::ComboBox (id));
    b->setName (id);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        b->addItemList (p->choices, 1);
    comboAtts.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (proc.apvts, id, *b));
    addAndMakeVisible (b);
    return *b;
}

void R744Editor::hook (DiceButton& b, Action a)
{
    b.onClick = [this, &b, a] { b.roll(); proc.act (a); };
    addAndMakeVisible (b);
}

void R744Editor::hook (SmallButton& b, Action a)
{
    b.onClick = [this, a] { proc.act (a); };
    addAndMakeVisible (b);
}

R744Editor::R744Editor (R744Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.apvts;

    struct K { Knob& k; const char* id; const char* label; juce::Colour c; };
    for (auto& k : { K { kEvap, "evap", "EXPANSION", cold }, K { kGrain, "grain", "KORN", cold },
                     K { kSpread, "spread", "SPRIDNING", cold }, K { kGate, "gate", "LÄNGD", concrete },
                     K { kPress, "press", "TRYCK", sodium }, K { kFold, "fold", "VIKNING", sodium },
                     K { kCrush, "crush", "KROSS", sodium }, K { kBass, "bass", "BAS-LÅS", concrete },
                     K { kTemp, "temp", "AMBIENT TEMP", sodium }, K { kReso, "reso", "RESONANS", sodium },
                     K { kHunt, "hunt", "PENDLING", cold },
                     K { kValve, "valve", "TRYCKFALL", cold }, K { kCirc, "circ", "CIRKULATION", cold },
                     K { kSwing, "swing", "SWING", concrete }, K { kDrift, "drift", "DRIFT", red },
                     K { kSteps, "steps", "STEG", concrete }, K { kPitch, "pitch", "TONH", concrete },
                     K { kHeat, "superheat", "SUPERHEAT", cold }, K { kOut, "out", "UT", concrete } })
    {
        k.k.attach (st, k.id, utf8 (k.label));
        k.k.setAccent (k.c);
        addAndMakeVisible (k.k);
    }
    for (auto* id : { "ftype", "huntRate", "echo", "rate", "slices", "evolve" }) combo (id);

    // presets
    const auto& rc = r7::recipes();
    for (int i = 0; i < (int) rc.size(); ++i) catBox.addItem (utf8 (rc[(size_t) i].name), i + 1);
    catBox.onChange = [this] { const int c = catBox.getSelectedId() - 1; if (c >= 0 && c != proc.currentCat) { proc.applyFactory (c, 0); } };
    presetBox.onChange = [this] { const int i = presetBox.getSelectedId() - 1; if (i >= 0 && i != proc.currentIndex) proc.applyFactory (proc.currentCat, i); };
    auto step = [this] (int dir)
    {
        int c = proc.currentCat, i = proc.currentIndex + dir;
        const int nc = (int) r7::recipes().size();
        if (i < 0) { c = (c + nc - 1) % nc; i = r7::kPerCat - 1; }
        if (i >= r7::kPerCat) { c = (c + 1) % nc; i = 0; }
        proc.applyFactory (c, i);
    };
    prevB.onClick = [step] { step (-1); };
    nextB.onClick = [step] { step (1); };
    for (juce::Component* c : { (juce::Component*) &catBox, (juce::Component*) &presetBox, (juce::Component*) &prevB, (juce::Component*) &nextB })
        addAndMakeVisible (c);

    // idéer och tärningar
    hook (ideaB, Action::Idea); hook (allB, Action::All); hook (mutB, Action::Mutate);
    hook (dEvap, Action::SecEvap); hook (dComp, Action::SecComp); hook (dCool, Action::SecCool);
    hook (dValve, Action::SecValve); hook (dGroove, Action::SecGroove);
    hook (dSteps, Action::PatSteps); hook (dSlices, Action::PatSlices); hook (dAcc, Action::PatAccents); hook (dRat, Action::PatRatchets);
    hook (euclidB, Action::Euclid); hook (shiftLB, Action::ShiftL); hook (shiftRB, Action::ShiftR);
    hook (mirrorB, Action::Mirror); hook (clearB, Action::Clear);
    undoB.onClick = [this] { proc.undo(); };
    addAndMakeVisible (undoB);

    holdB.setClickingTogglesState (true);
    holdB.setTooltip (utf8 ("PÅ: spelar när Bitwig spelar (och fritt i appen). AV: spelar bara när du håller en tangent."));
    holdAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, "hold", holdB);
    loadB.onClick = [this] { openFile(); };
    resetB.onClick = [this] { proc.resetSystem(); msg = utf8 ("SYSTEMET ÅTERSTÄLLT"); };
    for (auto* b : { &loadB, &holdB, &resetB }) addAndMakeVisible (b);

    addAndMakeVisible (grid);
    addChildComponent (about);
    refreshPresetBoxes();

    setSize (W, H);
    startTimerHz (30);
}

R744Editor::~R744Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void R744Editor::refreshPresetBoxes()
{
    catBox.setSelectedId (proc.currentCat + 1, juce::dontSendNotification);
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < r7::kPerCat; ++i)
        presetBox.addItem (utf8 (r7::presetName (proc.currentCat, i).c_str()), i + 1);
    presetBox.setSelectedId (proc.currentIndex + 1, juce::dontSendNotification);
    presetBox.setText (proc.currentName, juce::dontSendNotification);
}

void R744Editor::resized()
{
    auto findCombo = [this] (const char* id) -> juce::ComboBox& { for (auto* c : combos) if (c->getName() == id) return *c; return *combos[0]; };

    // rad 1: logga, presets, ladda/hold/reset
    logoArea = { 20, 6, 290, 44 };
    catBox.setBounds (330, 12, 130, 28);
    prevB.setBounds (466, 12, 26, 28);
    presetBox.setBounds (496, 12, 220, 28);
    nextB.setBounds (720, 12, 26, 28);
    resetB.setBounds (W - 20 - 80, 12, 80, 28);
    holdB.setBounds (resetB.getX() - 88, 12, 80, 28);
    loadB.setBounds (holdB.getX() - 88, 12, 80, 28);

    // rad 2: idéer
    ideaB.setBounds (20, 54, 130, 36);
    allB.setBounds (158, 56, 110, 32);
    mutB.setBounds (276, 56, 124, 32);
    undoB.setBounds (408, 56, 80, 32);
    findCombo ("evolve").setBounds (612, 58, 130, 28);

    // rad 3: vågform och p-h-diagram
    waveArea = { 20, 100, 660, 160 };
    phArea = { 692, 100, W - 20 - 692, 160 };

    // rad 4: sekvensern och groove
    seqArea = { 20, 272, 720, 150 };
    for (int i = 0; i < 4; ++i) rowLabels[i] = { 30, 302 + i * 23, 86, 23 };
    grid.setBounds (120, 300, 610, 92);
    dSteps.setBounds (96, 302, 20, 20);
    dAcc.setBounds (96, 325, 20, 20);
    dRat.setBounds (96, 348, 20, 20);
    dSlices.setBounds (74, 302, 20, 20);
    int tx = 120;
    for (auto* b : { &euclidB, &shiftLB, &shiftRB, &mirrorB, &clearB })
    {
        const int w = b->text.length() > 3 ? 70 : 36;
        b->setBounds (tx, 396, w, 22);
        tx += w + 6;
    }
    grooveArea = { 752, 272, W - 20 - 752, 150 };
    dGroove.setBounds (grooveArea.getRight() - 30, 278, 22, 22);
    {
        auto r = grooveArea.reduced (8).withTrimmedTop (22);
        auto knobs = r.removeFromTop (84);
        const int kw = knobs.getWidth() / 4;
        for (auto* k : { &kSwing, &kDrift, &kSteps, &kPitch }) k->setBounds (knobs.removeFromLeft (kw).reduced (2, 0));
        r.removeFromTop (4);
        findCombo ("rate").setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (4, 0).withHeight (24));
        findCombo ("slices").setBounds (r.reduced (4, 0).withHeight (24));
    }

    // rad 5: kretsens fem steg
    const int y = 434, ph = 200;
    panels[0] = { 20, y, 236, ph };
    panels[1] = { 266, y, 236, ph };
    panels[2] = { 512, y, 236, ph };
    panels[3] = { 758, y, 150, ph };
    panels[4] = { 918, y, W - 20 - 918, ph };
    DiceButton* dice[] = { &dEvap, &dComp, &dCool, &dValve };
    for (int i = 0; i < 4; ++i) dice[i]->setBounds (panels[i].getRight() - 30, y + 6, 22, 22);

    auto grid2 = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> cs)
    {
        r = r.reduced (8).withTrimmedTop (26);
        const int cw = r.getWidth() / 2, ch = r.getHeight() / 2;
        int i = 0;
        for (auto* c : cs)
        {
            if (c != nullptr) c->setBounds (r.getX() + (i % 2) * cw, r.getY() + (i / 2) * ch, cw, ch);
            ++i;
        }
    };
    grid2 (panels[0], { &kEvap, &kGrain, &kSpread, &kGate });
    grid2 (panels[1], { &kPress, &kFold, &kCrush, &kBass });
    grid2 (panels[2], { &kTemp, &kReso, &kHunt, nullptr });
    {
        auto r = panels[2].reduced (8).withTrimmedTop (26);
        auto cell = juce::Rectangle<int> (r.getX() + r.getWidth() / 2, r.getY() + r.getHeight() / 2, r.getWidth() / 2, r.getHeight() / 2).reduced (6, 10);
        findCombo ("ftype").setBounds (cell.removeFromTop (24));
        cell.removeFromTop (8);
        findCombo ("huntRate").setBounds (cell.removeFromTop (24));
    }
    {
        auto r = panels[3].reduced (8).withTrimmedTop (26);
        auto top = r.removeFromTop (r.getHeight() - 34);
        kValve.setBounds (top.removeFromLeft (top.getWidth() / 2));
        kCirc.setBounds (top);
        findCombo ("echo").setBounds (r.reduced (2, 4));
    }
    kHeat.setBounds (panels[4].withTrimmedTop (28).withSizeKeepingCentre (148, 164));

    gaugeArea = { 20, 646, W - 40 - 100, 60 };
    kOut.setBounds (gaugeArea.getRight() + 14, 640, 86, 76);

    about.setBounds (getLocalBounds());
}

void R744Editor::paint (juce::Graphics& g)
{
    g.fillAll (ground);

    // logga, klicka för om-sidan
    g.setColour (ArmColours::text);
    g.setFont (juce::FontOptions (38.0f, juce::Font::bold).withHorizontalScale (0.85f));
    g.drawText ("R-744", logoArea.withWidth (120), juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("TRANSCRITICAL", logoArea.withTrimmedLeft (124).removeFromTop (24).withTrimmedTop (8), juce::Justification::bottomLeft);
    g.drawText ("SAMPLER", logoArea.withTrimmedLeft (124).withTrimmedTop (24), juce::Justification::topLeft);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (utf8 ("SJÄLVSPEL"), juce::Rectangle<int> (530, 58, 78, 28), juce::Justification::centredRight);
    g.setColour (ArmColours::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (msg, juce::Rectangle<int> (760, 58, W - 780, 28), juce::Justification::centredRight, true);

    paintWave (g, waveArea);
    paintPh (g, phArea);

    // sekvensern
    drawPanel (g, seqArea, "VENTILSEKVENSER", utf8 ("klicka = på/av   dra upp/ner = slice"));
    const char* rows[] = { "STEG", "ACCENT", "RATCHET", "BAKÅT" };
    g.setColour (muted);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int i = 0; i < 4; ++i)
        g.drawText (utf8 (rows[i]), rowLabels[i].withWidth (44), juce::Justification::centredLeft);
    drawPanel (g, grooveArea, "GROOVE", {});

    // kretsen
    const bool flash = flashFrames > 0;
    drawPanel (g, panels[0], "1  EVAPORATOR", {});
    drawPanel (g, panels[1], "2  COMPRESSOR", {});
    drawPanel (g, panels[2], "3  GAS COOLER", {});
    drawPanel (g, panels[3], "4  VALVE", {});
    drawPanel (g, panels[4], "SUPERHEAT", "makro");
    if (flash)
    {
        g.setColour (red.withAlpha (juce::jmin (1.0f, flashFrames / 6.0f)));
        g.drawRoundedRectangle (panels[3].toFloat().reduced (1.0f), 6.0f, 2.0f);
    }
    // flödespilar mellan stegen
    g.setColour (sodium);
    for (int i = 0; i < 3; ++i)
    {
        const float x = (float) panels[i].getRight() + 5.0f, yy = (float) panels[i].getCentreY();
        juce::Path a;
        a.addTriangle (x - 3.0f, yy - 6.0f, x - 3.0f, yy + 6.0f, x + 4.0f, yy);
        g.fillPath (a);
    }

    paintGauge (g, gaugeArea);
    paintFreeze (g, waveArea.getUnion (phArea));

    g.setColour (muted);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (utf8 ("PHUNKSHUNZ AUDIO  ·  MASKINHALLEN"), juce::Rectangle<int> (20, H - 26, 400, 20), juce::Justification::centredLeft);
    g.drawText (utf8 ("Håll SUPERHEAT i botten i 2 s så fryser kretsen"), juce::Rectangle<int> (W - 440, H - 26, 420, 20), juce::Justification::centredRight);
}

void R744Editor::paintWave (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawPanel (g, area, {});
    auto r = area.reduced (10, 8).toFloat();
    const auto& pk = proc.peaks();
    if (pk.empty())
    {
        g.setColour (muted);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (utf8 ("SLÄPP EN LJUDFIL HÄR  ·  ELLER TRYCK LADDA"), r, juce::Justification::centred);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (utf8 ("Loopar, trumljud, röster, synthar. Den hackas i slices och spelas av sekvensern."), r.withTrimmedTop (44), juce::Justification::centred);
        return;
    }
    const int nSlices = r7::kSliceCounts[juce::jlimit (0, 3, (int) proc.apvts.getRawParameterValue ("slices")->load())];
    const int playing = proc.engine.uiSlice.load();
    const float sw = r.getWidth() / (float) nSlices;
    if (playing >= 0 && playing < nSlices && proc.engine.uiRunning.load())
    {
        g.setColour (sodium.withAlpha (0.12f));
        g.fillRect (r.getX() + playing * sw, r.getY(), sw, r.getHeight());
    }
    const float mid = r.getCentreY(), half = r.getHeight() * 0.5f;
    const float bw = r.getWidth() / (float) pk.size();
    g.setColour (line2.brighter (0.3f));
    for (size_t i = 0; i < pk.size(); ++i)
    {
        const float h = juce::jmax (1.0f, pk[i] * half);
        g.fillRect (r.getX() + (float) i * bw, mid - h, juce::jmax (1.0f, bw), h * 2.0f);
    }
    g.setColour (line2);
    for (int i = 1; i < nSlices; ++i) g.fillRect (r.getX() + i * sw, r.getY(), 1.0f, r.getHeight());
    g.setColour (muted);
    g.setFont (juce::FontOptions (9.0f));
    if (nSlices <= 16)
        for (int i = 0; i < nSlices; ++i) g.drawText (String (i + 1), juce::Rectangle<float> (r.getX() + i * sw + 3, r.getY(), 20, 12), juce::Justification::left);
    for (int v = 0; v < r7::kVoices; ++v)
    {
        const float pos = proc.engine.uiVoicePos[v].load();
        if (pos < 0.0f) continue;
        g.setColour (cold);
        g.fillRect (r.getX() + pos * r.getWidth() - 1.0f, r.getY(), 2.0f, r.getHeight());
    }
    g.setColour (ArmColours::text);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (proc.sampleName().toUpperCase(), r.removeFromBottom (14), juce::Justification::bottomRight, true);
}

// Ett log p-h-diagram för CO2. Kretsen ritas av rattarna och en prick åker runt i takt med sekvensern.
void R744Editor::paintPh (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawPanel (g, area, "p-h  R-744", {});
    auto r = area.reduced (14, 10).withTrimmedTop (18).toFloat();
    auto X = [&] (float h) { return r.getX() + (h - 150.0f) / 400.0f * r.getWidth(); };
    auto Y = [&] (float p) { return r.getBottom() - (std::log10 (p) - 1.0f) / (std::log10 (160.0f) - 1.0f) * r.getHeight(); };

    // mättningskupolen
    const float dome[][2] = { {200,10},{230,22},{258,38},{285,55},{308,67},{332,73.8f},{356,69},{385,58},{410,44},{428,28},{438,10} };
    juce::Path d;
    d.startNewSubPath (X (dome[0][0]), Y (dome[0][1]));
    for (auto& pt : dome) d.lineTo (X (pt[0]), Y (pt[1]));
    g.setColour (line2.brighter (0.25f));
    g.strokePath (d, juce::PathStrokeType (1.5f));
    // kritiska trycket
    const float yc = Y (73.8f);
    for (float x = r.getX(); x < r.getRight(); x += 8.0f) g.fillRect (x, yc, 4.0f, 1.0f);
    g.setColour (muted);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("73,8 bar", juce::Rectangle<float> (r.getX(), yc - 12, 60, 11), juce::Justification::left);

    // kretsen
    const float pr = proc.engine.uiPress.load(), tp = proc.engine.uiTemp.load();
    const float pe = 30.0f + proc.engine.uiLfo.load() * 3.0f * proc.apvts.getRawParameterValue ("hunt")->load();
    const float phi = 35.0f + 100.0f * pr;
    const float h1 = 435.0f, h2 = h1 + 25.0f + 0.45f * (phi - pe), h3 = 225.0f + 120.0f * tp;
    juce::Point<float> pts[] = { { X (h1), Y (pe) }, { X (h2), Y (phi) }, { X (h3), Y (phi) }, { X (h3), Y (pe) } };
    juce::Path cyc;
    cyc.startNewSubPath (pts[0]);
    for (int i = 1; i < 4; ++i) cyc.lineTo (pts[i]);
    cyc.closeSubPath();
    const auto col = heatColour (juce::jmax (pr, tp));
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (cyc);
    g.setColour (col);
    g.strokePath (cyc, juce::PathStrokeType (2.0f));
    const char* names[] = { "1", "2", "3", "4" };
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    for (int i = 0; i < 4; ++i) g.drawText (names[i], juce::Rectangle<float> (pts[i].x - 12, pts[i].y - 12, 10, 10), juce::Justification::centred);

    // prick som åker runt i takt
    float ph = proc.engine.uiPhase.load() * 4.0f;
    const int seg = juce::jlimit (0, 3, (int) ph);
    const float f = ph - (float) seg;
    const auto a = pts[seg], b = pts[(seg + 1) % 4];
    const auto dot = a + (b - a) * f;
    g.setColour (ArmColours::text);
    g.fillEllipse (dot.x - 4.0f, dot.y - 4.0f, 8.0f, 8.0f);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (phi > 73.8f ? red : cold);
    g.drawText (phi > 73.8f ? "TRANSCRITICAL" : "SUBCRITICAL", area.reduced (12, 8).removeFromTop (16), juce::Justification::centredRight);
}

void R744Editor::paintGauge (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawPanel (g, area, {});
    auto r = area.reduced (14, 8);
    auto& e = proc.engine;
    const float pr = e.uiPress.load(), tp = e.uiTemp.load();
    const float bar = 35.0f + 100.0f * pr, deg = 12.0f + 33.0f * tp;

    g.setColour (heatColour (juce::jmax (pr, tp)));
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold));
    g.drawText ("HIGH SIDE " + String (bar, 1).paddedLeft (' ', 5) + " bar    GC OUT " + String (deg, 1) + String::fromUTF8 (" °C"),
                r.removeFromTop (24), juce::Justification::centredLeft);

    g.setColour (muted);
    g.setFont (juce::FontOptions (11.0f));
    const String tele = "CPU " + String (juce::roundToInt (e.uiCpu.load() * 100.0f)) + " %   " + utf8 ("RÖSTER ") + String (e.uiVoices.load())
                      + "   TAKT " + String (e.uiBar.load() + 1) + "   FEL " + String (e.errors.load());
    g.drawText (tele, r, juce::Justification::centredLeft);

    String state;
    const float hold = e.uiHold.load();
    if (e.uiState.load() != r7::Engine::Run) state = "SYSTEM FREEZE";
    else if (e.uiSafe.load()) state = utf8 ("SÄKERT LÄGE - TRYCK RESET");
    else if (hold > 0.0f) state = "SUPERHEAT KRITISK  " + String (juce::jmax (0.0f, 2.0f - 2.0f * hold), 1) + " s";
    else if (! proc.hasSample()) state = "LADDA ETT LJUD";
    else if (! e.uiRunning.load()) state = utf8 ("STARTA BITWIG ELLER HÅLL EN TANGENT");
    g.setColour (state.startsWith ("SUPER") || state.startsWith ("SYSTEM") ? (blink ? red : sodium) : ArmColours::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (state, area.reduced (14, 8), juce::Justification::centredRight);
}

void R744Editor::paintFreeze (juce::Graphics& g, juce::Rectangle<int> area)
{
    const int state = proc.engine.uiState.load();
    if (state == r7::Engine::Run) return;
    g.setColour (ground.withAlpha (0.86f));
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (blink ? red : cold);
    g.drawRoundedRectangle (area.toFloat().reduced (1.0f), 6.0f, 2.0f);
    g.setFont (juce::FontOptions (44.0f, juce::Font::bold).withHorizontalScale (0.85f));
    g.drawText ("SYSTEM FREEZE", area.withTrimmedBottom (area.getHeight() / 2 + 4), juce::Justification::centredBottom);
    g.setColour (ArmColours::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (state == r7::Engine::Relief ? utf8 ("SÄKERHETSVENTILEN BLÅSER AV ...") : utf8 ("ISBILDNING I KRETSEN  ·  TRYCK RESET"),
                area.withTrimmedTop (area.getHeight() / 2 + 10), juce::Justification::centredTop);
}

void R744Editor::timerCallback()
{
    ++frame;
    if (frame % 8 == 0) blink = ! blink;

    const int v = proc.uiVersion.load();
    if (v != seenVersion)
    {
        seenVersion = v;
        refreshPresetBoxes();
        msg = proc.currentName.toUpperCase();
        repaint();
    }
    const int fl = proc.engine.uiFlash.load();
    if (fl != seenFlash) { seenFlash = fl; flashFrames = 6; }
    if (flashFrames > 0) { --flashFrames; repaint (panels[3]); }
    undoB.setEnabled (proc.canUndo());
    kHeat.setAccent (heatColour ((float) kHeat.slider.getValue()));

    repaint (waveArea.getUnion (phArea));
    repaint (gaugeArea);
    grid.repaint();
}

void R744Editor::mouseDown (const juce::MouseEvent& e)
{
    if (logoArea.contains (e.getPosition()))
    {
        about.setVisible (true);
        about.toFront (false);
    }
}

void R744Editor::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Ladda ljud", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                                  msg = proc.loadSample (f) ? f.getFileName().toUpperCase() : utf8 ("KUNDE INTE LÄSA FILEN");
                              repaint();
                          });
}

void R744Editor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    const juce::File f (files[0]);
    msg = proc.loadSample (f) ? f.getFileName().toUpperCase() : utf8 ("KUNDE INTE LÄSA FILEN");
    repaint();
}
