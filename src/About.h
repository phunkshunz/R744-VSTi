#pragma once

#include "Gui.h"

// MASKINHALLEN: om-sidan som öppnas när man klickar på loggan. Klick var som helst stänger.
class AboutOverlay : public juce::Component
{
public:
    AboutOverlay (const juce::String& productName, const juce::String& productLine)
        : product (productName), line (productLine) {}

    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        g.fillAll (ground.withAlpha (0.9f));
        auto card = getLocalBounds().withSizeKeepingCentre (680, 520);
        g.setColour (panel);
        g.fillRoundedRectangle (card.toFloat(), 8.0f);
        g.setColour (sodium);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 8.0f, 1.2f);

        // varningsrand i överkant, som på en maskinhall
        auto stripe = card.removeFromTop (10).toFloat().reduced (1.0f, 0.0f);
        g.saveState();
        g.reduceClipRegion (stripe.toNearestInt());
        for (float x = stripe.getX() - 20.0f; x < stripe.getRight(); x += 20.0f)
        {
            juce::Path p;
            p.addQuadrilateral (x, stripe.getBottom(), x + 10.0f, stripe.getY(), x + 20.0f, stripe.getY(), x + 10.0f, stripe.getBottom());
            g.setColour (sodium);
            g.fillPath (p);
        }
        g.restoreState();

        auto r = card.reduced (44, 28);
        g.setColour (sodium);
        g.setFont (juce::FontOptions (46.0f, juce::Font::bold).withHorizontalScale (0.8f));
        g.drawText ("MASKINHALLEN", r.removeFromTop (48), juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (juce::String::fromUTF8 ("BETONG  ·  ARMERING  ·  FUKT  ·  KOLV  ·  SMUTS  ·  VALV  ·  SMIDET  ·  R-744"), r.removeFromTop (16), juce::Justification::centredLeft);
        r.removeFromTop (26);

        g.setColour (text);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("Byggt i ett båtvarv."), r.removeFromTop (30), juce::Justification::centredLeft);
        r.removeFromTop (12);

        juce::AttributedString body;
        body.setLineSpacing (5.0f);
        const auto bodyFont = juce::Font (juce::FontOptions (14.5f));
        const auto boldFont = juce::Font (juce::FontOptions (14.5f, juce::Font::bold));
        auto add = [&] (const char* s, bool bold = false) { body.append (juce::String::fromUTF8 (s), bold ? boldFont : bodyFont, bold ? sodium : text); };
        add ("På 90-talet låg Stockholms fetaste technoklubb i ett skramlande båtvarv. Den hette ");
        add ("Docklands", true);
        add (".\n\nDär lärde jag mig hur techno ska kännas: basen som får plåten att skramla, golvet som svettas, "
             "och rytmen som hänger efter, snubblar och ändå alltid landar på ettan.\n\n"
             "Någon sa att min musik låter som ett fuktigt betonggolv. Det tog jag som ett beröm.\n\n");
        add ("R-744", true);
        add (" kommer från kylbranschen: ljudet går genom en CO2-krets, "
             "från förångaren till ventilen. Som allt annat i Maskinhallen är den byggd för att låta som det golvet.");
        juce::TextLayout layout;
        layout.createLayout (body, (float) r.getWidth());
        layout.draw (g, r.removeFromTop ((int) layout.getHeight() + 4).toFloat());

        auto foot = card.reduced (44, 22).removeFromBottom (18);
        g.setColour (muted);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (product + juce::String::fromUTF8 ("  ·  ") + line, foot, juce::Justification::centredLeft);
        g.drawText (juce::String::fromUTF8 ("Klicka för att stänga"), foot, juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }

private:
    juce::String product, line;
};
