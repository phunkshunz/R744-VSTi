#include "PluginEditor.h"

using namespace ArmColours;
using juce::String;

namespace
{
const juce::Colour red { 0xffe5432f };

// kall cyan -> natriumorange -> rött, som ett kylsystem som går varmt
juce::Colour heatColour (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    return v < 0.5f ? cold.interpolatedWith (sodium, v * 2.0f) : sodium.interpolatedWith (red, (v - 0.5f) * 2.0f);
}

constexpr int W = 880, H = 534;
}

R744Editor::R744Editor (R744Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& st = proc.apvts;

    struct K { Knob& k; const char* id; const char* label; juce::Colour c; };
    for (auto& k : { K { evap, "evap", "EXPANSION", cold }, K { press, "press", "PRESSURE", sodium },
                     K { temp, "temp", "AMBIENT TEMP", sodium }, K { valve, "valve", "PRESSURE DROP", cold },
                     K { heat, "superheat", "SUPERHEAT", cold }, K { out, "out", "OUTPUT", concrete } })
    {
        k.k.attach (st, k.id, k.label);
        k.k.setAccent (k.c);
        addAndMakeVisible (k.k);
    }

    holdB.setClickingTogglesState (true);
    holdB.setTooltip (utf8 ("Spelar utan MIDI. Annars styr tangenterna tonhöjd och startar om molnet."));
    holdAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, "hold", holdB);
    loadB.onClick = [this] { openFile(); };
    resetB.onClick = [this] { proc.resetSystem(); msg = utf8 ("SYSTEMET ÅTERSTÄLLT"); };
    for (auto* b : { &loadB, &holdB, &resetB }) addAndMakeVisible (b);

    addChildComponent (about);
    msg = proc.sampleName().isEmpty() ? utf8 ("SLÄPP EN LJUDFIL HÄR ELLER TRYCK LADDA") : proc.sampleName().toUpperCase();

    setSize (W, H);
    startTimerHz (30);
}

R744Editor::~R744Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void R744Editor::resized()
{
    logoArea = { 20, 10, 280, 46 };
    resetB.setBounds (W - 20 - 84, 18, 84, 28);
    holdB.setBounds (resetB.getX() - 92, 18, 84, 28);
    loadB.setBounds (holdB.getX() - 92, 18, 84, 28);

    waveArea = { 20, 66, W - 40, 120 };

    for (int i = 0; i < 4; ++i)
        panels[i] = { 20 + i * 170, 198, 152, 200 };
    panels[4] = { panels[3].getRight() + 16, 198, W - 20 - panels[3].getRight() - 16, 200 };

    Knob* stage[] = { &evap, &press, &temp, &valve };
    for (int i = 0; i < 4; ++i)
        stage[i]->setBounds (panels[i].withTrimmedTop (34).reduced (10, 4).withSizeKeepingCentre (112, 150));
    heat.setBounds (panels[4].withTrimmedTop (30).withSizeKeepingCentre (148, 164));

    gaugeArea = { 20, 410, W - 40 - 100, 76 };
    out.setBounds (gaugeArea.getRight() + 14, 404, 86, 92);

    about.setBounds (getLocalBounds());
}

