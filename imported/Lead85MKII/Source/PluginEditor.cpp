#include "PluginEditor.h"

//==============================================================================
Lead85MKIILNF::Lead85MKIILNF()
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff0b0b0b));
    setColour (juce::Label::textColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

//------------------------------------------------------------------------------
// Chicken-head rotary (dark bakelite, white pointer stripe, chrome collar)
//------------------------------------------------------------------------------
void Lead85MKIILNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                  float sliderPos, float startAng, float endAng,
                                  juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d  = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float r  = d * 0.5f;
    const float angle = startAng + sliderPos * (endAng - startAng);

    // Chrome collar
    {
        juce::ColourGradient collar (juce::Colour (0xffd0d0d0), cx, cy - r,
                                     juce::Colour (0xff303030), cx, cy + r, false);
        g.setGradientFill (collar);
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (juce::Colour (0xff0a0a0a));
        g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.0f);
    }

    // Recessed inner shadow
    const float ir = r * 0.90f;
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillEllipse (cx - ir, cy - ir, ir * 2, ir * 2);

    // Chicken-head body - dark bakelite diamond
    const float br = ir * 0.90f;
    juce::Path head;
    head.startNewSubPath (0, -br);
    head.lineTo         ( br * 0.55f, -br * 0.15f);
    head.lineTo         ( br * 0.70f,  br * 0.60f);
    head.lineTo         (-br * 0.70f,  br * 0.60f);
    head.lineTo         (-br * 0.55f, -br * 0.15f);
    head.closeSubPath();

    juce::AffineTransform t = juce::AffineTransform::rotation (angle).translated (cx, cy);

    // Shadow
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (head, t.translated (0, 2));

    // Body
    juce::ColourGradient bg (juce::Colour (0xff2a2a2a), cx, cy - br,
                             juce::Colour (0xff050505), cx, cy + br, false);
    g.setGradientFill (bg);
    g.fillPath (head, t);
    g.setColour (juce::Colour (0xff3a3a3a));
    g.strokePath (head, juce::PathStrokeType (1.0f), t);

    // White pointer stripe on the tip
    juce::Path tip;
    tip.addRoundedRectangle (-1.6f, -br * 0.95f, 3.2f, br * 0.40f, 1.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (tip, t);

    // Centre screw dome
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawLine (cx - 2.0f, cy, cx + 2.0f, cy, 1.0f);

    // Position mark at the chrome collar edge (small white tick, moves with knob)
    const float mr = r * 0.95f;
    const float mx = cx + std::cos (angle - juce::MathConstants<float>::halfPi) * mr;
    const float my = cy + std::sin (angle - juce::MathConstants<float>::halfPi) * mr;
    g.setColour (juce::Colours::white);
    g.fillEllipse (mx - 1.8f, my - 1.8f, 3.6f, 3.6f);

    // Scale ticks (0..10 numbers skipped for brevity; tick marks engraved white)
    const int nTicks = 11;
    for (int i = 0; i < nTicks; ++i)
    {
        const float a = startAng + (float) i / (float) (nTicks - 1) * (endAng - startAng);
        const float r1 = r + 2.0f;
        const float r2 = r + 7.0f;
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        juce::Path tk;
        tk.startNewSubPath (cx + std::cos (a - juce::MathConstants<float>::halfPi) * r1,
                            cy + std::sin (a - juce::MathConstants<float>::halfPi) * r1);
        tk.lineTo          (cx + std::cos (a - juce::MathConstants<float>::halfPi) * r2,
                            cy + std::sin (a - juce::MathConstants<float>::halfPi) * r2);
        g.strokePath (tk, juce::PathStrokeType (1.0f));
    }
}

