#pragma once
// ---------------------------------------------------------------------------
//  REVERIE - moteur audio (aucune dépendance à JUCE : testable seul)
// ---------------------------------------------------------------------------
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace rv
{
constexpr float PI = 3.14159265358979323846f;

inline float dbToGain (float db)  { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)   { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Lissage "un pôle" pour éviter les craquements quand on tourne un bouton
struct Smoother
{
    float cur = 0.0f, target = 0.0f, a = 0.01f;
    void prepare (double sr, float ms) { a = 1.0f - std::exp (-1.0f / (0.001f * ms * (float) sr)); }
    void reset (float v)               { cur = target = v; }
    void set (float v)                 { target = v; }
    inline float next()                { cur += a * (target - cur); return cur; }
};

struct OnePole
{
    float z = 0.0f, a = 1.0f;
    void setCutoff (float fc, float sr) { a = 1.0f - std::exp (-2.0f * PI * std::min (fc, 0.45f * sr) / sr); }
    void reset()                        { z = 0.0f; }
    inline float process (float x)      { z += a * (x - z); return z; }
};

// Ligne à retard avec interpolation linéaire. Utilisation : read() PUIS write().
// read(d) renvoie l'échantillon écrit il y a d échantillons (d >= 1).
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, pos = 0, size = 0;

    void prepare (int maxSamples)
    {
        size = 8;
        while (size < maxSamples + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        pos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x) { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }
    inline float read (float d) const
    {
        d = std::min (std::max (d, 1.0f), (float) (size - 3));
        const int i = (int) d;
        const float f = d - (float) i;
        const int i0 = (pos + 1 - i) & mask;
        const int i1 = (i0 - 1) & mask;
        const float a = buf[(size_t) i0], b = buf[(size_t) i1];
        return a + f * (b - a);
    }
};

struct Allpass
{
    DelayLine dl;
    int len = 1;
    float g = 0.6f;
    void prepare (int n, float gain) { len = std::max (1, n); g = gain; dl.prepare (len + 2); }
    void clear() { dl.clear(); }
    inline float process (float x)
    {
        const float z = dl.read ((float) len);
        const float v = x - g * z;
        dl.write (v);
        return g * v + z;
    }
};

// Pitch shifter granulaire à deux têtes de lecture (fonctionne sur du polyphonique)
struct PitchShifter
{
    DelayLine dl;
    float grain = 2048.0f, phase = 0.0f;

    void prepare (double sr, float grainMs)
    {
        grain = (float) (sr * grainMs * 0.001);
        dl.prepare ((int) grain + 16);
        phase = 0.0f;
    }
    void reset() { dl.clear(); phase = 0.0f; }

    inline float process (float in, float ratio)
    {
        phase += (1.0f - ratio) / grain;
        phase -= std::floor (phase);
        const float w1 = 0.5f - 0.5f * std::cos (2.0f * PI * phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float out = dl.read (2.0f + phase * grain) * w1
                        + dl.read (2.0f + p2 * grain) * (1.0f - w1);
        dl.write (in);
        return out;
    }
};

// Mixage "équi-puissance" sec / traité
struct MixGains
{
    static inline void get (float mix, float& dry, float& wet)
    {
        dry = std::cos (mix * 0.5f * PI);
        wet = std::sin (mix * 0.5f * PI);
    }
};

// ---------------------------------------------------------------------------
//  Réverbération à réseau de retards rétroactifs (FDN 8 lignes)
// ---------------------------------------------------------------------------
class Reverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate, int variant)
    {
        sr = (float) sampleRate;
        static const float baseA[N] = { 31.7f, 37.3f, 41.9f, 47.9f, 53.1f, 61.3f, 67.9f, 73.7f };
        static const float baseB[N] = { 23.9f, 29.9f, 36.7f, 44.3f, 52.9f, 64.1f, 71.3f, 86.9f };
        for (int i = 0; i < N; ++i)
        {
            baseMs[i] = variant == 0 ? baseA[i] : baseB[i];
            lines[i].prepare ((int) (0.30f * sr));
            lp[i].reset();
            phase[i] = (float) i / (float) N;
            static const float rates[N] = { 0.13f, 0.17f, 0.23f, 0.29f, 0.31f, 0.37f, 0.41f, 0.47f };
            phaseInc[i] = rates[i] / sr;
        }
        static const int apL[4] = { 142, 107, 379, 277 };
        static const int apR[4] = { 151, 113, 367, 263 };
        for (int i = 0; i < 4; ++i)
        {
            ap[0][i].prepare ((int) ((float) apL[i] * sr / 44100.0f), 0.65f);
            ap[1][i].prepare ((int) ((float) apR[i] * sr / 44100.0f), 0.65f);
        }
        preL.prepare ((int) (0.45f * sr));
        preR.prepare ((int) (0.45f * sr));
        sizeSm.prepare (sr, 250.0f);  sizeSm.reset (1.0f);
        preSm.prepare (sr, 120.0f);   preSm.reset (0.0f);
        setParams (0.5f, 3.0f, 0.4f, 0.3f, 0.0f);
        sizeSm.reset (curScale);
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        for (auto& f : lp) f.reset();
        for (auto& c : ap) for (auto& a : c) a.clear();
        preL.clear(); preR.clear();
    }

    void setParams (float size01, float decaySec, float damp01, float mod01, float preMs)
    {
        curScale = 0.6f + 1.4f * size01;
        sizeSm.set (curScale);
        const float t60 = std::max (0.1f, decaySec);
        const float fc = 16000.0f * std::pow (0.05f, damp01);
        for (int i = 0; i < N; ++i)
        {
            lp[i].setCutoff (fc, sr);
            const float dSec = baseMs[i] * 0.001f * curScale;
            gain[i] = std::pow (10.0f, -3.0f * dSec / t60);
        }
        modDepth = mod01 * 24.0f * sr / 48000.0f;
        preSm.set (preMs * 0.001f * sr);
        outScale = 2.1f / std::sqrt (std::max (0.5f, t60));   // niveau de sortie à peu près constant
    }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        const float scale = sizeSm.next();
        const float pre = std::max (1.0f, preSm.next());
        float xL = preL.read (pre);  preL.write (inL);
        float xR = preR.read (pre);  preR.write (inR);
        for (int i = 0; i < 4; ++i) { xL = ap[0][i].process (xL); xR = ap[1][i].process (xR); }

        float v[N];
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            phase[i] += phaseInc[i];
            if (phase[i] >= 1.0f) phase[i] -= 1.0f;
            const float d = baseMs[i] * 0.001f * sr * scale + modDepth * std::sin (2.0f * PI * phase[i]);
            const float y = lp[i].process (lines[i].read (d) * gain[i]);
            v[i] = y;
            sum += y;
        }
        const float k = sum * (2.0f / (float) N);   // matrice de Householder (sans perte)
        for (int i = 0; i < N; ++i)
            lines[i].write (((i & 1) ? xR : xL) * 0.5f + v[i] - k);

        outL = (v[0] + v[2] - v[4] - v[6]) * outScale;
        outR = (v[1] - v[3] + v[5] - v[7]) * outScale;
    }

