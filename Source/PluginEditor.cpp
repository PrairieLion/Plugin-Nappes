#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int kW = 1500, kH = 1030, kMargin = 22;
    constexpr int kStripLabelW = 160, kStripComboW = 130, kStripGap = 28;

    String pickSerifName()
    {
        const auto names = Font::findAllTypefaceNames();
        for (auto n : { "Palatino Linotype", "Palatino", "Book Antiqua", "Garamond", "Georgia" })
            if (names.contains (n)) return n;
        return Font::getDefaultSerifFontName();
    }

    // Feuille / pétale en forme d'amande, pointe en (0,0), longueur len vers +x
    Path leafPath (float len, float width)
    {
        Path p;
        p.startNewSubPath (0.0f, 0.0f);
        p.quadraticTo (len * 0.5f, -width, len, 0.0f);
        p.quadraticTo (len * 0.5f,  width, 0.0f, 0.0f);
        p.closeSubPath();
        return p;
    }

    void drawFlower (Graphics& g, Point<float> c, float r, Colour petal, Colour centre, int petals = 8)
    {
        const auto leaf = leafPath (r, r * 0.34f);
        for (int i = 0; i < petals; ++i)
        {
            const auto t = AffineTransform::rotation (MathConstants<float>::twoPi * (float) i / (float) petals).translated (c);
            g.setColour (petal.withAlpha (0.85f));
            g.fillPath (leaf, t);
            g.setColour (Pal::brown.withAlpha (0.7f));
            g.strokePath (leaf, PathStrokeType (0.8f), t);
        }
        g.setColour (centre);
        g.fillEllipse (c.x - r * 0.22f, c.y - r * 0.22f, r * 0.44f, r * 0.44f);
        g.setColour (Pal::brown.withAlpha (0.8f));
        g.drawEllipse (c.x - r * 0.22f, c.y - r * 0.22f, r * 0.44f, r * 0.44f, 0.8f);
    }

    // Courbe "coup de fouet" typique de l'Art nouveau
    Path whiplash()
    {
        Path p;
        p.startNewSubPath (0.0f, 0.0f);
        p.cubicTo (110.0f, -22.0f, 200.0f, 28.0f, 300.0f, 8.0f);
        p.cubicTo (345.0f, -2.0f, 368.0f, -32.0f, 340.0f, -44.0f);
        p.cubicTo (316.0f, -52.0f, 298.0f, -28.0f, 318.0f, -20.0f);
        return p;
    }

    Path archPath (Rectangle<float> r, float archH, float rad = 10.0f)
    {
        Path p;
        const float x = r.getX(), y = r.getY(), w = r.getWidth(), b = r.getBottom();
        p.startNewSubPath (x, y + archH);
        p.quadraticTo (x + w * 0.5f, y - archH, x + w, y + archH);
        p.lineTo (x + w, b - rad);
        p.quadraticTo (x + w, b, x + w - rad, b);
        p.lineTo (x + rad, b);
        p.quadraticTo (x, b, x, b - rad);
        p.closeSubPath();
        return p;
    }

    void drawHalo (Graphics& g, Point<float> c, float r, Colour tint)
    {
        g.setColour (Pal::paper);
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (tint.withAlpha (0.22f));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (Pal::gold);
        g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 2.2f);
        g.setColour (Pal::brown.withAlpha (0.6f));
        g.drawEllipse (c.x - r - 6, c.y - r - 6, 2 * r + 12, 2 * r + 12, 0.9f);
        const int dots = 40;
        for (int i = 0; i < dots; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / (float) dots;
            const float rr = r + 13.0f;
            g.setColour (i % 2 ? Pal::gold : tint.withAlpha (0.8f));
            g.fillEllipse (c.x + std::sin (a) * rr - 2.0f, c.y - std::cos (a) * rr - 2.0f, 4.0f, 4.0f);
        }
    }
}

// ============================================================================
//  LookAndFeel
// ============================================================================
Font MuchaLookAndFeel::serif (float height, bool bold, bool italic)
{
    static const String name = pickSerifName();
    int flags = Font::plain;
    if (bold) flags |= Font::bold;
    if (italic) flags |= Font::italic;
    return Font (FontOptions (name, height, flags));
}

MuchaLookAndFeel::MuchaLookAndFeel()
{
    setColour (ComboBox::textColourId, Pal::brown);
    setColour (PopupMenu::backgroundColourId, Pal::paper);
    setColour (PopupMenu::textColourId, Pal::brown);
    setColour (PopupMenu::highlightedBackgroundColourId, Pal::goldLight);
    setColour (PopupMenu::highlightedTextColourId, Pal::brown);
    setColour (Label::textColourId, Pal::brown);
    setColour (TextEditor::backgroundColourId, Pal::paper);
    setColour (TextEditor::textColourId, Pal::brown);
    setColour (TextEditor::highlightColourId, Pal::goldLight);
    setColour (CaretComponent::caretColourId, Pal::brown);
}

void MuchaLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const float size = (float) jmin (w, h);
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float R = size * 0.5f - 3.0f;
    const float body = R * 0.70f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // piste + arc de valeur
    Path track;  track.addCentredArc (cx, cy, R - 1.0f, R - 1.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::brown.withAlpha (0.22f));
    g.strokePath (track, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val;    val.addCentredArc (cx, cy, R - 1.0f, R - 1.0f, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (val, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));

    // ombre
    g.setColour (Pal::brown.withAlpha (0.25f));
    g.fillEllipse (cx - body + 1.5f, cy - body + 2.5f, body * 2, body * 2);
    // lunette dorée
    g.setGradientFill (ColourGradient (Pal::goldLight, cx - body, cy - body, Pal::gold.darker (0.3f), cx + body, cy + body, false));
    g.fillEllipse (cx - body, cy - body, body * 2, body * 2);
    // plateau parchemin
    const float inner = body * 0.82f;
    g.setGradientFill (ColourGradient (Pal::paper, cx - inner * 0.4f, cy - inner * 0.5f, Pal::parchment2, cx + inner, cy + inner, true));
    g.fillEllipse (cx - inner, cy - inner, inner * 2, inner * 2);
    g.setColour (Pal::brown.withAlpha (0.8f));
    g.drawEllipse (cx - inner, cy - inner, inner * 2, inner * 2, 1.0f);
    g.drawEllipse (cx - body, cy - body, body * 2, body * 2, 1.0f);

    // aiguille
    Path needle;
    needle.addRoundedRectangle (-1.4f, -inner * 0.92f, 2.8f, inner * 0.55f, 1.2f);
    g.setColour (Pal::brown);
    g.fillPath (needle, AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (accent);
    g.fillEllipse (cx - 2.4f, cy - 2.4f, 4.8f, 4.8f);
}

Label* MuchaLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (serif (12.5f));
    l->setJustificationType (Justification::centred);
    l->setColour (Label::textColourId, Pal::brown);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundWhenEditingColourId, Pal::paper);
    l->setColour (Label::textWhenEditingColourId, Pal::brown);
    return l;
}

void MuchaLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool isDown, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (isDown ? Pal::parchment2 : Pal::paper);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Pal::gold);
    g.drawRoundedRectangle (r, 6.0f, 1.8f);
    g.setColour (Pal::brown.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (3.0f), 4.0f, 0.7f);
    Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (box.isEnabled() ? Pal::brown : Pal::brown.withAlpha (0.4f));
    g.fillPath (arrow);
}

void MuchaLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (9, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void MuchaLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto accent = b.findColour (ToggleButton::tickColourId);
    auto area = b.getLocalBounds().toFloat();
    const float d = jmin (area.getHeight(), 26.0f);
    Rectangle<float> gem (area.getX() + 1.0f, area.getCentreY() - d * 0.5f, d, d);
    const bool on = b.getToggleState();

    g.setColour (Pal::brown.withAlpha (0.25f));
    g.fillEllipse (gem.translated (1.0f, 1.5f));
    g.setGradientFill (ColourGradient (Pal::goldLight, gem.getX(), gem.getY(), Pal::gold.darker (0.3f), gem.getRight(), gem.getBottom(), false));
    g.fillEllipse (gem);
    auto in = gem.reduced (3.2f);
    if (on)
        g.setGradientFill (ColourGradient (accent.brighter (0.6f), in.getX() + in.getWidth() * 0.3f, in.getY() + in.getHeight() * 0.3f,
                                           accent.darker (0.3f), in.getRight(), in.getBottom(), true));
    else
        g.setColour (Pal::parchment2);
    g.fillEllipse (in);
    g.setColour (Pal::brown.withAlpha (highlighted ? 1.0f : 0.8f));
    g.drawEllipse (gem, 1.0f);
    g.drawEllipse (in, 0.7f);

    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (Pal::brown);
        g.setFont (serif (13.0f, true));
        g.drawText (b.getButtonText(), area.withTrimmedLeft (d + 6.0f).toNearestInt(), Justification::centredLeft);
    }
}

void MuchaLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState() || down;
    const auto accent = b.findColour (TextButton::buttonOnColourId);
    g.setColour (Pal::brown.withAlpha (0.25f));
    g.fillRoundedRectangle (r.translated (1.0f, 2.0f), 9.0f);
    if (on)
        g.setGradientFill (ColourGradient (accent.brighter (0.5f), r.getX(), r.getY(), accent.darker (0.25f), r.getX(), r.getBottom(), false));
    else
        g.setGradientFill (ColourGradient (Pal::paper, r.getX(), r.getY(), Pal::parchment2, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (Pal::gold);
    g.drawRoundedRectangle (r, 9.0f, highlighted ? 2.6f : 2.0f);
    g.setColour (Pal::brown.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (3.0f), 7.0f, 0.7f);
}

void MuchaLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    g.setFont (serif (14.0f, true).withExtraKerningFactor (0.12f));
    g.setColour (on ? Pal::paper : Pal::brown);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

// ============================================================================
//  Petits composants
// ============================================================================
Knob::Knob (AudioProcessorValueTreeState& apvts, const char* paramID, const String& label, Colour accent)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (1.25f * MathConstants<float>::pi, 2.75f * MathConstants<float>::pi, true);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setColour (Slider::textBoxTextColourId, Pal::brown);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxHighlightColourId, Pal::goldLight);
    name.setText (label, dontSendNotification);
    name.setJustificationType (Justification::centred);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (name);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (15));
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmax (40, getWidth() - 4), 16);
    slider.setBounds (r);
}

