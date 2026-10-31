#include "PluginEditor.h"

namespace vxui
{
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::ComboBox::backgroundColourId, col::lcd);
    setColour (juce::ComboBox::textColourId, col::cyan);
    setColour (juce::ComboBox::arrowColourId, col::cyan);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::cyan);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::magenta);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, col::dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (11.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, col::dim);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::cyan);
    return l;
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float thick = juce::jmax (3.0f, r * 0.14f);
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? start + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start) : start;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - thick, r - thick, 0, start, end, true);
    g.setColour (juce::Colour (0xff2c303d));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - thick, r - thick, 0, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setGradientFill (juce::ColourGradient (col::cyan, c.x - r, c.y + r, col::magenta, c.x + r, c.y - r, false));
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float cap = r - thick * 2.2f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3e4c), c.x, c.y - cap, juce::Colour (0xff15161c), c.x, c.y + cap, false));
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2);
    g.setColour (col::text);
    g.drawLine ({ c.getPointOnCircumference (cap * 0.35f, angle), c.getPointOnCircumference (cap * 0.92f, angle) }, 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = colour;
    if (over) c = c.brighter (0.1f);
    if (down) c = c.darker (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
    if (b.getToggleState())
    {
        g.setColour (col::magenta.withAlpha (0.5f));
        g.drawRoundedRectangle (r.expanded (0.5f), 5.0f, 2.0f);
    }
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (col::cyan.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::cyan);
    g.fillPath (arrow);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l) : label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 14);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (14));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (col::text);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (label.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l) : label (l)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

void Choice::resized()
{
    box.setBounds (getLocalBounds().withTrimmedTop (15));
}

void Choice::paint (juce::Graphics& g)
{
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (label.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

//==============================================================================
BandDisplay::BandDisplay (VocXProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void BandDisplay::timerCallback()
{
    const int n = proc.numBandsShown.load();
    for (int i = 0; i < n; ++i)
    {
        const float env = proc.bandLevels[(size_t) i].load();
        const float db = 20.0f * std::log10 (env + 1.0e-6f);
        const float v = juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 60.0f);
        shown[(size_t) i] = v > shown[(size_t) i] ? v : shown[(size_t) i] * 0.86f;
        peak[(size_t) i] = v > peak[(size_t) i] ? v : juce::jmax (0.0f, peak[(size_t) i] - 0.012f);
    }
    repaint();
}

void BandDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::lcd);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (col::cyan.withAlpha (0.15f));
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    auto area = b.reduced (16.0f, 14.0f).withTrimmedBottom (14.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * i / 4.0f), area.getX(), area.getRight());

    const int n = proc.numBandsShown.load();
    const float w = area.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float hgt = area.getHeight() * shown[(size_t) i];
        auto bar = juce::Rectangle<float> (area.getX() + i * w + w * 0.15f, area.getBottom() - hgt, w * 0.7f, hgt);
        g.setGradientFill (juce::ColourGradient (col::cyan, 0, area.getBottom(), col::magenta, 0, area.getY(), false));
        g.fillRoundedRectangle (bar, 2.0f);

        const float py = area.getBottom() - area.getHeight() * peak[(size_t) i];
        g.setColour (col::text.withAlpha (0.8f));
        g.fillRect (area.getX() + i * w + w * 0.15f, py - 1.5f, w * 0.7f, 2.0f);
    }

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (10.0f));
    const auto* lo = proc.apvts.getParameter ("lowHz");
    const auto* hi = proc.apvts.getParameter ("highHz");
    g.drawText (lo->getCurrentValueAsText(), area.withY (area.getBottom() + 2).withHeight (12), juce::Justification::centredLeft);
    g.drawText (hi->getCurrentValueAsText(), area.withY (area.getBottom() + 2).withHeight (12), juce::Justification::centredRight);
    g.drawText (juce::String (n) + " BANDS", area.withY (area.getBottom() + 2).withHeight (12), juce::Justification::centred);

    if (proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f)
    {
        g.setColour (col::magenta);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("FROZEN", area.removeFromTop (16), juce::Justification::centredRight);
    }
}
} // namespace vxui

