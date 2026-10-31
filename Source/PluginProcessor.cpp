#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vx;

VocXProcessor::VocXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Voice", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain Carrier", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "VocX", params::createLayout())
{
    pMode = p ("mode"); pSource = p ("source"); pChord = p ("chord"); pScale = p ("scale"); pKey = p ("key");
    pPoly = p ("poly"); pDrone = p ("drone"); pOctave = p ("octave"); pGlide = p ("glide");
    pSaw = p ("saw"); pPulse = p ("pulse"); pPw = p ("pw"); pSub = p ("sub"); pNoise = p ("noise");
    pUnison = p ("unison"); pDetune = p ("detune"); pAtk = p ("attack"); pDec = p ("decay"); pSus = p ("sustain");
    pRel = p ("release"); pVibRate = p ("vibRate"); pVibDepth = p ("vibDepth");
    pBands = p ("bands"); pLow = p ("lowHz"); pHigh = p ("highHz"); pBw = p ("bandwidth"); pFormant = p ("formant");
    pEnvAtk = p ("envAttack"); pEnvRel = p ("envRelease"); pEmph = p ("emphasis"); pSib = p ("sibilance");
    pWidth = p ("width"); pInGain = p ("inGain"); pGate = p ("gate"); pFreeze = p ("freeze");
    pDrive = p ("drive"); pLofi = p ("lofi"); pEnsMix = p ("ensMix"); pEnsDepth = p ("ensDepth");
    pDlyMix = p ("dlyMix"); pDlyTime = p ("dlyTime"); pDlyFb = p ("dlyFb"); pDlyPing = p ("dlyPing");
    pRevMix = p ("revMix"); pRevSize = p ("revSize"); pDry = p ("dry"); pWet = p ("wet"); pOut = p ("out");

    for (auto& b : bandLevels) b.store (0.0f);
}

bool VocXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono())
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono())
            return false;
    }
    return true;
}

void VocXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    vocoder.prepare (sr);
    synth.prepare (sr);
    pitch.prepare (sr);
    driveLoFi.prepare (sr);
    ensemble.prepare (sr);
    delay.prepare (sr);
    reverb.setSampleRate (sr);
    reverb.reset();

    const int n = juce::jmax (samplesPerBlock, 512);
    dryBuffer.setSize (2, n);
    modBuffer.assign ((size_t) n, 0.0f);
    carBuffer.assign ((size_t) n, 0.0f);

    heldCount = 0;
    autoActive = false;
    autoPitch = -1.0f;
    lastMode = -1;
    modLevel = 0.0f;
}

//==============================================================================
float VocXProcessor::quantize (float midiPitch) const
{
    const int scale = (int) pScale->load();
    if (scale == 0)
        return midiPitch;

    const int key = (int) pKey->load();
    static const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minor[] = { 0, 2, 3, 5, 7, 8, 10 };

    if (scale == 1)
        return std::round (midiPitch);

    const int* deg = scale == 2 ? major : minor;
    float best = std::round (midiPitch);
    float bestDist = 1000.0f;
    const int base = (int) std::floor (midiPitch) - 12;
    for (int n = base; n <= base + 24; ++n)
    {
        const int pc = ((n - key) % 12 + 12) % 12;
        bool inScale = false;
        for (int i = 0; i < 7; ++i) if (deg[i] == pc) inScale = true;
        if (! inScale) continue;
        const float d = std::abs ((float) n - midiPitch);
        if (d < bestDist) { bestDist = d; best = (float) n; }
    }
    return best;
}

void VocXProcessor::startHeldNote (int note, float vel, bool retrigger)
{
    const int oct = (int) pOctave->load() * 12;
    const int poly = (int) pPoly->load();
    for (int k = 0; k < (int) intervals.size(); ++k)
    {
        const float pitchK = (float) (note + intervals[(size_t) k] + oct);
        if (poly == 2 && ! retrigger && synth.countHeld (kMonoKey) == (int) intervals.size())
            synth.retune (kMonoKey, k, pitchK);
        else
            synth.keyOn (kMonoKey, k, pitchK, vel, retrigger);
    }
    detectedNote.store ((float) (note + oct));
}

void VocXProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        synth.allOff();
        heldCount = 0;
        return;
    }

    if ((int) pMode->load() != 0)
        return; // MIDI notes only drive the carrier in MIDI mode

    const int poly = (int) pPoly->load();
    const int oct = (int) pOctave->load() * 12;

    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        const float vel = m.getFloatVelocity();
        if (poly == 0)
        {
            for (int k = 0; k < (int) intervals.size(); ++k)
                synth.keyOn (note, k, (float) (note + intervals[(size_t) k] + oct), vel, true);
            detectedNote.store ((float) (note + oct));
        }
        else
        {
            const bool wasHeld = heldCount > 0;
            if (heldCount < (int) heldStack.size())
                heldStack[(size_t) heldCount++] = note;
            heldVelocity = vel;
            startHeldNote (note, vel, poly == 1 || ! wasHeld);
        }
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        if (poly == 0)
        {
            synth.keyOff (note);
        }
        else
        {
            int w = 0;
            for (int i = 0; i < heldCount; ++i)
                if (heldStack[(size_t) i] != note) heldStack[(size_t) w++] = heldStack[(size_t) i];
            heldCount = w;
            if (heldCount > 0)
                startHeldNote (heldStack[(size_t) heldCount - 1], heldVelocity, poly == 1);
            else
                synth.keyOff (kMonoKey);
        }
    }
}

// runs every 32 samples: follow-voice and drone carriers
void VocXProcessor::controlUpdate()
{
    const int mode = (int) pMode->load();
    if (mode == 0)
        return;

    const int oct = (int) pOctave->load() * 12;

    if (mode == 1 && pitch.isVoiced())
    {
        const float f = pitch.getFrequency();
        if (f > 50.0f && f < 1200.0f)
            autoPitch = quantize (69.0f + 12.0f * std::log2 (f / 440.0f));
    }
    else if (mode == 2)
    {
        autoPitch = (float) (int) pDrone->load();
    }

    if (gateOpen && autoPitch > 0.0f)
    {
        const float root = autoPitch + (float) oct;
        if (autoActive && synth.countHeld (kAutoKey) == (int) intervals.size())
        {
            for (int k = 0; k < (int) intervals.size(); ++k)
                synth.retune (kAutoKey, k, root + (float) intervals[(size_t) k]);
        }
        else
        {
            synth.keyOff (kAutoKey);
            for (int k = 0; k < (int) intervals.size(); ++k)
                synth.keyOn (kAutoKey, k, root + (float) intervals[(size_t) k], 0.9f, true);
            autoActive = true;
        }
        detectedNote.store (root);
    }
    else if (autoActive && ! gateOpen)
    {
        synth.keyOff (kAutoKey);
        autoActive = false;
        detectedNote.store (-1.0f);
    }
}