Choice::Choice (AudioProcessorValueTreeState& apvts, const char* paramID, const String& label)
{
    if (auto* p = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (paramID)))
        box.addItemList (p->choices, 1);
    name.setText (label, dontSendNotification);
    name.setJustificationType (Justification::centred);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (box);
    addAndMakeVisible (name);
    att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (name.getText().isEmpty())
    {
        name.setBounds ({});
        box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 2, 28));
        return;
    }
    name.setBounds (r.removeFromTop (15));
    box.setBounds (r.withSizeKeepingCentre (jmin (r.getWidth() - 6, 96), 28));
}

Gem::Gem (AudioProcessorValueTreeState& apvts, const char* paramID, const String& caption, Colour accent)
{
    setButtonText (caption);
    setColour (ToggleButton::tickColourId, accent);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, *this);
}

FreezeButton::FreezeButton (AudioProcessorValueTreeState& apvts, const char* paramID, Colour accent, std::function<bool()> isMomentary)
    : TextButton ("FREEZE"), momentary (std::move (isMomentary))
{
    setClickingTogglesState (false);
    setColour (TextButton::buttonOnColourId, accent);
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramID, *this);
}

void FreezeButton::mouseDown (const MouseEvent&)
{
    if (! isEnabled()) return;
    if (momentary()) setToggleState (true, sendNotification);
    else             setToggleState (! getToggleState(), sendNotification);
}

void FreezeButton::mouseUp (const MouseEvent&)
{
    if (isEnabled() && momentary()) setToggleState (false, sendNotification);
}

// ---------------------------------------------------------------------------
EqCurve::EqCurve (AudioProcessorValueTreeState& apvts, Colour accent) : state (apvts), colour (accent)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (15);
}

float EqCurve::readSum() const
{
    float s = 0.0f, k = 1.0f;
    for (auto id : { ID::eqHp, ID::eqLsF, ID::eqLsG, ID::eq1F, ID::eq1G, ID::eq1Q, ID::eq2F, ID::eq2G, ID::eq2Q,
                     ID::eqHsF, ID::eqHsG, ID::eqLp })
    {
        s += state.getRawParameterValue (id)->load() * k;
        k *= 1.37f;
    }
    return s;
}

void EqCurve::timerCallback()
{
    const float s = readSum();
    if (s != lastSum) { lastSum = s; repaint(); }
}

