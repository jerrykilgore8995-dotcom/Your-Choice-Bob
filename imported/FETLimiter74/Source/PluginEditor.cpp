#include "PluginEditor.h"

//==============================================================================
//  VintageLNF
//==============================================================================
VintageLNF::VintageLNF()
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff0a0a0a));
    setColour (juce::Label::textColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, juce::Colour (0xff181818));
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

juce::Font VintageLNF::getLabelFont (juce::Label&)
{
    return juce::Font ("Helvetica", 10.5f, juce::Font::bold);
}

//------------------------------------------------------------------------------
// Chunky latching pushbutton with red lamp inset
//------------------------------------------------------------------------------
void VintageLNF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                   bool /*highlighted*/, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    // Body - WHITE plastic key with slight bevel
    {
        juce::ColourGradient grad (juce::Colour (down ? 0xffdcdcdc : 0xfff5f5f5),
                                   r.getCentreX(), r.getY(),
                                   juce::Colour (0xff9a9a9a),
                                   r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 2.0f);

        // Top specular highlight
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.35f).reduced (2.0f, 1.0f), 2.0f);
    }

    // Lamp dot (upper region)
    {
        const float d  = juce::jmin (r.getWidth(), r.getHeight()) * 0.22f;
        const float cx = r.getCentreX();
        const float cy = r.getY() + r.getHeight() * 0.30f;
        auto lamp = juce::Rectangle<float> (cx - d * 0.5f, cy - d * 0.5f, d, d);
        if (on)
        {
            juce::ColourGradient lg (juce::Colour (0xffffb0b0), cx, cy - d * 0.3f,
                                     juce::Colour (0xffa10000), cx, cy + d * 0.4f, false);
            g.setGradientFill (lg);
            g.fillEllipse (lamp);
            g.setColour (juce::Colour (0xffff4040).withAlpha (0.25f));
            g.drawEllipse (lamp.expanded (1.5f), 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff5a1414));
            g.fillEllipse (lamp);
        }
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (lamp, 0.8f);
    }

    // Border
    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawRoundedRectangle (r, 2.0f, 1.2f);

    // Label text (black on white body)
    g.setColour (juce::Colour (0xff0a0a0a));
    g.setFont (juce::Font ("Helvetica", juce::jmin (12.0f, r.getHeight() * 0.28f), juce::Font::bold));
    g.drawText (b.getButtonText(),
                r.toNearestInt().removeFromBottom ((int) (r.getHeight() * 0.55f)),
                juce::Justification::centred);
}

//------------------------------------------------------------------------------
// Chicken-head trimmer knob
//------------------------------------------------------------------------------
void VintageLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                   float sliderPos, float startAng, float endAng,
                                   juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d  = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float r  = d * 0.5f;
    const float angle = startAng + sliderPos * (endAng - startAng);

    // Recessed black bezel ring
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);

    // Chicken-head body: BLACK bakelite diamond pointer
    const float br = r * 0.80f;
    juce::Path head;
    head.startNewSubPath (0, -br);                   // tip
    head.lineTo         ( br * 0.55f, -br * 0.15f);
    head.lineTo         ( br * 0.70f,  br * 0.60f);
    head.lineTo         (-br * 0.70f,  br * 0.60f);
    head.lineTo         (-br * 0.55f, -br * 0.15f);
    head.closeSubPath();

    juce::AffineTransform t = juce::AffineTransform::rotation (angle).translated (cx, cy);

    // Shadow
    {
        g.saveState();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (head, t.translated (0, 2));
        g.restoreState();
    }
    // Bakelite body - black
    g.setColour (juce::Colour (0xff0f0f0f));
    g.fillPath (head, t);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.strokePath (head, juce::PathStrokeType (1.0f), t);

    // White pointer stripe on the tip
    juce::Path tip;
    tip.addRoundedRectangle (-1.6f, -br * 0.95f, 3.2f, br * 0.35f, 1.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (tip, t);

    // Centre dome
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);

    // Tick marks around the recess (white on black panel)
    const int nTicks = 11;
    for (int i = 0; i < nTicks; ++i)
    {
        const float a = startAng + (float) i / (float) (nTicks - 1) * (endAng - startAng);
        const float r1 = r + 2.0f;
        const float r2 = r + 6.0f;
        g.setColour (juce::Colours::white);
        juce::Path tk;
        tk.startNewSubPath (cx + std::cos (a - juce::MathConstants<float>::halfPi) * r1,
                            cy + std::sin (a - juce::MathConstants<float>::halfPi) * r1);
        tk.lineTo          (cx + std::cos (a - juce::MathConstants<float>::halfPi) * r2,
                            cy + std::sin (a - juce::MathConstants<float>::halfPi) * r2);
        g.strokePath (tk, juce::PathStrokeType (1.0f));
    }
}

