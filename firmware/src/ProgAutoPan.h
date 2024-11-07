#pragma once

class ProgAutoPan : public Program
{
public:
    constexpr std::string_view GetName() const override { return "Auto Pan"sv; }

    void Init()
    {
        lfo.Init(HW::seed.AudioSampleRate());
        lfo.SetWaveform(daisysp::Oscillator::WAVE_SIN);
        lfo.SetFreq(0.5);
        lfo.SetAmp(1);
    }

    void Process(ProcessArgs& args)
    {
        float cv = HW::CVIn::GetUnipolar(HW::CVIn::Pot).value_or(0.25f);
        float lfoFreq = 0.025f + 4.f * cv;
        lfo.SetFreq(lfoFreq);
        float pan = 0;
        for (auto&& [in, out] : std::views::zip(args.inbuf, args.outbuf)) {
            float inVal = in.left; // there's only 1 input channel
            pan = lfo.Process() / 2;
            out.left = inVal * (0.5 + pan);
            out.right = inVal * (0.5 - pan);
        }
        animation.SetPanPos(pan);
    }

    Animation* GetAnimation() const override { return &animation; }

protected:
    daisysp::Oscillator lfo;

    class ProgAnimation : public Animation
    {
    public:
        ProgAnimation() { }

        void Init() override { }

        bool Step(unsigned step) override
        {
            // Display the current panning position
            static constexpr unsigned radiusMax = 12;
            static constexpr unsigned xMargin = radiusMax;
            unsigned width = HW::display.Width() - 2 * xMargin;
            // panPos range is [-0.5, 0.5]
            //unsigned x = xMargin + width / 2 + width * panPos;
            unsigned x = HW::display.Width() - (xMargin + width / 2 + width * panPos);
            //unsigned x =  width - xMargin - width * panPos;
            unsigned radius = 2 * std::abs(panPos) * radiusMax + 0.5;
            HW::display.Fill(false);
            HW::display.DrawCircle(x, HW::display.Height() / 2, radius, true);
            HW::display.Update();

            // never stop
            return true;
        }

        void SetPanPos(float pos) { panPos = pos; }

    protected:
        float panPos = 0;
    };

    static inline ProgAnimation animation;
};