void EqCurve::paint (Graphics& g)
{
    auto v = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    const float fs = 48000.0f;
    std::vector<rv::BiquadCoeffs> stages;
    if (v (ID::eqHp) > 21.0f)            stages.push_back (rv::BiquadCoeffs::highpass (fs, v (ID::eqHp)));
    if (std::fabs (v (ID::eqLsG)) > 0.05f) stages.push_back (rv::BiquadCoeffs::lowShelf (fs, v (ID::eqLsF), v (ID::eqLsG)));
    if (std::fabs (v (ID::eq1G)) > 0.05f)  stages.push_back (rv::BiquadCoeffs::peak (fs, v (ID::eq1F), v (ID::eq1G), v (ID::eq1Q)));
    if (std::fabs (v (ID::eq2G)) > 0.05f)  stages.push_back (rv::BiquadCoeffs::peak (fs, v (ID::eq2F), v (ID::eq2G), v (ID::eq2Q)));
    if (std::fabs (v (ID::eqHsG)) > 0.05f) stages.push_back (rv::BiquadCoeffs::highShelf (fs, v (ID::eqHsF), v (ID::eqHsG)));
    if (v (ID::eqLp) < 19500.0f)         stages.push_back (rv::BiquadCoeffs::lowpass (fs, v (ID::eqLp)));

    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Pal::paper);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Pal::gold);
    g.drawRoundedRectangle (r, 8.0f, 1.8f);

    const auto a = r.reduced (10.0f, 10.0f);
    auto xOf = [&] (float f) { return a.getX() + a.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    auto yOf = [&] (float db) { return a.getCentreY() - jlimit (-20.0f, 20.0f, db) / 18.0f * a.getHeight() * 0.5f; };

    g.setColour (Pal::brown.withAlpha (0.13f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine ((int) xOf (f), a.getY(), a.getBottom());
    for (float db : { -12.0f, -6.0f, 6.0f, 12.0f })
        g.drawHorizontalLine ((int) yOf (db), a.getX(), a.getRight());
    g.setColour (Pal::brown.withAlpha (0.45f));
    g.drawHorizontalLine ((int) yOf (0.0f), a.getX(), a.getRight());

    g.setFont (MuchaLookAndFeel::serif (10.5f));
    g.setColour (Pal::brown.withAlpha (0.7f));
    const float lblF[3] = { 100.0f, 1000.0f, 10000.0f };
    const char* lblT[3] = { "100", "1k", "10k" };
    for (int i = 0; i < 3; ++i)
        g.drawText (lblT[i], Rectangle<float> (xOf (lblF[i]) - 16.0f, a.getBottom() - 12.0f, 32.0f, 12.0f), Justification::centred);

    Path curve;
    const int steps = 160;
    for (int i = 0; i < steps; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / (float) (steps - 1));
        float db = 0.0f;
        for (auto& s : stages) db += s.magnitudeDb (fs, f);
        const float x = xOf (f), y = yOf (db);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    Path fill (curve);
    fill.lineTo (a.getRight(), yOf (0.0f));
    fill.lineTo (a.getX(), yOf (0.0f));
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.22f));
    g.fillPath (fill);
    g.setColour (colour.darker (0.2f));
    g.strokePath (curve, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ============================================================================
//  Décor
// ============================================================================
void ArtComponent::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // fond parchemin
    g.setGradientFill (ColourGradient (Pal::parchment.brighter (0.05f), 0, 0, Pal::parchment2, 0, b.getBottom(), false));
    g.fillAll();

    // grands cercles décoratifs en filigrane (clin d'oeil aux calendriers de Mucha)
    for (int i = 0; i < 6; ++i)
    {
        g.setColour (Pal::gold.withAlpha (0.07f));
        const float r = 230.0f + 90.0f * (float) i;
        g.drawEllipse (b.getCentreX() - r, 60.0f - r, 2 * r, 2 * r, 1.2f);
    }

    // cadre
    g.setColour (Pal::gold);
    g.drawRoundedRectangle (b.reduced (8.0f), 12.0f, 3.0f);
    g.setColour (Pal::brown.withAlpha (0.7f));
    g.drawRoundedRectangle (b.reduced (14.0f), 9.0f, 1.0f);
    for (auto c : { Point<float> (14, 14), Point<float> (b.getRight() - 14, 14),
                    Point<float> (14, b.getBottom() - 14), Point<float> (b.getRight() - 14, b.getBottom() - 14) })
        drawFlower (g, c, 12.0f, Pal::rose, Pal::gold, 8);

    // en-tête : halo + titre + volutes
    const Point<float> hc (b.getCentreX(), 62.0f);
    const auto whip = whiplash();
    for (int side = 0; side < 2; ++side)
    {
        const bool left = side == 0;
        const float sx = left ? -1.0f : 1.0f;
        auto t = AffineTransform::scale (sx, 1.0f).translated (hc.x + sx * 110.0f, hc.y + 2.0f);
        g.setColour (Pal::gold);
        g.strokePath (whip, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded), t);
        g.setColour (Pal::olive);
        g.strokePath (whip, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded), t.translated (0, 4));
        const auto end = Point<float> (318.0f, -20.0f).transformedBy (t);
        drawFlower (g, end, 10.0f, side ? Pal::teal : Pal::rose, Pal::ochre, 7);
        auto leaf = leafPath (34.0f, 8.0f);
        g.setColour (Pal::olive.withAlpha (0.9f));
        g.fillPath (leaf, AffineTransform::rotation (left ? 2.6f : 0.55f).translated (hc.x + sx * 250.0f, hc.y + 14.0f));
        g.fillPath (leaf, AffineTransform::rotation (left ? 3.7f : -0.55f).translated (hc.x + sx * 300.0f, hc.y - 8.0f));
    }
    drawHalo (g, hc, 50.0f, Pal::teal);
    g.setColour (Pal::brown);
    g.setFont (MuchaLookAndFeel::serif (44.0f, true).withExtraKerningFactor (0.16f));
    g.drawText ("REVERIE", Rectangle<float> (hc.x - 180, hc.y - 30, 360, 50), Justification::centred);
    g.setFont (MuchaLookAndFeel::serif (13.5f, false, true).withExtraKerningFactor (0.1f));
    g.setColour (Pal::brown.withAlpha (0.85f));
    g.drawText ("ambient guitar atelier", Rectangle<float> (hc.x - 120, hc.y + 16, 240, 18), Justification::centred);

    // bandeau "chemin du signal"
    if (! stripRect.isEmpty())
    {
        g.setColour (Pal::paper.withAlpha (0.65f));
        g.fillRoundedRectangle (stripRect, 14.0f);
        g.setColour (Pal::gold);
        g.drawRoundedRectangle (stripRect, 14.0f, 1.8f);
        g.setColour (Pal::brown);
        g.setFont (MuchaLookAndFeel::serif (15.0f, true).withExtraKerningFactor (0.12f));
        g.drawText ("SIGNAL PATH", stripRect.withTrimmedLeft (22.0f).withWidth ((float) kStripLabelW - 22.0f), Justification::centredLeft);
        for (int i = 1; i < Params::NumModules; ++i)
        {
            const float cx = stripRect.getX() + (float) kStripLabelW + (float) (i * kStripComboW + (i - 1) * kStripGap) + (float) kStripGap * 0.5f;
            Path arrow;
            arrow.addTriangle (cx - 4.0f, stripRect.getCentreY() - 6.0f, cx - 4.0f, stripRect.getCentreY() + 6.0f, cx + 6.0f, stripRect.getCentreY());
            g.setColour (Pal::gold);
            g.fillPath (arrow);
        }
    }

    // panneaux en arche
    for (auto& p : panels)
    {
        const auto outer = archPath (p.r, 20.0f);
        g.setColour (Pal::brown.withAlpha (0.20f));
        g.fillPath (outer, AffineTransform::translation (3.0f, 4.0f));
        g.setGradientFill (ColourGradient (Pal::paper.brighter (0.15f), 0, p.r.getY(),
                                           Pal::paper.interpolatedWith (p.accent, 0.16f), 0, p.r.getBottom(), false));
        g.fillPath (outer);
        g.setColour (Pal::gold);
        g.strokePath (outer, PathStrokeType (2.6f));
        g.setColour (Pal::brown.withAlpha (0.55f));
        g.strokePath (archPath (p.r.reduced (6.0f), 16.0f, 7.0f), PathStrokeType (0.9f));

        const float cx = p.r.getCentreX(), ty = p.r.getY();
        // halo derrière le titre
        g.setColour (p.accent.withAlpha (0.28f));
        g.fillEllipse (cx - 30.0f, ty + 8.0f, 60.0f, 44.0f);
        g.setColour (Pal::gold);
        g.drawEllipse (cx - 30.0f, ty + 8.0f, 60.0f, 44.0f, 1.4f);
        drawFlower (g, { p.r.getX() + 30.0f, ty + 34.0f }, 10.0f, p.accent, Pal::ochre, 7);
        g.setColour (Pal::brown);
        g.setFont (MuchaLookAndFeel::serif (20.0f, true).withExtraKerningFactor (0.14f));
        g.drawText (p.title, Rectangle<float> (cx - 150, ty + 14, 300, 32), Justification::centred);

        // filet ondulé sous l'en-tête
        Path wave;
        const float wy = ty + 54.0f;
        wave.startNewSubPath (p.r.getX() + 16.0f, wy);
        for (float x = p.r.getX() + 16.0f; x < p.r.getRight() - 16.0f; x += 4.0f)
            wave.lineTo (x, wy + std::sin ((x - p.r.getX()) * 0.09f) * 2.0f);
        g.setColour (Pal::gold.withAlpha (0.9f));
        g.strokePath (wave, PathStrokeType (1.4f));
        g.setColour (p.accent);
        Path dia;  dia.addQuadrilateral (cx - 6, wy, cx, wy - 5, cx + 6, wy, cx, wy + 5);
        g.fillPath (dia);
    }
}

