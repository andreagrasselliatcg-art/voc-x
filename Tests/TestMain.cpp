// Voc-X headless tests: synthetic voice, all presets, pitch tracking, leak test, CPU, state, GUI snapshot.
#include "PluginProcessor.h"
#include "PluginEditor.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } else std::cout << "ok:   " << msg << "\n"; } while (0)

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 256;

// "a" vowel: glottal pulses through 3 formants, syllables, and an "s" burst every 2nd syllable
static juce::AudioBuffer<float> makeVoice (double seconds, float f0)
{
    const int n = (int) (seconds * SR);
    juce::AudioBuffer<float> b (1, n);
    vx::Biquad f1, f2, f3, hp;
    f1.setBandpass (SR, 730, 8); f2.setBandpass (SR, 1090, 10); f3.setBandpass (SR, 2440, 12);
    hp.setHighpass (SR, 5000);
    double ph = 0;
    juce::Random rng (7);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / SR;
        const double syl = std::fmod (t, 0.4);
        const int sylIndex = (int) (t / 0.4);
        const float env = syl < 0.3 ? (float) std::sin (juce::MathConstants<double>::pi * syl / 0.3) : 0.0f;
        const double f = f0 * (1.0 + 0.003 * std::sin (2 * juce::MathConstants<double>::pi * 5 * t));
        ph += f / SR;
        float pulse = 0.0f;
        if (ph >= 1.0) { ph -= 1.0; pulse = 1.0f; }
        float v = f1.process (pulse) * 1.0f + f2.process (pulse) * 0.6f + f3.process (pulse) * 0.3f;
        v *= env * 8.0f;
        if ((sylIndex % 2) == 1 && syl > 0.3 && syl < 0.38)
            v += hp.process (rng.nextFloat() * 2 - 1) * 0.15f;
        b.setSample (0, i, v);
    }
    return b;
}

static float rmsDb (const juce::AudioBuffer<float>& b, int start, int len)
{
    double s = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = start; i < start + len; ++i) s += b.getSample (c, i) * b.getSample (c, i);
    s /= (double) (len * b.getNumChannels());
    return (float) (10.0 * std::log10 (s + 1e-20));
}

static juce::AudioBuffer<float> run (VocXProcessor& p, const juce::AudioBuffer<float>& voice, std::vector<int> midiNotes)
{
    const int n = voice.getNumSamples();
    juce::AudioBuffer<float> out (2, n);
    const bool sc = p.getBusCount (true) > 1 && p.getBus (true, 1)->isEnabled();
    juce::AudioBuffer<float> buf (sc ? 4 : 2, BLOCK);
    double scPhase = 0.0;
    for (int pos = 0; pos + BLOCK <= n; pos += BLOCK)
    {
        for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, voice, 0, pos, BLOCK);
        if (sc)   // external carrier: a 110 Hz saw on the sidechain
            for (int i = 0; i < BLOCK; ++i)
            {
                scPhase += 110.0 / SR; if (scPhase >= 1.0) scPhase -= 1.0;
                buf.setSample (2, i, (float) (scPhase * 2.0 - 1.0) * 0.5f);
                buf.setSample (3, i, (float) (scPhase * 2.0 - 1.0) * 0.5f);
            }
        juce::MidiBuffer midi;
        if (pos == 0)
            for (int note : midiNotes) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, BLOCK);
    }
    return out;
}

