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
        PARAM_CVSOURCE(ITEM, ModRateControl, "Mod rate control", Fixed) \
        PARAM_CVSOURCE(ITEM, ModDepthControl, "Mod depth ctrl.", Fixed) \
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
        SetDelayCv(delaySave, 0);
        SetFeedbackAmount(feedbackAmount);

        mix.Init(daisysp::CROSSFADE_CPOW);
        SetMixLevel(effectMixLevel);

        lfoMod.Init(HW::sampleRate);
        lfoMod.SetWaveform(daisysp::Oscillator::WAVE_SIN);
        SetModRateHz(delayModRate);
        SetModDepth(delayModDepth);
    }

    void Process(ProcessArgs& args) override
    {
        // LFO and CV input are processed once per callback, not once per sample
        ReadCv(args);

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
    /// @details This is called once per Process callback, not once per audio sample.
    /// In addition to the input CVs, this also handles the software LFO.
    /// @param args 
    void ReadCv(const ProcessArgs& args)
    {
        // Modulation LFO
        HW::CVIn::GetUnipolarExp(GetModRateControl())
            .and_then([this](float val) { SetModRateCv(val); return emptyOpt; });
        HW::CVIn::GetUnipolar(GetModDepthControl())
            .and_then([this](float val) { SetModDepth(val); return emptyOpt; });
        float modVal = lfoMod.Process();

        // CV inputs
        // Must always call SetDelayCv even if GetUnipolarExp() returns nothing
        // because modVal must always be processed.
        auto cv = HW::CVIn::GetUnipolarExp(GetDelayControl());
        SetDelayCv(cv, modVal);
        HW::CVIn::GetUnipolar(GetFeedbackControl())
            .and_then([this](float val) { SetFeedbackAmount(val); return emptyOpt; });
        HW::CVIn::GetUnipolar(GetMixControl())
            .and_then([this](float val) { SetMixLevel(val); return emptyOpt; });
    }

    float delaySamples = 10000; ///< Delay time in samples

    float delaySave = 0.05;     ///< Last-used delay CV value, used to detect changes

    RunningAverage<float, 48> avgDelay; ///< Delay CV smoother-outer to avoid ugliness

    /// @brief Return the delay time in samples
    /// @return 
    float GetDelaySamples() const { return delaySamples; }

    /// @brief Set the delay time from a unipolar CV value, with modulation
    /// @param delay optional delay CV value
    /// @param modVal delay modulation value
    void SetDelayCv(std::optional<float> delay, float modVal)
    {
        // Update the current delay value, only if given and only if it has
        // changed sufficiently.
        if (delay) {
            static constexpr float minChange = 0.0001;
            delay = avgDelay.update(*delay);
            if (isDifferent(*delay, delaySave, minChange)) {
                delaySave = *delay;
            }
        }
        float delaySecs = delaySave * maxDelaySecs;
        // Always update the delay because modVal is always changing
        delaySecs += modVal;
        SetDelaySecs(delaySecs);
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

    float delayModRate = 5;     ///< Delay time modulation rate
    
    daisysp::Oscillator lfoMod; ///< LFO for delay time modulation

    /// @brief Return the delay time modulation rate in Hertz
    /// @return 
    float GetModRateHz() const { return delayModRate; }

    /// @brief Set the delay time modulation rate from a CV value
    /// @param rate 
    void SetModRateCv(float rate)
    {
        // CV value [0, 1] -> LFO freq [0.1, 10] approx
        SetModRateHz(rescale(rate, 0.f, 1.f, 0.1f, 10.1f));
    }

    /// @brief Set the delay time modulation rate in Hertz
    /// @param rate 
    void SetModRateHz(float rate)
    {
        delayModRate = rate;
        // Adjust the LFO frequency because the LFO is only processed once per
        // callback, not once per sample
        lfoMod.SetFreq(rate * HW::audioBlockSize);
    }

    float delayModDepth = 0.2;  ///< Delay time modulation depth

    /// @brief Return the delay time modulation depth
    /// @return 
    float GetModDepth() const { return delayModDepth; }

    /// @brief Set the delay time modulation depth
    /// @param depth 
    void SetModDepth(float depth)
    {
        delayModDepth = depth;
        // Map the CV to a useful amplitude range
        lfoMod.SetAmp(rescale(depth, 0.f, 1.f, 0.f, 0.002f));
    }

    float feedbackAmount = 0.2; ///< Feedback amount

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

    float effectMixLevel = 0.5; ///< Effect mix level

    daisysp::CrossFade mix;     ///< Effect mixer

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

    HW::Sys::timeus_t tTap = 0; ///< Last tap time

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
                delaySave = delaySecs / maxDelaySecs;
            }
        }
        // Reset the tap timer to now
        tTap = tNow;
    }

private:
    /// @brief Animation for this program shows input and output amplitudes
    static inline AnimAmplitude<3> animation;

protected:
    static inline this_t* theProgram = nullptr; // DEBUG: for DebugTask

public:
    friend class DebugTask;

    /// @brief @ref tasks::Task that prints (via serial output) current
    /// parameter values
    class DebugTask : public tasks::Task
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
                float modRate = theProgram->GetModRateHz();
                auto [modRateInt, modRateFrac] = splitFloat(modRate, 3);
                float modDepth = theProgram->GetModDepth();
                daisy2::DebugLog::PrintLine("delay=%u feedback=%d.%03u mix=%d.%03u mod rate=%d.%03u depth=%d",
                    delay, feedbackInt, feedbackFrac, mixInt, mixFrac,
                    modRateInt, modRateFrac, int(modDepth * 100));
            }
        }
    };
};
