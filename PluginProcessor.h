#pragma once

#include "FormulaEngine.h"
#include "SynthCore.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <memory>
#include <vector>

class BytebeatProcessor : public juce::AudioProcessor
{
public:
    struct Preset
    {
        const char* name;
        const char* formula;
    };

    BytebeatProcessor();
    ~BytebeatProcessor() override = default;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- formula (message thread only) ---
    // Compiles and activates the formula. Returns an empty string on success or a
    // readable error. On error the previous valid formula keeps playing, but the
    // text is still remembered so it is saved with the project.
    juce::String setFormulaText (const juce::String& text);
    juce::String getFormulaText() const { return formulaText; }
    juce::String getFormulaError() const { return formulaError; }

    static const std::vector<Preset>& getPresets();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    bb::SynthParams readParams() const;
    void handleMidi (const juce::MidiMessage& m);

    bb::SynthCore core;

    // shared_ptr atomic operations keep a compiled formula alive for the full
    // audio block, even while the editor compiles a replacement on the UI thread.
    std::shared_ptr<const bb::Formula> activeFormula;

    juce::String formulaText;
    juce::String formulaError;
    int currentProgram = 0;

    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pOctave = nullptr;
    std::atomic<float>* pCoarse = nullptr;
    std::atomic<float>* pFine = nullptr;
    std::atomic<float>* pCrush = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pVolume = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BytebeatProcessor)
};