//==============================================================================
namespace { constexpr int kW = 1200, kH = 800; }

VocXEditor::VocXEditor (VocXProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bands (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    const auto& presets = vx::factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
    };
    prevButton.onClick = [this] { presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() - 1 + presetBox.getNumItems()) % presetBox.getNumItems()); };
    nextButton.onClick = [this] { presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + 1) % presetBox.getNumItems()); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevButton, &nextButton, &bands, &keyboard })
        addAndMakeVisible (*c);

    // carrier
    choice ("mode", "Carrier");
    choice ("chord", "Chord");
    choice ("scale", "Scale / Tune");
    choice ("key", "Key");
    choice ("poly", "Voice Mode");
    for (auto [id, l] : std::initializer_list<std::pair<const char*, const char*>> {
             { "octave", "Octave" }, { "glide", "Glide" }, { "saw", "Saw" }, { "pulse", "Pulse" }, { "pw", "PW" }, { "sub", "Sub" },
             { "noise", "Noise" }, { "unison", "Unison" }, { "detune", "Detune" }, { "vibRate", "Vib Rate" }, { "vibDepth", "Vib Depth" }, { "drone", "Drone Note" },
             { "attack", "Attack" }, { "decay", "Decay" }, { "sustain", "Sustain" }, { "release", "Release" } })
        knob (id, l);

    // vocoder
    choice ("source", "Carrier Source");
    freeze = std::make_unique<vxui::Toggle> (proc.apvts, "freeze", "FREEZE");
    addAndMakeVisible (*freeze);
    for (auto [id, l] : std::initializer_list<std::pair<const char*, const char*>> {
             { "bands", "Bands" }, { "lowHz", "Low" }, { "highHz", "High" }, { "bandwidth", "Q / Width" },
             { "formant", "Formant" }, { "envAttack", "Env Atk" }, { "envRelease", "Env Rel" }, { "emphasis", "Emphasis" },
             { "sibilance", "Sibilance" }, { "width", "Stereo" }, { "inGain", "Input" }, { "gate", "Gate" } })
        knob (id, l);

    // fx + output
    choice ("dlyTime", "Delay Time");
    ping = std::make_unique<vxui::Toggle> (proc.apvts, "dlyPing", "PING-PONG");
    addAndMakeVisible (*ping);
    for (auto [id, l] : std::initializer_list<std::pair<const char*, const char*>> {
             { "drive", "Drive" }, { "lofi", "Lo-Fi" }, { "ensMix", "Ensemble" }, { "ensDepth", "Ens Depth" },
             { "dlyMix", "Delay" }, { "dlyFb", "Feedback" }, { "revMix", "Reverb" }, { "revSize", "Size" },
             { "dry", "Dry Voice" }, { "wet", "Vocoder" }, { "out", "Output" } })
        knob (id, l);

    static_cast<vxui::Choice*> (byId["mode"])->box.setTooltip ("MIDI: play the carrier from a MIDI track.  Follow Voice: no MIDI needed, the synth follows your singing.  Drone: fixed note/chord.");
    static_cast<vxui::Choice*> (byId["scale"])->box.setTooltip ("Follow Voice: Off = natural pitch, Chromatic = hard-tune robot, Major/Minor = snap to the key");
    static_cast<vxui::Choice*> (byId["source"])->box.setTooltip ("Sidechain: use any track (synth, pad...) as the carrier via Logic's sidechain menu");
    freeze->button.setTooltip ("Hold the current vowel/formant");

    keyboard.setAvailableRange (24, 96);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, vxui::col::magenta);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, vxui::col::cyan.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.3f));

    setSize (kW, kH);
    sendLookAndFeelChange();   // children were created before being parented: refresh their styling
    startTimerHz (20);
}