//==============================================================================
void VocXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (n > (int) modBuffer.size())   // host gave a bigger block than announced
    {
        dryBuffer.setSize (2, n, false, false, true);
        modBuffer.resize ((size_t) n);
        carBuffer.resize ((size_t) n);
    }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    // --- parameters (per block)
    const int mode = (int) pMode->load();
    const int chord = (int) pChord->load();
    const int poly = (int) pPoly->load();
    if (mode != lastMode || chord != lastChord || poly != lastPoly)
    {
        synth.allOff();
        heldCount = 0;
        autoActive = false;
        intervals = params::chordIntervals (chord);   // small vector, only on change
        if (mode != lastMode) { autoPitch = -1.0f; detectedNote.store (-1.0f); }
        lastMode = mode; lastChord = chord; lastPoly = poly;
    }

    CarrierSynth::Settings cs;
    cs.saw = pSaw->load(); cs.pulse = pPulse->load(); cs.pulseWidth = pPw->load(); cs.sub = pSub->load();
    cs.noise = pNoise->load(); cs.unison = (int) pUnison->load(); cs.detuneCents = pDetune->load();
    cs.attackMs = pAtk->load(); cs.decayMs = pDec->load(); cs.sustain = pSus->load(); cs.releaseMs = pRel->load();
    cs.glideMs = juce::jmax (pGlide->load(), mode == 1 && pScale->load() < 0.5f ? 12.0f : 0.0f);
    cs.vibRate = pVibRate->load(); cs.vibDepth = pVibDepth->load();
    cs.mono = poly != 0;
    synth.setSettings (cs);

    Vocoder::Settings vs;
    vs.bands = (int) pBands->load(); vs.lowHz = pLow->load(); vs.highHz = pHigh->load();
    vs.bandwidth = pBw->load(); vs.formantSemis = pFormant->load(); vs.emphasis = pEmph->load();
    vs.width = pWidth->load(); vs.attackMs = pEnvAtk->load(); vs.releaseMs = pEnvRel->load();
    vs.sibilance = pSib->load(); vs.freeze = pFreeze->load() > 0.5f;
    vocoder.update (vs);

    driveLoFi.set (pDrive->load(), pLofi->load());
    ensemble.set (pEnsMix->load(), pEnsDepth->load());

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = juce::jlimit (30.0, 300.0, *b);
    delay.set (params::delayBeats ((int) pDlyTime->load()) * 60.0 / bpm, pDlyFb->load(), pDlyMix->load(), pDlyPing->load() > 0.5f);

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.68f * pRevSize->load();
    rp.damping = 0.45f;
    rp.wetLevel = pRevMix->load() * 0.7f;
    rp.dryLevel = 1.0f - pRevMix->load() * 0.45f;
    rp.width = 1.0f;
    reverb.setParameters (rp);

    const float inGain = dbToGain (pInGain->load());
    const float gateThresh = dbToGain (pGate->load());
    const float wet = pWet->load();
    const float dry = pDry->load();
    const float outGain = dbToGain (pOut->load());
    const int source = (int) pSource->load();

    // --- inputs
    auto mainIn = getBusBuffer (buffer, true, 0);
    const int inCh = mainIn.getNumChannels();
    for (int c = 0; c < 2; ++c)
        dryBuffer.copyFrom (c, 0, mainIn, juce::jmin (c, inCh - 1), 0, n);

    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = (inCh > 1 ? 0.5f * (dryBuffer.getSample (0, i) + dryBuffer.getSample (1, i)) : dryBuffer.getSample (0, i)) * inGain;
        modBuffer[(size_t) i] = m;
        inPeak = juce::jmax (inPeak, std::abs (m));
    }

    std::fill (carBuffer.begin(), carBuffer.begin() + n, 0.0f);
    if (source != 0 && getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        const int scCh = sc.getNumChannels();
        for (int i = 0; i < n && scCh > 0; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < scCh; ++c) s += sc.getSample (c, i);
            carBuffer[(size_t) i] = s / (float) scCh;
        }
    }

    // --- main loop
    auto out = getBusBuffer (buffer, false, 0);
    float* outL = out.getWritePointer (0);
    float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const float levelRelease = std::exp (-1.0f / (float) (0.05 * sr));
    const int holdSamples = (int) (0.12 * sr);

    auto midiIt = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        const float mod = modBuffer[(size_t) i];
        const float a = std::abs (mod);
        modLevel = a > modLevel ? a : modLevel * levelRelease;
        if (modLevel > gateThresh) { gateOpen = true; gateHold = holdSamples; }
        else if (gateHold > 0) --gateHold;
        else gateOpen = false;

        if (mode == 1)
            pitch.push (mod);

        if (++controlCounter >= 32)
        {
            controlCounter = 0;
            controlUpdate();
        }

        float car = carBuffer[(size_t) i];
        if (source != 1)
            car += synth.render();

        float l, r;
        vocoder.process (gateOpen || mode == 0 ? mod : 0.0f, car, l, r);
        l *= wet; r *= wet;
        driveLoFi.process (l, r);
        ensemble.process (l, r);
        delay.process (l, r);

        outL[i] = l;
        if (outR) outR[i] = r;
        else outL[i] = 0.5f * (l + r);
    }

    if (outR) reverb.processStereo (outL, outR, n);
    else reverb.processMono (outL, n);

    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        outL[i] = (outL[i] + dry * dryBuffer.getSample (0, i)) * outGain;
        outPeak = juce::jmax (outPeak, std::abs (outL[i]));
        if (outR)
        {
            outR[i] = (outR[i] + dry * dryBuffer.getSample (1, i)) * outGain;
            outPeak = juce::jmax (outPeak, std::abs (outR[i]));
        }
    }

    // clear any extra output channels
    for (int c = 2; c < out.getNumChannels(); ++c)
        out.clear (c, 0, n);

    // --- meters
    numBandsShown.store (vocoder.getNumBands());
    for (int b = 0; b < vocoder.getNumBands(); ++b)
        bandLevels[(size_t) b].store (vocoder.getBandEnvelope (b));
    inputLevel.store (juce::jmax (inPeak, inputLevel.load() * 0.8f));
    outputLevel.store (juce::jmax (outPeak, outputLevel.load() * 0.8f));
    gateOpenFlag.store (gateOpen);
    if (mode == 0 && ! synth.anyActive())
        detectedNote.store (-1.0f);
}

//==============================================================================
void VocXProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentPreset = index;
    applyPreset (apvts, index);
}

const juce::String VocXProcessor::getProgramName (int index)
{
    const auto& list = factoryPresets();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

void VocXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("preset", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* VocXProcessor::createEditor()
{
    return new VocXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocXProcessor();
}