//------------------------------------------------------------------------------
// Graphic EQ vertical slider (black cap on chrome slot)
//------------------------------------------------------------------------------
void Lead85MKIILNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                  float sliderPos, float /*minPos*/, float /*maxPos*/,
                                  const juce::Slider::SliderStyle /*style*/, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // Recessed slot
    auto slot = bounds.withSizeKeepingCentre (4.0f, bounds.getHeight() - 24.0f);
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (slot, 1.5f);

    // Center "0 dB" tick on the slot
    const float midY = slot.getCentreY();
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawLine (slot.getRight() + 3.0f, midY, slot.getRight() + 10.0f, midY, 1.0f);

    // Side scale ticks (+12 .. -12)
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    for (int i = 0; i <= 8; ++i)
    {
        const float ty = slot.getY() + slot.getHeight() * (float) i / 8.0f;
        const float tw = (i == 0 || i == 4 || i == 8) ? 7.0f : 4.0f;
        g.drawLine (slot.getX() - tw - 3.0f, ty, slot.getX() - 3.0f, ty, 1.0f);
    }

    // Cap - black plastic w/ white indicator stripe + red stripe
    const float capW = bounds.getWidth() - 4.0f;
    const float capH = 20.0f;
    auto cap = juce::Rectangle<float> (bounds.getCentreX() - capW * 0.5f,
                                       sliderPos - capH * 0.5f, capW, capH);
    juce::DropShadow sh (juce::Colours::black.withAlpha (0.6f), 4, { 0, 2 });
    juce::Path pth; pth.addRoundedRectangle (cap, 2.0f);
    sh.drawForPath (g, pth);

    juce::ColourGradient grad (juce::Colour (0xff2a2a2a), cap.getCentreX(), cap.getY(),
                               juce::Colour (0xff040404), cap.getCentreX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.0f);

    g.setColour (juce::Colours::white);
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));
    g.setColour (juce::Colour (0xffdc1414));
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 1.0f)
                   .withY (cap.getCentreY() + 2.5f));

    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 2.0f, 0.8f);
}

//------------------------------------------------------------------------------
// Small pushbutton used for pull switches, LEAD, CLASS A, EQ IN, POWER
//------------------------------------------------------------------------------
void Lead85MKIILNF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                  bool /*highlighted*/, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    // Chrome body
    juce::ColourGradient grad (juce::Colour (down ? 0xff8a8a8a : 0xffbfbfbf),
                               r.getCentreX(), r.getY(),
                               juce::Colour (0xff2a2a2a),
                               r.getCentreX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);

    // Amber or red lamp inset (upper)
    const float d  = juce::jmin (r.getWidth(), r.getHeight()) * 0.22f;
    const float cx = r.getCentreX();
    const float cy = r.getY() + r.getHeight() * 0.30f;
    auto lamp = juce::Rectangle<float> (cx - d * 0.5f, cy - d * 0.5f, d, d);
    if (on)
    {
        juce::ColourGradient lg (juce::Colour (0xffffd080), cx, cy - d * 0.3f,
                                 juce::Colour (0xffb04000), cx, cy + d * 0.4f, false);
        g.setGradientFill (lg);
        g.fillEllipse (lamp);
        g.setColour (juce::Colour (0xffffa040).withAlpha (0.35f));
        g.drawEllipse (lamp.expanded (2.0f), 3.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff3a2410));
        g.fillEllipse (lamp);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (lamp, 0.6f);

    g.setColour (juce::Colour (0xff0a0a0a));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);

    g.setColour (juce::Colour (0xff0a0a0a));
    g.setFont (juce::Font ("Helvetica", juce::jmin (10.5f, r.getHeight() * 0.26f), juce::Font::bold));
    g.drawText (b.getButtonText(),
                r.toNearestInt().removeFromBottom ((int) (r.getHeight() * 0.55f)),
                juce::Justification::centred);
}

