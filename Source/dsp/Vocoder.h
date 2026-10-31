#pragma once
#include "Biquad.h"
#include <array>
#include <atomic>

namespace vx
{
// Classic channel vocoder: analysis filter bank on the voice (modulator),
// envelope followers, synthesis filter bank on the carrier.
class Vocoder
{
public:
    static constexpr int kMaxBands = 40;

    struct Settings
    {
        int bands = 20;
        float lowHz = 100.0f, highHz = 8000.0f;
        float bandwidth = 0.5f;     // 0 narrow .. 1 wide
        float formantSemis = 0.0f;  // shifts the synthesis filters
        float emphasis = 0.3f;      // -1 dark .. +1 bright
        float width = 0.5f;         // stereo spread of bands
        float attackMs = 3.0f, releaseMs = 60.0f;
        float sibilance = 0.3f;
        bool freeze = false;

        bool sameFilters (const Settings& o) const
        {
            return bands == o.bands && lowHz == o.lowHz && highHz == o.highHz && bandwidth == o.bandwidth
                && formantSemis == o.formantSemis && emphasis == o.emphasis && width == o.width;
        }
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : bands) { b.m1.reset(); b.m2.reset(); b.c1.reset(); b.c2.reset(); b.env = 0; }
        sibHp1.reset(); sibHp2.reset();
        sibHp1.setHighpass (sr, 4500.0);
        sibHp2.setHighpass (sr, 4500.0);
        configured = false;
        update (current, true);
    }

    void update (const Settings& s, bool force = false)
    {
        if (force || ! configured || ! s.sameFilters (current))
            designFilters (s);

        current = s;
        atkCoef = 1.0f - std::exp (-1.0f / (float) (std::max (0.1f, s.attackMs) * 0.001 * sr));
        relCoef = 1.0f - std::exp (-1.0f / (float) (std::max (1.0f, s.releaseMs) * 0.001 * sr));
        configured = true;
    }

    inline void process (float mod, float car, float& outL, float& outR) noexcept
    {
        float l = 0.0f, r = 0.0f;
        const bool frozen = current.freeze;

        for (int i = 0; i < numBands; ++i)
        {
            auto& b = bands[(size_t) i];

            if (! frozen)
            {
                const float m = b.m2.process (b.m1.process (mod));
                const float a = std::abs (m);
                b.env += (a > b.env ? atkCoef : relCoef) * (a - b.env);
            }

            const float c = b.c2.process (b.c1.process (car)) * b.env * b.gain;
            l += c * b.panL;
            r += c * b.panR;
        }

        // sibilance: let the voice's "s", "t", "sh" through for intelligibility
        const float sib = sibHp2.process (sibHp1.process (mod)) * current.sibilance * 0.8f;

        outL = l * makeup + sib;
        outR = r * makeup + sib;
    }

    int getNumBands() const { return numBands; }
    float getBandEnvelope (int i) const { return bands[(size_t) i].env; }
    float getBandFrequency (int i) const { return bands[(size_t) i].freq; }

private:
    struct Band
    {
        Biquad m1, m2, c1, c2;
        float env = 0.0f, gain = 1.0f, panL = 0.7071f, panR = 0.7071f, freq = 1000.0f;
    };

    void designFilters (const Settings& s)
    {
        const int n = std::clamp (s.bands, 2, kMaxBands);
        if (n != numBands)
            for (auto& b : bands) { b.m1.reset(); b.m2.reset(); b.c1.reset(); b.c2.reset(); }

        numBands = n;
        const double lo = std::max (20.0, (double) s.lowHz);
        const double hi = std::max (lo * 2.0, (double) s.highHz);
        const double ratio = std::pow (hi / lo, 1.0 / (n - 1));
        const double qBase = std::sqrt (ratio) / (ratio - 1.0);
        const double q = std::clamp (qBase * (2.6 - 2.0 * s.bandwidth), 0.5, 60.0);
        const double shift = std::pow (2.0, s.formantSemis / 12.0);

        for (int i = 0; i < n; ++i)
        {
            auto& b = bands[(size_t) i];
            const double f = lo * std::pow (ratio, i);
            b.freq = (float) f;
            b.m1.setBandpass (sr, f, q);
            b.m2.setBandpass (sr, f, q);
            b.c1.setBandpass (sr, f * shift, q);
            b.c2.setBandpass (sr, f * shift, q);
            b.gain = (float) std::pow (f / 1000.0, s.emphasis * 0.6);

            const float pan = ((i % 2) == 0 ? -1.0f : 1.0f) * s.width;
            b.panL = std::sqrt (0.5f * (1.0f - pan));
            b.panR = std::sqrt (0.5f * (1.0f + pan));
        }

        // narrower / more bands = less energy per band: compensate
        // calibrated so the vocoded signal sits near the input voice level for any band count / Q
        makeup = (float) (11.0 * std::pow (std::min (q, 9.0), 0.85));
    }

    double sr = 44100.0;
    std::array<Band, kMaxBands> bands;
    int numBands = 0;
    Settings current;
    bool configured = false;
    float atkCoef = 0.1f, relCoef = 0.01f, makeup = 1.0f;
    Biquad sibHp1, sibHp2;
};
} // namespace vx
