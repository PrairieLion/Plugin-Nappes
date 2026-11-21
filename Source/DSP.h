#pragma once
// ---------------------------------------------------------------------------
//  REVERIE - moteur audio (aucune dépendance à JUCE : testable seul)
// ---------------------------------------------------------------------------
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <memory>
#include <cstdint>

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


// ---------------------------------------------------------------------------
//  Utilitaires : tableau brut (sans avertissements de signe) et FFT radix-2
// ---------------------------------------------------------------------------
struct FArr
{
    std::unique_ptr<float[]> p;
    int n = 0;
    void alloc (int size, float v = 0.0f) { n = size; p.reset (new float[(size_t) size]); std::fill (p.get(), p.get() + size, v); }
    inline float& operator[] (int i)       { return p.get()[i]; }
    inline float  operator[] (int i) const { return p.get()[i]; }
    float* data() { return p.get(); }
};

class FFT
{
public:
    void prepare (int size)
    {
        n = size;
        twr.alloc (n / 2);  twi.alloc (n / 2);  rev.assign ((size_t) n, 0);
        int bits = 0;  while ((1 << bits) < n) ++bits;
        for (int k = 0; k < n / 2; ++k)
        {
            twr[k] =  std::cos (2.0f * PI * (float) k / (float) n);
            twi[k] = -std::sin (2.0f * PI * (float) k / (float) n);
        }
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    // Transformée directe ou inverse (non normalisée)
    void run (float* re, float* im, bool inverse) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float wr = twr[j * step];
                    const float wi = inverse ? -twi[j * step] : twi[j * step];
                    const int a = i + j, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr;  im[b] = im[a] - ti;
                    re[a] += tr;         im[a] += ti;
                }
        }
    }
private:
    int n = 0;
    FArr twr, twi;
    std::vector<int> rev;
};