static void writeWav (const juce::AudioBuffer<float>& b, const juce::String& path)
{
    juce::File f (path);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), SR, (unsigned) b.getNumChannels(), 24, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto voice = makeVoice (4.0, 150.0f);
    std::cout << "voice rms " << rmsDb (voice, 0, voice.getNumSamples()) << " dBFS\n";
    writeWav (voice, "/tmp/vx_voice.wav");

    // --- every preset
    const auto& presets = vx::factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        VocXProcessor p;
        p.setPlayConfigDetails (2, 2, SR, BLOCK);
        p.setCurrentProgram (i);
        if (p.apvts.getRawParameterValue ("source")->load() > 0.5f)
        {
            auto layout = p.getBusesLayout();
            layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
            CHECK (p.setBusesLayout (layout), "sidechain bus can be enabled");
        }
        p.prepareToPlay (SR, BLOCK);
        const bool midiMode = p.apvts.getRawParameterValue ("mode")->load() < 0.5f;
        auto out = run (p, voice, midiMode ? std::vector<int> { 48, 52, 55 } : std::vector<int> {});
        const float r = rmsDb (out, (int) SR, (int) (2.5 * SR));
        const float pk = out.getMagnitude (0, out.getNumSamples());
        std::cout << "   " << juce::String (presets[(size_t) i].name).paddedRight (' ', 30) << " rms " << juce::String (r, 1)
                  << " dB  peak " << juce::String (pk, 2) << "  note " << p.detectedNote.load() << "\n";
        CHECK (finite (out) && r > -36.0f && r < -3.0f && pk < 1.5f, juce::String ("preset level ok: ") + presets[(size_t) i].name);
        if (i == 1 || i == 3 || i == 9 || i == 10)
            writeWav (out, "/tmp/vx_preset_" + juce::String (i) + ".wav");
    }

    // --- vocoder-only level (no sibilance, no fx) should be close to the voice level
    {
        VocXProcessor p;
        p.setPlayConfigDetails (2, 2, SR, BLOCK);
        p.setCurrentProgram (1);
        auto set = [&] (const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set ("sibilance", 0.0f); set ("revMix", 0.0f);
        for (int bands : { 8, 20, 40 })
        {
            set ("bands", (float) bands);
            for (float bw : { 0.0f, 0.5f, 1.0f })
            {
                set ("bandwidth", bw);
                p.prepareToPlay (SR, BLOCK);
                auto out = run (p, voice, { 48 });
                std::cout << "   vocoder only, " << bands << " bands, bw " << bw << ": " << rmsDb (out, (int) SR, (int) (2.5 * SR)) << " dB\n";
            }
        }
    }

    // --- pitch follow: 150 Hz voice -> ~D3 (50.37)
    {
        VocXProcessor p;
        p.setPlayConfigDetails (2, 2, SR, BLOCK);
        p.setCurrentProgram (0);
        p.prepareToPlay (SR, BLOCK);
        float noteSeen = -1;
        auto v = makeVoice (1.0, 150.0f);
        juce::AudioBuffer<float> buf (2, BLOCK);
        for (int pos = 0; pos + BLOCK <= v.getNumSamples(); pos += BLOCK)
        {
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, v, 0, pos, BLOCK);
            juce::MidiBuffer m;
            p.processBlock (buf, m);
            if (pos > SR * 0.15 && pos < SR * 0.25) noteSeen = p.detectedNote.load();
        }
        CHECK (std::abs (noteSeen - 50.37f) < 0.4f, "pitch follow 150 Hz -> " + juce::String (noteSeen));

        // hard tune chromatic -> 50
        p.apvts.getParameter ("scale")->setValueNotifyingHost (p.apvts.getParameter ("scale")->convertTo0to1 (1));
        for (int pos = 0; pos + BLOCK <= v.getNumSamples(); pos += BLOCK)
        {
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, v, 0, pos, BLOCK);
            juce::MidiBuffer m;
            p.processBlock (buf, m);
            if (pos > SR * 0.15 && pos < SR * 0.25) noteSeen = p.detectedNote.load();
        }
        CHECK (std::abs (noteSeen - 50.0f) < 0.01f, "hard tune snaps to D3 = " + juce::String (noteSeen));
    }

    // --- MIDI mode, no voice: carrier must not leak
    {
        VocXProcessor p;
        p.setPlayConfigDetails (2, 2, SR, BLOCK);
        p.setCurrentProgram (1);
        p.apvts.getParameter ("revMix")->setValueNotifyingHost (0.0f);
        p.prepareToPlay (SR, BLOCK);
        juce::AudioBuffer<float> silence (1, (int) SR);
        silence.clear();
        auto out = run (p, silence, { 48, 55 });
        CHECK (rmsDb (out, BLOCK * 8, (int) SR - BLOCK * 9) < -90.0f, "no carrier leak without voice");
    }

    // --- CPU worst case: 40 bands, unison 7, 4-note chord x 4 MIDI notes
    {
        VocXProcessor p;
        p.setPlayConfigDetails (2, 2, SR, BLOCK);
        p.setCurrentProgram (1);
        auto set = [&] (const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set ("bands", 40); set ("unison", 7); set ("pulse", 1); set ("chord", 5); set ("ensMix", 0.5f); set ("dlyMix", 0.3f); set ("revMix", 0.3f);
        p.prepareToPlay (SR, BLOCK);
        auto v = makeVoice (10.0, 150.0f);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (p, v, { 48, 52, 55, 59 });
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::cout << "CPU worst case: 10 s audio in " << ms << " ms  (" << juce::String (ms / 100.0, 1) << "% of one core)\n";
        CHECK (ms < 4000.0, "realtime capable");
    }

    // --- state round trip
    {
        VocXProcessor a, b;
        a.setCurrentProgram (5);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (b.getCurrentProgram() == 5 && b.apvts.getRawParameterValue ("chord")->load() == a.apvts.getRawParameterValue ("chord")->load(), "state restore");
    }

    // --- GUI snapshot
    {
        VocXProcessor p;
        p.setPlayConfigDetails (2, 2, SR, BLOCK);
        p.setCurrentProgram (3);
        p.prepareToPlay (SR, BLOCK);
        run (p, makeVoice (0.6, 150.0f), {});
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        for (int i = 0; i < 10; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File png ("/tmp/vx_gui.png");
        png.deleteFile();
        juce::FileOutputStream os (png);
        juce::PNGImageFormat().writeImageToStream (img, os);
        CHECK (png.getSize() > 1000, "gui snapshot");
    }

    std::cout << "\n" << (failures == 0 ? juce::String ("ALL TESTS PASSED") : juce::String (failures) + " FAILURES") << "\n";
    return failures == 0 ? 0 : 1;
}
