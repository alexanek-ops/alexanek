#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>

class BytebeatEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit BytebeatEditor (BytebeatProcessor&);
    ~BytebeatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& title, const juce::String& suffix = {});
    void applyFormulaFromEditor();
    void showStatus (const juce::String& error);
    void timerCallback() override;

    BytebeatProcessor& processor;
    juce::LookAndFeel_V4 lnf;

    juce::Label titleLabel;
    juce::ComboBox presetBox;
    juce::TextEditor formulaBox;
    juce::Label statusLabel;
    juce::Label helpLabel;

    std::array<Knob, 8> knobs;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BytebeatEditor)
};
