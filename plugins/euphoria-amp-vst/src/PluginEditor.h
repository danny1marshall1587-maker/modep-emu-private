#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class EuphoriaAmpAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer {
public:
    explicit EuphoriaAmpAudioProcessorEditor(EuphoriaAmpAudioProcessor&);
    ~EuphoriaAmpAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;

    struct GuiKnobRect {
        int pageIdx = 0;
        bool isStepped = false;
        int steps = 0;
        juce::Rectangle<int> rect;
    };

    void initKnobLayout();
    juce::Point<int> toVirtualPt(juce::Point<float> localPt) const noexcept;

    void drawCornerProtector3D(juce::Graphics& g, int x, int y, int w, int h, int cornerType,
                               juce::Colour metalCol, juce::Colour hiCol);
    void drawFacetedJewelLamp(juce::Graphics& g, int cx, int cy, int r,
                              juce::Colour jewelBaseCol, juce::Colour glowCol, juce::Colour rimCol);
    void drawBasketweaveGrille(juce::Graphics& g, int x1, int y1, int x2, int y2,
                               juce::Colour darkYarn, juce::Colour lightYarn);
    void drawStylizedKnob(juce::Graphics& g, int paramIdx, int pageStyle);

    EuphoriaAmpAudioProcessor& processor;

    int activePage = 0;
    float animPhase = 0.0f;
    float strobeWheelPhase = 0.0f;

    uint32_t learnNoiseStartTime = 0;
    uint32_t learnNoiseDoneTime = 0;
    bool wasLearningNoise = false;
    uint32_t fuzzLearnStartTime = 0;

    int dragKnobIdx = -1;
    int dragStartY = 0;
    float dragStartVal = 0.0f;
    bool dragDidMove = false;

    std::vector<GuiKnobRect> knobLayout;

    // Embedded 3D Hardware Assets (loaded from BinaryData)
    juce::Image imgWoodFloor;
    juce::Image imgChassisWide;
    juce::Image imgChassisCleanBlue;
    juce::Image imgChassisCrunchBrown;
    juce::Image imgChassisLeadRed;
    juce::Image imgRackSpaceTopaz;
    juce::Image imgRackReverbPink;
    juce::Image imgRackTremOrange;
    juce::Image imgRackDelayGreen;
    juce::Image imgKlon;
    juce::Image imgTs9;
    juce::Image imgBlues;
    juce::Image imgRat;
    juce::Image imgFuzz;
    juce::Image imgKnobStrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuphoriaAmpAudioProcessorEditor)
};
