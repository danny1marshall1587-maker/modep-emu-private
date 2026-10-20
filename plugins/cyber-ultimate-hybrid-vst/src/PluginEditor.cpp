#include "PluginEditor.h"
#include <cmath>

void VintageHardwareLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) {

    const float radius = (float)juce::jmin(width / 2, height / 2) - 8.0f;
    const float centreX = (float)x + (float)width * 0.5f;
    const float centreY = (float)y + (float)height * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const int themeId = slider.getProperties().getWithDefault("themeId", 0);

    // Outer scale tick marks (1..10)
    for (int t = 0; t <= 10; ++t) {
        const float ta = rotaryStartAngle + (t / 10.0f) * (rotaryEndAngle - rotaryStartAngle);
        const float r1 = radius + 2.5f;
        const float r2 = radius + 6.5f;
        g.setColour((themeId == 1) ? juce::Colour(0xff291502) : juce::Colour(0xffcbd5e1));
        g.drawLine(centreX + std::sin(ta) * r1, centreY - std::cos(ta) * r1,
                   centreX + std::sin(ta) * r2, centreY - std::cos(ta) * r2, 1.6f);
    }

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(rx + 2.0f, ry + 4.0f, rw, rw);

    if (themeId == 0) {
        // '65 Blackface Skirted Silver-Insert Knob
        juce::ColourGradient skirtGrad(juce::Colour(0xff334155), centreX - radius * 0.4f, centreY - radius * 0.4f,
                                       juce::Colour(0xff090d16), centreX + radius, centreY + radius, true);
        g.setGradientFill(skirtGrad);
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawEllipse(rx, ry, rw, rw, 1.5f);

        // Brushed Aluminum center cap
        const float capR = radius * 0.58f;
        juce::ColourGradient capGrad(juce::Colour(0xfff8fafc), centreX - capR * 0.5f, centreY - capR * 0.5f,
                                     juce::Colour(0xff94a3b8), centreX + capR, centreY + capR, true);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

        // White pointer line
        juce::Path p;
        p.addRoundedRectangle(-2.0f, -radius + 2.0f, 4.0f, radius * 0.52f, 1.5f);
        g.setColour(juce::Colours::white);
        g.fillPath(p, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    } else if (themeId == 1) {
        // '59 Bassman Oxblood Chicken-Head Knob
        const float baseR = radius * 0.72f;
        g.setColour(juce::Colour(0xff2a0a0e));
        g.fillEllipse(centreX - baseR, centreY - baseR, baseR * 2.0f, baseR * 2.0f);
        g.setColour(juce::Colour(0xfffbbf24));
        g.drawEllipse(centreX - baseR, centreY - baseR, baseR * 2.0f, baseR * 2.0f, 1.5f);

        juce::Path beak;
        beak.startNewSubPath(0.0f, -radius - 2.0f);
        beak.lineTo(8.0f, baseR * 0.55f);
        beak.lineTo(-8.0f, baseR * 0.55f);
        beak.closeSubPath();
        g.setColour(juce::Colour(0xff450a0a));
        g.fillPath(beak, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour(0xfffef3c7));
        g.drawLine(centreX, centreY,
                   centreX + std::sin(angle) * (radius - 2.0f),
                   centreY - std::cos(angle) * (radius - 2.0f), 2.5f);
    } else {
        // Studio Rack Bakelite + Chrome Pointer Knob (Reverb & Tape Delay pages)
        juce::Colour accent = (themeId == 2) ? juce::Colour(0xff34d399) : juce::Colour(0xffc084fc);
        juce::ColourGradient bodyGrad(juce::Colour(0xff334155), centreX - radius * 0.4f, centreY - radius * 0.4f,
                                      juce::Colour(0xff090d16), centreX + radius, centreY + radius, true);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(accent);
        g.drawEllipse(rx, ry, rw, rw, 2.0f);

        juce::Path p;
        p.addRoundedRectangle(-2.2f, -radius + 3.0f, 4.4f, radius * 0.62f, 1.8f);
        g.setColour(juce::Colours::white);
        g.fillPath(p, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    }
}

CyberUltimateHybridAudioProcessorEditor::CyberUltimateHybridAudioProcessorEditor(
    CyberUltimateHybridAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {

    setLookAndFeel(&hwLookAndFeel);

    auto setupTabBtn = [this](juce::TextButton& b, int pageIdx) {
        b.onClick = [this, pageIdx]() { setActivePage(pageIdx); };
        addAndMakeVisible(b);
    };
    setupTabBtn(tabBlackface, 0);
    setupTabBtn(tabBassman,   1);
    setupTabBtn(tabReverb,    2);
    setupTabBtn(tabDelay,     3);

    btnAudioSettings.setButtonText("AUDIO SETTINGS");
    btnAudioSettings.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0e7490));
    btnAudioSettings.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0fdfa));
    btnAudioSettings.onClick = [this] { openAudioSettings(); };
    addAndMakeVisible(btnAudioSettings);

    // Header Global ComboBoxes
    channelModeBox.addItemList({ "Ch 1: '65 Blackface Clean", "Ch 2: '59 Bassman / JMP", "Ch 3: Jumpered Parallel" }, 1);
    addAndMakeVisible(channelModeBox);
    channelModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "channel_mode", channelModeBox);

    qualityModeBox.addItemList({ "Eco Live (1x)", "Stage (2x)", "Studio (4x)", "Ultra Bounce (8x)" }, 1);
    addAndMakeVisible(qualityModeBox);
    qualityModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "quality_mode", qualityModeBox);

    cabSimBox.addItemList({ "Cab: Bypass", "Cab: '65 Twin 2x12 C12N", "Cab: '59 Bassman 4x10 P10R", "Cab: '68 Marshall 4x12 G12M" }, 1);
    addAndMakeVisible(cabSimBox);
    cabSimAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "speaker_cab", cabSimBox);

    // Page-Specific Switches
    addAndMakeVisible(brightToggle);
    brightAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bright_sw", brightToggle);

    slopeModeBox.addItemList({ "56k '59 Tweed Bassman", "33k '68 Marshall JMP" }, 1);
    addAndMakeVisible(slopeModeBox);
    slopeModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "slope_mode", slopeModeBox);

    powerTubesBox.addItemList({ "Tubes: 2x 6L6GC American", "Tubes: 2x EL34 British" }, 1);
    addAndMakeVisible(powerTubesBox);
    powerTubesAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "power_tubes", powerTubesBox);

    tremModeBox.addItemList({ "Brownface / Greg Harmonic Tube", "Blackface Opto Photocells" }, 1);
    addAndMakeVisible(tremModeBox);
    tremModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "trem_mode", tremModeBox);

    addAndMakeVisible(pingPongToggle);
    pingPongAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "dly_ping_pong", pingPongToggle);

    // PAGE 0: '65 Blackface AB763 Knobs
    addKnob("clean_vol",     "VOLUME",       0, 0);
    addKnob("treble",        "TREBLE",       0, 0);
    addKnob("mid",           "MIDDLE",       0, 0);
    addKnob("bass",          "BASS",         0, 0);
    addKnob("master",        "MASTER",       0, 0);
    addKnob("output_level",  "OUTPUT TRIM",  0, 0);

    // PAGE 1: '59 Bassman / '68 JMP Knobs
    addKnob("drive_gain",    "PREAMP DRIVE", 1, 1);
    addKnob("drive_vol",     "CHANNEL VOL",  1, 1);
    addKnob("treble",        "TREBLE",       1, 1);
    addKnob("mid",           "MIDDLE",       1, 1);
    addKnob("bass",          "BASS",         1, 1);
    addKnob("presence",      "PRESENCE",     1, 1);
    addKnob("sag_amount",    "TUBE SAG",     1, 1);
    addKnob("speaker_drive", "CONE DRIVE",   1, 1);

    // PAGE 2: 3-Spring Reverb & Harmonic Tube Vibrato Knobs
    addKnob("rev_dwell",     "TUBE DWELL",   2, 2);
    addKnob("rev_mix",       "REVERB MIX",   2, 2);
    addKnob("rev_tone",      "TANK TONE",    2, 2);
    addKnob("rev_drip",      "SPRING DRIP",  2, 2);
    addKnob("trem_speed",    "VIB SPEED Hz", 2, 2);
    addKnob("trem_depth",    "INTENSITY",    2, 2);

    // PAGE 3: Studio Tape / Ping-Pong Delay Rack Knobs
    addKnob("dly_time_ms",    "DELAY TIME ms", 3, 3);
    addKnob("dly_feedback",   "FEEDBACK %",    3, 3);
    addKnob("dly_mix",        "WET MIX %",     3, 3);
    addKnob("dly_tape_drive", "OXIDE DRIVE %", 3, 3);
    addKnob("dly_flutter",    "WOW & FLUTTER", 3, 3);
    addKnob("dly_ducking",    "AUTO DUCKING",  3, 3);

    setSize(1020, 560);
    setActivePage(0);
    startTimerHz(30);
}

