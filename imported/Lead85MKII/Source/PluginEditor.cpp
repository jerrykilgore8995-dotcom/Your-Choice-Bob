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
// Numbered black hardware knob with a knurled skirt and white position line.
void Lead85MKIILNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float start, float end, juce::Slider&)
{
    const juce::Rectangle<float> bounds (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (w), static_cast<float> (h));
    const auto c = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.39f;
    auto disc = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black);
    g.fillEllipse (disc.translated (0.0f, 2.0f));
    g.setGradientFill ({ juce::Colour (0xff434344), c.x, disc.getY(),
                         juce::Colour (0xff080808), c.x, disc.getBottom(), false });
    g.fillEllipse (disc);
    for (int i = 0; i < 44; ++i)
    {
        const auto a = i * juce::MathConstants<float>::twoPi / 44.0f;
        const juce::Point<float> d (std::sin (a), std::cos (a));
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawLine ({ c + d * radius * 0.88f, c + d * radius }, 0.65f);
    }
    g.setColour (juce::Colour (0xff151515));
    g.fillEllipse (disc.reduced (radius * 0.26f));
    g.setFont (juce::Font (juce::FontOptions (7.5f, juce::Font::bold)));
    g.setColour (juce::Colour (0xffeeeae3));
    for (int i = 0; i <= 10; i += 2)
    {
        const auto a = start + (end - start) * (static_cast<float> (i) / 10.0f);
        const auto pt = c + juce::Point<float> (std::sin (a), -std::cos (a)) * radius * 0.72f;
        g.drawText (juce::String (i), juce::Rectangle<float> (13.0f, 9.0f).withCentre (pt),
                    juce::Justification::centred);
    }
    const auto angle = start + pos * (end - start);
    const juce::Point<float> direction (std::sin (angle), -std::cos (angle));
    g.drawLine ({ c + direction * radius * 0.16f, c + direction * radius * 0.53f }, 1.7f);
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
void Lead85MKIILNF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                      bool highlighted, bool)
{
    const auto r = button.getLocalBounds().toFloat();
    const auto on = button.getToggleState();
    const auto hardware = button.getComponentID() == "amp-switch";
    const auto foot = button.getComponentID() == "foot-jack";
    if (hardware || foot)
    {
        const auto c = r.getCentre();
        const auto metal = juce::Rectangle<float> (17.0f, 17.0f).withCentre (c);
        g.setColour (juce::Colours::black);
        g.fillEllipse (metal.expanded (2.0f).translated (0.0f, 1.0f));
        g.setGradientFill ({ juce::Colour (0xffdedad0), c.x, metal.getY(),
                             juce::Colour (0xff474742), c.x, metal.getBottom(), false });
        g.fillEllipse (metal);
        g.setColour (juce::Colour (0xff0c0c0c));
        g.fillEllipse (metal.reduced (4.0f));
        if (hardware)
        {
            const auto tip = c.translated (0.0f, on ? -9.0f : 9.0f);
            g.setColour (juce::Colour (0xff686965));
            g.drawLine ({ c.translated (1.0f, 1.0f), tip.translated (1.0f, 1.0f) }, 6.0f);
            g.setColour (juce::Colour (0xffe6e3d9));
            g.drawLine ({ c, tip }, 3.2f);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 4.0f).withCentre (tip));
        }
        else if (on)
        {
            g.setColour (juce::Colour (0xffe4a650));
            g.fillEllipse (metal.reduced (6.5f));
        }
        return;
    }

    auto plate = r.reduced (1.0f);
    g.setColour (on ? juce::Colour (0xffa96f32) : juce::Colour (highlighted ? 0xff42403b : 0xff222221));
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (on ? juce::Colours::black : juce::Colour (0xffe8e5dc));
    g.setFont (juce::Font (juce::FontOptions (9.7f, juce::Font::bold)));
    g.drawFittedText (button.getButtonText(), plate.toNearestInt().reduced (2, 0),
                      juce::Justification::centred, 1);
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
        pullBtn.setButtonText (pullCaption);
        addAndMakeVisible (pullBtn);
        pullAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pullParamId, pullBtn);

    }
}

