#pragma once
// R-744: generatorer. Fabrikspresets, tärningar och mönster. Allt körs på meddelandetråden.
#include "Engine.h"
#include <map>
#include <string>

namespace r7
{
using Patch = std::map<std::string, float>;   // parameter-id -> verkligt värde

// enkel deterministisk slump så fabrikspresets blir samma varje gång
class Dice
{
public:
    explicit Dice (uint32_t seed = 1) : s (seed * 2654435761u + 1u) {}
    float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); }
    float range (float a, float b) { return a + (b - a) * uni(); }
    int pick (int n) { return std::min (n - 1, (int) (uni() * (float) n)); }
    int irange (int a, int b) { return a + pick (b - a + 1); }
    bool chance (float p) { return uni() < p; }
private:
    uint32_t s;
};

//==============================================================================
// MÖNSTER
inline void euclid (Pattern& p, int k, int n, int rot)
{
    for (int i = 0; i < kSteps; ++i) p.s[i].on = 0;
    if (k <= 0) return;
    for (int i = 0; i < n; ++i)
        p.s[(i + rot) % kSteps].on = ((i * k) % n) < k ? 1 : 0;
}

enum SliceStyle { SlSeq, SlShuffle, SlRandom, SlStutter, SlBack };

inline void genSlices (Pattern& p, Dice& d, int style)
{
    int pool[4] = { d.pick (32), d.pick (32), d.pick (32), d.pick (32) };
    int order[kSteps];
    for (int i = 0; i < kSteps; ++i) order[i] = i;
    for (int i = kSteps - 1; i > 0; --i) std::swap (order[i], order[d.pick (i + 1)]);
    for (int i = 0; i < kSteps; ++i)
    {
        int v = i;
        switch (style)
        {
            case SlShuffle: v = order[i]; break;
            case SlRandom:  v = d.pick (32); break;
            case SlStutter: v = pool[d.pick (d.chance (0.6f) ? 2 : 4)]; break;
            case SlBack:    v = kSteps - 1 - i; break;
            default:        v = d.chance (0.15f) ? d.pick (32) : i; break;
        }
        p.s[i].slice = (uint8_t) v;
    }
}

inline void genSteps (Pattern& p, Dice& d, float density)
{
    const int mode = d.pick (3);
    if (mode == 0) euclid (p, std::max (1, (int) std::round (density * 16.0f)), 16, d.pick (4));
    else if (mode == 1)
    {
        // techno-skelett: fyra fasta plus fyllning
        for (int i = 0; i < kSteps; ++i) p.s[i].on = (i % 4 == 0) || d.chance (density * 0.6f);
    }
    else for (auto& s : p.s) s.on = d.chance (density);
    bool any = false;
    for (auto& s : p.s) any |= s.on != 0;
    if (! any) p.s[0].on = 1;
}

inline void genAccents (Pattern& p, Dice& d)
{
    const int mode = d.pick (3);
    for (int i = 0; i < kSteps; ++i)
        p.s[i].accent = mode == 0 ? (i % 4 == 0) : mode == 1 ? (i % 4 == 2) : d.chance (0.3f);
}

inline void genRatchets (Pattern& p, Dice& d, float amount)
{
    for (auto& s : p.s)
    {
        s.ratchet = d.chance (amount) ? (uint8_t) d.irange (2, 4) : 1;
        s.rev = d.chance (amount * 0.6f) ? 1 : 0;
    }
}

inline void rotate (Pattern& p, int dir)
{
    Pattern q = p;
    for (int i = 0; i < kSteps; ++i) p.s[(i + dir + kSteps) % kSteps] = q.s[i];
}

inline void mirror (Pattern& p)
{
    Pattern q = p;
    for (int i = 0; i < kSteps; ++i) p.s[i] = q.s[kSteps - 1 - i];
}

// liten förändring: används av MUTERA och av självspelet var N:e takt
inline void mutatePattern (Pattern& p, Dice& d, int changes)
{
    for (int c = 0; c < changes; ++c)
    {
        auto& s = p.s[d.pick (kSteps)];
        switch (d.pick (5))
        {
            case 0: s.on = ! s.on; break;
            case 1: s.slice = (uint8_t) d.pick (32); break;
            case 2: s.accent = ! s.accent; break;
            case 3: s.ratchet = s.ratchet > 1 ? 1 : (uint8_t) d.irange (2, 4); break;
            default: std::swap (s.slice, p.s[d.pick (kSteps)].slice); break;
        }
    }
    bool any = false;
    for (auto& s : p.s) any |= s.on != 0;
    if (! any) p.s[d.pick (kSteps)].on = 1;
}

