#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ID
{
    // Global
    inline constexpr const char* inGain  = "inGain";
    inline constexpr const char* outGain = "outGain";
    inline constexpr const char* slot[6] = { "slot1", "slot2", "slot3", "slot4", "slot5", "slot6" };

    // Swell
    inline constexpr const char* swOn = "swOn", *swSens = "swSens", *swAttack = "swAttack",
                                 *swRelease = "swRelease", *swDepth = "swDepth", *swShape = "swShape";
    // Reverb 1
    inline constexpr const char* r1On = "r1On", *r1Size = "r1Size", *r1Decay = "r1Decay", *r1Damp = "r1Damp",
                                 *r1Pre = "r1Pre", *r1Mod = "r1Mod", *r1Mix = "r1Mix";
    // Reverb 2 (shimmer)
    inline constexpr const char* r2On = "r2On", *r2Size = "r2Size", *r2Decay = "r2Decay", *r2Damp = "r2Damp",
                                 *r2Pre = "r2Pre", *r2Mod = "r2Mod", *r2Mix = "r2Mix",
                                 *r2Shim = "r2Shim", *r2Int = "r2Int";
    // Delay
    inline constexpr const char* dlOn = "dlOn", *dlSync = "dlSync", *dlTimeL = "dlTimeL", *dlTimeR = "dlTimeR",
                                 *dlDivL = "dlDivL", *dlDivR = "dlDivR", *dlFbL = "dlFbL", *dlFbR = "dlFbR",
                                 *dlLevL = "dlLevL", *dlLevR = "dlLevR", *dlCross = "dlCross",
                                 *dlTone = "dlTone", *dlMod = "dlMod", *dlMix = "dlMix";
    // Quadravox
    inline constexpr const char* qdOn = "qdOn", *qdMix = "qdMix";
    inline constexpr const char* qdInt[4] = { "qdInt1", "qdInt2", "qdInt3", "qdInt4" };
    inline constexpr const char* qdLev[4] = { "qdLev1", "qdLev2", "qdLev3", "qdLev4" };
    inline constexpr const char* qdPan[4] = { "qdPan1", "qdPan2", "qdPan3", "qdPan4" };
    // Compresseur
    inline constexpr const char* cpOn = "cpOn", *cpThresh = "cpThresh", *cpRatio = "cpRatio", *cpAttack = "cpAttack",
                                 *cpRelease = "cpRelease", *cpMakeup = "cpMakeup", *cpMix = "cpMix";
}

namespace Params
{
    // Modules (ordre = valeurs du choix dans les 6 "slots")
    enum Module { Swell = 0, Reverb1, Shimmer, Delay, Quad, Comp, NumModules };

    inline juce::StringArray moduleNames() { return { "Swell", "Reverb", "Shimmer", "Delay", "Quadravox", "Compressor" }; }