//------------------------------------------------------------------------------
// Vertical fader (VR1 input)
//------------------------------------------------------------------------------
void VintageLNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                   float sliderPos, float /*minPos*/, float /*maxPos*/,
                                   const juce::Slider::SliderStyle /*style*/, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // Recessed fader slot
    auto slot = bounds.withSizeKeepingCentre (6.0f, bounds.getHeight() - 20.0f);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (slot, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (slot, 2.0f, 1.0f);

    // Scale ticks on the right (WHITE on black panel)
    g.setColour (juce::Colours::white);
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = slot.getY() + slot.getHeight() * (float) i / 10.0f;
        const float tw = (i % 5 == 0) ? 10.0f : 5.0f;
        g.drawLine (slot.getRight() + 3.0f, ty, slot.getRight() + 3.0f + tw, ty, 1.0f);
    }

    // Fader cap - black plastic with white indicator stripe
    const float capW = bounds.getWidth() - 6.0f;
    const float capH = 22.0f;
    const float cy   = sliderPos;
    auto cap = juce::Rectangle<float> (bounds.getCentreX() - capW * 0.5f, cy - capH * 0.5f, capW, capH);

    juce::DropShadow sh (juce::Colours::black.withAlpha (0.55f), 4, { 0, 2 });
    juce::Path p; p.addRoundedRectangle (cap, 2.0f);
    sh.drawForPath (g, p);

    juce::ColourGradient grad (juce::Colour (0xff3a3a3a), cap.getCentreX(), cap.getY(),
                               juce::Colour (0xff0a0a0a), cap.getCentreX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.0f);

    g.setColour (juce::Colours::white);
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));

    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 2.0f, 0.8f);
}

//==============================================================================
//  LimitLamp
//==============================================================================
LimitLamp::LimitLamp (FETLimiter74Processor& p) : proc (p) { startTimerHz (30); }

void LimitLamp::timerCallback()
{
    // Lamp fully lit at >= 3 dB GR, dark below 1 dB, soft knee between.
    const float gr = proc.currentGrDb.load();
    const float target = juce::jlimit (0.0f, 1.0f, (gr - 1.0f) / 2.0f);
    brightness += (target - brightness) * 0.35f;   // envelope smoothing
    repaint();
}

void LimitLamp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto lamp = juce::Rectangle<float> (r.getCentreX() - d * 0.5f,
                                        r.getCentreY() - d * 0.5f, d, d);

    // Outer brass bezel
    juce::ColourGradient bez (juce::Colour (0xffa88b4a), lamp.getCentreX(), lamp.getY(),
                              juce::Colour (0xff4a3a1a), lamp.getCentreX(), lamp.getBottom(), false);
    g.setGradientFill (bez);
    g.fillEllipse (lamp);

    // Inset black ring
    auto inner = lamp.reduced (d * 0.14f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillEllipse (inner);

    // Filament glow
    const float bright = brightness;
    if (bright > 0.001f)
    {
        juce::ColourGradient glow (juce::Colour (0xffffb0a0).withAlpha (bright),
                                   inner.getCentreX(), inner.getCentreY(),
                                   juce::Colour (0xffa00000).withAlpha (bright * 0.5f),
                                   inner.getRight(), inner.getBottom(), true);
        g.setGradientFill (glow);
        g.fillEllipse (inner);

        // Outer red halo
        g.setColour (juce::Colour (0xffff2020).withAlpha (bright * 0.35f));
        g.drawEllipse (lamp.expanded (4.0f), 6.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff2a0a0a));
        g.fillEllipse (inner);
    }

    // Lamp specular
    g.setColour (juce::Colours::white.withAlpha (0.15f + bright * 0.4f));
    g.fillEllipse (inner.getX() + inner.getWidth() * 0.2f,
                   inner.getY() + inner.getHeight() * 0.12f,
                   inner.getWidth() * 0.4f, inner.getHeight() * 0.22f);
}

