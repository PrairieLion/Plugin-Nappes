#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "DSP.h"

class ReverieProcessor : public juce::AudioProcessor
{
public:
    ReverieProcessor();
    ~ReverieProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Reverie"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }
    float val (const char* id) const { return raw (id)->load(); }
    bool  on  (const char* id) const { return raw (id)->load() > 0.5f; }

    rv::Swell swell;
    rv::Reverb reverb1;
    rv::ShimmerReverb shimmer;
    rv::StereoDelay delay;
    rv::Quadravox quad;
    rv::Compressor comp;
    rv::Smoother inSm, outSm, r1MixSm, r2MixSm;

    bool wasOn[Params::NumModules] { true, true, true, true, true, true };
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverieProcessor)
};
