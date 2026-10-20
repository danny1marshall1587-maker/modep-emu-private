#include "PluginEditor.h"
#include <cmath>

void EuphoriaHardwareLookAndFeel::drawRotarySlider(
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
        g.setColour((themeId == 1) ? juce::Colour(0xfff59e0b) : juce::Colour(0xff34d399));
        g.drawLine(centreX + std::sin(ta) * r1, centreY - std::cos(ta) * r1,
                   centreX + std::sin(ta) * r2, centreY - std::cos(ta) * r2, 1.6f);
    }

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillEllipse(rx + 2.0f, ry + 4.0f, rw, rw);

    if (themeId == 0) {
        // Obsidian Jade Skirted Knob with Emerald Center
        juce::ColourGradient skirtGrad(juce::Colour(0xff064e3b), centreX - radius * 0.4f, centreY - radius * 0.4f,
                                       juce::Colour(0xff021b14), centreX + radius, centreY + radius, true);
        g.setGradientFill(skirtGrad);
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(juce::Colour(0xff34d399));
        g.drawEllipse(rx, ry, rw, rw, 1.5f);

        // Emerald Center Cap
        const float capR = radius * 0.58f;
        juce::ColourGradient capGrad(juce::Colour(0xff10b981), centreX - capR * 0.5f, centreY - capR * 0.5f,
                                     juce::Colour(0xff065f46), centreX + capR, centreY + capR, true);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

        // Gold pointer line
        juce::Path p;
        p.addRoundedRectangle(-2.0f, -radius + 2.0f, 4.0f, radius * 0.52f, 1.5f);
        g.setColour(juce::Colour(0xfffbbf24));
        g.fillPath(p, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    } else if (themeId == 1) {
        // Champagne Gold / Amber Chicken-Head Knob
        const float baseR = radius * 0.72f;
        g.setColour(juce::Colour(0xff042f2e));
        g.fillEllipse(centreX - baseR, centreY - baseR, baseR * 2.0f, baseR * 2.0f);
        g.setColour(juce::Colour(0xfffbbf24));
        g.drawEllipse(centreX - baseR, centreY - baseR, baseR * 2.0f, baseR * 2.0f, 1.5f);

        juce::Path beak;
        beak.startNewSubPath(0.0f, -radius - 2.0f);
        beak.lineTo(8.0f, baseR * 0.55f);
        beak.lineTo(-8.0f, baseR * 0.55f);
        beak.closeSubPath();
        g.setColour(juce::Colour(0xffb45309));
        g.fillPath(beak, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour(0xfffef3c7));
        g.drawLine(centreX, centreY,
                   centreX + std::sin(angle) * (radius - 2.0f),
                   centreY - std::cos(angle) * (radius - 2.0f), 2.5f);
    } else {
        // Studio Rack Emerald / Cyan Pointer Knob
        juce::Colour accent = (themeId == 2) ? juce::Colour(0xff34d399) : juce::Colour(0xff06b6d4);
        juce::ColourGradient bodyGrad(juce::Colour(0xff064e3b), centreX - radius * 0.4f, centreY - radius * 0.4f,
                                      juce::Colour(0xff021b14), centreX + radius, centreY + radius, true);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(rx, ry, rw, rw);
        g.setColour(accent);
        g.drawEllipse(rx, ry, rw, rw, 2.0f);

        juce::Path p;
        p.addRoundedRectangle(-2.2f, -radius + 3.0f, 4.4f, radius * 0.62f, 1.8f);
        g.setColour(juce::Colour(0xfffbbf24));
        g.fillPath(p, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    }
}

namespace {
const char* const kPageNames[7] = {
    "1. SUPREME GLASS CLEAN", "2. '59/'68 CRUNCH", "3. BE-100 LEAD", "4. PEDALBOARD",
    "5. FLUID SPACE", "6. REVERB & TREMOLO", "7. TAPE DELAY" };
const int kPageCols[7] = { 6, 5, 5, 6, 4, 6, 4 };
}

EuphoriaAmpAudioProcessorEditor::EuphoriaAmpAudioProcessorEditor(EuphoriaAmpAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    setLookAndFeel(&hwLookAndFeel);

    for (int i = 0; i < 7; ++i) {
        auto b = std::make_unique<juce::TextButton>(kPageNames[i]);
        b->onClick = [this, i] { setActivePage(i); };
        addAndMakeVisible(*b);
        tabs.push_back(std::move(b));
    }

    const auto& table = euphoriaParamTable();
    controls.reserve(table.size());
    for (size_t i = 0; i < table.size(); ++i) {
        const auto& d = table[i];
        Control c;
        c.paramIndex = (int)i;
        c.page = d.page;

        c.label = std::make_unique<juce::Label>();
        c.label->setText(d.label, juce::dontSendNotification);
        c.label->setJustificationType(juce::Justification::centred);
        c.label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        c.label->setColour(juce::Label::textColourId, juce::Colour(0xff6ee7b7));
        addChildComponent(*c.label);

        if (!d.choices.empty()) {
            c.combo = std::make_unique<juce::ComboBox>();
            int id = 1;
            for (auto* ch : d.choices) c.combo->addItem(ch, id++);
            addChildComponent(*c.combo);
            c.comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                audioProcessor.apvts, d.id, *c.combo);
        } else {
            c.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
            c.slider->setDoubleClickReturnValue(true, d.defVal);
            c.slider->getProperties().set("themeId", d.page == 3 ? 1 : (d.page == 1 || d.page == 2) ? 1 : (d.page >= 4 ? 2 : 0));
            addChildComponent(*c.slider);
            c.sliderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, d.id, *c.slider);
        }
        controls.push_back(std::move(c));
    }

    setResizable(true, true);
    setResizeLimits(900, 560, 1800, 1100);
    setSize(1180, 720);
    setActivePage(0);
}

