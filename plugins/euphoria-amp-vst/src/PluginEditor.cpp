#include "PluginEditor.h"
#include "BinaryData.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
static inline juce::Colour RGBCol(int r, int g, int b) noexcept {
    return juce::Colour((juce::uint8)std::clamp(r, 0, 255),
                        (juce::uint8)std::clamp(g, 0, 255),
                        (juce::uint8)std::clamp(b, 0, 255));
}

static void fillRoundRectJuce(juce::Graphics& g, int x1, int y1, int x2, int y2, int r,
                              juce::Colour fillCol, juce::Colour borderCol, int borderW = 2) {
    const float fx = (float)x1;
    const float fy = (float)y1;
    const float fw = (float)(x2 - x1);
    const float fh = (float)(y2 - y1);
    const float cr = (float)r * 0.5f;
    g.setColour(fillCol);
    g.fillRoundedRectangle(fx, fy, fw, fh, cr);
    if (borderW > 0) {
        g.setColour(borderCol);
        g.drawRoundedRectangle(fx, fy, fw, fh, cr, (float)borderW);
    }
}

static const juce::Rectangle<int> kTabRects[6] = {
    {  18, 12, 124, 36 }, // 1. AMP [AUTO-FLIP]
    { 148, 12, 106, 36 }, // 2. DRIVE
    { 260, 12, 106, 36 }, // 3. SPACE
    { 372, 12, 106, 36 }, // 4. REVERB
    { 484, 12, 106, 36 }, // 5. DELAY
    { 596, 12, 102, 36 }  // 6. TUNER
};

static const juce::Rectangle<int> kPresetPrevRect    { 704, 12,  22, 36 };
static const juce::Rectangle<int> kPresetNameRect    { 730, 12,  96, 36 };
static const juce::Rectangle<int> kPresetNextRect    { 830, 12,  22, 36 };
static const juce::Rectangle<int> kPresetSaveRect    { 856, 12,  48, 36 };
static const juce::Rectangle<int> kLearnNoiseBtnRect { 910, 12,  94, 36 };
static const juce::Rectangle<int> kRecSampleBtnRect  { 1010, 12, 78, 36 };
static const juce::Rectangle<int> kRiffBtnRect       { 1094, 12, 74, 36 };
static const juce::Rectangle<int> kAudioSettingsRect { 1174, 12, 90, 36 };

static const juce::Rectangle<int> kFuzzMorphMeterRect    { 1076 - 65, 366, 130, 14 };
static const juce::Rectangle<int> kBtnFuzzLearnCleanRect { 1076 - 65, 386,  62, 20 };
static const juce::Rectangle<int> kBtnFuzzLearnMaxRect   { 1076 +  3, 386,  62, 20 };

static const juce::Rectangle<int> kBtnTunerMuteRect  { 1022, 140, 208, 95 };
static const juce::Rectangle<int> kBtnTunerRefRect   { 1022, 245, 208, 80 };
static const juce::Rectangle<int> kBtnTunerSpeedRect { 1022, 335, 208, 80 };
static const juce::Rectangle<int> kTunerStatusPill   {  745, 686, 150, 24 };

static const juce::Rectangle<int> kMasterVolMinusRect { 1118, 686, 20, 24 };
static const juce::Rectangle<int> kMasterVolPillRect  { 1141, 686, 76, 24 };
static const juce::Rectangle<int> kMasterVolPlusRect  { 1220, 686, 20, 24 };

static void draw3DStompFootswitch(juce::Graphics& g, int cx, int cy, bool active, juce::Colour activeHaloCol) {
    // 0. Soft drop shadow on pedal enclosure
    g.setColour(RGBCol(8, 10, 14).withAlpha(0.7f));
    g.fillEllipse((float)(cx - 24), (float)(cy - 21), 48.0f, 48.0f);

    // 1. Halo Glow Ring when active
    if (active) {
        g.setColour(activeHaloCol.withAlpha(0.35f));
        g.drawEllipse((float)(cx - 26), (float)(cy - 26), 52.0f, 52.0f, 3.5f);
        g.setColour(activeHaloCol.withAlpha(0.65f));
        g.drawEllipse((float)(cx - 23), (float)(cy - 23), 46.0f, 46.0f, 1.5f);
    }

    // 2. Beveled Metallic Hex Nut / Collar (42x42px)
    fillRoundRectJuce(g, cx - 21, cy - 21, cx + 21, cy + 21, 10,
                      RGBCol(130, 140, 150), RGBCol(210, 220, 230), 1);
    fillRoundRectJuce(g, cx - 18, cy - 18, cx + 18, cy + 18, 8,
                      RGBCol(85, 95, 105), RGBCol(160, 170, 180), 1);

    // 3. Knurled Threaded Base Washer (30x30px)
    fillRoundRectJuce(g, cx - 15, cy - 15, cx + 15, cy + 15, 15,
                      RGBCol(45, 50, 60), RGBCol(100, 110, 125), 1);

    // 4. Chrome Dome Plunger Button (24x24px)
    juce::ColourGradient gradPlunger(
        RGBCol(245, 250, 255), (float)cx, (float)(cy - 12),
        RGBCol(110, 120, 135), (float)cx, (float)(cy + 12),
        false
    );
    g.setGradientFill(gradPlunger);
    g.fillEllipse((float)(cx - 12), (float)(cy - 12), 24.0f, 24.0f);
    g.setColour(RGBCol(255, 255, 255));
    g.drawEllipse((float)(cx - 12), (float)(cy - 12), 24.0f, 24.0f, 1.0f);

    // Specular highlight crescent
    g.setColour(RGBCol(255, 255, 255).withAlpha(0.85f));
    g.drawEllipse((float)(cx - 9), (float)(cy - 9), 12.0f, 8.0f, 1.5f);
}
} // namespace

EuphoriaAmpAudioProcessorEditor::EuphoriaAmpAudioProcessorEditor(EuphoriaAmpAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    imgWoodFloor         = juce::ImageCache::getFromMemory(BinaryData::wood_floor_tile_png, BinaryData::wood_floor_tile_pngSize);
    imgChassisWide       = juce::ImageCache::getFromMemory(BinaryData::chassis_wide_png, BinaryData::chassis_wide_pngSize);
    imgChassisCleanBlue  = juce::ImageCache::getFromMemory(BinaryData::chassis_clean_blue_png, BinaryData::chassis_clean_blue_pngSize);
    imgChassisCrunchBrown= juce::ImageCache::getFromMemory(BinaryData::chassis_crunch_brown_png, BinaryData::chassis_crunch_brown_pngSize);
    imgChassisLeadRed    = juce::ImageCache::getFromMemory(BinaryData::chassis_lead_red_png, BinaryData::chassis_lead_red_pngSize);
    imgRackSpaceTopaz    = juce::ImageCache::getFromMemory(BinaryData::rack_space_topaz_png, BinaryData::rack_space_topaz_pngSize);
    imgRackReverbPink    = juce::ImageCache::getFromMemory(BinaryData::rack_reverb_pink_png, BinaryData::rack_reverb_pink_pngSize);
    imgRackTremOrange    = juce::ImageCache::getFromMemory(BinaryData::rack_trem_orange_png, BinaryData::rack_trem_orange_pngSize);
    imgRackDelayGreen    = juce::ImageCache::getFromMemory(BinaryData::rack_delay_green_png, BinaryData::rack_delay_green_pngSize);
    imgKlon              = juce::ImageCache::getFromMemory(BinaryData::pedal_klon_gold_png, BinaryData::pedal_klon_gold_pngSize);
    imgTs9               = juce::ImageCache::getFromMemory(BinaryData::pedal_ts9_green_png, BinaryData::pedal_ts9_green_pngSize);
    imgBlues             = juce::ImageCache::getFromMemory(BinaryData::pedal_blues_blue_png, BinaryData::pedal_blues_blue_pngSize);
    imgRat               = juce::ImageCache::getFromMemory(BinaryData::pedal_rat_black_png, BinaryData::pedal_rat_black_pngSize);
    imgFuzz              = juce::ImageCache::getFromMemory(BinaryData::pedal_fuzz_round_png, BinaryData::pedal_fuzz_round_pngSize);
    imgKnobStrip         = juce::ImageCache::getFromMemory(BinaryData::knob_filmstrip_png, BinaryData::knob_filmstrip_pngSize);

    initKnobLayout();

    setResizable(true, true);
    setResizeLimits(960, 540, 2560, 1440);
    getConstrainer()->setFixedAspectRatio(1280.0 / 720.0);
    setSize(1280, 720);

    startTimerHz(30);
}

EuphoriaAmpAudioProcessorEditor::~EuphoriaAmpAudioProcessorEditor() {
    stopTimer();
}

void EuphoriaAmpAudioProcessorEditor::timerCallback() {
    animPhase += 0.10f;
    if (animPhase > 6.2831853f) animPhase -= 6.2831853f;
    strobeWheelPhase += processor.tuner.strobeSpeed.load() * 0.40f;
    if (strobeWheelPhase > 1000.0f) strobeWheelPhase -= 1000.0f;
    else if (strobeWheelPhase < -1000.0f) strobeWheelPhase += 1000.0f;

    if (processor.midiMap.checkAndResetNeedsSave()) {
        processor.midiMap.saveToFile(processor.getMidiConfigFile().getFullPathName().toStdString());
    }

    repaint();
}

void EuphoriaAmpAudioProcessorEditor::resized() {}

juce::Point<int> EuphoriaAmpAudioProcessorEditor::toVirtualPt(juce::Point<float> localPt) const noexcept {
    const float sx = 1280.0f / std::max(1.0f, (float)getWidth());
    const float sy = 720.0f  / std::max(1.0f, (float)getHeight());
    return { (int)std::lround(localPt.x * sx), (int)std::lround(localPt.y * sy) };
}

void EuphoriaAmpAudioProcessorEditor::initKnobLayout() {
    const auto& table = euphoriaParamTable();
    knobLayout.resize(table.size());
    for (size_t i = 0; i < table.size(); ++i) {
        knobLayout[i].pageIdx   = table[i].page;
        knobLayout[i].isStepped = !table[i].choices.empty();
        knobLayout[i].steps     = (int)table[i].choices.size();
        knobLayout[i].rect      = { 0, 0, 0, 0 };
    }

    // Global Header Selector Pills (indices 0..5)
    knobLayout[K_CH_MODE].rect      = {  18, 56, 138, 36 };
    knobLayout[K_QUALITY].rect      = { 160, 56, 114, 36 };
    knobLayout[K_CAB_SIM].rect      = { 278, 56, 164, 36 };
    knobLayout[K_CAB_SIZE].rect     = { 446, 56, 116, 36 };
    knobLayout[K_TUNING_REF].rect   = { 566, 56,  98, 36 };
    knobLayout[K_PICKUP_VOICE].rect = { 668, 56, 114, 36 };

    for (int page = 0; page < 7; ++page) {
        if (page == 3) {
            auto setR = [this](int idx, int l, int t, int r, int b) {
                knobLayout[(size_t)idx].rect = { l, t, r - l, b - t };
            };
            // Pedal 1: Fluid Centaur (cx = 204)
            setR(K_KLON_GAIN,   152 - 22, 206, 152 + 22, 292);
            setR(K_KLON_TREBLE, 204 - 22, 206, 204 + 22, 292);
            setR(K_KLON_OUTPUT, 256 - 22, 206, 256 + 22, 292);
            setR(K_KLON_BLEND,  160 - 24, 314, 160 + 24, 386);
            setR(K_KLON_HYPER,  248 - 28, 314, 248 + 28, 362);
            setR(K_KLON_ON,     0, 0, 0, 0);

            // Pedal 2: TS808 Screamer (cx = 422)
            setR(K_TS_DRIVE, 370 - 22, 206, 370 + 22, 292);
            setR(K_TS_TONE,  422 - 22, 206, 422 + 22, 292);
            setR(K_TS_LEVEL, 474 - 22, 206, 474 + 22, 292);
            setR(K_TS_MOD,   422 - 38 - 28, 314, 422 - 38 + 28, 362);
            setR(K_TS_HYPER, 422 + 38 - 28, 314, 422 + 38 + 28, 362);
            setR(K_TS_ON,    0, 0, 0, 0);

            // Pedal 3: BD-2 Blues Driver (cx = 640)
            setR(K_BD_GAIN,  588 - 22, 206, 588 + 22, 292);
            setR(K_BD_TONE,  640 - 22, 206, 640 + 22, 292);
            setR(K_BD_LEVEL, 692 - 22, 206, 692 + 22, 292);
            setR(K_BD_PHAT,  640 - 38 - 28, 314, 640 - 38 + 28, 362);
            setR(K_BD_HYPER, 640 + 38 - 28, 314, 640 + 38 + 28, 362);
            setR(K_BD_ON,    0, 0, 0, 0);

            // Pedal 4: Fluid Rodent (cx = 858)
            setR(K_RAT_DIST,   806 - 22, 206, 806 + 22, 292);
            setR(K_RAT_FILTER, 858 - 22, 206, 858 + 22, 292);
            setR(K_RAT_VOL,    910 - 22, 206, 910 + 22, 292);
            setR(K_RAT_TURBO,  858 - 38 - 28, 314, 858 - 38 + 28, 362);
            setR(K_RAT_HYPER,  858 + 38 - 28, 314, 858 + 38 + 28, 362);
            setR(K_RAT_ON,     0, 0, 0, 0);

            // Pedal 5: Fluid Fuzz Face (cx = 1076, Rectangular Stompbox Chassis)
            setR(K_FUZZ_GAIN,       1024 - 22, 206, 1024 + 22, 292);
            setR(K_FUZZ_SAG,        1076 - 22, 206, 1076 + 22, 292);
            setR(K_FUZZ_LEVEL,      1128 - 22, 206, 1128 + 22, 292);
            setR(K_FUZZ_TRANSISTOR, 1076 - 38 - 28, 314, 1076 - 38 + 28, 362);
            setR(K_FUZZ_HYPER,      1076 + 38 - 28, 314, 1076 + 38 + 28, 362);
            setR(K_FUZZ_ON,         0, 0, 0, 0);
            continue;
        }

        if (page == 5) {
            int revLeft = 148, revRight = 660;
            int revSlotW = (revRight - revLeft) / 7;
            for (int k = 0; k < 7; ++k) {
                int cx = revLeft + k * revSlotW + revSlotW / 2;
                knobLayout[(size_t)(K_REV_ON + k)].rect = { cx - 34, 184, 68, 120 };
            }
            int tremLeft = 148, tremRight = 660;
            int tremSlotW = (tremRight - tremLeft) / 4;
            for (int k = 0; k < 4; ++k) {
                int cx = tremLeft + k * tremSlotW + tremSlotW / 2;
                knobLayout[(size_t)(K_VIB_ON + k)].rect = { cx - 44, 474, 88, 130 };
            }
            continue;
        }

        if (page == 4 || page == 6) {
            std::vector<int> indices;
            for (int i = 0; i < (int)table.size(); ++i)
                if (table[(size_t)i].page == page) indices.push_back(i);
            int n = (int)indices.size();
            int slotW = 984 / n;
            for (int k = 0; k < n; ++k) {
                int cx = 148 + k * slotW + slotW / 2;
                knobLayout[(size_t)indices[(size_t)k]].rect = { cx - 42, 204, 84, 120 };
            }
            continue;
        }

        std::vector<int> indices;
        for (int i = 0; i < (int)table.size(); ++i)
            if (table[(size_t)i].page == page) indices.push_back(i);
        int faceLeft = 160, faceSpan = 960, halfW = 44;
        if (page == 0) {
            int nRow1 = 6, nRow2 = 5;
            int slotW1 = faceSpan / nRow1;
            int slotW2 = faceSpan / nRow2;
            for (int k = 0; k < nRow1; ++k) {
                int cx = faceLeft + k * slotW1 + slotW1 / 2;
                knobLayout[(size_t)indices[(size_t)k]].rect = { cx - halfW, 175, halfW * 2, 120 };
            }
            for (int k = 0; k < nRow2; ++k) {
                int cx = faceLeft + k * slotW2 + slotW2 / 2;
                knobLayout[(size_t)indices[(size_t)(nRow1 + k)]].rect = { cx - halfW, 305, halfW * 2, 120 };
            }
        } else if (page == 1 || page == 2) {
            int nPerRow = 5;
            int slotW = faceSpan / nPerRow;
            for (int k = 0; k < nPerRow; ++k) {
                int cx = faceLeft + k * slotW + slotW / 2;
                knobLayout[(size_t)indices[(size_t)k]].rect = { cx - halfW, 175, halfW * 2, 120 };
            }
            for (int k = 0; k < nPerRow; ++k) {
                int cx = faceLeft + k * slotW + slotW / 2;
                knobLayout[(size_t)indices[(size_t)(nPerRow + k)]].rect = { cx - halfW, 305, halfW * 2, 120 };
            }
        }
    }
}

void EuphoriaAmpAudioProcessorEditor::drawCornerProtector3D(juce::Graphics& g, int x, int y, int w, int h,
                                                            int cornerType, juce::Colour metalCol, juce::Colour hiCol) {
    fillRoundRectJuce(g, x, y, x + w, y + h, 14, metalCol, hiCol, 2);
    int rx = (cornerType == 0 || cornerType == 2) ? (x + 9) : (x + w - 9);
    int ry = (cornerType == 0 || cornerType == 1) ? (y + 9) : (y + h - 9);
    fillRoundRectJuce(g, rx - 3, ry - 3, rx + 3, ry + 3, 6, RGBCol(30, 41, 59), RGBCol(226, 232, 240), 1);
}

void EuphoriaAmpAudioProcessorEditor::drawFacetedJewelLamp(juce::Graphics& g, int cx, int cy, int r,
                                                           juce::Colour jewelBaseCol, juce::Colour glowCol, juce::Colour rimCol) {
    for (int gr = r + 8; gr >= r; gr -= 2) {
        g.setColour(glowCol.withAlpha(0.28f));
        g.fillEllipse((float)(cx - gr), (float)(cy - gr), (float)(gr * 2), (float)(gr * 2));
    }
    fillRoundRectJuce(g, cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4, (r + 4) * 2,
                      RGBCol(203, 213, 225), RGBCol(71, 85, 105), 2);
    for (int k = 0; k < 8; ++k) {
        float a = k * 0.785398f;
        int kx = cx + (int)(std::cos(a) * (r + 3));
        int ky = cy + (int)(std::sin(a) * (r + 3));
        fillRoundRectJuce(g, kx - 1, ky - 1, kx + 1, ky + 1, 2, RGBCol(51, 65, 85), RGBCol(51, 65, 85), 1);
    }
    fillRoundRectJuce(g, cx - r, cy - r, cx + r, cy + r, r * 2, jewelBaseCol, rimCol, 1);
    for (int f = 0; f < 8; ++f) {
        float a1 = f * 0.785398f;
        float a2 = (f + 1) * 0.785398f;
        float x1 = (float)cx + std::cos(a1) * (float)(r - 2);
        float y1 = (float)cy + std::sin(a1) * (float)(r - 2);
        float x2 = (float)cx + std::cos(a2) * (float)(r - 2);
        float y2 = (float)cy + std::sin(a2) * (float)(r - 2);
        juce::Path tri;
        tri.addTriangle((float)cx, (float)cy, x1, y1, x2, y2);
        g.setColour((f % 2 == 0) ? jewelBaseCol : glowCol);
        g.fillPath(tri);
        g.setColour(rimCol);
        g.strokePath(tri, juce::PathStrokeType(1.0f));
    }
    fillRoundRectJuce(g, cx - r / 2 - 2, cy - r / 2 - 2, cx - r / 2 + 3, cy - r / 2 + 3, 6,
                      RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);
}