// ============================================================================
//  Editeur
// ============================================================================
Knob* ReverieEditor::addKnob (const char* id, const String& label, Colour accent, int module)
{
    auto* k = knobs.add (new Knob (proc.apvts, id, label, accent));
    content.addAndMakeVisible (k);
    if (module >= 0) groups[module].push_back (k);
    return k;
}
Choice* ReverieEditor::addChoice (const char* id, const String& label, int module)
{
    auto* c = choices.add (new Choice (proc.apvts, id, label));
    content.addAndMakeVisible (c);
    if (module >= 0) groups[module].push_back (c);
    return c;
}
Gem* ReverieEditor::addGem (const char* id, const String& text, Colour accent)
{
    auto* g = gems.add (new Gem (proc.apvts, id, text, accent));
    content.addAndMakeVisible (g);
    return g;
}

ReverieEditor::ReverieEditor (ReverieProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    using M = Params::Module;
    for (auto& e : lastEnabled) e = -1;
    setLookAndFeel (&laf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (art);

    inK  = addKnob (ID::inGain,  "Input",  Pal::gold, -1);
    outK = addKnob (ID::outGain, "Output", Pal::gold, -1);
    for (int i = 0; i < Params::NumModules; ++i) slot[i] = addChoice (ID::slot[i], {});

    // Swell
    swGem = addGem (ID::swOn, "ON", Pal::olive);
    swK[0] = addKnob (ID::swSens,    "Sensitivity", Pal::olive, M::Swell);
    swK[1] = addKnob (ID::swAttack,  "Attack",      Pal::olive, M::Swell);
    swK[2] = addKnob (ID::swRelease, "Release",     Pal::olive, M::Swell);
    swK[3] = addKnob (ID::swDepth,   "Depth",       Pal::olive, M::Swell);
    swK[4] = addKnob (ID::swShape,   "Shape",       Pal::olive, M::Swell);

    // Reverb 1
    r1Gem = addGem (ID::r1On, "ON", Pal::teal);
    r1K[0] = addKnob (ID::r1Size,  "Size",  Pal::teal, M::Reverb1);
    r1K[1] = addKnob (ID::r1Decay, "Decay", Pal::teal, M::Reverb1);
    r1K[2] = addKnob (ID::r1Damp,  "Damp",  Pal::teal, M::Reverb1);
    r1K[3] = addKnob (ID::r1Pre,   "Pre-delay", Pal::teal, M::Reverb1);
    r1K[4] = addKnob (ID::r1Mod,   "Mod",   Pal::teal, M::Reverb1);
    r1K[5] = addKnob (ID::r1Mix,   "Mix",   Pal::teal, M::Reverb1);

    // Shimmer
    r2Gem = addGem (ID::r2On, "ON", Pal::rose);
    r2K[0] = addKnob (ID::r2Size,  "Size",  Pal::rose, M::Shimmer);
    r2K[1] = addKnob (ID::r2Decay, "Decay", Pal::rose, M::Shimmer);
    r2K[2] = addKnob (ID::r2Damp,  "Damp",  Pal::rose, M::Shimmer);
    r2K[3] = addKnob (ID::r2Pre,   "Pre-delay", Pal::rose, M::Shimmer);
    r2K[4] = addKnob (ID::r2Mod,   "Mod",   Pal::rose, M::Shimmer);
    r2K[5] = addKnob (ID::r2Mix,   "Mix",   Pal::rose, M::Shimmer);
    r2K[6] = addKnob (ID::r2Shim,  "Shimmer", Pal::ochre, M::Shimmer);
    r2Int  = addChoice (ID::r2Int, "Interval", M::Shimmer);

    // Superego
    sgGem = addGem (ID::sgOn, "ON", Pal::plum);
    sgMode = addChoice (ID::sgMode, "Mode", M::Superego);
    sgFx   = addChoice (ID::sgFx,   "FX",   M::Superego);
    sgK[0] = addKnob (ID::sgAttack, "Attack",    Pal::plum, M::Superego);
    sgK[1] = addKnob (ID::sgDecay,  "Decay",     Pal::plum, M::Superego);
    sgK[2] = addKnob (ID::sgThresh, "Threshold", Pal::plum, M::Superego);
    sgK[3] = addKnob (ID::sgLayer,  "Layer",     Pal::plum, M::Superego);
    sgK[4] = addKnob (ID::sgGlide,  "Glide",     Pal::plum, M::Superego);
    sgK[5] = addKnob (ID::sgRate,   "FX Rate",   Pal::ochre, M::Superego);
    sgK[6] = addKnob (ID::sgDepth,  "FX Depth",  Pal::ochre, M::Superego);
    sgK[7] = addKnob (ID::sgDry,    "Dry",       Pal::plum, M::Superego);
    sgK[8] = addKnob (ID::sgEffect, "Effect",    Pal::plum, M::Superego);
    freezeBtn = std::make_unique<FreezeButton> (proc.apvts, ID::sgFreeze, Pal::sky,
                    [this] { return (int) proc.apvts.getRawParameterValue (ID::sgMode)->load() == (int) rv::Superego::Moment; });
    content.addAndMakeVisible (*freezeBtn);
    groups[M::Superego].push_back (freezeBtn.get());
    clearBtn.setColour (TextButton::buttonOnColourId, Pal::rose);
    clearBtn.onClick = [this] { proc.requestSuperegoClear(); };
    content.addAndMakeVisible (clearBtn);
    groups[M::Superego].push_back (&clearBtn);

    // Delay
    dlGem   = addGem (ID::dlOn, "ON", Pal::plum);
    syncGem = addGem (ID::dlSync, "SYNC", Pal::plum);
    dlK[0] = addKnob (ID::dlTimeL, "Time L",  Pal::plum, M::Delay);
    dlK[1] = addKnob (ID::dlFbL,   "Feedback L", Pal::plum, M::Delay);
    dlK[2] = addKnob (ID::dlLevL,  "Level L", Pal::plum, M::Delay);
    dlK[3] = addKnob (ID::dlTimeR, "Time R",  Pal::sky, M::Delay);
    dlK[4] = addKnob (ID::dlFbR,   "Feedback R", Pal::sky, M::Delay);
    dlK[5] = addKnob (ID::dlLevR,  "Level R", Pal::sky, M::Delay);
    dlK[6] = addKnob (ID::dlCross, "Cross",   Pal::plum, M::Delay);
    dlK[7] = addKnob (ID::dlTone,  "Tone",    Pal::plum, M::Delay);
    dlK[8] = addKnob (ID::dlMod,   "Mod",     Pal::plum, M::Delay);
    dlK[9] = addKnob (ID::dlMix,   "Mix",     Pal::plum, M::Delay);
    dlDivL = addChoice (ID::dlDivL, "Div L", M::Delay);
    dlDivR = addChoice (ID::dlDivR, "Div R", M::Delay);

    // Quadravox
    qdGem = addGem (ID::qdOn, "ON", Pal::sky);
    for (int v = 0; v < 4; ++v)
    {
        const String n = String (v + 1);
        qdK[v * 3 + 0] = addKnob (ID::qdInt[v], "Pitch " + n, Pal::sky, M::Quad);
        qdK[v * 3 + 1] = addKnob (ID::qdLev[v], "Level " + n, Pal::sky, M::Quad);
        qdK[v * 3 + 2] = addKnob (ID::qdPan[v], "Pan " + n,   Pal::sky, M::Quad);
    }
    qdMix = addKnob (ID::qdMix, "Mix", Pal::sky, M::Quad);

    // Egaliseur
    eqGem = addGem (ID::eqOn, "ON", Pal::teal);
    const char* eqIds[12]    = { ID::eqHp, ID::eqLsF, ID::eqLsG, ID::eq1F, ID::eq1G, ID::eq1Q, ID::eq2F, ID::eq2G, ID::eq2Q, ID::eqHsF, ID::eqHsG, ID::eqLp };
    const char* eqLabels[12] = { "Low Cut", "Low Freq", "Low Gain", "Mid1 Freq", "Mid1 Gain", "Mid1 Q", "Mid2 Freq", "Mid2 Gain", "Mid2 Q", "High Freq", "High Gain", "High Cut" };
    for (int i = 0; i < 12; ++i) eqK[i] = addKnob (eqIds[i], eqLabels[i], Pal::teal, M::EQ);
    eqCurve = std::make_unique<EqCurve> (proc.apvts, Pal::teal);
    content.addAndMakeVisible (*eqCurve);
    groups[M::EQ].push_back (eqCurve.get());

    // Compresseur
    cpGem = addGem (ID::cpOn, "ON", Pal::ochre);
    cpK[0] = addKnob (ID::cpThresh,  "Threshold", Pal::ochre, M::Comp);
    cpK[1] = addKnob (ID::cpRatio,   "Ratio",     Pal::ochre, M::Comp);
    cpK[2] = addKnob (ID::cpAttack,  "Attack",    Pal::ochre, M::Comp);
    cpK[3] = addKnob (ID::cpRelease, "Release",   Pal::ochre, M::Comp);
    cpK[4] = addKnob (ID::cpMakeup,  "Makeup",    Pal::ochre, M::Comp);
    cpK[5] = addKnob (ID::cpMix,     "Mix",       Pal::ochre, M::Comp);

    layoutContent();

    // fenêtre redimensionnable (proportions conservées)
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setResizeLimits (kW / 2, kH / 2, kW * 3 / 2, kH * 3 / 2);
    setSize ((int) (kW * 0.75), (int) (kH * 0.75));   // en dernier : déclenche resized() une fois tout créé
    startTimerHz (10);
}

ReverieEditor::~ReverieEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ReverieEditor::timerCallback()
{
    const char* onIds[Params::NumModules] = { ID::swOn, ID::r1On, ID::r2On, ID::sgOn, ID::dlOn, ID::qdOn, ID::eqOn, ID::cpOn };
    for (int m = 0; m < Params::NumModules; ++m)
    {
        const int en = proc.apvts.getRawParameterValue (onIds[m])->load() > 0.5f ? 1 : 0;
        if (en == lastEnabled[m]) continue;
        lastEnabled[m] = en;
        for (auto* c : groups[m]) c->setAlpha (en ? 1.0f : 0.4f);   // module coupé : commandes estompées
    }
}

void ReverieEditor::resized()
{
    content.setTransform ({});
    content.setBounds (0, 0, kW, kH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) kW));
}