CyberUltimateHybridAudioProcessorEditor::~CyberUltimateHybridAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void CyberUltimateHybridAudioProcessorEditor::addKnob(
    const juce::String& paramId, const juce::String& title, int pageIdx, int themeId) {

    BoundRotary br;
    br.paramId = paramId;
    br.title = title;
    br.pageIndex = pageIdx;
    br.themeId = themeId;

    br.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    br.slider->getProperties().set("themeId", themeId);
    br.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 20);
    if (themeId == 1) {
        br.slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff291502));
        br.slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x44fef3c7));
        br.slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    } else {
        br.slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff8fafc));
        br.slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f172a));
        br.slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
    addAndMakeVisible(*br.slider);

    br.label = std::make_unique<juce::Label>("", title);
    br.label->setJustificationType(juce::Justification::centred);
    br.label->setFont(juce::Font(13.0f, juce::Font::bold));
    br.label->setColour(juce::Label::textColourId,
                        (themeId == 1) ? juce::Colour(0xff1c0a00) : juce::Colour(0xffe2e8f0));
    addAndMakeVisible(*br.label);

    br.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, *br.slider);

    knobs.push_back(std::move(br));
}

void CyberUltimateHybridAudioProcessorEditor::setActivePage(int pageIdx) {
    currentPage = pageIdx;

    auto styleTab = [](juce::TextButton& b, bool active, juce::Colour activeCol) {
        b.setColour(juce::TextButton::buttonColourId, active ? activeCol : juce::Colour(0xff1e293b));
        b.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : juce::Colour(0xff94a3b8));
    };
    styleTab(tabBlackface, currentPage == 0, juce::Colour(0xff0284c7));
    styleTab(tabBassman,   currentPage == 1, juce::Colour(0xffd97706));
    styleTab(tabReverb,    currentPage == 2, juce::Colour(0xff059669));
    styleTab(tabDelay,     currentPage == 3, juce::Colour(0xff7c3aed));

    for (auto& k : knobs) {
        const bool vis = (k.pageIndex == currentPage);
        k.slider->setVisible(vis);
        k.label->setVisible(vis);
    }

    brightToggle.setVisible(currentPage == 0);
    slopeModeBox.setVisible(currentPage == 1);
    powerTubesBox.setVisible(currentPage == 1);
    tremModeBox.setVisible(currentPage == 2);
    pingPongToggle.setVisible(currentPage == 3);

    resized();
    repaint();
}

