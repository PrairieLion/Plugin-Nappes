#include "PluginProcessor.h"
#include "PluginEditor.h"

ReverieProcessor::ReverieProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", Params::createLayout())
{
}

bool ReverieProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Sortie stéréo ; entrée mono (guitare) ou stéréo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ReverieProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    swell.prepare (sr);
    reverb1.prepare (sr, 0);
    shimmer.prepare (sr);
    delay.prepare (sr);
    quad.prepare (sr);
    comp.prepare (sr);
    inSm.prepare (sr, 30.0f);   inSm.reset (rv::dbToGain (val (ID::inGain)));
    outSm.prepare (sr, 30.0f);  outSm.reset (rv::dbToGain (val (ID::outGain)));
    r1MixSm.prepare (sr, 30.0f); r1MixSm.reset (val (ID::r1Mix));
    r2MixSm.prepare (sr, 30.0f); r2MixSm.reset (val (ID::r2Mix));
}

void ReverieProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    if (getTotalNumInputChannels() == 1)            // guitare mono -> on duplique sur R
        std::copy (l, l + n, r);

    // --- tempo de l'hôte pour la synchro du delay ---------------------------------
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;

    // --- lecture des paramètres (une fois par bloc) ----------------------------------
    inSm.set  (rv::dbToGain (val (ID::inGain)));
    outSm.set (rv::dbToGain (val (ID::outGain)));
    r1MixSm.set (val (ID::r1Mix));
    r2MixSm.set (val (ID::r2Mix));

    swell.setParams (val (ID::swSens), val (ID::swAttack), val (ID::swRelease), val (ID::swDepth), val (ID::swShape));
    reverb1.setParams (val (ID::r1Size), val (ID::r1Decay), val (ID::r1Damp), val (ID::r1Mod), val (ID::r1Pre));
    shimmer.setParams (val (ID::r2Size), val (ID::r2Decay), val (ID::r2Damp), val (ID::r2Mod), val (ID::r2Pre),
                       val (ID::r2Shim), Params::shimmerSemis[juce::jlimit (0, 4, (int) val (ID::r2Int))]);

    float tL = val (ID::dlTimeL), tR = val (ID::dlTimeR);
    if (on (ID::dlSync))
    {
        const float msPerBeat = (float) (60000.0 / bpm);
        tL = msPerBeat * Params::divisionBeats[juce::jlimit (0, 9, (int) val (ID::dlDivL))];
        tR = msPerBeat * Params::divisionBeats[juce::jlimit (0, 9, (int) val (ID::dlDivR))];
    }
    delay.setParams (tL, tR, val (ID::dlFbL), val (ID::dlFbR), val (ID::dlLevL), val (ID::dlLevR),
                     val (ID::dlCross), val (ID::dlTone), val (ID::dlMod), val (ID::dlMix));

    float semis[4], lev[4], pan[4];
    for (int v = 0; v < 4; ++v) { semis[v] = val (ID::qdInt[v]); lev[v] = val (ID::qdLev[v]); pan[v] = val (ID::qdPan[v]); }
    quad.setParams (semis, lev, pan, val (ID::qdMix));

    comp.setParams (val (ID::cpThresh), val (ID::cpRatio), val (ID::cpAttack), val (ID::cpRelease),
                    val (ID::cpMakeup), val (ID::cpMix));

    // --- ordre de la chaîne : chaque module n'apparaît qu'une seule fois ---------------
    int order[Params::NumModules];
    bool used[Params::NumModules] {};
    int count = 0;
    for (int s = 0; s < 6; ++s)
    {
        const int m = juce::jlimit (0, Params::NumModules - 1, (int) val (ID::slot[s]));
        if (! used[m]) { used[m] = true; order[count++] = m; }
    }
    for (int m = 0; m < Params::NumModules; ++m)
        if (! used[m]) order[count++] = m;

    const bool enabled[Params::NumModules] = { on (ID::swOn), on (ID::r1On), on (ID::r2On),
                                               on (ID::dlOn), on (ID::qdOn), on (ID::cpOn) };
    if (enabled[Params::Swell]   && ! wasOn[Params::Swell])   swell.reset();
    if (enabled[Params::Reverb1] && ! wasOn[Params::Reverb1]) reverb1.reset();
    if (enabled[Params::Shimmer] && ! wasOn[Params::Shimmer]) shimmer.reset();
    if (enabled[Params::Delay]   && ! wasOn[Params::Delay])   delay.reset();
    if (enabled[Params::Quad]    && ! wasOn[Params::Quad])    quad.reset();
    for (int m = 0; m < Params::NumModules; ++m) wasOn[m] = enabled[m];

    // --- traitement ------------------------------------------------------------------
    for (int i = 0; i < n; ++i) { const float g = inSm.next(); l[i] *= g; r[i] *= g; }

    for (int k = 0; k < Params::NumModules; ++k)
    {
        const int m = order[k];
        if (! enabled[m]) continue;
        switch (m)
        {
            case Params::Swell:   for (int i = 0; i < n; ++i) swell.process (l[i], r[i]); break;
            case Params::Reverb1:
                for (int i = 0; i < n; ++i)
                {
                    float wl, wr, dry, wet;
                    reverb1.process (l[i], r[i], wl, wr);
                    rv::MixGains::get (r1MixSm.next(), dry, wet);
                    l[i] = l[i] * dry + wl * wet;
                    r[i] = r[i] * dry + wr * wet;
                }
                break;
            case Params::Shimmer:
                for (int i = 0; i < n; ++i)
                {
                    float wl, wr, dry, wet;
                    shimmer.process (l[i], r[i], wl, wr);
                    rv::MixGains::get (r2MixSm.next(), dry, wet);
                    l[i] = l[i] * dry + wl * wet;
                    r[i] = r[i] * dry + wr * wet;
                }
                break;
            case Params::Delay:   for (int i = 0; i < n; ++i) delay.process (l[i], r[i]); break;
            case Params::Quad:    for (int i = 0; i < n; ++i) quad.process (l[i], r[i]); break;
            case Params::Comp:    for (int i = 0; i < n; ++i) comp.process (l[i], r[i]); break;
            default: break;
        }
    }

    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.next();
        float a = l[i] * g, b = r[i] * g;
        if (! std::isfinite (a)) a = 0.0f;
        if (! std::isfinite (b)) b = 0.0f;
        l[i] = juce::jlimit (-8.0f, 8.0f, a);
        r[i] = juce::jlimit (-8.0f, 8.0f, b);
    }
}

juce::AudioProcessorEditor* ReverieProcessor::createEditor() { return new ReverieEditor (*this); }

void ReverieProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ReverieProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReverieProcessor(); }
