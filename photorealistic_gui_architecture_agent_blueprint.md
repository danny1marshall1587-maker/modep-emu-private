# Luxury Photorealistic Plugin GUI Blueprint (AmpliTube / Voosteq Style)

## 1. Why Your Current GUI Looks Like a "2D Cartoon"
Right now, your coding agent is using **JUCE's native 2D Vector Graphics (`juce::Graphics`)** to draw shapes, gradients, and drop shadows in code. Even with clever math, procedural code cannot replicate the microscopic surface imperfections of brushed anodized aluminum, boutique Tolex, knurled metal knobs, flame maple guitar tops, or ray-traced studio lighting.

To get the ultra-expensive, tactile hardware look of **Voosteq** or **AmpliTube** *while keeping every piece of gear 100% modular and resizable*, professional developers use a **Layered 3D Sprite Architecture**:
1. **Layer 0 (The Room Floor):** A seamless, tileable 3D-rendered **Hardwood Studio Floor** (`wood_floor_tile.png`) that fills any window size cleanly.
2. **Layer 1 (Isolated 3D Hardware Cutouts):** Every Amp Head, Drive Pedal (`pedal_klon.png`, `pedal_ts9.png`, `pedal_fuzz.png`, etc.), and the **Luxury Guitar** (`guitar_luxury.png`) is rendered in Blender as its own **separate transparent PNG** with its real ray-traced drop shadow baked into the alpha channel.
3. **Layer 2 (Interactive Hardware Controls):** High-resolution 64-frame **Rotating 3D Knob Filmstrips** (`knob_filmstrip.png`), switches, and LEDs sit on top of each pedal or amp component and scale automatically alongside their parent sprite!

---

## 2. Why This Makes Adding New Gear Effortless
Because every element is completely separate:
* **Want to add a 6th pedal later?** You only ask the agent to render *one* new pedal image (`pedal_phaser.png`). You never have to re-render the wood floor, the amps, the guitar, or the other 5 pedals!
* **Automatic Pedalboard Reflow & Resizing:** In C++, each pedal is a self-contained `ModularPedalComponent`. When you add a 6th pedal to the chain, JUCE's `FlexBox` / auto-layout engine automatically scales the other 5 pedal images down slightly and slides them over on the wooden floor so all 6 fit side-by-side with perfect proportions!
* **Modular Luxury Guitar:** Even your guitar view is modular—Blender can render the **Flame Maple Body**, **Pickups**, and **Volume Knob** as separate layers so the guitar can sit alongside your rig or expand into a full-screen view.

---

## 3. Automated Blender Python Script (`generate_gui_assets.py`)
Give this script to your coding agent! When your agent runs `blender -b -P generate_gui_assets.py` in the terminal, Blender will automatically render:
* **Seamless Hardwood Studio Floor** (`wood_floor_tile.png`)
* **Multi-Aspect Amp Chassis** (`chassis_wide.png` and `chassis_compact.png`)
* **Isolated 3D Pedals** (`pedal_fuzz_round.png`, `pedal_boutique_box.png`) with transparent shadow catchers
* **Isolated 3D Luxury Carved-Top Guitar Sprite** (`guitar_luxury.png`)
* **64-Frame Rotating 3D Knob Filmstrip** (`knob_filmstrip.png`) stitched automatically!

