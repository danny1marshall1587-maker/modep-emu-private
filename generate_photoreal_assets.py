import bpy
import math
import os
import time
import shutil

OUTPUT_DIR = os.path.join(os.getcwd(), "JUCE_Assets")
ASSETS_DIRS = [
    os.path.join(os.getcwd(), "JUCE_Assets"),
    os.path.join(os.getcwd(), "assets"),
    os.path.join(os.getcwd(), "standalone", "assets"),
    os.path.join(os.getcwd(), "dist", "windows-x64", "assets"),
    os.path.join(os.getcwd(), "plugins", "euphoria-amp-vst", "assets")
]
for d in ASSETS_DIRS:
    os.makedirs(d, exist_ok=True)

def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def setup_lighting_and_camera(res_x, res_y, ortho_scale, use_shadow_catcher=True):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 18
    scene.cycles.use_denoising = True
    scene.render.film_transparent = True
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y

    # Camera directly overhead (Orthographic top-down)
    bpy.ops.object.camera_add(location=(0, 0, 8), rotation=(0, 0, 0))
    cam = bpy.context.object
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = ortho_scale
    scene.camera = cam

    # Main Key Light: Warm studio softbox from top-left (-35 degrees)
    bpy.ops.object.light_add(type='AREA', location=(-4.0, 4.0, 6.0))
    key_light = bpy.context.object
    key_light.data.energy = 240.0
    key_light.data.size = 5.0
    key_light.rotation_euler = (math.radians(35), math.radians(-35), 0)

    # Accent Rim Light: Cool blue-white glint from bottom-right
    bpy.ops.object.light_add(type='AREA', location=(4.5, -4.5, 4.0))
    rim_light = bpy.context.object
    rim_light.data.energy = 95.0
    rim_light.data.size = 3.0
    rim_light.data.color = (0.85, 0.92, 1.0)
    rim_light.rotation_euler = (math.radians(-30), math.radians(30), 0)

    # Top Fill Light: Ambient studio wash
    bpy.ops.object.light_add(type='AREA', location=(0, 0, 7.0))
    top_fill = bpy.context.object
    top_fill.data.energy = 45.0
    top_fill.data.size = 8.0

    if use_shadow_catcher:
        bpy.ops.mesh.primitive_plane_add(size=30.0, location=(0, 0, -0.05))
        plane = bpy.context.object
        plane.is_shadow_catcher = True

def create_pbr_material(name, base_color, roughness=0.30, metallic=0.9, specular=0.8, bump_strength=0.0, noise_scale=120.0, is_brushed=False):
    mat = bpy.data.materials.new(name=name)
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")

    if bsdf:
        bsdf.inputs["Base Color"].default_value = base_color
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if "Specular IOR Level" in bsdf.inputs:
            bsdf.inputs["Specular IOR Level"].default_value = specular
        elif "Specular" in bsdf.inputs:
            bsdf.inputs["Specular"].default_value = specular

        if bump_strength > 0.001:
            tex_coord = nodes.new(type="ShaderNodeTexCoord")
            mapping = nodes.new(type="ShaderNodeMapping")
            links.new(tex_coord.outputs["Object"], mapping.inputs["Vector"])

            if is_brushed:
                mapping.inputs["Scale"].default_value = (80.0, 1.5, 1.0)
            else:
                mapping.inputs["Scale"].default_value = (1.0, 1.0, 1.0)

            noise = nodes.new(type="ShaderNodeTexNoise")
            noise.inputs["Scale"].default_value = noise_scale
            noise.inputs["Detail"].default_value = 3.5
            noise.inputs["Roughness"].default_value = 0.65
            links.new(mapping.outputs["Vector"], noise.inputs["Vector"])

            bump = nodes.new(type="ShaderNodeBump")
            bump.inputs["Strength"].default_value = bump_strength
            bump.inputs["Distance"].default_value = 0.03
            links.new(noise.outputs["Fac"], bump.inputs["Height"])
            links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    return mat