//==============================================================================
//  ChoiceButtons - internal helper for radio-grouped lamp pushbuttons
//  bound to an AudioParameterChoice.
//==============================================================================
namespace {

class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::AudioProcessorValueTreeState& s,
                   const juce::String& paramId,
                   std::initializer_list<juce::String> labels)
    {
        auto* parameter = s.getParameter (paramId);
        jassert (parameter != nullptr);

        int i = 0;
        for (auto& l : labels)
        {
            auto b = std::make_unique<juce::ToggleButton> (l);
            b->setClickingTogglesState (true);
            b->setRadioGroupId (1);
            const int idx = i++;
            auto* bPtr = b.get();
            b->onClick = [this, idx, bPtr]
            {
                if (bPtr->getToggleState())
                    attachment->setValueAsCompleteGesture (static_cast<float> (idx));
            };
            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
        }

        attachment = std::make_unique<juce::ParameterAttachment> (
            *parameter, [this] (float value) { syncButtons (static_cast<int> (value)); });
        attachment->sendInitialUpdate();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (buttons.empty()) return;
        const int w = r.getWidth() / (int) buttons.size();
        for (auto& b : buttons) b->setBounds (r.removeFromLeft (w).reduced (2));
    }

private:
    void syncButtons (int idx)
    {
        for (int i = 0; i < (int) buttons.size(); ++i)
            buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    }

    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace

//==============================================================================
//  FETLimiter74Editor
//==============================================================================
FETLimiter74Editor::FETLimiter74Editor (FETLimiter74Processor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- Brand strip ----
    brandLbl.setText ("FET LIMITER '74", juce::dontSendNotification);
    brandLbl.setFont (juce::Font ("Helvetica", 20.0f, juce::Font::bold));
    brandLbl.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (brandLbl);

    modelLbl.setText ("MODEL FL-74   STEREO FEEDFORWARD FET LIMITER", juce::dontSendNotification);
    modelLbl.setFont (juce::Font ("Helvetica", 10.0f, juce::Font::plain));
    modelLbl.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modelLbl);

    originLbl.setText ("EMERGENT AUDIO LABS", juce::dontSendNotification);
    originLbl.setFont (juce::Font ("Helvetica", 9.5f, juce::Font::plain));
    originLbl.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (originLbl);

    // ---- Input fader ----
    inputFader.setRange (-20.0, 40.0, 0.1);
    inputFader.setSkewFactor (0.6);
    inputFader.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (inputFader);
    inputAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "inputDb", inputFader);

    inputLbl.setText ("INPUT", juce::dontSendNotification);
    inputLbl.setJustificationType (juce::Justification::centred);
    inputLbl.setFont (juce::Font ("Helvetica", 11.0f, juce::Font::bold));
    addAndMakeVisible (inputLbl);

    // ---- Choice-button groups (MIC/LINE, Attack, Release, Output format) ----
    micLineGroup = std::make_unique<ChoiceButtons> (proc.apvts, "micLine",
                                                    std::initializer_list<juce::String> { "MIC", "LINE" });
    attackGroup  = std::make_unique<ChoiceButtons> (proc.apvts, "attackMode",
                                                    std::initializer_list<juce::String> { "FAST", "SLOW" });
    releaseGroup = std::make_unique<ChoiceButtons> (proc.apvts, "releaseMode",
                                                    std::initializer_list<juce::String> { "FAST", "AUTO", "MED", "SLOW" });
    outFmtGroup  = std::make_unique<ChoiceButtons> (proc.apvts, "outputFormat",
                                                    std::initializer_list<juce::String> { "0 dBm", "-30 dBm" });
    addAndMakeVisible (*micLineGroup);
    addAndMakeVisible (*attackGroup);
    addAndMakeVisible (*releaseGroup);
    addAndMakeVisible (*outFmtGroup);

    auto cap = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica", 10.5f, juce::Font::bold));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    cap (srcLbl,     "SOURCE");
    cap (attackLbl,  "ATTACK");
    cap (releaseLbl, "RELEASE");
    cap (outLvlLbl,  "OUTPUT LEVEL");
    cap (limiterLbl, "LIMITER");
    cap (linkLbl,    "STEREO LINK");
    cap (psuLbl,     "PSU RIPPLE");

    // ---- IN/OUT bypass + stereo link + PSU ripple ----
    limiterInBtn.setClickingTogglesState (true);
    linkBtn     .setClickingTogglesState (true);
    psuBtn      .setClickingTogglesState (true);
    addAndMakeVisible (limiterInBtn);
    addAndMakeVisible (linkBtn);
    addAndMakeVisible (psuBtn);
    limiterAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "limiterOn",  limiterInBtn);
    linkAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "stereoLink", linkBtn);
    psuAtt     = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "psuRipple",  psuBtn);

    // ---- Trimmers ----
    auto initTrim = [this] (juce::Slider& k, juce::Label& l, const juce::String& text,
                            const juce::String& paramId,
                            std::unique_ptr<APVTS::SliderAttachment>& att)
    {
        k.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (k);
        att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, k);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica", 10.5f, juce::Font::bold));
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    initTrim (thresholdK, thresholdLbl, "THRESHOLD",  "thresholdDb", thresholdAtt);
    initTrim (ledCalK,    ledCalLbl,    "LED CAL",    "ledCal",      ledCalAtt);
    initTrim (distCompK,  distCompLbl,  "DIST COMP",  "distComp",    distCompAtt);
    initTrim (outputK,    outputLbl,    "OUTPUT",     "outputDb",    outputAtt);

    // ---- LIMIT lamp ----
    limitLamp = std::make_unique<LimitLamp> (proc);
    addAndMakeVisible (*limitLamp);
    cap (limitLampLbl, "LIMIT  > 3 dB");

    setResizable (false, false);
    setSize (980, 320);
    buildFaceplate();
}

//------------------------------------------------------------------------------
void FETLimiter74Editor::buildFaceplate()
{
    faceplate = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (faceplate);

    // All-black painted steel face
    g.fillAll (juce::Colour (0xff0a0a0a));

    // Very subtle vertical gradient for depth
    juce::ColourGradient base (juce::Colour (0xff161616), 0, 0,
                               juce::Colour (0xff050505), 0, (float) getHeight(), false);
    g.setGradientFill (base);
    g.fillAll();

    // Subtle brushed texture
    juce::Random rnd (0xBEEF99);
    for (int i = 0; i < getHeight() * 2; ++i)
    {
        const int y = rnd.nextInt (getHeight());
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f));
        g.drawLine (0.0f, (float) y, (float) getWidth(), (float) y, 0.5f);
    }

    // Darker inset panel area (central working zone)
    auto panel = getLocalBounds().reduced (14, 14);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawRect (panel, 1);

    // Four corner rack screws
    auto drawScrew = [&] (int x, int y)
    {
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillEllipse ((float) x - 6, (float) y - 6, 12, 12);
        g.setColour (juce::Colour (0xff555555));
        g.drawEllipse ((float) x - 6, (float) y - 6, 12, 12, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawLine ((float) x - 4, (float) y - 1, (float) x + 4, (float) y + 1, 1.2f);
    };
    drawScrew (18, 18);
    drawScrew (getWidth() - 18, 18);
    drawScrew (18, getHeight() - 18);
    drawScrew (getWidth() - 18, getHeight() - 18);

    // Red hairline under brand strip
    g.setColour (juce::Colour (0xffc81414));
    g.fillRect (28, 52, getWidth() - 56, 2);
}

void FETLimiter74Editor::paint (juce::Graphics& g)
{
    g.drawImageAt (faceplate, 0, 0);
}

