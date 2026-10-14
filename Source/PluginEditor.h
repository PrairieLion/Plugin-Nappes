#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
//  Palette inspirée d'Alphonse Mucha : parchemin, or patiné, vert olive, bleu-vert, rose poudré
// ---------------------------------------------------------------------------
namespace Pal
{
    const juce::Colour parchment  { 0xFFF3E4BE };
    const juce::Colour parchment2 { 0xFFE6CF98 };
    const juce::Colour paper      { 0xFFF8EDD0 };
    const juce::Colour gold       { 0xFFB8892B };
    const juce::Colour goldLight  { 0xFFDDB964 };
    const juce::Colour brown      { 0xFF4B3424 };
    const juce::Colour olive      { 0xFF7C8450 };
    const juce::Colour teal       { 0xFF4E8077 };
    const juce::Colour rose       { 0xFFC2705A };
    const juce::Colour plum       { 0xFF86566F };
    const juce::Colour sky        { 0xFF5F86A6 };
    const juce::Colour ochre      { 0xFFC79A3E };
}

class MuchaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MuchaLookAndFeel();
    static juce::Font serif (float height, bool bold = false, bool italic = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return serif (14.0f); }
    juce::Font getPopupMenuFont() override { return serif (15.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override { return serif (12.5f, true); }
};

// Dessin décoratif (cadre, arches, halos, fleurs) – mis en cache dans une image
class ArtComponent : public juce::Component
{
public:
    struct Panel { juce::Rectangle<float> r; juce::String title; juce::Colour accent; };
    ArtComponent() { setInterceptsMouseClicks (false, false); setBufferedToImage (true); }
    void setPanels (std::vector<Panel> p, juce::Rectangle<float> strip) { panels = std::move (p); stripRect = strip; repaint(); }
    void paint (juce::Graphics&) override;
private:
    std::vector<Panel> panels;
    juce::Rectangle<float> stripRect;
};

// Un bouton rotatif avec son étiquette et son attachement au paramètre
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& label, juce::Colour accent);
    void resized() override;
private:
    juce::Slider slider;
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& label);
    void resized() override;
    juce::ComboBox box;
private:
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class Gem : public juce::ToggleButton
{
public:
    Gem (juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& text, juce::Colour accent);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class ReverieEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverieEditor (ReverieProcessor&);
    ~ReverieEditor() override;
    void resized() override;

private:
    Knob* addKnob (const char* id, const juce::String& label, juce::Colour accent);
    Choice* addChoice (const char* id, const juce::String& label);
    Gem* addGem (const char* id, const juce::String& text, juce::Colour accent);

    ReverieProcessor& proc;
    MuchaLookAndFeel laf;
    ArtComponent art;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Choice> choices;
    juce::OwnedArray<Gem> gems;

    // pointeurs de mise en page
    Knob *inK, *outK;
    Choice* slot[6];
    Gem *swGem, *r1Gem, *r2Gem, *dlGem, *qdGem, *cpGem, *syncGem;
    Knob *swK[5], *r1K[6], *r2K[7];
    Choice* r2Int;
    Knob *dlK[10];
    Choice *dlDivL, *dlDivR;
    Knob *qdK[12], *qdMix;
    Knob *cpK[6];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverieEditor)
};