def save_to_all_destinations(filename):
    p_primary = os.path.join(OUTPUT_DIR, filename)
    bpy.context.scene.render.filepath = p_primary
    bpy.ops.render.render(write_still=True)
    for target_dir in ASSETS_DIRS:
        dest_file = os.path.join(target_dir, filename)
        if dest_file != p_primary:
            for attempt in range(5):
                try:
                    shutil.copy2(p_primary, dest_file)
                    break
                except Exception:
                    time.sleep(0.3)
    print(f"Rendered and synced: {filename}")

# ============================================================================
# 1. HARDWOOD STUDIO FLOOR (Seamless, High Definition)
# ============================================================================
def render_studio_wood_floor():
    print(">>> Rendering Studio Wood Floor...")
    clear_scene()
    setup_lighting_and_camera(1024, 1024, ortho_scale=4.0, use_shadow_catcher=False)
    bpy.context.scene.render.film_transparent = False

    wood_colors = [
        (0.18, 0.10, 0.05, 1.0),
        (0.22, 0.13, 0.06, 1.0),
        (0.16, 0.09, 0.04, 1.0),
        (0.20, 0.11, 0.05, 1.0),
        (0.15, 0.08, 0.04, 1.0),
        (0.19, 0.11, 0.05, 1.0),
        (0.17, 0.09, 0.04, 1.0),
        (0.21, 0.12, 0.06, 1.0)
    ]
    plank_h = 0.50
    for idx, row in enumerate(range(-4, 4)):
        y_pos = row * plank_h + (plank_h * 0.5)
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, y_pos, 0))
        plank = bpy.context.object
        plank.scale = (5.0, plank_h - 0.015, 0.1)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        plank.modifiers["Bevel"].width = 0.008
        plank.modifiers["Bevel"].segments = 3
        mat = create_pbr_material(f"Wood_{idx}", wood_colors[idx % len(wood_colors)], roughness=0.38, metallic=0.0, bump_strength=0.04, noise_scale=200.0)
        plank.data.materials.append(mat)

    save_to_all_destinations("wood_floor_tile.png")

