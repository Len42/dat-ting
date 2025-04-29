#pragma once

/// @brief Bitcrusher @ref Program
/// @details Bit depth and sample rate are set using the potentiometer.
/// @todo Flexible CV control of bit depth and sample rate
class ProgBitcrush : public Program
{
    using this_t = ProgBitcrush;

    // Declare the configurable parameters of this program
    #define PARAM_VALUES(ITEM) \
        ITEM(BitDepth, "Bit depth") \
        ITEM(SampleRate, "Sample rate")
    DECL_PARAM_VALUES(KnobControl)
    #undef PARAM_VALUES
    #define PROG_PARAMS(ITEM) \
        PARAM_NUM(ITEM, KnobControl, "Knob control", 0)
    DECL_PROG_PARAMS
    #undef PROG_PARAMS

public:
    constexpr std::string_view GetName() const override { return "Bitcrusher"sv; }

    void Init() override
    {
        theProgram = this;

        sampleRate = HW::seed.AudioSampleRate();
        sampler.Init(GetCrushRate(), sampleRate);
    }

    void Process(ProcessArgs& args) override
    {
        auto pot = HW::CVIn::GetUnipolar(HW::CVIn::Pot).value_or(0.25f);
        if (KnobControl(GetKnobControl()) == KnobControl::BitDepth) {
            unsigned bits = unsigned(std::round(pot * 17.f));
            SetBitDepth(bits);
        } else {
            float rate = pot * sampleRate;
            SetCrushRate(rate);
        }

        for (auto&& [in, out] : std::views::zip(args.inbuf, args.outbuf)) {
            if (sampler.Process()) {
                unsigned bitMask = 0xFFFFu << (16 - GetBitDepth());
                unsigned sample = std::abs(in.left) * 65536;
                sample &= bitMask;
                float flSample = float(sample) / 65536.f;
                lastSample.left = lastSample.right = std::copysign(flSample, in.left);
            }
            out = lastSample;
        }
    }

    Animation* GetAnimation() const override { return &animation; }

protected:
    unsigned GetBitDepth() const { return bitDepth; }

    void SetBitDepth(unsigned bits) { bitDepth = std::clamp(bits, 1u, 16u); }

    float GetCrushRate() const { return crushRate; }

    void SetCrushRate(float rate)
    {
        crushRate = std::clamp(rate, 40.f, sampleRate);
        sampler.SetFreq(crushRate);
    }

private:
    float sampleRate = 0;

    unsigned bitDepth = 8;

    float crushRate = 10000;

    daisy2::AudioSample lastSample{};

    daisysp::Metro sampler;

protected:
    static inline this_t* theProgram = nullptr; // for ProgAnimation and DebugTask

    /// @brief @ref Animation for @ref ProgBitcrush
    /// @details Displays a waveform with a symbolic (not accurate) representation
    /// of the bit depth and sample rate.
    class ProgAnimation : public Animation
    {
    public:
        void Init() override { }

        bool Step(unsigned step) override
        {
            HW::display.Fill(false);

            // Draw a crushed triangle wave, illustrating the current selected parameters
            unsigned bitDepth = theProgram ? (theProgram->GetBitDepth() / 4) : 4;
            float increment = theProgram ? (theProgram->GetCrushRate() / HW::sampleRate) : 1;
            int y = 0;
            int yStep = -1;
            int yCrushed = y;
            float t = 0;
            for (int x = 0; x < 128; ++x) {
                // calc the waveform
                int yNext = y + yStep;
                if (yNext < -16) {
                    yNext = -2*16 - yNext;
                    yStep = -yStep;
                } else if (yNext > 16) {
                    yNext = 2*16 - yNext;
                    yStep = -yStep;
                }
                y = yNext;
                // sample the waveform
                int yCrushedNext = yCrushed;
                if ((t += increment) >= 1) {
                    t -= 1;
                    // crush the waveform
                    unsigned bitMask = 0x1Fu << (4 - bitDepth);
                    yCrushedNext = std::abs(yNext);
                    yCrushedNext &= bitMask;
                    if (yNext < 0) yCrushedNext = -yCrushedNext;
                }
                HW::display.DrawLine(x, yCrushed+16, x+1, yCrushedNext+16, true);
                yCrushed = yCrushedNext;
            }

            HW::display.Update();
            return true;
        }
    };

    static inline ProgAnimation animation;

public:
    friend class DebugTask;

    /// @brief @ref tasks::Task that prints (via serial output) the bit depth
    /// and sample rate
    class DebugTask : public tasks::Task
    {
    public:
        unsigned intervalMicros() const { return 1'000'000; }

        void init() { }

        void execute()
        {
            if (theProgram) {
                int bit_depth = theProgram->GetBitDepth();
                float crush_rate = theProgram->GetCrushRate();
                auto [flInt, flFrac] = splitFloat(crush_rate, 3);
                daisy2::DebugLog::PrintLine("bits=%d rate=%d.%03u", bit_depth, flInt, flFrac);
            }
        }
    };
};
