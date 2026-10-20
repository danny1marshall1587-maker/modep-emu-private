#!/usr/bin/env bash
# ==============================================================================
# build_linux_arm_plugins.sh
# Native & Cross-Compilation Build Script for Patchbox OS / MODEP (ARM32 / ARM64)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ARCH=$(uname -m)
echo "================================================================"
echo "   Building MODEP LV2 Plugins for Linux / ARM ($ARCH)           "
echo "================================================================"

CXX="${CXX:-g++}"
CXXFLAGS="-O3 -fPIC -shared -static-libstdc++ -static-libgcc -I plugins/include"

PLUGINS=(
  "harmonic-tremolo:src/harmonic_tremolo_lv2.cpp:harmonic_tremolo.so:harmonic-tremolo.lv2"
  "cyber-denoiser:src/cyber_denoiser_lv2.cpp:cyber_denoiser.so:cyber-denoiser.lv2"
  "cyber-denoiser-mono.lv2:src/cyber_denoiser_mono_lv2.cpp:cyber_denoiser_mono.so:cyber-denoiser-mono.lv2"
  "galaxy-strobe-tune:src/galaxy_strobe_tune_lv2.cpp:galaxy_strobe_tune.so:galaxy-strobe-tune.lv2"
  "dimension-c:src/dimension_c_lv2.cpp:dimension_c.so:dimension-c.lv2"
  "Dimension_IV.lv2:src/dimension_iv_lv2.cpp:Dimension_IV_dsp.so:Dimension_IV.lv2"
  "Dearmondo610.lv2:src/dearmondo610_lv2.cpp:DeArmondo610_dsp.so:Dearmondo610.lv2"
  "guitar-midi:src/guitar_midi_lv2.cpp:guitar_midi.so:guitar-midi.lv2"
  "bluesbreaker.lv2:src/bluesbreaker_lv2.cpp:bluesbreaker.so:bluesbreaker.lv2"
  "aether.lv2:src/aether_lv2.cpp:aether_dsp.so:aether.lv2"
  "nam-loader.lv2:src/nam_loader_lv2.cpp:nam_loader.so:nam-loader.lv2"
  "cyber-hum-killer.lv2:src/cyber_hum_killer_lv2.cpp:cyber_hum_killer.so:cyber-hum-killer.lv2"
  "cyber-hum-killer-mono.lv2:src/cyber_hum_killer_mono_lv2.cpp:cyber_hum_killer_mono.so:cyber-hum-killer-mono.lv2"
  "smart-fizz-killer.lv2:src/smart_fizz_killer_lv2.cpp:smart_fizz_killer.so:smart-fizz-killer.lv2"
  "smart-fizz-killer-mono.lv2:src/smart_fizz_killer_mono.cpp:smart_fizz_killer_mono.so:smart-fizz-killer-mono.lv2"
  "cyber-expression-cv.lv2:src/cyber_expression_cv_lv2.cpp:cyber_expression_cv.so:cyber-expression-cv.lv2"
  "aelapse.lv2:src/aelapse_lv2.cpp:aelapse_dsp.so:aelapse.lv2"
  "aelapse-mono.lv2:src/aelapse_mono_lv2.cpp:aelapse_mono_dsp.so:aelapse-mono.lv2"
  "cyber-stomp-box.lv2:src/cyber_stomp_box_lv2.cpp:cyber_stomp_box.so:cyber-stomp-box.lv2"
  "cyber-puresustain-delay.lv2:src/cyber_puresustain_delay.cpp:cyber_puresustain_delay.so:cyber-puresustain-delay.lv2"
  "cyber-puresustain-delay-mono.lv2:src/cyber_puresustain_delay_mono.cpp:cyber_puresustain_delay_mono.so:cyber-puresustain-delay-mono.lv2"
  "cyber-cloud-bloom.lv2:src/cyber_cloud_bloom.cpp:cyber_cloud_bloom.so:cyber-cloud-bloom.lv2"
  "cyber-cloud-bloom-mono.lv2:src/cyber_cloud_bloom_mono.cpp:cyber_cloud_bloom_mono.so:cyber-cloud-bloom-mono.lv2"
  "cyber-spring-reverb.lv2:src/cyber_spring_reverb.cpp:cyber_spring_reverb.so:cyber-spring-reverb.lv2"
  "cyber-spring-reverb-stereo.lv2:src/cyber_spring_reverb_stereo.cpp:cyber_spring_reverb_stereo.so:cyber-spring-reverb-stereo.lv2"
  "cyber-acoustic-feedbacker.lv2:src/cyber_acoustic_feedbacker.cpp:cyber_acoustic_feedbacker.so:cyber-acoustic-feedbacker.lv2"
  "cyber-acoustic-feedbacker-mono.lv2:src/cyber_acoustic_feedbacker_mono.cpp:cyber_acoustic_feedbacker_mono.so:cyber-acoustic-feedbacker-mono.lv2"
  "cyber-feedback-room.lv2:src/cyber_feedback_room.cpp:cyber_feedback_room.so:cyber-feedback-room.lv2"
  "cyber-feedback-room-mono.lv2:src/cyber_feedback_room_mono.cpp:cyber_feedback_room_mono.so:cyber-feedback-room-mono.lv2"
  "cyber-feedback-super.lv2:src/cyber_feedback_super.cpp:cyber_feedback_super.so:cyber-feedback-super.lv2"
  "cyber-feedback-super-mono.lv2:src/cyber_feedback_super_mono.cpp:cyber_feedback_super_mono.so:cyber-feedback-super-mono.lv2"
  "cyber-cv-reverser.lv2:src/cyber_cv_reverser.cpp:cyber_cv_reverser.so:cyber-cv-reverser.lv2"
  "cyber-cv-splitter.lv2:src/cyber_cv_splitter.cpp:cyber_cv_splitter.so:cyber-cv-splitter.lv2"
  "cyber-ycv40.lv2:src/cyber_ycv40_lv2.cpp:cyber_ycv40.so:cyber-ycv40.lv2"
  "cyber-dumble-ods.lv2:src/cyber_dumble_ods_lv2.cpp:cyber_dumble_ods.so:cyber-dumble-ods.lv2"
  "cyber-fender-bassman59.lv2:src/cyber_fender_bassman59_lv2.cpp:cyber_fender_bassman59.so:cyber-fender-bassman59.lv2"
  "cyber-fender-deluxe6g3.lv2:src/cyber_fender_deluxe6g3_lv2.cpp:cyber_fender_deluxe6g3.so:cyber-fender-deluxe6g3.lv2"
  "cyber-fender-vibrolux6g11.lv2:src/cyber_fender_vibrolux6g11_lv2.cpp:cyber_fender_vibrolux6g11.so:cyber-fender-vibrolux6g11.lv2"
  "cyber-fender-vibroverb63.lv2:src/cyber_fender_vibroverb63_lv2.cpp:cyber_fender_vibroverb63.so:cyber-fender-vibroverb63.lv2"
  "cyber-friedman-be100.lv2:src/cyber_friedman_be100_lv2.cpp:cyber_friedman_be100.so:cyber-friedman-be100.lv2"
  "cyber-hiwatt-dr504.lv2:src/cyber_hiwatt_dr504_lv2.cpp:cyber_hiwatt_dr504.so:cyber-hiwatt-dr504.lv2"
  "cyber-koch-the-greg.lv2:src/cyber_koch_the_greg_lv2.cpp:cyber_koch_the_greg.so:cyber-koch-the-greg.lv2"
  "cyber-magnatone-280.lv2:src/cyber_magnatone_280_lv2.cpp:cyber_magnatone_280.so:cyber-magnatone-280.lv2"
  "cyber-marshall-jtm45.lv2:src/cyber_marshall_jtm45_lv2.cpp:cyber_marshall_jtm45.so:cyber-marshall-jtm45.lv2"
  "cyber-matchless-dc30.lv2:src/cyber_matchless_dc30_lv2.cpp:cyber_matchless_dc30.so:cyber-matchless-dc30.lv2"
  "cyber-orange-or120.lv2:src/cyber_orange_or120_lv2.cpp:cyber_orange_or120.so:cyber-orange-or120.lv2"
  "cyber-rockman-x100.lv2:src/cyber_rockman_x100_lv2.cpp:cyber_rockman_x100.so:cyber-rockman-x100.lv2"
  "cyber-roland-jc120.lv2:src/cyber_roland_jc120_lv2.cpp:cyber_roland_jc120.so:cyber-roland-jc120.lv2"
  "cyber-toneking-imperial.lv2:src/cyber_toneking_imperial_lv2.cpp:cyber_toneking_imperial.so:cyber-toneking-imperial.lv2"
  "cyber-trainwreck-express.lv2:src/cyber_trainwreck_express_lv2.cpp:cyber_trainwreck_express.so:cyber-trainwreck-express.lv2"
  "cyber-tworock-crs.lv2:src/cyber_tworock_crs_lv2.cpp:cyber_tworock_crs.so:cyber-tworock-crs.lv2"
  "cyber-vox-ac30pa.lv2:src/cyber_vox_ac30pa_lv2.cpp:cyber_vox_ac30pa.so:cyber-vox-ac30pa.lv2"
  "cyber-vox-ac30tb.lv2:src/cyber_vox_ac30tb_lv2.cpp:cyber_vox_ac30tb.so:cyber-vox-ac30tb.lv2"
  "cyber-slide-driver.lv2:src/cyber_slide_driver_lv2.cpp:cyber_slide_driver.so:cyber-slide-driver.lv2"
  "cyber-organ9.lv2:src/cyber_organ9_lv2.cpp:cyber_organ9.so:cyber-organ9.lv2"
  "cyber-mel9.lv2:src/cyber_mel9_lv2.cpp:cyber_mel9.so:cyber-mel9.lv2"
  "cyber-synth9.lv2:src/cyber_synth9_lv2.cpp:cyber_synth9.so:cyber-synth9.lv2"
  "cyber-string9.lv2:src/cyber_synth9_lv2.cpp:cyber_string9.so:cyber-string9.lv2"
  "cyber-bass9.lv2:src/cyber_bass9_lv2.cpp:cyber_bass9.so:cyber-bass9.lv2"
  "cyber-evans-bass-pro.lv2:src/cyber_evans_bass_pro.cpp:cyber_evans_bass_pro.so:cyber-evans-bass-pro.lv2"
  "cyber-evans-bass-pro-mono.lv2:src/cyber_evans_bass_pro_mono.cpp:cyber_evans_bass_pro_mono.so:cyber-evans-bass-pro-mono.lv2"
  "cyber-tone3000.lv2:src/cyber_tone3000_lv2.cpp:cyber_tone3000.so:cyber-tone3000.lv2"
  "lv2-psx-reverb.lv2:src/psx-reverb.c:psx-reverb.so:lv2-psx-reverb.lv2"
  "dfjpverb.lv2:src/dfjpverb_lv2.cpp plugins/dfjpverb.lv2/src/DFJPverb.cpp:dfjpverb.so:dfjpverb.lv2"
  "duskverb.lv2:src/duskverb_lv2.cpp:duskverb.so:duskverb.lv2"
  "invada-delay-munge.lv2:src/inv_delay.c plugins/invada-delay-munge.lv2/src/common.c:inv_delay.so:invada-delay-munge.lv2"
  "infamous-stuck.lv2:src/stuck.c plugins/infamous-stuck.lv2/src/rms_calc.c:stuck.so:infamous-stuck.lv2"
  "grains-of-sand.lv2:src/grains_lv2.cpp:grains-of-sand.so:grains-of-sand.lv2"
  "dusk-tape-echo.lv2:src/tape_echo_lv2.cpp:dusk-tape-echo.so:dusk-tape-echo.lv2"
)