//==============================================================================
// KATEGORIER. Varje kategori är ett recept: intervall per ratt + hur mönstret ska se ut.
struct Range { const char* id; float lo, hi; };
struct Recipe
{
    const char* name;
    std::vector<Range> r;
    float density, ratchets;
    int sliceStyle;
};

inline const std::vector<Recipe>& recipes()
{
    static const std::vector<Recipe> list = {
        { "HYPNOS", { {"evap",.1f,.4f},{"grain",.3f,.6f},{"spread",0,.3f},{"gate",.3f,.6f},{"press",.1f,.4f},{"fold",0,.2f},{"crush",0,.1f},
                      {"bass",0,.4f},{"temp",.2f,.45f},{"reso",.3f,.7f},{"hunt",.3f,.7f},{"huntRate",4,6},{"circ",.3f,.6f},{"echo",1,3},
                      {"valve",0,.15f},{"drift",0,.1f},{"swing",0,.2f},{"steps",12,16},{"rate",1,1},{"ftype",0,0} }, .7f, .08f, SlStutter },
        { "HÅRD",   { {"evap",0,.15f},{"grain",.2f,.4f},{"spread",0,.1f},{"gate",.15f,.35f},{"press",.4f,.8f},{"fold",.2f,.6f},{"crush",.1f,.4f},
                      {"bass",.4f,.7f},{"temp",.05f,.3f},{"reso",.2f,.5f},{"hunt",0,.3f},{"huntRate",2,5},{"circ",.1f,.35f},{"echo",0,2},
                      {"valve",.1f,.3f},{"drift",0,.08f},{"swing",0,.1f},{"steps",16,16},{"rate",1,1},{"ftype",0,0} }, .6f, .15f, SlSeq },
        { "DUB",    { {"evap",.2f,.5f},{"grain",.4f,.7f},{"spread",.1f,.4f},{"gate",.4f,.8f},{"press",.1f,.3f},{"fold",0,.15f},{"crush",0,.1f},
                      {"bass",.2f,.5f},{"temp",.35f,.6f},{"reso",.3f,.6f},{"hunt",.2f,.5f},{"huntRate",4,6},{"circ",.6f,.9f},{"echo",2,4},
                      {"valve",0,.15f},{"drift",0,.15f},{"swing",.1f,.3f},{"steps",16,16},{"rate",1,3},{"ftype",0,1} }, .25f, .05f, SlShuffle },
        { "BRUTEN", { {"evap",0,.4f},{"grain",.2f,.6f},{"spread",0,.4f},{"gate",.2f,.5f},{"press",.2f,.5f},{"fold",0,.4f},{"crush",0,.3f},
                      {"bass",.2f,.6f},{"temp",.1f,.4f},{"reso",.2f,.6f},{"hunt",.1f,.5f},{"huntRate",1,4},{"circ",.1f,.5f},{"echo",0,4},
                      {"valve",.2f,.5f},{"drift",.4f,.8f},{"swing",.2f,.6f},{"steps",5,15},{"rate",0,4},{"ftype",0,2} }, .5f, .2f, SlShuffle },
        { "MOLN",   { {"evap",.6f,1},{"grain",.4f,.9f},{"spread",.5f,1},{"gate",.7f,1},{"press",0,.2f},{"fold",0,.1f},{"crush",0,.05f},
                      {"bass",0,.3f},{"temp",.3f,.6f},{"reso",.2f,.5f},{"hunt",.3f,.8f},{"huntRate",4,6},{"circ",.3f,.7f},{"echo",2,4},
                      {"valve",0,.1f},{"drift",0,.3f},{"swing",0,.2f},{"steps",8,16},{"rate",3,5},{"ftype",0,1} }, .3f, 0, SlRandom },
        { "SKRIK",  { {"evap",0,.3f},{"grain",.2f,.5f},{"spread",0,.2f},{"gate",.2f,.5f},{"press",.3f,.6f},{"fold",.3f,.7f},{"crush",0,.2f},
                      {"bass",.3f,.6f},{"temp",.3f,.7f},{"reso",.75f,1},{"hunt",.5f,1},{"huntRate",1,3},{"circ",.1f,.4f},{"echo",0,2},
                      {"valve",0,.25f},{"drift",0,.1f},{"swing",0,.25f},{"steps",16,16},{"rate",1,1},{"ftype",0,1} }, .75f, .1f, SlSeq },
        { "TRYCKFALL", { {"evap",0,.4f},{"grain",.1f,.4f},{"spread",0,.4f},{"gate",.1f,.4f},{"press",.3f,.7f},{"fold",.1f,.5f},{"crush",.3f,.7f},
                      {"bass",.3f,.6f},{"temp",.1f,.4f},{"reso",.2f,.6f},{"hunt",0,.4f},{"huntRate",0,3},{"circ",.1f,.5f},{"echo",0,2},
                      {"valve",.5f,.9f},{"drift",.2f,.5f},{"swing",0,.3f},{"steps",12,16},{"rate",0,2},{"ftype",0,2} }, .6f, .35f, SlRandom },
        { "ISKALL", { {"evap",0,.15f},{"grain",.2f,.4f},{"spread",0,.1f},{"gate",.1f,.3f},{"press",0,.2f},{"fold",0,.1f},{"crush",0,0},
                      {"bass",0,.3f},{"temp",0,.2f},{"reso",.1f,.4f},{"hunt",0,.3f},{"huntRate",3,6},{"circ",0,.3f},{"echo",1,3},
                      {"valve",0,.1f},{"drift",0,.05f},{"swing",.1f,.3f},{"steps",16,16},{"rate",1,1},{"ftype",0,0} }, .35f, .05f, SlSeq },
    };
    return list;
}

