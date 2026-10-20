# Fluid Simulation Stability, Reverb Space & Fender Harmonic Tremolo Invariants

## 1. Non-Linear Polynomial Boundedness
- All polynomial expansions ($x^2$, $x^3$) representing corner eddy vortex stresses or asymmetric harmonics in fluid dynamics cells must operate strictly on bounded coordinates $x = \tanh(\text{signal}) \in (-1, 1)$.
- Unbounded polynomial evaluation directly on amplified drive voltages will lead to positive feedback loops and continuous Nyquist screeching at large chamber sizes ($\ge \times 2$) or heavy input drives.

## 2. Fluid Dynamics Reverb Space (`FluidReverbSpace`)
- **Chamber Shape**: Continuously morphs boundary reflections between pure radial specular modes (Circular Cylinder, Shape = 0) and turbulent modal diffusion with edge diffraction (Square Box, Shape $\to$ 1.0).
- **432 Hz Chamber Size**: Scales the fundamental acoustic delays along the 19-position natural 432 Hz period ladder ($\div 10 \dots \div 2$, $1\times$ center, $\times 2 \dots \times 10$).
- **Send Filtering**: Low-Cut ($40\text{ Hz} \dots 600\text{ Hz}$) and High-Cut ($2.5\text{ kHz} \dots 14\text{ kHz}$) filters condition the send signal *before* fluid non-linear drive to prevent low-end mud and pick splash.

## 3. Vintage Fender Fluid Space Harmonic Tremolo (`FluidHarmonicTremolo`)
- Emulates the 1960–1963 Fender Brownface/Blonde 6G-series circuit with a phase-aligned Linkwitz-Riley crossover splitting the signal at $\sim 750\text{ Hz}$.
- Low and High frequency bands are driven through twin Fluid Dynamics Tube Cells modulated in $180^\circ$ anti-phase by the LFO.
- Anti-phase non-linear fluid compression creates authentic tube warmth, dynamic sag, and three-dimensional Doppler pitch wobble without volume loss.