EuphoriaAmpAudioProcessorEditor::~EuphoriaAmpAudioProcessorEditor() {
    setLookAndFeel(nullptr);
}

void EuphoriaAmpAudioProcessorEditor::setActivePage(int pageIdx) {
    currentPage = juce::jlimit(0, 6, pageIdx);
    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i]->setColour(juce::TextButton::buttonColourId,
                           (int)i == currentPage ? juce::Colour(0xff059669) : juce::Colour(0xff0f2a24));
    for (auto& c : controls) {
        const bool vis = (c.page == -1) || (c.page == currentPage);
        c.label->setVisible(vis);
        if (c.slider) c.slider->setVisible(vis);
        if (c.combo)  c.combo->setVisible(vis);
    }
    resized();
    repaint();
}

void EuphoriaAmpAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff06140f));
    g.setColour(juce::Colour(0xff34d399));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("EUPHORIA AMP", 12, getHeight() - 28, 300, 24, juce::Justification::centredLeft);
}

void EuphoriaAmpAudioProcessorEditor::layoutPage(juce::Rectangle<int> area) {
    std::vector<Control*> pageCtl;
    for (auto& c : controls) if (c.page == currentPage) pageCtl.push_back(&c);
    if (pageCtl.empty()) return;

    const int cols = kPageCols[currentPage];
    const int rows = ((int)pageCtl.size() + cols - 1) / cols;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / rows;

    for (size_t i = 0; i < pageCtl.size(); ++i) {
        auto cell = juce::Rectangle<int>(area.getX() + ((int)i % cols) * cellW,
                                         area.getY() + ((int)i / cols) * cellH, cellW, cellH).reduced(6);
        auto* c = pageCtl[i];
        c->label->setBounds(cell.removeFromTop(18));
        if (c->slider) {
            c->slider->setBounds(cell);
        } else {
            c->combo->setBounds(cell.withSizeKeepingCentre(juce::jmin(cell.getWidth(), 190), 28));
        }
    }
}

void EuphoriaAmpAudioProcessorEditor::resized() {
    auto r = getLocalBounds().reduced(8);

    // Tab row
    auto tabRow = r.removeFromTop(34);
    const int tabW = tabRow.getWidth() / (int)tabs.size();
    for (auto& t : tabs) t->setBounds(tabRow.removeFromLeft(tabW).reduced(2));

    // Global header row (routing / oversample / speaker / cab size / tuning / pickup)
    auto header = r.removeFromTop(64);
    std::vector<Control*> hdr;
    for (auto& c : controls) if (c.page == -1) hdr.push_back(&c);
    if (!hdr.empty()) {
        const int hw = header.getWidth() / (int)hdr.size();
        for (auto* c : hdr) {
            auto cell = header.removeFromLeft(hw).reduced(4);
            c->label->setBounds(cell.removeFromTop(18));
            c->combo->setBounds(cell.removeFromTop(28));
        }
    }

    r.removeFromBottom(30);
    layoutPage(r);
}
