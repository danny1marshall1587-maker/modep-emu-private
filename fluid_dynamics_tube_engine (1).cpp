#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>

class FluidTubeDSP {
private:
    // System parameters
    float sampleRate;
    float timeStep;
    
    // Fluid / Pressure state variables
    float previousPressure;      // The previous audio sample
    float fluidMomentum;         // Velocity of the pressure wave
    float chamberPressure;       // Represents the "sag" or power supply voltage
    
    // Virtual physical constants for the "Perfect Tube"
    const float fluidViscosity = 0.85f;    // Controls how quickly energy dissipates
    const float harmonicPurity = 0.4f;     // Ratio of pure even-order harmonics
    const float chamberRecovery = 0.002f;  // How fast the "power supply" recovers
    
    // Internal limits to maintain mathematical stability
    const float maxPressure = 1.0f;
    
    // Synergistic memory: What harmonics did this tube generate?
    float generatedHarmonics; 

public:
    FluidTubeDSP() : sampleRate(48000.0f), previousPressure(0.0f), 
                     fluidMomentum(0.0f), chamberPressure(1.0f), generatedHarmonics(0.0f) {}

    void prepare(float newSampleRate) {
        sampleRate = newSampleRate;
        timeStep = 1.0f / sampleRate;
        reset();
    }

    void reset() {
        previousPressure = 0.0f;
        fluidMomentum = 0.0f;
        chamberPressure = 1.0f;
        generatedHarmonics = 0.0f;
    }

    float processSample(float inputPressure, float driveLevel, float incomingHarmonicSynergy = 0.0f) {
        // 1. Calculate Fluid Velocity & Inertia
        float currentVelocity = inputPressure - previousPressure;
        fluidMomentum = (fluidMomentum * fluidViscosity) + (currentVelocity * (1.0f - fluidViscosity));

        // 2. Chamber Pressure (Dynamic Sag)
        float energyDraw = std::abs(inputPressure) * driveLevel;
        chamberPressure -= energyDraw * 0.01f;
        chamberPressure += (1.0f - chamberPressure) * chamberRecovery;
        chamberPressure = std::max(0.1f, std::min(chamberPressure, 1.0f));

        // 3. Synergistic Harmonic Generation
        // We inject the harmonic energy from the PREVIOUS tube (incomingHarmonicSynergy)
        // to guide the phase of this tube's distortion, compounding the "pure" divisions.
        float amplifiedSignal = inputPressure * driveLevel * chamberPressure;
        
        // Add the harmonic slipstream from the last tube
        amplifiedSignal += incomingHarmonicSynergy * 0.2f; 
        
        float outputPhase;
        if (amplifiedSignal > 0.0f) {
            outputPhase = std::tanh(amplifiedSignal) + (harmonicPurity * std::pow(amplifiedSignal, 2.0f) * 0.5f);
        } else {
            outputPhase = std::tanh(amplifiedSignal) - (harmonicPurity * std::pow(std::abs(amplifiedSignal), 2.5f) * 0.2f);
        }

        // Calculate exactly how much pure harmonic energy we just generated
        float linearOutput = amplifiedSignal; // What it would be without tube physics
        generatedHarmonics = outputPhase - std::tanh(linearOutput);

        // 4. Inject Fluid Momentum
        outputPhase += fluidMomentum * 0.5f;
        float finalOutput = std::tanh(outputPhase);
        previousPressure = finalOutput;

        return finalOutput;
    }

    float getGeneratedHarmonics() const {
        return generatedHarmonics;
    }
};

class FluidSpeakerDSP {
private:
    float sampleRate;
    float timeStep;
    
    // Mechanical Speaker Physics
    float conePosition;
    float coneVelocity;
    
    // Virtual physical parameters for the speaker cone
    const float coneMass = 0.005f;       // Inertia of the physical paper/magnet
    const float suspensionStiffness = 0.8f; // How hard the spider/surround pulls back
    const float airDamping = 0.1f;       // Air resistance pushing against the cone
    const float coneCryThreshold = 0.7f; // Excursion limit before paper begins to ripple
    
public:
    FluidSpeakerDSP() : sampleRate(48000.0f), timeStep(1.0f/48000.0f), 
                        conePosition(0.0f), coneVelocity(0.0f) {}
                        
    void prepare(float newSampleRate) {
        sampleRate = newSampleRate;
        timeStep = 1.0f / sampleRate;
        conePosition = 0.0f;
        coneVelocity = 0.0f;
    }
    