// ---------------------------------------------------------------------------
//  Filtres biquad (formules RBJ) : passe-haut/bas, cloche, plateaux
// ---------------------------------------------------------------------------
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    static BiquadCoeffs make (float b0, float b1, float b2, float a0, float a1, float a2)
    {
        BiquadCoeffs c;
        c.b0 = b0 / a0;  c.b1 = b1 / a0;  c.b2 = b2 / a0;  c.a1 = a1 / a0;  c.a2 = a2 / a0;
        return c;
    }
    static float clampF (float f, float fs) { return std::min (std::max (f, 10.0f), 0.47f * fs); }

    static BiquadCoeffs lowpass (float fs, float f, float q = 0.7071f)
    {
        const float w = 2.0f * PI * clampF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        return make ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoeffs highpass (float fs, float f, float q = 0.7071f)
    {
        const float w = 2.0f * PI * clampF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        return make ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    static BiquadCoeffs peak (float fs, float f, float gainDb, float q)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * PI * clampF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0f * std::max (0.1f, q));
        return make (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    static BiquadCoeffs lowShelf (float fs, float f, float gainDb)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * PI * clampF (f, fs) / fs, c = std::cos (w), s = std::sin (w);
        const float t = 2.0f * std::sqrt (A) * (s * 0.70710678f);
        return make (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
                     (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    static BiquadCoeffs highShelf (float fs, float f, float gainDb)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * PI * clampF (f, fs) / fs, c = std::cos (w), s = std::sin (w);
        const float t = 2.0f * std::sqrt (A) * (s * 0.70710678f);
        return make (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
                     (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
    // Réponse en amplitude (dB) à la fréquence f
    float magnitudeDb (float fs, float f) const
    {
        const float w = 2.0f * PI * f / fs;
        const float c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const float dr = 1 + a1 * c1 + a2 * c2,  di = -(a1 * s1 + a2 * s2);
        return 10.0f * std::log10 (std::max (1.0e-12f, (nr * nr + ni * ni) / std::max (1.0e-12f, dr * dr + di * di)));
    }
};

struct Biquad
{
    BiquadCoeffs c;
    float z1[2] { 0, 0 }, z2[2] { 0, 0 };
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    inline float process (int ch, float x)
    {
        const float y = c.b0 * x + z1[ch];
        z1[ch] = c.b1 * x - c.a1 * y + z2[ch];
        z2[ch] = c.b2 * x - c.a2 * y;
        return y;
    }
};

// ---------------------------------------------------------------------------
//  Égaliseur : coupe-bas, plateau grave, 2 cloches, plateau aigu, coupe-haut
// ---------------------------------------------------------------------------
class ParametricEQ
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; reset(); }
    void reset() { hp.reset(); ls.reset(); m1.reset(); m2.reset(); hs.reset(); lp.reset(); }

    void setParams (float hpF, float lsF, float lsG, float m1F, float m1G, float m1Q,
                    float m2F, float m2G, float m2Q, float hsF, float hsG, float lpF)
    {
        useHp = hpF > 21.0f;     useLp = lpF < 19500.0f;
        useLs = std::fabs (lsG) > 0.05f;  use1 = std::fabs (m1G) > 0.05f;
        use2  = std::fabs (m2G) > 0.05f;  useHs = std::fabs (hsG) > 0.05f;
        hp.c = BiquadCoeffs::highpass (sr, hpF);
        ls.c = BiquadCoeffs::lowShelf (sr, lsF, lsG);
        m1.c = BiquadCoeffs::peak (sr, m1F, m1G, m1Q);
        m2.c = BiquadCoeffs::peak (sr, m2F, m2G, m2Q);
        hs.c = BiquadCoeffs::highShelf (sr, hsF, hsG);
        lp.c = BiquadCoeffs::lowpass (sr, lpF);
    }

    inline void process (float& l, float& r)
    {
        float x[2] = { l, r };
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = x[ch];
            if (useHp) v = hp.process (ch, v);
            if (useLs) v = ls.process (ch, v);
            if (use1)  v = m1.process (ch, v);
            if (use2)  v = m2.process (ch, v);
            if (useHs) v = hs.process (ch, v);
            if (useLp) v = lp.process (ch, v);
            x[ch] = v;
        }
        l = x[0]; r = x[1];
    }

private:
    float sr = 44100.0f;
    bool useHp = false, useLp = false, useLs = false, use1 = false, use2 = false, useHs = false;
    Biquad hp, ls, m1, m2, hs, lp;
};

// ---------------------------------------------------------------------------
//  Moteur de "gel" du son (vocodeur de phase figé) : sustain infini
//  Le son est analysé par FFT, puis resynthétisé en continu avec des phases
//  qui avancent à la fréquence exacte de chaque partiel -> note tenue stable.
// ---------------------------------------------------------------------------
class FreezeEngine
{
public:
    static constexpr int N = 8192, HOP = N / 4, BINS = N / 2 + 1, RING = 16384;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        fft.prepare (N);
        ring.alloc (RING);  win.alloc (N);  re.alloc (N);  im.alloc (N);
        m1.alloc (BINS);  p1.alloc (BINS);  cur.alloc (BINS);  target.alloc (BINS);
        ph.alloc (BINS);  adv.alloc (BINS);  off.alloc (BINS);  nm.alloc (BINS);  np.alloc (BINS);  na.alloc (BINS);
        olaL.alloc (N);  olaR.alloc (N);
        for (int i = 0; i < N; ++i) win[i] = 0.5f - 0.5f * std::cos (2.0f * PI * (float) i / (float) N);
        std::uint32_t s = 0x9E3779B9u;
        for (int k = 0; k < BINS; ++k)
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            off[k] = ((float) (s & 0xFFFFFF) / 16777216.0f - 0.5f) * PI * 0.9f;   // décorrélation stéréo fixe
        }
        wp = 0; stop();
        setGlideMs (200.0f);
    }

    void stop()
    {
        active = false; outPos = 0; hopCount = 0;
        if (olaL.n > 0) { std::fill (olaL.data(), olaL.data() + N, 0.0f); std::fill (olaR.data(), olaR.data() + N, 0.0f); }
    }
    bool isActive() const { return active; }

    void setGlideMs (float ms)
    {
        const float hopSec = (float) HOP / sr;
        glide = 1.0f - std::exp (-hopSec / std::max (0.005f, ms * 0.001f));
    }

    inline void pushInput (float x) { ring[wp] = x; wp = (wp + 1) & (RING - 1); }

    // Fige les ~170 ms les plus récentes. layer = niveau conservé de l'ancien gel.
    void capture (float layer)
    {
        // deux analyses espacées d'un HOP : modules et différence de phase
        for (int f = 0; f < 2; ++f)
        {
            const int end = wp - (1 - f) * HOP;
            for (int i = 0; i < N; ++i) { re[i] = ring[(end - N + i) & (RING - 1)] * win[i]; im[i] = 0.0f; }
            fft.run (re.data(), im.data(), false);
            for (int k = 0; k < BINS; ++k)
            {
                const float mg = std::sqrt (re[k] * re[k] + im[k] * im[k]);
                const float pp = std::atan2 (im[k], re[k]);
                if (f == 0) { m1[k] = mg; p1[k] = pp; }
                else        { nm[k] = 0.5f * (mg + m1[k]); np[k] = pp; }
            }
        }
        for (int k = 0; k < BINS; ++k)
        {
            const float expected = 0.5f * PI * (float) k;           // 2*pi*k*HOP/N
            float dev = (np[k] - p1[k]) - expected;
            dev -= 2.0f * PI * std::floor (dev / (2.0f * PI) + 0.5f);
            na[k] = expected + dev;
        }

        float eNew = 0.0f;
        for (int k = 0; k < BINS; ++k) eNew += nm[k] * nm[k];

        if (! active)
        {
            for (int k = 0; k < BINS; ++k) { target[k] = cur[k] = nm[k]; adv[k] = na[k]; ph[k] = np[k] - na[k]; }
            active = true;
            std::fill (olaL.data(), olaL.data() + N, 0.0f);
            std::fill (olaR.data(), olaR.data() + N, 0.0f);
            outPos = 0;  hopCount = 0;
            synthesize();
        }
        else
        {
            for (int k = 0; k < BINS; ++k)
            {
                const float old = layer * target[k];
                if (nm[k] >= old) { adv[k] = na[k]; ph[k] = np[k] - na[k]; }     // le nouveau son domine ce partiel
                target[k] = old + nm[k];
            }
        }
        float eT = 0.0f;
        for (int k = 0; k < BINS; ++k) eT += target[k] * target[k];
        const float cap = 2.5f * eNew;
        if (eT > cap && eT > 0.0f)
        {
            const float g = std::sqrt (cap / eT);
            for (int k = 0; k < BINS; ++k) target[k] *= g;
        }
    }

    inline void process (float& outL, float& outR)
    {
        outL = olaL[outPos];  outR = olaR[outPos];
        olaL[outPos] = 0.0f;  olaR[outPos] = 0.0f;
        outPos = (outPos + 1) & (N - 1);
        if (++hopCount >= HOP) { hopCount = 0; synthesize(); }
    }

private:
    static float wrapPi (float x) { return x - 2.0f * PI * std::floor (x / (2.0f * PI) + 0.5f); }

    void synthesize()
    {
        for (int k = 0; k < BINS; ++k)
        {
            cur[k] += glide * (target[k] - cur[k]);
            ph[k] = wrapPi (ph[k] + adv[k]);
        }
        const float scale = 1.0f / (1.5f * (float) N);
        for (int ch = 0; ch < 2; ++ch)
        {
            re[0] = im[0] = 0.0f;  re[N / 2] = im[N / 2] = 0.0f;
            for (int k = 1; k < N / 2; ++k)
            {
                const float a = ph[k] + (ch ? off[k] : 0.0f);
                const float m = cur[k];
                re[k] = m * std::cos (a);  im[k] = m * std::sin (a);
                re[N - k] = re[k];         im[N - k] = -im[k];
            }
            fft.run (re.data(), im.data(), true);
            float* ola = ch ? olaR.data() : olaL.data();
            for (int i = 0; i < N; ++i) ola[(outPos + i) & (N - 1)] += re[i] * win[i] * scale;
        }
    }

    float sr = 44100.0f, glide = 0.3f;
    bool active = false;
    int wp = 0, outPos = 0, hopCount = 0;
    FFT fft;
    FArr ring, win, re, im, m1, p1, cur, target, ph, adv, off, nm, np, na, olaL, olaR;
};