# ============================================================================
# 2. 3D AMP HEAD CHASSIS (Blue Clean, Brown Crunch, Red Lead)
# ============================================================================
def render_amp_chassis(filename, tolex_color, face_color, grille_color, corner_color, w=4.2, h=1.4, res_x=1600, res_y=540):
    print(f">>> Rendering Amp Chassis: {filename}...")
    clear_scene()
    setup_lighting_and_camera(res_x, res_y, ortho_scale=w * 1.12, use_shadow_catcher=True)

    # 1. Main Tolex Head Cabinet (Beveled wood sleeve with authentic leather grain & dirt)
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0.2))
    cab = bpy.context.object
    cab.scale = (w, h, 0.4)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    cab.modifiers["Bevel"].width = 0.12
    cab.modifiers["Bevel"].segments = 6
    cab.data.materials.append(create_pbr_material("Tolex", tolex_color, roughness=0.52, metallic=0.04, bump_strength=0.10, noise_scale=160.0))

    # 2. Upper Control Faceplate (Brushed Anodized Aluminum with horizontal grain)
    face_h = h * 0.56
    face_y = (h * 0.5) - (face_h * 0.5) - 0.05
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, face_y, 0.32))
    face = bpy.context.object
    face.scale = (w * 0.88, face_h, 0.18)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    face.modifiers["Bevel"].width = 0.02
    face.modifiers["Bevel"].segments = 3
    face.data.materials.append(create_pbr_material("BrushedFace", face_color, roughness=0.22, metallic=0.92, bump_strength=0.03, noise_scale=60.0, is_brushed=True))

    # 3. Lower Basketweave Grille Cloth Section
    grille_h = h * 0.32
    grille_y = (-h * 0.5) + (grille_h * 0.5) + 0.05
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, grille_y, 0.30))
    grille = bpy.context.object
    grille.scale = (w * 0.88, grille_h, 0.16)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    grille.modifiers["Bevel"].width = 0.02
    grille.modifiers["Bevel"].segments = 3
    grille.data.materials.append(create_pbr_material("GrilleCloth", grille_color, roughness=0.75, metallic=0.0, bump_strength=0.12, noise_scale=220.0))

    # 4. Piping Trim Strip between Faceplate and Grille
    pipe_y = (face_y - face_h * 0.5 + grille_y + grille_h * 0.5) * 0.5
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, pipe_y, 0.34))
    pipe = bpy.context.object
    pipe.scale = (w * 0.885, 0.035, 0.12)
    bpy.ops.object.transform_apply(scale=True)
    pipe.data.materials.append(create_pbr_material("PipingTrim", corner_color, roughness=0.15, metallic=0.95))

    # 5. Heavy Leather Handle on Top Center
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, h * 0.5 - 0.02, 0.45))
    handle = bpy.context.object
    handle.scale = (w * 0.28, 0.08, 0.06)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    handle.modifiers["Bevel"].width = 0.025
    handle.modifiers["Bevel"].segments = 4
    handle.data.materials.append(create_pbr_material("StitchedLeather", (0.05, 0.03, 0.02, 1.0), roughness=0.45, metallic=0.0, bump_strength=0.08, noise_scale=140.0))

    # Handle Chrome/Brass Brackets
    for bx in [-w * 0.14, w * 0.14]:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(bx, h * 0.5 - 0.02, 0.44))
        bra = bpy.context.object
        bra.scale = (0.08, 0.10, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        bra.data.materials.append(create_pbr_material("HandleBracket", corner_color, roughness=0.10, metallic=1.0))

    # 6. Four Metal Corner Protectors (Authentic small beveled corner caps on outer cabinet edge)
    corner_r = 0.035
    corners = [
        (-w * 0.5 + 0.05,  h * 0.5 - 0.05),
        ( w * 0.5 - 0.05,  h * 0.5 - 0.05),
        (-w * 0.5 + 0.05, -h * 0.5 + 0.05),
        ( w * 0.5 - 0.05, -h * 0.5 + 0.05),
    ]
    for cx, cy in corners:
        bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=corner_r, depth=0.46, location=(cx, cy, 0.22))
        cp = bpy.context.object
        cp.data.materials.append(create_pbr_material("CornerMetal", corner_color, roughness=0.18, metallic=0.95))

    save_to_all_destinations(filename)