void ReverieEditor::layoutContent()
{
    art.setBounds (0, 0, kW, kH);
    inK->setBounds  (40, 24, 84, 78);
    outK->setBounds (kW - 40 - 84, 24, 84, 78);

    // bandeau de l'ordre des modules
    const Rectangle<int> strip (kMargin, 106, kW - 2 * kMargin, 36);
    for (int i = 0; i < Params::NumModules; ++i)
        slot[i]->setBounds (strip.getX() + kStripLabelW + i * (kStripComboW + kStripGap), strip.getY() + 1, kStripComboW, strip.getHeight() - 2);

    // trois rangées de panneaux
    const int gap = 16, hA = 270, hB = 270, hC = 292;
    const int yA = 150, yB = yA + hA + 12, yC = yB + hB + 12;
    auto split = [&] (int total, std::initializer_list<float> fr)
    {
        std::vector<int> w;  int used = 0, i = 0;
        const int n = (int) fr.size();
        const int usable = total - (n - 1) * gap;
        for (float f : fr) { ++i; const int v = (i == n) ? usable - used : (int) (usable * f); w.push_back (v); used += v; }
        return w;
    };
    const int total = kW - 2 * kMargin;
    const auto wA = split (total, { 0.27f, 0.31f, 0.42f });
    const auto wB = split (total, { 0.50f, 0.50f });
    const auto wC = split (total, { 0.28f, 0.50f, 0.22f });

    Rectangle<int> swell  (kMargin, yA, wA[0], hA);
    Rectangle<int> reverb (swell.getRight() + gap, yA, wA[1], hA);
    Rectangle<int> shim   (reverb.getRight() + gap, yA, wA[2], hA);
    Rectangle<int> sego   (kMargin, yB, wB[0], hB);
    Rectangle<int> dly    (sego.getRight() + gap, yB, wB[1], hB);
    Rectangle<int> quad   (kMargin, yC, wC[0], hC);
    Rectangle<int> eqR    (quad.getRight() + gap, yC, wC[1], hC);
    Rectangle<int> compR  (eqR.getRight() + gap, yC, wC[2], hC);

    auto contentOf = [] (Rectangle<int> r) { return r.withTrimmedTop (60).reduced (10, 2).withTrimmedBottom (10); };
    auto cell = [] (Rectangle<int> c, int cols, int rows, int col, int row)
    {
        const int cw = c.getWidth() / cols, ch = c.getHeight() / rows;
        return Rectangle<int> (c.getX() + col * cw, c.getY() + row * ch, cw, ch).reduced (2);
    };
    auto gemPos = [] (Rectangle<int> r, Gem* g) { g->setBounds (r.getRight() - 62, r.getY() + 23, 50, 26); };

    gemPos (swell, swGem);
    for (int i = 0; i < 5; ++i) swK[i]->setBounds (cell (contentOf (swell), 3, 2, i % 3, i / 3));

    gemPos (reverb, r1Gem);
    for (int i = 0; i < 6; ++i) r1K[i]->setBounds (cell (contentOf (reverb), 3, 2, i % 3, i / 3));

    gemPos (shim, r2Gem);
    for (int i = 0; i < 7; ++i) r2K[i]->setBounds (cell (contentOf (shim), 4, 2, i % 4, i / 4));
    r2Int->setBounds (cell (contentOf (shim), 4, 2, 3, 1));

    // Superego : 7 colonnes x 2 lignes
    gemPos (sego, sgGem);
    const auto sc = contentOf (sego);
    sgMode->setBounds (cell (sc, 7, 2, 0, 0));
    for (int i = 0; i < 5; ++i) sgK[i]->setBounds (cell (sc, 7, 2, 1 + i, 0));
    freezeBtn->setBounds (cell (sc, 7, 2, 6, 0).withSizeKeepingCentre (jmin (cell (sc, 7, 2, 6, 0).getWidth() - 4, 88), 44).translated (0, 8));
    sgFx->setBounds (cell (sc, 7, 2, 0, 1));
    sgK[5]->setBounds (cell (sc, 7, 2, 1, 1));
    sgK[6]->setBounds (cell (sc, 7, 2, 2, 1));
    sgK[7]->setBounds (cell (sc, 7, 2, 3, 1));
    sgK[8]->setBounds (cell (sc, 7, 2, 4, 1));
    clearBtn.setBounds (cell (sc, 7, 2, 6, 1).withSizeKeepingCentre (jmin (cell (sc, 7, 2, 6, 1).getWidth() - 4, 88), 34).translated (0, 6));

    // Delay : 6 colonnes x 2 lignes (gauche | centre | droite)
    gemPos (dly, dlGem);
    syncGem->setBounds (dly.getRight() - 62 - 78, dly.getY() + 23, 74, 26);
    const auto dc = contentOf (dly);
    for (int i = 0; i < 6; ++i) dlK[i]->setBounds (cell (dc, 6, 2, i, 0));
    dlDivL->setBounds (cell (dc, 6, 2, 0, 1));
    dlK[6]->setBounds (cell (dc, 6, 2, 1, 1));
    dlK[7]->setBounds (cell (dc, 6, 2, 2, 1));
    dlK[8]->setBounds (cell (dc, 6, 2, 3, 1));
    dlK[9]->setBounds (cell (dc, 6, 2, 4, 1));
    dlDivR->setBounds (cell (dc, 6, 2, 5, 1));

    // Quadravox : 4 voix en colonnes (pitch / level / pan) + mix
    gemPos (quad, qdGem);
    const auto qc = contentOf (quad);
    for (int v = 0; v < 4; ++v)
        for (int k = 0; k < 3; ++k)
            qdK[v * 3 + k]->setBounds (cell (qc, 5, 3, v, k));
    qdMix->setBounds (cell (qc, 5, 3, 4, 1));

    // Egaliseur : courbe à gauche, 6 bandes en colonnes (frequence / gain / Q)
    gemPos (eqR, eqGem);
    auto ec = contentOf (eqR);
    eqCurve->setBounds (ec.removeFromLeft (250).reduced (2, 4));
    ec.removeFromLeft (6);
    auto at = [&] (int col, int row) { return cell (ec, 6, 3, col, row); };
    eqK[0]->setBounds (at (0, 0));
    eqK[1]->setBounds (at (1, 0));  eqK[2]->setBounds (at (1, 1));
    eqK[3]->setBounds (at (2, 0));  eqK[4]->setBounds (at (2, 1));  eqK[5]->setBounds (at (2, 2));
    eqK[6]->setBounds (at (3, 0));  eqK[7]->setBounds (at (3, 1));  eqK[8]->setBounds (at (3, 2));
    eqK[9]->setBounds (at (4, 0));  eqK[10]->setBounds (at (4, 1));
    eqK[11]->setBounds (at (5, 0));

    gemPos (compR, cpGem);
    for (int i = 0; i < 6; ++i) cpK[i]->setBounds (cell (contentOf (compR), 3, 2, i % 3, i / 3));

    art.setPanels ({ { swell.toFloat(),  "SWELL",        Pal::olive },
                     { reverb.toFloat(), "REVERB",       Pal::teal },
                     { shim.toFloat(),   "SHIMMER",      Pal::rose },
                     { sego.toFloat(),   "SUPEREGO",     Pal::plum },
                     { dly.toFloat(),    "STEREO DELAY", Pal::sky },
                     { quad.toFloat(),   "QUADRAVOX",    Pal::sky },
                     { eqR.toFloat(),    "EQUALIZER",    Pal::teal },
                     { compR.toFloat(),  "COMPRESSOR",   Pal::ochre } },
                   strip.toFloat());
}