private:
    float sr = 44100.0f, curScale = 1.0f, modDepth = 0.0f, outScale = 0.2f;
    float baseMs[N] {}, gain[N] {}, phase[N] {}, phaseInc[N] {};
    DelayLine lines[N];
    OnePole lp[N];
    Allpass ap[2][4];
    DelayLine preL, preR;
    Smoother sizeSm, preSm;
};

// Réverb + boucle "shimmer" (la queue est transposée puis réinjectée)
class ShimmerReverb
{
public:
    void prepare (double sr)
    {
        rev.prepare (sr, 1);
        psL.prepare (sr, 90.0f);
        psR.prepare (sr, 90.0f);
        lpL.setCutoff (6500.0f, (float) sr);
        lpR.setCutoff (6500.0f, (float) sr);
        hpL.setCutoff (150.0f, (float) sr);
        hpR.setCutoff (150.0f, (float) sr);
        amtSm.prepare (sr, 60.0f);
        reset();
    }
    void reset()
    {
        rev.reset(); psL.reset(); psR.reset();
        lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
        fbL = fbR = 0.0f;
    }
    void setParams (float size, float decay, float damp, float mod, float pre, float shimmer01, float semitones)
    {
        rev.setParams (size, decay, damp, mod, pre);
        ratio = std::pow (2.0f, semitones / 12.0f);
        amtSm.set (shimmer01 * 0.55f);
    }
    inline void process (float inL, float inR, float& outL, float& outR)
    {
        rev.process (inL + fbL, inR + fbR, outL, outR);
        const float amt = amtSm.next();
        float sL = psL.process (outL, ratio);
        float sR = psR.process (outR, ratio);
        sL = lpL.process (sL);  sL -= hpL.process (sL);
        sR = lpR.process (sR);  sR -= hpR.process (sR);
        fbL = std::tanh (sL * amt * 3.0f) * 0.20f;
        fbR = std::tanh (sR * amt * 3.0f) * 0.20f;
    }
private:
    Reverb rev;
    PitchShifter psL, psR;
    OnePole lpL, lpR, hpL, hpR;
    Smoother amtSm;
    float ratio = 2.0f, fbL = 0.0f, fbR = 0.0f;
};