// ---------------------------------------------------------------------------
//  Effets internes du Superego : Detune, Chorus, Tremolo, Filtre, Pitch
// ---------------------------------------------------------------------------
class SuperegoFx
{
public:
    enum Type { Off = 0, Detune, Chorus, Tremolo, Filter, Pitch };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        psL.prepare (sampleRate, 60.0f);  psR.prepare (sampleRate, 60.0f);
        dL.prepare ((int) (0.06f * sr));  dR.prepare ((int) (0.06f * sr));
        reset();
    }
    void reset() { psL.reset(); psR.reset(); dL.clear(); dR.clear(); lowL = bandL = lowR = bandR = 0.0f; phase = 0.0f; }

    void setParams (int t, float rate01, float depth01)
    {
        type = t;  depth = depth01;
        rateHz = 0.1f * std::pow (80.0f, rate01);        // 0.1 .. 8 Hz
        lfoInc = rateHz / sr;
    }

    inline void process (float& l, float& r)
    {
        if (type == Off) return;
        phase += lfoInc;  if (phase >= 1.0f) phase -= 1.0f;
        const float s1 = std::sin (2.0f * PI * phase);
        const float s2 = std::sin (2.0f * PI * (phase + 0.25f));
        switch (type)
        {
            case Detune:
            {
                const float c = depth * 35.0f;
                l = 0.5f * (l + psL.process (l, std::pow (2.0f, c / 1200.0f)));
                r = 0.5f * (r + psR.process (r, std::pow (2.0f, -c / 1200.0f)));
                break;
            }
            case Chorus:
            {
                const float base = 0.014f * sr, d = depth * 0.008f * sr;
                const float yl = dL.read (base + d * s1);   dL.write (l);
                const float yr = dR.read (base + d * s2);   dR.write (r);
                l = 0.5f * (l + yl);  r = 0.5f * (r + yr);
                break;
            }
            case Tremolo:
            {
                l *= 1.0f - depth * 0.5f * (1.0f + s1);
                r *= 1.0f - depth * 0.5f * (1.0f + s2);
                break;
            }
            case Filter:
            {
                const float fcL = 250.0f * std::pow (16.0f, 0.5f + 0.5f * depth * s1);
                const float fcR = 250.0f * std::pow (16.0f, 0.5f + 0.5f * depth * s2);
                const float fL = 2.0f * std::sin (PI * std::min (fcL, 0.2f * sr) / sr);
                const float fR = 2.0f * std::sin (PI * std::min (fcR, 0.2f * sr) / sr);
                const float q = 0.35f;
                lowL += fL * bandL;  const float hL = l - lowL - q * bandL;  bandL += fL * hL;
                lowR += fR * bandR;  const float hR = r - lowR - q * bandR;  bandR += fR * hR;
                l = lowL * 1.4f;  r = lowR * 1.4f;
                break;
            }
            case Pitch:
            {
                const float semis = std::round ((depth * 2.0f - 1.0f) * 12.0f);
                const float ratio = std::pow (2.0f, semis / 12.0f);
                l = psL.process (l, ratio);  r = psR.process (r, ratio);
                break;
            }
            default: break;
        }
    }