void EuphoriaAmpAudioProcessorEditor::drawBasketweaveGrille(juce::Graphics& g, int x1, int y1, int x2, int y2,
                                                            juce::Colour darkYarn, juce::Colour lightYarn) {
    fillRoundRectJuce(g, x1, y1, x2, y2, 8, darkYarn, RGBCol(15, 15, 15), 2);
    for (int y = y1 + 4; y < y2 - 4; y += 8) {
        for (int x = x1 + 4; x < x2 - 4; x += 16) {
            bool oddRow = (((y - y1) / 8) % 2 == 1);
            int blockX = oddRow ? (x + 8) : x;
            if (blockX + 8 < x2 - 4) {
                g.setColour(lightYarn);
                g.drawLine((float)blockX, (float)(y + 2), (float)(blockX + 8), (float)(y + 2), 2.0f);
                g.drawLine((float)blockX, (float)(y + 5), (float)(blockX + 8), (float)(y + 5), 2.0f);
                g.setColour(darkYarn);
                g.drawLine((float)(blockX + 8), (float)y, (float)(blockX + 8), (float)(y + 8), 2.0f);
            }
        }
    }
    fillRoundRectJuce(g, x1 + 2, y1 + 2, x2 - 2, y1 + 8, 4, RGBCol(8, 8, 8), RGBCol(8, 8, 8), 1);
}

static const char* shortDisplayLabel(int paramIdx, const char* rawLabel) {
    switch (paramIdx) {
        case K_KLON_GAIN:   return "GAIN";
        case K_KLON_TREBLE: return "TREBLE";
        case K_KLON_OUTPUT: return "OUTPUT";
        case K_KLON_BLEND:  return "CLEAN BLEND";
        case K_KLON_HYPER:  return "CIRCUIT";
        case K_TS_DRIVE:    return "OVERDRIVE";
        case K_TS_TONE:     return "TONE";
        case K_TS_LEVEL:    return "LEVEL";
        case K_TS_MOD:      return "VOICING";
        case K_TS_HYPER:    return "CIRCUIT";
        case K_BD_GAIN:     return "GAIN";
        case K_BD_TONE:     return "TONE";
        case K_BD_LEVEL:    return "LEVEL";
        case K_BD_PHAT:     return "VOICING";
        case K_BD_HYPER:    return "CIRCUIT";
        case K_RAT_DIST:    return "DISTORT";
        case K_RAT_FILTER:  return "FILTER";
        case K_RAT_VOL:     return "VOLUME";
        case K_RAT_TURBO:   return "CLIPPING";
        case K_RAT_HYPER:   return "CIRCUIT";
        case K_FUZZ_GAIN:       return "FUZZ";
        case K_FUZZ_SAG:        return "AMP SAG";
        case K_FUZZ_LEVEL:      return "LEVEL";
        case K_FUZZ_TRANSISTOR: return "TRANSISTOR";
        case K_FUZZ_HYPER:      return "CIRCUIT";
        default:            return rawLabel;
    }
}

void EuphoriaAmpAudioProcessorEditor::drawStylizedKnob(juce::Graphics& g, int paramIdx, int pageStyle) {
    const auto& table = euphoriaParamTable();
    const auto& d = table[(size_t)paramIdx];
    const auto& k = knobLayout[(size_t)paramIdx];
    if (k.rect.getWidth() <= 0 || k.rect.getHeight() <= 0) return;

    const float curVal = processor.getParamValue(paramIdx);
    const int cx = k.rect.getCentreX();
    const int topY = k.rect.getY();
    const char* dispLabel = shortDisplayLabel(paramIdx, d.label);

    if (pageStyle == 3 && k.isStepped) {
        int step = std::max(0, std::min(k.steps - 1, (int)std::lround(curVal)));
        bool active = (step > 0);

        juce::Colour labelCol = (cx < 320) ? RGBCol(254, 240, 138)
                              : ((cx < 514) ? RGBCol(167, 243, 208)
                              : ((cx < 752) ? RGBCol(254, 240, 138)
                              : ((cx < 992) ? RGBCol(254, 240, 138)
                              : RGBCol(254, 202, 202))));
        juce::Colour ledCol   = (cx < 320) ? (active ? RGBCol(239, 68, 68) : RGBCol(60, 15, 15))
                              : ((cx < 514) ? (active ? RGBCol(34, 197, 94) : RGBCol(20, 60, 35))
                              : ((cx < 752) ? (active ? RGBCol(59, 130, 246) : RGBCol(20, 40, 80))
                              : ((cx < 992) ? (active ? RGBCol(239, 68, 68) : RGBCol(60, 12, 12))
                              : (active ? RGBCol(56, 189, 248) : RGBCol(20, 40, 60)))));
        juce::Colour ledGlow  = (cx < 320) ? (active ? RGBCol(254, 202, 202) : RGBCol(80, 25, 25))
                              : ((cx < 514) ? (active ? RGBCol(187, 247, 208) : RGBCol(40, 90, 55))
                              : ((cx < 752) ? (active ? RGBCol(191, 219, 254) : RGBCol(35, 65, 120))
                              : ((cx < 992) ? (active ? RGBCol(254, 202, 202) : RGBCol(80, 20, 20))
                              : (active ? RGBCol(186, 230, 253) : RGBCol(35, 70, 100)))));

        // 1. Compact Label above switch
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.setColour(labelCol);
        g.drawText(dispLabel, cx - 28, topY, 56, 12, juce::Justification::centred, false);

        // 2. Beveled Dark Machined Switch Faceplate (Half Size: 36px wide x 16px high)
        int swTop = topY + 13;
        int swBottom = topY + 29;
        int swHalfW = 18;
        fillRoundRectJuce(g, cx - swHalfW + 1, swTop + 1, cx + swHalfW + 1, swBottom + 1, 4, RGBCol(8, 10, 12), RGBCol(8, 10, 12), 1);
        fillRoundRectJuce(g, cx - swHalfW, swTop, cx + swHalfW, swBottom, 4, RGBCol(22, 25, 30), RGBCol(71, 85, 105), 1);

        // Micro-screws
        g.setColour(RGBCol(148, 163, 184));
        g.fillRect(cx - swHalfW + 2, swTop + 2, 2, 2);
        g.fillRect(cx + swHalfW - 4, swTop + 2, 2, 2);
        g.fillRect(cx - swHalfW + 2, swBottom - 4, 2, 2);
        g.fillRect(cx + swHalfW - 4, swBottom - 4, 2, 2);

        // Recessed slot
        int slotLeft = cx - 11, slotRight = cx + 11;
        int slotTop = swTop + 4, slotBottom = swBottom - 4;
        fillRoundRectJuce(g, slotLeft, slotTop, slotRight, slotBottom, 4, RGBCol(10, 12, 16), RGBCol(30, 41, 59), 1);

        // 3D Chrome Bat
        int batCenter = active ? (cx + 5) : (cx - 5);
        int batHalfW = 4;
        fillRoundRectJuce(g, batCenter - batHalfW, slotTop, batCenter + batHalfW, slotBottom, 3, RGBCol(226, 232, 240), RGBCol(255, 255, 255), 1);
        g.setColour(RGBCol(100, 116, 139));
        g.drawVerticalLine(batCenter, (float)(slotTop + 1), (float)(slotBottom - 1));

        // LED Indicator (3x3 dot)
        int ledX = active ? (cx + 13) : (cx - 13);
        int ledY = (swTop + swBottom) / 2;
        fillRoundRectJuce(g, ledX - 2, ledY - 2, ledX + 2, ledY + 2, 4, ledCol, ledGlow, 1);
        if (active)
            fillRoundRectJuce(g, ledX - 1, ledY - 1, ledX + 1, ledY + 1, 2, RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);

        // 3. Compact Choice Pill (54px wide x 13px high)
        int pillHalfW = 27;
        fillRoundRectJuce(g, cx - pillHalfW, topY + 31, cx + pillHalfW, topY + 44, 3,
                          RGBCol(4, 16, 20), active ? ledGlow : RGBCol(30, 41, 59), 1);
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.setColour(active ? labelCol : RGBCol(148, 163, 184));
        g.drawText(d.choices[(size_t)step], cx - pillHalfW, topY + 31, pillHalfW * 2, 13, juce::Justification::centred, false);
        return;
    }

    int r = 24;
    int cy = k.rect.getCentreY() - 16;
    int pillHalfW = 34;

    if (pageStyle == 0 || pageStyle == 1 || pageStyle == 2) {
        r = 23;
        cy = topY + 32;
        pillHalfW = 40;
    } else if (pageStyle == 3) {
        if (topY > 300) {
            r = 16; cy = topY + 22; pillHalfW = 26;
        } else {
            r = 16; cy = topY + 20; pillHalfW = 18;
        }
    } else if (pageStyle == 4 || pageStyle == 6) {
        r = 21; cy = topY + 32; pillHalfW = 38;
    } else if (pageStyle == 5) {
        if (topY < 350) {
            r = 18; cy = topY + 32; pillHalfW = 29;
        } else {
            r = 22; cy = topY + 38; pillHalfW = 34;
        }
    }

    float norm = (curVal - d.minVal) / std::max(0.0001f, d.maxVal - d.minVal);
    norm = std::clamp(norm, 0.0f, 1.0f);

    const float startAngle = -3.9269908f;
    const float totalSweep = 4.712389f;
    float angle = startAngle + norm * totalSweep;

    // 1. Cast drop shadow
    g.setColour(RGBCol(4, 12, 10).withAlpha(0.75f));
    g.fillEllipse((float)(cx - r + 2), (float)(cy - r + 3), (float)(r * 2 + 1), (float)(r * 2 + 1));

    // 2. Calibration Ticks
    int tickLen1 = r + ((r <= 17) ? 2 : 3);
    int tickLen2 = r + ((r <= 17) ? 4 : 7);
    float tickW  = (r <= 17) ? 1.2f : 2.0f;

    juce::Colour tickCol = (pageStyle == 0) ? RGBCol(245, 158, 11)
                         : (pageStyle == 1) ? RGBCol(254, 215, 170)
                         : (pageStyle == 2) ? RGBCol(248, 113, 113)
                         : (pageStyle == 3) ? ((cx < 274) ? RGBCol(251, 191, 36)
                                            : ((cx < 514) ? RGBCol(134, 239, 172)
                                            : ((cx < 752) ? RGBCol(147, 197, 253)
                                            : ((cx < 992) ? RGBCol(254, 240, 138)
                                            : RGBCol(252, 165, 165)))))
                         : (pageStyle == 4) ? RGBCol(34, 211, 238)
                         : (pageStyle == 5) ? RGBCol(244, 114, 182)
                                            : RGBCol(251, 191, 36);

    for (int t = 0; t <= 10; ++t) {
        float ta = startAngle + (t / 10.0f) * totalSweep;
        float tx1 = (float)cx + std::cos(ta) * (float)tickLen1;
        float ty1 = (float)cy + std::sin(ta) * (float)tickLen1;
        float tx2 = (float)cx + std::cos(ta) * (float)tickLen2;
        float ty2 = (float)cy + std::sin(ta) * (float)tickLen2;
        g.setColour(RGBCol(2, 10, 8));
        g.drawLine(tx1 + 1.0f, ty1 + 1.0f, tx2 + 1.0f, ty2 + 1.0f, tickW);
        g.setColour(tickCol);
        g.drawLine(tx1, ty1, tx2, ty2, tickW);
    }

    if (imgKnobStrip.isValid()) {
        int frameIdx = std::clamp((int)(norm * 63.0f), 0, 63);
        g.drawImage(imgKnobStrip, cx - r, cy - r, r * 2, r * 2, 0, frameIdx * 128, 128, 128);
    } else {
        fillRoundRectJuce(g, cx - r, cy - r, cx + r, cy + r, r * 2, RGBCol(30, 35, 42), tickCol, 2);
        int capR = r - 6;
        fillRoundRectJuce(g, cx - capR, cy - capR, cx + capR, cy + capR, capR * 2, RGBCol(75, 82, 92), RGBCol(180, 190, 200), 1);
        float px1 = (float)cx + std::cos(angle) * 4.0f;
        float py1 = (float)cy + std::sin(angle) * 4.0f;
        float px2 = (float)cx + std::cos(angle) * (float)(r - 2);
        float py2 = (float)cy + std::sin(angle) * (float)(r - 2);
        g.setColour(RGBCol(255, 255, 255));
        g.drawLine(px1, py1, px2, py2, 2.5f);
    }

    // 4. Label & Inset LED Value Pill
    int lblTop = cy + r + ((r <= 17) ? 3 : 6);
    int lblBot = lblTop + ((r <= 17) ? 12 : 16);
    int valTop = lblBot + 1;
    int valBot = valTop + ((r <= 17) ? 14 : 18);
    int lblHalfW = (r <= 17) ? 24 : 52;

    g.setFont(juce::FontOptions((r <= 17) ? 9.5f : 11.0f, juce::Font::bold));
    g.setColour(RGBCol(0, 0, 0));
    g.drawText(dispLabel, cx - lblHalfW + 1, lblTop + 1, lblHalfW * 2, lblBot - lblTop, juce::Justification::centred, false);
    g.setColour((pageStyle == 3 && cx >= 514 && cx < 752) ? RGBCol(254, 240, 138) : RGBCol(255, 255, 255));
    g.drawText(dispLabel, cx - lblHalfW, lblTop, lblHalfW * 2, lblBot - lblTop, juce::Justification::centred, false);

    fillRoundRectJuce(g, cx - pillHalfW, valTop, cx + pillHalfW, valBot, 4, RGBCol(2, 15, 12), RGBCol(20, 83, 70), 1);

    char valBuf[64];
    if (k.isStepped) {
        int idx = std::clamp((int)std::lround(curVal), 0, k.steps - 1);
        std::snprintf(valBuf, sizeof(valBuf), "%s", d.choices[(size_t)idx]);
    } else {
        std::snprintf(valBuf, sizeof(valBuf), "%.1f", curVal);
    }

    juce::Colour valCol = (pageStyle == 0) ? RGBCol(167, 243, 208)
                        : (pageStyle == 1) ? RGBCol(254, 215, 170)
                        : (pageStyle == 2) ? RGBCol(254, 202, 202)
                        : (pageStyle == 3) ? ((cx < 320) ? RGBCol(254, 240, 138)
                                           : ((cx < 514) ? RGBCol(167, 243, 208)
                                           : ((cx < 752) ? RGBCol(191, 219, 254)
                                           : ((cx < 992) ? RGBCol(254, 240, 138)
                                           : RGBCol(254, 202, 202)))))
                        : (pageStyle == 4) ? RGBCol(103, 232, 249)
                        : (pageStyle == 5) ? RGBCol(244, 114, 182)
                                           : RGBCol(253, 230, 138);
    g.setFont(juce::FontOptions((r <= 17 || k.isStepped) ? 9.5f : 11.0f, juce::Font::bold));
    g.setColour(valCol);
    g.drawText(valBuf, cx - pillHalfW, valTop, pillHalfW * 2, valBot - valTop, juce::Justification::centred, false);
}