//==============================================================================
ChickenKnob::ChickenKnob (juce::AudioProcessorValueTreeState& s,
                         const juce::String& paramId,
                         const juce::String& caption,
                         const juce::String& pullParamId,
                         const juce::String& pullCaption)
{
    addAndMakeVisible (knob);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, knob);

    cap.setText (caption, juce::dontSendNotification);
    cap.setFont (juce::Font ("Helvetica", 11.0f, juce::Font::bold));
    cap.setJustificationType (juce::Justification::centred);
    cap.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (cap);

    if (pullParamId.isNotEmpty())
    {
        hasPull = true;
        pullBtn.setClickingTogglesState (true);
        addAndMakeVisible (pullBtn);
        pullAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pullParamId, pullBtn);

        pullCap.setText (pullCaption, juce::dontSendNotification);
        pullCap.setFont (juce::Font ("Helvetica", 9.0f, juce::Font::bold));
        pullCap.setJustificationType (juce::Justification::centred);
        pullCap.setColour (juce::Label::textColourId, juce::Colour (0xffffae40));
        addAndMakeVisible (pullCap);
    }
}

void ChickenKnob::resized()
{
    auto r = getLocalBounds();
    cap.setBounds (r.removeFromTop (16));
    if (hasPull)
    {
        auto bot = r.removeFromBottom (26);
        pullCap.setBounds (bot.removeFromBottom (12));
        pullBtn.setBounds (bot.withSizeKeepingCentre (40, 14));
    }
    knob.setBounds (r.reduced (4));
}

//==============================================================================
// DriveLed
//==============================================================================
void Lead85MKIIEditor::DriveLed::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto lamp = juce::Rectangle<float> (r.getCentreX() - d * 0.5f,
                                        r.getCentreY() - d * 0.5f, d, d);

    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (lamp);
    const float br = juce::jlimit (0.0f, 1.0f, b);
    if (br > 0.001f)
    {
        juce::ColourGradient grad (juce::Colour (0xffffb040).withAlpha (br),
                                   lamp.getCentreX(), lamp.getCentreY(),
                                   juce::Colour (0xffa02000).withAlpha (br * 0.5f),
                                   lamp.getRight(), lamp.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (lamp.reduced (d * 0.1f));
        g.setColour (juce::Colour (0xffff8020).withAlpha (br * 0.35f));
        g.drawEllipse (lamp.expanded (3.0f), 4.0f);
    }
    g.setColour (juce::Colour (0xff8a8a8a));
    g.drawEllipse (lamp, 1.0f);
}

