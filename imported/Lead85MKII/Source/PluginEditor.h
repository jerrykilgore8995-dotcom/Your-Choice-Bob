#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
//  Lead85MKIILNF - black-tolex panel, chrome accents, chicken-head knobs,
//  amber pull-switch indicator lamps.  Original visual design inspired by
//  '80s American boutique guitar preamp aesthetic.
//==============================================================================
class Lead85MKIILNF : public juce::LookAndFeel_V4
{
public:
    Lead85MKIILNF();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float startAng, float endAng,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style, juce::Slider&) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool highlighted, bool down) override;

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (10.5f, juce::Font::bold));
    }
};

//==============================================================================
//  Chicken-head knob with its own white caption AND an optional small amber
//  "PULL" lamp + micro-button to the side that drives a bool parameter.
//==============================================================================
class ChickenKnob : public juce::Component
{
public:
    ChickenKnob (juce::AudioProcessorValueTreeState& s,
                 const juce::String& paramId,
                 const juce::String& caption,
                 const juce::String& pullParamId = {},
                 const juce::String& pullCaption = {});

    void resized() override;

private:
    juce::Slider knob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label cap;
    juce::ToggleButton pullBtn { "PULL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pullAtt;
    bool hasPull { false };
};

//==============================================================================
class Lead85MKIIEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Lead85MKIIEditor (Lead85MKIIProcessor&);
    ~Lead85MKIIEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    Lead85MKIIProcessor& proc;
    Lead85MKIILNF lnf;

    // Row 1 - knob row (chicken-heads with optional pull switches)
    std::unique_ptr<ChickenKnob> kVol1, kTreble, kMiddle, kBass, kMaster1,
                                 kLeadDrive, kLeadMaster, kPresence, kReverb;

    // Input + Output + Lead mode + Class + Power
    juce::Slider inputKnob   { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider outputKnob  { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label inputLbl, outputLbl;
    juce::ToggleButton standbyBtn { "STANDBY" };
    juce::ToggleButton cabinetBtn { "CAB" };
    juce::TextButton loadIR { "Load WAV..." };
    juce::ComboBox factoryIR;
    juce::Label irName;
    juce::String displayedIR;
    std::unique_ptr<juce::FileChooser> chooser;
    void timerCallback() override;
    void chooseIR();
    juce::ToggleButton leadBtn  { "LEAD" };
    juce::ToggleButton classBtn { "CLASS A" };
    juce::ToggleButton reverbOnBtn { "REV IN" };
    juce::ToggleButton powerBtn { "POWER" };
    std::unique_ptr<APVTS::SliderAttachment> inAtt, outAtt;
    std::unique_ptr<APVTS::ButtonAttachment> leadAtt, classAtt, reverbOnAtt, powerAtt, standbyAtt, cabinetAtt;

    // Row 2 - graphic EQ (5 sliders + ON switch + freq labels)
    std::array<juce::Slider, 5> geqBands;
    std::array<juce::Label,  5> geqLbls;
    std::array<std::unique_ptr<APVTS::SliderAttachment>, 5> geqAtts;
    juce::ToggleButton geqOnBtn { "EQ IN" };
    std::unique_ptr<APVTS::ButtonAttachment> geqOnAtt;

    // Power/drive LED
    class DriveLed : public juce::Component, private juce::Timer
    {
    public:
        DriveLed (Lead85MKIIProcessor& p) : proc (p) { startTimerHz (30); }
        ~DriveLed() override { stopTimer(); }
        void paint (juce::Graphics& g) override;
    private:
        void timerCallback() override { b = proc.currentDriveLedBrightness.load(); repaint(); }
        Lead85MKIIProcessor& proc;
        float b { 0.0f };
    };
    std::unique_ptr<DriveLed> driveLed;

    juce::Image faceplate;
    void buildFaceplate();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Lead85MKIIEditor)
};
