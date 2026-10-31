#pragma once
#include "Biquad.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace vx
{
// Drive + Lo-Fi (bit crush & sample-rate reduction)
class DriveLoFi
{
public:
    void prepare (double) { holdL = holdR = 0; counter = 0; }

    void set (float drive, float lofi)
    {
        k = 1.0f + drive * drive * 24.0f;
        // small-signal gain grows gently, peaks are clamped by tanh
        norm = (1.0f + (k - 1.0f) * 0.3f) / k;
        driveOn = drive > 0.001f;
        lofiOn = lofi > 0.001f;
        const float bits = 16.0f - 12.0f * lofi;
        steps = std::pow (2.0f, bits - 1.0f);
        downsample = 1 + (int) (lofi * 11.0f);
    }

    inline void process (float& l, float& r) noexcept
    {
        if (driveOn)
        {
            l = std::tanh (l * k) * norm;
            r = std::tanh (r * k) * norm;
        }
        if (lofiOn)
        {
            if (++counter >= downsample)
            {
                counter = 0;
                holdL = std::round (l * steps) / steps;
                holdR = std::round (r * steps) / steps;
            }
            l = holdL;
            r = holdR;
        }
    }

private:
    float k = 1.0f, norm = 1.0f, steps = 32768.0f, holdL = 0, holdR = 0;
    int downsample = 1, counter = 0;
    bool driveOn = false, lofiOn = false;
};

// Three-phase string-machine style ensemble chorus
class Ensemble
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 0.05) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        pos = 0; slow = fast = 0.0;
    }

    void set (float mixAmount, float depthAmount) { mix = mixAmount; depth = depthAmount; }

    inline void process (float& l, float& r) noexcept
    {
        if (mix <= 0.001f)
            return;

        bufL[(size_t) pos] = l;
        bufR[(size_t) pos] = r;

        slow += 0.63 / sr; if (slow >= 1.0) slow -= 1.0;
        fast += 5.8 / sr;  if (fast >= 1.0) fast -= 1.0;

        float wl = 0.0f, wr = 0.0f;
        for (int t = 0; t < 3; ++t)
        {
            const double off = t / 3.0;
            const double modL = 0.75 * std::sin (2 * kPi * (slow + off)) + 0.25 * std::sin (2 * kPi * (fast + off));
            const double modR = 0.75 * std::sin (2 * kPi * (slow + off + 0.5)) + 0.25 * std::sin (2 * kPi * (fast + off + 0.25));
            wl += read (bufL, (0.010 + 0.0045 * depth * (1.0 + modL)) * sr);
            wr += read (bufR, (0.010 + 0.0045 * depth * (1.0 + modR)) * sr);
        }
        wl /= 3.0f; wr /= 3.0f;

        l = l * (1.0f - 0.5f * mix) + wl * mix;
        r = r * (1.0f - 0.5f * mix) + wr * mix;
        pos = (pos + 1) % size;
    }

private:
    inline float read (const std::vector<float>& b, double delaySamples) const noexcept
    {
        double rp = pos - delaySamples;
        while (rp < 0) rp += size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float f = (float) (rp - i0);
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }

    double sr = 44100.0, slow = 0.0, fast = 0.0;
    std::vector<float> bufL, bufR;
    int size = 1, pos = 0;
    float mix = 0.0f, depth = 0.5f;
};

// Tempo-synced stereo / ping-pong delay with damping
class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 4.0) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        pos = 0;
        dampL = dampR = 0.0f;
        smoothedDelay = -1.0;
    }

    void set (double delaySeconds, float fb, float mixAmount, bool ping)
    {
        targetDelay = std::clamp (delaySeconds * sr, 1.0, (double) size - 2.0);
        if (smoothedDelay < 0) smoothedDelay = targetDelay;
        feedback = fb;
        mix = mixAmount;
        pingPong = ping;
    }

    inline void process (float& l, float& r) noexcept
    {
        if (mix <= 0.001f && ! tailActive)
            return;

        smoothedDelay += (targetDelay - smoothedDelay) * 0.0005;
        const float dl = read (bufL, smoothedDelay);
        const float dr = read (bufR, smoothedDelay);

        dampL += 0.35f * (dl - dampL);
        dampR += 0.35f * (dr - dampR);

        if (pingPong)
        {
            bufL[(size_t) pos] = 0.5f * (l + r) + dampR * feedback;
            bufR[(size_t) pos] = dampL * feedback;
        }
        else
        {
            bufL[(size_t) pos] = l + dampL * feedback;
            bufR[(size_t) pos] = r + dampR * feedback;
        }
        pos = (pos + 1) % size;

        tailActive = std::abs (dl) + std::abs (dr) > 1.0e-6f || mix > 0.001f;
        l += dl * mix;
        r += dr * mix;
    }

private:
    inline float read (const std::vector<float>& b, double d) const noexcept
    {
        double rp = pos - d;
        while (rp < 0) rp += size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float f = (float) (rp - i0);
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }

    double sr = 44100.0, targetDelay = 1000.0, smoothedDelay = -1.0;
    std::vector<float> bufL, bufR;
    int size = 1, pos = 0;
    float feedback = 0.3f, mix = 0.0f, dampL = 0, dampR = 0;
    bool pingPong = true, tailActive = false;
};
} // namespace vx