void CyberUltimateHybridAudioProcessorEditor::timerCallback() {
    if (currentPage == 3 || currentPage == 2) {
        reelAngle += 0.06f;
        if (reelAngle > 6.2831853f) reelAngle -= 6.2831853f;
        repaint();
    }
}

void CyberUltimateHybridAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff090d16));

    // Top Navigation Bar Background
    g.setColour(juce::Colour(0xff111827));
    g.fillRect(0, 0, getWidth(), 54);
    g.setColour(juce::Colour(0xff334155));
    g.drawLine(0.0f, 54.0f, (float)getWidth(), 54.0f, 1.5f);

    juce::Rectangle<float> bodyArea(18.0f, 68.0f, (float)getWidth() - 36.0f, (float)getHeight() - 86.0f);

    if (currentPage == 0) {
        // =====================================================================
        // PAGE 0: '65 FENDER BLACKFACE AB763 AMP HEAD
        // =====================================================================
        g.setColour(juce::Colour(0xff14171f));
        g.fillRoundedRectangle(bodyArea, 14.0f);
        g.setColour(juce::Colour(0xff475569));
        g.drawRoundedRectangle(bodyArea, 14.0f, 2.5f);

        // Chrome Trim + Black Anodized Faceplate
        juce::Rectangle<float> faceplate = bodyArea.reduced(16.0f).withHeight(250.0f);
        g.setColour(juce::Colour(0xff05070b));
        g.fillRoundedRectangle(faceplate, 8.0f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(faceplate, 8.0f, 2.0f);

        // Title & Blue Jewel Pilot Lamp
        g.setColour(juce::Colour(0xff38bdf8));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("CHANNEL 1  |  AB763 PLATE-DRIVEN TONE STACK CIRCUIT",
                   (int)faceplate.getX() + 18, (int)faceplate.getY() + 10, 460, 20, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(24.0f, juce::Font::bold | juce::Font::italic));
        g.drawText("Cyber '65 Twin / Super Blackface",
                   (int)faceplate.getX() + 18, (int)faceplate.getY() + 28, 460, 30, juce::Justification::centredLeft);

        // Cobalt Blue Jewel Lamp
        float lampX = faceplate.getRight() - 48.0f, lampY = faceplate.getY() + 24.0f;
        g.setColour(juce::Colour(0x553b82f6));
        g.fillEllipse(lampX - 6.0f, lampY - 6.0f, 32.0f, 32.0f);
        g.setColour(juce::Colour(0xff2563eb));
        g.fillEllipse(lampX, lampY, 20.0f, 20.0f);
        g.setColour(juce::Colour(0xff93c5fd));
        g.fillEllipse(lampX + 5.0f, lampY + 4.0f, 7.0f, 7.0f);

        // Silver Sparkle Grille Cloth Bottom Panel
        juce::Rectangle<float> grille = bodyArea.reduced(16.0f).withTop(faceplate.getBottom() + 14.0f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.fillRoundedRectangle(grille, 8.0f);
        g.setColour(juce::Colour(0xff334155));
        for (float y = grille.getY(); y < grille.getBottom(); y += 5.0f)
            g.drawHorizontalLine((int)y, grille.getX(), grille.getRight());
        for (float x = grille.getX(); x < grille.getRight(); x += 5.0f)
            g.drawVerticalLine((int)x, grille.getY(), grille.getBottom());

        // Emblem Badge on Grille
        juce::Rectangle<float> badge(grille.getX() + 24.0f, grille.getCentreY() - 26.0f, 440.0f, 52.0f);
        g.setColour(juce::Colour(0xe6090d16));
        g.fillRoundedRectangle(badge, 6.0f);
        g.setColour(juce::Colour(0xffe2e8f0));
        g.drawRoundedRectangle(badge, 6.0f, 1.5f);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText("FENDER AB763 GLASSY CLEAN VOICING", badge.toNearestInt(), juce::Justification::centred);
    }
    else if (currentPage == 1) {
        // =====================================================================
        // PAGE 1: '59 TWEED BASSMAN 5F6-A / '68 MARSHALL JMP PLEXI HEAD
        // =====================================================================
        g.setColour(juce::Colour(0xffb47b2a));
        g.fillRoundedRectangle(bodyArea, 14.0f);
        g.setColour(juce::Colour(0xff78350f));
        for (float d = -bodyArea.getHeight(); d < bodyArea.getWidth(); d += 7.0f) {
            g.drawLine(bodyArea.getX() + juce::jmax(0.0f, d),
                       bodyArea.getY() + juce::jmax(0.0f, -d),
                       bodyArea.getX() + juce::jmin(bodyArea.getWidth(), d + bodyArea.getHeight()),
                       bodyArea.getY() + juce::jmin(bodyArea.getHeight(), bodyArea.getHeight() - d), 1.0f);
        }
        g.drawRoundedRectangle(bodyArea, 14.0f, 3.0f);

        // Brushed Gold / Brass Control Chassis
        juce::Rectangle<float> chassis = bodyArea.reduced(16.0f).withHeight(255.0f);
        juce::ColourGradient brassGrad(juce::Colour(0xfffde68a), chassis.getX(), chassis.getY(),
                                       juce::Colour(0xffd97706), chassis.getX(), chassis.getBottom(), false);
        g.setGradientFill(brassGrad);
        g.fillRoundedRectangle(chassis, 8.0f);
        g.setColour(juce::Colour(0xff451a03));
        g.drawRoundedRectangle(chassis, 8.0f, 2.5f);

        g.setColour(juce::Colour(0xff451a03));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("CHANNEL 2  |  DC-COUPLED 12AX7 CATHODE FOLLOWER + PUSH-PULL POWER AMP",
                   (int)chassis.getX() + 18, (int)chassis.getY() + 10, 560, 20, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff1c0a00));
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText("'59 Tweed Bassman 5F6-A  /  '68 JMP Plexi",
                   (int)chassis.getX() + 18, (int)chassis.getY() + 28, 520, 30, juce::Justification::centredLeft);

        // Oxblood Grille Cloth
        juce::Rectangle<float> oxblood = bodyArea.reduced(16.0f).withTop(chassis.getBottom() + 14.0f);
        g.setColour(juce::Colour(0xff450a0a));
        g.fillRoundedRectangle(oxblood, 8.0f);
        g.setColour(juce::Colour(0x55f59e0b));
        for (float y = oxblood.getY(); y < oxblood.getBottom(); y += 6.0f)
            g.drawHorizontalLine((int)y, oxblood.getX(), oxblood.getRight());

        juce::Rectangle<float> badge(oxblood.getX() + 24.0f, oxblood.getCentreY() - 26.0f, 520.0f, 52.0f);
        g.setColour(juce::Colour(0xe61c0a00));
        g.fillRoundedRectangle(badge, 6.0f);
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawRoundedRectangle(badge, 6.0f, 1.5f);
        g.setFont(juce::Font(17.0f, juce::Font::bold));
        g.drawText("CATHODE-DRIVEN TONE STACK  |  56k TWEED / 33k PLEXI", badge.toNearestInt(), juce::Justification::centred);
    }
    else if (currentPage == 2) {
        // =====================================================================
        // PAGE 2: 3-SPRING ACCUTRONICS REVERB & HARMONIC TUBE VIBRATO RACK
        // =====================================================================
        juce::ColourGradient rackGrad(juce::Colour(0xff1e293b), bodyArea.getX(), bodyArea.getY(),
                                      juce::Colour(0xff090d16), bodyArea.getX(), bodyArea.getBottom(), false);
        g.setGradientFill(rackGrad);
        g.fillRoundedRectangle(bodyArea, 12.0f);
        g.setColour(juce::Colour(0xff10b981));
        g.drawRoundedRectangle(bodyArea, 12.0f, 2.0f);

        // Rack Ears & Mounting Bolts
        for (int ry : { (int)bodyArea.getY() + 28, (int)bodyArea.getBottom() - 36 }) {
            g.setColour(juce::Colour(0xff64748b));
            g.fillEllipse(bodyArea.getX() + 14.0f, (float)ry, 12.0f, 12.0f);
            g.fillEllipse(bodyArea.getRight() - 26.0f, (float)ry, 12.0f, 12.0f);
        }

        g.setColour(juce::Colour(0xff34d399));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("OUTBOARD TUBE TANK & CROSSOVER MODULATOR RACK",
                   (int)bodyArea.getX() + 44, (int)bodyArea.getY() + 16, 540, 20, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(23.0f, juce::Font::bold));
        g.drawText("Accutronics 3-Spring Reverb & Brownface / Greg Harmonic Vibrato",
                   (int)bodyArea.getX() + 44, (int)bodyArea.getY() + 36, 660, 28, juce::Justification::centredLeft);

        // 3-Spring Tank Window
        juce::Rectangle<float> tankWin(bodyArea.getX() + 44.0f, bodyArea.getBottom() - 150.0f, bodyArea.getWidth() - 88.0f, 115.0f);
        g.setColour(juce::Colour(0xff050811));
        g.fillRoundedRectangle(tankWin, 8.0f);
        g.setColour(juce::Colour(0xff059669));
        g.drawRoundedRectangle(tankWin, 8.0f, 1.5f);

        // Draw 3 animated physical springs
        for (int s = 0; s < 3; ++s) {
            float sy = tankWin.getY() + 28.0f + s * 30.0f;
            juce::Path sp;
            sp.startNewSubPath(tankWin.getX() + 24.0f, sy);
            for (float x = tankWin.getX() + 24.0f; x < tankWin.getRight() - 24.0f; x += 6.0f) {
                float coil = std::sin((x * 0.22f) + reelAngle * (s + 1)) * 7.5f;
                sp.lineTo(x, sy + coil);
            }
            g.setColour(s == 0 ? juce::Colour(0xff34d399) : (s == 1 ? juce::Colour(0xff10b981) : juce::Colour(0xff059669)));
            g.strokePath(sp, juce::PathStrokeType(2.0f));
        }
    }
    else {
        // =====================================================================
        // PAGE 3: STUDIO 15/30 IPS TAPE & PING-PONG DELAY RACK UNIT
        // =====================================================================
        juce::ColourGradient rackGrad(juce::Colour(0xff1e1b4b), bodyArea.getX(), bodyArea.getY(),
                                      juce::Colour(0xff090d16), bodyArea.getX(), bodyArea.getBottom(), false);
        g.setGradientFill(rackGrad);
        g.fillRoundedRectangle(bodyArea, 12.0f);
        g.setColour(juce::Colour(0xffa855f7));
        g.drawRoundedRectangle(bodyArea, 12.0f, 2.0f);

        g.setColour(juce::Colour(0xffc084fc));
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText("STUDIO 2\" MAGNETIC TAPE ECHO & PING-PONG DELAY RACK",
                   (int)bodyArea.getX() + 44, (int)bodyArea.getY() + 16, 540, 20, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(23.0f, juce::Font::bold));
        g.drawText("Live Mixer Aux Send 3-4 Tape Delay (15/30 IPS Oxide + Auto-Duck)",
                   (int)bodyArea.getX() + 44, (int)bodyArea.getY() + 36, 680, 28, juce::Justification::centredLeft);

        // Twin Spinning Magnetic Tape Reels Deck at Bottom
        juce::Rectangle<float> deckWin(bodyArea.getX() + 44.0f, bodyArea.getBottom() - 155.0f, bodyArea.getWidth() - 88.0f, 120.0f);
        g.setColour(juce::Colour(0xff070a12));
        g.fillRoundedRectangle(deckWin, 8.0f);
        g.setColour(juce::Colour(0xff7c3aed));
        g.drawRoundedRectangle(deckWin, 8.0f, 1.5f);

        for (float rxOff : { deckWin.getCentreX() - 140.0f, deckWin.getCentreX() + 140.0f }) {
            float ry = deckWin.getCentreY();
            float rr = 44.0f;
            g.setColour(juce::Colour(0xff1e293b));
            g.fillEllipse(rxOff - rr, ry - rr, rr * 2.0f, rr * 2.0f);
            g.setColour(juce::Colour(0xffc084fc));
            g.drawEllipse(rxOff - rr, ry - rr, rr * 2.0f, rr * 2.0f, 2.2f);
            for (int spoke = 0; spoke < 3; ++spoke) {
                float a = reelAngle + spoke * 2.094395f;
                g.drawLine(rxOff, ry, rxOff + std::cos(a) * (rr - 4.0f), ry + std::sin(a) * (rr - 4.0f), 3.0f);
            }
            g.setColour(juce::Colours::white);
            g.fillEllipse(rxOff - 8.0f, ry - 8.0f, 16.0f, 16.0f);
        }

        // Magnetic Tape Path between Reels
        g.setColour(juce::Colour(0xfff59e0b));
        g.drawLine(deckWin.getCentreX() - 140.0f, deckWin.getCentreY() + 42.0f,
                   deckWin.getCentreX() + 140.0f, deckWin.getCentreY() + 42.0f, 2.0f);
    }
}

