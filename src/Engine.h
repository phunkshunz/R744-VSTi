#pragma once
// R-744 v2: hela ljudmotorn. Ren C++ utan JUCE, så den kan testas fristående.
//
// Kretsen:
//   VENTILSEKVENSERN (16 steg, polymeter, swing, drift) startar röster som spelar slices
//   -> FÖRÅNGARE (varje röst kan sträckas till ett granulärt moln)
//   -> BAS-LÅS (basen går förbi resten)
//   -> KOMPRESSOR (tryck, vikning, kross)
//   -> + CIRKULATION (det som kommit ut går runt igen, i takt)
//   -> GASKYLARE (LP/BP/HP, temp = cutoff, PENDLING = jagande ventil-LFO)
//   -> UT, och tillbaka in i cirkulationen
// TRYCKFALL ger glitchar per steg. ÖVERHETTNING trycker allt mot rött.
// I botten i 2 s: ISBILDNING. Säkerhetsventilen väser, kretsen fryser till en isig drönare tills RESET.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace r7
{
constexpr float kPi = 3.14159265358979f;
constexpr int kSteps = 16;
constexpr int kVoices = 8;
constexpr int kGrains = 4;

// tabeller som parametrarna pekar in i
constexpr double kRateBeats[] = { 0.125, 0.25, 1.0 / 6.0, 0.5, 1.0 / 3.0, 1.0 };      // 1/32 1/16 1/16T 1/8 1/8T 1/4
constexpr double kEchoBeats[] = { 0.25, 0.5, 0.75, 1.0, 1.5 };                        // 1/16 1/8 3/16 1/4 3/8
constexpr double kHuntBeats[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };              // 1/16 .. 4 takter
constexpr int kSliceCounts[] = { 4, 8, 16, 32 };

struct Sample
{
    std::vector<float> l, r;
    double sr = 44100.0;
    int len() const { return (int) l.size(); }
};

struct Step { uint8_t on = 0, slice = 0, accent = 0, ratchet = 1, rev = 0; };
struct Pattern { Step s[kSteps]; };

struct Params
{
    float evap = 0, grain = 0.4f, spread = 0, gate = 0.4f;
    float press = 0, fold = 0, crush = 0, bass = 0;
    float temp = 0, reso = 0.3f, hunt = 0;
    int ftype = 0, huntRate = 3;
    float valve = 0, circ = 0;
    int echo = 2;
    float swing = 0, drift = 0;
    int steps = 16, rate = 1, slices = 2, pitch = 0;
    float superheat = 0, out = 0.7f;
    bool hold = false;
};

struct Clock { double bpm = 128.0, ppq = 0.0; bool playing = false, freeRun = true; };   // freeRun: fristående app

class Rng
{
public:
    float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); }
    float bi() { return uni() * 2.0f - 1.0f; }
    int pick (int n) { return std::min (n - 1, (int) (uni() * (float) n)); }
private:
    uint32_t s = 0x9e3779b9u;
};

//==============================================================================
// En röst = en slice som spelas. Med EXPANSION > 0 blir den ett sträckt kornmoln.
struct Voice
{
    struct Grain { double pos = 0, rate = 1; int age = 0, len = 1; float gl = 1, gr = 1; bool on = false; };

    bool on = false;
    double pos = 0, rate = 1;
    float amp = 0, decay = 0.999f, gain = 1;
    int age = 0, countdown = 0;
    Grain g[kGrains];

    void start (double p, double r, float dec, float gn)
    {
        on = true; pos = p; rate = r; decay = dec; gain = gn; amp = 1.0f; age = 0; countdown = 0;
        for (auto& x : g) x.on = false;
    }

    static float read (const std::vector<float>& v, double p, int L)
    {
        p = std::fmod (p, (double) L);
        if (p < 0) p += L;
        const int i0 = (int) p;
        const int i1 = i0 + 1 < L ? i0 + 1 : 0;
        const float f = (float) (p - i0);
        return v[(size_t) i0] + (v[(size_t) i1] - v[(size_t) i0]) * f;
    }

