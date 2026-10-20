#pragma once
#define CYBER_ENABLE_STUDIO_DELAY 1
#include "../../euphoria-amp.lv2/src/EuphoriaAmpEngine.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include <cmath>

// ============================================================================
// 1. 64-FRAME ROTATING 3D KNOB FILMSTRIP LOOK AND FEEL
// ============================================================================
class FilmstripKnobLookAndFeel : public juce::LookAndFeel_V4 {
private:
    juce::Image filmstrip;
    int numFrames;

public:
    FilmstripKnobLookAndFeel(const juce::Image& stripImg = {}, int frames = 64)
        : filmstrip(stripImg), numFrames(frames) {}

    void setFilmstrip(const juce::Image& img, int frames = 64) {
        filmstrip = img;
        numFrames = frames;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float, juce::Slider&) override {
        if (filmstrip.isValid() && numFrames > 0) {
            int frameIndex = juce::jlimit(0, numFrames - 1, (int)(sliderPos * (numFrames - 1)));
            int frameHeight = filmstrip.getHeight() / numFrames;
            int frameWidth = filmstrip.getWidth();

            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(filmstrip, x, y, width, height,
                        0, frameIndex * frameHeight, frameWidth, frameHeight);
        } else {
            // Procedural luxury knurled dial fallback
            const float radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
            const float centreX = (float)x + (float)width * 0.5f;
            const float centreY = (float)y + (float)height * 0.5f;

            // Shadow
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillEllipse(centreX - radius + 1.5f, centreY - radius + 3.0f, radius * 2.0f, radius * 2.0f);

            // Metallic Body
            juce::ColourGradient grad(juce::Colour(0xff2a2d32), centreX - radius * 0.4f, centreY - radius * 0.4f,
                                      juce::Colour(0xff121315), centreX + radius, centreY + radius, true);
            g.setGradientFill(grad);
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

            // Bevel rim
            g.setColour(juce::Colour(0xff4a5058));
            g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

            // Pointer notch
            const float angle = -2.356194f + sliderPos * 4.712389f; // -135 to +135 deg
            const float px = centreX + std::sin(angle) * (radius - 4.0f);
            const float py = centreY - std::cos(angle) * (radius - 4.0f);
            g.setColour(juce::Colour(0xfff3f4f6));
            g.drawLine(centreX, centreY, px, py, 2.2f);
        }
    }
};

// ============================================================================
// 2. SELF-CONTAINED 3D PEDAL COMPONENT (Keeps Knobs Locked to Its Own Sprite!)
// ============================================================================
class Modular3DPedalSlot : public juce::Component {
public:
    juce::Image pedalSprite;
    juce::String pedalName;
    juce::Slider knobDrive, knobTone, knobLevel;
    juce::Label lblDrive, lblTone, lblLevel;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::TextButton learnMaxBtn { "MAX (10)" };
    juce::TextButton learnCleanBtn { "CLEAN (7)" };
    bool hasLearnButtons = false;
    bool hasToneKnob = true;
    bool isActive = true;

    Modular3DPedalSlot(const juce::Image& spriteImage, const juce::String& name,
                       bool isFuzzWithLearn = false, bool withTone = true,
                       FilmstripKnobLookAndFeel* laf = nullptr)
        : pedalSprite(spriteImage), pedalName(name),
          hasLearnButtons(isFuzzWithLearn), hasToneKnob(withTone)
    {
        auto setupKnob = [this, laf](juce::Slider& s, juce::Label& l, const juce::String& txt) {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setRange(0.0, 10.0, 0.1);
            s.setValue(5.0);
            if (laf) s.setLookAndFeel(laf);
            addAndMakeVisible(s);

            l.setText(txt, juce::dontSendNotification);
            l.setFont(juce::Font(10.0f, juce::Font::bold));
            l.setJustificationType(juce::Justification::centred);
            l.setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
            addAndMakeVisible(l);
        };

        setupKnob(knobDrive, lblDrive, isFuzzWithLearn ? "FUZZ" : (name.contains("Rat") ? "DIST" : "DRIVE"));
        if (hasToneKnob) {
            setupKnob(knobTone, lblTone, name.contains("Rat") ? "FILTER" : (name.contains("Klon") ? "TREBLE" : "TONE"));
        }
        setupKnob(knobLevel, lblLevel, "LEVEL");

        bypassButton.setToggleState(true, juce::dontSendNotification);
        bypassButton.onClick = [this]() {
            isActive = bypassButton.getToggleState();
            repaint();
        };
        addAndMakeVisible(bypassButton);

        if (hasLearnButtons) {
            learnMaxBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb91c1c));
            learnMaxBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(learnMaxBtn);

            learnCleanBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff047857));
            learnCleanBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(learnCleanBtn);
        }
    }

    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        if (pedalSprite.isValid()) {
            // Draws isolated 3D pedal + ray-traced transparent drop shadow onto the wood floor!
            g.drawImage(pedalSprite, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }

        // Active LED Indicator
        auto b = getLocalBounds().toFloat();
        float ledX = b.getCentreX();
        float ledY = b.getHeight() * 0.22f;
        float ledR = b.getWidth() * 0.035f;

        if (isActive) {
            g.setColour(juce::Colour(0xffef4444).withAlpha(0.35f));
            g.fillEllipse(ledX - ledR * 2.2f, ledY - ledR * 2.2f, ledR * 4.4f, ledR * 4.4f);
            g.setColour(juce::Colour(0xfff87171));
            g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
        } else {
            g.setColour(juce::Colour(0xff374151));
            g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
        }
    }

    void resized() override {
        auto b = getLocalBounds().toFloat();
        float knobSize = b.getWidth() * 0.24f;
        float lblH = 14.0f;

        if (hasToneKnob) {
            // 3-Knob Triangle Layout
            // Top Row: Drive (left) and Tone/Treble (right)
            float topY = b.getHeight() * 0.32f;
            knobDrive.setBounds(juce::Rectangle<float>(knobSize, knobSize)
                .withCentre({ b.getCentreX() - knobSize * 0.85f, topY }).toNearestInt());
            lblDrive.setBounds(juce::Rectangle<float>(knobSize * 1.5f, lblH)
                .withCentre({ b.getCentreX() - knobSize * 0.85f, topY - knobSize * 0.65f }).toNearestInt());

            knobTone.setBounds(juce::Rectangle<float>(knobSize, knobSize)
                .withCentre({ b.getCentreX() + knobSize * 0.85f, topY }).toNearestInt());
            lblTone.setBounds(juce::Rectangle<float>(knobSize * 1.5f, lblH)
                .withCentre({ b.getCentreX() + knobSize * 0.85f, topY - knobSize * 0.65f }).toNearestInt());

            // Center Below: Level/Output
            float midY = b.getHeight() * 0.50f;
            knobLevel.setBounds(juce::Rectangle<float>(knobSize, knobSize)
                .withCentre({ b.getCentreX(), midY }).toNearestInt());
            lblLevel.setBounds(juce::Rectangle<float>(knobSize * 1.5f, lblH)
                .withCentre({ b.getCentreX(), midY - knobSize * 0.65f }).toNearestInt());
        } else {
            // 2-Knob Side-by-Side (Fuzz Face style)
            float knobY = b.getHeight() * 0.35f;
            knobDrive.setBounds(juce::Rectangle<float>(knobSize * 1.1f, knobSize * 1.1f)
                .withCentre({ b.getCentreX() - knobSize * 0.85f, knobY }).toNearestInt());
            lblDrive.setBounds(juce::Rectangle<float>(knobSize * 1.5f, lblH)
                .withCentre({ b.getCentreX() - knobSize * 0.85f, knobY - knobSize * 0.7f }).toNearestInt());

            knobLevel.setBounds(juce::Rectangle<float>(knobSize * 1.1f, knobSize * 1.1f)
                .withCentre({ b.getCentreX() + knobSize * 0.85f, knobY }).toNearestInt());
            lblLevel.setBounds(juce::Rectangle<float>(knobSize * 1.5f, lblH)
                .withCentre({ b.getCentreX() + knobSize * 0.85f, knobY - knobSize * 0.7f }).toNearestInt());
        }

        // Fuzz Two-Button Volume Learn (10s Continuous Window)
        if (hasLearnButtons) {
            float btnW = b.getWidth() * 0.38f;
            float btnH = b.getHeight() * 0.08f;
            float btnY = b.getHeight() * 0.56f;
            learnMaxBtn.setBounds(juce::Rectangle<float>(btnW, btnH)
                .withCentre({ b.getCentreX() - btnW * 0.55f, btnY }).toNearestInt());
            learnCleanBtn.setBounds(juce::Rectangle<float>(btnW, btnH)
                .withCentre({ b.getCentreX() + btnW * 0.55f, btnY }).toNearestInt());
        }

        // Footswitch Button
        float swW = b.getWidth() * 0.45f;
        float swH = b.getHeight() * 0.12f;
        bypassButton.setBounds(juce::Rectangle<float>(swW, swH)
            .withCentre({ b.getCentreX(), b.getHeight() * 0.84f }).toNearestInt());
    }
};

