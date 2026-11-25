#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
//  VintageLNF - '70s British studio-rack LookAndFeel.
//==============================================================================
class VintageLNF : public juce::LookAndFeel_V4
{
public:
    VintageLNF();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool highlighted, bool down) override;

    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
//  LIMIT lamp - round red filament lamp driven by live gain reduction
//==============================================================================
class LimitLamp : public juce::Component, private juce::Timer
{
public:
    explicit LimitLamp (FETLimiter74Processor& p);
    ~LimitLamp() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    FETLimiter74Processor& proc;
    float brightness { 0.0f };    // 0..1 displayed
};

//==============================================================================
class FETLimiter74Editor  : public juce::AudioProcessorEditor
{
public:
    explicit FETLimiter74Editor (FETLimiter74Processor&);
    ~FETLimiter74Editor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    FETLimiter74Processor& proc;
    VintageLNF lnf;

    // Children owned opaquely (ChoiceButtons helper is defined in the .cpp).
    std::unique_ptr<juce::Component> micLineGroup;
    std::unique_ptr<juce::Component> attackGroup;
    std::unique_ptr<juce::Component> releaseGroup;
    std::unique_ptr<juce::Component> outFmtGroup;

    // Captions for the button rows
    juce::Label srcLbl, attackLbl, releaseLbl, outLvlLbl;

    // IN/OUT bypass + stereo link + PSU ripple
    juce::ToggleButton limiterInBtn { "IN" };
    juce::ToggleButton linkBtn      { "LINK" };
    juce::ToggleButton psuBtn       { "RIPPLE" };
    juce::Label limiterLbl, linkLbl, psuLbl;
    std::unique_ptr<APVTS::ButtonAttachment> limiterAtt, linkAtt, psuAtt;

    // Input fader (VR1) + scale labels
    juce::Slider inputFader { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label  inputLbl;
    std::unique_ptr<APVTS::SliderAttachment> inputAtt;

    // Trimmers: VR2 THRESHOLD, VR3 LED CAL, VR4 DIST COMP, OUTPUT
    juce::Slider thresholdK  { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider ledCalK     { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider distCompK   { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider outputK     { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  thresholdLbl, ledCalLbl, distCompLbl, outputLbl;
    std::unique_ptr<APVTS::SliderAttachment> thresholdAtt, ledCalAtt, distCompAtt, outputAtt;

    // LIMIT lamp
    std::unique_ptr<LimitLamp> limitLamp;
    juce::Label limitLampLbl;

    // Brand strip
    juce::Label brandLbl, modelLbl, originLbl;

    juce::Image faceplate;
    void buildFaceplate();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FETLimiter74Editor)
};
