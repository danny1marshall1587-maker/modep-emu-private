#include "PluginProcessor.h"
#include "PluginEditor.h"

void StrobeView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto center = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;

    if (isActive)
    {
        int numBars = 32;
        for (int i = 0; i < numBars; ++i)
        {
            float angle = rotation + (juce::MathConstants<float>::twoPi * i / numBars);
            juce::Path p;
            p.addCentredArc(center.x, center.y, radius, radius, angle, -0.05f, 0.05f, true);
            
            juce::Colour c = std::abs(deviationCents) < 1.0f ? juce::Colours::lightgreen : juce::Colour(0xFFD400FF);
            g.setColour(c.withAlpha(0.8f));
            g.strokePath(p, juce::PathStrokeType(15.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(c.withAlpha(0.2f));
            g.strokePath(p, juce::PathStrokeType(30.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        g.setColour(juce::Colours::grey.withAlpha(0.1f));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2, 1.0f);
    }
}

GalaxyStrobeTuneAudioProcessorEditor::GalaxyStrobeTuneAudioProcessorEditor (GalaxyStrobeTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel(&neonLookAndFeel);
    setSize (500, 600);

    refASlider.setSliderStyle(juce::Slider::LinearHorizontal);
    addAndMakeVisible(refASlider);
    refAAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "refA", refASlider);
    refALabel.setText("REF A4", juce::dontSendNotification);
    refALabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(refALabel);

    capoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    addAndMakeVisible(capoSlider);
    capoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "capo", capoSlider);
    capoLabel.setText("CAPO", juce::dontSendNotification);
    capoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(capoLabel);

    smoothingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    addAndMakeVisible(smoothingSlider);
    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "smoothing", smoothingSlider);
    smoothingLabel.setText("STABILITY", juce::dontSendNotification);
    smoothingLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(smoothingLabel);

    addAndMakeVisible(profileSelector);
    profileAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "profile", profileSelector);
    for (int i = 0; i < (int)audioProcessor.getProfiles().size(); ++i)
        profileSelector.addItem(audioProcessor.getProfiles()[i].name, i + 1);

    addAndMakeVisible(captureButton);
    captureButton.onClick = [this] {
        float freq = audioProcessor.getCurrentFrequency();
        if (freq > 20.0f)
        {
            float midi = 12.0f * std::log2(freq / (audioProcessor.apvts.getRawParameterValue("refA")->load())) + 69.0f;
            int capo = (int)audioProcessor.apvts.getRawParameterValue("capo")->load();
            midi -= capo;
            int noteIndex = (int)std::fmod(std::round(midi) + 120, 12);
            float cents = (midi - std::round(midi)) * 100.0f;
            int profileIdx = (int)audioProcessor.apvts.getRawParameterValue("profile")->load();
            audioProcessor.getProfiles()[profileIdx].offsets[noteIndex] = cents;
            captureButton.setButtonText("SAVED " + juce::String(cents, 1));
        }
    };

    addAndMakeVisible(strobeView);

    noteLabel.setFont(juce::Font(100.0f, juce::Font::bold));
    noteLabel.setJustificationType(juce::Justification::centred);
    noteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(noteLabel);

    centsLabel.setFont(juce::Font(24.0f));
    centsLabel.setJustificationType(juce::Justification::centred);
    centsLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD400FF));
    addAndMakeVisible(centsLabel);

    for (int i = 0; i < 5; ++i)
    {
        LavaBlob blob;
        blob.pos = { (float)juce::Random::getSystemRandom().nextInt(500), (float)juce::Random::getSystemRandom().nextInt(600) };
        blob.velocity = { juce::Random::getSystemRandom().nextFloat() * 1.0f - 0.5f, juce::Random::getSystemRandom().nextFloat() * 1.0f - 0.5f };
        blob.radius = (float)juce::Random::getSystemRandom().nextInt(juce::Range<int>(100, 250));
        blob.colour = juce::Colour(0xFFD400FF).withAlpha(0.06f);
        blobs.push_back(blob);
    }

    startTimerHz(60);
    openGLContext.attachTo(*this);
}

GalaxyStrobeTuneAudioProcessorEditor::~GalaxyStrobeTuneAudioProcessorEditor() { openGLContext.detach(); setLookAndFeel(nullptr); }

void GalaxyStrobeTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0A001A));
    for (auto& blob : blobs)
    {
        g.setColour(blob.colour);
        g.fillEllipse(blob.pos.x - blob.radius, blob.pos.y - blob.radius, blob.radius * 2, blob.radius * 2);
    }
}

void GalaxyStrobeTuneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto topBar = area.removeFromTop(100);
    
    profileSelector.setBounds(topBar.removeFromTop(35).reduced(50, 5));
    
    auto sliderArea = topBar;
    auto widthThird = sliderArea.getWidth() / 3;
    
    auto refArea = sliderArea.removeFromLeft(widthThird);
    refALabel.setBounds(refArea.removeFromTop(20));
    refASlider.setBounds(refArea.reduced(5, 0));
    
    auto capoArea = sliderArea.removeFromLeft(widthThird);
    capoLabel.setBounds(capoArea.removeFromTop(20));
    capoSlider.setBounds(capoArea.reduced(5, 0));
    
    smoothingLabel.setBounds(sliderArea.removeFromTop(20));
    smoothingSlider.setBounds(sliderArea.reduced(5, 0));
    
    auto bottomArea = area.removeFromBottom(160);
    captureButton.setBounds(bottomArea.removeFromBottom(35).reduced(120, 0));
    noteLabel.setBounds(bottomArea.removeFromTop(90));
    centsLabel.setBounds(bottomArea);
    
    strobeView.setBounds(area.reduced(10));
}

void GalaxyStrobeTuneAudioProcessorEditor::timerCallback()
{
    for (auto& blob : blobs) blob.update(getWidth(), getHeight());

    float alpha = audioProcessor.apvts.getRawParameterValue("smoothing")->load();
    float freq = audioProcessor.getCurrentFrequency();
    int capo = (int)audioProcessor.apvts.getRawParameterValue("capo")->load();

    if (freq > 20.0f)
    {
        float midi = 12.0f * std::log2(freq / (audioProcessor.apvts.getRawParameterValue("refA")->load())) + 69.0f;
        midi -= capo;
        float rounded = std::round(midi);
        int noteIndex = (int)std::fmod(rounded + 120, 12);
        float cents = (midi - rounded) * 100.0f;
        int profileIdx = (int)audioProcessor.apvts.getRawParameterValue("profile")->load();
        float offset = audioProcessor.getProfiles()[profileIdx].offsets[noteIndex];
        float targetsCents = cents - offset;

        smoothedCents = alpha * smoothedCents + (1.0f - alpha) * targetsCents;
        juce::String notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        noteLabel.setText(notes[noteIndex], juce::dontSendNotification);
        centsLabel.setText((smoothedCents >= 0 ? "+" : "") + juce::String(smoothedCents, 1) + " cents", juce::dontSendNotification);
        strobeView.setActive(true);
        strobeView.setDeviation(smoothedCents);
    }
    else
    {
        noteLabel.setText("---", juce::dontSendNotification);
        centsLabel.setText("READY", juce::dontSendNotification);
        strobeView.setActive(false);
        smoothedCents = 0.0f;
        if (captureButton.getButtonText().startsWith("SAVED")) captureButton.setButtonText("CAPTURE OFFSET");
    }
    
    strobeView.updateAnimation();
    repaint();
}