private:
    float sr = 44100.0f, depth = 0.5f, rateHz = 1.0f, lfoInc = 0.0001f, phase = 0.0f;
    float lowL = 0, bandL = 0, lowR = 0, bandR = 0;
    int type = 0;
    PitchShifter psL, psR;
    DelayLine dL, dR;
};

// ---------------------------------------------------------------------------
//  Superego : gel du son (Moment / Latch / Auto) ou effets en direct (Live)
// ---------------------------------------------------------------------------
class Superego
{
public:
    enum Mode { Moment = 0, Latch, Auto, Live };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        engine.prepare (sampleRate);
        fx.prepare (sampleRate);
        aF = coef (0.5f); rF = coef (25.0f); aS = coef (45.0f); rS = coef (250.0f);
        dryS.prepare (sr, 30.0f);  effS.prepare (sr, 30.0f);
        dryS.reset (1.0f);  effS.reset (0.9f);
        reset();
    }

    void reset()
    {
        engine.stop();  fx.reset();
        holding = false;  g = 0.0f;  prevFreeze = true;  autoArmed = false;
        envF = envS = 0.0f;  armed = true;  holdoff = 0;  pending = -1;
    }

    void setParams (int modeIn, bool freezeOn, bool clearReq, float attackMs, float decayMs, float threshDb,
                    float layerIn, float glideMs, int fxType, float rate01, float depth01, float dry, float effect)
    {
        if (modeIn != mode)
        {
            mode = modeIn;
            holding = false; autoArmed = false; pending = -1;
        }
        thr = std::pow (10.0f, threshDb * 0.05f);
        layer = layerIn;
        attC = attackMs < 2.0f ? 1.0f : coef (attackMs / 3.0f);
        decC = coef (std::max (5.0f, decayMs / 3.0f));
        engine.setGlideMs (glideMs);
        fx.setParams (fxType, rate01, depth01);
        dryS.set (dry);  effS.set (effect);

        const bool rising  = freezeOn && ! prevFreeze;
        const bool falling = ! freezeOn && prevFreeze;
        prevFreeze = freezeOn;

        if (clearReq) { holding = false; autoArmed = false; pending = -1; }

        switch (mode)
        {
            case Moment:
                if (rising)  { engine.capture (layer); holding = true; }
                if (falling) holding = false;
                break;
            case Latch:
                if (rising)  { engine.capture (layer); holding = true; }
                break;
            case Auto:
                if (rising)  autoArmed = true;
                if (falling) { autoArmed = false; holding = false; pending = -1; }
                break;
            default:
                holding = false; break;
        }
    }

    inline void process (float& l, float& r)
    {
        const float m = 0.5f * (l + r);
        engine.pushInput (m);

        if (mode == Auto && autoArmed) detectOnset (std::max (std::fabs (l), std::fabs (r)));

        float wl = 0.0f, wr = 0.0f;
        if (mode == Live)
        {
            wl = l;  wr = r;
        }
        else if (engine.isActive())
        {
            const float tgt = holding ? 1.0f : 0.0f;
            g += (tgt > g ? attC : decC) * (tgt - g);
            if (! holding && g < 1.0e-4f) { g = 0.0f; engine.stop(); }
            else
            {
                float el, er;
                engine.process (el, er);
                wl = el * g;  wr = er * g;
            }
        }
        fx.process (wl, wr);
        const float d = dryS.next(), e = effS.next();
        l = l * d + wl * e;
        r = r * d + wr * e;
    }

