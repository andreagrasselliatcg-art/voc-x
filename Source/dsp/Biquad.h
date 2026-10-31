#pragma once
#include <cmath>
#include <algorithm>

namespace vx
{
constexpr double kPi = 3.14159265358979323846;

// RBJ biquad, transposed direct form II
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setBandpass (double sr, double freq, double q)
    {
        freq = std::clamp (freq, 10.0, sr * 0.47);
        const double w = 2.0 * kPi * freq / sr;
        const double alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (alpha / a0);
        b1 = 0.0f;
        b2 = (float) (-alpha / a0);
        a1 = (float) (-2.0 * std::cos (w) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void setLowpass (double sr, double freq, double q = 0.7071)
    {
        freq = std::clamp (freq, 10.0, sr * 0.47);
        const double w = 2.0 * kPi * freq / sr;
        const double alpha = std::sin (w) / (2.0 * q);
        const double c = std::cos (w);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 - c) * 0.5 / a0);
        b1 = (float) ((1.0 - c) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void setHighpass (double sr, double freq, double q = 0.7071)
    {
        freq = std::clamp (freq, 10.0, sr * 0.47);
        const double w = 2.0 * kPi * freq / sr;
        const double alpha = std::sin (w) / (2.0 * q);
        const double c = std::cos (w);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 + c) * 0.5 / a0);
        b1 = (float) (-(1.0 + c) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
};

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
} // namespace vx
