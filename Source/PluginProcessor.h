#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/Vocoder.h"
#include "dsp/CarrierSynth.h"
#include "dsp/PitchTracker.h"
#include "dsp/Effects.h"

class VocXProcessor : public juce::AudioProcessor
{
public:
    VocXProcessor();
    ~VocXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return (int) vx::factoryPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // meters for the GUI
    std::array<std::atomic<float>, vx::Vocoder::kMaxBands> bandLevels {};
    std::atomic<int> numBandsShown { 20 };
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };
    std::atomic<float> detectedNote { -1.0f };   // MIDI pitch of the carrier root, -1 = none
    std::atomic<bool> gateOpenFlag { false };

private:
    void handleMidi (const juce::MidiMessage& m);
    void controlUpdate();
    void startHeldNote (int note, float vel, bool retrigger);
    float quantize (float midiPitch) const;

    static constexpr int kMonoKey = 1000;
    static constexpr int kAutoKey = 1001;

    vx::Vocoder vocoder;
    vx::CarrierSynth synth;
    vx::PitchTracker pitch;
    vx::DriveLoFi driveLoFi;
    vx::Ensemble ensemble;
    vx::StereoDelay delay;
    juce::Reverb reverb;

    juce::AudioBuffer<float> dryBuffer;
    std::vector<float> modBuffer, carBuffer;

    double sr = 44100.0;
    int currentPreset = 0;

    // state
    int lastMode = -1, lastChord = -1, lastPoly = -1;
    std::array<int, 16> heldStack {};
    int heldCount = 0;
    float heldVelocity = 0.8f;
    float modLevel = 0.0f;
    int gateHold = 0;
    bool gateOpen = false;
    float autoPitch = -1.0f;
    bool autoActive = false;
    int controlCounter = 0;
    std::vector<int> intervals { 0 };

    // cached parameter pointers
    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }
    std::atomic<float> *pMode, *pSource, *pChord, *pScale, *pKey, *pPoly, *pDrone, *pOctave, *pGlide,
        *pSaw, *pPulse, *pPw, *pSub, *pNoise, *pUnison, *pDetune, *pAtk, *pDec, *pSus, *pRel, *pVibRate, *pVibDepth,
        *pBands, *pLow, *pHigh, *pBw, *pFormant, *pEnvAtk, *pEnvRel, *pEmph, *pSib, *pWidth, *pInGain, *pGate, *pFreeze,
        *pDrive, *pLofi, *pEnsMix, *pEnsDepth, *pDlyMix, *pDlyTime, *pDlyFb, *pDlyPing, *pRevMix, *pRevSize,
        *pDry, *pWet, *pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocXProcessor)
};