//==============================================================================
// Lead85MKIIEditor
//==============================================================================
Lead85MKIIEditor::Lead85MKIIEditor (Lead85MKIIProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    kVol1       = std::make_unique<ChickenKnob> (proc.apvts, "volume1",    "VOLUME 1",
                                                 "pullBright1",  "PULL BRIGHT");
    kTreble     = std::make_unique<ChickenKnob> (proc.apvts, "treble",     "TREBLE",
                                                 "pullTreble",   "PULL SHIFT");
    kMiddle     = std::make_unique<ChickenKnob> (proc.apvts, "middle",     "MIDDLE");
    kBass       = std::make_unique<ChickenKnob> (proc.apvts, "bass",       "BASS",
                                                 "pullShift",    "PULL SHIFT");
    kMaster1    = std::make_unique<ChickenKnob> (proc.apvts, "master1",    "MASTER 1",
                                                 "pullGain",     "PULL GAIN");
    kLeadDrive  = std::make_unique<ChickenKnob> (proc.apvts, "leadDrive",  "LEAD DRIVE",
                                                 "pullLeadBright","PULL BRIGHT");
    kLeadMaster = std::make_unique<ChickenKnob> (proc.apvts, "leadMaster", "LEAD MASTER");
    kPresence   = std::make_unique<ChickenKnob> (proc.apvts, "presence",   "PRESENCE");
    kReverb     = std::make_unique<ChickenKnob> (proc.apvts, "reverb",     "REVERB");

    for (auto* k : { kVol1.get(), kTreble.get(), kMiddle.get(), kBass.get(),
                     kMaster1.get(), kLeadDrive.get(), kLeadMaster.get(),
                     kPresence.get(), kReverb.get() })
        addAndMakeVisible (k);

    // Input / Output trim
    addAndMakeVisible (inputKnob);
    addAndMakeVisible (outputKnob);
    inAtt  = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "input",  inputKnob);
    outAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "output", outputKnob);

    auto initLbl = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font ("Helvetica", 11.0f, juce::Font::bold));
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (l);
    };
    initLbl (inputLbl,  "INPUT");
    initLbl (outputLbl, "OUTPUT");
    initLbl (leadLbl,   "CHANNEL");
    initLbl (classLbl,  "POWER MODE");
    initLbl (reverbOnLbl, "REVERB");
    initLbl (powerLbl,  "MAIN");
    initLbl (geqSectLbl,"5-BAND GRAPHIC EQUALISER        +/-12 dB");

    // Mode switches
    for (auto* btn : { &leadBtn, &classBtn, &reverbOnBtn, &powerBtn, &geqOnBtn })
    {
        btn->setClickingTogglesState (true);
        addAndMakeVisible (*btn);
    }
    leadAtt     = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "leadMode",  leadBtn);
    classAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "classA",    classBtn);
    reverbOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "reverbOn",  reverbOnBtn);
    powerAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "powerOn",   powerBtn);
    geqOnAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "geqOn",     geqOnBtn);

    // Graphic EQ sliders - +/-12 dB range
    const char* ids[5] = { "geq0", "geq1", "geq2", "geq3", "geq4" };
    const char* captions[5] = { "80", "240", "750", "2.2k", "6.6k" };
    for (int i = 0; i < 5; ++i)
    {
        geqBands[i].setSliderStyle (juce::Slider::LinearVertical);
        geqBands[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        geqBands[i].setRange (-12.0, 12.0, 0.1);
        geqBands[i].setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (geqBands[i]);
        geqAtts[i] = std::make_unique<APVTS::SliderAttachment> (proc.apvts, ids[i], geqBands[i]);

        geqLbls[i].setText (captions[i], juce::dontSendNotification);
        geqLbls[i].setFont (juce::Font ("Helvetica", 10.5f, juce::Font::bold));
        geqLbls[i].setJustificationType (juce::Justification::centred);
        geqLbls[i].setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (geqLbls[i]);
    }

    driveLed = std::make_unique<DriveLed> (proc);
    addAndMakeVisible (*driveLed);

    setResizable (false, false);
    setSize (1100, 520);
    buildFaceplate();
}