// ---------------------------------------------------------------------------
//  Auto-swell réglable : détecte l'attaque de la note et la fait "gonfler"
// ---------------------------------------------------------------------------
class Swell
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        aF = coef (0.5f);   rF = coef (25.0f);
        aS = coef (45.0f);  rS = coef (250.0f);
        fallFast = coef (3.0f);
        coefUp = coef (2.0f);
        reset();
    }
    void reset() { envF = envS = 0.0f; gain = 1.0f; swelling = false; armed = true; holdoff = 0; t = 0.0f; }

    void setParams (float threshDb, float attackMs, float releaseMs, float depth, float shape)
    {
        thr = dbToGain (threshDb);
        attackSamples = std::max (1.0f, attackMs * 0.001f * sr);
        rel = coef (releaseMs);
        floorGain = 1.0f - depth;
        expo = std::pow (3.0f, shape);   // shape -1..1 -> exposant 0.33..3
    }

    inline void process (float& l, float& r)
    {
        const float x = std::max (std::fabs (l), std::fabs (r));
        envF += (x > envF ? aF : rF) * (x - envF);
        envS += (x > envS ? aS : rS) * (x - envS);

        if (holdoff > 0) --holdoff;
        if (envF < thr * 0.5f) { armed = true; if (swelling) { swelling = false; } }

        const bool onset = envF > thr && (armed || (holdoff == 0 && envF > envS * 2.2f));
        if (onset && holdoff == 0)
        {
            swelling = true; armed = false; t = 0.0f;
            holdoff = (int) (0.12f * sr);
        }

        float target = floorGain;
        if (swelling)
        {
            t += 1.0f;
            const float ramp = std::min (1.0f, t / attackSamples);
            target = floorGain + (1.0f - floorGain) * std::pow (ramp, expo);
        }
        const float c = (target > gain) ? coefUp : (swelling ? fallFast : rel);
        gain += c * (target - gain);
        l *= gain;
        r *= gain;
    }

private:
    float coef (float ms) const { return 1.0f - std::exp (-1.0f / (0.001f * ms * sr)); }
    float sr = 44100.0f, aF = 0.1f, rF = 0.001f, aS = 0.001f, rS = 0.0001f, rel = 0.001f, fallFast = 0.01f;
    float coefUp = 0.02f;
    float thr = 0.01f, attackSamples = 20000.0f, floorGain = 0.0f, expo = 1.0f;
    float envF = 0.0f, envS = 0.0f, gain = 1.0f, t = 0.0f;
    bool swelling = false, armed = true;
    int holdoff = 0;
};

// ---------------------------------------------------------------------------
//  Delay stéréo : réglages gauche / droite indépendants
// ---------------------------------------------------------------------------
class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        dlL.prepare ((int) (2.2f * sr));
        dlR.prepare ((int) (2.2f * sr));
        tL.prepare (sr, 90.0f);  tR.prepare (sr, 90.0f);
        mixSm.prepare (sr, 30.0f);
        tL.reset (0.3f * sr);  tR.reset (0.45f * sr);
        reset();
    }
    void reset() { dlL.clear(); dlR.clear(); lpL.reset(); lpR.reset(); }

    void setParams (float timeLms, float timeRms, float fbl, float fbr, float levl, float levr,
                    float crossAmt, float tone01, float mod01, float mix)
    {
        tL.set (std::min (timeLms, 2000.0f) * 0.001f * sr);
        tR.set (std::min (timeRms, 2000.0f) * 0.001f * sr);
        fbL = fbl; fbR = fbr; levL = levl; levR = levr; cross = crossAmt;
        const float fc = 500.0f * std::pow (40.0f, tone01);
        lpL.setCutoff (fc, sr); lpR.setCutoff (fc, sr);
        modSamples = mod01 * 0.004f * sr;
        mixSm.set (mix);
    }

    inline void process (float& l, float& r)
    {
        const float dl = tL.next(), dr = tR.next();
        phase += 0.45f / sr;  if (phase >= 1.0f) phase -= 1.0f;
        const float m = std::sin (2.0f * PI * phase) * modSamples;
        const float yl = lpL.process (dlL.read (dl + m));
        const float yr = lpR.process (dlR.read (dr - m));
        const float fl = std::tanh (yl * fbL), fr = std::tanh (yr * fbR);
        dlL.write (l + (1.0f - cross) * fl + cross * fr);
        dlR.write (r + (1.0f - cross) * fr + cross * fl);

        float dry, wet;
        MixGains::get (mixSm.next(), dry, wet);
        l = l * dry + yl * levL * wet;
        r = r * dry + yr * levR * wet;
    }