private:
    float coef (float ms) const { return 1.0f - std::exp (-1.0f / (0.001f * ms * sr)); }

    inline void detectOnset (float x)
    {
        envF += (x > envF ? aF : rF) * (x - envF);
        envS += (x > envS ? aS : rS) * (x - envS);
        if (holdoff > 0) --holdoff;
        if (envF < thr * 0.5f) armed = true;
        if (pending > 0 && --pending == 0)
        {
            engine.capture (layer);
            holding = true;  pending = -1;
        }
        if (envF > thr && holdoff == 0 && (armed || envF > envS * 2.2f))
        {
            armed = false;
            holdoff = (int) (0.25f * sr);
            pending = (int) (0.18f * sr);        // on laisse la note s'installer avant de la figer
        }
    }

    float sr = 44100.0f, thr = 0.01f, layer = 0.5f, g = 0.0f, attC = 0.001f, decC = 0.001f;
    float aF = 0.1f, rF = 0.001f, aS = 0.001f, rS = 0.0001f, envF = 0.0f, envS = 0.0f;
    int mode = 0, holdoff = 0, pending = -1;
    bool holding = false, prevFreeze = true, autoArmed = false, armed = true;
    FreezeEngine engine;
    SuperegoFx fx;
    Smoother dryS, effS;
};

} // namespace rv