# ============================================================================
# 3. 3D STUDIO RACK UNITS (Topaz Space, Pink Reverb, Orange Tremolo, Green Delay)
# ============================================================================
def render_rack_unit(filename, rack_color, w=4.2, h=1.4, res_x=1600, res_y=540, has_lower_viewport=True, has_split_display=False):
    print(f">>> Rendering Rack Unit: {filename}...")
    clear_scene()
    setup_lighting_and_camera(res_x, res_y, ortho_scale=w * 1.12, use_shadow_catcher=True)

    # 1. Main Rack Enclosure Faceplate (Heavy anodized aluminum chassis)
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0.2))
    rack = bpy.context.object
    rack.scale = (w, h, 0.35)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    rack.modifiers["Bevel"].width = 0.04
    rack.modifiers["Bevel"].segments = 4
    rack.data.materials.append(create_pbr_material("RackAnodized", rack_color, roughness=0.25, metallic=0.90, bump_strength=0.03, noise_scale=50.0, is_brushed=True))

    face_color = (rack_color[0] * 0.82, rack_color[1] * 0.82, rack_color[2] * 0.82, 1.0)

    if has_split_display:
        # Left Side Control Inset for Knobs
        ctrl_w = w * 0.44
        ctrl_x = -w * 0.20
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(ctrl_x, 0, 0.32))
        face = bpy.context.object
        face.scale = (ctrl_w, h * 0.78, 0.16)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        face.modifiers["Bevel"].width = 0.02
        face.modifiers["Bevel"].segments = 3
        face.data.materials.append(create_pbr_material("RackControlArea", face_color, roughness=0.20, metallic=0.92, bump_strength=0.02, noise_scale=40.0, is_brushed=True))

        # Right Side Display Bay Inset (for Reverb cavity / Tremolo CRT)
        disp_w = w * 0.40
        disp_x = w * 0.23
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(disp_x, 0, 0.28))
        viewport = bpy.context.object
        viewport.scale = (disp_w, h * 0.78, 0.14)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        viewport.modifiers["Bevel"].width = 0.02
        viewport.modifiers["Bevel"].segments = 3
        viewport.data.materials.append(create_pbr_material("DarkViewportRecess", (0.02, 0.03, 0.04, 1.0), roughness=0.30, metallic=0.3))
    else:
        # 2. Inner Recessed Faceplate (Control Area)
        face_h = (h * 0.44) if has_lower_viewport else (h * 0.78)
        face_y = ((h * 0.5) - (face_h * 0.5) - 0.06) if has_lower_viewport else 0.0
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, face_y, 0.32))
        face = bpy.context.object
        face.scale = (w * 0.78, face_h, 0.16)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        face.modifiers["Bevel"].width = 0.02
        face.modifiers["Bevel"].segments = 3
        face.data.materials.append(create_pbr_material("RackControlArea", face_color, roughness=0.20, metallic=0.92, bump_strength=0.02, noise_scale=40.0, is_brushed=True))

        # 3. Lower Display Viewport Recess (framed cleanly between rack ears)
        if has_lower_viewport:
            grille_h = h * 0.44
            grille_y = (-h * 0.5) + (grille_h * 0.5) + 0.06
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, grille_y, 0.28))
            viewport = bpy.context.object
            viewport.scale = (w * 0.78, grille_h, 0.14)
            bpy.ops.object.transform_apply(scale=True)
            bpy.ops.object.modifier_add(type='BEVEL')
            viewport.modifiers["Bevel"].width = 0.02
            viewport.modifiers["Bevel"].segments = 3
            viewport.data.materials.append(create_pbr_material("DarkViewportRecess", (0.02, 0.03, 0.04, 1.0), roughness=0.30, metallic=0.3))

    # 4. Studio Rack Ears on Left and Right (19" Rack Standard)
    for side_x in [-w * 0.5 + 0.12, w * 0.5 - 0.12]:
        # Vertical Rack Ear Bar
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(side_x, 0, 0.36))
        ear = bpy.context.object
        ear.scale = (0.16, h * 0.94, 0.15)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        ear.modifiers["Bevel"].width = 0.015
        ear.modifiers["Bevel"].segments = 3
        ear.data.materials.append(create_pbr_material("RackEarMetal", (0.75, 0.75, 0.78, 1.0), roughness=0.15, metallic=0.98))

        # Machined Hex Screws in Rack Ear
        screw_y_positions = [-h * 0.38, -h * 0.14, h * 0.14, h * 0.38] if (h > 1.0) else [-h * 0.30, h * 0.30]
        for sy in screw_y_positions:
            bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.04, depth=0.18, location=(side_x, sy, 0.42))
            sc = bpy.context.object
            sc.data.materials.append(create_pbr_material("RackScrew", (0.15, 0.15, 0.16, 1.0), roughness=0.2, metallic=0.9))

    save_to_all_destinations(filename)