    void tick (const Sample& s, const float* win, int winN, float evap, int grainLen, float spread,
               double sliceLen, int maxG, Rng& rng, float& oL, float& oR)
    {
        const int L = s.len();
        const float env = std::min (1.0f, (float) age / 48.0f) * amp * gain;
        ++age;
        amp *= decay;
        if (amp < 1e-4f) { on = false; return; }

        if (evap < 0.01f && spread < 0.01f)            // ren uppspelning, skarpa slag
        {
            oL += read (s.l, pos, L) * env;
            oR += read (s.r, pos, L) * env;
            pos += rate;
            return;
        }

        const double stretch = 1.0 + evap * evap * 15.0;
        if (--countdown <= 0)
        {
            countdown = std::max (8, (int) (grainLen * 0.5f * (1.0f + rng.bi() * spread * 0.6f)));
            for (int i = 0; i < maxG; ++i)
            {
                auto& x = g[i];
                if (x.on) continue;
                const float pan = 0.5f + rng.bi() * 0.5f * spread;
                x.pos = pos + rng.bi() * spread * sliceLen;
                x.rate = rate * (rng.uni() < spread * 0.15f ? 0.5 : 1.0);
                x.age = 0;
                x.len = std::max (32, grainLen);
                x.gl = std::min (1.0f, 2.0f * (1.0f - pan));
                x.gr = std::min (1.0f, 2.0f * pan);
                x.on = true;
                break;
            }
        }
        float sl = 0, sr = 0;
        for (int i = 0; i < kGrains; ++i)
        {
            auto& x = g[i];
            if (! x.on) continue;
            const double p = x.pos + x.age * x.rate;
            const float w = win[(int) ((int64_t) x.age * (winN - 1) / x.len)];
            sl += read (s.l, p, L) * w * x.gl;
            sr += read (s.r, p, L) * w * x.gr;
            if (++x.age >= x.len) x.on = false;
        }
        oL += sl * env;
        oR += sr * env;
        pos += rate / stretch;
    }
};

//==============================================================================
// TPT-SVF, ger LP, BP och HP på en gång
struct Svf
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    inline void tick (float x, float g, float k, float& lp, float& bp, float& hp)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

//==============================================================================
class Engine
{
public:
    enum State { Run = 0, Relief = 1, Ice = 2 };