private:
    float sr = 44100.0f, fbL = 0.4f, fbR = 0.4f, levL = 1.0f, levR = 1.0f, cross = 0.0f, modSamples = 0.0f, phase = 0.0f;
    DelayLine dlL, dlR;
    OnePole lpL, lpR;
    Smoother tL, tR, mixSm;
};

// ---------------------------------------------------------------------------
//  Quadravox : 4 voix d'harmonisation indépendantes (intervalle / niveau / pan)
// ---------------------------------------------------------------------------
class Quadravox
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int v = 0; v < 4; ++v)
        {
            ps[v].prepare (sampleRate, 60.0f);
            gL[v].prepare (sr, 40.0f);  gR[v].prepare (sr, 40.0f);
            gL[v].reset (0.0f);         gR[v].reset (0.0f);
        }
        mixSm.prepare (sr, 30.0f);
        reset();
    }
    void reset() { for (auto& p : ps) p.reset(); }

    void setParams (const float* semis, const float* level, const float* pan, float mix)
    {
        for (int v = 0; v < 4; ++v)
        {
            ratio[v] = std::pow (2.0f, semis[v] / 12.0f);
            const float a = (pan[v] + 1.0f) * 0.25f * PI;
            gL[v].set (level[v] * std::cos (a));
            gR[v].set (level[v] * std::sin (a));
        }
        mixSm.set (mix);
    }

    inline void process (float& l, float& r)
    {
        const float m = 0.5f * (l + r);
        float wl = 0.0f, wr = 0.0f;
        for (int v = 0; v < 4; ++v)
        {
            const float y = ps[v].process (m, ratio[v]);
            wl += y * gL[v].next();
            wr += y * gR[v].next();
        }
        float dry, wet;
        MixGains::get (mixSm.next(), dry, wet);
        l = l * dry + wl * wet;
        r = r * dry + wr * wet;
    }

private:
    float sr = 44100.0f, ratio[4] { 2.0f, 1.5f, 0.5f, 1.0f };
    PitchShifter ps[4];
    Smoother gL[4], gR[4], mixSm;
};

// ---------------------------------------------------------------------------
//  Compresseur (détection crête liée stéréo, coude doux, mix parallèle)
// ---------------------------------------------------------------------------
class Compressor
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; mixSm.prepare (sr, 30.0f); reset(); }
    void reset() { grDb = 0.0f; }

    void setParams (float threshDb, float ratioIn, float attackMs, float releaseMs, float makeupDb, float mix)
    {
        thr = threshDb; ratio = std::max (1.0f, ratioIn);
        aA = std::exp (-1.0f / (0.001f * std::max (0.05f, attackMs) * sr));
        aR = std::exp (-1.0f / (0.001f * std::max (1.0f, releaseMs) * sr));
        makeup = makeupDb;
        mixSm.set (mix);
    }

    inline void process (float& l, float& r)
    {
        const float lvl = gainToDb (std::max (std::fabs (l), std::fabs (r)));
        const float over = lvl - thr;
        float out;
        if (2.0f * over < -knee)            out = lvl;
        else if (2.0f * std::fabs (over) <= knee)
        {
            const float t = over + knee * 0.5f;
            out = lvl + (1.0f / ratio - 1.0f) * t * t / (2.0f * knee);
        }
        else                                out = thr + over / ratio;
        const float target = out - lvl;                     // <= 0
        grDb = (target < grDb) ? aA * grDb + (1.0f - aA) * target
                               : aR * grDb + (1.0f - aR) * target;
        const float g = dbToGain (grDb + makeup);
        const float m = mixSm.next();   // mix parallèle linéaire
        l = l * (1.0f - m) + l * g * m;
        r = r * (1.0f - m) + r * g * m;
    }

private:
    float sr = 44100.0f, thr = -18.0f, ratio = 3.0f, aA = 0.99f, aR = 0.999f, makeup = 0.0f, grDb = 0.0f, knee = 8.0f;
    Smoother mixSm;
};

} // namespace rv
