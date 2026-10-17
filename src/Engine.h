#pragma once
// R-744: hela ljudmotorn. Ren C++ utan JUCE, så den kan testas fristående.
// Kedjan följer CO2-kretsen:
//   FÖRÅNGARE (granulärt moln) -> KOMPRESSOR (vikning, krossning, vikningsdistorsion)
//   -> GASKYLARE (resonant lågpass) -> EXPANSIONSVENTIL (ryckig grind) -> UT
// ÖVERHETTNING trycker allt mot rött. Ligger den i botten i 2 s: SYSTEMFRYS.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace r7
{
constexpr float kPi = 3.14159265358979f;

// Laddat ljud. Bara meddelandetråden byter ut det, ljudtråden läser.
struct Sample
{
    std::vector<float> l, r;
    double sr = 44100.0;
    int len() const { return (int) l.size(); }
};

struct Params
{
    float evap = 0, press = 0, temp = 0, valve = 0, superheat = 0, out = 0.7f;
    bool hold = false;
};

class Rng
{
public:
    float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); }
    float bi() { return uni() * 2.0f - 1.0f; }
    float gauss() { return (uni() + uni() + uni() + uni() - 2.0f) * 1.732f; }
private:
    uint32_t s = 0x9e3779b9u;
};

//==============================================================================
// 1. FÖRÅNGARE. Vid 0 nästan ren uppspelning (Hann 50 % överlapp),
// vid 1 sträckt 16 gånger, utspritt, oktav ner och baklänges.
class Evaporator
{
public:
    static constexpr int kMax = 32;
    static constexpr int kWin = 2048;

    void prepare (double rate)
    {
        sr = rate;
        for (int i = 0; i < kWin; ++i)
            win[i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) (kWin - 1));
        reset();
    }

    void reset() { for (auto& g : gr) g.on = false; head = 0.0; countdown = 0; active = 0; }
    void retrigger() { head = 0.0; countdown = 0; }

    void tick (const Sample& s, float amt, double pitch, int maxActive, float& oL, float& oR)
    {
        const int L = s.len();
        const double srcStep = s.sr / sr * pitch;
        const double stretch = 1.0 + amt * amt * 15.0;
        const double interval = sr / (50.0 + 40.0 * amt);
        const int size = (int) (sr * (0.04 + 0.25 * amt));

        if (--countdown <= 0)
        {
            countdown = std::max (1, (int) (interval * (1.0 + rng.bi() * amt * 0.9)));
            if (active < maxActive) spawn (L, amt, srcStep, size);
        }

        float sl = 0, sr_ = 0;
        active = 0;
        for (auto& g : gr)
        {
            if (! g.on) continue;
            double p = std::fmod (g.pos + g.age * g.rate, (double) L);
            if (p < 0) p += L;
            const int i0 = (int) p;
            const int i1 = i0 + 1 < L ? i0 + 1 : 0;
            const float fr = (float) (p - i0);
            const float w = win[(int) ((int64_t) g.age * (kWin - 1) / g.len)];
            sl  += (s.l[i0] + (s.l[i1] - s.l[i0]) * fr) * w * g.gl;
            sr_ += (s.r[i0] + (s.r[i1] - s.r[i0]) * fr) * w * g.gr;
            if (++g.age >= g.len) g.on = false; else ++active;
        }
        // utspridda korn summeras okorrelerat, därför roten ur överlappet
        const float norm = 1.0f / (float) std::sqrt (std::max (1.0, size / interval * 0.5));
        oL = sl * norm;
        oR = sr_ * norm;
        head = std::fmod (head + srcStep / stretch, (double) L);
    }

    float grainPos (int i, int L) const { return gr[i].on && L > 0 ? (float) (std::fmod (std::fabs (gr[i].pos + gr[i].age * gr[i].rate), (double) L) / L) : -1.0f; }
    double headPos() const { return head; }
    int active = 0;

private:
    struct Grain { double pos = 0, rate = 1; int age = 0, len = 1; float gl = 1, gr = 1; bool on = false; };

    void spawn (int L, float amt, double srcStep, int size)
    {
        for (auto& g : gr)
        {
            if (g.on) continue;
            double rate = srcStep * std::pow (2.0, rng.gauss() * amt * 0.35);
            const float r = rng.uni();
            if (r < amt * 0.25f) rate *= 0.5;            // oktav ner = expansion
            else if (r < amt * 0.4f) rate = -rate;       // baklänges
            const float pan = 0.5f + rng.bi() * 0.5f * amt;
            g.pos = std::fmod (head + rng.bi() * amt * 1.2 * L * 0.25 + L, (double) L);
            g.rate = rate;
            g.age = 0;
            g.len = std::max (16, size);
            g.gl = std::min (1.0f, 2.0f * (1.0f - pan));
            g.gr = std::min (1.0f, 2.0f * pan);
            g.on = true;
            ++active;
            return;
        }
    }

    Grain gr[kMax];
    float win[kWin] {};
    double sr = 44100.0, head = 0.0;
    int countdown = 0;
    Rng rng;
};

