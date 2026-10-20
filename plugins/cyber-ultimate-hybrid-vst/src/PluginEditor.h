#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>

class VintageHardwareLookAndFeel : public juce::LookAndFeel_V4 {
public:
    enum class KnobTheme { BlackfaceSkirted, BassmanChicken, RackBakelite, TapeChrome };

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
};

class CyberUltimateHybridAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer {
public:
    explicit CyberUltimateHybridAudioProcessorEditor(CyberUltimateHybridAudioProcessor&);
    ~CyberUltimateHybridAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setActivePage(int pageIdx);

    CyberUltimateHybridAudioProcessor& audioProcessor;
    VintageHardwareLookAndFeel hwLookAndFeel;
    int currentPage = 0; // 0=Blackface, 1=Bassman/JMP, 2=Spring Reverb & Vibrato, 3=Studio Tape Delay
    float reelAngle = 0.0f;

    // Top Navigation Tabs
    juce::TextButton tabBlackface { "1. '65 BLACKFACE AB763" };
    juce::TextButton tabBassman   { "2. '59 BASSMAN / '68 JMP" };
    juce::TextButton tabReverb    { "3. SPRING REVERB & VIBRATO" };
    juce::TextButton tabDelay     { "4. STUDIO TAPE DELAY RACK" };
    juce::TextButton btnAudioSettings { "AUDIO SETTINGS" };

    void openAudioSettings();

    // Global HeaderSelectors
    juce::ComboBox channelModeBox;
    juce::ComboBox qualityModeBox;
    juce::ComboBox cabSimBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabSimAtt;

    struct BoundRotary {
        juce::String paramId;
        juce::String title;
        int pageIndex;
        int themeId;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<BoundRotary> knobs;

    // Page-specific hardware switches
    juce::ToggleButton brightToggle { "120pF BRIGHT CAP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> brightAtt;

    juce::ComboBox slopeModeBox;
    juce::ComboBox powerTubesBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> powerTubesAtt;

    juce::ComboBox tremModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tremModeAtt;

    juce::ToggleButton pingPongToggle { "STEREO PING-PONG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAtt;

    void addKnob(const juce::String& paramId, const juce::String& title, int pageIdx, int themeId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyberUltimateHybridAudioProcessorEditor)
};