//==============================================================================
void Lead85MKIIEditor::buildFaceplate()
{
    faceplate = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (faceplate);

    // ---- Deep black tolex with coarse grain ----
    g.fillAll (juce::Colour (0xff080808));
    juce::Random rnd (0xB00B1E5);
    // Dense grain layer 1
    for (int i = 0; i < getWidth() * getHeight() / 70; ++i)
    {
        const int x = rnd.nextInt (getWidth());
        const int y = rnd.nextInt (getHeight());
        const juce::uint8 a = (juce::uint8)(14 + rnd.nextInt (50));
        g.setColour (juce::Colour (255, 255, 255).withAlpha ((float) a / 255.0f * 0.09f));
        g.fillRect (x, y, 2, 2);
    }
    // Darker speckle layer 2
    for (int i = 0; i < getWidth() * getHeight() / 180; ++i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (rnd.nextInt (getWidth()), rnd.nextInt (getHeight()), 3, 3);
    }

    // ---- Edge vignette (deeper corners for depth) ----
    juce::ColourGradient vig (juce::Colours::transparentBlack,
                              (float) getWidth() * 0.5f, (float) getHeight() * 0.5f,
                              juce::Colours::black.withAlpha (0.45f),
                              0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillAll();

    // ---- Chrome top plate (brushed) with multi-stop gradient ----
    auto chrome = juce::Rectangle<int> (22, 18, getWidth() - 44, 56);
    {
        juce::ColourGradient cg (juce::Colour (0xfff0f0f0),
                                 (float) chrome.getX(), (float) chrome.getY(),
                                 juce::Colour (0xff5a5a5a),
                                 (float) chrome.getX(), (float) chrome.getBottom(), false);
        cg.addColour (0.35, juce::Colour (0xffbcbcbc));
        cg.addColour (0.55, juce::Colour (0xff888888));
        cg.addColour (0.85, juce::Colour (0xff444444));
        g.setGradientFill (cg);
        g.fillRoundedRectangle (chrome.toFloat(), 3.0f);
        // Brushed horizontal hairlines
        for (int y = chrome.getY() + 2; y < chrome.getBottom() - 2; y += 1)
        {
            const int grain = rnd.nextInt (3) - 1;
            g.setColour (juce::Colours::black.withAlpha (0.025f + grain * 0.01f));
            g.drawLine ((float) chrome.getX() + 3, (float) y,
                        (float) chrome.getRight() - 3, (float) y, 0.6f);
        }
        // Top & bottom bevel
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawLine ((float) chrome.getX() + 2, (float) chrome.getY() + 1,
                    (float) chrome.getRight() - 2, (float) chrome.getY() + 1, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine ((float) chrome.getX() + 2, (float) chrome.getBottom() - 1,
                    (float) chrome.getRight() - 2, (float) chrome.getBottom() - 1, 1.0f);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawRoundedRectangle (chrome.toFloat(), 3.0f, 1.4f);
    }

    // ---- Embossed brand text (dark shadow + bright highlight for depth) ----
    auto drawEmbossed = [&] (const juce::String& txt, juce::Rectangle<int> area,
                             juce::Justification just, float sz, juce::Font::FontStyleFlags style)
    {
        g.setFont (juce::Font ("Helvetica", sz, style));
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (txt, area.translated (0, 1), just);
        g.setColour (juce::Colour (0xff0a0a0a));
        g.drawText (txt, area, just);
    };
    drawEmbossed ("LEAD-85  MKII", chrome.reduced (14, 4),
                  juce::Justification::centredLeft, 24.0f, juce::Font::bold);
    drawEmbossed ("DEMPWOLF TUBE MODEL   /   MODEL 85/II DX",
                  chrome.reduced (190, 4),
                  juce::Justification::centredLeft, 10.0f, juce::Font::bold);
    drawEmbossed ("EMERGENT AUDIO LABS",
                  chrome.reduced (14, 4),
                  juce::Justification::centredRight, 10.0f, juce::Font::bold);

    // ---- Red hairline running under the chrome strip ----
    g.setColour (juce::Colour (0xffdc1414));
    g.fillRect (chrome.getX(), chrome.getBottom() + 2, chrome.getWidth(), 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (chrome.getX(), chrome.getBottom() + 4, chrome.getWidth(), 1);

    // ---- Brass divider between knob row and GEQ row ----
    g.setColour (juce::Colour (0xffd4a44a));
    g.fillRect (28, 282, getWidth() - 56, 1);
    g.setColour (juce::Colour (0xff553a1a));
    g.fillRect (28, 283, getWidth() - 56, 1);

    // ---- Outer chrome edge frame ----
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawRect (6, 6, getWidth() - 12, getHeight() - 12, 2);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRect (8, 8, getWidth() - 16, getHeight() - 16, 1);

    // ---- Four cross-head corner mounting screws (slightly bigger) ----
    auto drawScrew = [&] (int x, int y)
    {
        // Dark shadow
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse ((float) x - 9, (float) y - 7, 18, 18);
        // Brushed steel head
        juce::ColourGradient sg (juce::Colour (0xffbbbbbb), (float) x - 4, (float) y - 4,
                                 juce::Colour (0xff3a3a3a), (float) x + 4, (float) y + 4, false);
        g.setGradientFill (sg);
        g.fillEllipse ((float) x - 8, (float) y - 8, 16, 16);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawEllipse ((float) x - 8, (float) y - 8, 16, 16, 1.0f);
        // Cross-slot
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawLine ((float) x - 5, (float) y, (float) x + 5, (float) y, 1.5f);
        g.drawLine ((float) x, (float) y - 5, (float) x, (float) y + 5, 1.5f);
        // Highlight
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.fillEllipse ((float) x - 3, (float) y - 3, 3, 2);
    };
    drawScrew (22, 22); drawScrew (getWidth() - 22, 22);
    drawScrew (22, getHeight() - 22); drawScrew (getWidth() - 22, getHeight() - 22);
}

void Lead85MKIIEditor::paint (juce::Graphics& g) { g.drawImageAt (faceplate, 0, 0); }

void Lead85MKIIEditor::resized()
{
    if (faceplate.getWidth() != getWidth() || faceplate.getHeight() != getHeight())
        buildFaceplate();

    auto r = getLocalBounds().reduced (28, 20);
    r.removeFromTop (54 + 12);   // brand strip + spacer

    // ---- Row 1: INPUT + 9 chicken-head knobs + OUTPUT ----
    auto row1 = r.removeFromTop (180);
    const int totalKnobs = 11;    // input + 9 ChickenKnobs + output
    const int kw = row1.getWidth() / totalKnobs;

    auto layoutSimple = [&] (juce::Rectangle<int> cell, juce::Slider& k, juce::Label& l)
    {
        l.setBounds (cell.removeFromTop (16));
        cell.removeFromBottom (26);
        k.setBounds (cell.reduced (6));
    };

    layoutSimple (row1.removeFromLeft (kw), inputKnob, inputLbl);
    kVol1      ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kTreble    ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kMiddle    ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kBass      ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kMaster1   ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kLeadDrive ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kLeadMaster->setBounds (row1.removeFromLeft (kw).reduced (3));
    kPresence  ->setBounds (row1.removeFromLeft (kw).reduced (3));
    kReverb    ->setBounds (row1.removeFromLeft (kw).reduced (3));
    layoutSimple (row1.removeFromLeft (kw), outputKnob, outputLbl);

    r.removeFromTop (10);   // divider spacer

    // ---- Row 2: 5-band Graphic EQ + mode switches ----
    auto row2 = r.removeFromTop (190);

    // Section label
    geqSectLbl.setBounds (row2.getX(), row2.getY(), row2.getWidth() * 2/3, 16);
    row2.removeFromTop (18);

    // Mode switch bay on the right (CHANNEL, POWER MODE, REVERB, EQ IN, MAIN POWER)
    auto modes = row2.removeFromRight (row2.getWidth() * 2 / 7);
    const int mh = modes.getHeight() / 5;
    auto placeSwitch = [&] (juce::Rectangle<int> cell, juce::Label& l, juce::ToggleButton& b)
    {
        l.setBounds (cell.removeFromLeft (90));
        b.setBounds (cell.removeFromLeft (90).withSizeKeepingCentre (80, mh - 8));
    };
    placeSwitch (modes.removeFromTop (mh), leadLbl,     leadBtn);
    placeSwitch (modes.removeFromTop (mh), classLbl,    classBtn);
    placeSwitch (modes.removeFromTop (mh), reverbOnLbl, reverbOnBtn);
    // EQ IN + LED
    {
        auto cell = modes.removeFromTop (mh);
        geqOnBtn.setBounds (cell.removeFromLeft (90).withSizeKeepingCentre (80, mh - 8));
        driveLed->setBounds (cell.removeFromLeft (90).withSizeKeepingCentre (24, 24));
    }
    placeSwitch (modes.removeFromTop (mh), powerLbl, powerBtn);

    // GEQ sliders on the left two-thirds
    const int gw = row2.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto cell = row2.removeFromLeft (gw);
        geqLbls[i].setBounds (cell.removeFromBottom (16));
        geqBands[i].setBounds (cell.reduced (10, 4));
    }
}