// ============================================================================
// 3. LAYERED STUDIO FLOOR + AUTO-REFLOWING PEDALBOARD & MODULAR GUITAR
// ============================================================================
class Responsive3DRigView : public juce::Component {
private:
    juce::Image woodFloorTile;
    juce::Image chassisWideImg, chassisCompactImg;
    juce::Image luxuryGuitarSprite;
    FilmstripKnobLookAndFeel filmstripLaf;

    juce::Rectangle<float> ampBounds;
    juce::Rectangle<float> guitarBounds;
    bool isCompactLayout = false;

    juce::OwnedArray<Modular3DPedalSlot> pedalSlots;

public:
    Responsive3DRigView() {
        loadAssets();
    }

    void loadAssets() {
        auto loadImg = [](const juce::String& name) -> juce::Image {
            // Check JUCE_Assets or plugins/euphoria-amp-vst/assets
            juce::File f1 = juce::File::getCurrentWorkingDirectory().getChildFile("JUCE_Assets").getChildFile(name);
            if (f1.existsAsFile()) return juce::ImageFileFormat::loadFrom(f1);

            juce::File f2 = juce::File::getCurrentWorkingDirectory().getChildFile("plugins/euphoria-amp-vst/assets").getChildFile(name);
            if (f2.existsAsFile()) return juce::ImageFileFormat::loadFrom(f2);

            return {};
        };

        woodFloorTile      = loadImg("wood_floor_tile.png");
        chassisWideImg     = loadImg("chassis_wide.png");
        chassisCompactImg  = loadImg("chassis_compact.png");
        luxuryGuitarSprite = loadImg("guitar_luxury.png");
        
        juce::Image knobStrip = loadImg("knob_filmstrip.png");
        if (knobStrip.isValid()) {
            filmstripLaf.setFilmstrip(knobStrip, 64);
        }

        // Initialize default 5-pedal suite
        juce::Image fuzzImg  = loadImg("pedal_fuzz_round.png");
        juce::Image klonImg  = loadImg("pedal_klon_gold.png");
        juce::Image ts808Img = loadImg("pedal_ts9_green.png");
        juce::Image bd2Img   = loadImg("pedal_blues_blue.png");
        juce::Image ratImg   = loadImg("pedal_rat_black.png");

        addModularPedal(fuzzImg,  "Fluid Fuzz Face",  true,  false);
        addModularPedal(klonImg,  "Fluid Centaur",    false, true);
        addModularPedal(ts808Img, "TS808 Screamer",   false, true);
        addModularPedal(bd2Img,   "BD-2 Blues Driver",false, true);
        addModularPedal(ratImg,   "Fluid Rodent",     false, true);
    }

    void addModularPedal(const juce::Image& pedalImg, const juce::String& name,
                         bool isFuzz = false, bool withTone = true) {
        auto* newPedal = pedalSlots.add(new Modular3DPedalSlot(pedalImg, name, isFuzz, withTone, &filmstripLaf));
        addAndMakeVisible(newPedal);
        resized();
    }

    int getNumPedals() const { return pedalSlots.size(); }
    Modular3DPedalSlot* getPedal(int index) { return pedalSlots[index]; }

    EuphoriaDSP::EuphoriaAmpEngine* activeEngine = nullptr;

