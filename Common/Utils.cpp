#include "Utils.h"

float Lfo(float phase, Waveform waveform)
{
    float outData = 0.0f;

    switch (waveform)
    {
    case SINE:
        outData = 0.5f + 0.5f * sinf(TWO_PI * phase);
        break;
    case TRIANGLE:
    {
        if (phase < 0.25f)
            outData = 0.5f + 2.0f * phase;
        else if (phase < 0.75f)
            outData = 1.0f - 2.0f * (phase - 0.25f);
        else
            outData = 2.0f * (phase - 0.75f);
        break;
    }
    case SWATOOTH:
    {
        if (phase < 0.5f)
            outData = 0.5f + phase;
        else
            outData = phase - 0.5f;
        break;
    }
    case INVERSE_SWATOOTH:
    {
        if (phase < 0.5f)
            outData = 0.5f - phase;
        else
            outData = 1.5f - phase;
        break;
    }
    }

    return outData;
}
