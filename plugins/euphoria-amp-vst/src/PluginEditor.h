#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class EuphoriaHardwareLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
};

// Table-driven editor: every control of the Windows standalone (all 7 pages,
// the five pedals with their separate voicing + (OLD)/HYPER switches, space,
// reverb, tremolo, tape delay) is built from euphoriaParamTable().
class EuphoriaAmpAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit EuphoriaAmpAudioProcessorEditor(EuphoriaAmpAudioProcessor&);
    ~EuphoriaAmpAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Control {
        int paramIndex = 0;
        int page = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;     // continuous params
        std::unique_ptr<juce::ComboBox> combo;    // stepped params
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
    };

    void setActivePage(int pageIdx);
    void layoutPage(juce::Rectangle<int> area);

    EuphoriaAmpAudioProcessor& audioProcessor;
    EuphoriaHardwareLookAndFeel hwLookAndFeel;
    int currentPage = 0;

    std::vector<std::unique_ptr<juce::TextButton>> tabs;
    std::vector<Control> controls; // header (page -1) + all pages

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuphoriaAmpAudioProcessorEditor)
};