    void attachToEngine(EuphoriaDSP::EuphoriaAmpEngine* engine) {
        activeEngine = engine;
        if (!activeEngine) return;

        // Pedal 0: Fluid Fuzz Face
        if (pedalSlots.size() > 0) {
            auto* fuzz = pedalSlots[0];
            fuzz->knobDrive.setValue(engine->fuzzFace.fuzz, juce::dontSendNotification);
            fuzz->knobDrive.onValueChange = [this, fuzz]() {
                if (activeEngine) activeEngine->fuzzFace.fuzz = (float)fuzz->knobDrive.getValue();
            };
            fuzz->knobLevel.setValue(engine->fuzzFace.level, juce::dontSendNotification);
            fuzz->knobLevel.onValueChange = [this, fuzz]() {
                if (activeEngine) activeEngine->fuzzFace.level = (float)fuzz->knobLevel.getValue();
            };
            fuzz->isActive = engine->fuzzFace.enabled;
            fuzz->bypassButton.setToggleState(engine->fuzzFace.enabled, juce::dontSendNotification);
            fuzz->bypassButton.onClick = [this, fuzz]() {
                fuzz->isActive = fuzz->bypassButton.getToggleState();
                if (activeEngine) activeEngine->fuzzFace.enabled = fuzz->isActive;
                fuzz->repaint();
            };
            fuzz->learnMaxBtn.onClick = [this]() {
                if (activeEngine) activeEngine->fuzzFace.triggerLearnMaxVolume(10.0f);
            };
            fuzz->learnCleanBtn.onClick = [this]() {
                if (activeEngine) activeEngine->fuzzFace.triggerLearnCleanVolume(10.0f);
            };
        }

        // Pedal 1: Fluid Centaur (Klon)
        if (pedalSlots.size() > 1) {
            auto* klon = pedalSlots[1];
            klon->knobDrive.setValue(engine->centaur.gain, juce::dontSendNotification);
            klon->knobDrive.onValueChange = [this, klon]() {
                if (activeEngine) activeEngine->centaur.gain = (float)klon->knobDrive.getValue();
            };
            klon->knobTone.setValue(engine->centaur.treble, juce::dontSendNotification);
            klon->knobTone.onValueChange = [this, klon]() {
                if (activeEngine) activeEngine->centaur.treble = (float)klon->knobTone.getValue();
            };
            klon->knobLevel.setValue(engine->centaur.output, juce::dontSendNotification);
            klon->knobLevel.onValueChange = [this, klon]() {
                if (activeEngine) activeEngine->centaur.output = (float)klon->knobLevel.getValue();
            };
            klon->isActive = engine->centaur.enabled;
            klon->bypassButton.setToggleState(engine->centaur.enabled, juce::dontSendNotification);
            klon->bypassButton.onClick = [this, klon]() {
                klon->isActive = klon->bypassButton.getToggleState();
                if (activeEngine) activeEngine->centaur.enabled = klon->isActive;
                klon->repaint();
            };
        }

        // Pedal 2: TS808 Screamer
        if (pedalSlots.size() > 2) {
            auto* ts = pedalSlots[2];
            ts->knobDrive.setValue(engine->ts808.drive, juce::dontSendNotification);
            ts->knobDrive.onValueChange = [this, ts]() {
                if (activeEngine) activeEngine->ts808.drive = (float)ts->knobDrive.getValue();
            };
            ts->knobTone.setValue(engine->ts808.tone, juce::dontSendNotification);
            ts->knobTone.onValueChange = [this, ts]() {
                if (activeEngine) activeEngine->ts808.tone = (float)ts->knobTone.getValue();
            };
            ts->knobLevel.setValue(engine->ts808.level, juce::dontSendNotification);
            ts->knobLevel.onValueChange = [this, ts]() {
                if (activeEngine) activeEngine->ts808.level = (float)ts->knobLevel.getValue();
            };
            ts->isActive = engine->ts808.enabled;
            ts->bypassButton.setToggleState(engine->ts808.enabled, juce::dontSendNotification);
            ts->bypassButton.onClick = [this, ts]() {
                ts->isActive = ts->bypassButton.getToggleState();
                if (activeEngine) activeEngine->ts808.enabled = ts->isActive;
                ts->repaint();
            };
        }

        // Pedal 3: BD-2 Blues Driver
        if (pedalSlots.size() > 3) {
            auto* bd = pedalSlots[3];
            bd->knobDrive.setValue(engine->bd2.gain, juce::dontSendNotification);
            bd->knobDrive.onValueChange = [this, bd]() {
                if (activeEngine) activeEngine->bd2.gain = (float)bd->knobDrive.getValue();
            };
            bd->knobTone.setValue(engine->bd2.tone, juce::dontSendNotification);
            bd->knobTone.onValueChange = [this, bd]() {
                if (activeEngine) activeEngine->bd2.tone = (float)bd->knobTone.getValue();
            };
            bd->knobLevel.setValue(engine->bd2.level, juce::dontSendNotification);
            bd->knobLevel.onValueChange = [this, bd]() {
                if (activeEngine) activeEngine->bd2.level = (float)bd->knobLevel.getValue();
            };
            bd->isActive = engine->bd2.enabled;
            bd->bypassButton.setToggleState(engine->bd2.enabled, juce::dontSendNotification);
            bd->bypassButton.onClick = [this, bd]() {
                bd->isActive = bd->bypassButton.getToggleState();
                if (activeEngine) activeEngine->bd2.enabled = bd->isActive;
                bd->repaint();
            };
        }

        // Pedal 4: Fluid Rodent (RAT)
        if (pedalSlots.size() > 4) {
            auto* rat = pedalSlots[4];
            rat->knobDrive.setValue(engine->rodent.distortion, juce::dontSendNotification);
            rat->knobDrive.onValueChange = [this, rat]() {
                if (activeEngine) activeEngine->rodent.distortion = (float)rat->knobDrive.getValue();
            };
            rat->knobTone.setValue(engine->rodent.filter, juce::dontSendNotification);
            rat->knobTone.onValueChange = [this, rat]() {
                if (activeEngine) activeEngine->rodent.filter = (float)rat->knobTone.getValue();
            };
            rat->knobLevel.setValue(engine->rodent.volume, juce::dontSendNotification);
            rat->knobLevel.onValueChange = [this, rat]() {
                if (activeEngine) activeEngine->rodent.volume = (float)rat->knobLevel.getValue();
            };
            rat->isActive = engine->rodent.enabled;
            rat->bypassButton.setToggleState(engine->rodent.enabled, juce::dontSendNotification);
            rat->bypassButton.onClick = [this, rat]() {
                rat->isActive = rat->bypassButton.getToggleState();
                if (activeEngine) activeEngine->rodent.enabled = rat->isActive;
                rat->repaint();
            };
        }
    }

