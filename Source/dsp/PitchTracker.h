#pragma once
#include "Biquad.h"
#include <array>
#include <atomic>
#include <vector>

namespace vx
{
// YIN pitch detector running on a decimated copy of the voice (~11 kHz).
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decim = std::max (1, (int) std::lround (sampleRate / 11025.0));
        dsr = sampleRate / decim;
        lp1.setLowpass (sampleRate, std::min (2000.0, dsr * 0.4));
        lp2.setLowpass (sampleRate, std::min (2000.0, dsr * 0.4));
        lp1.reset(); lp2.reset();
        tauMin = std::max (2, (int) (dsr / 1000.0));   // 1 kHz max
        tauMax = std::min (kWindow - 1, (int) (dsr / 60.0)); // 60 Hz min
        ring.assign (kRing, 0.0f);
        writePos = 0; decimCount = 0; hopCount = 0;
        voiced = false; frequency = 0.0f; confidence = 0.0f;
    }

    inline void push (float x) noexcept
    {
        const float y = lp2.process (lp1.process (x));
        if (++decimCount < decim)
            return;
        decimCount = 0;
        ring[(size_t) writePos] = y;
        writePos = (writePos + 1) & (kRing - 1);
        if (++hopCount >= kHop)
        {
            hopCount = 0;
            analyse();
        }
    }

    bool isVoiced() const { return voiced; }
    float getFrequency() const { return frequency; }
    float getConfidence() const { return confidence; }

private:
    static constexpr int kRing = 2048;
    static constexpr int kWindow = 384;
    static constexpr int kHop = 96;

    void analyse() noexcept
    {
        const int need = kWindow + tauMax;
        std::array<float, kRing> x;
        int start = (writePos - need + kRing) & (kRing - 1);
        float energy = 0.0f;
        for (int i = 0; i < need; ++i)
        {
            x[(size_t) i] = ring[(size_t) ((start + i) & (kRing - 1))];
            energy += x[(size_t) i] * x[(size_t) i];
        }

        if (energy / (float) need < 1.0e-6f)   // silence (~ -60 dBFS)
        {
            voiced = false; confidence = 0.0f;
            return;
        }

        float runningSum = 0.0f;
        int bestTau = -1;
        float bestVal = 1.0f;
        std::array<float, kWindow> dprime {};

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            for (int j = 0; j < kWindow; ++j)
            {
                const float diff = x[(size_t) j] - x[(size_t) (j + tau)];
                d += diff * diff;
            }
            runningSum += d;
            dprime[(size_t) tau] = runningSum > 0.0f ? d * (float) tau / runningSum : 1.0f;
        }

        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (dprime[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && dprime[(size_t) (tau + 1)] < dprime[(size_t) tau])
                    ++tau;
                bestTau = tau;
                bestVal = dprime[(size_t) tau];
                break;
            }
        }

        if (bestTau < 0)
        {
            voiced = false;
            confidence = 0.0f;
            return;
        }

        // parabolic interpolation
        float t = (float) bestTau;
        if (bestTau > 1 && bestTau < tauMax - 1)
        {
            const float a = dprime[(size_t) bestTau - 1], b = dprime[(size_t) bestTau], c = dprime[(size_t) bestTau + 1];
            const float den = a + c - 2.0f * b;
            if (std::abs (den) > 1.0e-9f)
                t += 0.5f * (a - c) / den;
        }

        frequency = (float) (dsr / t);
        confidence = 1.0f - bestVal;
        voiced = true;
    }

    Biquad lp1, lp2;
    int decim = 4, decimCount = 0, hopCount = 0, writePos = 0;
    double dsr = 11025.0;
    int tauMin = 11, tauMax = 180;
    std::vector<float> ring;
    bool voiced = false;
    float frequency = 0.0f, confidence = 0.0f;
};
} // namespace vx