void R744Editor::paint (juce::Graphics& g)
{
    g.fillAll (ground);

    // logga, klicka för om-sidan
    g.setColour (text);
    g.setFont (juce::FontOptions (38.0f, juce::Font::bold).withHorizontalScale (0.85f));
    g.drawText ("R-744", logoArea.withWidth (120), juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("TRANSCRITICAL", logoArea.withTrimmedLeft (124).removeFromTop (24).withTrimmedTop (8), juce::Justification::bottomLeft);
    g.drawText ("SAMPLER", logoArea.withTrimmedLeft (124).withTrimmedTop (24), juce::Justification::topLeft);

    paintWave (g, waveArea);

    const char* titles[] = { "1  EVAPORATOR", "2  COMPRESSOR", "3  GAS COOLER", "4  EXP. VALVE", "SUPERHEAT" };
    const char* subs[] = { "förångare", "kompressor", "gaskylare", "ventil", "makro" };
    for (int i = 0; i < 5; ++i)
        drawPanel (g, panels[i], titles[i], utf8 (subs[i]));

    // flödespilar mellan stegen, CO2:n går runt
    g.setColour (sodium);
    for (int i = 0; i < 3; ++i)
    {
        const float x = (float) panels[i].getRight() + 9.0f, y = (float) panels[i].getCentreY();
        juce::Path a;
        a.addTriangle (x - 4.0f, y - 6.0f, x - 4.0f, y + 6.0f, x + 4.0f, y);
        g.fillPath (a);
    }

    paintGauge (g, gaugeArea);

    g.setColour (muted);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (utf8 ("PHUNKSHUNZ AUDIO  ·  MASKINHALLEN"), juce::Rectangle<int> (20, H - 30, 400, 20), juce::Justification::centredLeft);
    g.drawText (utf8 ("Håll SUPERHEAT i botten i 2 s och systemet fryser"), juce::Rectangle<int> (W - 420, H - 30, 400, 20), juce::Justification::centredRight);
}

void R744Editor::paintWave (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawPanel (g, area, {});
    auto r = area.reduced (10, 8).toFloat();
    const auto& pk = proc.peaks();

    if (pk.empty())
    {
        g.setColour (muted);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (utf8 ("SLÄPP EN LJUDFIL HÄR"), r, juce::Justification::centred);
    }
    else
    {
        const float mid = r.getCentreY(), half = r.getHeight() * 0.5f;
        const float bw = r.getWidth() / (float) pk.size();
        g.setColour (line2);
        for (size_t i = 0; i < pk.size(); ++i)
        {
            const float h = juce::jmax (1.0f, pk[i] * half);
            g.fillRect (r.getX() + (float) i * bw, mid - h, juce::jmax (1.0f, bw), h * 2.0f);
        }
        // kornen i molnet
        for (int i = 0; i < r7::Evaporator::kMax; ++i)
        {
            const float pos = proc.engine.uiGrain[i].load();
            if (pos < 0.0f) continue;
            const float y = mid + (float) (((i * 37) % 17) - 8) / 8.0f * half * 0.8f;
            g.setColour (cold.withAlpha (0.85f));
            g.fillEllipse (r.getX() + pos * r.getWidth() - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }
        g.setColour (sodium);
        const float hx = r.getX() + proc.engine.uiHead.load() * r.getWidth();
        g.fillRect (hx - 1.0f, r.getY(), 2.0f, r.getHeight());
    }

    const int state = proc.engine.uiState.load();
    const float hold = proc.engine.uiHold.load();
    String big, small;
    if (state == r7::Engine::Relief) { big = "SYSTEM FREEZE"; small = utf8 ("TRYCKAVLASTNING ..."); }
    else if (state == r7::Engine::Dead) { big = "SYSTEM FREEZE"; small = utf8 ("TRYCK RESET"); }
    else if (proc.engine.uiSafe.load()) { big = utf8 ("SÄKERT LÄGE"); small = utf8 ("DSP-FEL. TRYCK RESET"); }

    if (big.isNotEmpty())
    {
        g.setColour (ground.withAlpha (0.82f));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (blink ? red : sodium);
        g.drawRoundedRectangle (area.toFloat().reduced (1.0f), 6.0f, 2.0f);
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold).withHorizontalScale (0.85f));
        g.drawText (big, area.withTrimmedBottom (34), juce::Justification::centredBottom);
        g.setColour (text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (small, area.withTrimmedTop (area.getHeight() / 2 + 8), juce::Justification::centredTop);
    }
    else if (hold > 0.0f)
    {
        auto bar = area.reduced (1).removeFromTop (22).toFloat();
        g.setColour (red.withAlpha (0.3f));
        g.fillRect (bar);
        g.setColour (red);
        g.fillRect (bar.withWidth (bar.getWidth() * juce::jmin (1.0f, hold)));
        g.setColour (text);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (utf8 ("SUPERHEAT KRITISK  ") + String (juce::jmax (0.0f, 2.0f - 2.0f * hold), 1) + " s",
                    bar.toNearestInt(), juce::Justification::centred);
    }
}

void R744Editor::paintGauge (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawPanel (g, area, {});
    auto r = area.reduced (14, 10);
    auto& st = proc.apvts;
    const float sh = st.getRawParameterValue ("superheat")->load();
    auto eff = [sh, &st] (const char* id) { const float v = st.getRawParameterValue (id)->load(); return v + (1.0f - v) * sh; };
    const float pr = eff ("press"), tp = eff ("temp");
    const float bar = 35.0f + 100.0f * pr, deg = 12.0f + 33.0f * tp;
    const bool trans = bar > 73.8f;          // CO2:s kritiska tryck

    g.setColour (heatColour (juce::jmax (pr, tp)));
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::bold));
    g.drawText ("HIGH SIDE " + String (bar, 1).paddedLeft (' ', 5) + " bar    GC OUT " + String (deg, 1) + String::fromUTF8 (" °C"),
                r.removeFromTop (26), juce::Justification::centredLeft);
    g.setColour (trans ? red : cold);
    g.drawText (trans ? "TRANSCRITICAL" : "SUBCRITICAL", area.reduced (14, 10).removeFromTop (26), juce::Justification::centredRight);

    auto& e = proc.engine;
    g.setColour (muted);
    g.setFont (juce::FontOptions (11.0f));
    const String tele = "CPU " + String (juce::roundToInt (e.uiCpu.load() * 100.0f)) + " %   KORN " + String (e.uiActive.load())
                      + "/" + String (e.uiMax.load()) + "   FEL " + String (e.errors.load());
    g.drawText (tele, r.removeFromTop (16).withTrimmedTop (6), juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText (msg, area.reduced (14, 10).withTrimmedTop (32), juce::Justification::centredRight);
}

void R744Editor::timerCallback()
{
    if (++frame % 8 == 0) blink = ! blink;
    heat.setAccent (heatColour (heat.slider.getValue() > 0.0 ? (float) heat.slider.getValue() : 0.0f));
    repaint (waveArea);
    repaint (gaugeArea);
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