    void updateTelemetry() {
        if (!activeEngine) return;
        if (pedalSlots.size() > 0) {
            auto* fuzz = pedalSlots[0];
            if (activeEngine->fuzzFace.isLearningMax) {
                int secs = (int)std::ceil(activeEngine->fuzzFace.learnSamplesRemaining / activeEngine->fuzzFace.sampleRate);
                fuzz->learnMaxBtn.setButtonText("LRN " + juce::String(secs) + "s");
            } else {
                fuzz->learnMaxBtn.setButtonText("MAX (10)");
            }

            if (activeEngine->fuzzFace.isLearningClean) {
                int secs = (int)std::ceil(activeEngine->fuzzFace.learnSamplesRemaining / activeEngine->fuzzFace.sampleRate);
                fuzz->learnCleanBtn.setButtonText("LRN " + juce::String(secs) + "s");
            } else {
                fuzz->learnCleanBtn.setButtonText("CLEAN (7)");
            }
        }
    }

    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        // LAYER 0: Tiled 3D Hardwood Studio Floor Background
        if (woodFloorTile.isValid()) {
            g.setTiledImageFill(woodFloorTile, 0, 0, 1.0f);
            g.fillAll();
        } else {
            // Elegant dark walnut procedural fallback
            juce::ColourGradient woodGrad(juce::Colour(0xff22140c), 0, 0,
                                         juce::Colour(0xff120904), (float)getWidth(), (float)getHeight(), false);
            g.setGradientFill(woodGrad);
            g.fillAll();
        }

        // Soft ambient room vignette
        juce::ColourGradient roomVignette(juce::Colours::transparentBlack,
                                          getWidth() * 0.5f, getHeight() * 0.4f,
                                          juce::Colours::black.withAlpha(0.6f),
                                          0.0f, 0.0f, true);
        g.setGradientFill(roomVignette);
        g.fillAll();

        // LAYER 1A: 3D Amp Chassis Sprite with Baked Shadow
        const juce::Image& activeChassis = isCompactLayout ? chassisCompactImg : chassisWideImg;
        if (activeChassis.isValid()) {
            g.drawImage(activeChassis, ampBounds, juce::RectanglePlacement::centred);
        }

        // LAYER 1B: Modular 3D Luxury Carved-Top Amber Sunburst Guitar
        if (luxuryGuitarSprite.isValid()) {
            g.drawImage(luxuryGuitarSprite, guitarBounds, juce::RectanglePlacement::centred);
        }
    }

    void resized() override {
        auto bounds = getLocalBounds().toFloat().reduced(14.0f);
        isCompactLayout = (bounds.getWidth() / std::max(1.0f, bounds.getHeight())) < 1.35f;

        // Top section: Amp Head + Modular Luxury Guitar View
        auto topSection = bounds.removeFromTop(bounds.getHeight() * 0.44f);
        guitarBounds = topSection.removeFromRight(topSection.getWidth() * 0.28f).reduced(4.0f);
        ampBounds    = topSection.reduced(4.0f);

        // Bottom section: Dynamic Pedalboard Strip on the Wood Floor
        // Automatically divides available width by however many pedals currently exist!
        int numPedals = pedalSlots.size();
        if (numPedals > 0) {
            float slotWidth = bounds.getWidth() / (float)numPedals;
            for (int i = 0; i < numPedals; ++i) {
                auto slotRect = bounds.removeFromLeft(slotWidth).reduced(6.0f);
                pedalSlots[i]->setBounds(slotRect.toNearestInt());
            }
        }
    }
};