mkdir -p dist

for entry in "${PLUGINS[@]}"; do
  IFS=":" read -r pdir psrc pbin bname <<< "$entry"
  echo "[*] Compiling $pdir -> $pbin..."
  $CXX $CXXFLAGS -I "plugins/$pdir/src" "plugins/$pdir/$psrc" -o "plugins/$pdir/$pbin"
  echo " [+] SUCCESS: plugins/$pdir/$pbin"

  # Copy into dist bundle
  target_b="dist/$bname"
  mkdir -p "$target_b"
  cp -r plugins/$pdir/*.ttl "$target_b/" 2>/dev/null || true
  cp -r plugins/$pdir/*.so "$target_b/" 2>/dev/null || true
  if [ -d "plugins/$pdir/modgui" ]; then
    cp -r "plugins/$pdir/modgui" "$target_b/"
  fi
done

echo ""
echo "[*] Packaging dist/modep_plugins_${ARCH}.tar.gz..."
cd dist
tar -czvf "modep_plugins_${ARCH}.tar.gz" *.lv2
cd ..

echo "================================================================"
echo " [OK] All plugins compiled for ARM / Linux ($ARCH) successfully!"
echo " Deploy to Patchbox OS by copying dist/*.lv2 to /var/modep/lv2/  "
echo "================================================================"
