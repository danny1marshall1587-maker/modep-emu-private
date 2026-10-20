#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

struct NeonLookAndFeel : public juce::LookAndFeel_V4
{
    NeonLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFA000FF));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF1A0033));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFD400FF));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1A0033));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFFD400FF));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A0033));
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        auto cornerSize = 8.0f;
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, cornerSize);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId).withAlpha(0.6f));
        g.drawRoundedRectangle (bounds, cornerSize, 1.5f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId).withAlpha(0.2f));
        g.drawRoundedRectangle (bounds.expanded(1.0f), cornerSize + 1.0f, 3.0f);
        juce::Path p;
        float arrowSize = 6.0f;
        p.addTriangle (width - 15.0f, height * 0.5f - arrowSize * 0.4f,
                       width - 15.0f - arrowSize, height * 0.5f - arrowSize * 0.4f,
                       width - 15.0f - arrowSize * 0.5f, height * 0.5f + arrowSize * 0.4f);
        g.setColour (juce::Colours::white.withAlpha(0.7f));
        g.fillPath (p);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override
    {
        auto cornerSize = 8.0f;
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto baseColour = backgroundColour;
        if (isButtonDown) baseColour = baseColour.brighter (0.2f);
        else if (isMouseOverButton) baseColour = baseColour.brighter (0.1f);
        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);
        g.setColour (juce::Colour(0xFFD400FF).withAlpha(0.6f));
        g.drawRoundedRectangle (bounds, cornerSize, 1.5f);
        if (isMouseOverButton) {
            g.setColour (juce::Colour(0xFFD400FF).withAlpha(0.3f));
            g.drawRoundedRectangle (bounds.expanded(1.0f), cornerSize + 1.0f, 3.0f);
        }
    }
    
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto trackWidth = 4.0f;
        auto isHorizontal = slider.isHorizontal();
        auto track = isHorizontal ? juce::Rectangle<float> (x, y + height * 0.5f - trackWidth * 0.5f, width, trackWidth)
                                  : juce::Rectangle<float> (x + width * 0.5f - trackWidth * 0.5f, y, trackWidth, height);
        g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
        g.fillRoundedRectangle (track, 2.0f);
        auto thumbSize = 16.0f;
        auto thumbX = isHorizontal ? sliderPos - thumbSize * 0.5f : x + width * 0.5f - thumbSize * 0.5f;
        auto thumbY = isHorizontal ? y + height * 0.5f - thumbSize * 0.5f : sliderPos - thumbSize * 0.5f;
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillEllipse (thumbX, thumbY, thumbSize, thumbSize);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId).withAlpha(0.3f));
        g.drawEllipse (thumbX - 2, thumbY - 2, thumbSize + 4, thumbSize + 4, 2.0f);
    }
};

struct LavaBlob
{
    juce::Point<float> pos, velocity;
    float radius;
    juce::Colour colour;
    void update(int w, int h) {
        pos += velocity;
        if (pos.x < 0 || pos.x > w) velocity.x *= -1;
        if (pos.y < 0 || pos.y > h) velocity.y *= -1;
    }
};

class StrobeView : public juce::Component
{
public:
    void setDeviation(float cents) { deviationCents = cents; }
    void setActive(bool active) { isActive = active; }
    void paint(juce::Graphics& g) override;
    void updateAnimation() { if (isActive) rotation += deviationCents * 0.005f; }
private:
    float deviationCents = 0.0f;
    float rotation = 0.0f;
    bool isActive = false;
};

class GalaxyStrobeTuneAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    GalaxyStrobeTuneAudioProcessorEditor (GalaxyStrobeTuneAudioProcessor&);
    ~GalaxyStrobeTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    GalaxyStrobeTuneAudioProcessor& audioProcessor;
    NeonLookAndFeel neonLookAndFeel;
    std::vector<LavaBlob> blobs;

    juce::Slider refASlider, smoothingSlider, capoSlider;
    juce::Label refALabel, smoothingLabel, capoLabel;
    juce::ComboBox profileSelector;
    juce::TextButton captureButton { "CAPTURE OFFSET" };
    
    StrobeView strobeView;
    juce::Label noteLabel, centsLabel;

    juce::OpenGLContext openGLContext;
    float smoothedCents = 0.0f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> refAAttachment, smoothingAttachment, capoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GalaxyStrobeTuneAudioProcessorEditor)
};