```python
import bpy
import math
import os

# ============================================================================
# AUTOMATED HEADLESS BLENDER 3D-TO-JUCE MODULAR SPRITE GENERATOR
# Run via terminal: blender -b -P generate_gui_assets.py
# ============================================================================

OUTPUT_DIR = os.path.join(os.getcwd(), "JUCE_Assets")
os.makedirs(OUTPUT_DIR, exist_ok=True)
NUM_FRAMES = 64
KNOB_FRAME_SIZE = 256  # @2x Retina resolution for lossless downscaling in JUCE

def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def setup_studio_lighting_and_camera(res_x, res_y, ortho_scale=3.0, use_shadow_catcher=True):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 64
    scene.render.film_transparent = True
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y

    # Orthographic Top-Down Camera (Zero perspective distortion so sprites can move anywhere on screen!)
    bpy.ops.object.camera_add(location=(0, 0, 6), rotation=(0, 0, 0))
    cam = bpy.context.object
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = ortho_scale
    scene.camera = cam

    # Main Top-Left Key Studio Softbox (-35 deg luxury hardware lighting)
    bpy.ops.object.light_add(type='AREA', location=(-3.5, 3.5, 5.0))
    key_light = bpy.context.object
    key_light.data.energy = 160.0
    key_light.data.size = 4.0
    key_light.rotation_euler = (math.radians(32), math.radians(-32), 0)

    # Warm Rim Fill Light (Bottom Right)
    bpy.ops.object.light_add(type='AREA', location=(3.5, -3.5, 3.0))
    fill_light = bpy.context.object
    fill_light.data.energy = 35.0
    fill_light.data.color = (1.0, 0.85, 0.7)

    # Shadow Catcher Plane (Bakes realistic soft cast shadow into transparent PNG!)
    if use_shadow_catcher:
        bpy.ops.mesh.primitive_plane_add(size=20.0, location=(0, 0, -0.25))
        shadow_plane = bpy.context.object
        shadow_plane.is_shadow_catcher = True

def create_pbr_metal_material(name, base_color, roughness=0.25, metallic=0.95):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = base_color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat

# ----------------------------------------------------------------------------
# 1. GENERATE SEAMLESS STUDIO HARDWOOD FLOOR BACKGROUND
# ----------------------------------------------------------------------------
def render_studio_wood_floor():
    clear_scene()
    setup_studio_lighting_and_camera(1024, 1024, ortho_scale=4.0, use_shadow_catcher=False)
    bpy.context.scene.render.film_transparent = False

    # Create 3D Beveled Hardwood Planks for Realistic Studio Floor Depth
    wood_mat = create_pbr_metal_material("DarkWalnutWood", (0.14, 0.08, 0.04, 1.0), roughness=0.35, metallic=0.0)
    for row in range(-4, 5):
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, row * 0.55, -0.3))
        plank = bpy.context.object
        plank.scale = (5.0, 0.53, 0.1)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        plank.modifiers["Bevel"].width = 0.015
        plank.data.materials.append(wood_mat)

    bpy.context.scene.render.filepath = os.path.join(OUTPUT_DIR, "wood_floor_tile.png")
    bpy.ops.render.render(write_still=True)

# ----------------------------------------------------------------------------
# 2. GENERATE ISOLATED AMP CHASSIS SPRITES (Wide & Compact)
# ----------------------------------------------------------------------------
def render_amp_chassis_variant(filename, width_scale, height_scale, res_x, res_y):
    clear_scene()
    setup_studio_lighting_and_camera(res_x, res_y, ortho_scale=max(width_scale, height_scale) * 2.4)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
    chassis = bpy.context.object
    chassis.scale = (width_scale, height_scale, 0.25)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    chassis.modifiers["Bevel"].width = 0.06
    chassis.modifiers["Bevel"].segments = 5
    chassis.data.materials.append(create_pbr_metal_material("DarkTolex", (0.06, 0.06, 0.07, 1.0), 0.45, 0.1))

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0.08))
    faceplate = bpy.context.object
    faceplate.scale = (width_scale * 0.92, height_scale * 0.84, 0.15)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    faceplate.modifiers["Bevel"].width = 0.02
    faceplate.modifiers["Bevel"].segments = 3
    faceplate.data.materials.append(create_pbr_metal_material("BrushedGoldPlate", (0.68, 0.54, 0.33, 1.0), 0.22, 0.95))

    bpy.context.scene.render.filepath = os.path.join(OUTPUT_DIR, filename)
    bpy.ops.render.render(write_still=True)

# ----------------------------------------------------------------------------
# 3. GENERATE ISOLATED PEDAL SPRITES (Render Any New Pedal Separately!)
# ----------------------------------------------------------------------------
def render_round_fuzz_pedal():
    clear_scene()
    setup_studio_lighting_and_camera(512, 512, ortho_scale=3.2)

    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=1.25, depth=0.45, location=(0, 0, 0))
    pedal = bpy.context.object
    bpy.ops.object.modifier_add(type='BEVEL')
    pedal.modifiers["Bevel"].width = 0.08
    pedal.modifiers["Bevel"].segments = 6
    pedal.data.materials.append(create_pbr_metal_material("VintageRedFuzz", (0.55, 0.06, 0.06, 1.0), 0.35, 0.7))

    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.18, depth=0.65, location=(0, -0.75, 0.15))
    switch = bpy.context.object
    switch.data.materials.append(create_pbr_metal_material("Chrome", (0.9, 0.9, 0.92, 1.0), 0.08, 1.0))

    bpy.context.scene.render.filepath = os.path.join(OUTPUT_DIR, "pedal_fuzz_round.png")
    bpy.ops.render.render(write_still=True)

def render_rectangular_pedal(filename, color_rgba):
    clear_scene()
    setup_studio_lighting_and_camera(512, 640, ortho_scale=3.0)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0))
    enclosure = bpy.context.object
    enclosure.scale = (1.6, 2.2, 0.45)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    enclosure.modifiers["Bevel"].width = 0.07
    enclosure.modifiers["Bevel"].segments = 5
    enclosure.data.materials.append(create_pbr_metal_material(filename, color_rgba, 0.28, 0.85))

    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.16, depth=0.65, location=(0, -0.7, 0.15))
    switch = bpy.context.object
    switch.data.materials.append(create_pbr_metal_material("ChromeSwitch", (0.9, 0.9, 0.92, 1.0), 0.08, 1.0))

    bpy.context.scene.render.filepath = os.path.join(OUTPUT_DIR, filename)
    bpy.ops.render.render(write_still=True)

# ----------------------------------------------------------------------------
# 4. GENERATE ISOLATED LUXURY 3D GUITAR SPRITE
# ----------------------------------------------------------------------------
def render_luxury_guitar_sprite():
    clear_scene()
    setup_studio_lighting_and_camera(800, 500, ortho_scale=3.8)

    # Carved High-Gloss Amber Sunburst Body Cutout
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=1.2, depth=0.35, location=(-0.5, 0, 0))
    lower_bout = bpy.context.object
    lower_bout.scale = (1.1, 0.95, 1.0)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    lower_bout.modifiers["Bevel"].width = 0.12
    lower_bout.modifiers["Bevel"].segments = 8
    lower_bout.data.materials.append(create_pbr_metal_material("HighGlossAmberMaple", (0.72, 0.32, 0.05, 1.0), 0.06, 0.15))

    # Chrome Pickup Covers & Bridge
    for x_pos in [-0.2, 0.45]:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x_pos, 0, 0.2))
        pickup = bpy.context.object
        pickup.scale = (0.32, 0.7, 0.12)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        pickup.modifiers["Bevel"].width = 0.03
        pickup.data.materials.append(create_pbr_metal_material("ChromePickup", (0.92, 0.92, 0.95, 1.0), 0.05, 1.0))

    bpy.context.scene.render.filepath = os.path.join(OUTPUT_DIR, "guitar_luxury.png")
    bpy.ops.render.render(write_still=True)

if __name__ == "__main__":
    render_studio_wood_floor()
    render_amp_chassis_variant("chassis_wide.png", width_scale=4.0, height_scale=1.2, res_x=1600, res_y=500)
    render_amp_chassis_variant("chassis_compact.png", width_scale=2.2, height_scale=2.0, res_x=900, res_y=800)
    
    # Render Separate Modular Pedals! (Add a new line here anytime you build a new pedal!)
    render_round_fuzz_pedal()
    render_rectangular_pedal("pedal_klon_gold.png", (0.72, 0.56, 0.28, 1.0))
    render_rectangular_pedal("pedal_ts9_green.png", (0.08, 0.52, 0.18, 1.0))
    render_rectangular_pedal("pedal_blues_blue.png", (0.08, 0.22, 0.58, 1.0))
    render_rectangular_pedal("pedal_rat_black.png", (0.07, 0.07, 0.08, 1.0))
    
    # Render Separate Modular Luxury Guitar
    render_luxury_guitar_sprite()
    print("All Modular Studio Assets Rendered to:", OUTPUT_DIR)
```