VocXEditor::~VocXEditor()
{
    setLookAndFeel (nullptr);
}

vxui::Knob& VocXEditor::knob (const char* id, const char* label)
{
    knobs.push_back (std::make_unique<vxui::Knob> (proc.apvts, id, label));
    addAndMakeVisible (*knobs.back());
    byId[id] = knobs.back().get();
    return *knobs.back();
}

vxui::Choice& VocXEditor::choice (const char* id, const char* label)
{
    choices.push_back (std::make_unique<vxui::Choice> (proc.apvts, id, label));
    addAndMakeVisible (*choices.back());
    byId[id] = choices.back().get();
    return *choices.back();
}

void VocXEditor::layoutGrid (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int cols, int cellH)
{
    const int cellW = area.getWidth() / cols;
    int i = 0;
    for (auto* c : items)
    {
        if (c != nullptr)
            c->setBounds (area.getX() + (i % cols) * cellW + 2, area.getY() + (i / cols) * cellH, cellW - 4, cellH - 4);
        ++i;
    }
}

void VocXEditor::timerCallback()
{
    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);

    inMeter = proc.inputLevel.load();
    outMeter = proc.outputLevel.load();
    gateLit = proc.gateOpenFlag.load();
    const float note = proc.detectedNote.load();
    noteText = note < 0 ? juce::String ("--") : juce::MidiMessage::getMidiNoteName ((int) std::round (note), true, true, 3);
    repaint (0, 0, kW, 76);
}

void VocXEditor::paint (juce::Graphics& g)
{
    using namespace vxui;
    g.fillAll (col::bg);

    // header
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff171922), 0, 0, col::bg, 0, 76, false));
    g.fillRect (0, 0, kW, 76);
    g.setGradientFill (juce::ColourGradient (col::cyan, 20, 0, col::magenta, 170, 0, false));
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold | juce::Font::italic));
    g.drawText ("VOC-X", 20, 6, 180, 46, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("TUNE TALKS  |  MULTI-MODE VOCODER", 22, 50, 260, 16, juce::Justification::centredLeft);

    // note LCD
    auto lcd = juce::Rectangle<float> (760, 14, 190, 50);
    g.setColour (col::lcd);
    g.fillRoundedRectangle (lcd, 6.0f);
    g.setColour (col::cyan.withAlpha (0.2f));
    g.drawRoundedRectangle (lcd, 6.0f, 1.0f);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("CARRIER NOTE", lcd.reduced (10, 5).removeFromTop (12), juce::Justification::topLeft);
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    g.drawText (vx::params::modeChoices[mode].toUpperCase(), lcd.reduced (10, 5).removeFromTop (12), juce::Justification::topRight);
    g.setColour (col::cyan);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (noteText, lcd.reduced (10, 4).withTrimmedTop (14), juce::Justification::centredLeft);
    // gate led
    g.setColour (gateLit ? col::magenta : col::magenta.withAlpha (0.15f));
    g.fillEllipse (lcd.getRight() - 22, lcd.getBottom() - 20, 10, 10);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText ("VOICE", juce::Rectangle<float> (lcd.getRight() - 66, lcd.getBottom() - 21, 40, 12), juce::Justification::centredRight);

    // meters
    auto meter = [&] (float x, const char* label, float v)
    {
        auto r = juce::Rectangle<float> (x, 16, 14, 46);
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 3.0f);
        const float db = 20.0f * std::log10 (v + 1.0e-6f);
        const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setGradientFill (juce::ColourGradient (col::cyan, 0, r.getBottom(), db > -3.0f ? juce::Colours::red : col::magenta, 0, r.getY(), false));
        g.fillRoundedRectangle (r.withTop (r.getBottom() - h), 3.0f);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (label, juce::Rectangle<float> (x - 8, 63, 30, 10), juce::Justification::centred);
    };
    meter (1150, "IN", inMeter);
    meter (1172, "OUT", outMeter);

    // section panels
    for (auto& s : sections)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (s.bounds.toFloat(), 10.0f);
        g.setColour (col::cyan);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.reduced (14, 8).removeFromTop (16), juce::Justification::centredLeft);
        g.setGradientFill (juce::ColourGradient (col::cyan.withAlpha (0.6f), (float) s.bounds.getX(), 0, col::magenta.withAlpha (0.0f), (float) s.bounds.getRight(), 0, false));
        g.fillRect (s.bounds.getX() + 14, s.bounds.getY() + 28, s.bounds.getWidth() - 28, 1);
    }

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Logic: insert Voc-X on the vocal track (MIDI-controlled Effects). For MIDI mode, set a MIDI/instrument track's output to Voc-X. "
                "Follow Voice works with no MIDI.",
                juce::Rectangle<int> (16, kH - 26, kW - 32, 20), juce::Justification::centredLeft);
}