# ============================================================================
# 4. 3D BOUTIQUE PEDALS (Klon, TS9, Blues Driver, Rat, Fuzz Face)
# ============================================================================
def render_pedal(filename, w, h, base_rgba, is_wedge=True, has_ribbed_pad=False, is_round=False):
    print(f">>> Rendering Pedal: {filename}...")
    clear_scene()
    setup_lighting_and_camera(512, 640, ortho_scale=h * 1.25, use_shadow_catcher=True)

    if is_round:
        # Authentic Round Cast-Iron Dallas Arbiter Fuzz Face
        radius = w * 0.48
        bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=radius, depth=0.38, location=(0, 0, 0.19))
        pedal = bpy.context.object
        bpy.ops.object.modifier_add(type='BEVEL')
        pedal.modifiers["Bevel"].width = 0.06
        pedal.modifiers["Bevel"].segments = 6
        pedal.data.materials.append(create_pbr_material("FuzzIron", base_rgba, roughness=0.35, metallic=0.72, bump_strength=0.08, noise_scale=150.0))

        # Black Rubber "Smile" Stomp Pad
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=radius * 0.42, depth=0.08, location=(0, -radius * 0.28, 0.39))
        pad = bpy.context.object
        pad.scale = (1.2, 0.55, 1.0)
        bpy.ops.object.transform_apply(scale=True)
        pad.data.materials.append(create_pbr_material("RubberPad", (0.05, 0.05, 0.05, 1.0), roughness=0.85, metallic=0.0))

        # Chrome 3PDT Footswitch in center
        bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.13, depth=0.35, location=(0, -radius * 0.28, 0.48))
        sw = bpy.context.object
        sw.data.materials.append(create_pbr_material("ChromeSW", (0.92, 0.92, 0.95, 1.0), roughness=0.08, metallic=1.0))
    else:
        # Die-cast Box Enclosure (with sloped wedge top)
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0.22))
        box = bpy.context.object
        box.scale = (w, h, 0.42)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        box.modifiers["Bevel"].width = 0.06
        box.modifiers["Bevel"].segments = 5
        box.data.materials.append(create_pbr_material("PedalPaint", base_rgba, roughness=0.28, metallic=0.82, bump_strength=0.05, noise_scale=120.0))

        # Inset Top Recess / Faceplate Area
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0.08, 0.36))
        plate = bpy.context.object
        plate.scale = (w * 0.88, h * 0.88, 0.16)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.modifier_add(type='BEVEL')
        plate.modifiers["Bevel"].width = 0.02
        plate.modifiers["Bevel"].segments = 3
        plate_rgba = (base_rgba[0] * 0.85, base_rgba[1] * 0.85, base_rgba[2] * 0.85, 1.0)
        plate.data.materials.append(create_pbr_material("PedalPlate", plate_rgba, roughness=0.22, metallic=0.88, bump_strength=0.03, noise_scale=100.0))

        if has_ribbed_pad:
            # TS808 Rubber Treadle Pad
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, -h * 0.30, 0.42))
            pad = bpy.context.object
            pad.scale = (w * 0.65, h * 0.22, 0.08)
            bpy.ops.object.transform_apply(scale=True)
            bpy.ops.object.modifier_add(type='BEVEL')
            pad.modifiers["Bevel"].width = 0.015
            pad.modifiers["Bevel"].segments = 3
            pad.data.materials.append(create_pbr_material("TS808Pad", (0.06, 0.07, 0.06, 1.0), roughness=0.8, metallic=0.0))
        else:
            # Chrome 3PDT Footswitch Button with Hex Nut & Washer
            bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.18, depth=0.06, location=(0, -h * 0.30, 0.42))
            nut = bpy.context.object
            nut.data.materials.append(create_pbr_material("HexNut", (0.85, 0.85, 0.88, 1.0), roughness=0.12, metallic=1.0))

            bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.13, depth=0.28, location=(0, -h * 0.30, 0.50))
            sw = bpy.context.object
            sw.data.materials.append(create_pbr_material("ChromeButton", (0.92, 0.92, 0.95, 1.0), roughness=0.08, metallic=1.0))

        # Jewel LED Indicator Lamp at upper center
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.06, depth=0.12, location=(0, h * 0.36, 0.44))
        led = bpy.context.object
        led.data.materials.append(create_pbr_material("RedLED", (0.9, 0.1, 0.1, 1.0), roughness=0.1, metallic=0.2))

        # LED Chrome Bezel
        bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.09, depth=0.08, location=(0, h * 0.36, 0.42))
        led_b = bpy.context.object
        led_b.data.materials.append(create_pbr_material("LEDBezel", (0.9, 0.9, 0.92, 1.0), roughness=0.08, metallic=1.0))

    save_to_all_destinations(filename)

