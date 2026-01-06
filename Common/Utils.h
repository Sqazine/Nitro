#pragma once
#include <JuceHeader.h>
constexpr float TWO_PI = 2.0f * 3.1415926535f;

enum Waveform
{
    SINE = 0,
    TRIANGLE,
    SWATOOTH,
    INVERSE_SWATOOTH
};

enum Interpolation
{
    NEAREST_NEIGHBOUR = 0,
    LINEAR,
    CUBIC
};
const juce::StringArray mWaveformItemsUI =
    {
        "Sine",
        "Triangle",
        "Sawtooth(Rising)",
        "Sawtooth(Falling)",
};

const juce::StringArray mInterpolationItemsUI =
    {
        "NearestNeighbour",
        "Linear",
        "Cubic",
};

float Lfo(float phase, Waveform waveform);