#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace vx
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // real-world values; choices = index
};

// Modes: 0 MIDI, 1 Follow Voice, 2 Drone
// Chords: 0 Single 1 Octaves 2 Power 3 Major 4 Minor 5 Maj7 6 Min7 7 Sus4 8 5ths 9 Choir
// Scale: 0 Off 1 Chromatic 2 Major 3 Minor   Poly: 0 Poly 1 Mono 2 Legato
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> p {
        { "Init - Auto Vocoder (no MIDI)", {} },
        { "Classic MIDI Vocoder", { { "mode", 0 } } },
        { "Robot Funk (no MIDI)", { { "mode", 1 }, { "scale", 1 }, { "bands", 16 }, { "saw", 1.0f }, { "pulse", 0.3f },
                                    { "unison", 2 }, { "detune", 8 }, { "emphasis", 0.4f }, { "sibilance", 0.35f },
                                    { "ensMix", 0.2f }, { "drive", 0.2f }, { "revMix", 0.08f } } },
        { "Hard Tune Robot", { { "mode", 1 }, { "scale", 1 }, { "saw", 0.3f }, { "pulse", 0.8f }, { "pw", 0.3f },
                               { "bands", 24 }, { "sibilance", 0.5f }, { "envRelease", 40 }, { "revMix", 0.1f } } },
        { "Auto Chord Major", { { "mode", 1 }, { "chord", 3 }, { "scale", 2 }, { "unison", 3 }, { "detune", 12 },
                                { "ensMix", 0.3f }, { "revMix", 0.22f }, { "revSize", 0.65f } } },
        { "Dark Minor Choir", { { "mode", 1 }, { "chord", 4 }, { "scale", 3 }, { "key", 9 }, { "octave", -1 },
                                { "formant", -3 }, { "unison", 5 }, { "detune", 18 }, { "ensMix", 0.5f },
                                { "revMix", 0.4f }, { "revSize", 0.85f }, { "attack", 60 }, { "release", 500 } } },
        { "Harmony Stack 5ths", { { "mode", 1 }, { "chord", 8 }, { "scale", 3 }, { "key", 9 }, { "unison", 2 },
                                  { "ensMix", 0.4f }, { "revMix", 0.25f } } },
        { "Vocal Thickener", { { "mode", 1 }, { "chord", 1 }, { "dry", 0.85f }, { "wet", 0.45f }, { "unison", 3 },
                               { "detune", 10 }, { "ensMix", 0.4f }, { "revMix", 0.12f } } },
        { "70s Machine", { { "mode", 0 }, { "bands", 12 }, { "saw", 0.9f }, { "sub", 0.3f }, { "bandwidth", 0.6f }, { "envRelease", 80 },
                           { "ensMix", 0.3f }, { "lofi", 0.1f }, { "highHz", 6000 } } },
        { "String Choir Ensemble", { { "mode", 0 }, { "bands", 20 }, { "saw", 1.0f }, { "sub", 0.1f }, { "unison", 5 }, { "detune", 18 },
                                     { "attack", 150 }, { "release", 700 }, { "ensMix", 0.8f }, { "ensDepth", 0.7f },
                                     { "revMix", 0.35f }, { "revSize", 0.8f } } },
        { "Talkbox", { { "mode", 0 }, { "bands", 40 }, { "bandwidth", 0.25f }, { "saw", 0.4f }, { "pulse", 1.0f }, { "pw", 0.35f },
                       { "sub", 0.0f }, { "envAttack", 1 }, { "envRelease", 30 }, { "emphasis", 0.5f },
                       { "poly", 2 }, { "glide", 40 }, { "drive", 0.25f } } },
        { "Whisper", { { "mode", 2 }, { "saw", 0.0f }, { "sub", 0.0f }, { "noise", 1.0f }, { "bands", 32 },
                       { "sibilance", 0.6f }, { "revMix", 0.25f } } },
        { "Deep Monster", { { "mode", 1 }, { "formant", -9 }, { "octave", -1 }, { "sub", 0.6f }, { "drive", 0.35f },
                            { "bands", 24 }, { "revMix", 0.15f } } },
        { "Alien Chipmunk", { { "mode", 1 }, { "formant", 9 }, { "octave", 1 }, { "chord", 1 }, { "dlyMix", 0.2f },
                              { "dlyTime", 2 }, { "vibDepth", 0.3f }, { "vibRate", 6.5f } } },
        { "Lo-Fi Radio", { { "lofi", 0.55f }, { "drive", 0.4f }, { "lowHz", 300 }, { "highHz", 4000 }, { "bands", 14 },
                           { "revMix", 0.0f } } },
        { "Space Echo Pad", { { "mode", 0 }, { "unison", 4 }, { "detune", 14 }, { "attack", 300 }, { "release", 1200 },
                              { "dlyMix", 0.35f }, { "dlyFb", 0.5f }, { "dlyTime", 4 }, { "revMix", 0.5f },
                              { "revSize", 0.9f }, { "ensMix", 0.5f } } },
        { "Power Chord Drone", { { "mode", 2 }, { "drone", 40 }, { "chord", 2 }, { "saw", 1.0f }, { "unison", 3 },
                                 { "drive", 0.5f }, { "revMix", 0.15f } } },
        { "Sidechain Carrier", { { "mode", 0 }, { "source", 1 }, { "bands", 24 }, { "revMix", 0.05f } } },
        { "Frozen Choir", { { "mode", 1 }, { "chord", 9 }, { "unison", 4 }, { "envRelease", 400 },
                            { "ensMix", 0.6f }, { "revMix", 0.45f }, { "revSize", 0.9f } } },
    };
    return p;
}

inline void applyPreset (juce::AudioProcessorValueTreeState& state, int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size())
        return;

    // reset everything to defaults first
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : list[(size_t) index].values)
        if (auto* rp = state.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}
} // namespace vx
