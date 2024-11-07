#pragma once

class ProgSynthDrums : public Program
{
    using this_t = ProgSynthDrums;

// TODO: Lots more drum settings!
// TODO: Accent control by CV - but that messes up how the gates and parameters work

    #define PARAM_VALUES(ITEM) \
        ITEM(HhOpen, "Hihat open/close") \
        ITEM(HhDecay, "Hihat decay") \
        ITEM(HhAccent, "Hihat accent") \
        ITEM(BassAccent, "Bass accent") \
        ITEM(SnareAccent, "Snare accent") \
        ITEM(AllAccent, "All accent")
    DECL_PARAM_VALUES(KnobControl)
    #undef PARAM_VALUES

    #define PROG_PARAMS(ITEM) \
        PARAM_GATESOURCE(ITEM, HihatGate, "Hihat gate", Button) \
        PARAM_GATESOURCE(ITEM, BassGate, "Bass gate", CV1) \
        PARAM_GATESOURCE(ITEM, SnareGate, "Snare gate", CV2) \
        PARAM_NUM(ITEM, KnobControl, "Knob control", 0)
    DECL_PROG_PARAMS
    #undef PROG_PARAMS

public:
    constexpr std::string_view GetName() const override { return "Drums - Synth"sv; }

    void Init() override
    {
        theProgram = this; // DEBUG

        // TODO: Need better settings for all the drums
        auto sampleRate = HW::seed.AudioSampleRate();
        bass.Init(sampleRate);
        SetBassSettings();
        snare.Init(sampleRate);
        SetSnareSettings();
        hihat.Init(sampleRate);
        SetHhSettings();
    }

    void Process(ProcessArgs& args) override
    {
        // Update drum settings according to the current potentiometer value
        auto pot = HW::CVIn::GetUnipolar(HW::CVIn::Pot).value_or(0);
        KnobControl knob = KnobControl(GetKnobControl());
        UpdateHhSettings(knob, pot);
        UpdateBassSettings(knob, pot);
        UpdateSnareSettings(knob, pot);

        // Check for drum triggers
        if (args.GateOn(GetSnareGate()))
            snare.Trig();
        if (args.GateOn(GetBassGate()))
            bass.Trig();
        // For the hihat, check if it should open/close on gate on/off
        if (args.GateOn(GetHihatGate())) {
            if (hhSettings.open) {
                hhSettings.isOpen = true;
                hihat.SetDecay(decayHhOpen);
            }
            hihat.Trig();
        }
        if (args.GateOff(GetHihatGate())) {
            hhSettings.isOpen = false;
            hihat.SetDecay(hhSettings.decay);
        }

		// Synth output
        float bassOut = 0;
        float snareOut = 0;
        float hihatOut = 0;
        for (auto&& out : args.outbuf) {
            bassOut = bass.Process();
            snareOut = snare.Process();
            hihatOut = hihat.Process();
            out.left = hihatOut + bassOut/2;
            out.right = snareOut + bassOut/2;
        }

        // Update the animation display with the last-calculated result
        animation.SetAmplitude(hihatOut, bassOut, snareOut);
    }

    // DEBUG
    float GetHhDecay() const { return hhSettings.decay; }

    Animation* GetAnimation() const override { return &animation; }

private:
    static constexpr float decayHhOpen = 1.175f;

    // DEBUG: This is mainly for testing. Probably remove it and have more limited settings
    // as defined in params.
    struct HhSettings {
        float freq = 3000;
        float tone = 0.5f;
        float decay = 0.635f;
        float noisy = 0.8f;
        float accent = 0.8f;
        bool open = false;
        bool isOpen = false;
    };

    HhSettings hhSettings;

    void SetHhSettings()
    {
        hihat.SetFreq(hhSettings.freq);
        hihat.SetTone(hhSettings.tone);
        // If hhSettings.open the decay is set on gate on/off.
        if (!hhSettings.open)
            hihat.SetDecay(hhSettings.decay);
        hihat.SetNoisiness(hhSettings.noisy);
        hihat.SetAccent(hhSettings.accent);
    }

    void UpdateHhSettings(KnobControl knob, float pot)
    {
        if (knob == KnobControl::HhAccent || knob == KnobControl::AllAccent) {
            hhSettings.accent = pot; // TODO: probably not needed
            hihat.SetAccent(pot);
        } else if (knob == KnobControl::HhOpen) {
            hhSettings.open = (pot > 0.5);
        } else if (knob == KnobControl::HhDecay) {
            hhSettings.decay = pot;
            if (!hhSettings.isOpen) {
                hihat.SetDecay(pot);
            }
        }
    }

    void SetBassSettings()
    {
        // Just use the default settings.
    }

    void UpdateBassSettings(KnobControl knob, float pot)
    {
        if (knob == KnobControl::BassAccent || knob == KnobControl::AllAccent) {
            bass.SetAccent(pot);
        }
    }

    void SetSnareSettings()
    {
        snare.SetSnappy(0.2f);
    }

    void UpdateSnareSettings(KnobControl knob, float pot)
    {
        if (knob == KnobControl::SnareAccent || knob == KnobControl::AllAccent) {
            snare.SetAccent(pot);
        }
    }

private:
    //daisysp::HiHat<> hihat;
    daisysp::HiHat<daisysp::RingModNoise> hihat;

    daisysp::SyntheticBassDrum bass;

    daisysp::SyntheticSnareDrum snare;

    static inline AnimAmplitude<3> animation;

protected:
    static inline this_t* theProgram = nullptr; // DEBUG: for DebugTask

public:
    friend class DebugTask;

    class DebugTask : public tasks::Task<DebugTask>
    {
    public:
        unsigned intervalMicros() const { return 1'000'000; }

        void init() { }

        void execute()
        {
            if (theProgram) {
                float decay = theProgram->GetHhDecay();
                auto [decayInt, decayFrac] = splitFloat(decay, 3);
                daisy2::DebugLog::PrintLine("decay=%d.%u", decayInt, decayFrac);
            }
        }
    };
};