//==============================================================================
// 2. KOMPRESSOR. Inte dynamik: vikning -> mättnad -> sample-and-hold (vikningsdistorsion) -> bitkross.
class Compressor
{
public:
    void reset() { hc = 0; hl = hr = 0; }

    void tick (float p, float& l, float& r)
    {
        if (p < 0.005f) return;
        const float drive = 1.0f + 24.0f * p * p;
        const float sat = 1.0f + 3.0f * p;
        auto shape = [&] (float x)
        {
            float y = std::fmod (x * drive - 1.0f, 4.0f);
            if (y < 0) y += 4.0f;
            y = std::fabs (y - 2.0f) - 1.0f;             // triangelvikning
            return std::tanh (y * sat);
        };
        const int k = 1 + (int) (p * p * 30.0f);
        if (++hc >= k) { hc = 0; hl = shape (l); hr = shape (r); }
        float yl = hl, yr = hr;
        if (p > 0.5f)
        {
            const float q = std::exp2 (16.0f - 26.0f * (p - 0.5f));   // 16 -> 3 bitar
            yl = std::round (yl * q) / q;
            yr = std::round (yr * q) / q;
        }
        const float wet = std::min (1.0f, p * 3.0f);
        l += (yl - l) * wet;
        r += (yr - r) * wet;
    }

private:
    int hc = 0;
    float hl = 0, hr = 0;
};

//==============================================================================
// 3. GASKYLARE. Två TPT-SVF i serie = 24 dB/okt. Hög omgivningstemp
// stryper toppen mot 90 Hz och resonansen skriker.
class GasCooler
{
public:
    void prepare (double rate) { sr = (float) rate; reset(); }
    void reset() { for (auto& v : z) v = 0; n = 0; }

    void tick (float t, float& l, float& r)
    {
        if (n-- <= 0)
        {
            n = 15;
            const float fc = std::min (0.45f * sr, 90.0f + 19500.0f * (1.0f - t) * (1.0f - t) * (1.0f - t));
            g = std::tan (kPi * fc / sr);
            k2 = std::max (0.08f, 2.0f - 1.9f * std::pow (t, 1.5f));
        }
        l = stage (stage (l, 1.414f, 0), k2, 2);
        r = stage (stage (r, 1.414f, 4), k2, 6);
        l = std::tanh (l);
        r = std::tanh (r);
    }

private:
    float stage (float x, float k, int s)
    {
        float& ic1 = z[s];
        float& ic2 = z[s + 1];
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    float z[8] {};
    float sr = 44100.0f, g = 0.5f, k2 = 2.0f;
    int n = 0;
};

//==============================================================================
// 4. EXPANSIONSVENTIL. Osynkad slumpgrind: öppna bitar är slag som dör ut, stängda är tystnad.
class Valve
{
public:
    void prepare (double rate) { sr = (float) rate; reset(); }
    void reset() { left = 0; open = true; att = 0; dec = 1; }

    float tick (float v)
    {
        if (v < 0.005f) return 1.0f;
        if (--left <= 0)
        {
            open = rng.uni() < 0.75f - 0.35f * v;
            const float ms = -std::log (std::max (1e-6f, rng.uni())) * (220.0f - 190.0f * v) + 8.0f;
            left = std::max (16, (int) (ms * sr * 0.001f));
            const float decay = sr * (0.02f + 0.5f * (1.0f - v) * (1.0f - v)) * (0.5f + rng.uni());
            dk = std::exp (-1.0f / decay);
            att = 0;
            dec = 1;
        }
        float env = 0;
        if (open)
        {
            att += (1.0f - att) * 0.03f;               // cirka 0,7 ms attack, ger ett klick
            dec *= dk;
            env = att * dec;
        }
        const float d = std::min (1.0f, v * 1.5f);
        return 1.0f - d + d * env;
    }

private:
    float sr = 44100.0f, att = 0, dec = 1, dk = 0.999f;
    int left = 0;
    bool open = true;
    Rng rng;
};

//==============================================================================
class Engine
{
public:
    enum State { Run = 0, Relief = 1, Dead = 2 };

    void prepare (double rate)
    {
        sr = rate;
        evap.prepare (rate);
        cool.prepare (rate);
        valve.prepare (rate);
        fullReset();
    }

    void noteOn (int note) { pitch = std::pow (2.0, (note - 60) / 12.0); gate = true; evap.retrigger(); }
    void noteOff() { gate = false; }
    void allOff() { gate = false; }
    void requestReset() { wantReset.store (true); }

