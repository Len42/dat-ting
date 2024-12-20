#pragma once

/// @brief Variable-shape VCO program
class ProgVariableOsc : public Program
{
    using this_t = ProgVariableOsc;

    // Declare the configurable parameters of this program
    #define PROG_PARAMS(ITEM) \
        PARAM_CVSOURCE(ITEM, ShapeControl, "Shape control", Pot) \
        PARAM_CVSOURCE(ITEM, WidthControl, "Width control", Fixed) \
        PARAM_FLOAT(ITEM, ModAmount, "Mod amount", 0)
    DECL_PROG_PARAMS
    #undef PROG_PARAMS

protected:
    /// @brief Oscillator settings
    struct OscParams
    {
        float freq;     ///< Frequency
        float shape;    ///< Wave shape parameter
        float width;    ///< Pulse width parameter
    };

    /// @brief Update the oscillator settings from the current CV inputs
    /// @param args 
    /// @param pparams 
    void UpdateOscParams(const ProcessArgs& args, OscParams* pparams)
    {
        pparams->freq = HW::CVIn::GetFreqWithMod(HW::CVIn::CV1, HW::CVIn::CV2, GetModAmount());
        HW::CVIn::GetUnipolar(GetShapeControl())
            .and_then([pparams](float val) { pparams->shape = val; return emptyOpt; });
        HW::CVIn::GetUnipolar(GetWidthControl())
            .and_then([pparams](float val) { pparams->width = val; return emptyOpt; });
    }

protected:
    /// @brief Base class for oscillator implementation
    /// @details There's a subclass for the actual oscillator and a subclass for
    /// waveform display.
    /// @tparam SUB VarOscBase subclass (CRTP)
    template<typename SUB>
    class VarOscBase
    {
        SUB* subclass() { return static_cast<SUB*>(this); }

    public:
        /// @brief Initialize the oscillator
        void Init()
        {
            auto sampleRate = HW::seed.AudioSampleRate();
            subclass()->InitImpl(sampleRate);
            osc.Init(sampleRate);
            osc.SetSync(false);
        }

        /// @brief Produce output using a @ref daisysp::VariableShapeOscillator
        /// @details Called from @ref Program::Process
        /// @param args 
        /// @param params 
        void Process(ProcessArgs& args, const OscParams& params)
        {
            // Set the oscillator frequency, either from the CV input or
            // constant for waveform display
            float freq = subclass()->GetFreq(params.freq);
            // NOTE: VariableShapeOscillator uses SetSyncFreq() instead of SetFreq()
            osc.SetSyncFreq(freq);
            // Set the shape parameters
            osc.SetWaveshape(params.shape);
            osc.SetPW(params.width);
            // Fill the output with samples from the oscillator
            for (auto&& out : args.outbuf) {
                out.left = out.right = osc.Process();
            }
        }

    protected:
        /// @brief The oscillator
        daisysp::VariableShapeOscillator osc;
    };

    /// @brief Actual oscillator implementation
    class VarOscImpl : public VarOscBase<VarOscImpl>
    {
    protected:
        void InitImpl(float sampleRate) { }

        float GetFreq(float freqParam) { return freqParam; }

        template<typename SUB> friend class VarOscBase;
    };

    /// @brief Implemenmtation of oscillator specialized for waveform display
    class VarOscAnim : public VarOscBase<VarOscAnim>
    {
    public:
        VarOscAnim() { }

    protected:
        void InitImpl(float sampleRate) { freq = 2 * sampleRate / animBufSize; }

        float GetFreq(float freqParam) { return freq; }

        float freq = 100;
    
        template<typename SUB> friend class VarOscBase;
    };

    /// @brief @ref Animation for @ref ProgVariableOsc
    class ProgAnimation : public Animation
    {
    public:
        void Init() override { }

        bool Step(unsigned step) override
        {
            // Set up a phony oscillator to generate a waveform for the display
            oscAnim.Init();
            static daisy2::AudioSample inTemp[animBufSize]; // needed, but just a dummy value
            daisy2::AudioInBuf inbuf(inTemp);
            static daisy2::AudioSample outTemp[animBufSize];
            daisy2::AudioOutBuf outbuf(outTemp);
            ProcessArgs args = MakeProcessArgs(inbuf, outbuf);
            oscAnim.Process(args, oscParams);

            // Display the waveform
            HW::display.Fill(false);
            unsigned yHalf = HW::display.Height() / 2;
            unsigned yPrev = 0;
            for (auto&& [x, sample] : outbuf | std::views::enumerate) {
                float yFl = yHalf - sample.left * yHalf;
                unsigned y = (yFl < 0) ? 0 : unsigned(yFl);
                if (x == 0) {
                    HW::display.DrawPixel(x, y, true);
                } else {
                    HW::display.DrawLine(x-1, yPrev, x, y, true);
                }
                yPrev = y;
            }
            HW::display.Update();

            // never stop
            return true;
        }

        /// @brief Set the oscillator parameters to use for animation
        /// @param oscParamsNew 
        void SetOscParams(const OscParams& oscParamsNew) { oscParams = oscParamsNew; }

    protected:
        OscParams oscParams;

        VarOscAnim oscAnim;
    };

public:
    constexpr std::string_view GetName() const override { return "Variable Osc"; }

    void Init() override
    {
        oscImpl.Init();
        oscParams = { .freq=440, .shape=0.2, .width=0.5 };
    }

    void Process(ProcessArgs& args) override
    {
        UpdateOscParams(args, &oscParams);
        oscImpl.Process(args, oscParams);
        animation.SetOscParams(oscParams);
    }

    Animation* GetAnimation() const override { return &animation; }

protected:
    VarOscImpl oscImpl;

    OscParams oscParams = { .freq=220, .shape=0.2, .width=0.5 };

    static constexpr unsigned animBufSize = 128;

    static inline ProgAnimation animation;
};
