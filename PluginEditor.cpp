#include "PluginEditor.h"

namespace
{
const juce::Colour kBackground (0xff161a22);
const juce::Colour kPanel      (0xff222835);
const juce::Colour kAccent     (0xffff8a1f);
const juce::Colour kGood       (0xff5fd38d);
const juce::Colour kBad        (0xffff6b6b);
const juce::Colour kDim        (0xff8b93a7);
}

BytebeatEditor::BytebeatEditor (BytebeatProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    lnf.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, kPanel.brighter (0.2f));
    lnf.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    lnf.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::ComboBox::backgroundColourId, kPanel);
    lnf.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    lnf.setColour (juce::ComboBox::outlineColourId, kPanel.brighter (0.2f));
    lnf.setColour (juce::PopupMenu::backgroundColourId, kPanel);
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);

    titleLabel.setText ("BYTEBEAT SYNTH", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (titleLabel);

    // presets
    const auto& presets = BytebeatProcessor::getPresets();
    for (size_t i = 0; i < presets.size(); ++i)
        presetBox.addItem (presets[i].name, (int) i + 1);
    presetBox.setTextWhenNothingSelected ("Custom formula");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx < 0)
            return;
        processor.setCurrentProgram (idx);
        formulaBox.setText (processor.getFormulaText(), juce::dontSendNotification);
        showStatus (processor.getFormulaError());
    };
    addAndMakeVisible (presetBox);

    // formula field
    formulaBox.setMultiLine (false);
    formulaBox.setReturnKeyStartsNewLine (false);
    formulaBox.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::plain));
    formulaBox.setJustification (juce::Justification::centredLeft);
    formulaBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0e1117));
    formulaBox.setColour (juce::TextEditor::textColourId, juce::Colour (0xffffd9a8));
    formulaBox.setColour (juce::TextEditor::outlineColourId, kPanel.brighter (0.2f));
    formulaBox.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    formulaBox.setColour (juce::CaretComponent::caretColourId, kAccent);
    formulaBox.setIndents (12, 0);
    formulaBox.setText (processor.getFormulaText(), juce::dontSendNotification);
    formulaBox.onTextChange = [this] { applyFormulaFromEditor(); };
    formulaBox.onReturnKey  = [this] { applyFormulaFromEditor(); };
    addAndMakeVisible (formulaBox);

    statusLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (statusLabel);
    showStatus (processor.getFormulaError());

    // knobs
    addKnob (knobs[0], "rate",    "RATE", " Hz");
    addKnob (knobs[1], "octave",  "OCTAVE");
    addKnob (knobs[2], "coarse",  "SEMI");
    addKnob (knobs[3], "fine",    "CENTS");
    addKnob (knobs[4], "crush",   "BITS");
    addKnob (knobs[5], "attack",  "ATTACK", " s");
    addKnob (knobs[6], "release", "RELEASE", " s");
    addKnob (knobs[7], "volume",  "VOLUME");

    helpLabel.setText ("t = time   n = MIDI note   v = velocity   f = note Hz        "
                       "+ - * / %   << >>   & | ^ ~ !   < > <= >= == !=   && ||   a ? b : c        "
                       "sin cos abs min max sqrt        output = value & 255",
                       juce::dontSendNotification);
    helpLabel.setFont (juce::FontOptions (12.5f));
    helpLabel.setColour (juce::Label::textColourId, kDim);
    helpLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (helpLabel);

    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (24.0f);
    keyboard.setLowestVisibleKey (48);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, kAccent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kAccent.withAlpha (0.4f));
    addAndMakeVisible (keyboard);

    setSize (760, 440);
    startTimerHz (10);
    timerCallback();
}

BytebeatEditor::~BytebeatEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BytebeatEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& title, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setDoubleClickReturnValue (true, processor.apvts.getParameter (paramId)->convertFrom0to1 (
                                                  processor.apvts.getParameter (paramId)->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void BytebeatEditor::applyFormulaFromEditor()
{
    const auto error = processor.setFormulaText (formulaBox.getText());
    presetBox.setSelectedItemIndex (-1, juce::dontSendNotification);
    showStatus (error);
}

void BytebeatEditor::showStatus (const juce::String& error)
{
    if (error.isEmpty())
    {
        statusLabel.setText ("OK - formula compiled", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, kGood);
    }
    else
    {
        statusLabel.setText ("Error: " + error + "  (previous formula keeps playing)", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, kBad);
    }
}

void BytebeatEditor::timerCallback()
{
    // follow changes coming from the host (program change, state restore)
    if (! formulaBox.hasKeyboardFocus (true) && formulaBox.getText() != processor.getFormulaText())
    {
        formulaBox.setText (processor.getFormulaText(), juce::dontSendNotification);
        showStatus (processor.getFormulaError());
    }

    const auto& presets = BytebeatProcessor::getPresets();
    int match = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (processor.getFormulaText() == presets[i].formula)
            match = (int) i;

    if (presetBox.getSelectedItemIndex() != match)
        presetBox.setSelectedItemIndex (match, juce::dontSendNotification);
}

void BytebeatEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (kPanel);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f).withHeight (232.0f).withY (52.0f), 8.0f);
}

void BytebeatEditor::resized()
{
    auto r = getLocalBounds().reduced (14);

    auto top = r.removeFromTop (32);
    presetBox.setBounds (top.removeFromRight (220));
    titleLabel.setBounds (top);

    r.removeFromTop (14);
    r.reduce (6, 0);

    formulaBox.setBounds (r.removeFromTop (48));
    statusLabel.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto knobRow = r.removeFromTop (118);
    const int colW = knobRow.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = knobRow.removeFromLeft (colW);
        k.label.setBounds (col.removeFromTop (18));
        k.slider.setBounds (col);
    }

    r.removeFromTop (14);
    helpLabel.setBounds (r.removeFromTop (34));

    keyboard.setBounds (getLocalBounds().reduced (8).removeFromBottom (100));
}