inline bool isChoice (const std::string& id) { return id == "huntRate" || id == "echo" || id == "rate" || id == "ftype" || id == "steps" || id == "slices"; }

inline void applyRecipe (Patch& p, Pattern& pat, const Recipe& rc, Dice& d)
{
    for (auto& x : rc.r)
    {
        float v = d.range (x.lo, x.hi);
        if (isChoice (x.id)) v = (float) d.irange ((int) x.lo, (int) x.hi);
        p[x.id] = v;
    }
    p["slices"] = d.chance (0.7f) ? 2.0f : (float) d.irange (1, 3);
    p["pitch"] = d.chance (0.75f) ? 0.0f : (float) (d.chance (0.5f) ? -12 : (d.pick (2) ? 7 : -5));
    p["superheat"] = 0.0f;
    genSteps (pat, d, rc.density);
    genSlices (pat, d, rc.sliceStyle);
    genAccents (pat, d);
    genRatchets (pat, d, rc.ratchets);
}

// ALLT: vild slump över allt, utan recept
inline void wild (Patch& p, Pattern& pat, Dice& d)
{
    for (auto* id : { "evap","grain","spread","gate","press","fold","crush","bass","temp","reso","hunt","circ","valve","swing","drift" })
        p[id] = d.uni();
    p["temp"] = d.range (0, .7f);
    p["circ"] = d.range (0, .85f);
    p["huntRate"] = (float) d.pick (7); p["echo"] = (float) d.pick (5); p["ftype"] = (float) d.pick (3);
    p["rate"] = (float) d.pick (6); p["slices"] = (float) d.pick (4); p["steps"] = (float) d.irange (3, 16);
    p["pitch"] = (float) d.irange (-12, 12);
    p["superheat"] = 0.0f;
    genSteps (pat, d, d.range (.2f, .9f));
    genSlices (pat, d, d.pick (5));
    genAccents (pat, d);
    genRatchets (pat, d, d.range (0, .4f));
}

// sektionstärningar: bara en del av receptet, från en slumpad kategori
inline void diceSection (Patch& p, std::initializer_list<const char*> ids, Dice& d)
{
    const auto& rc = recipes()[(size_t) d.pick ((int) recipes().size())];
    for (auto* id : ids)
        for (auto& x : rc.r)
            if (std::string (x.id) == id)
                p[id] = isChoice (id) ? (float) d.irange ((int) x.lo, (int) x.hi) : d.range (x.lo, x.hi);
}

// namn på fabrikspresets, av kylteknikens ord
inline std::string presetName (int cat, int i)
{
    static const char* adj[] = { "Isig", "Läckande", "Kall", "Het", "Tryckt", "Fuktig", "Rostig", "Frusen",
                                 "Kokande", "Överhettad", "Underkyld", "Pendlande", "Svettig", "Trasig", "Mörk", "Tjutande" };
    static const char* noun[] = { "Ventil", "Kompressor", "Förångare", "Gaskylare", "Krets", "Kapillär", "Pressostat", "Lödning",
                                  "Kondensor", "Fläkt", "Schrader", "Torkfilter", "Manometer", "Köldbrygga", "Sugledning", "Docka" };
    Dice d ((uint32_t) (cat * 131 + i * 7 + 3));
    return std::string (adj[(i * 5 + cat * 3 + d.pick (3)) % 16]) + " " + noun[(i * 11 + cat * 7) % 16];
}

constexpr int kPerCat = 16;
} // namespace r7
