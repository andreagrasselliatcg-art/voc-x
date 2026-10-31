#pragma once
#include "Biquad.h"
#include <array>
#include <cstdint>
#include <vector>

namespace vx
{
// Polyphonic, band-limited (PolyBLEP) synth used as the vocoder carrier.
class CarrierSynth
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxUnison = 7;

    struct Settings
    {
        float saw = 0.8f, pulse = 0.0f, pulseWidth = 0.5f, sub = 0.2f, noise = 0.05f;
        int unison = 1;
        float detuneCents = 15.0f;
        float attackMs = 5.0f, decayMs = 200.0f, sustain = 1.0f, releaseMs = 200.0f;
        float glideMs = 0.0f;
        float vibRate = 5.0f, vibDepth = 0.0f; // depth in semitones
        bool mono = false;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v = Voice{};
        lfoPhase = 0.0;
    }

    void setSettings (const Settings& s)
    {
        set = s;
        set.unison = std::clamp (s.unison, 1, kMaxUnison);
        glideCoef = s.glideMs <= 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (float) (s.glideMs * 0.001 * sr));
        for (int u = 0; u < kMaxUnison; ++u)
        {
            const float spread = set.unison == 1 ? 0.0f : ((float) u / (float) (set.unison - 1) * 2.0f - 1.0f);
            uniRatio[(size_t) u] = std::pow (2.0, spread * set.detuneCents / 1200.0);
        }
    }

    // key = id of the held thing (MIDI note, or -1 for follow / drone), voices are per chord note
    void keyOn (int key, int slot, float midiPitch, float velocity, bool retrigger)
    {
        Voice* v = findVoice (key, slot);
        const bool wasActive = v->active && v->stage != Stage::Release;
        v->key = key;
        v->slot = slot;
        v->targetPitch = midiPitch;
        if (! v->active || glideCoef >= 1.0f)
            v->pitch = midiPitch;
        v->velocity = velocity;
        v->active = true;
        if (retrigger || ! wasActive)
        {
            v->stage = Stage::Attack;
            if (! wasActive)
                for (int u = 0; u < kMaxUnison; ++u)
                    v->phase[u] = rand01();
        }
        v->age = ++ageCounter;
    }

    // move an existing key's voice to a new pitch without retriggering (legato / follow mode)
    void retune (int key, int slot, float midiPitch)
    {
        for (auto& v : voices)
            if (v.active && v.key == key && v.slot == slot && v.stage != Stage::Release)
                { v.targetPitch = midiPitch; return; }
    }

    void keyOff (int key)
    {
        for (auto& v : voices)
            if (v.active && v.key == key && v.stage != Stage::Release)
                v.stage = Stage::Release;
    }

    void allOff()
    {
        for (auto& v : voices)
            if (v.active) v.stage = Stage::Release;
    }

    int countHeld (int key) const
    {
        int n = 0;
        for (auto& v : voices)
            if (v.active && v.key == key && v.stage != Stage::Release) ++n;
        return n;
    }

    inline float render() noexcept
    {
        lfoPhase += set.vibRate / sr;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const float vib = set.vibDepth * (float) std::sin (2.0 * kPi * lfoPhase);

        float out = 0.0f;
        const int uni = set.unison;
        const float uniGain = 1.0f / std::sqrt ((float) uni);

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const float env = advanceEnvelope (v);
            if (! v.active)
                continue;

            v.pitch += (v.targetPitch - v.pitch) * glideCoef;
            const double baseHz = 440.0 * std::exp2 ((v.pitch + vib - 69.0) / 12.0);

            float s = 0.0f;
            for (int u = 0; u < uni; ++u)
            {
                const double hz = baseHz * uniRatio[(size_t) u];
                const double dt = std::min (0.45, hz / sr);
                double& ph = v.phase[u];

                if (set.saw > 0.0f)
                    s += set.saw * blepSaw (ph, dt);
                if (set.pulse > 0.0f)
                {
                    double ph2 = ph + set.pulseWidth;
                    if (ph2 >= 1.0) ph2 -= 1.0;
                    s += set.pulse * 0.5f * (blepSaw (ph, dt) - blepSaw (ph2, dt));
                }
                ph += dt;
                if (ph >= 1.0) ph -= 1.0;
            }
            s *= uniGain;

            if (set.sub > 0.0f)
            {
                const double dt = std::min (0.45, baseHz * 0.5 / sr);
                double ph2 = v.subPhase + 0.5;
                if (ph2 >= 1.0) ph2 -= 1.0;
                s += set.sub * 0.5f * (blepSaw (v.subPhase, dt) - blepSaw (ph2, dt));
                v.subPhase += dt;
                if (v.subPhase >= 1.0) v.subPhase -= 1.0;
            }

            if (set.noise > 0.0f)
                s += set.noise * (rand01f() * 2.0f - 1.0f);

            out += s * env * (0.5f + 0.5f * v.velocity);
        }
        return out * 0.5f;
    }

    bool anyActive() const
    {
        for (auto& v : voices) if (v.active) return true;
        return false;
    }

private:
    enum class Stage { Attack, Decay, Sustain, Release };

    struct Voice
    {
        bool active = false;
        int key = 0, slot = 0;
        float pitch = 60.0f, targetPitch = 60.0f, velocity = 1.0f;
        float env = 0.0f;
        Stage stage = Stage::Release;
        std::array<double, kMaxUnison> phase {};
        double subPhase = 0.0;
        uint32_t age = 0;
    };

    Voice* findVoice (int key, int slot)
    {
        // same key + same chord slot (re-press / legato) -> reuse
        for (auto& v : voices)
            if (v.active && v.key == key && v.slot == slot)
                return &v;
        for (auto& v : voices)
            if (! v.active) return &v;
        // steal: prefer released voices, then the oldest
        Voice* best = nullptr;
        for (auto& v : voices)
            if (v.stage == Stage::Release && (best == nullptr || v.env < best->env)) best = &v;
        if (best) return best;
        best = &voices[0];
        for (auto& v : voices)
            if (v.age < best->age) best = &v;
        return best;
    }

    inline float advanceEnvelope (Voice& v) noexcept
    {
        const float a = 1.0f / (float) (std::max (0.5f, set.attackMs) * 0.001 * sr);
        const float d = 1.0f / (float) (std::max (1.0f, set.decayMs) * 0.001 * sr);
        const float r = 1.0f / (float) (std::max (1.0f, set.releaseMs) * 0.001 * sr);

        switch (v.stage)
        {
            case Stage::Attack:
                v.env += a;
                if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Stage::Decay; }
                break;
            case Stage::Decay:
                v.env -= d * (1.0f - set.sustain);
                if (v.env <= set.sustain) { v.env = set.sustain; v.stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                v.env = set.sustain;
                break;
            case Stage::Release:
                v.env -= r;
                if (v.env <= 0.0f) { v.env = 0.0f; v.active = false; }
                break;
        }
        return v.env * v.env;
    }

    static inline float polyBlep (double t, double dt) noexcept
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    static inline float blepSaw (double ph, double dt) noexcept
    {
        return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt);
    }

    double rand01() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0; }
    float rand01f() { seed = seed * 1664525u + 1013904223u; return (float) ((seed >> 8) / 16777216.0); }

    double sr = 44100.0;
    Settings set;
    float glideCoef = 1.0f;
    std::array<double, kMaxUnison> uniRatio { 1, 1, 1, 1, 1, 1, 1 };
    std::array<Voice, kMaxVoices> voices;
    double lfoPhase = 0.0;
    uint32_t seed = 12345u, ageCounter = 0;
};
} // namespace vx