void EuphoriaAmpAudioProcessorEditor::paint(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState state(g);
    const float sx = (float)getWidth() / 1280.0f;
    const float sy = (float)getHeight() / 720.0f;
    g.addTransform(juce::AffineTransform::scale(sx, sy));

    constexpr int w = 1280;
    constexpr int h = 720;

    // 0. Hardwood Floor Background & Top Header Bar
    g.fillAll(RGBCol(2, 8, 12));
    if (imgWoodFloor.isValid()) {
        g.setTiledImageFill(imgWoodFloor, 0, 50, 1.0f);
        g.fillRect(0, 50, w, h - 50);
    } else {
        g.setColour(RGBCol(42, 26, 14));
        g.fillRect(0, 50, w, h - 50);
    }
    g.setColour(RGBCol(2, 8, 12));
    g.fillRect(0, 0, w, 98);

    // 1. Top Navigation Tabs
    int curCh = std::clamp((int)std::lround(processor.getParamValue(K_CH_MODE)), 0, 3);
    juce::String ampTitle = (curCh == 0) ? "1. AMP [CLEAN]" :
                            (curCh == 1) ? "1. AMP [CRUNCH]" :
                            (curCh == 2) ? "1. AMP [LEAD]" : "1. AMP [BLEND]";

    juce::String tabTitles[6] = {
        ampTitle,
        "2. DRIVE",
        "3. SPACE",
        "4. REVERB",
        "5. DELAY",
        "6. TUNER"
    };
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    for (int t = 0; t < 6; ++t) {
        bool active = false;
        juce::Colour bg, br;
        if (t == 0) {
            active = (activePage < 3);
            int ampPg = (activePage < 3) ? activePage : ((curCh == 0) ? 0 : (curCh == 2 ? 2 : 1));
            bg = active ? (ampPg == 0 ? RGBCol(6, 78, 59)   :
                           ampPg == 1 ? RGBCol(90, 30, 10)  :
                                        RGBCol(90, 10, 20))
                        : RGBCol(4, 20, 24);
            br = active ? (ampPg == 0 ? RGBCol(245, 158, 11) :
                           ampPg == 1 ? RGBCol(249, 115, 22) :
                                        RGBCol(239, 68, 68))
                        : RGBCol(19, 78, 74);
        } else {
            int pageForTab = t + 2;
            active = (activePage == pageForTab);
            bg = active ? (t == 1 ? RGBCol(120, 80, 10) :
                           t == 2 ? RGBCol(8, 51, 68)   :
                           t == 3 ? RGBCol(68, 10, 40)  :
                           t == 4 ? RGBCol(30, 27, 75)  :
                                    RGBCol(12, 74, 110))
                        : RGBCol(4, 20, 24);
            br = active ? (t == 1 ? RGBCol(251, 191, 36) :
                           t == 2 ? RGBCol(6, 182, 212)  :
                           t == 3 ? RGBCol(244, 114, 182) :
                           t == 4 ? RGBCol(251, 191, 36) :
                                    RGBCol(56, 189, 248))
                        : RGBCol(19, 78, 74);
        }
        const auto& tr = kTabRects[t];
        fillRoundRectJuce(g, tr.getX(), tr.getY(), tr.getRight(), tr.getBottom(), 8, bg, br, active ? 2 : 1);
        g.setColour(active ? RGBCol(254, 240, 138) : RGBCol(148, 163, 184));
        g.drawText(tabTitles[t], tr, juce::Justification::centred, false);
    }

    // Preset Controls
    fillRoundRectJuce(g, kPresetPrevRect.getX(), kPresetPrevRect.getY(), kPresetPrevRect.getRight(), kPresetPrevRect.getBottom(), 6,
                      RGBCol(15, 23, 42), RGBCol(245, 158, 11), 1);
    g.setColour(RGBCol(254, 240, 138));
    g.drawText("<", kPresetPrevRect, juce::Justification::centred, false);

    fillRoundRectJuce(g, kPresetNameRect.getX(), kPresetNameRect.getY(), kPresetNameRect.getRight(), kPresetNameRect.getBottom(), 6,
                      RGBCol(6, 34, 28), RGBCol(20, 184, 166), 1);
    juce::String pName = (!processor.presets.empty() && processor.currentPresetIdx >= 0 && processor.currentPresetIdx < (int)processor.presets.size())
                         ? processor.presets[(size_t)processor.currentPresetIdx].name : "default";
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(RGBCol(254, 240, 138));
    g.drawText("PRESET: " + pName, kPresetNameRect, juce::Justification::centred, true);

    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    fillRoundRectJuce(g, kPresetNextRect.getX(), kPresetNextRect.getY(), kPresetNextRect.getRight(), kPresetNextRect.getBottom(), 6,
                      RGBCol(15, 23, 42), RGBCol(245, 158, 11), 1);
    g.setColour(RGBCol(254, 240, 138));
    g.drawText(">", kPresetNextRect, juce::Justification::centred, false);

    fillRoundRectJuce(g, kPresetSaveRect.getX(), kPresetSaveRect.getY(), kPresetSaveRect.getRight(), kPresetSaveRect.getBottom(), 6,
                      RGBCol(245, 158, 11), RGBCol(254, 240, 138), 2);
    g.setColour(RGBCol(30, 20, 0));
    g.drawText("SAVE", kPresetSaveRect, juce::Justification::centred, false);

    // Synchronized Learn Noise Button (Rule 19 timeout safeguard)
    auto* eng = processor.getEngine();
    bool isLearning = eng ? eng->isLearningNoise() : false;
    uint32_t now = juce::Time::getMillisecondCounter();
    if (isLearning && (now - learnNoiseStartTime > 1400)) {
        if (eng) {
            eng->humKiller.triggerLearn(false);
            eng->cyberDenoiser.cancelLearn();
        }
        isLearning = false;
    }
    if (wasLearningNoise && !isLearning) learnNoiseDoneTime = now;
    wasLearningNoise = isLearning;
    bool justFinished = (!isLearning && (now - learnNoiseDoneTime < 2200) && learnNoiseDoneTime > 0);

    juce::Colour lnBg = isLearning ? RGBCol(180, 83, 9) : (justFinished ? RGBCol(6, 95, 70) : RGBCol(13, 48, 44));
    juce::Colour lnBr = isLearning ? RGBCol(251, 191, 36) : (justFinished ? RGBCol(52, 211, 153) : RGBCol(20, 184, 166));
    fillRoundRectJuce(g, kLearnNoiseBtnRect.getX(), kLearnNoiseBtnRect.getY(),
                      kLearnNoiseBtnRect.getRight(), kLearnNoiseBtnRect.getBottom(), 6, lnBg, lnBr, 2);
    if (isLearning && eng) {
        int pct = std::clamp((int)(eng->getLearnNoiseProgress() * 100.0f), 1, 99);
        g.setColour(RGBCol(254, 240, 138));
        g.drawText("LEARN " + juce::String(pct) + "%", kLearnNoiseBtnRect, juce::Justification::centred, false);
    } else if (justFinished) {
        g.setColour(RGBCol(167, 243, 208));
        g.drawText("PROFILED OK", kLearnNoiseBtnRect, juce::Justification::centred, false);
    } else {
        g.setColour(RGBCol(240, 253, 250));
        g.drawText("LEARN NOISE", kLearnNoiseBtnRect, juce::Justification::centred, false);
    }

    // REC 6s / Status Badge
    fillRoundRectJuce(g, kRecSampleBtnRect.getX(), kRecSampleBtnRect.getY(),
                      kRecSampleBtnRect.getRight(), kRecSampleBtnRect.getBottom(), 6,
                      RGBCol(45, 10, 15), RGBCol(239, 68, 68), 2);
    g.setColour(RGBCol(254, 202, 202));
    g.drawText(juce::String::fromUTF8("\xE2\x97\x8F REC 6s"), kRecSampleBtnRect, juce::Justification::centred, false);

    // Play Test Guitar Riff Button
    bool riffOn = processor.testRiffActive.load();
    fillRoundRectJuce(g, kRiffBtnRect.getX(), kRiffBtnRect.getY(),
                      kRiffBtnRect.getRight(), kRiffBtnRect.getBottom(), 6,
                      riffOn ? RGBCol(245, 158, 11) : RGBCol(6, 78, 59),
                      riffOn ? RGBCol(254, 240, 138) : RGBCol(20, 184, 166), 2);
    g.setColour(riffOn ? RGBCol(40, 20, 0) : RGBCol(240, 253, 250));
    g.drawText(riffOn ? "RIFF: ON" : "TEST RIFF", kRiffBtnRect, juce::Justification::centred, false);

    // Audio Settings Button
    fillRoundRectJuce(g, kAudioSettingsRect.getX(), kAudioSettingsRect.getY(),
                      kAudioSettingsRect.getRight(), kAudioSettingsRect.getBottom(), 6,
                      RGBCol(13, 148, 136), RGBCol(45, 212, 191), 2);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(RGBCol(255, 255, 255));
    g.drawText("AUDIO SETTINGS", kAudioSettingsRect, juce::Justification::centred, false);

    // 2. Global Header Selector Pills
    const auto& table = euphoriaParamTable();
    for (int i = 0; i <= 5; ++i) {
        const auto& d = table[(size_t)i];
        const auto& gk = knobLayout[(size_t)i];
        float curVal = processor.getParamValue(i);
        bool is432 = (i == K_TUNING_REF && curVal > 0.5f);
        bool isPickupCustom = (i == K_PICKUP_VOICE && curVal > 0.5f);
        juce::Colour pillBg = is432 ? RGBCol(16, 185, 129)
                            : (isPickupCustom ? (curVal > 1.5f ? RGBCol(180, 83, 9) : RGBCol(14, 116, 144))
                            : RGBCol(6, 34, 28));
        juce::Colour pillBorder = is432 ? RGBCol(167, 243, 208)
                                : (isPickupCustom ? (curVal > 1.5f ? RGBCol(253, 230, 138) : RGBCol(103, 232, 249))
                                : RGBCol(245, 158, 11));
        fillRoundRectJuce(g, gk.rect.getX(), gk.rect.getY(), gk.rect.getRight(), gk.rect.getBottom(), 6, pillBg, pillBorder, 1);
        int idx = std::clamp((int)std::lround(curVal), 0, gk.steps - 1);

        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(is432 ? RGBCol(209, 250, 229) : RGBCol(251, 191, 36));
        g.drawText(d.label, gk.rect.getX() + 2, gk.rect.getY() + 2, gk.rect.getWidth() - 4, 14, juce::Justification::centred, false);

        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour((is432 || isPickupCustom) ? RGBCol(255, 255, 255) : RGBCol(254, 243, 199));
        g.drawText(d.choices[(size_t)idx], gk.rect.getX() + 2, gk.rect.getY() + 16, gk.rect.getWidth() - 4, 18, juce::Justification::centred, false);
    }

    // Live Real-Time Hardware Performance Monitor HUD
    fillRoundRectJuce(g, 774, 56, 1238, 92, 6, RGBCol(3, 18, 22), RGBCol(20, 184, 166), 1);
    float cpuVal  = processor.dspCpuPercent.load();
    float timeVal = processor.dspTimeUs.load();
    int intRate   = processor.dspInternalRate.load();
    int bufSz     = processor.currentBlockSize.load();
    const char* osTag = (intRate >= 384000) ? "8x" : ((intRate >= 192000) ? "4x" : ((intRate >= 96000) ? "2x" : "1x"));

    char hudLine1[96];
    std::snprintf(hudLine1, sizeof(hudLine1), "DSP: %.1f%%  |  %u us  |  CORE: %d kHz (%s)  |  BUF: %d",
                  cpuVal, (unsigned int)timeVal, intRate / 1000, osTag, bufSz);
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour((cpuVal > 50.0f) ? RGBCol(248, 113, 113) : ((cpuVal > 25.0f) ? RGBCol(251, 191, 36) : RGBCol(52, 211, 153)));
    g.drawText(hudLine1, 782, 59, 448, 16, juce::Justification::centredLeft, false);

    fillRoundRectJuce(g, 782, 77, 1230, 87, 3, RGBCol(2, 10, 12), RGBCol(13, 148, 136), 1);
    float normLoad = std::clamp(cpuVal / 40.0f, 0.0f, 1.0f);
    int filledW = (int)(448.0f * normLoad);
    if (filledW > 2) {
        juce::Colour barCol = (cpuVal > 30.0f) ? RGBCol(239, 68, 68) : ((cpuVal > 15.0f) ? RGBCol(245, 158, 11) : RGBCol(16, 185, 129));
        fillRoundRectJuce(g, 783, 78, 783 + filledW, 86, 2, barCol, barCol, 1);
    }

    // 3. Main 3D Skeuomorphic Amp Chassis / Pedalboard / Rack
    int bx1 = 18, by1 = 104, bx2 = w - 18, by2 = h - 46;

    if (activePage == 0) {
        const juce::Image& imgClean = imgChassisCleanBlue.isValid() ? imgChassisCleanBlue : imgChassisWide;
        if (imgClean.isValid()) {
            g.drawImage(imgClean, bx1, by1, bx2 - bx1, by2 - by1, 0, 0, imgClean.getWidth(), imgClean.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, by1, bx2, by2, 18, RGBCol(4, 47, 46), RGBCol(245, 158, 11), 3);
            fillRoundRectJuce(g, bx1 + 18, by1 + 16, bx2 - 18, by1 + 280, 10, RGBCol(6, 34, 28), RGBCol(251, 191, 36), 2);
            drawBasketweaveGrille(g, bx1 + 18, by1 + 292, bx2 - 18, by2 - 16, RGBCol(4, 47, 46), RGBCol(217, 119, 6));
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(2, 20, 16));
        g.drawText("CHANNEL 1  |  EUPHORIA SUPREME GLASS CLEAN (ACTIVE 3D TONE MATRIX & 16kHz EXTENDED AIR)", bx1 + 39, by1 + 17, 880, 20, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(52, 211, 153));
        g.drawText("CHANNEL 1  |  EUPHORIA SUPREME GLASS CLEAN (ACTIVE 3D TONE MATRIX & 16kHz EXTENDED AIR)", bx1 + 38, by1 + 16, 880, 20, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(2, 20, 16));
        g.drawText("Euphoria Supreme Glass Clean", bx1 + 39, by1 + 35, 760, 30, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(254, 240, 138));
        g.drawText("Euphoria Supreme Glass Clean", bx1 + 38, by1 + 34, 760, 30, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, bx2 - 68, by1 + 40, 16, RGBCol(5, 150, 105), RGBCol(52, 211, 153), RGBCol(254, 240, 138));
    }
    else if (activePage == 1) {
        const juce::Image& imgCrunch = imgChassisCrunchBrown.isValid() ? imgChassisCrunchBrown : imgChassisWide;
        if (imgCrunch.isValid()) {
            g.drawImage(imgCrunch, bx1, by1, bx2 - bx1, by2 - by1, 0, 0, imgCrunch.getWidth(), imgCrunch.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, by1, bx2, by2, 18, RGBCol(67, 20, 7), RGBCol(249, 115, 22), 3);
            fillRoundRectJuce(g, bx1 + 18, by1 + 16, bx2 - 18, by1 + 280, 10, RGBCol(124, 45, 18), RGBCol(254, 215, 170), 2);
            drawBasketweaveGrille(g, bx1 + 18, by1 + 292, bx2 - 18, by2 - 16, RGBCol(45, 15, 6), RGBCol(194, 65, 12));
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(20, 5, 2));
        g.drawText("CHANNEL 2  |  FLUID MOMENTUM CATHODE FOLLOWER + 56k TWEED / 33k PLEXI VISCOUS SLOPE", bx1 + 39, by1 + 17, 780, 20, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(254, 215, 170));
        g.drawText("CHANNEL 2  |  FLUID MOMENTUM CATHODE FOLLOWER + 56k TWEED / 33k PLEXI VISCOUS SLOPE", bx1 + 38, by1 + 16, 780, 20, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(20, 5, 2));
        g.drawText("Euphoria '59 Bassman / '68 JMP Crunch", bx1 + 39, by1 + 35, 760, 30, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(255, 237, 213));
        g.drawText("Euphoria '59 Bassman / '68 JMP Crunch", bx1 + 38, by1 + 34, 760, 30, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, bx2 - 68, by1 + 40, 16, RGBCol(194, 65, 12), RGBCol(251, 146, 60), RGBCol(254, 240, 138));
    }
    else if (activePage == 2) {
        const juce::Image& imgLead = imgChassisLeadRed.isValid() ? imgChassisLeadRed : imgChassisWide;
        if (imgLead.isValid()) {
            g.drawImage(imgLead, bx1, by1, bx2 - bx1, by2 - by1, 0, 0, imgLead.getWidth(), imgLead.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, by1, bx2, by2, 18, RGBCol(45, 10, 15), RGBCol(239, 68, 68), 3);
            fillRoundRectJuce(g, bx1 + 18, by1 + 16, bx2 - 18, by1 + 280, 10, RGBCol(24, 16, 18), RGBCol(239, 68, 68), 2);
            drawBasketweaveGrille(g, bx1 + 18, by1 + 292, bx2 - 18, by2 - 16, RGBCol(18, 16, 16), RGBCol(60, 50, 50));
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(20, 5, 8));
        g.drawText("CHANNEL 3  |  4-STAGE 12AX7 CASCADE + 10k COLD CLIPPER + C45 MID PUNCH + DYNAMIC DE-HISS", bx1 + 39, by1 + 17, 820, 20, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(254, 202, 202));
        g.drawText("CHANNEL 3  |  4-STAGE 12AX7 CASCADE + 10k COLD CLIPPER + C45 MID PUNCH + DYNAMIC DE-HISS", bx1 + 38, by1 + 16, 820, 20, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(20, 5, 8));
        g.drawText("Euphoria '80s Friedman BE-100 / JCM 800 Lead", bx1 + 39, by1 + 35, 760, 30, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(254, 240, 138));
        g.drawText("Euphoria '80s Friedman BE-100 / JCM 800 Lead", bx1 + 38, by1 + 34, 760, 30, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, bx2 - 68, by1 + 40, 16, RGBCol(185, 28, 28), RGBCol(239, 68, 68), RGBCol(254, 202, 202));
    }
    else if (activePage == 3) {
        if (imgWoodFloor.isValid()) {
            g.setTiledImageFill(imgWoodFloor, bx1, by1, 1.0f);
            g.fillRect(bx1, by1, bx2 - bx1, by2 - by1);
            g.setColour(RGBCol(24, 14, 8));
            g.drawRect(bx1, by1, bx2 - bx1, by2 - by1, 2);
        } else {
            fillRoundRectJuce(g, bx1, by1, bx2, by2, 14, RGBCol(42, 26, 14), RGBCol(80, 50, 28), 2);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(254, 240, 138));
        g.drawText("HARDWOOD PEDALBOARD DECK  |  5 INDEPENDENT SERIES STOMPBOXES  |  MASSIVE CLEAN BOOST HEADROOM (+22dB)",
                   bx1 + 24, by1 + 6, bx2 - bx1 - 48, 18, juce::Justification::centred, false);

        auto drawCornerRivets = [&](int x1, int y1, int x2, int y2) {
            int rx[4] = { x1 + 10, x2 - 10, x1 + 10, x2 - 10 };
            int ry[4] = { y1 + 10, y1 + 10, y2 - 10, y2 - 10 };
            for (int i = 0; i < 4; ++i) {
                g.setColour(RGBCol(160, 160, 165));
                g.fillEllipse((float)(rx[i] - 3), (float)(ry[i] - 3), 6.0f, 6.0f);
                g.setColour(RGBCol(45, 45, 50));
                g.drawLine((float)(rx[i] - 2), (float)ry[i], (float)(rx[i] + 2), (float)ry[i], 1.0f);
            }
        };

        // PEDAL 1: FLUID CENTAUR (Rectangular Boutique Chassis)
        int p1X1 = 109, p1X2 = 299, p1Y1 = 135, p1Y2 = 555;
        bool klonActive = (processor.getParamValue(K_KLON_ON) > 0.5f);
        bool klonHyp    = (processor.getParamValue(K_KLON_HYPER) > 0.5f);

        // Floor drop shadow
        fillRoundRectJuce(g, p1X1 + 5, p1Y1 + 7, p1X2 + 5, p1Y2 + 7, 14, RGBCol(18, 10, 6), RGBCol(18, 10, 6), 1);

        // Rectangular Champagne Gold Enclosure
        juce::Colour klonChassisBg = klonActive ? RGBCol(190, 145, 45) : RGBCol(115, 85, 28);
        juce::Colour klonBorder    = klonActive ? RGBCol(254, 240, 138) : RGBCol(165, 125, 45);
        fillRoundRectJuce(g, p1X1, p1Y1, p1X2, p1Y2, 14, klonChassisBg, klonBorder, klonActive ? 2 : 1);
        drawCornerRivets(p1X1, p1Y1, p1X2, p1Y2);

        // Recessed Oxblood Faceplate Accent
        fillRoundRectJuce(g, 204 - 74, p1Y1 + 42, 204 + 74, p1Y2 - 110, 14,
                          RGBCol(48, 14, 16), RGBCol(95, 30, 25), 1);

        // Header Plaque
        fillRoundRectJuce(g, p1X1 + 10, p1Y1 + 8, p1X2 - 10, p1Y1 + 48, 6,
                          RGBCol(65, 14, 16), RGBCol(217, 119, 6), 1);

        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.setColour(klonActive ? RGBCol(254, 240, 138) : RGBCol(200, 170, 110));
        g.drawText("FLUID CENTAUR", p1X1 + 12, p1Y1 + 10, p1X2 - p1X1 - 24, 24, juce::Justification::centred, false);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(RGBCol(254, 202, 202));
        g.drawText(klonHyp ? "HYPER 27V ADAA (+22dB)" : "(OLD) 18V PURE GERMANIUM", p1X1 + 2, p1Y1 + 32, p1X2 - p1X1 - 4, 14, juce::Justification::centred, false);

        // Jewel LED Lamp (Amber/Red)
        fillRoundRectJuce(g, 204 - 8, p1Y1 + 52, 204 + 8, p1Y1 + 68, 16,
                          klonActive ? RGBCol(239, 68, 68) : RGBCol(50, 12, 12),
                          klonActive ? RGBCol(254, 202, 202) : RGBCol(80, 20, 20), 1);
        if (klonActive)
            fillRoundRectJuce(g, 204 - 3, p1Y1 + 56, 204 + 3, p1Y1 + 62, 6, RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);

        // Circuit Mode Badge Pill
        fillRoundRectJuce(g, p1X1 + 12, p1Y2 - 110, p1X2 - 12, p1Y2 - 90, 4,
                          RGBCol(55, 12, 15), RGBCol(217, 119, 6), 1);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(RGBCol(254, 240, 138));
        g.drawText(klonHyp ? "HYPER 27V ADAA (+22dB)" : "(OLD) 18V VINTAGE KLON", p1X1 + 14, p1Y2 - 109, p1X2 - p1X1 - 28, 18, juce::Justification::centred, false);

        draw3DStompFootswitch(g, 204, 480, klonActive, RGBCol(245, 158, 11));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(klonActive ? RGBCol(254, 240, 138) : RGBCol(140, 115, 75));
        g.drawText(klonActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", p1X1, p1Y2 - 24, p1X2 - p1X1, 18, juce::Justification::centred, false);

        // PEDAL 2: TS808 SCREAMER (Rectangular Boutique Chassis)
        int p2X1 = 327, p2X2 = 517, p2Y1 = 135, p2Y2 = 555;
        bool tsActive = (processor.getParamValue(K_TS_ON) > 0.5f);
        bool tsHyp    = (processor.getParamValue(K_TS_HYPER) > 0.5f);
        bool tsFat    = (processor.getParamValue(K_TS_MOD) > 0.5f);

        // Floor drop shadow
        fillRoundRectJuce(g, p2X1 + 5, p2Y1 + 7, p2X2 + 5, p2Y2 + 7, 14, RGBCol(18, 10, 6), RGBCol(18, 10, 6), 1);

        // Rectangular Emerald Green Enclosure
        juce::Colour tsChassisBg = tsActive ? RGBCol(22, 105, 52) : RGBCol(16, 68, 32);
        juce::Colour tsBorder    = tsActive ? RGBCol(110, 231, 183) : RGBCol(40, 115, 60);
        fillRoundRectJuce(g, p2X1, p2Y1, p2X2, p2Y2, 14, tsChassisBg, tsBorder, tsActive ? 2 : 1);
        drawCornerRivets(p2X1, p2Y1, p2X2, p2Y2);

        // Recessed Dark Emerald Faceplate Accent
        fillRoundRectJuce(g, 422 - 74, p2Y1 + 42, 422 + 74, p2Y2 - 110, 14,
                          RGBCol(12, 48, 24), RGBCol(25, 95, 45), 1);

        // Header Plaque
        fillRoundRectJuce(g, p2X1 + 10, p2Y1 + 8, p2X2 - 10, p2Y1 + 48, 6,
                          RGBCol(10, 50, 24), RGBCol(34, 197, 94), 1);

        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.setColour(tsActive ? RGBCol(167, 243, 208) : RGBCol(110, 170, 135));
        g.drawText("TS808 SCREAMER", p2X1 + 12, p2Y1 + 10, p2X2 - p2X1 - 24, 24, juce::Justification::centred, false);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(RGBCol(209, 250, 229));
        const char* tsSub = tsHyp ? (tsFat ? "HYPER FAT VELVET JRC" : "HYPER VELVET JRC4558")
                                  : (tsFat ? "(OLD) FAT MOD 340Hz" : "720.5Hz MID-HUMP & JRC");
        g.drawText(tsSub, p2X1 + 2, p2Y1 + 32, p2X2 - p2X1 - 4, 14, juce::Justification::centred, false);

        // Jewel LED Lamp (Emerald/Amber)
        fillRoundRectJuce(g, 422 - 8, p2Y1 + 52, 422 + 8, p2Y1 + 68, 16,
                          tsActive ? RGBCol(34, 197, 94) : RGBCol(10, 45, 20),
                          tsActive ? RGBCol(187, 247, 208) : RGBCol(20, 80, 35), 1);
        if (tsActive)
            fillRoundRectJuce(g, 422 - 3, p2Y1 + 56, 422 + 3, p2Y1 + 62, 6, RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);

        // Circuit Mode Badge Pill
        fillRoundRectJuce(g, p2X1 + 12, p2Y2 - 110, p2X2 - 12, p2Y2 - 90, 4,
                          RGBCol(8, 42, 22), RGBCol(52, 211, 153), 1);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(RGBCol(167, 243, 208));
        const char* tsBdg = tsHyp ? (tsFat ? "HYPER FAT (+20dB)" : "HYPER VELVET (+20dB)")
                                  : (tsFat ? "(OLD) FAT MOD 340Hz" : "(OLD) STOCK 720Hz");
        g.drawText(tsBdg, p2X1 + 14, p2Y2 - 109, p2X2 - p2X1 - 28, 18, juce::Justification::centred, false);

        draw3DStompFootswitch(g, 422, 480, tsActive, RGBCol(34, 197, 94));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(tsActive ? RGBCol(167, 243, 208) : RGBCol(100, 150, 120));
        g.drawText(tsActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", p2X1, p2Y2 - 24, p2X2 - p2X1, 18, juce::Justification::centred, false);

        // PEDAL 3: BD-2 BLUES DRIVER (Rectangular Boutique Chassis)
        int p3X1 = 545, p3X2 = 735, p3Y1 = 135, p3Y2 = 555;
        bool bdActive = (processor.getParamValue(K_BD_ON) > 0.5f);
        bool bdHyp    = (processor.getParamValue(K_BD_HYPER) > 0.5f);
        bool bdPhat   = (processor.getParamValue(K_BD_PHAT) > 0.5f);

        // Floor drop shadow
        fillRoundRectJuce(g, p3X1 + 5, p3Y1 + 7, p3X2 + 5, p3Y2 + 7, 14, RGBCol(18, 10, 6), RGBCol(18, 10, 6), 1);

        // Rectangular Sapphire Blue Enclosure
        juce::Colour bdChassisBg = bdActive ? RGBCol(20, 72, 150) : RGBCol(14, 48, 105);
        juce::Colour bdBorder    = bdActive ? RGBCol(147, 197, 253) : RGBCol(55, 100, 165);
        fillRoundRectJuce(g, p3X1, p3Y1, p3X2, p3Y2, 14, bdChassisBg, bdBorder, bdActive ? 2 : 1);
        drawCornerRivets(p3X1, p3Y1, p3X2, p3Y2);

        // Recessed Dark Cobalt Faceplate Accent
        fillRoundRectJuce(g, 640 - 74, p3Y1 + 42, 640 + 74, p3Y2 - 110, 14,
                          RGBCol(10, 28, 65), RGBCol(28, 65, 125), 1);

        // Header Plaque
        fillRoundRectJuce(g, p3X1 + 10, p3Y1 + 8, p3X2 - 10, p3Y1 + 48, 6,
                          RGBCol(12, 32, 75), RGBCol(56, 189, 248), 1);

        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.setColour(bdActive ? RGBCol(254, 240, 138) : RGBCol(210, 200, 120));
        g.drawText("Blues Driver", p3X1 + 12, p3Y1 + 10, p3X2 - p3X1 - 24, 24, juce::Justification::centred, false);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(RGBCol(191, 219, 254));
        const char* bdSub = bdHyp ? (bdPhat ? "HYPER PHAT JFET (+22dB)" : "HYPER JFET SLIPSTREAM")
                                  : (bdPhat ? "(OLD) PHAT MOD 160Hz" : "BD-2 FLUID LAB | DUAL JFET");
        g.drawText(bdSub, p3X1 + 2, p3Y1 + 32, p3X2 - p3X1 - 4, 14, juce::Justification::centred, false);

        // Jewel LED Lamp (Cyan/Azure)
        fillRoundRectJuce(g, 640 - 8, p3Y1 + 52, 640 + 8, p3Y1 + 68, 16,
                          bdActive ? RGBCol(34, 211, 238) : RGBCol(10, 25, 55),
                          bdActive ? RGBCol(186, 230, 253) : RGBCol(25, 50, 95), 1);
        if (bdActive)
            fillRoundRectJuce(g, 640 - 3, p3Y1 + 56, 640 + 3, p3Y1 + 62, 6, RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);

        // Circuit Mode Badge Pill
        fillRoundRectJuce(g, p3X1 + 12, p3Y2 - 110, p3X2 - 12, p3Y2 - 90, 4,
                          RGBCol(10, 30, 68), RGBCol(56, 189, 248), 1);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(RGBCol(186, 230, 253));
        const char* bdBdg = bdHyp ? (bdPhat ? "HYPER PHAT (+22dB)" : "HYPER SLIPSTREAM")
                                  : (bdPhat ? "(OLD) PHAT MOD 160Hz" : "(OLD) STOCK BD-2");
        g.drawText(bdBdg, p3X1 + 14, p3Y2 - 109, p3X2 - p3X1 - 28, 18, juce::Justification::centred, false);

        draw3DStompFootswitch(g, 640, 480, bdActive, RGBCol(56, 189, 248));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(bdActive ? RGBCol(254, 240, 138) : RGBCol(130, 160, 200));
        g.drawText(bdActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", p3X1, p3Y2 - 24, p3X2 - p3X1, 18, juce::Justification::centred, false);

        // PEDAL 4: FLUID RODENT (Rectangular Boutique Chassis)
        int p4X1 = 763, p4X2 = 953, p4Y1 = 135, p4Y2 = 555;
        bool ratActive  = (processor.getParamValue(K_RAT_ON) > 0.5f);
        bool ratIsTurbo = (processor.getParamValue(K_RAT_TURBO) > 0.5f);
        bool ratHyp     = (processor.getParamValue(K_RAT_HYPER) > 0.5f);

        // Floor drop shadow
        fillRoundRectJuce(g, p4X1 + 5, p4Y1 + 7, p4X2 + 5, p4Y2 + 7, 14, RGBCol(18, 10, 6), RGBCol(18, 10, 6), 1);

        // Rectangular Obsidian Enclosure
        juce::Colour ratChassisBg = ratActive ? RGBCol(32, 34, 40) : RGBCol(22, 23, 27);
        juce::Colour ratBorder    = ratActive ? (ratIsTurbo ? RGBCol(252, 165, 165) : RGBCol(254, 240, 138)) : RGBCol(75, 78, 85);
        fillRoundRectJuce(g, p4X1, p4Y1, p4X2, p4Y2, 14, ratChassisBg, ratBorder, ratActive ? 2 : 1);
        drawCornerRivets(p4X1, p4Y1, p4X2, p4Y2);

        // Recessed Gunmetal Faceplate Accent
        fillRoundRectJuce(g, 858 - 74, p4Y1 + 42, 858 + 74, p4Y2 - 110, 14,
                          RGBCol(16, 17, 20), RGBCol(45, 48, 55), 1);

        // Header Plaque
        fillRoundRectJuce(g, p4X1 + 10, p4Y1 + 8, p4X2 - 10, p4Y1 + 48, 6,
                          RGBCol(18, 19, 22), ratIsTurbo ? RGBCol(239, 68, 68) : RGBCol(245, 158, 11), 1);

        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.setColour(ratActive ? (ratIsTurbo ? RGBCol(254, 202, 202) : RGBCol(254, 240, 138)) : RGBCol(180, 180, 180));
        g.drawText(ratIsTurbo ? "TURBO RODENT" : "FLUID RODENT", p4X1 + 12, p4Y1 + 10, p4X2 - p4X1 - 24, 24, juce::Justification::centred, false);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(ratIsTurbo ? RGBCol(252, 165, 165) : RGBCol(254, 240, 138));
        const char* ratSub = ratHyp ? (ratIsTurbo ? "HYPER TURBO VIOLIN LED" : "HYPER LM308 SLEW & SHUNT")
                                    : (ratIsTurbo ? "TURBO RED LED & LM308 SLEW" : "1978 LM308 SLEW & DIODE SHUNT");
        g.drawText(ratSub, p4X1 + 2, p4Y1 + 32, p4X2 - p4X1 - 4, 14, juce::Justification::centred, false);

        // Jewel LED Lamp (Red)
        fillRoundRectJuce(g, 858 - 8, p4Y1 + 52, 858 + 8, p4Y1 + 68, 16,
                          ratActive ? RGBCol(239, 68, 68) : RGBCol(50, 12, 12),
                          ratActive ? RGBCol(254, 202, 202) : RGBCol(80, 20, 20), 1);
        if (ratActive)
            fillRoundRectJuce(g, 858 - 3, p4Y1 + 56, 858 + 3, p4Y1 + 62, 6, RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);

        // Circuit Mode Badge Pill
        fillRoundRectJuce(g, p4X1 + 12, p4Y2 - 110, p4X2 - 12, p4Y2 - 90, 4,
                          RGBCol(22, 24, 28), ratIsTurbo ? RGBCol(239, 68, 68) : RGBCol(245, 158, 11), 1);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(ratIsTurbo ? RGBCol(252, 165, 165) : RGBCol(254, 240, 138));
        const char* ratBdg = ratHyp ? (ratIsTurbo ? "HYPER TURBO VIOLIN" : "HYPER LM308 SILICON")
                                    : (ratIsTurbo ? "(OLD) TURBO RED LED" : "(OLD) SILICON DIODE");
        g.drawText(ratBdg, p4X1 + 14, p4Y2 - 109, p4X2 - p4X1 - 28, 18, juce::Justification::centred, false);

        draw3DStompFootswitch(g, 858, 480, ratActive, RGBCol(239, 68, 68));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(ratActive ? (ratIsTurbo ? RGBCol(252, 165, 165) : RGBCol(254, 240, 138)) : RGBCol(140, 140, 140));
        g.drawText(ratActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", p4X1, p4Y2 - 24, p4X2 - p4X1, 18, juce::Justification::centred, false);

        // PEDAL 5: FLUID FUZZ FACE (Rectangular Boutique Chassis)
        int p5X1 = 981, p5X2 = 1171, p5Y1 = 135, p5Y2 = 555;
        bool ffActive    = (processor.getParamValue(K_FUZZ_ON) > 0.5f);
        bool ffIsSilicon = (processor.getParamValue(K_FUZZ_TRANSISTOR) > 0.5f);
        bool ffHyp       = (processor.getParamValue(K_FUZZ_HYPER) > 0.5f);

        // Cast shadow on hardwood floor
        fillRoundRectJuce(g, p5X1 + 5, p5Y1 + 7, p5X2 + 5, p5Y2 + 7, 14, RGBCol(18, 10, 6), RGBCol(18, 10, 6), 1);

        // Rectangular Boutique Stompbox Enclosure
        juce::Colour ffChassisBg = ffActive ? (ffIsSilicon ? RGBCol(55, 65, 85) : RGBCol(165, 42, 42))
                                            : (ffIsSilicon ? RGBCol(38, 45, 58) : RGBCol(105, 30, 30));
        juce::Colour ffBorder    = ffActive ? (ffIsSilicon ? RGBCol(147, 197, 253) : RGBCol(252, 165, 165))
                                            : (ffIsSilicon ? RGBCol(96, 115, 145) : RGBCol(160, 60, 60));
        fillRoundRectJuce(g, p5X1, p5Y1, p5X2, p5Y2, 14, ffChassisBg, ffBorder, ffActive ? 2 : 1);
        drawCornerRivets(p5X1, p5Y1, p5X2, p5Y2);

        // Circular Vintage Faceplate Disc Accent
        fillRoundRectJuce(g, 1076 - 72, p5Y1 + 42, 1076 + 72, p5Y2 - 110, 72,
                          ffIsSilicon ? RGBCol(28, 34, 46) : RGBCol(42, 16, 18),
                          ffIsSilicon ? RGBCol(65, 80, 105) : RGBCol(80, 30, 35), 1);

        // Header Plaque
        fillRoundRectJuce(g, p5X1 + 10, p5Y1 + 8, p5X2 - 10, p5Y1 + 48, 6,
                          ffIsSilicon ? RGBCol(20, 26, 36) : RGBCol(55, 12, 15),
                          ffIsSilicon ? RGBCol(96, 165, 250) : RGBCol(239, 68, 68), 1);

        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.setColour(ffActive ? RGBCol(254, 240, 138) : RGBCol(220, 180, 160));
        g.drawText("FUZZ FACE", p5X1 + 12, p5Y1 + 10, p5X2 - p5X1 - 24, 24, juce::Justification::centred, false);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(ffIsSilicon ? RGBCol(191, 219, 254) : RGBCol(254, 202, 202));
        const char* ffSub = ffHyp ? (ffIsSilicon ? "HYPER BC108 (432Hz)" : "HYPER NKT275 (432Hz)")
                                  : (ffIsSilicon ? "(OLD) BC108 SILICON" : "(OLD) NKT275 GERMANIUM");
        g.drawText(ffSub, p5X1 + 2, p5Y1 + 32, p5X2 - p5X1 - 4, 14, juce::Justification::centred, false);
        fillRoundRectJuce(g, 1076 - 8, p5Y1 + 52, 1076 + 8, p5Y1 + 68, 16,
                          ffActive ? (ffIsSilicon ? RGBCol(56, 189, 248) : RGBCol(239, 68, 68))
                                   : (ffIsSilicon ? RGBCol(12, 38, 55)   : RGBCol(50, 12, 12)),
                          ffActive ? (ffIsSilicon ? RGBCol(186, 230, 253) : RGBCol(254, 202, 202))
                                   : (ffIsSilicon ? RGBCol(25, 60, 85)   : RGBCol(80, 20, 20)), 1);
        if (ffActive)
            fillRoundRectJuce(g, 1076 - 3, p5Y1 + 56, 1076 + 3, p5Y1 + 62, 6, RGBCol(255, 255, 255), RGBCol(255, 255, 255), 1);

        // Fuzz Morph & Calibration Buttons
        bool isFfLearnMax   = eng ? eng->isFuzzFaceLearningMax() : false;
        bool isFfLearnClean = eng ? eng->isFuzzFaceLearningClean() : false;
        if ((isFfLearnMax || isFfLearnClean) && (now - fuzzLearnStartTime > 10500)) {
            if (eng) eng->finishFuzzFaceLearning();
            isFfLearnMax = false;
            isFfLearnClean = false;
        }
        int secRemaining = 10;
        if (isFfLearnMax || isFfLearnClean) {
            uint32_t elapsed = now - fuzzLearnStartTime;
            secRemaining = (elapsed < 10000) ? std::max(1, (int)std::ceil((10000 - elapsed) / 1000.0f)) : 0;
        }
        float morphVal = eng ? std::clamp(eng->getFuzzFaceMorph(), 0.0f, 1.0f) : 0.0f;
        fillRoundRectJuce(g, kFuzzMorphMeterRect.getX(), kFuzzMorphMeterRect.getY(),
                          kFuzzMorphMeterRect.getRight(), kFuzzMorphMeterRect.getBottom(), 4,
                          RGBCol(12, 16, 20), ffActive ? (ffIsSilicon ? RGBCol(75, 90, 115) : RGBCol(90, 35, 40)) : RGBCol(40, 25, 25), 1);
        int meterInnerW = kFuzzMorphMeterRect.getWidth() - 4;
        int fillW = (int)(meterInnerW * morphVal);
        if (fillW > 2 && ffActive) {
            juce::Colour fillCol = (morphVal < 0.35f) ? RGBCol(34, 211, 238) : ((morphVal < 0.70f) ? RGBCol(245, 158, 11) : RGBCol(239, 68, 68));
            fillRoundRectJuce(g, kFuzzMorphMeterRect.getX() + 2, kFuzzMorphMeterRect.getY() + 2,
                              kFuzzMorphMeterRect.getX() + 2 + fillW, kFuzzMorphMeterRect.getBottom() - 2, 2, fillCol, fillCol, 1);
        }
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        char morphTxt[48];
        if (isFfLearnMax || isFfLearnClean) {
            std::snprintf(morphTxt, sizeof(morphTxt), "CALIBRATING: %ds", secRemaining);
            g.setColour(RGBCol(254, 240, 138));
        } else {
            std::snprintf(morphTxt, sizeof(morphTxt), "FUZZ MORPH: %.0f%%", morphVal * 100.0f);
            g.setColour(ffActive ? RGBCol(255, 255, 255) : RGBCol(140, 120, 120));
        }
        g.drawText(morphTxt, kFuzzMorphMeterRect, juce::Justification::centred, false);
        g.setColour(RGBCol(167, 243, 208));
        g.drawText("CLN 7", kFuzzMorphMeterRect.getX() + 4, kFuzzMorphMeterRect.getY(), 42, 16, juce::Justification::centredLeft, false);
        g.setColour(RGBCol(254, 202, 202));
        g.drawText("BORE 10", kFuzzMorphMeterRect.getRight() - 48, kFuzzMorphMeterRect.getY(), 44, 16, juce::Justification::centredRight, false);

        fillRoundRectJuce(g, kBtnFuzzLearnMaxRect.getX(), kBtnFuzzLearnMaxRect.getY(),
                          kBtnFuzzLearnMaxRect.getRight(), kBtnFuzzLearnMaxRect.getBottom(), 4,
                          isFfLearnMax ? RGBCol(180, 83, 9) : (ffIsSilicon ? RGBCol(24, 30, 42) : RGBCol(45, 12, 15)),
                          isFfLearnMax ? RGBCol(251, 191, 36) : (ffIsSilicon ? RGBCol(96, 165, 250) : RGBCol(239, 68, 68)), isFfLearnMax ? 2 : 1);
        char maxTxt[32];
        std::snprintf(maxTxt, sizeof(maxTxt), "MAX 10: %.3f", eng ? eng->getFuzzFaceCalibratedMax() : 0.080f);
        g.setColour(ffActive ? RGBCol(254, 240, 138) : RGBCol(200, 180, 170));
        g.drawText(maxTxt, kBtnFuzzLearnMaxRect, juce::Justification::centred, false);

        fillRoundRectJuce(g, kBtnFuzzLearnCleanRect.getX(), kBtnFuzzLearnCleanRect.getY(),
                          kBtnFuzzLearnCleanRect.getRight(), kBtnFuzzLearnCleanRect.getBottom(), 4,
                          isFfLearnClean ? RGBCol(6, 95, 70) : (ffIsSilicon ? RGBCol(24, 30, 42) : RGBCol(45, 12, 15)),
                          isFfLearnClean ? RGBCol(52, 211, 153) : (ffIsSilicon ? RGBCol(96, 165, 250) : RGBCol(239, 68, 68)), isFfLearnClean ? 2 : 1);
        char clnTxt[32];
        std::snprintf(clnTxt, sizeof(clnTxt), "CLN 7: %.3f", eng ? eng->getFuzzFaceCalibratedClean() : 0.018f);
        g.setColour(ffActive ? RGBCol(167, 243, 208) : RGBCol(180, 200, 190));
        g.drawText(clnTxt, kBtnFuzzLearnCleanRect, juce::Justification::centred, false);

        draw3DStompFootswitch(g, 1076, 480, ffActive, ffIsSilicon ? RGBCol(56, 189, 248) : RGBCol(239, 68, 68));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(ffActive ? RGBCol(254, 240, 138) : RGBCol(160, 120, 120));
        g.drawText(ffActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", p5X1, p5Y2 - 24, p5X2 - p5X1, 18, juce::Justification::centred, false);
    }
    else if (activePage == 4) {
        const juce::Image& imgSpace = imgRackSpaceTopaz.isValid() ? imgRackSpaceTopaz : imgChassisWide;
        if (imgSpace.isValid()) {
            g.drawImage(imgSpace, bx1, by1, bx2 - bx1, by2 - by1, 0, 0, imgSpace.getWidth(), imgSpace.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, by1, bx2, by2, 18, RGBCol(6, 24, 38), RGBCol(6, 182, 212), 3);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(103, 232, 249));
        g.drawText("ACOUSTIC RESONATOR LAB  |  GEOMETRIC FLUID CHAMBER MORPH (CIRCLE <-> BOX) + 432 Hz SIZE LADDER",
                   bx1 + 38, by1 + 14, 880, 18, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(255, 255, 255));
        g.drawText("Euphoria Fluid Space Chamber & Modulator", bx1 + 38, by1 + 30, 760, 28, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, bx2 - 68, by1 + 36, 16, RGBCol(8, 145, 178), RGBCol(103, 232, 249), RGBCol(255, 255, 255));

        int grLeft = 152, grRight = 1128, grTop = 330, grBottom = by2 - 16;
        fillRoundRectJuce(g, grLeft, grTop, grRight, grBottom, 8, RGBCol(2, 12, 18), RGBCol(6, 182, 212), 2);
        fillRoundRectJuce(g, grLeft + 4, grTop + 4, grRight - 4, grBottom - 4, 6, RGBCol(1, 8, 12), RGBCol(8, 40, 50), 1);

        fillRoundRectJuce(g, grLeft + 7, grTop + 7, grLeft + 13, grTop + 13, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, grRight - 13, grTop + 7, grRight - 7, grTop + 13, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, grLeft + 7, grBottom - 13, grLeft + 13, grBottom - 7, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, grRight - 13, grBottom - 13, grRight - 7, grBottom - 7, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);

        int viewMidX = (grLeft + grRight) / 2;
        g.setColour(RGBCol(14, 116, 144));
        for (int dy = grTop + 10; dy < grBottom - 10; dy += 4)
            g.drawVerticalLine(viewMidX, (float)dy, (float)(dy + 2));

        int leftCenterX = (grLeft + 18 + viewMidX) / 2;
        int centerY = (grTop + grBottom) / 2;
        int shapeIdx = std::clamp((int)std::lround(processor.getParamValue(K_SPACE_SHAPE)), 0, 10);
        static const float s_chamberShapeFactors[11] = {
            0.00f, 0.08f, 0.16f, 0.28f, 0.40f, 0.50f, 0.60f, 0.72f, 0.82f, 0.92f, 1.00f
        };
        float shapeFact = s_chamberShapeFactors[shapeIdx];
        int sizeIdx = std::clamp((int)std::lround(processor.getParamValue(K_SPACE_SIZE)), 0, 18);
        float dispR = 24.0f + ((float)sizeIdx / 18.0f) * 36.0f;
        bool modActive = (processor.getParamValue(K_SPACE_MOD_ON) > 0.5f);
        float modSpeed = processor.getParamValue(K_SPACE_MOD_RATE);

        g.setColour(RGBCol(12, 74, 96));
        for (int r = 20; r <= 70; r += 16)
            g.drawEllipse((float)(leftCenterX - r), (float)(centerY - r), (float)(r * 2), (float)(r * 2), 1.0f);

        juce::Path poly;
        constexpr int numPts = 36;
        for (int p = 0; p <= numPts; ++p) {
            float theta = ((float)p / (float)numPts) * 6.2831853f;
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);
            float maxCoord = std::max(std::abs(cosT), std::abs(sinT));
            float rSquare = dispR / std::max(0.001f, maxCoord);
            float rFinal = (1.0f - shapeFact) * dispR + shapeFact * rSquare;
            if (modActive)
                rFinal += std::sin(theta * 4.0f + animPhase * (modSpeed * 0.4f + 0.5f)) * 4.0f;
            float px = (float)leftCenterX + cosT * rFinal;
            float py = (float)centerY + sinT * rFinal;
            if (p == 0) poly.startNewSubPath(px, py);
            else poly.lineTo(px, py);
        }
        poly.closeSubPath();
        g.setColour(RGBCol(6, 44, 58));
        g.fillPath(poly);
        g.setColour(RGBCol(34, 211, 238));
        g.strokePath(poly, juce::PathStrokeType(2.0f));

        if (shapeFact > 0.25f) {
            float cornerDist = dispR * 1.32f;
            static const float s_cornerAngles[4] = { 0.785398f, 2.356194f, 3.92699f, 5.497787f };
            for (int c = 0; c < 4; ++c) {
                int ecx = leftCenterX + (int)(std::cos(s_cornerAngles[c]) * cornerDist);
                int ecy = centerY + (int)(std::sin(s_cornerAngles[c]) * cornerDist);
                int er = (int)(6.0f * shapeFact);
                fillRoundRectJuce(g, ecx - er, ecy - er, ecx + er, ecy + er, er * 2, RGBCol(251, 191, 36), RGBCol(245, 158, 11), 1);
            }
        }

        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(RGBCol(103, 232, 249));
        char leftLbl[96];
        std::snprintf(leftLbl, sizeof(leftLbl), "CHAMBER: %s  |  432Hz SIZE: %s",
                      kShapeNames[shapeIdx], kSizeNames[sizeIdx]);
        g.drawText(leftLbl, grLeft + 16, grBottom - 24, viewMidX - grLeft - 26, 18, juce::Justification::centred, false);

        int oscLeft = viewMidX + 24, oscRight = grRight - 20;
        int oscTop = grTop + 14, oscBottom = grBottom - 28;
        int oscW = oscRight - oscLeft;
        int oscMidY = (oscTop + oscBottom) / 2;

        g.setColour(RGBCol(10, 50, 65));
        for (int gy = oscTop + 12; gy < oscBottom; gy += 16)
            g.drawHorizontalLine(gy, (float)oscLeft, (float)oscRight);
        for (int gx = oscLeft + 24; gx < oscRight; gx += 40)
            g.drawVerticalLine(gx, (float)oscTop, (float)oscBottom);

        juce::Path wave432;
        for (int x = oscLeft; x <= oscRight; ++x) {
            float fx = (float)(x - oscLeft) / (float)oscW;
            float standingWave = std::sin(fx * 24.0f + animPhase * 3.0f) * 12.0f
                               + std::sin(fx * 38.83f + animPhase * 4.85f) * 5.0f;
            float wy = (float)oscMidY + standingWave;
            if (x == oscLeft) wave432.startNewSubPath((float)x, wy);
            else wave432.lineTo((float)x, wy);
        }
        g.setColour(RGBCol(16, 185, 129));
        g.strokePath(wave432, juce::PathStrokeType(2.0f));

        if (modActive) {
            juce::Path modWave;
            int waveType = std::clamp((int)std::lround(processor.getParamValue(K_SPACE_WAVEFORM)), 0, 3);
            for (int x = oscLeft; x <= oscRight; ++x) {
                float fx = (float)(x - oscLeft) / (float)oscW;
                float phase = fx * 6.2831853f * 2.0f + animPhase * modSpeed;
                float normPhase = std::fmod(phase / 6.2831853f, 1.0f);
                if (normPhase < 0.0f) normPhase += 1.0f;
                float lfoVal = 0.0f;
                if (waveType == 0) lfoVal = std::sin(phase);
                else if (waveType == 1) lfoVal = (normPhase < 0.5f) ? (4.0f * normPhase - 1.0f) : (3.0f - 4.0f * normPhase);
                else if (waveType == 2) lfoVal = 0.65f * std::sin(phase) + 0.35f * std::sin(phase * 1.618034f);
                else lfoVal = 0.50f * std::sin(phase) + 0.30f * std::sin(phase * 0.707f) + 0.20f * std::sin(phase * 2.14f);
                float wy = (float)oscMidY - lfoVal * 20.0f;
                if (x == oscLeft) modWave.startNewSubPath((float)x, wy);
                else modWave.lineTo((float)x, wy);
            }
            g.setColour(RGBCol(251, 191, 36));
            g.strokePath(modWave, juce::PathStrokeType(2.0f));
        }

        g.setColour(modActive ? RGBCol(251, 191, 36) : RGBCol(148, 163, 184));
        char rightLbl[96];
        if (modActive) {
            int wt = std::clamp((int)std::lround(processor.getParamValue(K_SPACE_WAVEFORM)), 0, 3);
            std::snprintf(rightLbl, sizeof(rightLbl), "SPACE MOD LFO: ACTIVE (%.1f Hz %s  |  SPREAD: %.0f deg)",
                          modSpeed, table[K_SPACE_WAVEFORM].choices[(size_t)wt], processor.getParamValue(K_SPACE_SPREAD));
        } else {
            std::snprintf(rightLbl, sizeof(rightLbl), "SPACE MODULATION: BYPASSED (432Hz HARMONIC MATRIX STABLE)");
        }
        g.drawText(rightLbl, viewMidX + 10, grBottom - 24, grRight - viewMidX - 26, 18, juce::Justification::centred, false);
    }
    else if (activePage == 5) {
        int r1Y1 = 104, r1Y2 = 380;
        int r2Y1 = 398, r2Y2 = 674;

        const juce::Image& imgRev = imgRackReverbPink.isValid() ? imgRackReverbPink : imgChassisWide;
        if (imgRev.isValid()) {
            g.drawImage(imgRev, bx1, r1Y1, bx2 - bx1, r1Y2 - r1Y1, 0, 0, imgRev.getWidth(), imgRev.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, r1Y1, bx2, r1Y2, 14, RGBCol(55, 10, 35), RGBCol(244, 114, 182), 2);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(251, 113, 133));
        g.drawText("RACK UNIT 1  |  PURE FLUID REVERB SPACE (FDN 432Hz CAVITY & DIFFUSION)", bx1 + 38, r1Y1 + 16, 610, 20, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(255, 240, 245));
        g.drawText("Euphoria Fluid Reverb Space", bx1 + 38, r1Y1 + 34, 560, 30, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, 642, r1Y1 + 42, 16, RGBCol(190, 24, 93), RGBCol(244, 114, 182), RGBCol(255, 255, 255));

        bool revActive = (processor.getParamValue(K_REV_ON) > 0.5f);
        int revShapeIdx = std::clamp((int)std::lround(processor.getParamValue(K_REV_SHAPE)), 0, 10);
        int revSizeIdx  = std::clamp((int)std::lround(processor.getParamValue(K_REV_SIZE)), 0, 18);

        int revBayX1 = 680, revBayX2 = 1128, revBayY1 = r1Y1 + 48, revBayY2 = r1Y2 - 18;
        fillRoundRectJuce(g, revBayX1, revBayY1, revBayX2, revBayY2, 8, RGBCol(24, 8, 18), RGBCol(244, 114, 182), 2);
        fillRoundRectJuce(g, revBayX1 + 4, revBayY1 + 4, revBayX2 - 4, revBayY2 - 4, 6, RGBCol(14, 4, 10), RGBCol(70, 15, 45), 1);

        fillRoundRectJuce(g, revBayX1 + 6, revBayY1 + 6, revBayX1 + 11, revBayY1 + 11, 4, RGBCol(251, 113, 133), RGBCol(136, 19, 55), 1);
        fillRoundRectJuce(g, revBayX2 - 11, revBayY1 + 6, revBayX2 - 6, revBayY1 + 11, 4, RGBCol(251, 113, 133), RGBCol(136, 19, 55), 1);
        fillRoundRectJuce(g, revBayX1 + 6, revBayY2 - 11, revBayX1 + 11, revBayY2 - 6, 4, RGBCol(251, 113, 133), RGBCol(136, 19, 55), 1);
        fillRoundRectJuce(g, revBayX2 - 11, revBayY2 - 11, revBayX2 - 6, revBayY2 - 6, 4, RGBCol(251, 113, 133), RGBCol(136, 19, 55), 1);

        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(revActive ? RGBCol(251, 113, 133) : RGBCol(148, 163, 184));
        g.drawText("FDN 432Hz CAVITY ACOUSTICS & STEREO DIFFUSION TANK", revBayX1 + 12, revBayY1 + 6, revBayX2 - revBayX1 - 24, 14, juce::Justification::centred, false);

        int cavMidX = (revBayX1 + revBayX2) / 2;
        int cavMidY = (revBayY1 + revBayY2) / 2 + 2;
        if (revActive) {
            for (int wIdx = 0; wIdx < 4; ++wIdx) {
                float waveFrac = std::fmod((animPhase * 0.45f + (float)wIdx * 0.25f), 1.0f);
                float wrX = 14.0f + waveFrac * 170.0f;
                float wrY = 8.0f + waveFrac * 70.0f;
                int alphaVal = (int)((1.0f - waveFrac) * 220.0f);
                g.setColour(RGBCol(std::min(255, 120 + alphaVal / 2), std::min(255, 30 + alphaVal / 4), std::min(255, 90 + alphaVal / 2)));
                g.drawEllipse((float)cavMidX - wrX, (float)cavMidY - wrY, wrX * 2.0f, wrY * 2.0f, (wIdx % 2 == 0) ? 2.0f : 1.0f);
            }
            g.setColour(RGBCol(253, 224, 71));
            for (int r = 0; r < 6; ++r) {
                float rayAngle = (float)r * 1.047197f + animPhase * 0.2f;
                float rx = (float)cavMidX + std::cos(rayAngle) * 130.0f;
                float ry = (float)cavMidY + std::sin(rayAngle) * 55.0f;
                g.drawLine((float)cavMidX, (float)cavMidY, rx, ry, 1.0f);
            }
            fillRoundRectJuce(g, cavMidX - 6, cavMidY - 6, cavMidX + 6, cavMidY + 6, 12, RGBCol(244, 114, 182), RGBCol(255, 255, 255), 1);
        } else {
            g.setColour(RGBCol(70, 20, 45));
            g.drawHorizontalLine(cavMidY, (float)(revBayX1 + 20), (float)(revBayX2 - 20));
        }

        g.setColour(revActive ? RGBCol(253, 224, 71) : RGBCol(148, 163, 184));
        char revBottomTxt[96];
        if (revActive) {
            std::snprintf(revBottomTxt, sizeof(revBottomTxt), "CAVITY: %s  |  SIZE: %s  |  DWELL: %.1f  |  MIX: %.0f%%",
                          kShapeNames[revShapeIdx], kSizeNames[revSizeIdx],
                          processor.getParamValue(K_REV_DWELL), processor.getParamValue(K_REV_MIX));
        } else {
            std::snprintf(revBottomTxt, sizeof(revBottomTxt), "REVERB ENGINE: BYPASSED (TRUE ZERO-BLEED DRY PASS)");
        }
        g.drawText(revBottomTxt, revBayX1 + 10, revBayY2 - 20, revBayX2 - revBayX1 - 20, 15, juce::Justification::centred, false);

        // LOWER RACK: FENDER 6G HARMONIC TREMOLO
        const juce::Image& imgTrem = imgRackTremOrange.isValid() ? imgRackTremOrange : imgChassisWide;
        if (imgTrem.isValid()) {
            g.drawImage(imgTrem, bx1, r2Y1, bx2 - bx1, r2Y2 - r2Y1, 0, 0, imgTrem.getWidth(), imgTrem.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, r2Y1, bx2, r2Y2, 14, RGBCol(45, 18, 5), RGBCol(249, 115, 22), 2);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(251, 146, 60));
        g.drawText("RACK UNIT 2  |  VINTAGE FENDER 6G HARMONIC TREMOLO", bx1 + 38, r2Y1 + 16, 520, 20, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(255, 237, 213));
        g.drawText("Fender 6G Harmonic Tremolo", bx1 + 38, r2Y1 + 34, 520, 30, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, 642, r2Y1 + 42, 16, RGBCol(194, 65, 12), RGBCol(249, 115, 22), RGBCol(254, 240, 138));

        int tremBayX1 = 680, tremBayX2 = 1128, tremBayY1 = r2Y1 + 48, tremBayY2 = r2Y2 - 18;
        fillRoundRectJuce(g, tremBayX1, tremBayY1, tremBayX2, tremBayY2, 8, RGBCol(18, 8, 4), RGBCol(249, 115, 22), 2);
        fillRoundRectJuce(g, tremBayX1 + 4, tremBayY1 + 4, tremBayX2 - 4, tremBayY2 - 4, 6, RGBCol(10, 4, 2), RGBCol(75, 30, 10), 1);

        fillRoundRectJuce(g, tremBayX1 + 6, tremBayY1 + 6, tremBayX1 + 11, tremBayY1 + 11, 4, RGBCol(251, 146, 60), RGBCol(124, 45, 18), 1);
        fillRoundRectJuce(g, tremBayX2 - 11, tremBayY1 + 6, tremBayX2 - 6, tremBayY1 + 11, 4, RGBCol(251, 146, 60), RGBCol(124, 45, 18), 1);
        fillRoundRectJuce(g, tremBayX1 + 6, tremBayY2 - 11, tremBayX1 + 11, tremBayY2 - 6, 4, RGBCol(251, 146, 60), RGBCol(124, 45, 18), 1);
        fillRoundRectJuce(g, tremBayX2 - 11, tremBayY2 - 11, tremBayX2 - 6, tremBayY2 - 6, 4, RGBCol(251, 146, 60), RGBCol(124, 45, 18), 1);

        int oscLeft = tremBayX1 + 16, oscRight = tremBayX2 - 16;
        int oscTop = tremBayY1 + 22, oscBottom = tremBayY2 - 24;
        int oscW = oscRight - oscLeft;
        int oscMidY = (oscTop + oscBottom) / 2;

        g.setColour(RGBCol(55, 24, 10));
        for (int gy = oscTop + 10; gy < oscBottom; gy += 16)
            g.drawHorizontalLine(gy, (float)(oscLeft + 4), (float)(oscRight - 4));
        for (int gx = oscLeft + 24; gx < oscRight; gx += 40)
            g.drawVerticalLine(gx, (float)(oscTop + 4), (float)(oscBottom - 4));

        bool tremActive  = (processor.getParamValue(K_VIB_ON) > 0.5f);
        float tremSpeed  = processor.getParamValue(K_VIB_SPEED);
        float tremDepth  = processor.getParamValue(K_VIB_DEPTH) * 0.10f;
        bool isHarmonic  = (processor.getParamValue(K_VIB_MODE) < 0.5f);

        if (tremActive) {
            juce::Path lowPath, highPath;
            for (int x = oscLeft; x <= oscRight; ++x) {
                float fx = (float)(x - oscLeft) / (float)oscW;
                float phase = fx * 6.2831853f * 3.0f + animPhase * (tremSpeed * 0.45f);
                float modLow  = std::sin(phase) * tremDepth * 18.0f;
                float phaseHi = isHarmonic ? (phase + 3.14159265f) : phase;
                float modHigh = std::sin(phaseHi) * tremDepth * 18.0f;
                if (x == oscLeft) {
                    lowPath.startNewSubPath((float)x, (float)oscMidY - modLow);
                    highPath.startNewSubPath((float)x, (float)oscMidY - modHigh);
                } else {
                    lowPath.lineTo((float)x, (float)oscMidY - modLow);
                    highPath.lineTo((float)x, (float)oscMidY - modHigh);
                }
            }
            g.setColour(RGBCol(249, 115, 22));
            g.strokePath(lowPath, juce::PathStrokeType(2.0f));
            g.setColour(RGBCol(253, 224, 71));
            g.strokePath(highPath, juce::PathStrokeType(2.0f));
        } else {
            g.setColour(RGBCol(90, 40, 15));
            g.drawHorizontalLine(oscMidY, (float)oscLeft, (float)oscRight);
        }

        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(tremActive ? RGBCol(254, 240, 138) : RGBCol(148, 163, 184));
        char tremRightLbl[128];
        if (tremActive) {
            std::snprintf(tremRightLbl, sizeof(tremRightLbl), "FENDER 6G TREMOLO: %s (%.1f Hz  |  DEPTH: %.1f/10)",
                          isHarmonic ? "HARMONIC 180 deg ANTI-PHASE TUBE" : "OPTO PHOTOCELL CHOP",
                          tremSpeed, processor.getParamValue(K_VIB_DEPTH));
        } else {
            std::snprintf(tremRightLbl, sizeof(tremRightLbl), "FENDER 6G TREMOLO: BYPASSED");
        }
        g.drawText(tremRightLbl, tremBayX1 + 10, tremBayY2 - 20, tremBayX2 - tremBayX1 - 20, 15, juce::Justification::centred, false);
    }
    else if (activePage == 6) {
        const juce::Image& imgDelay = imgRackDelayGreen.isValid() ? imgRackDelayGreen : imgChassisWide;
        if (imgDelay.isValid()) {
            g.drawImage(imgDelay, bx1, by1, bx2 - bx1, by2 - by1, 0, 0, imgDelay.getWidth(), imgDelay.getHeight());
        } else {
            fillRoundRectJuce(g, bx1, by1, bx2, by2, 18, RGBCol(16, 44, 30), RGBCol(34, 197, 94), 3);
        }

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(167, 243, 208));
        g.drawText("LIVE MIXER AUX SEND 3-4 STUDIO 2\" MAGNETIC FLUID TAPE ECHO & PING-PONG", bx1 + 38, by1 + 14, 840, 18, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(254, 252, 232));
        g.drawText("Euphoria Studio 15/30 IPS Tape Delay", bx1 + 38, by1 + 30, 840, 28, juce::Justification::centredLeft, false);

        drawFacetedJewelLamp(g, bx2 - 68, by1 + 36, 16, RGBCol(22, 101, 52), RGBCol(74, 222, 128), RGBCol(254, 240, 138));

        int grLeft = 152, grRight = 1128, grTop = 330, grBottom = by2 - 16;
        fillRoundRectJuce(g, grLeft, grTop, grRight, grBottom, 8, RGBCol(6, 20, 14), RGBCol(34, 197, 94), 2);
        fillRoundRectJuce(g, grLeft + 4, grTop + 4, grRight - 4, grBottom - 4, 6, RGBCol(3, 10, 7), RGBCol(16, 50, 30), 1);

        fillRoundRectJuce(g, grLeft + 7, grTop + 7, grLeft + 13, grTop + 13, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, grRight - 13, grTop + 7, grRight - 7, grTop + 13, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, grLeft + 7, grBottom - 13, grLeft + 13, grBottom - 7, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, grRight - 13, grBottom - 13, grRight - 7, grBottom - 7, 6, RGBCol(148, 163, 184), RGBCol(71, 85, 105), 1);

        fillRoundRectJuce(g, 510, grTop + 16, 770, grTop + 48, 4, RGBCol(2, 16, 12), RGBCol(20, 184, 166), 1);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(RGBCol(45, 212, 191));
        bool dlyActive = (processor.getParamValue(K_DLY_ON) > 0.5f);
        char vfdBuf[96];
        if (dlyActive)
            std::snprintf(vfdBuf, sizeof(vfdBuf), "15 IPS  |  REEL A: 00:24:18.6  |  WOW: %.1f%%", processor.getParamValue(K_DLY_FLUTTER));
        else
            std::snprintf(vfdBuf, sizeof(vfdBuf), "TAPE TRANSPORT: STOPPED (BYPASS)");
        g.drawText(vfdBuf, 510, grTop + 16, 260, 32, juce::Justification::centred, false);

        // Twin Studio Analog VU Meters
        auto drawVu = [&](int vx1, int vy1, int vx2, int vy2, float vuVal, const char* lbl, juce::Colour lblCol) {
            fillRoundRectJuce(g, vx1, vy1, vx2, vy2, 6, RGBCol(20, 24, 22), RGBCol(71, 85, 105), 2);
            fillRoundRectJuce(g, vx1 + 4, vy1 + 4, vx2 - 4, vy2 - 4, 4, RGBCol(254, 243, 199), RGBCol(180, 160, 120), 1);
            juce::Path arcP;
            arcP.addCentredArc((float)(vx1 + vx2) * 0.5f, (float)(vy2 - 6), 34.0f, 34.0f, 0.0f, -0.785f, 0.785f, true);
            g.setColour(RGBCol(40, 30, 20));
            g.strokePath(arcP, juce::PathStrokeType(2.0f));
            float needleAngle = -2.35f + std::clamp(vuVal, 0.0f, 1.0f) * 1.57f;
            float px = (float)(vx1 + vx2) * 0.5f;
            float py = (float)(vy2 - 6);
            g.setColour((vuVal > 0.7f) ? RGBCol(239, 68, 68) : RGBCol(20, 20, 20));
            g.drawLine(px, py, px + std::cos(needleAngle) * 38.0f, py + std::sin(needleAngle) * 38.0f, 2.0f);
            fillRoundRectJuce(g, (int)px - 4, (int)py - 4, (int)px + 4, (int)py + 4, 8, RGBCol(20, 20, 20), RGBCol(20, 20, 20), 1);
            g.setColour(lblCol);
            g.drawText(lbl, vx1, vy2 + 3, vx2 - vx1, 14, juce::Justification::centred, false);
        };
        drawVu(grLeft + 20, grTop + 65, grLeft + 118, grTop + 165, processor.inMeterRms.load() * 2.85f, "INPUT RMS VU", RGBCol(167, 243, 208));
        drawVu(grRight - 118, grTop + 65, grRight - 20, grTop + 165, processor.outMeterPeakL.load(), "REPRO VU", RGBCol(251, 191, 36));

        int reelY = (grTop + grBottom) / 2 + 10;
        int supplyX = 360, takeupX = 920, rr = 66, aluR = rr - 14;
        auto drawReel = [&](int rx, int ry) {
            fillRoundRectJuce(g, rx - rr, ry - rr, rx + rr, ry + rr, rr * 2, RGBCol(55, 28, 16), RGBCol(28, 14, 8), 2);
            fillRoundRectJuce(g, rx - aluR, ry - aluR, rx + aluR, ry + aluR, aluR * 2, RGBCol(180, 185, 190), RGBCol(220, 225, 230), 2);
            g.setColour(RGBCol(245, 245, 250));
            float phase = dlyActive ? animPhase : 0.0f;
            for (int sp = 0; sp < 3; ++sp) {
                float a = phase + sp * 2.094395f;
                g.drawLine((float)rx, (float)ry, (float)rx + std::cos(a) * (float)(aluR - 3), (float)ry + std::sin(a) * (float)(aluR - 3), 4.0f);
            }
            fillRoundRectJuce(g, rx - 14, ry - 14, rx + 14, ry + 14, 28, RGBCol(220, 38, 38), RGBCol(255, 255, 255), 2);
            fillRoundRectJuce(g, rx - 6, ry - 6, rx + 6, ry + 6, 12, RGBCol(30, 30, 35), RGBCol(180, 180, 185), 1);
        };
        drawReel(supplyX, reelY);
        drawReel(takeupX, reelY);

        int headX1 = 490, headX2 = 790, headY1 = reelY - 45, headY2 = reelY + 55;
        fillRoundRectJuce(g, headX1, headY1, headX2, headY2, 6, RGBCol(24, 28, 26), RGBCol(100, 116, 139), 2);
        fillRoundRectJuce(g, headX1 + 3, headY1 + 3, headX2 - 3, headY2 - 3, 4, RGBCol(16, 20, 18), RGBCol(40, 50, 45), 1);

        g.setColour(RGBCol(78, 38, 20));
        g.drawLine((float)supplyX, (float)(reelY + rr - 4), (float)(headX1 + 10), (float)(reelY + 6), 5.0f);
        g.drawLine((float)(headX1 + 10), (float)(reelY + 6), (float)(headX2 - 10), (float)(reelY + 6), 5.0f);
        g.drawLine((float)(headX2 - 10), (float)(reelY + 6), (float)takeupX, (float)(reelY + rr - 4), 5.0f);

        fillRoundRectJuce(g, 532, reelY - 14, 562, reelY + 22, 4, RGBCol(10, 10, 12), RGBCol(200, 170, 60), 2);
        g.setColour(RGBCol(251, 191, 36));
        g.drawLine(547.0f, (float)(reelY - 12), 547.0f, (float)(reelY + 20), 1.0f);

        fillRoundRectJuce(g, 625, reelY - 14, 655, reelY + 22, 4, RGBCol(203, 213, 225), RGBCol(239, 68, 68), 2);
        fillRoundRectJuce(g, 637, reelY - 8, 643, reelY - 2, 4, dlyActive ? RGBCol(239, 68, 68) : RGBCol(60, 12, 12), RGBCol(255, 255, 255), 1);

        fillRoundRectJuce(g, 718, reelY - 14, 748, reelY + 22, 4, RGBCol(148, 163, 184), RGBCol(34, 197, 94), 2);
        fillRoundRectJuce(g, 730, reelY - 8, 736, reelY - 2, 4, dlyActive ? RGBCol(34, 197, 94) : RGBCol(10, 50, 20), RGBCol(255, 255, 255), 1);

        fillRoundRectJuce(g, headX1 + 10, reelY - 8, headX1 + 24, reelY + 18, 14, RGBCol(203, 213, 225), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, headX1 + 15, reelY + 3, headX1 + 19, reelY + 7, 4, RGBCol(239, 68, 68), RGBCol(255, 255, 255), 1);
        fillRoundRectJuce(g, headX2 - 24, reelY - 8, headX2 - 10, reelY + 18, 14, RGBCol(203, 213, 225), RGBCol(71, 85, 105), 1);
        fillRoundRectJuce(g, headX2 - 19, reelY + 3, headX2 - 15, reelY + 7, 4, RGBCol(239, 68, 68), RGBCol(255, 255, 255), 1);
        fillRoundRectJuce(g, 760, reelY - 10, 776, reelY + 16, 14, RGBCol(18, 18, 20), RGBCol(60, 60, 65), 2);
        fillRoundRectJuce(g, 755, reelY - 4, 759, reelY + 10, 4, RGBCol(240, 240, 245), RGBCol(180, 180, 185), 1);

        g.setColour(dlyActive ? RGBCol(167, 243, 208) : RGBCol(148, 163, 184));
        char tapeStatusTxt[128];
        if (dlyActive) {
            std::snprintf(tapeStatusTxt, sizeof(tapeStatusTxt), "TAPE TRANSPORT: 15/30 IPS | TIME: %.0f ms | FEEDBACK: %.0f%% | DRIVE: %.0f%% | DUCK: %.0f%%",
                          processor.getParamValue(K_DLY_TIME), processor.getParamValue(K_DLY_FB),
                          processor.getParamValue(K_DLY_DRIVE), processor.getParamValue(K_DLY_DUCK));
        } else {
            std::snprintf(tapeStatusTxt, sizeof(tapeStatusTxt), "TAPE TRANSPORT: BYPASSED (TRUE ANALOG ZERO-BLEED PASS)");
        }
        g.drawText(tapeStatusTxt, grLeft + 20, grBottom - 22, grRight - grLeft - 40, 16, juce::Justification::centred, false);
    }
    else if (activePage == 7) {
        fillRoundRectJuce(g, bx1, by1, bx2, by2, 18, RGBCol(10, 16, 26), RGBCol(30, 41, 59), 3);
        drawCornerProtector3D(g, bx1, by1, 28, 28, 0, RGBCol(251, 191, 36), RGBCol(254, 240, 138));
        drawCornerProtector3D(g, bx2 - 28, by1, 28, 28, 1, RGBCol(251, 191, 36), RGBCol(254, 240, 138));
        drawCornerProtector3D(g, bx1, by2 - 28, 28, 28, 2, RGBCol(251, 191, 36), RGBCol(254, 240, 138));
        drawCornerProtector3D(g, bx2 - 28, by2 - 28, 28, 28, 3, RGBCol(251, 191, 36), RGBCol(254, 240, 138));

        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.setColour(RGBCol(56, 189, 248));
        g.drawText("LIVE MIXER PRECISION INSTRUMENT INFORMATICS  |  SUB-CENT ACCURACY (+/-0.05 ct)", bx1 + 38, by1 + 14, 840, 18, juce::Justification::centredLeft, false);

        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.setColour(RGBCol(241, 245, 249));
        g.drawText("Euphoria Pro 0.1-Cent Virtual Strobe Tuner", bx1 + 38, by1 + 30, 840, 28, juce::Justification::centredLeft, false);

        bool isMuted = processor.tunerMute.load();
        drawFacetedJewelLamp(g, bx2 - 68, by1 + 36, 16,
                             isMuted ? RGBCol(185, 28, 28) : RGBCol(14, 116, 144),
                             isMuted ? RGBCol(239, 68, 68) : RGBCol(56, 189, 248), RGBCol(255, 255, 255));

        int dispLeft = 148, dispTop = by1 + 80, dispRight = 1000, dispBottom = by2 - 16;
        fillRoundRectJuce(g, dispLeft, dispTop, dispRight, dispBottom, 10, RGBCol(4, 8, 14), RGBCol(14, 116, 144), 2);
        fillRoundRectJuce(g, dispLeft + 4, dispTop + 4, dispRight - 4, dispBottom - 4, 8, RGBCol(2, 4, 8), RGBCol(8, 47, 73), 1);

        float curRef   = processor.tuner.refFreq.load();
        float detPitch = processor.tuner.detectedPitch.load();
        float tgtPitch = processor.tuner.targetPitch.load();
        float cents    = processor.tuner.cents.load();
        float clarity  = processor.tuner.confidence.load();
        float inRms    = processor.tuner.inRms.load();
        bool sigActive = processor.tuner.signalActive.load();
        int nIdx       = processor.tuner.noteIdx.load();
        int octave     = processor.tuner.octave.load();
        char nName[8];
        processor.tuner.getNoteName(nName);
        float sRate    = processor.tuner.getSampleRate();

        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(RGBCol(56, 189, 248));
        char dispInfoBuf[180];
        std::snprintf(dispInfoBuf, sizeof(dispInfoBuf),
                      "REFERENCE: A4 = %.1f Hz  |  STABILIZED CHROMATIC YIN PITCH ENGINE  |  DEVICE RATE: %.0f Hz",
                      curRef, sRate);
        g.drawText(dispInfoBuf, dispLeft + 16, dispTop + 8, dispRight - dispLeft - 32, 20, juce::Justification::centred, false);

        bool inTune = sigActive && (std::abs(cents) <= 1.5f);
        static const char* const s_chromaticNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const bool s_isAccidental[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
        int ribLeft = dispLeft + 16, ribRight = dispRight - 16, ribWidth = ribRight - ribLeft;
        int ribTop = dispTop + 32, ribBot = dispTop + 68;

        for (int k = 0; k < 12; ++k) {
            int x1 = ribLeft + (k * ribWidth) / 12 + 2;
            int x2 = ribLeft + ((k + 1) * ribWidth) / 12 - 2;
            bool isCurrent = sigActive && (nIdx == k);
            juce::Colour bgCol = isCurrent ? (inTune ? RGBCol(6, 78, 59) : (cents < -1.5f ? RGBCol(12, 74, 110) : RGBCol(120, 53, 15)))
                                           : (s_isAccidental[k] ? RGBCol(6, 10, 18) : RGBCol(12, 18, 28));
            juce::Colour brCol = isCurrent ? (inTune ? RGBCol(52, 211, 153) : (cents < -1.5f ? RGBCol(56, 189, 248) : RGBCol(251, 191, 36)))
                                           : (s_isAccidental[k] ? RGBCol(23, 32, 48) : RGBCol(30, 41, 59));
            juce::Colour txCol = isCurrent ? RGBCol(255, 255, 255) : (s_isAccidental[k] ? RGBCol(100, 116, 139) : RGBCol(148, 163, 184));
            fillRoundRectJuce(g, x1, ribTop, x2, ribBot, 4, bgCol, brCol, isCurrent ? 2 : 1);
            g.setColour(txCol);
            g.drawText(s_chromaticNames[k], x1, ribTop, x2 - x1, ribBot - ribTop, juce::Justification::centred, false);
        }

        int centerDispX = (dispLeft + dispRight) / 2;
        int flatX1 = dispLeft + 16, flatX2 = flatX1 + 160, pY1 = dispTop + 74, pY2 = dispTop + 158;
        bool isFlat = sigActive && (cents < -1.5f);
        fillRoundRectJuce(g, flatX1, pY1, flatX2, pY2, 6, isFlat ? RGBCol(12, 74, 110) : RGBCol(8, 14, 22), isFlat ? RGBCol(56, 189, 248) : RGBCol(23, 32, 48), isFlat ? 2 : 1);
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.setColour(isFlat ? RGBCol(56, 189, 248) : RGBCol(51, 65, 85));
        g.drawText("< FLAT", flatX1, pY1 + 10, flatX2 - flatX1, 22, juce::Justification::centred, false);

        int liveX1 = flatX2 + 6, liveX2 = centerDispX - 145;
        fillRoundRectJuce(g, liveX1, pY1, liveX2, pY2, 6, RGBCol(10, 16, 26), RGBCol(30, 41, 59), 1);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(RGBCol(148, 163, 184));
        g.drawText("DETECTED", liveX1, pY1 + 10, liveX2 - liveX1, 18, juce::Justification::centred, false);
        char detHzBuf[32];
        if (sigActive) std::snprintf(detHzBuf, sizeof(detHzBuf), "%.2f Hz", detPitch);
        else std::strcpy(detHzBuf, "-- Hz");
        g.setColour(sigActive ? RGBCol(241, 245, 249) : RGBCol(100, 116, 139));
        g.drawText(detHzBuf, liveX1, pY1 + 32, liveX2 - liveX1, 24, juce::Justification::centred, false);

        int noteX1 = centerDispX - 140, noteX2 = centerDispX + 140;
        juce::Colour noteBorder = !sigActive ? RGBCol(51, 65, 85) : (inTune ? RGBCol(52, 211, 153) : (cents < 0 ? RGBCol(56, 189, 248) : RGBCol(251, 191, 36)));
        juce::Colour noteBg     = !sigActive ? RGBCol(15, 23, 42) : (inTune ? RGBCol(6, 44, 30) : (cents < 0 ? RGBCol(8, 35, 58) : RGBCol(50, 32, 10)));
        fillRoundRectJuce(g, noteX1, pY1, noteX2, pY2, 8, noteBg, noteBorder, 2);
        g.setFont(juce::FontOptions(38.0f, juce::Font::bold));
        g.setColour(noteBorder);
        char noteOctBuf[16];
        if (sigActive) std::snprintf(noteOctBuf, sizeof(noteOctBuf), "%s%d", nName, octave);
        else std::strcpy(noteOctBuf, "--");
        g.drawText(noteOctBuf, noteX1, pY1 + 2, noteX2 - noteX1, 46, juce::Justification::centred, false);

        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        char centsBuf[48];
        if (sigActive)
            std::snprintf(centsBuf, sizeof(centsBuf), "%+.1f CENTS %s", cents, inTune ? "[LOCKED]" : "");
        else
            std::strcpy(centsBuf, "AWAITING SIGNAL");
        g.setColour(sigActive ? RGBCol(254, 240, 138) : RGBCol(100, 116, 139));
        g.drawText(centsBuf, noteX1, pY1 + 52, noteX2 - noteX1, 24, juce::Justification::centred, false);

        int tgtX1 = centerDispX + 145, tgtX2 = ribRight - 166;
        fillRoundRectJuce(g, tgtX1, pY1, tgtX2, pY2, 6, RGBCol(10, 16, 26), RGBCol(30, 41, 59), 1);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(RGBCol(148, 163, 184));
        g.drawText("TARGET", tgtX1, pY1 + 10, tgtX2 - tgtX1, 18, juce::Justification::centred, false);
        char tgtHzBuf[32];
        if (sigActive) std::snprintf(tgtHzBuf, sizeof(tgtHzBuf), "%.2f Hz", tgtPitch);
        else std::strcpy(tgtHzBuf, "-- Hz");
        g.setColour(sigActive ? RGBCol(241, 245, 249) : RGBCol(100, 116, 139));
        g.drawText(tgtHzBuf, tgtX1, pY1 + 32, tgtX2 - tgtX1, 24, juce::Justification::centred, false);

        int sharpX1 = ribRight - 160, sharpX2 = ribRight;
        bool isSharp = sigActive && (cents > 1.5f);
        fillRoundRectJuce(g, sharpX1, pY1, sharpX2, pY2, 6, isSharp ? RGBCol(120, 53, 15) : RGBCol(8, 14, 22), isSharp ? RGBCol(251, 191, 36) : RGBCol(23, 32, 48), isSharp ? 2 : 1);
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.setColour(isSharp ? RGBCol(251, 191, 36) : RGBCol(51, 65, 85));
        g.drawText("SHARP >", sharpX1, pY1 + 10, sharpX2 - sharpX1, 22, juce::Justification::centred, false);

        int strobeX1 = dispLeft + 16, strobeX2 = dispRight - 16, strobeY1 = dispTop + 164, strobeY2 = dispTop + 316;
        fillRoundRectJuce(g, strobeX1, strobeY1, strobeX2, strobeY2, 6, RGBCol(3, 7, 12), RGBCol(20, 83, 45), 2);
        juce::Colour strobeLit = !sigActive ? RGBCol(20, 30, 45) : (inTune ? RGBCol(16, 185, 129) : (cents < 0 ? RGBCol(14, 165, 233) : RGBCol(249, 115, 22)));
        int bandH = (strobeY2 - strobeY1) / 4;
        static const float s_bandPeriods[4] = { 64.0f, 32.0f, 21.33f, 16.0f };
        g.setColour(strobeLit);
        for (int b = 0; b < 4; ++b) {
            int byTop = strobeY1 + b * bandH;
            int byBot = byTop + bandH - 2;
            float period = s_bandPeriods[b];
            float halfP = period * 0.5f;
            float shift = std::fmod(strobeWheelPhase * (b + 1) * 3.5f, period);
            if (shift < 0.0f) shift += period;
            for (float px = (float)strobeX1 - period; px < (float)strobeX2 + period; px += period) {
                int clL = std::max(strobeX1 + 1, (int)(px + shift));
                int clR = std::min(strobeX2 - 1, (int)(px + shift + halfP));
                if (clR > clL) g.fillRect(clL, byTop, clR - clL, byBot - byTop);
            }
        }
        g.setColour(inTune ? RGBCol(255, 255, 255) : RGBCol(239, 68, 68));
        g.drawVerticalLine(centerDispX, (float)(strobeY1 + 2), (float)(strobeY2 - 2));

        // Right Hardware Bay on Tuner Page
        int rBayLeft = 1018, rBayRight = bx2 - 20;
        fillRoundRectJuce(g, rBayLeft, dispTop, rBayRight, dispBottom, 8, RGBCol(8, 12, 18), RGBCol(30, 41, 59), 2);
        fillRoundRectJuce(g, kBtnTunerMuteRect.getX(), kBtnTunerMuteRect.getY(), kBtnTunerMuteRect.getRight(), kBtnTunerMuteRect.getBottom(), 8,
                          isMuted ? RGBCol(185, 28, 28) : RGBCol(6, 78, 59),
                          isMuted ? RGBCol(239, 68, 68) : RGBCol(52, 211, 153), 2);
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.setColour(RGBCol(255, 255, 255));
        g.drawText(isMuted ? "[ MUTE TO TUNE ]" : "[ AUDIO ACTIVE ]", kBtnTunerMuteRect, juce::Justification::centred, false);

        fillRoundRectJuce(g, kBtnTunerRefRect.getX(), kBtnTunerRefRect.getY(), kBtnTunerRefRect.getRight(), kBtnTunerRefRect.getBottom(), 6,
                          RGBCol(15, 23, 42), RGBCol(56, 189, 248), 1);
        g.setColour(RGBCol(254, 240, 138));
        g.drawText("REF: " + juce::String((int)curRef) + " Hz", kBtnTunerRefRect, juce::Justification::centred, false);

        bool isHiPrec = processor.tuner.highPrecisionMode.load();
        fillRoundRectJuce(g, kBtnTunerSpeedRect.getX(), kBtnTunerSpeedRect.getY(), kBtnTunerSpeedRect.getRight(), kBtnTunerSpeedRect.getBottom(), 6,
                          RGBCol(15, 23, 42), isHiPrec ? RGBCol(56, 189, 248) : RGBCol(16, 185, 129), 1);
        g.setColour(isHiPrec ? RGBCol(56, 189, 248) : RGBCol(52, 211, 153));
        g.drawText(isHiPrec ? "TRACKING: ULTRA HI-RES" : "TRACKING: CHROMATIC STD", kBtnTunerSpeedRect, juce::Justification::centred, false);

        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(RGBCol(148, 163, 184));
        char lockInfoBuf[64];
        std::snprintf(lockInfoBuf, sizeof(lockInfoBuf), "LOCK: %.0f%%  |  RMS: %.3f", clarity * 100.0f, inRms);
        g.drawText(lockInfoBuf, rBayLeft + 6, dispBottom - 28, rBayRight - rBayLeft - 12, 20, juce::Justification::centred, false);
    }

    // 4. Draw Knobs belonging to the Active Page
    for (int i = 0; i < (int)table.size(); ++i) {
        if (table[(size_t)i].page == activePage)
            drawStylizedKnob(g, i, activePage);
    }

    // 5. Bottom Live Meter Bar and Hardware Status
    float inPk = std::clamp(processor.inMeterPeak.load(), 0.0f, 1.0f);
    float inRms = std::clamp(processor.inMeterRms.load(), 0.0f, 1.0f);
    float inRmsDb = processor.inMeterRmsDb.load();
    float outL = std::clamp(processor.outMeterPeakL.load(), 0.0f, 1.0f);
    float outR = std::clamp(processor.outMeterPeakR.load(), 0.0f, 1.0f);

    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(RGBCol(204, 251, 241));
    char statusBuf[128];
    std::snprintf(statusBuf, sizeof(statusBuf), "EUPHORIA PURE FLUID ENGINE  |  SR: %.0f Hz  |  BUF: %d",
                  processor.getSampleRate(), processor.currentBlockSize.load());
    g.drawText(statusBuf, 22, h - 36, 418, 26, juce::Justification::centredLeft, false);

    int meterX = 450;
    g.setColour(RGBCol(45, 212, 191));
    g.drawText("GUITAR IN (RMS)", meterX, h - 34, 95, 24, juce::Justification::centredLeft, false);

    int inBarX = meterX + 98, inW = 85, inBarTop = h - 28, inBarBottom = h - 14;
    fillRoundRectJuce(g, inBarX, inBarTop, inBarX + inW, inBarBottom, 4, RGBCol(4, 24, 20), RGBCol(19, 78, 74), 1);
    // Sweet spot target: -22 dBFS to -12 dBFS on -48..0 dB scale (0.542 to 0.750 of bar width)
    int targetX1 = inBarX + (int)(inW * 0.542f);
    int targetX2 = inBarX + (int)(inW * 0.750f);
    fillRoundRectJuce(g, targetX1, inBarTop + 1, targetX2, inBarBottom - 1, 2, RGBCol(6, 44, 34), RGBCol(16, 185, 129), 1);
    if (inRmsDb > -48.0f) {
        float normRms = std::clamp((inRmsDb + 48.0f) / 48.0f, 0.0f, 1.0f);
        int fillW = (int)(inW * normRms);
        if (fillW > 2) {
            juce::Colour c = (inRmsDb > -6.0f || inPk > 0.95f) ? RGBCol(239, 68, 68)
                           : (inRmsDb > -12.0f)                ? RGBCol(245, 158, 11)
                           : (inRmsDb >= -24.0f)               ? RGBCol(16, 185, 129)
                                                               : RGBCol(20, 184, 166);
            fillRoundRectJuce(g, inBarX + 1, inBarTop + 1, inBarX + fillW, inBarBottom - 1, 3, c, c, 1);
        }
    }
    g.setColour(RGBCol(52, 211, 153));
    g.drawVerticalLine(targetX1, (float)(inBarTop - 2), (float)(inBarBottom + 2));
    g.drawVerticalLine(targetX2, (float)(inBarTop - 2), (float)(inBarBottom + 2));

    juce::Rectangle<int> badgeRect(inBarX + inW + 8, h - 34, 125, 24);
    juce::Colour badgeBg, badgeBorder, badgeText;
    char badgeBuf[48];
    if (inRmsDb < -48.0f) {
        badgeBg = RGBCol(10, 20, 22); badgeBorder = RGBCol(30, 50, 55); badgeText = RGBCol(100, 116, 139);
        std::snprintf(badgeBuf, sizeof(badgeBuf), "INPUT: IDLE");
    } else if (inRmsDb < -24.0f) {
        badgeBg = RGBCol(34, 25, 8); badgeBorder = RGBCol(180, 130, 20); badgeText = RGBCol(251, 191, 36);
        std::snprintf(badgeBuf, sizeof(badgeBuf), "LOW: %.0fdB", inRmsDb);
    } else if (inRmsDb <= -12.0f) {
        badgeBg = RGBCol(4, 52, 40); badgeBorder = RGBCol(52, 211, 153); badgeText = RGBCol(254, 240, 138);
        std::snprintf(badgeBuf, sizeof(badgeBuf), "* SWEET %.0fdB *", inRmsDb);
    } else if (inRmsDb <= -6.0f) {
        badgeBg = RGBCol(50, 35, 10); badgeBorder = RGBCol(245, 158, 11); badgeText = RGBCol(254, 215, 170);
        std::snprintf(badgeBuf, sizeof(badgeBuf), "HOT: %.0fdB", inRmsDb);
    } else {
        badgeBg = RGBCol(70, 15, 15); badgeBorder = RGBCol(239, 68, 68); badgeText = RGBCol(254, 202, 202);
        std::snprintf(badgeBuf, sizeof(badgeBuf), "! CLIP %.0fdB !", inRmsDb);
    }
    fillRoundRectJuce(g, badgeRect.getX(), badgeRect.getY(), badgeRect.getRight(), badgeRect.getBottom(), 4, badgeBg, badgeBorder, 1);
    g.setColour(badgeText);
    g.drawText(badgeBuf, badgeRect, juce::Justification::centred, false);

    // Tuner Status Pill
    char tunerPillBuf[64];
    juce::Colour tPillBg, tPillBorder, tPillText;
    if (activePage == 7) {
        tPillBg = RGBCol(12, 74, 110); tPillBorder = RGBCol(56, 189, 248); tPillText = RGBCol(224, 242, 254);
        std::snprintf(tunerPillBuf, sizeof(tunerPillBuf), "[ TUNER: ACTIVE ]");
    } else if (processor.tuner.signalActive.load()) {
        char pn[8]; processor.tuner.getNoteName(pn);
        float ct = processor.tuner.cents.load();
        bool it = (std::abs(ct) <= 1.5f);
        tPillBg = it ? RGBCol(6, 78, 59) : RGBCol(15, 23, 42);
        tPillBorder = it ? RGBCol(52, 211, 153) : RGBCol(56, 189, 248);
        tPillText = it ? RGBCol(254, 240, 138) : RGBCol(224, 242, 254);
        std::snprintf(tunerPillBuf, sizeof(tunerPillBuf), "TUNER: %s%d %+.1fct", pn, processor.tuner.octave.load(), ct);
    } else {
        tPillBg = RGBCol(15, 23, 42); tPillBorder = RGBCol(51, 65, 85); tPillText = RGBCol(148, 163, 184);
        std::snprintf(tunerPillBuf, sizeof(tunerPillBuf), "TUNER: IDLE");
    }
    fillRoundRectJuce(g, kTunerStatusPill.getX(), kTunerStatusPill.getY(), kTunerStatusPill.getRight(), kTunerStatusPill.getBottom(), 4, tPillBg, tPillBorder, 1);
    g.setColour(tPillText);
    g.drawText(tunerPillBuf, kTunerStatusPill, juce::Justification::centred, false);

    // Master Output L/R Meters
    int outMeterX = kTunerStatusPill.getRight() + 14;
    g.setColour(RGBCol(251, 191, 36));
    g.drawText("MASTER OUT", outMeterX, h - 34, 74, 24, juce::Justification::centredLeft, false);

    int outW = 95, outBarX = outMeterX + 78;
    fillRoundRectJuce(g, outBarX, h - 33, outBarX + outW, h - 23, 4, RGBCol(6, 34, 28), RGBCol(19, 78, 74), 1);
    fillRoundRectJuce(g, outBarX, h - 19, outBarX + outW, h - 9,  4, RGBCol(6, 34, 28), RGBCol(19, 78, 74), 1);
    if (outL > 0.005f)
        fillRoundRectJuce(g, outBarX, h - 33, outBarX + (int)(outW * std::min(1.0f, std::sqrt(outL))), h - 23, 4, RGBCol(245, 158, 11), RGBCol(245, 158, 11), 1);
    if (outR > 0.005f)
        fillRoundRectJuce(g, outBarX, h - 19, outBarX + (int)(outW * std::min(1.0f, std::sqrt(outR))), h - 9,  4, RGBCol(245, 158, 11), RGBCol(245, 158, 11), 1);

    // Interactive Master Output Volume Control [-] [VOL: 5.0] [+]
    int mstrX = outBarX + outW + 12;
    g.setColour(RGBCol(251, 191, 36));
    g.drawText("VOL", mstrX, h - 34, 26, 24, juce::Justification::centredLeft, false);

    fillRoundRectJuce(g, kMasterVolMinusRect.getX(), kMasterVolMinusRect.getY(),
                      kMasterVolMinusRect.getRight(), kMasterVolMinusRect.getBottom(), 3,
                      RGBCol(15, 23, 42), RGBCol(245, 158, 11), 1);
    g.setColour(RGBCol(254, 240, 138));
    g.drawText("-", kMasterVolMinusRect, juce::Justification::centred, false);

    float curMstr = processor.getParamValue(K_CLEAN_OUT);
    fillRoundRectJuce(g, kMasterVolPillRect.getX(), kMasterVolPillRect.getY(),
                      kMasterVolPillRect.getRight(), kMasterVolPillRect.getBottom(), 3,
                      RGBCol(10, 20, 30), RGBCol(245, 158, 11), 1);
    int pillW = kMasterVolPillRect.getWidth() - 4;
    int mstrFill = (int)(pillW * std::clamp(curMstr / 10.0f, 0.0f, 1.0f));
    if (mstrFill > 2 && curMstr > 0.05f) {
        fillRoundRectJuce(g, kMasterVolPillRect.getX() + 2, kMasterVolPillRect.getY() + 2,
                          kMasterVolPillRect.getX() + 2 + mstrFill, kMasterVolPillRect.getBottom() - 2, 2,
                          RGBCol(70, 45, 10), RGBCol(180, 110, 20), 1);
    }
    char mstrBuf[32];
    if (curMstr <= 0.05f) {
        std::snprintf(mstrBuf, sizeof(mstrBuf), "MUTE");
        g.setColour(RGBCol(239, 68, 68));
    } else {
        std::snprintf(mstrBuf, sizeof(mstrBuf), "VOL: %.1f", curMstr);
        g.setColour(RGBCol(254, 240, 138));
    }
    g.drawText(mstrBuf, kMasterVolPillRect, juce::Justification::centred, false);

    fillRoundRectJuce(g, kMasterVolPlusRect.getX(), kMasterVolPlusRect.getY(),
                      kMasterVolPlusRect.getRight(), kMasterVolPlusRect.getBottom(), 3,
                      RGBCol(15, 23, 42), RGBCol(245, 158, 11), 1);
    g.setColour(RGBCol(254, 240, 138));
    g.drawText("+", kMasterVolPlusRect, juce::Justification::centred, false);

    // =========================================================================
    // MIDI LEARN & STATUS HUD OVERLAYS
    // =========================================================================
    if (processor.midiMap.isLearning()) {
        int lIdx = processor.midiMap.getLearningParam();
        const auto& table = euphoriaParamTable();
        juce::String pName = (lIdx >= 0 && lIdx < (int)table.size()) ? table[(size_t)lIdx].label : "Control";

        juce::Rectangle<int> r(0, 0, 0, 0);
        if (lIdx == K_KLON_ON) r = juce::Rectangle<int>(204 - 45, 435, 90, 120);
        else if (lIdx == K_TS_ON) r = juce::Rectangle<int>(422 - 45, 435, 90, 120);
        else if (lIdx == K_BD_ON) r = juce::Rectangle<int>(640 - 45, 435, 90, 120);
        else if (lIdx == K_RAT_ON) r = juce::Rectangle<int>(858 - 45, 435, 90, 120);
        else if (lIdx == K_FUZZ_ON) r = juce::Rectangle<int>(1076 - 45, 435, 90, 120);
        else if (lIdx == K_CLEAN_OUT) r = kMasterVolPillRect;
        else if (lIdx >= 0 && lIdx < (int)knobLayout.size()) r = knobLayout[(size_t)lIdx].rect;

        if (!r.isEmpty() && (lIdx < 6 || knobLayout[(size_t)lIdx].pageIdx == activePage || (lIdx >= K_KLON_ON && lIdx <= K_FUZZ_HYPER && activePage == 3))) {
            float pulse = 0.6f + 0.4f * std::sin(animPhase * 4.0f);
            g.setColour(juce::Colour::fromFloatRGBA(0.0f, 0.95f, 1.0f, pulse));
            g.drawRoundedRectangle(r.toFloat().expanded(4.0f), 8.0f, 3.0f);
        }

        int bw = 720, bh = 34;
        int bx = (1280 - bw) / 2, by = 672;
        fillRoundRectJuce(g, bx, by, bx + bw, by + bh, 8, RGBCol(10, 25, 40), RGBCol(0, 220, 255), 2);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(RGBCol(0, 240, 255));
        g.drawText("[MIDI LEARN ACTIVE] Move any knob, slider, or press a footswitch to assign to '" + pName + "' (Click or Esc to cancel)",
                   bx, by, bw, bh, juce::Justification::centred, false);
    }
    else if (processor.midiMap.getAssignedFlashTimer() > 0) {
        processor.midiMap.decrementFlashTimer();
        int aIdx = processor.midiMap.getLastAssignedParam();
        int aCc = processor.midiMap.getLastAssignedCc();
        const auto& table = euphoriaParamTable();
        juce::String pName = (aIdx >= 0 && aIdx < (int)table.size()) ? table[(size_t)aIdx].label : "Control";

        int bw = 540, bh = 34;
        int bx = (1280 - bw) / 2, by = 672;
        fillRoundRectJuce(g, bx, by, bx + bw, by + bh, 8, RGBCol(12, 42, 20), RGBCol(52, 211, 153), 2);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.setColour(RGBCol(167, 243, 208));
        g.drawText("Assigned MIDI CC #" + juce::String(aCc) + " to '" + pName + "'",
                   bx, by, bw, bh, juce::Justification::centred, false);
    }
}

bool EuphoriaAmpAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (key.isKeyCode(juce::KeyPress::escapeKey)) {
        if (processor.midiMap.isLearning()) {
            processor.midiMap.cancelLearn();
            repaint();
            return true;
        }
    }
    return false;
}

void EuphoriaAmpAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    const auto pt = toVirtualPt(e.position);

    if (processor.midiMap.isLearning()) {
        processor.midiMap.cancelLearn();
        repaint();
        return;
    }

    if (e.mods.isRightButtonDown()) {
        int targetParam = -1;

        if (activePage == 3) {
            const juce::Rectangle<int> rKlonFoot(204 - 45, 435, 90, 120);
            const juce::Rectangle<int> rTsFoot(422 - 45, 435, 90, 120);
            const juce::Rectangle<int> rBdFoot(640 - 45, 435, 90, 120);
            const juce::Rectangle<int> rRatFoot(858 - 45, 435, 90, 120);
            const juce::Rectangle<int> rFuzzFoot(1076 - 45, 435, 90, 120);
            if (rKlonFoot.contains(pt)) targetParam = K_KLON_ON;
            else if (rTsFoot.contains(pt)) targetParam = K_TS_ON;
            else if (rBdFoot.contains(pt)) targetParam = K_BD_ON;
            else if (rRatFoot.contains(pt)) targetParam = K_RAT_ON;
            else if (rFuzzFoot.contains(pt)) targetParam = K_FUZZ_ON;
        }

        if (targetParam < 0 && (kMasterVolPillRect.contains(pt) || kMasterVolMinusRect.contains(pt) || kMasterVolPlusRect.contains(pt))) {
            targetParam = K_CLEAN_OUT;
        }

        if (targetParam < 0) {
            const auto& table = euphoriaParamTable();
            for (int i = 0; i < (int)table.size(); ++i) {
                if (knobLayout[(size_t)i].pageIdx != -1 && knobLayout[(size_t)i].pageIdx != activePage) continue;
                if (knobLayout[(size_t)i].rect.contains(pt)) {
                    targetParam = i;
                    break;
                }
            }
        }

        if (targetParam >= 0) {
            const auto& table = euphoriaParamTable();
            const char* label = (targetParam < (int)table.size()) ? table[(size_t)targetParam].label : "Control";
            int currentCc = processor.midiMap.getCcForParam(targetParam);

            juce::PopupMenu menu;
            menu.addSectionHeader("MIDI Control: " + juce::String(label));
            menu.addSeparator();

            if (currentCc >= 0) {
                menu.addItem(1, "MIDI Learn (Currently: CC #" + juce::String(currentCc) + ")");
                menu.addItem(2, "Clear MIDI CC #" + juce::String(currentCc));
            } else {
                menu.addItem(1, "MIDI Learn (Move knob or press footswitch)");
            }

            if (knobLayout[(size_t)targetParam].isStepped) {
                menu.addSeparator();
                menu.addItem(10, "Cycle Previous Step");
            }

            menu.addSeparator();
            menu.addItem(3, "Clear All MIDI Mappings");

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
                juce::Rectangle<int>(e.getScreenPosition().x, e.getScreenPosition().y, 1, 1)),
                [this, targetParam](int result) {
                    if (result == 1) {
                        processor.midiMap.startLearn(targetParam);
                        repaint();
                    } else if (result == 2) {
                        processor.midiMap.clearBinding(targetParam);
                        processor.midiMap.saveToFile(processor.getMidiConfigFile().getFullPathName().toStdString());
                        repaint();
                    } else if (result == 3) {
                        processor.midiMap.clearAllBindings();
                        processor.midiMap.saveToFile(processor.getMidiConfigFile().getFullPathName().toStdString());
                        repaint();
                    } else if (result == 10) {
                        int steps = knobLayout[(size_t)targetParam].steps;
                        int prev = ((int)std::lround(processor.getParamValue(targetParam)) - 1 + steps) % steps;
                        processor.setParamValueFromGui(targetParam, (float)prev);
                        repaint();
                    }
                });
            return;
        }
        return;
    }

    for (int t = 0; t < 6; ++t) {
        if (kTabRects[t].contains(pt)) {
            if (t == 0) {
                int curCh = std::clamp((int)std::lround(processor.getParamValue(K_CH_MODE)), 0, 3);
                int targetAmpPage = (curCh == 0) ? 0 : (curCh == 1) ? 1 : (curCh == 2) ? 2 : 1;
                if (activePage >= 3) {
                    activePage = targetAmpPage;
                } else {
                    int nextCh = (curCh + 1) % 3;
                    processor.setParamValueFromGui(K_CH_MODE, (float)nextCh);
                    activePage = nextCh;
                }
            } else {
                activePage = t + 2;
            }
            repaint();
            return;
        }
    }
    if (kTunerStatusPill.contains(pt)) {
        int curCh = std::clamp((int)std::lround(processor.getParamValue(K_CH_MODE)), 0, 3);
        int targetAmpPage = (curCh == 0) ? 0 : (curCh == 1) ? 1 : (curCh == 2) ? 2 : 1;
        activePage = (activePage == 7) ? targetAmpPage : 7;
        repaint();
        return;
    }
    if (kPresetPrevRect.contains(pt)) {
        if (!processor.presets.empty()) {
            int n = (int)processor.presets.size();
            processor.applyPreset((processor.currentPresetIdx - 1 + n) % n);
            if (activePage < 3) {
                int curCh = std::clamp((int)std::lround(processor.getParamValue(K_CH_MODE)), 0, 3);
                activePage = (curCh == 0) ? 0 : (curCh == 2 ? 2 : 1);
            }
            repaint();
        }
        return;
    }
    if (kPresetNextRect.contains(pt)) {
        if (!processor.presets.empty()) {
            int n = (int)processor.presets.size();
            processor.applyPreset((processor.currentPresetIdx + 1) % n);
            if (activePage < 3) {
                int curCh = std::clamp((int)std::lround(processor.getParamValue(K_CH_MODE)), 0, 3);
                activePage = (curCh == 0) ? 0 : (curCh == 2 ? 2 : 1);
            }
            repaint();
        }
        return;
    }
    if (kPresetNameRect.contains(pt)) {
        juce::PopupMenu m;
        for (int i = 0; i < (int)processor.presets.size(); ++i)
            m.addItem(i + 1, processor.presets[(size_t)i].name, true, i == processor.currentPresetIdx);
        m.showMenuAsync(juce::PopupMenu::Options(), [this](int res) {
            if (res > 0) {
                processor.applyPreset(res - 1);
                if (activePage < 3) {
                    int curCh = std::clamp((int)std::lround(processor.getParamValue(K_CH_MODE)), 0, 3);
                    activePage = (curCh == 0) ? 0 : (curCh == 2 ? 2 : 1);
                }
                repaint();
            }
        });
        return;
    }
    if (kPresetSaveRect.contains(pt)) {
        auto* aw = new juce::AlertWindow("Save Euphoria Preset", "Enter a name for your custom preset:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor("name", (!processor.presets.empty() ? processor.presets[(size_t)processor.currentPresetIdx].name : "Custom Tone"));
        aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int res) {
            if (res == 1) {
                processor.saveUserPreset(aw->getTextEditorContents("name"));
                repaint();
            }
            delete aw;
        }), false);
        return;
    }
    if (kLearnNoiseBtnRect.contains(pt)) {
        learnNoiseStartTime = juce::Time::getMillisecondCounter();
        if (auto* eng = processor.getEngine()) eng->triggerLearnNoise();
        repaint();
        return;
    }
    if (kRiffBtnRect.contains(pt)) {
        processor.testRiffActive.store(!processor.testRiffActive.load());
        repaint();
        return;
    }
    if (kAudioSettingsRect.contains(pt)) {
        if (auto* top = getTopLevelComponent()) {
            for (int c = 0; c < top->getNumChildComponents(); ++c) {
                if (auto* btn = dynamic_cast<juce::Button*>(top->getChildComponent(c))) {
                    btn->triggerClick();
                    return;
                }
            }
        }
        return;
    }

    if (activePage == 3) {
        auto* eng = processor.getEngine();
        if (kBtnFuzzLearnMaxRect.contains(pt) && eng) {
            fuzzLearnStartTime = juce::Time::getMillisecondCounter();
            if (eng->isFuzzFaceLearningMax()) eng->finishFuzzFaceLearning();
            else eng->triggerFuzzFaceLearnMax();
            repaint();
            return;
        }
        if (kBtnFuzzLearnCleanRect.contains(pt) && eng) {
            fuzzLearnStartTime = juce::Time::getMillisecondCounter();
            if (eng->isFuzzFaceLearningClean()) eng->finishFuzzFaceLearning();
            else eng->triggerFuzzFaceLearnClean();
            repaint();
            return;
        }

        const juce::Rectangle<int> rKlonFoot(204 - 45, 435, 90, 120);
        if (rKlonFoot.contains(pt)) {
            processor.setParamValueFromGui(K_KLON_ON, processor.getParamValue(K_KLON_ON) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
        const juce::Rectangle<int> rTsFoot(422 - 45, 435, 90, 120);
        if (rTsFoot.contains(pt)) {
            processor.setParamValueFromGui(K_TS_ON, processor.getParamValue(K_TS_ON) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
        const juce::Rectangle<int> rBdFoot(640 - 45, 435, 90, 120);
        if (rBdFoot.contains(pt)) {
            processor.setParamValueFromGui(K_BD_ON, processor.getParamValue(K_BD_ON) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
        const juce::Rectangle<int> rRatFoot(858 - 45, 435, 90, 120);
        if (rRatFoot.contains(pt)) {
            processor.setParamValueFromGui(K_RAT_ON, processor.getParamValue(K_RAT_ON) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
        const juce::Rectangle<int> rFuzzFoot(1076 - 45, 435, 90, 120);
        if (rFuzzFoot.contains(pt)) {
            processor.setParamValueFromGui(K_FUZZ_ON, processor.getParamValue(K_FUZZ_ON) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
    }

    if (activePage == 7) {
        if (kBtnTunerMuteRect.contains(pt)) {
            processor.tunerMute.store(!processor.tunerMute.load());
            repaint();
            return;
        }
        if (kBtnTunerRefRect.contains(pt)) {
            float curRef = processor.tuner.refFreq.load();
            processor.tuner.refFreq.store((curRef > 436.0f) ? 432.0f : 440.0f);
            repaint();
            return;
        }
        if (kBtnTunerSpeedRect.contains(pt)) {
            processor.tuner.highPrecisionMode.store(!processor.tuner.highPrecisionMode.load());
            repaint();
            return;
        }
    }

    if (kMasterVolMinusRect.contains(pt)) {
        float curVal = processor.getParamValue(K_CLEAN_OUT);
        processor.setParamValueFromGui(K_CLEAN_OUT, std::clamp(curVal - 0.5f, 0.0f, 10.0f));
        repaint();
        return;
    }
    if (kMasterVolPlusRect.contains(pt)) {
        float curVal = processor.getParamValue(K_CLEAN_OUT);
        processor.setParamValueFromGui(K_CLEAN_OUT, std::clamp(curVal + 0.5f, 0.0f, 10.0f));
        repaint();
        return;
    }
    if (kMasterVolPillRect.contains(pt)) {
        dragKnobIdx  = K_CLEAN_OUT;
        dragStartY   = pt.y;
        dragStartVal = processor.getParamValue(K_CLEAN_OUT);
        dragDidMove  = false;
        return;
    }

    for (int i = 0; i < (int)knobLayout.size(); ++i) {
        if (knobLayout[(size_t)i].pageIdx != -1 && knobLayout[(size_t)i].pageIdx != activePage) continue;
        if (knobLayout[(size_t)i].rect.contains(pt)) {
            dragKnobIdx  = i;
            dragStartY   = pt.y;
            dragStartVal = processor.getParamValue(i);
            dragDidMove  = false;
            return;
        }
    }
}

void EuphoriaAmpAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragKnobIdx < 0) return;
    const auto pt = toVirtualPt(e.position);
    int dy = dragStartY - pt.y;
    if (std::abs(dy) >= 4) dragDidMove = true;

    const auto& k = knobLayout[(size_t)dragKnobIdx];
    const auto& d = euphoriaParamTable()[(size_t)dragKnobIdx];
    if (k.isStepped) {
        int stepDelta = dy / 8;
        int targetStep = std::clamp((int)std::lround(dragStartVal) + stepDelta, 0, k.steps - 1);
        if ((int)std::lround(processor.getParamValue(dragKnobIdx)) != targetStep) {
            processor.setParamValueFromGui(dragKnobIdx, (float)targetStep);
            if (dragKnobIdx == K_CH_MODE && activePage < 3) {
                activePage = (targetStep == 0) ? 0 : (targetStep == 2 ? 2 : 1);
            }
            repaint();
        }
    } else {
        float range = d.maxVal - d.minVal;
        float newVal = std::clamp(dragStartVal + ((float)dy / 140.0f) * range, d.minVal, d.maxVal);
        processor.setParamValueFromGui(dragKnobIdx, newVal);
        repaint();
    }
}

void EuphoriaAmpAudioProcessorEditor::mouseUp(const juce::MouseEvent&) {
    if (dragKnobIdx >= 0) {
        const auto& k = knobLayout[(size_t)dragKnobIdx];
        if (k.isStepped && !dragDidMove) {
            int next = ((int)std::lround(processor.getParamValue(dragKnobIdx)) + 1) % k.steps;
            processor.setParamValueFromGui(dragKnobIdx, (float)next);
            if (dragKnobIdx == K_CH_MODE && activePage < 3) {
                activePage = (next == 0) ? 0 : (next == 2 ? 2 : 1);
            }
            repaint();
        }
        dragKnobIdx = -1;
        dragDidMove = false;
    }
}

void EuphoriaAmpAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto pt = toVirtualPt(e.position);
    if (kMasterVolPillRect.contains(pt) || kMasterVolMinusRect.contains(pt) || kMasterVolPlusRect.contains(pt)) {
        processor.setParamValueFromGui(K_CLEAN_OUT, 5.0f);
        repaint();
        return;
    }
    const auto& table = euphoriaParamTable();
    for (int i = 0; i < (int)knobLayout.size(); ++i) {
        if (knobLayout[(size_t)i].pageIdx != -1 && knobLayout[(size_t)i].pageIdx != activePage) continue;
        if (knobLayout[(size_t)i].rect.contains(pt)) {
            processor.setParamValueFromGui(i, table[(size_t)i].defVal);
            if (i == K_CH_MODE && activePage < 3) {
                int defCh = (int)std::lround(table[i].defVal);
                activePage = (defCh == 0) ? 0 : (defCh == 2 ? 2 : 1);
            }
            repaint();
            return;
        }
    }
}

void EuphoriaAmpAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    const auto pt = toVirtualPt(e.position);
    if (kMasterVolPillRect.contains(pt)) {
        float curVal = processor.getParamValue(K_CLEAN_OUT);
        processor.setParamValueFromGui(K_CLEAN_OUT, std::clamp(curVal + (wheel.deltaY > 0.0f ? 0.2f : -0.2f), 0.0f, 10.0f));
        repaint();
        return;
    }
    const auto& table = euphoriaParamTable();
    for (int i = 0; i < (int)knobLayout.size(); ++i) {
        if (knobLayout[(size_t)i].pageIdx != -1 && knobLayout[(size_t)i].pageIdx != activePage) continue;
        if (knobLayout[(size_t)i].rect.contains(pt)) {
            const auto& k = knobLayout[(size_t)i];
            const auto& d = table[(size_t)i];
            if (k.isStepped) {
                int delta = (wheel.deltaY > 0.0f) ? 1 : ((wheel.deltaY < 0.0f) ? -1 : 0);
                int next = std::clamp((int)std::lround(processor.getParamValue(i)) + delta, 0, k.steps - 1);
                processor.setParamValueFromGui(i, (float)next);
                if (i == K_CH_MODE && activePage < 3) {
                    activePage = (next == 0) ? 0 : (next == 2 ? 2 : 1);
                }
            } else {
                float range = d.maxVal - d.minVal;
                float newVal = std::clamp(processor.getParamValue(i) + wheel.deltaY * 0.08f * range, d.minVal, d.maxVal);
                processor.setParamValueFromGui(i, newVal);
            }
            repaint();
            return;
        }
    }
}
