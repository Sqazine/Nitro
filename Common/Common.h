#pragma once
#include <JuceHeader.h>
#include "PluginParameterComboBox.h"
#include "PluginParameterSlider.h"
#include "PluginParameterToggle.h"
#include "Utils.h"

using DspIIRFilterFloat = juce::dsp::IIR::Filter<float>;
using DspIIRCoefficientsFloat = juce::dsp::IIR::Coefficients<float>;