    // Valeurs rythmiques pour la synchro tempo (en temps / noires)
    inline juce::StringArray divisionNames() { return { "1/16", "1/8T", "1/8", "1/8.", "1/4T", "1/4", "1/4.", "1/2", "1/2.", "1 bar" }; }
    inline constexpr float divisionBeats[10] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };

    inline juce::StringArray shimmerNames() { return { "+1 oct", "+5th", "+oct +5th", "-1 oct", "+2 oct" }; }
    inline constexpr float shimmerSemis[5] = { 12.0f, 7.0f, 19.0f, -12.0f, 24.0f };

    using PID = juce::ParameterID;

    inline std::unique_ptr<juce::AudioParameterFloat> F (const char* id, const char* name, float lo, float hi, float def,
                                                         const juce::String& unit = {}, int dec = 2, float skewCentre = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        auto attrs = juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([unit, dec] (float v, int)
                        {
                            return juce::String (v, dec) + (unit.isEmpty() ? juce::String() : " " + unit);
                        });
        return std::make_unique<juce::AudioParameterFloat> (PID { id, 1 }, name, r, def, attrs);
    }

    inline std::unique_ptr<juce::AudioParameterBool> B (const char* id, const char* name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (PID { id, 1 }, name, def);
    }

    inline std::unique_ptr<juce::AudioParameterChoice> C (const char* id, const char* name, const juce::StringArray& items, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (PID { id, 1 }, name, items, def);
    }

    inline std::unique_ptr<juce::AudioParameterInt> I (const char* id, const char* name, int lo, int hi, int def)
    {
        auto attrs = juce::AudioParameterIntAttributes()
                        .withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; });
        return std::make_unique<juce::AudioParameterInt> (PID { id, 1 }, name, lo, hi, def, attrs);
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout L;

        L.add (F (ID::inGain,  "Input",  -24.0f, 24.0f, 0.0f, "dB", 1));
        L.add (F (ID::outGain, "Output", -24.0f, 12.0f, 0.0f, "dB", 1));

        // Ordre par défaut : Swell > Quadravox > Delay > Reverb > Shimmer > Compresseur
        const int defaultOrder[6] = { Swell, Quad, Delay, Reverb1, Shimmer, Comp };
        for (int i = 0; i < 6; ++i)
            L.add (C (ID::slot[i], ("Slot " + juce::String (i + 1)).toRawUTF8(), moduleNames(), defaultOrder[i]));

        // Swell
        L.add (B (ID::swOn, "Swell On", true));
        L.add (F (ID::swSens,    "Sensitivity", -60.0f, -10.0f, -38.0f, "dB", 0));
        L.add (F (ID::swAttack,  "Attack",   20.0f, 4000.0f, 700.0f, "ms", 0, 600.0f));
        L.add (F (ID::swRelease, "Release",  20.0f, 3000.0f, 350.0f, "ms", 0, 400.0f));
        L.add (F (ID::swDepth,   "Depth",     0.0f, 1.0f, 1.0f, {}, 2));
        L.add (F (ID::swShape,   "Shape",    -1.0f, 1.0f, 0.0f, {}, 2));

        // Reverb 1
        L.add (B (ID::r1On, "Reverb On", true));
        L.add (F (ID::r1Size,  "R1 Size",  0.0f, 1.0f, 0.55f));
        L.add (F (ID::r1Decay, "R1 Decay", 0.3f, 20.0f, 3.5f, "s", 1, 3.0f));
        L.add (F (ID::r1Damp,  "R1 Damp",  0.0f, 1.0f, 0.4f));
        L.add (F (ID::r1Pre,   "R1 Pre-delay", 0.0f, 250.0f, 20.0f, "ms", 0));
        L.add (F (ID::r1Mod,   "R1 Mod",   0.0f, 1.0f, 0.3f));
        L.add (F (ID::r1Mix,   "R1 Mix",   0.0f, 1.0f, 0.35f));

        // Reverb 2 / Shimmer
        L.add (B (ID::r2On, "Shimmer On", true));
        L.add (F (ID::r2Size,  "R2 Size",  0.0f, 1.0f, 0.8f));
        L.add (F (ID::r2Decay, "R2 Decay", 0.3f, 20.0f, 7.0f, "s", 1, 4.0f));
        L.add (F (ID::r2Damp,  "R2 Damp",  0.0f, 1.0f, 0.45f));
        L.add (F (ID::r2Pre,   "R2 Pre-delay", 0.0f, 250.0f, 40.0f, "ms", 0));
        L.add (F (ID::r2Mod,   "R2 Mod",   0.0f, 1.0f, 0.5f));
        L.add (F (ID::r2Mix,   "R2 Mix",   0.0f, 1.0f, 0.3f));
        L.add (F (ID::r2Shim,  "Shimmer",  0.0f, 1.0f, 0.5f));
        L.add (C (ID::r2Int,   "Shimmer Interval", shimmerNames(), 0));

        // Delay stéréo
        L.add (B (ID::dlOn, "Delay On", true));
        L.add (B (ID::dlSync, "Delay Sync", false));
        L.add (F (ID::dlTimeL, "Time L", 20.0f, 2000.0f, 375.0f, "ms", 0, 450.0f));
        L.add (F (ID::dlTimeR, "Time R", 20.0f, 2000.0f, 560.0f, "ms", 0, 450.0f));
        L.add (C (ID::dlDivL,  "Division L", divisionNames(), 2));
        L.add (C (ID::dlDivR,  "Division R", divisionNames(), 3));
        L.add (F (ID::dlFbL,   "Feedback L", 0.0f, 1.0f, 0.45f));
        L.add (F (ID::dlFbR,   "Feedback R", 0.0f, 1.0f, 0.45f));
        L.add (F (ID::dlLevL,  "Level L", 0.0f, 1.0f, 0.8f));
        L.add (F (ID::dlLevR,  "Level R", 0.0f, 1.0f, 0.8f));
        L.add (F (ID::dlCross, "Cross", 0.0f, 1.0f, 0.2f));
        L.add (F (ID::dlTone,  "Tone",  0.0f, 1.0f, 0.55f));
        L.add (F (ID::dlMod,   "Mod",   0.0f, 1.0f, 0.2f));
        L.add (F (ID::dlMix,   "Delay Mix", 0.0f, 1.0f, 0.3f));

        // Quadravox
        L.add (B (ID::qdOn, "Quad On", true));
        L.add (F (ID::qdMix, "Quad Mix", 0.0f, 1.0f, 0.4f));
        const int   defInt[4] = { 12, 7, -12, 4 };
        const float defLev[4] = { 0.6f, 0.45f, 0.5f, 0.0f };
        const float defPan[4] = { -0.6f, 0.6f, 0.0f, 0.0f };
        for (int v = 0; v < 4; ++v)
        {
            L.add (I (ID::qdInt[v], ("Pitch " + juce::String (v + 1)).toRawUTF8(), -24, 24, defInt[v]));
            L.add (F (ID::qdLev[v], ("Level " + juce::String (v + 1)).toRawUTF8(), 0.0f, 1.0f, defLev[v]));
            L.add (F (ID::qdPan[v], ("Pan " + juce::String (v + 1)).toRawUTF8(), -1.0f, 1.0f, defPan[v]));
        }

        // Compresseur
        L.add (B (ID::cpOn, "Comp On", true));
        L.add (F (ID::cpThresh,  "Threshold", -60.0f, 0.0f, -20.0f, "dB", 1));
        L.add (F (ID::cpRatio,   "Ratio", 1.0f, 20.0f, 3.0f, ":1", 1, 4.0f));
        L.add (F (ID::cpAttack,  "Attack", 0.1f, 200.0f, 15.0f, "ms", 1, 20.0f));
        L.add (F (ID::cpRelease, "Release", 10.0f, 1500.0f, 200.0f, "ms", 0, 250.0f));
        L.add (F (ID::cpMakeup,  "Makeup", 0.0f, 24.0f, 3.0f, "dB", 1));
        L.add (F (ID::cpMix,     "Comp Mix", 0.0f, 1.0f, 1.0f));

        return L;
    }
}
