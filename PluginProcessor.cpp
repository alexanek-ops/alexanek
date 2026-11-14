#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::Identifier kFormulaProp ("formula");
}

const std::vector<BytebeatProcessor::Preset>& BytebeatProcessor::getPresets()
{
    static const std::vector<Preset> presets {
        { "Crunch",        "t*2|t|t/2" },
        { "Saw",           "t*2" },
        { "Square",        "t&128?255:0" },
        { "Pulse 25%",     "(t&255)<64?255:0" },
        { "Triangle",      "(t&128?~t:t)*2" },
        { "Sine",          "128+sin(t)" },
        { "Sierpinski",    "t&t>>8" },
        { "Rising Steps",  "t*(1+(t>>10&3))" },
        { "Arp Machine",   "t*((t>>12|t>>8)&63&t>>4)" },
        { "Wobble",        "t*(t>>9|t>>13)" },
        { "Noise Burst",   "(t*t>>7)^(t>>3)" },
        { "Velocity Saw",  "(t*2)&(v*2)" },
    };
    return presets;
}

BytebeatProcessor::BytebeatProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pRate    = apvts.getRawParameterValue ("rate");
    pOctave  = apvts.getRawParameterValue ("octave");
    pCoarse  = apvts.getRawParameterValue ("coarse");
    pFine    = apvts.getRawParameterValue ("fine");
    pCrush   = apvts.getRawParameterValue ("crush");
    pAttack  = apvts.getRawParameterValue ("attack");
    pRelease = apvts.getRawParameterValue ("release");
    pVolume  = apvts.getRawParameterValue ("volume");

    setFormulaText (getPresets().front().formula);
}

juce::AudioProcessorValueTreeState::ParameterLayout BytebeatProcessor::createLayout()
{
    using juce::AudioParameterFloat;
    using juce::AudioParameterInt;
    using juce::ParameterID;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "rate", 1 }, "Rate",
                                                       Range (100.0f, 48000.0f, 0.0f, 0.35f), 8000.0f,
                                                       juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -3, 3, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "coarse", 1 }, "Semitones", -12, 12, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fine", 1 }, "Cents",
                                                       Range (-100.0f, 100.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "crush", 1 }, "Bits", 1, 8, 8));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
                                                       Range (0.0f, 2.0f, 0.0f, 0.4f), 0.002f,
                                                       juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
                                                       Range (0.0f, 3.0f, 0.0f, 0.4f), 0.15f,
                                                       juce::AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
                                                       Range (0.0f, 1.0f, 0.0f), 0.7f));
    return layout;
}

bb::SynthParams BytebeatProcessor::readParams() const
{
    bb::SynthParams p;
    p.rate      = pRate->load();
    p.octave    = (int) std::lround (pOctave->load());
    p.coarse    = pCoarse->load();
    p.fine      = pFine->load();
    p.crushBits = (int) std::lround (pCrush->load());
    p.attack    = pAttack->load();
    p.release   = pRelease->load();
    p.volume    = pVolume->load();
    return p;
}

void BytebeatProcessor::prepareToPlay (double sampleRate, int)
{
    core.prepare (sampleRate);
    keyboardState.reset();
}

bool BytebeatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BytebeatProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        core.noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff())
        core.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        core.allNotesOff();
}

void BytebeatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // notes played on the on-screen keyboard
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    const auto formulaForThisBlock = std::atomic_load (&activeFormula);
    core.setFormula (formulaForThisBlock.get());
    const auto params = readParams();

    float* out = buffer.getWritePointer (0);
    int pos = 0;

    for (const auto meta : midi)
    {
        const int at = juce::jlimit (pos, numSamples, meta.samplePosition);
        if (at > pos)
            core.render (out + pos, at - pos, params);
        pos = at;
        handleMidi (meta.getMessage());
    }

    if (pos < numSamples)
        core.render (out + pos, numSamples - pos, params);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

juce::String BytebeatProcessor::setFormulaText (const juce::String& text)
{
    formulaText = text;
    apvts.state.setProperty (kFormulaProp, text, nullptr);

    auto compiled = std::make_unique<bb::Formula>();
    std::string error;

    if (! compiled->compile (text.toStdString(), error))
    {
        formulaError = juce::String (error);
        return formulaError;
    }

    std::shared_ptr<const bb::Formula> ready (std::move (compiled));
    std::atomic_store (&activeFormula, std::move (ready));
    formulaError.clear();
    return {};
}

int BytebeatProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

const juce::String BytebeatProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void BytebeatProcessor::setCurrentProgram (int index)
{
    const auto& p = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size()))
        return;

    currentProgram = index;
    setFormulaText (p[(size_t) index].formula);
}

void BytebeatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty (kFormulaProp, formulaText, nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BytebeatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const auto saved = apvts.state.getProperty (kFormulaProp, getPresets().front().formula).toString();
            setFormulaText (saved);
        }
    }
}

juce::AudioProcessorEditor* BytebeatProcessor::createEditor()
{
    return new BytebeatEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BytebeatProcessor();
}