//------------------------------------------------------------------------------
void FETLimiter74Editor::resized()
{
    if (faceplate.getWidth() != getWidth() || faceplate.getHeight() != getHeight())
        buildFaceplate();

    auto r = getLocalBounds().reduced (28, 20);

    // ---- Brand strip ----
    auto brand = r.removeFromTop (40);
    brandLbl.setBounds (brand.removeFromLeft (240));
    originLbl.setBounds (brand.removeFromRight (220));
    modelLbl.setBounds (brand);

    r.removeFromTop (10);

    // ---- Main working area ----
    // LEFT: Input fader bay (120 wide)
    auto left = r.removeFromLeft (110);
    inputLbl.setBounds (left.removeFromTop (16));
    inputFader.setBounds (left.reduced (10, 4));

    r.removeFromLeft (10);

    // RIGHT: Trim knobs + LIMIT lamp (260 wide)
    auto right = r.removeFromRight (300);
    {
        auto top = right.removeFromTop (right.getHeight() / 2).reduced (0, 4);
        const int kw = top.getWidth() / 2;

        auto trimLayout = [&] (juce::Rectangle<int> cell, juce::Slider& k, juce::Label& l)
        {
            l.setBounds (cell.removeFromTop (14));
            cell.removeFromTop (2);
            k.setBounds (cell.reduced (10, 0));
        };
        trimLayout (top.removeFromLeft (kw), thresholdK, thresholdLbl);
        trimLayout (top.removeFromLeft (kw), ledCalK,    ledCalLbl);

        auto bot = right;
        trimLayout (bot.removeFromLeft (kw), distCompK, distCompLbl);
        trimLayout (bot.removeFromLeft (kw), outputK,   outputLbl);
    }

    r.removeFromRight (10);

    // CENTER: pushbutton banks
    // Row 1: LIMITER (IN) + SOURCE (MIC/LINE)   + LIMIT lamp
    auto center = r;
    auto row1 = center.removeFromTop (center.getHeight() / 2).reduced (0, 4);

    const int c1w = row1.getWidth() / 4;         // IN button
    const int c2w = row1.getWidth() / 4;         // MIC/LINE
    const int c3w = row1.getWidth() * 2 / 4;     // LIMIT lamp

    auto cell = [&] (juce::Rectangle<int> cellR, juce::Label& capLbl, juce::Component& ctrl, int ctrlH)
    {
        capLbl.setBounds (cellR.removeFromTop (14));
        cellR.removeFromTop (2);
        ctrl.setBounds (cellR.removeFromTop (ctrlH).reduced (6, 0));
    };

    cell (row1.removeFromLeft (c1w), limiterLbl, limiterInBtn, 54);
    cell (row1.removeFromLeft (c2w), srcLbl,     *micLineGroup, 54);

    // LIMIT lamp occupies the right half of row 1
    auto lampCell = row1;
    limitLampLbl.setBounds (lampCell.removeFromTop (14));
    lampCell.removeFromTop (2);
    auto lampSquare = lampCell.withSizeKeepingCentre (54, 54);
    limitLamp->setBounds (lampSquare);
    juce::ignoreUnused (c3w);

    // Row 2: ATTACK + RELEASE + STEREO LINK + PSU + OUTPUT LEVEL
    center.removeFromTop (6);
    auto row2 = center;
    const int atkW   = row2.getWidth() * 20 / 100;
    const int relW   = row2.getWidth() * 36 / 100;
    const int linkW  = row2.getWidth() * 14 / 100;
    const int psuW   = row2.getWidth() * 14 / 100;
    const int outLvW = row2.getWidth() - atkW - relW - linkW - psuW;

    cell (row2.removeFromLeft (atkW),   attackLbl,  *attackGroup,  54);
    cell (row2.removeFromLeft (relW),   releaseLbl, *releaseGroup, 54);
    cell (row2.removeFromLeft (linkW),  linkLbl,    linkBtn,       54);
    cell (row2.removeFromLeft (psuW),   psuLbl,     psuBtn,        54);
    cell (row2.removeFromLeft (outLvW), outLvlLbl,  *outFmtGroup,  54);
}