# ============================================================================
# 5. MODULAR LUXURY GUITAR SPRITE (Flame Maple Carved Top + PAF Humbuckers)
# ============================================================================
def render_luxury_guitar():
    print(">>> Rendering Luxury Flame Maple Guitar...")
    clear_scene()
    setup_lighting_and_camera(800, 500, ortho_scale=3.4, use_shadow_catcher=True)

    # 1. Carved Double-Cut Body (Amber Sunburst)
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=1.3, depth=0.30, location=(-0.35, 0, 0.15))
    lower = bpy.context.object
    lower.scale = (1.15, 0.90, 1.0)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    lower.modifiers["Bevel"].width = 0.12
    lower.modifiers["Bevel"].segments = 8
    lower.data.materials.append(create_pbr_material("FlameMapleAmber", (0.78, 0.38, 0.06, 1.0), roughness=0.08, metallic=0.15))

    # Upper bout / horn
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.9, depth=0.28, location=(0.55, 0.15, 0.15))
    upper = bpy.context.object
    upper.scale = (1.0, 0.85, 1.0)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.modifier_add(type='BEVEL')
    upper.modifiers["Bevel"].width = 0.10
    upper.modifiers["Bevel"].segments = 6
    upper.data.materials.append(create_pbr_material("FlameMapleAmber2", (0.78, 0.38, 0.06, 1.0), roughness=0.08, metallic=0.15))

    # 2. Chrome Pickup Rings & Humbuckers with Pole Screws
    for px in [-0.15, 0.40]:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(px, 0, 0.32))
        ring = bpy.context.object
        ring.scale = (0.34, 0.72, 0.08)
        bpy.ops.object.transform_apply(scale=True)
        ring.data.materials.append(create_pbr_material("ChromeRing", (0.9, 0.9, 0.92, 1.0), roughness=0.08, metallic=1.0))

        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(px, 0, 0.35))
        paf = bpy.context.object
        paf.scale = (0.28, 0.65, 0.10)
        bpy.ops.object.transform_apply(scale=True)
        paf.data.materials.append(create_pbr_material("NickelPAF", (0.85, 0.84, 0.82, 1.0), roughness=0.15, metallic=0.95))

    # 3. Chrome Tune-o-matic Bridge & Stopbar Tailpiece
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(-0.55, 0, 0.33))
    bridge = bpy.context.object
    bridge.scale = (0.16, 0.68, 0.12)
    bpy.ops.object.transform_apply(scale=True)
    bridge.data.materials.append(create_pbr_material("TOMBridge", (0.92, 0.92, 0.95, 1.0), roughness=0.08, metallic=1.0))

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(-0.82, 0, 0.31))
    tail = bpy.context.object
    tail.scale = (0.14, 0.65, 0.10)
    bpy.ops.object.transform_apply(scale=True)
    tail.data.materials.append(create_pbr_material("Stopbar", (0.92, 0.92, 0.95, 1.0), roughness=0.08, metallic=1.0))

    # 4. Amber Bell Volume & Tone Knobs
    for kx, ky in [(-0.65, -0.55), (-0.40, -0.65)]:
        bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.11, depth=0.15, location=(kx, ky, 0.34))
        kn = bpy.context.object
        kn.data.materials.append(create_pbr_material("AmberBellKnob", (0.85, 0.55, 0.10, 1.0), roughness=0.15, metallic=0.2))

    save_to_all_destinations("guitar_luxury.png")