void ChickenKnob::resized()
{
    auto r = getLocalBounds();
    cap.setBounds (r.removeFromTop (16));
    if (hasPull)
    {
        pullBtn.setBounds (r.removeFromBottom (17).reduced (3, 1));
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
    const auto on = proc.apvts.getRawParameterValue ("powerOn")->load() > 0.5f;
    const float br = on ? juce::jlimit (0.45f, 1.0f, 0.50f + b) : 0.0f;
    if (br > 0.001f)
    {
        juce::ColourGradient grad (juce::Colour (0xffffb040).withAlpha (br),
                                   lamp.getCentreX(), lamp.getCentreY(),
                                   juce::Colour (0xffa02000).withAlpha (br * 0.5f),
                                   lamp.getRight(), lamp.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (lamp.reduced (d * 0.1f));
        g.setColour (juce::Colour (0xff338aff).withAlpha (br * 0.35f));
        g.drawEllipse (lamp.expanded (3.0f), 4.0f);
    }
    g.setColour (juce::Colour (0xffafc2df));
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
                                                 "pullGain",     "PULL DEEP");
    kLeadDrive  = std::make_unique<ChickenKnob> (proc.apvts, "leadDrive",  "LEAD DRIVE",
                                                 "leadMode", "PULL LEAD");
    kLeadMaster = std::make_unique<ChickenKnob> (proc.apvts, "leadMaster", "LEAD MASTER",
                                                 "pullLeadBright", "PULL BRIGHT");
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

    // Mode switches
    for (auto* btn : { &leadBtn, &classBtn, &reverbOnBtn, &powerBtn, &geqOnBtn, &standbyBtn, &cabinetBtn })
    {
        btn->setClickingTogglesState (true);
        addAndMakeVisible (*btn);
    }
    for (auto* button : { &powerBtn, &standbyBtn, &geqOnBtn })
        button->setComponentID ("amp-switch");
    leadBtn.setComponentID ("foot-jack");
    leadBtn.setTooltip ("Rhythm / Lead footswitch");
    geqOnBtn.setTooltip ("Five-band graphic EQ");
    standbyBtn.setTooltip ("Standby: mute the amplifier");
    powerBtn.setTooltip ("Amplifier power");
    standbyAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "standby", standbyBtn);
    cabinetAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "cabEnabled", cabinetBtn);

    addAndMakeVisible (loadIR);
    addAndMakeVisible (factoryIR);
    addAndMakeVisible (irName);
    irName.setColour (juce::Label::textColourId, juce::Colour (0xffe0ded5));
    irName.setMinimumHorizontalScale (0.85f);
    loadIR.onClick = [this] { chooseIR(); };
    int factoryId = 1;
    for (auto* name : Lead85Cabinet::factoryNames())
        factoryIR.addItem (name, factoryId++);
    factoryIR.setTextWhenNothingSelected ("Factory cabs");
    factoryIR.onChange = [this]
    {
        if (factoryIR.getSelectedId() == 0)
            return;
        proc.useFactoryCabinet (factoryIR.getSelectedId() - 1);
        proc.apvts.getParameter ("cabEnabled")->setValueNotifyingHost (1.0f);
        displayedIR.clear();
        timerCallback();
    };

    leadAtt     = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "leadMode",  leadBtn);
    classAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "classA",    classBtn);
    reverbOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "reverbOn",  reverbOnBtn);
    powerAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "powerOn",   powerBtn);
    geqOnAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "geqOn",     geqOnBtn);

    // Graphic EQ sliders - +/-12 dB range
    const char* ids[5] = { "geq0", "geq1", "geq2", "geq3", "geq4" };
    const char* captions[5] = { "80", "240", "750", "2200", "6600" };
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
    setSize (1160, 360);
    timerCallback();
    startTimerHz (6);
}