void VocXEditor::resized()
{
    presetBox.setBounds (330, 22, 330, 34);
    prevButton.setBounds (668, 22, 34, 34);
    nextButton.setBounds (708, 22, 34, 34);

    bands.setBounds (12, 82, kW - 24, 150);

    const int top = 242, h = 384;
    sections[0] = { "CARRIER  /  SYNTH", { 12, top, 500, h } };
    sections[1] = { "VOCODER", { 520, top, 340, h } };
    sections[2] = { "EFFECTS  /  OUTPUT", { 868, top, 320, h } };

    // carrier
    {
        auto r = sections[0].bounds.reduced (12, 0).withTrimmedTop (36);
        auto row = r.removeFromTop (44);
        const int cw = row.getWidth() / 5;
        for (auto* id : { "mode", "chord", "scale", "key", "poly" })
            byId[id]->setBounds (row.removeFromLeft (cw).reduced (3, 0));
        r.removeFromTop (8);
        layoutGrid (r, { byId["octave"], byId["glide"], byId["saw"], byId["pulse"], byId["pw"], byId["sub"],
                         byId["noise"], byId["unison"], byId["detune"], byId["vibRate"], byId["vibDepth"], byId["drone"],
                         byId["attack"], byId["decay"], byId["sustain"], byId["release"] }, 6, 96);
    }
    // vocoder
    {
        auto r = sections[1].bounds.reduced (12, 0).withTrimmedTop (36);
        auto row = r.removeFromTop (44);
        byId["source"]->setBounds (row.removeFromLeft (200).reduced (3, 0));
        freeze->setBounds (row.reduced (6, 0).withTrimmedTop (15));
        r.removeFromTop (8);
        layoutGrid (r, { byId["bands"], byId["lowHz"], byId["highHz"], byId["bandwidth"],
                         byId["formant"], byId["envAttack"], byId["envRelease"], byId["emphasis"],
                         byId["sibilance"], byId["width"], byId["inGain"], byId["gate"] }, 4, 96);
    }
    // fx
    {
        auto r = sections[2].bounds.reduced (12, 0).withTrimmedTop (36);
        auto row = r.removeFromTop (44);
        byId["dlyTime"]->setBounds (row.removeFromLeft (150).reduced (3, 0));
        ping->setBounds (row.reduced (6, 0).withTrimmedTop (15));
        r.removeFromTop (8);
        layoutGrid (r, { byId["drive"], byId["lofi"], byId["ensMix"], byId["ensDepth"],
                         byId["dlyMix"], byId["dlyFb"], byId["revMix"], byId["revSize"],
                         byId["dry"], byId["wet"], byId["out"] }, 4, 96);
    }

    keyboard.setBounds (12, top + h + 10, kW - 24, kH - (top + h + 10) - 32);
    keyboard.setKeyWidth ((float) (kW - 24) / 43.0f);   // C1..C7 = 43 white keys
    keyboard.setLowestVisibleKey (24);
}
