#include "PluginProcessor.h"
#include "PluginEditor.h"

TunerAudioProcessorEditor::TunerAudioProcessorEditor (TunerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 300);
    startTimer(30);
}

TunerAudioProcessorEditor::~TunerAudioProcessorEditor()
{
    stopTimer();
}

void TunerAudioProcessorEditor::timerCallback()
{
    repaint();
}

void TunerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    
    auto bounds = getLocalBounds();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();
    auto centerX = width / 2;
    
    bool noteValid = audioProcessor.isNoteValid();
    int detectedNote = audioProcessor.getDetectedNote();
    float centsOff = audioProcessor.getCentsOff();
    float frequency = audioProcessor.getDetectedFrequency();
    float amplitude = audioProcessor.getSignalAmplitude();
    
    // Note name area (top half)
    auto noteArea = juce::Rectangle<int>(0, 20, width, height / 2 - 20);
    g.setColour(juce::Colours::white);
    
    if (noteValid && detectedNote >= 0)
    {
        g.setFont(70.0f);
        g.drawFittedText(getNoteName(detectedNote), noteArea, juce::Justification::centred, 1);
    }
    else
    {
        g.setFont(30.0f);
        g.drawFittedText("---", noteArea, juce::Justification::centred, 1);
    }
    
    // Cents text (middle)
    auto centsArea = juce::Rectangle<int>(0, height / 2, width, 40);
    g.setFont(24.0f);
    g.setColour(juce::Colours::white);
    
    if (noteValid)
    {
        juce::String centsText;
        if (centsOff >= 0)
            centsText = "+" + juce::String(centsOff, 1) + " cents";
        else
            centsText = juce::String(centsOff, 1) + " cents";
        g.drawFittedText(centsText, centsArea, juce::Justification::centred, 1);
    }
    
    // Cents indicator bar
    auto barWidth = 200;
    auto barHeight = 16;
    auto barY = height / 2 + 45;
    auto barX = centerX - barWidth / 2;
    
    g.setColour(juce::Colours::darkgrey);
    g.fillRoundedRectangle(barX, barY, barWidth, barHeight, 5.0f);
    
    if (noteValid)
    {
        float normalizedCents = juce::jlimit(-50.0f, 50.0f, centsOff) / 50.0f;
        auto indicatorX = barX + barWidth / 2 + normalizedCents * (barWidth / 2) - 3;
        
        juce::Colour indicatorColour = juce::Colours::green;
        if (std::abs(centsOff) > 10.0f)
            indicatorColour = juce::Colours::orange;
        if (std::abs(centsOff) > 25.0f)
            indicatorColour = juce::Colours::red;
            
        g.setColour(indicatorColour);
        g.fillRoundedRectangle(indicatorX, barY - 2, 6, barHeight + 4, 2.0f);
    }
    
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(barX, barY, barWidth, barHeight, 5.0f, 1.0f);
    
    // Frequency display
    auto freqArea = juce::Rectangle<int>(0, barY + 30, width, 30);
    g.setColour(juce::Colours::grey);
    g.setFont(16.0f);
    
    if (noteValid && frequency > 0.0f)
    {
        g.drawFittedText(juce::String(frequency, 1) + " Hz", freqArea, juce::Justification::centred, 1);
    }
    
    // Signal level bar
    auto levelWidth = 120;
    auto levelHeight = 8;
    auto levelY = height - 35;
    auto levelX = centerX - levelWidth / 2;
    
    g.setColour(juce::Colours::darkgrey);
    g.fillRoundedRectangle(levelX, levelY, levelWidth, levelHeight, 3.0f);
    
    g.setColour(noteValid ? juce::Colours::green : juce::Colours::orange);
    auto levelFillWidth = juce::jlimit(0.0f, 1.0f, amplitude * 5.0f) * levelWidth;
    g.fillRoundedRectangle(levelX, levelY, levelFillWidth, levelHeight, 3.0f);
}

void TunerAudioProcessorEditor::resized()
{
}

juce::String TunerAudioProcessorEditor::getNoteName(int midiNote) const
{
    static const juce::String noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    return noteNames[noteIndex] + juce::String(octave);
}
