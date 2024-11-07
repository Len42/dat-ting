#pragma once

// BUG: ReverbSc variable should be a static data member in ProgReverb but then
// it cannot be stored in SDRAM (DSY_SDRAM_BSS does nothing in that case).
daisysp::ReverbSc DSY_SDRAM_BSS reverbSc1;

class ProgReverb : public Program
{
    using this_t = ProgReverb;

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

    float GetFeedbackAmount() const { return feedbackAmount; }

    // NOTE: No actual return value, but must return a std::optional because the
    // caller requires it
    void SetFeedbackAmount(float amount)
    {
        // Map the CV to a range that goes a bit over 1.0
        // TODO: Calibrate this
        feedbackAmount = rescale(amount, 0.0f, 0.95f, 0.0f, 1.1f);
        reverbSc1.SetFeedback(feedbackAmount);
    }

    float GetFilterCutoff() const { return filterCutoff / (sampleRate / 2); }
    
    void SetFilterCutoff(float cutoff)
    {
        filterCutoff = cutoff * (sampleRate / 2);
        reverbSc1.SetLpFreq(filterCutoff);
    }

    float GetMixLevel() const { return effectMixLevel; }

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
                // DEBUG
            }
        }
    };
};