void CyberUltimateHybridAudioProcessorEditor::resized() {
    // Top Navigation Tabs
    int x = 14;
    tabBlackface.setBounds(x, 10, 160, 34); x += 166;
    tabBassman.setBounds(x,   10, 160, 34); x += 166;
    tabReverb.setBounds(x,    10, 175, 34); x += 181;
    tabDelay.setBounds(x,     10, 175, 34);

    // Global Right-Side Header Selectors & Audio Settings Button
    btnAudioSettings.setBounds(getWidth() - 395, 10, 130, 34);
    channelModeBox.setBounds(getWidth() - 255, 10, 130, 34);
    qualityModeBox.setBounds(getWidth() - 120, 10, 106, 34);

    // Page-Specific Switches
    if (currentPage == 0) {
        brightToggle.setBounds(500, 92, 160, 28);
        cabSimBox.setBounds(670, 92, 240, 28);
        cabSimBox.setVisible(true);
    } else if (currentPage == 1) {
        slopeModeBox.setBounds(545, 92, 195, 28);
        powerTubesBox.setBounds(750, 92, 200, 28);
        cabSimBox.setVisible(false);
    } else if (currentPage == 2) {
        tremModeBox.setBounds(700, 92, 255, 28);
        cabSimBox.setVisible(false);
    } else if (currentPage == 3) {
        pingPongToggle.setBounds(760, 92, 195, 28);
        cabSimBox.setVisible(false);
    }

    // Layout Knobs for the Active Page
    std::vector<BoundRotary*> pageKnobs;
    for (auto& k : knobs) {
        if (k.pageIndex == currentPage)
            pageKnobs.push_back(&k);
    }

    const int n = (int)pageKnobs.size();
    if (n > 0) {
        const int areaX = 48;
        const int areaW = getWidth() - 96;
        const int slotW = areaW / n;
        const int knobY = 152;
        for (int i = 0; i < n; ++i) {
            int cx = areaX + i * slotW + slotW / 2;
            pageKnobs[i]->label->setBounds(cx - 58, knobY, 116, 22);
            pageKnobs[i]->slider->setBounds(cx - 52, knobY + 24, 104, 126);
        }
    }
}

void CyberUltimateHybridAudioProcessorEditor::openAudioSettings() {
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Cyber Ultimate Hybrid Audio Settings",
        "AUDIO HARDWARE ROUTING & GUITAR INPUT:\n\n"
        "1. Standalone Application (Windows PC):\n"
        "   - Use the desktop application ('CyberUltimateHybrid-Standalone.exe') which features a dedicated Hardware Audio Settings window.\n"
        "   - Click the top right [AUDIO SETTINGS] button to select your USB Guitar Audio Interface (Audient, Focusrite, etc.), Instrument Input Channel (Channel 1 / Channel 2), and Latency Buffer Size (128 samples / 2.7ms).\n"
        "   - The real-time input meter verifies your guitar cable signal.\n\n"
        "2. VST3 / AU DAW Plugin & MODEP Mixer:\n"
        "   - Audio input channels, sample rate (48 kHz), and buffer latency are managed directly in your DAW's Audio Device Preferences (REAPER, Ableton, Cubase, Studio One, etc.) or MODEP Jack router."
    );
}

