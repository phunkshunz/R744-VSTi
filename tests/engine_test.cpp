// R-744 KLANGMASKIN: test av motorn utan JUCE.
//   c++ -std=c++17 -O2 -Isrc tests/engine_test.cpp -o engine_test && ./engine_test
#include "Klang.h"
#include <cstdio>

using namespace r744;
static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

// En trumloop: kick (fallande sinus), hat (brus) och clap, 16 slag i 126 BPM
static Sample makeLoop (double sr)
{
    Sample s; s.rate = sr; s.beats = 16;
    const int len = (int) (16 * 60.0 / 126.0 * sr);
    s.L.assign ((size_t) len, 0.0f);
    Rng r (5);
    const int beat = len / 16;
    for (int b = 0; b < 16; ++b)
        for (int i = 0; i < beat; ++i)
        {
            const float t = (float) i / (float) sr;
            float x = 0.8f * std::sin (kTwoPi * (50.0f + 120.0f * std::exp (-t * 30.0f)) * t) * std::exp (-t * 9.0f);
            const int half = beat / 2;
            if (i >= half) { const float th = (float) (i - half) / (float) sr; x += 0.3f * (r.uni() * 2 - 1) * std::exp (-th * 60.0f); }
            s.L[(size_t) (b * beat + i)] += x;
        }
    s.R = s.L;
    return s;
}

static float bandEnergy (const std::vector<float>& x, float hz, double sr)
{
    double re = 0, im = 0;
    for (size_t i = 0; i < x.size(); ++i) { const double ph = 2.0 * 3.14159265358979 * hz * (double) i / sr; re += x[i] * std::cos (ph); im += x[i] * std::sin (ph); }
    return (float) (std::sqrt (re * re + im * im) / (double) x.size());
}

int main()
{
    const double sr = 48000.0;
    Sample loop = makeLoop (sr);
    Engine e; e.prepare (sr); e.setSample (&loop);

    Params p; p.loop = 0.0f; p.cloud = 0.0f; p.room = 0.0f; p.breath = 0.0f; p.wow = 0.0f;
    p.res = 1.0f; p.chord = 4; p.root = 9; p.octave = 0; p.spread = 0.0f; // A-kvint, inga oktaver
    e.setParams (p);

    const int n = 512; std::vector<float> L (n), R (n), all;
    double ppq = 0;
    for (int k = 0; k < 400; ++k) { e.process (L.data(), R.data(), n, true, ppq, 126.0); ppq += n / sr * 126.0 / 60.0; all.insert (all.end(), L.begin(), L.end()); }
    bool finite = true; float peak = 0; for (float v : all) { finite &= std::isfinite (v); peak = std::max (peak, std::fabs (v)); }
    CHECK (finite, "inga NaN");
    CHECK (peak > 0.02f && peak < 1.0f, "resonansen hörs och klipper aldrig");
    std::vector<float> tail (all.end() - 48000, all.end());
    const float onA = bandEnergy (tail, mtof (57), sr), onE = bandEnergy (tail, mtof (64), sr), off = bandEnergy (tail, mtof (61), sr);
    std::printf ("     A %.4f  E %.4f  C# (utanför) %.4f\n", onA, onE, off);
    CHECK (onA > off * 3 && onE > off * 3, "trummorna ringer i ackordets toner (A och E)");

    // hållna tangenter stämmer om
    int held[2] = { 60, 67 }; e.setHeld (held, 2);
    all.clear();
    for (int k = 0; k < 200; ++k) { e.process (L.data(), R.data(), n, true, ppq, 126.0); ppq += n / sr * 126.0 / 60.0; all.insert (all.end(), L.begin(), L.end()); }
    std::vector<float> tail2 (all.end() - 48000, all.end());
    const float c = bandEnergy (tail2, mtof (60), sr), a2 = bandEnergy (tail2, mtof (57), sr);
    std::printf ("     C %.4f  A %.4f\n", c, a2);
    CHECK (c > a2 * 2, "tangenterna stämmer om resonatorerna");
    e.setHeld (held, 0);

    // molnet: mjukt, inga klick
    p.res = 0.0f; p.cloud = 1.0f; p.density = 0.7f; p.grain = 0.5f; e.setParams (p);
    all.clear();
    for (int k = 0; k < 300; ++k) { e.process (L.data(), R.data(), n, true, ppq, 126.0); ppq += n / sr * 126.0 / 60.0; all.insert (all.end(), L.begin(), L.end()); }
    float rms = 0; for (float v : all) rms += v * v; rms = std::sqrt (rms / all.size());
    CHECK (rms > 0.005f, "molnet hörs");

    // stoppad transport: loopen tystnar mjukt, inga NaN
    for (int k = 0; k < 100; ++k) e.process (L.data(), R.data(), n, false, ppq, 126.0);
    bool fin2 = true; for (float v : L) fin2 &= std::isfinite (v);
    CHECK (fin2, "stopp: inga NaN");

    // SLAG: tangent knackar på resonatorerna utan loop
    Engine e2; e2.prepare (sr); Params p2; p2.res = 1.0f; p2.cloud = 0; p2.room = 0; p2.ping = 1.0f; e2.setParams (p2);
    e2.ping (1.0f);
    float pk = 0; for (int k = 0; k < 40; ++k) { e2.process (L.data(), R.data(), n, false, 0.0, 120.0); for (float v : L) pk = std::max (pk, std::fabs (v)); }
    CHECK (pk > 0.01f, "SLAG: tangenter låter även utan loop");

    std::printf ("\n%s (%d fel)\n", failures ? "MISSLYCKADES" : "ALLT GRÖNT", failures);
    return failures ? 1 : 0;
}
