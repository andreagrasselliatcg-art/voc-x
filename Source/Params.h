#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vx::params
{
inline const juce::StringArray modeChoices   { "MIDI", "Follow Voice", "Drone" };
inline const juce::StringArray sourceChoices { "Synth", "Sidechain", "Synth + Sidechain" };
inline const juce::StringArray chordChoices  { "Single", "Octaves", "Power (1-5-8)", "Major", "Minor",
                                               "Major 7", "Minor 7", "Sus4", "5ths Stack", "Choir (1-5-8-10)" };
inline const juce::StringArray scaleChoices  { "Off (natural)", "Chromatic", "Major", "Minor" };
inline const juce::StringArray keyChoices    { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline const juce::StringArray polyChoices   { "Poly", "Mono", "Legato" };
inline const juce::StringArray delayChoices  { "1/16", "1/8", "1/8 Dotted", "1/4", "1/4 Dotted", "1/2", "1/8 Triplet" };

inline std::vector<int> chordIntervals (int chord)
{
    switch (chord)
    {
        case 1: return { 0, 12 };
        case 2: return { 0, 7, 12 };
        case 3: return { 0, 4, 7 };
        case 4: return { 0, 3, 7 };
        case 5: return { 0, 4, 7, 11 };
        case 6: return { 0, 3, 7, 10 };
        case 7: return { 0, 5, 7 };
        case 8: return { 0, 7, 14 };
        case 9: return { -12, 0, 7, 16 };
        default: return { 0 };
    }
}

inline double delayBeats (int choice)
{
    static const double beats[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 1.0 / 3.0 };
    return beats[juce::jlimit (0, 6, choice)];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)) + "%"; });
    auto ms  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1000 ? String (v, v < 10 ? 1 : 0) + " ms" : String (v / 1000.0f, 2) + " s"; });
    auto hz  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1000 ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz"; });
    auto db  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto st  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; });

    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes attr, float skewCentre = -1.0f)
    {
        NormalisableRange<float> r (lo, hi);
        if (skewCentre > 0) r.setSkewForCentre (skewCentre);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr));
    };
    auto c = [&] (const char* id, const char* name, const StringArray& ch, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, ch, def)); };
    auto i = [&] (const char* id, const char* name, int lo, int hi, int def)
    { l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto b = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // --- carrier
    c ("mode", "Carrier Mode", modeChoices, 1);
    c ("source", "Carrier Source", sourceChoices, 0);
    c ("chord", "Chord", chordChoices, 0);
    c ("scale", "Scale", scaleChoices, 0);
    c ("key", "Key", keyChoices, 0);
    c ("poly", "Voice Mode", polyChoices, 0);
    i ("drone", "Drone Note", 24, 84, 48);
    i ("octave", "Octave", -2, 2, 0);
    f ("glide", "Glide", 0.0f, 1000.0f, 0.0f, ms, 150.0f);
    f ("saw", "Saw", 0.0f, 1.0f, 0.8f, pct);
    f ("pulse", "Pulse", 0.0f, 1.0f, 0.0f, pct);
    f ("pw", "Pulse Width", 0.05f, 0.95f, 0.5f, pct);
    f ("sub", "Sub Osc", 0.0f, 1.0f, 0.2f, pct);
    f ("noise", "Noise", 0.0f, 1.0f, 0.05f, pct);
    i ("unison", "Unison", 1, 7, 1);
    f ("detune", "Detune", 0.0f, 50.0f, 15.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ct"; }));
    f ("attack", "Attack", 0.5f, 2000.0f, 5.0f, ms, 100.0f);
    f ("decay", "Decay", 1.0f, 3000.0f, 200.0f, ms, 300.0f);
    f ("sustain", "Sustain", 0.0f, 1.0f, 1.0f, pct);
    f ("release", "Release", 1.0f, 4000.0f, 200.0f, ms, 300.0f);
    f ("vibRate", "Vibrato Rate", 0.1f, 12.0f, 5.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " Hz"; }));
    f ("vibDepth", "Vibrato Depth", 0.0f, 1.0f, 0.0f, st);

    // --- vocoder
    i ("bands", "Bands", 4, 40, 20);
    f ("lowHz", "Low Freq", 50.0f, 500.0f, 100.0f, hz, 150.0f);
    f ("highHz", "High Freq", 2000.0f, 16000.0f, 8000.0f, hz, 6000.0f);
    f ("bandwidth", "Bandwidth", 0.0f, 1.0f, 0.5f, pct);
    f ("formant", "Formant Shift", -12.0f, 12.0f, 0.0f, st);
    f ("envAttack", "Env Attack", 0.5f, 100.0f, 3.0f, ms, 10.0f);
    f ("envRelease", "Env Release", 5.0f, 1000.0f, 60.0f, ms, 100.0f);
    f ("emphasis", "Emphasis", -1.0f, 1.0f, 0.3f, pct);
    f ("sibilance", "Sibilance", 0.0f, 1.0f, 0.3f, pct);
    f ("width", "Stereo Width", 0.0f, 1.0f, 0.5f, pct);
    f ("inGain", "Input Gain", -12.0f, 24.0f, 0.0f, db);
    f ("gate", "Gate", -80.0f, -20.0f, -60.0f, db);
    b ("freeze", "Freeze", false);

    // --- effects
    f ("drive", "Drive", 0.0f, 1.0f, 0.0f, pct);
    f ("lofi", "Lo-Fi", 0.0f, 1.0f, 0.0f, pct);
    f ("ensMix", "Ensemble Mix", 0.0f, 1.0f, 0.0f, pct);
    f ("ensDepth", "Ensemble Depth", 0.0f, 1.0f, 0.5f, pct);
    f ("dlyMix", "Delay Mix", 0.0f, 1.0f, 0.0f, pct);
    c ("dlyTime", "Delay Time", delayChoices, 3);
    f ("dlyFb", "Delay Feedback", 0.0f, 0.95f, 0.35f, pct);
    b ("dlyPing", "Ping-Pong", true);
    f ("revMix", "Reverb Mix", 0.0f, 1.0f, 0.1f, pct);
    f ("revSize", "Reverb Size", 0.0f, 1.0f, 0.5f, pct);

    // --- output
    f ("dry", "Dry Voice", 0.0f, 1.0f, 0.0f, pct);
    f ("wet", "Vocoder Level", 0.0f, 1.0f, 1.0f, pct);
    f ("out", "Output", -24.0f, 12.0f, 0.0f, db);
    return l;
}
} // namespace vx::params
