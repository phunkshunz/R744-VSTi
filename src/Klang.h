#pragma once
// R-744 KLANGMASKIN — ljudmotorn. Ren C++ utan JUCE, så den kan testas fristående.
//
// En loop spelas i takt med DAW:en. Den går sedan genom:
//   RESONANS  åtta stämda resonatorer i ett ackord: varje slag i loopen ringer som ackordet
//   MOLN      korn ur loopen (Hann-fönster, aldrig klick) som sträcks till mattor och stäms i ackordet
//   ANDNING   ett filter som andas med fraserna, och band-wow
//   RUM       ett stort, mjukt rum
// Håller man tangenter stäms resonatorerna och molnet om till just de tonerna.
// Allt glider: tonbyten, nivåer och filter. Ingen hård klippning, bara mjuk mättning på slutet.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace r744
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float mtof (float n) noexcept { return 440.0f * std::pow (2.0f, (n - 69.0f) / 12.0f); }
inline float smooth (float cur, float target, float coef) noexcept { return target + (cur - target) * coef; }

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 1) { s = seed ? seed * 2654435761u + 1u : 1u; }
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
};

// Zero-delay SVF (Simper)
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.4f;
    void set (float fc, float q, float sr) noexcept
    {
        fc = std::clamp (fc, 10.0f, sr * 0.45f);
        const float g = std::tan (kPi * fc / sr);
        k = 1.0f / std::max (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void tick (float v0, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    float lp (float x) noexcept { float l, b, h; tick (x, l, b, h); return l; }
    float hp (float x) noexcept { float l, b, h; tick (x, l, b, h); return h; }
    void reset() noexcept { ic1 = ic2 = 0; }
};

struct Sample
{
    std::vector<float> L, R;
    double rate = 48000.0;
    int beats = 16;          // hur många taktslag loopen är
    int length() const noexcept { return (int) L.size(); }
};

// Hur många taktslag är loopen? Den längd i 4/8/16/32/64 slag som ger ett rimligt tempo (80–170 BPM).
inline int guessBeats (double seconds) noexcept
{
    int best = 16; double bestErr = 1e9;
    for (int b : { 4, 8, 16, 32, 64 })
    {
        const double bpm = b * 60.0 / std::max (0.01, seconds);
        const double err = bpm < 80 ? 80 - bpm : (bpm > 170 ? bpm - 170 : std::fabs (bpm - 126.0) * 0.1);
        if (err < bestErr) { bestErr = err; best = b; }
    }
    return best;
}

// Ackord (halvtoner från grundtonen). Resonatorerna fördelas över dem och över oktaver.
constexpr int kNumChords = 10;
inline const char* chordName (int i) noexcept
{
    static const char* n[kNumChords] = { "MOLL", "MOLL 7", "MOLL 9", "SUS 4", "KVINT", "OKTAV", "DUR 7", "DORISK", "MÖRK KLUSTER", "TRITONUS" };
    return n[std::clamp (i, 0, kNumChords - 1)];
}
inline int chordNotes (int i, int* out) noexcept
{
    static const int c[kNumChords][5] = { { 0, 3, 7, -1, -1 }, { 0, 3, 7, 10, -1 }, { 0, 3, 7, 10, 14 }, { 0, 5, 7, 10, -1 },
                                          { 0, 7, -1, -1, -1 }, { 0, 12, -1, -1, -1 }, { 0, 4, 7, 11, -1 }, { 0, 3, 7, 9, 14 },
                                          { 0, 1, 3, 7, -1 }, { 0, 6, 7, 13, -1 } };
    int n = 0;
    for (int k = 0; k < 5; ++k) if (c[std::clamp (i, 0, kNumChords - 1)][k] >= 0) out[n++] = c[std::clamp (i, 0, kNumChords - 1)][k];
    return n;
}

struct Params
{
    float loop = 0.6f;        // LOOP: hur mycket av originalet som hörs
    float res = 0.7f;         // RESONANS nivå
    float ring = 0.45f;       // RINGNING 0..1 -> 0,15..6 s
    float bright = 0.5f;      // LJUS: hur mycket diskant som driver resonatorerna
    int chord = 1;            // ackord
    int root = 9;             // grundton 0..11 (A)
    int octave = 0;           // -1..+1
    float spread = 0.5f;      // SPRIDNING över oktaver
    float cloud = 0.4f;       // MOLN nivå
    float grain = 0.5f;       // KORN storlek 0..1 -> 40..500 ms
    float density = 0.5f;     // TÄTHET korn per sekund
    float wander = 0.3f;      // VANDRA: hur fort molnet glider genom loopen
    float tune = 0.7f;        // STÄMNING: andel korn som stäms i ackordet
    bool freeze = false;      // FRYS: molnet står still
    float breath = 0.4f;      // ANDNING
    int phraseBars = 4;       // fraslängd för andningen
    float wow = 0.2f;         // BAND
    float room = 0.35f;       // RUM nivå
    float size = 0.6f;        // RUM storlek
    float outDb = 0.0f;
    float ping = 0.5f;        // SLAG: tangenter knackar på resonatorerna även utan loop
};

class Engine
{
public:
    static constexpr int kRes = 8, kGrains = 24;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        hpIn.set (70.0f, 0.7f, sr);
        agcCoef = 1.0f - std::exp (-1.0f / (3.0f * sr));
        for (int i = 0; i < 4; ++i) { const size_t len = (size_t) (sr * 0.25f) + 8; fdn[i].assign (len, 0.0f); }
        wowL.assign ((size_t) (sr * 0.02f) + 8, 0.0f); wowR.assign (wowL.size(), 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& r : res) { r.y1[0] = r.y1[1] = r.y2[0] = r.y2[1] = 0.0f; }
        for (auto& g : grains) g.active = false;
        for (auto& f : fdn) std::fill (f.begin(), f.end(), 0.0f);
        std::fill (wowL.begin(), wowL.end(), 0.0f); std::fill (wowR.begin(), wowR.end(), 0.0f);
        fdnW = 0; wowW = 0; hpIn.reset(); brL.reset(); brR.reset();
        for (auto& d : damp) d = 0.0f;
        grainClock = 0.0f; pingEnv = 0.0f; scan = 0.0;
        lvlLoop = lvlRes = lvlCloud = lvlRoom = 0.0f; outGain = 1.0f;
    }

    // Message-tråden byter sample via pekare som audio-tråden läser. Ägs av processorn.
    void setSample (const Sample* s) noexcept { sample = s; }
    void setParams (const Params& p) noexcept { prm = p; }

    // Hållna tangenter (MIDI-noter). Tomt = ackordet från rattarna.
    void setHeld (const int* notes, int n) noexcept
    {
        numHeld = std::clamp (n, 0, 8);
        for (int i = 0; i < numHeld; ++i) held[i] = notes[i];
    }
    void ping (float vel) noexcept { pingEnv = std::max (pingEnv, vel); }

    // ppq = host-position, playing = transporten går
    void process (float* outL, float* outR, int n, bool playing, double ppq, double bpm)
    {
        retune();
        const float c = std::exp (-1.0f / (0.03f * sr)); // 30 ms glidning för nivåer
        const double ppqPerSample = bpm / 60.0 / sr;
        const Sample* s = sample;
        const int len = s ? s->length() : 0;
        const double loopSamplesPerBeat = s && s->beats > 0 ? (double) len / s->beats : 0.0;

        // RINGNING
        const float ringSec = 0.15f * std::pow (40.0f, prm.ring);
        const float r = std::exp (-6.9f / (ringSec * sr));
        for (auto& x : res) x.r = r;
        const float resNorm = (1.0f - r) * 3.0f + 0.002f;

        // ANDNING-filter
        const float roomFb = 0.55f + 0.4f * prm.size;
        const float gainTarget = std::pow (10.0f, prm.outDb / 20.0f);

        for (int i = 0; i < n; ++i)
        {
            const double q = ppq + i * ppqPerSample;
            lvlLoop = smooth (lvlLoop, playing && s ? prm.loop : 0.0f, c);
            lvlRes = smooth (lvlRes, prm.res, c);
            lvlCloud = smooth (lvlCloud, prm.cloud, c);
            lvlRoom = smooth (lvlRoom, prm.room, c);
            outGain = smooth (outGain, gainTarget, c);

            // ---- loopen, i takt (spelar bara när transporten går)
            float lL = 0.0f, lR = 0.0f;
            double posSamples = 0.0;
            if (s && len > 4 && playing)
            {
                const double beatsPos = std::fmod (std::max (0.0, q), (double) s->beats);
                posSamples = beatsPos * loopSamplesPerBeat;
                read (*s, posSamples, lL, lR);
                fadeIn = std::min (1.0f, fadeIn + 1.0f / (0.01f * sr));
            }
            else fadeIn = std::max (0.0f, fadeIn - 1.0f / (0.01f * sr));
            lL *= fadeIn; lR *= fadeIn;

            // ---- excitation till resonatorerna: loopen (utan botten) + SLAG från tangenterna
            float ex = hpIn.hp ((lL + lR) * 0.5f);
            float brl, brb, brh; tilt.tick (ex, brl, brb, brh);
            ex = lerp (brl, brh, prm.bright) + brb * 0.5f;
            if (pingEnv > 1.0e-4f) { ex += pingEnv * (rng.uni() * 2.0f - 1.0f) * 0.35f * prm.ping; pingEnv *= 0.995f; }

            // ---- RESONANS
            float rl = 0.0f, rr = 0.0f;
            for (int k = 0; k < kRes; ++k)
            {
                auto& x = res[k];
                if ((i & 31) == 0)
                {
                    x.hz = smooth (x.hz, x.targetHz, 0.6f);
                    const float w = kTwoPi * std::min (x.hz, sr * 0.45f) / sr;
                    x.a1 = 2.0f * x.r * std::cos (w); x.a2 = x.r * x.r;
                }
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float y = resNorm * ex + x.a1 * x.y1[ch] - x.a2 * x.y2[ch];
                    x.y2[ch] = x.y1[ch]; x.y1[ch] = y;
                }
                const float v = (x.y1[0] + x.y1[1]) * 0.5f * x.gain;
                rl += v * (1.0f - x.pan); rr += v * (1.0f + x.pan);
            }
            rl = std::tanh (rl * 0.6f) * lvlRes; rr = std::tanh (rr * 0.6f) * lvlRes;

            // ---- MOLN
            float cl = 0.0f, cr = 0.0f;
            if (s && len > 4 && lvlCloud > 0.001f)
            {
                if (! prm.freeze)
                {
                    // scanpositionen följer loopen men släpar och vandrar
                    const double target = playing ? posSamples : scan;
                    const double drift = (rng.uni() - 0.5f) * prm.wander * 0.002 * len / sr * 48000.0;
                    scan = playing ? target - prm.wander * 0.25 * len * (0.5 + 0.5 * std::sin (q * 0.19)) + drift : scan;
                    while (scan < 0) scan += len;
                    while (scan >= len) scan -= len;
                }
                const float perSec = 4.0f + 60.0f * prm.density * prm.density;
                grainClock += perSec / sr;
                if (grainClock >= 1.0f) { grainClock -= 1.0f; spawnGrain (*s); }
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    const float t = (float) g.age / (float) g.len;
                    const float w = 0.5f - 0.5f * std::cos (kTwoPi * t); // Hann
                    float gl, gr; read (*s, g.pos, gl, gr);
                    cl += gl * w * g.gain * (1.0f - g.pan); cr += gr * w * g.gain * (1.0f + g.pan);
                    g.pos += g.rate; if (g.pos >= len) g.pos -= len;
                    if (++g.age >= g.len) g.active = false;
                }
                cl *= lvlCloud; cr *= lvlCloud;
            }

            // ---- ANDNING: filter som öppnar och stänger med fraserna + band-wow på klangen
            float wl = rl + cl, wr = rr + cr;
            if (prm.breath > 0.001f)
            {
                const double ph = q / (4.0 * std::max (1, prm.phraseBars));
                const float b = 0.5f - 0.5f * std::cos (kTwoPi * (float) (ph - std::floor (ph)));
                if ((i & 15) == 0)
                {
                    const float fc = 18000.0f * std::pow (2.0f, -prm.breath * 5.5f * (1.0f - b));
                    brL.set (fc, 0.9f, sr); brR.set (fc, 0.9f, sr);
                }
                wl = brL.lp (wl); wr = brR.lp (wr);
            }
            if (prm.wow > 0.001f && ! wowL.empty())
            {
                wowPh += 0.4f / sr; if (wowPh >= 1.0f) wowPh -= 1.0f;
                flPh += 6.1f / sr; if (flPh >= 1.0f) flPh -= 1.0f;
                const int wl2 = (int) wowL.size();
                wowL[(size_t) wowW] = wl; wowR[(size_t) wowW] = wr;
                const float d = (0.002f + prm.wow * 0.004f * (0.5f + 0.4f * std::sin (kTwoPi * wowPh) + 0.1f * std::sin (kTwoPi * flPh))) * sr;
                float rp = (float) wowW - d; while (rp < 0) rp += (float) wl2;
                const int r0 = (int) rp, r1 = (r0 + 1) % wl2; const float fr = rp - (float) r0;
                wl = wowL[(size_t) r0] + (wowL[(size_t) r1] - wowL[(size_t) r0]) * fr;
                wr = wowR[(size_t) r0] + (wowR[(size_t) r1] - wowR[(size_t) r0]) * fr;
                if (++wowW >= wl2) wowW = 0;
            }

            // ---- RUM: 4-linjers FDN med mjuk dämpning
            float roomL = 0.0f, roomR = 0.0f;
            if (lvlRoom > 0.001f)
            {
                static const float ms[4] = { 41.1f, 53.7f, 67.9f, 83.3f };
                float o[4];
                for (int k = 0; k < 4; ++k)
                {
                    const int L = (int) fdn[k].size();
                    const int d = std::min (L - 2, (int) (sr * ms[k] * 0.001f * (0.5f + 1.5f * prm.size)));
                    int rp = fdnW - d; while (rp < 0) rp += L;
                    o[k] = fdn[k][(size_t) (rp % L)];
                }
                const float sum = 0.5f * (o[0] + o[1] + o[2] + o[3]);
                const float in = (wl + wr) * 0.25f + (lL + lR) * 0.05f;
                for (int k = 0; k < 4; ++k)
                {
                    damp[k] += 0.35f * ((o[k] - sum) - damp[k]);
                    const int L = (int) fdn[k].size();
                    fdn[k][(size_t) (fdnW % L)] = in + damp[k] * roomFb;
                }
                ++fdnW; if (fdnW > (1 << 30)) fdnW = 0;
                roomL = (o[0] + o[2]) * 0.6f * lvlRoom; roomR = (o[1] + o[3]) * 0.6f * lvlRoom;
            }

            float L = lL * lvlLoop + wl + roomL;
            float R = lR * lvlLoop + wr + roomR;
            // automatisk nivå: långsam (ca 3 s), högst ±9 dB, så alla lägen landar ungefär lika starkt
            msLevel += (0.5f * (L * L + R * R) - msLevel) * agcCoef;
            if ((i & 63) == 0)
            {
                const float rmsNow = std::sqrt (msLevel);
                const float want = rmsNow > 1.0e-4f ? std::clamp (0.18f / rmsNow, 0.35f, 2.8f) : agc;
                agc += (want - agc) * 0.02f;
            }
            L *= agc; R *= agc;
            // mjuk mättning, aldrig hård klippning
            L = std::tanh (L * outGain * 0.9f) * 0.95f;
            R = std::tanh (R * outGain * 0.9f) * 0.95f;
            if (! std::isfinite (L) || ! std::isfinite (R)) { reset(); L = R = 0.0f; }
            outL[i] = L; outR[i] = R;
            peakMeter = std::max (peakMeter * 0.9995f, std::max (std::fabs (L), std::fabs (R)));
        }
        scanShown = len > 0 ? (float) (scan / len) : 0.0f;
    }

    float scanShown = 0.0f, peakMeter = 0.0f;
    // Tonerna resonatorerna står på just nu (för panelen)
    int currentNotes (int* out) const noexcept { for (int k = 0; k < kRes; ++k) out[k] = res[k].note; return kRes; }