    void prepare (double rate)
    {
        sr = rate;
        for (int i = 0; i < kWin; ++i)
            win[i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) (kWin - 1));
        dlen = (int) (sr * 3.0) + 1;
        levelA = 1.0f - std::exp (-1.0f / (0.3f * (float) sr));
        agcA = 1.0f - std::exp (-1.0f / (1.5f * (float) sr));
        dl.assign ((size_t) dlen, 0.0f);
        dr.assign ((size_t) dlen, 0.0f);
        fullReset();
    }

    void setPattern (const Pattern& p) { pat = p; }
    void noteOn (int note) { pitchNote = note - 60; if (! noteHeld) freePpq = -1.0; noteHeld = true; heldCount++; }
    void noteOff() { if (--heldCount <= 0) { heldCount = 0; noteHeld = false; } }
    void allOff() { heldCount = 0; noteHeld = false; }
    void requestReset() { wantReset.store (true); }

    void process (float* l, float* r, int n, const Sample* s, const Params& p, const Clock& c)
    {
        if (wantReset.exchange (false)) fullReset();

        // ISBILDNING: övervakar råvärdet på ÖVERHETTNING
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
        if (state != Run) { frozen (l, r, n); publish (s, p); return; }
        if (safe) { clear (l, r, n); publish (s, p); return; }

        const bool hostRun = c.playing;
        const bool running = (p.hold && (hostRun || c.freeRun)) || noteHeld;
        const double bpm = std::clamp (c.bpm > 1.0 ? c.bpm : 128.0, 30.0, 300.0);
        const double beatsPerSample = bpm / 60.0 / sr;
        const double stepBeats = kRateBeats[std::clamp (p.rate, 0, 5)];
        const double stepSamples = stepBeats / beatsPerSample;
        const int steps = std::clamp (p.steps, 1, kSteps);
        const int L = s ? s->len() : 0;
        const int nSlices = kSliceCounts[std::clamp (p.slices, 0, 3)];
        const double sliceLen = L > 0 ? (double) L / nSlices : 1.0;
        const double srcRatio = s ? s->sr / sr : 1.0;
        const int grainLen = (int) (sr * (0.012 + 0.3 * p.grain * p.grain));
        const double huntBeats = kHuntBeats[std::clamp (p.huntRate, 0, 6)];
        const int echoSamples = std::clamp ((int) (kEchoBeats[std::clamp (p.echo, 0, 4)] / beatsPerSample), 1, dlen - 2);

        if (! running) freePpq = -1.0;
        double ppq = hostRun ? c.ppq : std::max (0.0, freePpq);

        for (int i = 0; i < n; ++i)
        {
            // --- klocka och steg
            if (running)
            {
                if (! hostRun && freePpq < 0.0) { freePpq = 0.0; ppq = 0.0; lastAbs = -1; }
                const int64_t absStep = (int64_t) std::floor (ppq / stepBeats + 1e-9);
                if (absStep != lastAbs)
                {
                    lastAbs = absStep;
                    triggerStep (absStep, steps, stepSamples, p);
                }
            }
            else lastAbs = INT64_MIN / 2;

            // --- utjämnade rattar (ÖVERHETTNING-makrot via eff)
            sm (smEvap, p.evap); sm (smSpread, p.spread); sm (smPress, p.press); sm (smFold, p.fold);
            sm (smCrush, p.crush); sm (smTemp, p.temp); sm (smReso, p.reso); sm (smHunt, p.hunt);
            sm (smCirc, p.circ); sm (smHeat, p.superheat); sm (smOut, p.out); sm (smBass, p.bass);

            // --- PENDLING: jagande ventil, en sinus som aldrig riktigt hittar hem
            const double ph = ppq / huntBeats;
            const int64_t cyc = (int64_t) std::floor (ph);
            if (cyc != huntCycle) { huntCycle = cyc; huntTarget = huntRng.bi(); }
            huntWalk += (huntTarget - huntWalk) * 0.0005f;
            const float lfo = 0.7f * std::sin (2.0f * kPi * (float) (ph - (double) cyc)) + 0.3f * huntWalk;
            const float hunt = smHunt * lfo;

            // --- schemalagda steg (swing, drift, ratchet)
            for (auto& t : pend)
                if (t.on && --t.wait <= 0) { t.on = false; startVoice (t, s, sliceLen, srcRatio, stepSamples, p); }

            // --- röster
            float vl = 0, vr = 0;
            if (s && L > 64)
            {
                const float ev = eff (smEvap), spr = std::clamp (eff (smSpread) * 0.6f + std::fabs (hunt) * 0.2f, 0.0f, 1.0f);
                for (int v = 0; v < maxVoices; ++v)
                    if (voices[v].on)
                        voices[v].tick (*s, win, kWin, ev, grainLen, spr, sliceLen, maxGrains, rng, vl, vr);
            }

            // --- BAS-LÅS
            float lowL = 0, lowR = 0;
            if (smBass > 0.01f)
            {
                if (--bassN <= 0) { bassN = 16; bassG = std::tan (kPi * (40.0f + 210.0f * smBass) / (float) sr); }
                float b, h;
                bassL.tick (vl, bassG, 1.414f, lowL, b, h);
                bassR.tick (vr, bassG, 1.414f, lowR, b, h);
                vl -= lowL; vr -= lowR;
            }

            // --- KOMPRESSOR (på det nya ljudet)
            float xl = vl, xr = vr;
            compress (std::clamp (eff (smPress) + hunt * 0.25f, 0.0f, 1.0f), eff (smFold), eff (smCrush), xl, xr);

            // --- CIRKULATION: det som gått runt kommer tillbaka efter kompressorn,
            // så slingan inte förstärker sig själv till en vägg. Varje varv går genom gaskylaren och dör ut.
            int rd = dw - echoSamples; if (rd < 0) rd += dlen;
            const float fb = std::min (0.92f, eff (smCirc) * 0.92f) * (smCirc > 0.002f || smHeat > 0.002f ? 1.0f : 0.0f);
            xl += dl[(size_t) rd] * fb;
            xr += dr[(size_t) rd] * fb;

            // --- GASKYLARE. ÖVERHETTNING får filtret att skrika, inte att strypas till tystnad.
            const float tEff = smTemp + std::max (0.0f, 0.62f - smTemp) * smHeat;
            const float rEff = std::min (1.0f, smReso + smHeat * 0.55f);
            cooler (tEff, rEff, hunt, p.ftype, xl, xr);

            // --- DC-spärr, sen tillbaka in i kretsen. Resonansen dämpas i returen så den inte självsvänger.
            dcL = xl - dcXL + 0.995f * dcL; dcXL = xl; xl = dcL;
            dcR = xr - dcXR + 0.995f * dcR; dcXR = xr; xr = dcR;
            const float loopTrim = 1.0f / (1.0f + rEff * 2.5f);
            dl[(size_t) dw] = std::tanh (xl) * loopTrim;
            dr[(size_t) dw] = std::tanh (xr) * loopTrim;
            if (++dw >= dlen) dw = 0;

            // NIVÅ: långsam automatisk utjämning så alla lägen landar på samma volym.
            // Höjer aldrig tystnad och svansar, rör sig över cirka en och en halv sekund.
            const float ol = xl + lowL, orr = xr + lowR;
            levelMs += (0.5f * (ol * ol + orr * orr) - levelMs) * levelA;
            if (levelMs > 1e-5f)
            {
                const float target = std::clamp (0.24f / std::sqrt (levelMs), 0.5f, 6.0f);
                agc += (target - agc) * agcA;
            }
            l[i] = std::tanh (ol * agc * 1.1f) * smOut;
            r[i] = std::tanh (orr * agc * 1.1f) * smOut;

            ppq += beatsPerSample;
            lastLfo = lfo;
        }
        uiLfo.store (lastLfo);
        if (! hostRun && running) freePpq = ppq;

        // vakt mot NaN/inf
        bool bad = false;
        for (int i = 0; i < n; ++i) if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) { bad = true; break; }
        if (bad)
        {
            clear (l, r, n);
            softReset();
            errors.fetch_add (1);
            if (++errRun >= 20) safe = true;
        }
        else errRun = 0;

        uiBar.store ((int) std::floor (ppq / 4.0));
        uiRunning.store (running);
        publish (s, p);
    }

    // CPU-vakt: färre korn och röster när datorn inte hinner, fler när det lugnat sig
    void reportLoad (float load)
    {
        cpu += (load - cpu) * 0.1f;
        if (load > 0.6f) ++over; else if (over > 0) --over;
        if (over > 6)
        {
            over = 0;
            if (maxGrains > 2) maxGrains = 2;
            else if (maxVoices > 4) maxVoices = 4;
        }
        else if (cpu < 0.2f && ++calm > 400)
        {
            calm = 0;
            if (maxVoices < kVoices) maxVoices = kVoices;
            else if (maxGrains < kGrains) maxGrains = kGrains;
        }
        uiCpu.store (cpu);
    }

    void markError() { errors.fetch_add (1); if (++errRun >= 20) safe = true; }

    // telemetri till fönstret
    std::atomic<int> uiState { Run }, uiStep { -1 }, uiSlice { -1 }, uiBar { 0 }, errors { 0 }, uiVoices { 0 };
    std::atomic<float> uiHold { 0 }, uiCpu { 0 }, uiLfo { 0 }, uiPress { 0 }, uiTemp { 0 }, uiPhase { 0 };
    std::atomic<bool> uiSafe { false }, uiRunning { false }, uiLite { false };
    std::atomic<float> uiVoicePos[kVoices] {};
    std::atomic<int> uiFlash { 0 };    // räknas upp vid varje glitch, fönstret blinkar

