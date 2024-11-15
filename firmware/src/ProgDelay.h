#pragma once

/// @brief Maximum delay time in seconds
static constexpr float maxDelaySecs = 10;

/// @brief Maximum delay time in samples
static constexpr size_t maxDelaySamples = size_t(maxDelaySecs * HW::sampleRate);

/// @brief Specialized delay-line type
using DelayLine = daisysp::DelayLine<float, maxDelaySamples>;

// BUG: DelayLine variables should be static data members in ProgDelay but then
// it cannot be stored in SDRAM (DSY_SDRAM_BSS does nothing in that case).

/// @brief Delay line for regular delay
static DelayLine DSY_SDRAM_BSS delayLine1;

/// @brief Delay line for ping-pong delay
static DelayLine DSY_SDRAM_BSS delayLine2;

/// @brief Delay/echo @ref Program
/// @details Implements normal and ping-pong delay. Various parameters can be set
/// by the potentiometer or CV inputs. Pushbutton is used for tap tempo.
class ProgDelay : public Program
{
    // DEBUG: TODO: Some of these need to be implemented permanently
    static constexpr bool fDebReadTimer = false;
    static constexpr unsigned debReadTimerFreq = 1000;
    static constexpr bool fDebAverage = true;
    static constexpr unsigned debAverageSamples = 48;
    static constexpr bool fDebMinChange = true;
    static constexpr float debMinChange = 0.0001;

    using this_t = ProgDelay;

    // Declare the configurable parameters of this program
    #define PARAM_VALUES(ITEM) \
        ITEM(Normal, "Normal") \
        ITEM(PingPong, "Ping-pong")
    DECL_PARAM_VALUES(Mode)
    #undef PARAM_VALUES
    #define PARAM_VALUES(ITEM) \
        ITEM(Div31, "3:1") \
        ITEM(Div21, "2:1") \
        ITEM(Div32, "3:2") \
        ITEM(Div11, "1:1") \
        ITEM(Div23, "2:3")
    DECL_PARAM_VALUES(TapDiv)
    #undef PARAM_VALUES
    #define PROG_PARAMS(ITEM) \
        PARAM_NUM(ITEM, Mode, "Delay mode", unsigned(Mode::Normal)) \
        PARAM_CVSOURCE(ITEM, DelayControl, "Delay control", Pot) \
        PARAM_CVSOURCE(ITEM, FeedbackControl, "Feedback control", Fixed) \
        PARAM_CVSOURCE(ITEM, MixControl, "Mix control", Fixed) \
        PARAM_NUM(ITEM, TapDiv, "Tap division", unsigned(TapDiv::Div11))
    DECL_PROG_PARAMS
    #undef PROG_PARAMS

public:
    constexpr std::string_view GetName() const override { return "Delay"sv; }

    void Init() override
    {
        theProgram = this; // DEBUG

        delayLine1.Init();
        delayLine2.Init();
        SetDelaySamples(delaySamples);
        mix.Init(daisysp::CROSSFADE_CPOW);
        SetMixLevel(effectMixLevel);
        timerReadCv.Init(freqReadCv, HW::sampleRate);
        // Must call ReadCv to initialize things because Process() only calls it occasionally
        // TODO: Is that right?
        ProcessArgs args = Program::MakeProcessArgs({},{});
        ReadCv(args);
    }

    void Process(ProcessArgs& args) override
    {
        // Only call ReadCv() occasionally to avoid noise artifacts
        if (!fDebReadTimer || timerReadCv.Process()) {
            ReadCv(args);
        }

        // Tap tempo setting
        bool tapped = false;
        if (args.GateOn(HW::CVIn::Button)) {
            HandleTap();
            tapped = true;
        }

        // Delay processing
        for (auto&& [in, out] : std::views::zip(args.inbuf, args.outbuf)) {
            float input = in.left;
            float delayed = delayLine1.Read();
            float feedback = delayed * feedbackAmount;
            out.left = mix.Process(input, delayed);
            if (GetMode() == unsigned(Mode::PingPong)) {
                // Ping-pong stereo delay: Two delay lines, one for each channel
                delayLine2.Write(feedback);
                delayed = delayLine2.Read();
                feedback = delayed * feedbackAmount;
                out.right = mix.Process(input, delayed);
            } else {
                // Normal delay: Single delay line output on both channels
                out.right = out.left;
            }
            delayLine1.Write(feedback + input);
        }

        // Update the animation display with the last-calculated result
        auto animIn = args.inbuf.back();
        auto animOut = args.outbuf.back();
        animation.SetAmplitude(animOut.left,
                               tapped ? 0.25f : animIn.left,
                               animOut.right);
    }

    Animation* GetAnimation() const override { return &animation; }

protected:
    /// @brief Update various CV-controlled parameters according to settings
    /// @param args 
    void ReadCv(const ProcessArgs& args)
    {
        std::optional<unsigned> cv;
        HW::CVIn::GetUnipolarExp(GetDelayControl())
            .and_then([this](float val) { SetDelayCv(val); return emptyOpt; });
        HW::CVIn::GetUnipolar(GetFeedbackControl())
            .and_then([this](float val) { SetFeedbackAmount(val); return emptyOpt; });
        HW::CVIn::GetUnipolar(GetMixControl())
            .and_then([this](float val) { SetMixLevel(val); return emptyOpt; });
    }