    float processSample(float acousticPressure) {
        // 1. Mechanical Acceleration (Force = Mass * Acceleration)
        // The electrical acousticPressure acts as the force from the voice coil
        float restoringForce = -suspensionStiffness * conePosition;
        float dampingForce = -airDamping * coneVelocity;
        
        float totalForce = acousticPressure + restoringForce + dampingForce;
        float acceleration = totalForce / coneMass;
        
        // 2. Integrate to find Velocity and Position
        coneVelocity += acceleration * timeStep;
        conePosition += coneVelocity * timeStep;
        
        // 3. "Cone Cry" and Paper Stress Fluid Simulation
        // When the cone moves too far, the edges of the paper buckle and ripple.
        // Instead of ugly digital clipping, we use mathematical wavefolding to model 
        // the physical folding of the paper membrane, creating a euphoric, tearing harmonic.
        float outputAcoustics = conePosition;
        
        if (std::abs(conePosition) > coneCryThreshold) {
            // Calculate how far past the limit the cone is being pushed
            float excessStress = std::abs(conePosition) - coneCryThreshold;
            
            // The paper "folds" back against the fluid air, creating a secondary ripple
            // We use a clean sine function based on velocity to ensure it aligns with the fluid motion
            float paperRipple = std::sin(excessStress * 15.0f) * coneVelocity * 0.1f;
            
            // Add the physical cone cry to the acoustic output
            outputAcoustics -= (conePosition > 0 ? excessStress : -excessStress) * 0.2f; 
            outputAcoustics += paperRipple;
        }
        
        // Soft limit the absolute physical excursion of the speaker
        return std::tanh(outputAcoustics * 2.0f);
    }
};

class MultiStageFluidAmp {
private:
    std::vector<FluidTubeDSP> tubeStages;
    FluidSpeakerDSP speakerModel;
    float interStageAttenuation; 

public:
    MultiStageFluidAmp(int numStages, float attenuation = 0.5f) {
        tubeStages.resize(numStages);
        interStageAttenuation = attenuation;
    }

    void prepare(float sampleRate) {
        for (auto& tube : tubeStages) {
            tube.prepare(sampleRate);
        }
        speakerModel.prepare(sampleRate);
    }

    void reset() {
        for (auto& tube : tubeStages) {
            tube.reset();
        }
    }

    float processSample(float inputSignal, float initialDrive) {
        float currentPressure = inputSignal;
        float cascadingHarmonics = 0.0f; // Stores the overtone slipstream
        
        for (size_t i = 0; i < tubeStages.size(); ++i) {
            float stageDrive = (i == 0) ? initialDrive : 2.0f; 
            
            // Pass the generated harmonics from the previous tube into the next one
            // This creates the "synergy" where even-order harmonics compound perfectly
            currentPressure = tubeStages[i].processSample(currentPressure, stageDrive, cascadingHarmonics);
            
            // Capture this tube's harmonic reflection to feed the next stage
            cascadingHarmonics = tubeStages[i].getGeneratedHarmonics();
            
            if (i < tubeStages.size() - 1) {
                currentPressure *= interStageAttenuation; 
            }
        }
        
        // Final Stage: Push the synergistic fluid output through the physical speaker emulation
        // This converts the electrical fluid model into a mechanical/acoustic fluid model
        float finalAcousticOutput = speakerModel.processSample(currentPressure);
        
        return finalAcousticOutput;
    }
};

int main() {
    std::cout << "Initializing Synergistic Fluid Amp with Mechanical Speaker Emulation..." << std::endl;
    
    // 3-tube architecture with perfectly linked harmonic stages
    MultiStageFluidAmp ampHead(3, 0.45f); 
    ampHead.prepare(48000.0f); 

    const int numSamples = 20;
    float frequency = 432.0f; 
    float drive = 4.5f; // Pushing even harder to trigger Speaker Cone Cry
    
    std::cout << "Processing 432Hz Test Wave at Drive Level " << drive << "...\n" << std::endl;
    std::cout << "Input Pressure\t|\tAcoustic Output\t|\tTotal Harmonic Change" << std::endl;
    std::cout << "--------------------------------------------------------------" << std::endl;

    for (int i = 0; i < numSamples; ++i) {
        float time = i / 48000.0f;
        float inputSignal = std::sin(2.0f * M_PI * frequency * time);
        
        // Process through synergistic tubes AND the mechanical speaker
        float outputSignal = ampHead.processSample(inputSignal, drive);
        
        std::cout << inputSignal << "\t\t|\t" 
                  << outputSignal << "\t\t|\t" 
                  << (outputSignal - inputSignal) << std::endl;
    }

    std::cout << "\nSynergistic DSP and Mechanical Speaker tests complete." << std::endl;
    return 0;
}