private:
    struct Trig { bool on = false; int wait = 0, slice = 0; float gain = 1, dec = 1; double rate = 1; bool rev = false; };
    static constexpr int kWin = 2048;
    static constexpr int kPend = 32;

    float eff (float v) const { return std::min (1.0f, v + (1.0f - v) * smHeat); }
    static void sm (float& s, float t) { s += (t - s) * 0.002f; }
    static void clear (float* l, float* r, int n) { std::fill (l, l + n, 0.0f); std::fill (r, r + n, 0.0f); }

    void schedule (int wait, int slice, float gain, float dec, double rate, bool rev)
    {
        for (auto& t : pend)
            if (! t.on) { t = { true, std::max (1, wait), slice, gain, dec, rate, rev }; return; }
    }

    void triggerStep (int64_t absStep, int steps, double stepSamples, const Params& p)
    {
        const int idx = (int) (((absStep % steps) + steps) % steps);
        uiStep.store (idx);
        uiPhase.store ((float) idx / (float) steps);
        const Step& st = pat.s[idx];
        if (! st.on) return;

        const float sh = smHeat;
        const float drift = std::min (1.0f, p.drift + sh * 0.4f);
        const float valve = std::min (1.0f, p.valve + (1.0f - p.valve) * sh);

        // OTAKT: drift tappar eller dubblar ibland steg, och släpar dem i tid
        if (rng.uni() < drift * 0.12f) return;
        int wait = 0;
        if ((absStep & 1) != 0) wait += (int) (p.swing * 0.33 * stepSamples);
        wait += (int) (rng.uni() * drift * 0.45f * (float) stepSamples);

        int slice = st.slice;
        int ratchet = std::max (1, (int) st.ratchet);
        bool rev = st.rev != 0;
        double rate = std::pow (2.0, (p.pitch + pitchNote) / 12.0);
        float gain = st.accent ? 1.0f : 0.55f;
        float tau = (float) stepSamples * (0.12f + 6.0f * p.gate * p.gate) * (st.accent ? 1.4f : 1.0f);

        // TRYCKFALL: glitch-händelser, i takt men inte snälla
        if (rng.uni() < valve * 0.6f)
        {
            uiFlash.fetch_add (1);
            switch (rng.pick (6))
            {
                case 0: ratchet = 3 + rng.pick (6); tau *= 0.3f; break;   // stamning
                case 1: rev = ! rev; break;
                case 2: rate *= 0.5; break;                                // oktav ner
                case 3: return;                                            // tryckfallet: tystnad
                case 4: slice = rng.pick (32); break;                      // hoppar i ljudet
                default: rate *= 2.0; tau *= 0.5f; break;                  // oktav upp
            }
        }
        if (rng.uni() < drift * 0.08f) ratchet *= 2;

        const float dec = std::exp (-1.0f / std::max (64.0f, tau / (float) ratchet));
        for (int j = 0; j < ratchet; ++j)
            schedule (wait + (int) (j * stepSamples / ratchet), slice, gain * (j == 0 ? 1.0f : 0.8f), dec, rate, rev);
    }

    void startVoice (const Trig& t, const Sample* s, double sliceLen, double srcRatio, double, const Params& p)
    {
        if (s == nullptr || s->len() < 64) return;
        const int nSlices = kSliceCounts[std::clamp (p.slices, 0, 3)];
        const int sl = t.slice % nSlices;
        uiSlice.store (sl);
        // äldsta tysta rösten, annars den svagaste
        int best = 0; float bestAmp = 2.0f;
        for (int v = 0; v < maxVoices; ++v)
        {
            if (! voices[v].on) { best = v; break; }
            if (voices[v].amp < bestAmp) { bestAmp = voices[v].amp; best = v; }
        }
        const double start = t.rev ? (sl + 1) * sliceLen - 1.0 : sl * sliceLen;
        voices[best].start (start, (t.rev ? -1.0 : 1.0) * t.rate * srcRatio, t.dec, t.gain);
    }

    void compress (float pr, float fold, float crush, float& l, float& r)
    {
        const float wet = std::min (1.0f, pr * 3.0f + fold + crush);
        if (wet < 0.003f) return;
        const float drive = 1.0f + 30.0f * pr * pr;
        auto shape = [&] (float x)
        {
            const float y = x * drive;
            float f = std::fmod (y * (1.0f + fold * 3.0f) - 1.0f, 4.0f);
            if (f < 0) f += 4.0f;
            f = std::fabs (f - 2.0f) - 1.0f;                   // triangelvikning
            return std::tanh (y) * (1.0f - fold) + f * fold;
        };
        const int k = 1 + (int) (crush * crush * 32.0f);
        if (++hc >= k) { hc = 0; hl = shape (l); hr = shape (r); }
        float yl = hl, yr = hr;
        if (crush > 0.4f)
        {
            const float q = std::exp2 (16.0f - 13.0f * (crush - 0.4f) / 0.6f);   // 16 -> 3 bitar
            yl = std::round (yl * q) / q;
            yr = std::round (yr * q) / q;
        }
        l += (yl - l) * wet;
        r += (yr - r) * wet;
    }

    void cooler (float t, float reso, float hunt, int type, float& l, float& r)
    {
        if (--coolN <= 0)
        {
            coolN = 16;
            float fc = 60.0f + 19000.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
            fc *= std::exp2 (hunt * 3.0f);
            fc = std::clamp (fc, 30.0f, 0.45f * (float) sr);
            coolG = std::tan (kPi * fc / (float) sr);
            coolK = std::max (0.06f, 2.0f - 1.94f * std::min (1.0f, reso + t * 0.3f));
        }
        float lp, bp, hp;
        auto run = [&] (Svf& a, Svf& b, float x)
        {
            a.tick (x, coolG, 1.414f, lp, bp, hp);
            const float x2 = type == 0 ? lp : (type == 1 ? bp * 1.414f : hp);
            b.tick (x2, coolG, coolK, lp, bp, hp);
            return std::tanh (type == 0 ? lp : (type == 1 ? bp : hp));
        };
        l = run (c1L, c2L, l);
        r = run (c1R, c2R, r);
    }

    void frozen (float* l, float* r, int n)
    {
        const int total = (int) (3.0 * sr);
        const float fade = (float) (0.4 * sr);
        for (int i = 0; i < n; ++i)
        {
            // isig drönare: det som fanns i kretsen loopar, mörkt och tyst
            iceL += (dl[(size_t) dw] * 0.35f - iceL) * 0.05f;
            iceR += (dr[(size_t) dw] * 0.35f - iceR) * 0.05f;
            if (++dw >= dlen) dw = 0;
            float hl_ = 0, hr_ = 0;
            if (noiseLeft > 0)
            {
                const float tt = (float) (total - noiseLeft);
                const float env = std::min (1.0f, tt / 200.0f) * std::min (1.0f, (float) noiseLeft / fade);
                hl_ = noise.bi() * 0.3f * env;
                hr_ = noise.bi() * 0.3f * env;
                if (--noiseLeft <= 0) state = Ice;
            }
            l[i] = iceL + hl_;
            r[i] = iceR + hr_;
        }
    }

    void softReset()
    {
        for (auto& v : voices) v.on = false;
        for (auto& t : pend) t.on = false;
        c1L.reset(); c2L.reset(); c1R.reset(); c2R.reset(); bassL.reset(); bassR.reset();
        std::fill (dl.begin(), dl.end(), 0.0f);
        std::fill (dr.begin(), dr.end(), 0.0f);
        dcL = dcR = dcXL = dcXR = 0; hl = hr = 0; iceL = iceR = 0; levelMs = 0; agc = 1.0f;
    }

    void fullReset()
    {
        softReset();
        state = Run; holdCount = 0; noiseLeft = 0; safe = false; errRun = 0;
        smHeat = 0; maxVoices = kVoices; maxGrains = kGrains;
        lastAbs = INT64_MIN / 2; freePpq = -1.0;
    }

    void publish (const Sample* s, const Params& p)
    {
        const int L = s ? s->len() : 0;
        uiState.store (state);
        uiSafe.store (safe);
        int count = 0;
        for (int v = 0; v < kVoices; ++v)
        {
            const bool on = voices[v].on && L > 0 && v < maxVoices;
            count += on ? 1 : 0;
            uiVoicePos[v].store (on ? (float) (std::fmod (std::fabs (voices[v].pos), (double) L) / L) : -1.0f);
        }
        uiVoices.store (count);
        uiPress.store (eff (smPress));
        uiTemp.store (smTemp + std::max (0.0f, 0.62f - smTemp) * smHeat);
        (void) p;
    }

    // tillstånd
    Pattern pat;
    Voice voices[kVoices];
    Trig pend[kPend];
    float win[kWin] {};
    std::vector<float> dl, dr;
    int dlen = 1, dw = 0;
    Svf c1L, c2L, c1R, c2R, bassL, bassR;
    Rng rng, noise, huntRng;

    double sr = 44100.0, freePpq = -1.0;
    int64_t lastAbs = INT64_MIN / 2, huntCycle = 0;
    int pitchNote = 0, heldCount = 0;
    bool noteHeld = false, safe = false;
    int state = Run, holdCount = 0, noiseLeft = 0, errRun = 0;
    int maxVoices = kVoices, maxGrains = kGrains, over = 0, calm = 0;
    int hc = 0, coolN = 0, bassN = 0;
    float hl = 0, hr = 0, coolG = 0.5f, coolK = 2.0f, bassG = 0.01f;
    float huntWalk = 0, huntTarget = 0, lastLfo = 0;
    float levelMs = 0, agc = 1.0f, levelA = 1e-4f, agcA = 1e-5f;
    float dcL = 0, dcR = 0, dcXL = 0, dcXR = 0, iceL = 0, iceR = 0;
    float smEvap = 0, smSpread = 0, smPress = 0, smFold = 0, smCrush = 0, smTemp = 0, smReso = 0.3f, smHunt = 0,
          smCirc = 0, smHeat = 0, smOut = 0.7f, smBass = 0, cpu = 0;
    std::atomic<bool> wantReset { false };
};
} // namespace r7