private:
    struct Res { float hz = 220, targetHz = 220, r = 0.999f, a1 = 0, a2 = 0, gain = 1, pan = 0; float y1[2] {}, y2[2] {}; int note = 57; };
    struct Grain { bool active = false; double pos = 0, rate = 1; int age = 0, len = 1; float gain = 1, pan = 0; };

    float sr = 48000.0f;
    Params prm;
    const Sample* sample = nullptr;
    Res res[kRes];
    Grain grains[kGrains];
    Svf hpIn, tilt, brL, brR;
    std::array<std::vector<float>, 4> fdn;
    float damp[4] {};
    int fdnW = 0;
    std::vector<float> wowL, wowR;
    int wowW = 0;
    float wowPh = 0.0f, flPh = 0.0f, fadeIn = 0.0f;
    int held[8] {}; int numHeld = 0;
    float pingEnv = 0.0f, grainClock = 0.0f;
    double scan = 0.0;
    float lvlLoop = 0, lvlRes = 0, lvlCloud = 0, lvlRoom = 0, outGain = 1;
    float msLevel = 0.0f, agc = 1.0f, agcCoef = 0.0001f;
    Rng rng { 744 };
    int lastTuneKey = -1;

    void read (const Sample& s, double pos, float& l, float& r) const noexcept
    {
        const int len = s.length();
        int i0 = (int) pos; const float f = (float) (pos - i0);
        i0 = ((i0 % len) + len) % len; const int i1 = (i0 + 1) % len;
        l = s.L[(size_t) i0] + (s.L[(size_t) i1] - s.L[(size_t) i0]) * f;
        const auto& RR = s.R.empty() ? s.L : s.R;
        r = RR[(size_t) i0] + (RR[(size_t) i1] - RR[(size_t) i0]) * f;
    }

    // Vilka toner står resonatorerna på? Hållna tangenter om några, annars ackordet.
    void chordTones (int* notes, int& n) const noexcept
    {
        if (numHeld > 0) { n = numHeld; for (int i = 0; i < n; ++i) notes[i] = held[i]; return; }
        int iv[5]; const int m = chordNotes (prm.chord, iv);
        const int base = 48 + prm.root + 12 * prm.octave; // C3 + grundton
        n = m; for (int i = 0; i < m; ++i) notes[i] = base + iv[i];
    }

    void retune() noexcept
    {
        int notes[8]; int n = 0; chordTones (notes, n);
        if (n <= 0) return;
        tilt.set (lerp (600.0f, 5000.0f, prm.bright), 0.7f, sr);
        for (int k = 0; k < kRes; ++k)
        {
            // fördela resonatorerna över ackordets toner och över oktaver (SPRIDNING)
            const int tone = notes[k % n];
            const int layer = k / n;
            int oct = 0;
            if (layer > 0) oct = (layer % 2 == 1 ? 1 : -1) * ((layer + 1) / 2);
            if (prm.spread < 0.33f) oct = 0; else if (prm.spread < 0.66f) oct = std::clamp (oct, -1, 1);
            const int note = std::clamp (tone + 12 * oct, 24, 108);
            res[k].note = note;
            res[k].targetHz = mtof ((float) note);
            res[k].gain = (layer == 0 ? 1.0f : 0.6f) * (note < 40 ? 0.7f : 1.0f);
            res[k].pan = std::sin ((float) k * 2.399f) * 0.6f;
        }
    }

    void spawnGrain (const Sample& s) noexcept
    {
        for (auto& g : grains)
        {
            if (g.active) continue;
            const int len = s.length();
            g.active = true;
            g.age = 0;
            g.len = std::max (64, (int) (sr * (0.04f + 0.46f * prm.grain * prm.grain) * (0.75f + 0.5f * rng.uni())));
            g.pos = scan + (rng.uni() - 0.5f) * 0.05f * len;
            float semis = 0.0f;
            if (rng.uni() < prm.tune)
            {
                int notes[8]; int n = 0; chordTones (notes, n);
                const int base = notes[0];
                const int pick = notes[(int) (rng.uni() * n) % std::max (1, n)];
                semis = (float) (pick - base) + (rng.uni() < 0.25f ? 12.0f : 0.0f) - (rng.uni() < 0.15f ? 12.0f : 0.0f);
            }
            g.rate = std::pow (2.0, semis / 12.0) * s.rate / sr;
            g.gain = 0.35f / std::sqrt (1.0f + 0.15f * (4.0f + 60.0f * prm.density * prm.density) * (float) g.len / sr);
            g.pan = rng.uni() * 1.4f - 0.7f;
            return;
        }
    }
};

} // namespace r744