# ============================================================================
# MAIN ENTRYPOINT
# ============================================================================
if __name__ == "__main__":
    t0 = time.time()

    # 1. Studio Hardwood Floor
    render_studio_wood_floor()

    # 2. Distinct Color Amp Chassis Heads (Clean = Blue, Crunch = Brown, Lead = Red)
    # Page 0: Clean = Boutique Royal Sapphire Blue Tolex, Brushed Champagne/Silver Faceplate, Chrome Corners
    render_amp_chassis("chassis_clean_blue.png",
                       tolex_color=(0.04, 0.20, 0.60, 1.0),
                       face_color=(0.88, 0.88, 0.90, 1.0),
                       grille_color=(0.14, 0.20, 0.28, 1.0),
                       corner_color=(0.92, 0.92, 0.95, 1.0))

    # Page 1: Crunch = Vintage Tweed / Chocolate Brown Tolex, Aged Brass Faceplate, Brass Corners
    render_amp_chassis("chassis_crunch_brown.png",
                       tolex_color=(0.38, 0.20, 0.08, 1.0),
                       face_color=(0.80, 0.65, 0.32, 1.0),
                       grille_color=(0.28, 0.22, 0.14, 1.0),
                       corner_color=(0.88, 0.74, 0.40, 1.0))

    # Page 2: Lead = Hot-Rod British Crimson Red Tolex, Brushed Black Anodized Faceplate, Chrome Corners
    render_amp_chassis("chassis_lead_red.png",
                       tolex_color=(0.62, 0.07, 0.09, 1.0),
                       face_color=(0.10, 0.10, 0.11, 1.0),
                       grille_color=(0.18, 0.12, 0.14, 1.0),
                       corner_color=(0.92, 0.92, 0.95, 1.0))

    # Default fallback chassis
    render_amp_chassis("chassis_wide.png",
                       tolex_color=(0.06, 0.06, 0.07, 1.0),
                       face_color=(0.75, 0.60, 0.35, 1.0),
                       grille_color=(0.22, 0.18, 0.12, 1.0),
                       corner_color=(0.88, 0.76, 0.45, 1.0))

    # 3. Distinct Color Rack Units (Space = Topaz, Reverb = Pink, Tremolo = Orange, Delay = Green)
    # Page 4: Space Chamber = Deep Topaz / Electric Cyan Rack
    render_rack_unit("rack_space_topaz.png", rack_color=(0.04, 0.28, 0.38, 1.0), has_lower_viewport=True)

    # Page 5: Upper Rack = Velvet Pink / Rose Gold Reverb Space Rack
    render_rack_unit("rack_reverb_pink.png", rack_color=(0.52, 0.14, 0.28, 1.0), h=0.72, res_y=280, has_lower_viewport=False, has_split_display=True)

    # Page 5: Lower Rack = Vintage Amber / Orange Fender 6G Tremolo Rack
    render_rack_unit("rack_trem_orange.png", rack_color=(0.62, 0.26, 0.05, 1.0), h=0.72, res_y=280, has_lower_viewport=False, has_split_display=True)

    # Page 6: Delay = Vintage British Racing Green / Sage 3D Tape Delay Rack
    render_rack_unit("rack_delay_green.png", rack_color=(0.06, 0.30, 0.15, 1.0), has_lower_viewport=True)

    # 4. 5 Stompbox Pedals (Sized for generous floor margins)
    render_pedal("pedal_klon_gold.png",  w=1.5, h=2.1, base_rgba=(0.76, 0.60, 0.28, 1.0), is_wedge=True)
    render_pedal("pedal_ts9_green.png",  w=1.45, h=2.1, base_rgba=(0.10, 0.55, 0.20, 1.0), has_ribbed_pad=True)
    render_pedal("pedal_blues_blue.png", w=1.45, h=2.1, base_rgba=(0.08, 0.25, 0.65, 1.0))
    render_pedal("pedal_rat_black.png",  w=1.5, h=2.1, base_rgba=(0.08, 0.08, 0.09, 1.0), is_wedge=True)
    render_pedal("pedal_fuzz_round.png", w=1.8, h=1.8, base_rgba=(0.58, 0.08, 0.08, 1.0), is_round=True)

    # 5. Modular Luxury Guitar
    render_luxury_guitar()

    print(f"\nALL 3D PHOTOREAL GEAR ASSETS RENDERED AND SYNCED SUCCESSFULLY IN {time.time() - t0:.1f}s!")
