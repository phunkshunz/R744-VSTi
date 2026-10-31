#pragma once

// MASKINHALLEN · språk (SV / EN)
//
// Samma fil i alla maskiner. Det enda som skiljer är Ordlista.h, som har
// maskinens egna ord.
//
// Så funkar det:
//  - Koden skrivs på svenska som vanligt.
//  - sprak::tr() byter till engelska när EN är valt. Finns ordet inte i
//    ordlistan visas den svenska texten, så inget kan gå sönder.
//  - Valet sparas i en gemensam fil för alla Phunkshunz-plugins:
//      macOS:   ~/Library/Application Support/Phunkshunz/MASKINHALLEN.settings
//      Windows: %APPDATA%\Phunkshunz\MASKINHALLEN.settings
//      Linux:   ~/.config/Phunkshunz/MASKINHALLEN.settings
//    Väljer kunden EN i en maskin får alla andra maskiner också engelska.
//  - Första gången: svensk dator ger SV, alla andra får EN.
//  - Parameter-ID:n och namnen i DAW:ens automation rörs aldrig, så gamla
//    projekt och automationer fungerar precis som förut.

#include <juce_gui_basics/juce_gui_basics.h>
#include <string>
#include <unordered_map>

namespace sprak
{
struct Par { const char* sv; const char* en; };
}

#include "Ordlista.h"

namespace sprak
{
inline juce::PropertiesFile::Options installningar()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "MASKINHALLEN";
   #if JUCE_LINUX
    o.folderName          = ".config/Phunkshunz";
   #else
    o.folderName          = "Phunkshunz";
   #endif
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

inline bool svenskDator()
{
    return juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("sv")
        || juce::SystemStats::getDisplayLanguage().startsWithIgnoreCase ("sv");
}

inline bool& lage()
{
    static bool en = ! svenskDator();
    return en;
}

// Sant när valet är inläst. Text som skapas innan dess (när pluginet laddas)
// står kvar på svenska och översätts först när panelen byggs.
inline bool& laddat()
{
    static bool l = false;
    return l;
}

// Läser om valet från filen (anropas när fönstret öppnas, så ett val gjort
// i en annan maskin syns direkt).
inline void laddaOm()
{
    juce::PropertiesFile f (installningar());
    const auto v = f.getValue ("sprak");
    lage() = v.isEmpty() ? ! svenskDator() : (v == "en");
    laddat() = true;
}

inline bool engelska() { return laddat() && lage(); }

inline void sattEngelska (bool en)
{
    lage() = en;
    laddat() = true;
    juce::PropertiesFile f (installningar());
    f.setValue ("sprak", en ? "en" : "sv");
    f.saveIfNeeded();
}

inline const std::unordered_map<std::string, const char*>& karta()
{
    static const auto k = []
    {
        std::unordered_map<std::string, const char*> m;
        for (const auto& p : kOrdlista) m.emplace (p.sv, p.en);
        return m;
    }();
    return k;
}

// Svensk text in, rätt språk ut.
inline juce::String tr (const juce::String& s)
{
    if (! engelska() || s.isEmpty()) return s;
    const auto& k = karta();
    if (auto it = k.find (s.toStdString()); it != k.end())
        return juce::String::fromUTF8 (it->second);

    // samma ord med mellanslag runt om, t.ex. "SPÅR " i "SPÅR 3"
    const auto t = s.trim();
    if (t.isNotEmpty() && t != s)
        if (auto it = k.find (t.toStdString()); it != k.end())
        {
            const int start = s.indexOf (t);
            return s.substring (0, start) + juce::String::fromUTF8 (it->second) + s.substring (start + t.length());
        }
    return s;
}

inline juce::String trU (const char* s)        { return tr (juce::String::fromUTF8 (s)); }
inline juce::String trU (const char* s, int n) { return tr (juce::String::fromUTF8 (s, n)); }

// Går igenom en färdigbyggd panel och byter text på knappar, etiketter,
// listor och tooltips som skrevs in utan tr().
inline void oversattAllt (juce::Component& c)
{
    if (! engelska()) return;

    if (auto* b = dynamic_cast<juce::Button*> (&c))
        b->setButtonText (tr (b->getButtonText()));

    if (auto* l = dynamic_cast<juce::Label*> (&c))
        l->setText (tr (l->getText()), juce::dontSendNotification);

    if (auto* cb = dynamic_cast<juce::ComboBox*> (&c))
    {
        for (int i = 0; i < cb->getNumItems(); ++i)
            cb->changeItemText (cb->getItemId (i), tr (cb->getItemText (i)));
        cb->setTextWhenNothingSelected (tr (cb->getTextWhenNothingSelected()));
        cb->setTextWhenNoChoicesAvailable (tr (cb->getTextWhenNoChoicesAvailable()));
        cb->setText (tr (cb->getText()), juce::dontSendNotification);
    }

    if (auto* tt = dynamic_cast<juce::SettableTooltipClient*> (&c))
        tt->setTooltip (tr (tt->getTooltip()));

    for (auto* child : c.getChildren())
        oversattAllt (*child);
}

// SV | EN-knappen. Ritas som en liten skylt i maskinens färger.
class Knapp : public juce::Button
{
public:
    Knapp() : juce::Button ("SV | EN")
    {
        setTooltip ("Svenska / English");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Colour accent { 0xffff8a1f }, dim { 0xff8b8e91 }, kant { 0xff34383b };

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (over ? accent.withAlpha (0.7f) : kant);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        auto halv = r.getWidth() * 0.5f;
        auto sv = r.withWidth (halv), en = r.withTrimmedLeft (halv);
        g.setFont (juce::FontOptions (juce::jmin (11.0f, r.getHeight() * 0.72f), juce::Font::bold));
        g.setColour (engelska() ? dim : accent);
        g.drawText ("SV", sv, juce::Justification::centred, false);
        g.setColour (engelska() ? accent : dim);
        g.drawText ("EN", en, juce::Justification::centred, false);
    }
};
} // namespace sprak
