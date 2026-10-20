# Non-Destructive Hiss Elimination, Discrete Channel EQs, Fluid Stompboxes & High-Gain Cascades

## 1. Non-Destructive High-Frequency Hiss Elimination
- Never apply static low-pass filtering that dulls guitar pick attack, treble chime, or harmonic brilliance.
- Hiss reduction must use dynamic program-dependent downward expansion and spectral subtraction, opening with instantaneous peak response ($\le 0.5\text{ ms}$) during notes to preserve 100% of guitar highs ($3\text{ kHz} \dots 15\text{ kHz}$), and clamping only the low-level noise floor as notes decay.
- The clean lab denoiser must be seeded with calibrated defaults (never inactive $-100\text{ dBFS}$), and post-power/post-cab dynamic hiss tracking must operate strictly below the guitar signal threshold ($\le -55\text{ dBFS}$).

## 2. Discrete Channel Tone Stacks
- In multi-channel guitar engines (Clean, Crunch, High-Gain Lead), channels must NEVER share tone stack state or GUI knobs.
- Each channel must expose its own calibrated Bass, Middle, and Treble controls matching its authentic hardware circuit topology (Blackface AB763 plate-driven vs Bassman/Plexi cathode-follower vs BE-100 lead).

## 3. Pre-Amp Stompbox Drive Staging
- Selectable overdrive pedals (Fluid Klon Centaur vs Tube Screamer TS808) must execute directly after the clean lab and before the tube preamp.
- All clipping and diode bridges must operate on bounded fluid non-linearities ($x = \tanh(\cdot)$) with authentic tone-stack filtering (Centaur active $4\text{ kHz}$ treble boost and germanium clipping; TS808 $720\text{ Hz}$ high-pass cut and $723\text{ Hz}$ active tone).

## 4. High-Gain British Rock Topology (Friedman BE-100 / Marshall JCM 800)
- Must incorporate the authentic 10k Cold Clipper (V1B) asymmetric transfer curve, optional HBE cascade, C45 voicing, and EL34 push-pull power stage.
