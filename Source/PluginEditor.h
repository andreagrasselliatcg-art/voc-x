#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace vxui
{
namespace col
{
    const juce::Colour bg       { 0xff0f1015 };
    const juce::Colour panel    { 0xff1b1d25 };
    const juce::Colour panelHi  { 0xff252834 };
    const juce::Colour text     { 0xffe8e9ef };
    const juce::Colour dim      { 0xff8b8fa3 };
    const juce::Colour cyan     { 0xff2fe0ff };
    const juce::Colour magenta  { 0xffff3fa4 };
    const juce::Colour lcd      { 0xff0a1418 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::ComboBox box;
private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class BandDisplay : public juce::Component, private juce::Timer
{
public:
    explicit BandDisplay (VocXProcessor& p);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    VocXProcessor& proc;
    std::array<float, vx::Vocoder::kMaxBands> shown {}, peak {};
};

struct Section
{
    juce::String title;
    juce::Rectangle<int> bounds;
};
} // namespace vxui

class VocXEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocXEditor (VocXProcessor&);
    ~VocXEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    vxui::Knob& knob (const char* id, const char* label);
    vxui::Choice& choice (const char* id, const char* label);
    void layoutGrid (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int cols, int cellH);

    VocXProcessor& proc;
    vxui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    vxui::BandDisplay bands;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<vxui::Knob>> knobs;
    std::vector<std::unique_ptr<vxui::Choice>> choices;
    std::unique_ptr<vxui::Toggle> freeze, ping;
    std::map<juce::String, juce::Component*> byId;

    std::array<vxui::Section, 3> sections;
    float inMeter = 0, outMeter = 0;
    juce::String noteText;
    bool gateLit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocXEditor)
};