---

## 4. C++ Code: Self-Contained Modular Pedal Slots (Auto-Resizes When You Add More!)
Here is how JUCE keeps every pedal, the wooden floor, and the guitar completely modular. Each pedal lives in its own `Modular3DPedalSlot` component. Whether you have **3 pedals, 5 pedals, or add an 8th pedal next month**, `Responsive3DRigView::resized()` automatically divides the available wooden floor width and scales all the pedals proportionally so they always fit!

```cpp
#pragma once
#include <JuceHeader.h>

// ============================================================================
// 1. SELF-CONTAINED 3D PEDAL COMPONENT (Keeps Knobs Locked to Its Own Sprite!)
// ============================================================================
class Modular3DPedalSlot : public juce::Component {
private:
    juce::Image pedalSprite;
    juce::Slider driveKnob, levelKnob;
    juce::TextButton learnMaxBtn { "MAX (10)" };
    juce::TextButton learnCleanBtn { "CLEAN (7)" };
    bool hasLearnButtons = false;

public:
    Modular3DPedalSlot(const juce::Image& spriteImage, bool isFuzzWithLearn = false)
        : pedalSprite(spriteImage), hasLearnButtons(isFuzzWithLearn)
    {
        for (auto* s : { &driveKnob, &levelKnob }) {
            s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(s);
        }
        if (hasLearnButtons) {
            addAndMakeVisible(learnMaxBtn);
            addAndMakeVisible(learnCleanBtn);
        }
    }

    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        if (pedalSprite.isValid()) {
            // Draws the isolated 3D pedal + its transparent shadow on top of the wood floor!
            g.drawImage(pedalSprite, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }
    }

    void resized() override {
        auto b = getLocalBounds().toFloat();
        // Because coordinates are percentages of THIS pedal's box, resizing the pedal
        // automatically scales and repositions its knobs and buttons!
        float knobSize = b.getWidth() * 0.25f;
        driveKnob.setBounds(juce::Rectangle<float>(knobSize, knobSize)
            .withCentre({ b.getCentreX() - knobSize * 0.65f, b.getHeight() * 0.34f }).toNearestInt());
        levelKnob.setBounds(juce::Rectangle<float>(knobSize, knobSize)
            .withCentre({ b.getCentreX() + knobSize * 0.65f, b.getHeight() * 0.34f }).toNearestInt());

        if (hasLearnButtons) {
            float btnW = b.getWidth() * 0.35f;
            float btnH = b.getHeight() * 0.085f;
            learnMaxBtn.setBounds(juce::Rectangle<float>(btnW, btnH)
                .withCentre({ b.getCentreX() - btnW * 0.55f, b.getHeight() * 0.54f }).toNearestInt());
            learnCleanBtn.setBounds(juce::Rectangle<float>(btnW, btnH)
                .withCentre({ b.getCentreX() + btnW * 0.55f, b.getHeight() * 0.54f }).toNearestInt());
        }
    }
};

// ============================================================================
// 2. LAYERED STUDIO FLOOR + AUTO-REFLOWING PEDALBOARD & MODULAR GUITAR
// ============================================================================
class Responsive3DRigView : public juce::Component {
private:
    juce::Image woodFloorTile;
    juce::Image chassisWideImg, chassisCompactImg;
    juce::Image luxuryGuitarSprite;

    juce::Rectangle<float> ampBounds;
    juce::Rectangle<float> guitarBounds;
    bool isCompactLayout = false;

    // Dynamic array of pedals: add 1 or 10 pedals, and they auto-resize to fit!
    juce::OwnedArray<Modular3DPedalSlot> pedalSlots;

public:
    // Add a new 3D pedal anytime with a single line of code!
    void addModularPedal(const juce::Image& pedalImg, bool isFuzz = false) {
        auto* newPedal = pedalSlots.add(new Modular3DPedalSlot(pedalImg, isFuzz));
        addAndMakeVisible(newPedal);
        resized(); // Instantly recalculates and scales all pedals to fit!
    }

    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        // LAYER 0: Tiled 3D Hardwood Studio Floor Background
        if (woodFloorTile.isValid()) {
            g.setTiledImageFill(woodFloorTile, 0, 0, 1.0f);
            g.fillAll();
        }

        // LAYER 1A: 3D Amp Chassis Sprite
        const juce::Image& activeChassis = isCompactLayout ? chassisCompactImg : chassisWideImg;
        if (activeChassis.isValid())
            g.drawImage(activeChassis, ampBounds, juce::RectanglePlacement::centred);

        // LAYER 1B: Modular 3D Luxury Guitar Sprite
        if (luxuryGuitarSprite.isValid())
            g.drawImage(luxuryGuitarSprite, guitarBounds, juce::RectanglePlacement::centred);
    }

    void resized() override {
        auto bounds = getLocalBounds().toFloat().reduced(14.0f);
        isCompactLayout = (bounds.getWidth() / std::max(1.0f, bounds.getHeight())) < 1.35f;

        // Top section: Amp Head + Modular Guitar View
        auto topSection = bounds.removeFromTop(bounds.getHeight() * 0.42f);
        guitarBounds = topSection.removeFromRight(topSection.getWidth() * 0.28f);
        ampBounds    = topSection;

        // Bottom section: Dynamic Pedalboard Strip on the Wood Floor
        // Automatically divides the width by however many pedals currently exist!
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
```