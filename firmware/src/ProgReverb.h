#pragma once

/// @brief Stereo reverb program
/// BUG: Should be a static data member in ProgReverb but then it cannot be
/// stored in SDRAM (DSY_SDRAM_BSS does nothing in that case).
static daisysp::ReverbSc DSY_SDRAM_BSS reverbSc1;

/// @brief Stereo reverb program
class ProgReverb : public Program
{
    using this_t = ProgReverb;

    // Declare the configurable parameters of this program
    #define PROG_PARAMS(ITEM) \
        PARAM_CVSOURCE(ITEM, FeedbackControl, "Feedback control", Fixed) \
        PARAM_CVSOURCE(ITEM, FilterControl, "Filter control", Fixed) \
        PARAM_CVSOURCE(ITEM, MixControl, "Mix control", Pot)
    DECL_PROG_PARAMS
    #undef PROG_PARAMS

public:
    constexpr std::string_view GetName() const override { return "Reverb"sv; }

    void Init() override
    {
        theProgram = this; // DEBUG

        sampleRate = HW::seed.AudioSampleRate();
        reverbSc1.Init(sampleRate);
        mix.Init(daisysp::CROSSFADE_CPOW);
        SetMixLevel(effectMixLevel);
    }

    void Process(ProcessArgs& args) override
    {
        HW::CVIn::GetUnipolar(GetFeedbackControl())
            .and_then([this](float val) { SetFeedbackAmount(val); return emptyOpt; });
        HW::CVIn::GetUnipolar(GetFilterControl())
            .and_then([this](float val) { SetFilterCutoff(val); return emptyOpt; });
        HW::CVIn::GetUnipolar(GetMixControl())
            .and_then([this](float val) { SetMixLevel(val); return emptyOpt; });

        float outL, outR;
        for (auto&& [in, out] : std::views::zip(args.inbuf, args.outbuf)) {
            float input = in.left;
            reverbSc1.Process(input, input, &outL, &outR);
            out.left = mix.Process(input, outL);
            out.right = mix.Process(input, outR);
        }

        // Update the animation display with the last-calculated result
        auto animIn = args.inbuf.back();
        auto animOut = args.outbuf.back();
        animation.SetAmplitude(animOut.left, animIn.left, animOut.right);
    }

    Animation* GetAnimation() const override { return &animation; }

protected:
    /// @brief Return the feedback amount
    /// @return float in [0, 1]
    float GetFeedbackAmount() const { return feedbackAmount; }

    /// @brief Set the feedback amount
    /// @details Amount of 1.0 will give more than unity feedback.
    /// @param amount float in [0, 1]
    void SetFeedbackAmount(float amount)
    {
        // Map the CV to a range that goes a bit over 1.0
        feedbackAmount = rescale(amount, 0.0f, 0.95f, 0.0f, 1.1f);
        reverbSc1.SetFeedback(feedbackAmount);
    }

    /// @brief Get the filter cutoff frequency
    /// @return float in [0, 1]
    float GetFilterCutoff() const { return filterCutoff / (sampleRate / 2); }
    
    /// @brief Set the filter cutoff frequency
    /// @param cutoff float in [0, 1]
    void SetFilterCutoff(float cutoff)
    {
        filterCutoff = cutoff * (sampleRate / 2);
        reverbSc1.SetLpFreq(filterCutoff);
    }

    /// @brief Get the effect mix level
    /// @return float in [0, 1]
    float GetMixLevel() const { return effectMixLevel; }

    /// @brief Set the effect mix level
    /// @param mixLevel float in [0, 1]
    void SetMixLevel(float mixLevel)
    {
        // KLUDGE: Map mixLevel so there's a dead zone at each end, otherwise we
        // can't get fully-dry and fully-wet with imperfect pot, ADC, etc.
        mixLevel = rescale(mixLevel, 0.05f, 0.95f, 0.0f, 1.0f);
        effectMixLevel = mixLevel;
        mix.SetPos(mixLevel);
    }

private:
    float sampleRate = 0;

    float feedbackAmount = 0.9;

    float filterCutoff = 0.5;

    float effectMixLevel = 0.5;

    daisysp::CrossFade mix;

    /// @brief Animation for this program shows input and output amplitudes
    static inline AnimAmplitude<3> animation;

protected:
    static inline this_t* theProgram = nullptr; // DEBUG: for DebugTask

public:
    friend class DebugTask;

    /// @brief @ref tasks::Task that prints some info (via serial output)
    class DebugTask : public tasks::Task
    {
    public:
        unsigned intervalMicros() const { return 1'000'000; }

        void init() { }

        void execute()
        {
            if (theProgram) {
                // TODO
                //daisy2::DebugLog::PrintLine("");
            }
        }
    };
};