    // Fyller l och r. Gör aldrig något farligt: ingen minnesallokering, inga lås.
    void process (float* l, float* r, int n, const Sample* s, const Params& p)
    {
        if (wantReset.exchange (false)) fullReset();

        // SYSTEMFRYS: övervakar råvärdet, inte det utjämnade
        if (state == Run)
        {
            if (p.superheat >= 0.995f)
            {
                holdCount += n;
                if (holdCount >= (int) (2.0 * sr)) { state = Relief; noiseLeft = (int) (3.0 * sr); holdCount = 0; }
            }
            else holdCount = 0;
        }
        uiHold.store (state == Run ? (float) holdCount / (float) (2.0 * sr) : 0.0f);

        if (state != Run) { relief (l, r, n); publish (s); return; }
        if (safe || s == nullptr || s->len() < 64) { clear (l, r, n); publish (s); return; }

        const bool on = gate || p.hold;
        for (int i = 0; i < n; ++i)
        {
            sm (smEvap, p.evap); sm (smPress, p.press); sm (smTemp, p.temp);
            sm (smValve, p.valve); sm (smHeat, p.superheat); sm (smOut, p.out);
            amp += ((on ? 1.0f : 0.0f) - amp) * (on ? 0.006f : 0.0004f);
            if (amp < 1e-5f && ! on) { l[i] = r[i] = 0; continue; }

            float a, b;
            evap.tick (*s, eff (smEvap), pitch, maxActive, a, b);
            comp.tick (eff (smPress), a, b);
            cool.tick (eff (smTemp), a, b);
            const float g = valve.tick (eff (smValve)) * amp * smOut;
            l[i] = std::tanh (a * 1.2f) * g;
            r[i] = std::tanh (b * 1.2f) * g;
        }

        // vakt mot NaN/inf: nollställ, och efter många fel i rad, stäng av tills RESET
        bool bad = false;
        for (int i = 0; i < n; ++i) if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) { bad = true; break; }
        if (bad)
        {
            clear (l, r, n);
            evap.reset(); comp.reset(); cool.reset(); valve.reset();
            errors.fetch_add (1);
            if (++errRun >= 20) safe = true;
        }
        else errRun = 0;
        publish (s);
    }

    // CPU-vakt: anropas med blockets last (0..1+). Färre korn när datorn inte hinner.
    void reportLoad (float load)
    {
        cpu += (load - cpu) * 0.1f;
        if (load > 0.6f) ++over; else if (over > 0) --over;
        if (over > 6 && maxActive > 6) { maxActive -= 4; over = 0; }
        else if (cpu < 0.2f && maxActive < Evaporator::kMax && ++calm > 200) { ++maxActive; calm = 0; }
        uiCpu.store (cpu);
        uiMax.store (maxActive);
    }

    void markError() { errors.fetch_add (1); if (++errRun >= 20) safe = true; }

    // Telemetri till fönstret
    std::atomic<int> uiState { Run }, uiActive { 0 }, uiMax { Evaporator::kMax }, errors { 0 };
    std::atomic<float> uiHold { 0 }, uiHead { 0 }, uiCpu { 0 };
    std::atomic<bool> uiSafe { false };
    std::atomic<float> uiGrain[Evaporator::kMax] {};

private:
    float eff (float v) const { return std::min (1.0f, v + (1.0f - v) * smHeat); }   // ÖVERHETTNING-makrot
    static void sm (float& s, float t) { s += (t - s) * 0.002f; }
    static void clear (float* l, float* r, int n) { std::fill (l, l + n, 0.0f); std::fill (r, r + n, 0.0f); }

    void relief (float* l, float* r, int n)
    {
        const int total = (int) (3.0 * sr);
        const float fade = (float) (0.4 * sr);
        for (int i = 0; i < n; ++i)
        {
            if (noiseLeft <= 0) { l[i] = r[i] = 0; continue; }
            const float t = (float) (total - noiseLeft);
            const float env = std::min (1.0f, t / 200.0f) * std::min (1.0f, (float) noiseLeft / fade);
            l[i] = noise.bi() * 0.35f * env;
            r[i] = noise.bi() * 0.35f * env;
            if (--noiseLeft <= 0) state = Dead;
        }
    }

    void fullReset()
    {
        state = Run; holdCount = 0; noiseLeft = 0; safe = false; errRun = 0;
        smHeat = 0; amp = 0;
        maxActive = Evaporator::kMax;
        evap.reset(); comp.reset(); cool.reset(); valve.reset();
    }

    void publish (const Sample* s)
    {
        const int L = s ? s->len() : 0;
        uiState.store (state);
        uiSafe.store (safe);
        uiActive.store (evap.active);
        uiHead.store (L > 0 ? (float) (evap.headPos() / L) : 0.0f);
        for (int i = 0; i < Evaporator::kMax; ++i) uiGrain[i].store (evap.grainPos (i, L));
    }

    Evaporator evap;
    Compressor comp;
    GasCooler cool;
    Valve valve;
    Rng noise;

    double sr = 44100.0, pitch = 1.0;
    bool gate = false, safe = false;
    int state = Run, holdCount = 0, noiseLeft = 0, errRun = 0, maxActive = Evaporator::kMax, over = 0, calm = 0;
    float smEvap = 0, smPress = 0, smTemp = 0, smValve = 0, smHeat = 0, smOut = 0.7f, amp = 0, cpu = 0;
    std::atomic<bool> wantReset { false };
};
} // namespace r7