//==============================================================================
void Lead85MKIIEditor::buildFaceplate()
{
    faceplate = juce::Image (juce::Image::RGB, getWidth(), getHeight(), false);
    juce::Graphics g (faceplate);
    g.fillAll (juce::Colour (0xff191a1a));

    const juce::Rectangle<float> cabinet (9.0f, 8.0f, 1142.0f, 256.0f);
    g.setGradientFill ({ juce::Colour (0xff20201f), 0.0f, 8.0f,
                         juce::Colour (0xff080808), 0.0f, 260.0f, false });
    g.fillRoundedRectangle (cabinet, 13.0f);
    juce::Random texture (1895);
    for (int i = 0; i < 6900; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (texture.nextFloat() * 0.10f));
        g.fillRect (14 + texture.nextInt (1132), 13 + texture.nextInt (243), 2, 1);
    }

    const juce::Rectangle<float> grille (31.0f, 27.0f, 1098.0f, 104.0f);
    g.setColour (juce::Colour (0xffe2ddd1));
    g.drawRoundedRectangle (grille.expanded (2.0f), 4.0f, 1.3f);
    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRect (grille);
    for (int y = 28; y < 130; y += 3)
    {
        g.setColour (juce::Colour (0xff323232));
        g.drawHorizontalLine (y, 32.0f, 1128.0f);
    }
    for (int x = 32; x < 1128; x += 4)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawVerticalLine (x, 28.0f, 130.0f);
    }
    g.setGradientFill ({ juce::Colours::black.withAlpha (0.5f), 0.0f, 28.0f,
                         juce::Colours::transparentBlack, 0.0f, 78.0f, false });
    g.fillRect (grille);

    const juce::Rectangle<float> panel (30.0f, 139.0f, 1100.0f, 105.0f);
    g.setGradientFill ({ juce::Colour (0xff282829), 0.0f, 139.0f,
                         juce::Colour (0xff111112), 0.0f, 244.0f, false });
    g.fillRect (panel);
    g.setColour (juce::Colour (0xff777570));
    g.drawRect (panel, 1.0f);

    auto mark = [&] (const juce::String& caption, juce::Rectangle<int> r)
    {
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.setColour (juce::Colour (0xfff0ede6));
        g.drawText (caption, r, juce::Justification::centred);
    };
    mark ("INPUT", { 37, 154, 48, 15 });
    mark ("FOOT", { 37, 199, 48, 15 });
    mark ("EQ IN", { 934, 154, 44, 15 });
    mark ("STANDBY", { 976, 154, 60, 15 });
    mark ("POWER", { 1034, 154, 44, 15 });

    const auto jack = juce::Rectangle<float> (17.0f, 17.0f).withCentre ({ 61.0f, 182.0f });
    g.setColour (juce::Colour (0xffb4b2aa));
    g.fillEllipse (jack);
    g.setColour (juce::Colours::black);
    g.fillEllipse (jack.reduced (4.0f));
    const auto pilot = juce::Rectangle<float> (17.0f, 17.0f).withCentre ({ 1103.0f, 185.0f });
    g.setColour (juce::Colour (0xff9e9d97));
    g.drawEllipse (pilot.expanded (3.0f), 2.0f);

    g.setColour (juce::Colour (0xff111212));
    g.fillRoundedRectangle (juce::Rectangle<float> (9.0f, 273.0f, 1142.0f, 78.0f), 6.0f);
}

void Lead85MKIIEditor::paint (juce::Graphics& g) { g.drawImageAt (faceplate, 0, 0); }

void Lead85MKIIEditor::resized()
{
    if (faceplate.getWidth() != getWidth() || faceplate.getHeight() != getHeight())
        buildFaceplate();

    int x = 89;
    for (auto* knob : { kVol1.get(), kTreble.get(), kBass.get(), kMiddle.get(),
                         kMaster1.get(), kLeadDrive.get(), kLeadMaster.get() })
    {
        knob->setBounds (x, 147, 87, 89);
        x += 87;
    }

    for (int i = 0; i < 5; ++i)
    {
        const auto left = 707 + i * 43;
        geqLbls[static_cast<size_t> (i)].setBounds (left, 153, 42, 14);
        geqBands[static_cast<size_t> (i)].setBounds (left, 166, 42, 65);
    }

    leadBtn.setBounds (48, 211, 26, 24);
    geqOnBtn.setBounds (939, 169, 34, 32);
    standbyBtn.setBounds (989, 169, 34, 32);
    powerBtn.setBounds (1039, 169, 34, 32);
    driveLed->setBounds (1092, 174, 22, 22);

    inputLbl.setBounds (20, 278, 58, 13);
    inputKnob.setBounds (25, 293, 48, 49);
    outputLbl.setBounds (82, 278, 58, 13);
    outputKnob.setBounds (87, 293, 48, 49);
    kPresence->setBounds (152, 278, 76, 66);
    kReverb->setBounds (233, 278, 76, 66);
    reverbOnBtn.setBounds (315, 298, 65, 26);
    classBtn.setBounds (387, 298, 74, 26);
    cabinetBtn.setBounds (493, 298, 52, 26);
    loadIR.setBounds (552, 298, 100, 26);
    factoryIR.setBounds (659, 298, 183, 26);
    irName.setBounds (850, 298, 274, 26);
}

void Lead85MKIIEditor::timerCallback()
{
    const auto name = proc.getCabinetName();
    if (name != displayedIR)
    {
        displayedIR = name;
        factoryIR.setSelectedId (proc.getFactoryCabinet() + 1, juce::dontSendNotification);
        irName.setText (name, juce::dontSendNotification);
        irName.setTooltip (name);
    }
}

void Lead85MKIIEditor::chooseIR()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load a cabinet impulse response", juce::File(), "*.wav");
    const juce::Component::SafePointer<Lead85MKIIEditor> parent (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [parent] (const juce::FileChooser& result)
        {
            if (parent == nullptr || result.getResult() == juce::File())
                return;

            auto loaded = parent->proc.loadCabinet (result.getResult());
            if (loaded.wasOk())
            {
                parent->proc.apvts.getParameter ("cabEnabled")->setValueNotifyingHost (1.0f);
                parent->displayedIR.clear();
                parent->timerCallback();
            }
            else
            {
                parent->irName.setText (loaded.getErrorMessage(), juce::dontSendNotification);
            }
        });
}