    /// @brief Return the delay time in samples
    /// @return 
    float GetDelaySamples() const { return delaySamples; }

    /// @brief Set the delay time from a unipolar CV value
    /// @param delay 
    void SetDelayCv(float delay)
    {
        static constexpr float minChange = debMinChange; // DEBUG
        if constexpr (fDebAverage) // DEBUG
            delay = avgDelay.update(delay);
        if (!fDebMinChange || isDifferent(delay, delaySave, minChange)) {
            delaySave = delay;
            float delaySecs = delay * maxDelaySecs;
            SetDelaySecs(delaySecs);
        }
    }

    /// @brief Set the delay time in seconds
    /// @param delaySecs 
    void SetDelaySecs(float delaySecs) { SetDelaySamples(delaySecs * HW::sampleRate); }

    /// @brief Set the delay time in samples
    /// @param samples 
    void SetDelaySamples(float samples)
    {
        delaySamples = samples;
        delayLine1.SetDelay(samples);
        delayLine2.SetDelay(samples);
    }

    /// @brief Return the feedback amount
    /// @return 
    float GetFeedbackAmount() const { return feedbackAmount; }

    /// @brief Set the feedback amount from a unipolar CV value
    /// @details Amount of 1.0 will give more than unity feedback.
    /// @param amount float value in [0, 1]
    void SetFeedbackAmount(float amount)
    {
        // Map the CV to a range that goes a bit over 1.0
        feedbackAmount = rescale(amount, 0.0f, 0.95f, 0.0f, 1.1f);
    }

    /// @brief Return the effect mix level
    /// @return 
    float GetMixLevel() const { return effectMixLevel; }

    /// @brief Set the effect mix level from a unipolar CV value
    /// @param mixLevel 
    void SetMixLevel(float mixLevel)
    {
        // KLUDGE: Map mixLevel so there's a dead zone at each end, otherwise we
        // can't get fully-dry and fully-wet with imperfect pot, ADC, etc.
        mixLevel = rescale(mixLevel, 0.05f, 0.95f, 0.0f, 1.0f);
        effectMixLevel = mixLevel;
        mix.SetPos(mixLevel);
    }

    /// @brief Handle tap tempo using the pushbutton
    void HandleTap()
    {
        // Called when tap button is pressed
        HW::Sys::timeus_t tNow = HW::Sys::GetUsLong();
        if (tTap == 0) {
            // No previous tap - nothing to do
        } else {
            // Second tap - Set the delay time. Make it fixed (not CV-controlled)
            // and adjust for the tap-division setting.
            float delaySecs = (tNow - tTap) / 1e6f;
            switch(TapDiv(GetTapDiv())) {
                using enum TapDiv;
                case Div31: delaySecs /= 3.f;       break;
                case Div21: delaySecs /= 2.f;       break;
                case Div32: delaySecs *= 2.f/3.f;   break;
                case Div23: delaySecs *= 3.f/2.f;   break;
                case Div11: break;
                default:    break;
            }
            if (delaySecs <= maxDelaySecs) {
                SetDelayControl(HW::CVIn::Fixed);
                SetDelaySecs(delaySecs);
            }
        }
        // Reset the tap timer to now
        tTap = tNow;
    }

private:
    float delaySamples = 10000;

    float feedbackAmount = 0.5;

    float effectMixLevel = 0.5;

    daisysp::CrossFade mix;

    static constexpr float freqReadCv = debReadTimerFreq; // DEBUG 1000;

    daisysp::Metro timerReadCv;

    float delaySave = 99;

    RunningAverage<float, debAverageSamples> avgDelay;

    HW::Sys::timeus_t tTap = 0;

    /// @brief Animation for this program shows input and output amplitudes
    static inline AnimAmplitude<3> animation;

protected:
    static inline this_t* theProgram = nullptr; // DEBUG: for DebugTask

public:
    friend class DebugTask;

    /// @brief @ref tasks::Task that prints (via serial output) current
    /// parameter values
    class DebugTask : public tasks::Task<DebugTask>
    {
    public:
        unsigned intervalMicros() const { return 1'000'000; }

        void init() { }

        void execute()
        {
            if (theProgram) {
                unsigned delay = unsigned(theProgram->GetDelaySamples());
                float feedback = theProgram->GetFeedbackAmount();
                auto [feedbackInt, feedbackFrac] = splitFloat(feedback, 3);
                float mix = theProgram->GetMixLevel();
                auto [mixInt, mixFrac] = splitFloat(mix, 3);
                daisy2::DebugLog::PrintLine("delay=%u feedback=%d.%03u mix=%d.%03u",
                    delay, feedbackInt, feedbackFrac, mixInt, mixFrac);
            }
        }
    };
};
