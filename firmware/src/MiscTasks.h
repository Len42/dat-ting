#pragma once

#include "tasks.h"

/// @brief @ref tasks::Task that blinks the on-board LED
class BlinkTask : public tasks::Task
{
public:
    unsigned intervalMicros() const { return 500'000; }

    void init() { }

    void execute() { HW::seed.SetLed(on = !on); }

private:
    bool on = false;
};

/// @brief @ref tasks::Task that shows the state of the pushbutton using the
/// on-board LED
class ButtonLedTask : public tasks::Task
{
public:
    unsigned intervalMicros() const { return 50'000; }

    void init() { }

    void execute() { HW::seed.SetLed(HW::button.IsOn()); }
};

/// @brief @ref tasks::Task that shows the state of the CV1 gate input using the
/// on-board LED and serial output
class GateLedTask : public tasks::Task
{
public:
    unsigned intervalMicros() const { return 2'000; }

    void init() { }

    void execute() {
        bool fGateOn = HW::CVIn::IsGateOn(HW::CVIn::CV1);
        HW::seed.SetLed(fGateOn);
        if (HW::CVIn::GateTurnedOn(HW::CVIn::CV1))
            daisy2::DebugLog::PrintLine("gate ON = %s", fGateOn ? "ON" : "off");
        if (HW::CVIn::GateTurnedOff(HW::CVIn::CV1))
            daisy2::DebugLog::PrintLine("gate off = %s", fGateOn ? "ON" : "off");
    }
};

/// @brief @ref tasks::Task that prints (via serial output) ADC values from the
/// CV inputs
class AdcOutputTask : public tasks::Task
{
public:
    unsigned intervalMicros() const { return 500'000; }

    void init() { }

    void execute()
    {
        cv1.update(HW::CVIn::CV1);
        //cv2.update(HW::CVIn::CV2);
        //cvPot.update(HW::CVIn::Pot);
    }

private:
    struct CV {
        unsigned cv = 0;
        const char* name;

        CV(const char* name_) : name(name_) { }

        void update(HW::CVIn::ADC input) {
            unsigned cvNew = HW::CVIn::GetRaw(input);
            float cvUniFloat = HW::CVIn::GetUnipolar(input).value_or(9999);
            auto [cvUniInt, cvUniFrac] = splitFloat(cvUniFloat, 3);
            float cvExpFloat = HW::CVIn::GetUnipolarExp(input).value_or(9999);
            auto [cvExpInt, cvExpFrac] = splitFloat(cvExpFloat, 3);
            float cvBiFloat = HW::CVIn::GetBipolar(input).value_or(9999);
            auto [cvBiInt, cvBiFrac] = splitFloat(cvBiFloat, 3);
            float note = HW::CVIn::GetNote(input);
            auto [noteInt, noteFrac] = splitFloat(note, 3);
            float freq = HW::CVIn::GetFrequency(input);
            auto [freqInt, freqFrac] = splitFloat(freq, 3);
            int diff = int(cvNew) - int(cv);
            daisy2::DebugLog::PrintLine(
                "%s: cv = %u, uni = %d.%03u, exp = %d.%03u, bi = %d.%03u, note = %d.%03u, freq = %d.%03u Hz, diff = %d",
                name, cvNew, cvUniInt, cvUniFrac, cvExpInt, cvExpFrac, cvBiInt, cvBiFrac,
                noteInt, noteFrac, freqInt, freqFrac, diff);
            //daisy2::DebugLog::PrintLine("%d, %d", int(cvUniFloat * 1000), int(cvBiFloat * 1000));
            cv = cvNew;
        }
    };

    CV cv1{"1"};
    CV cv2{"2"};
    CV cvPot{"Pot"};
};

/// @brief @ref tasks::Task that prints (via serial output) ADC values to help
/// with CV input calibration
class AdcCalibrateTask : public tasks::Task
{
public:
    unsigned intervalMicros() const { return 1'500'000; }

    void init() { }

    void execute()
    {
        // Get an average value
        if (std::abs(int(HW::CVIn::GetRaw(HW::CVIn::CV1)) - int(adcPrev)) >= 500) {
            static constexpr int iters = 1000;
            unsigned adcTotal = 0;
            for (int i = 0; i < iters; ++i) {
                HW::Sys::Delay(1);
                adcTotal += HW::CVIn::GetRaw(HW::CVIn::CV1);
            }
            unsigned adcAvg = int(float(adcTotal) / float(iters) + 0.5);
            HW::seed.PrintLine("%u", adcAvg);
            adcPrev = adcAvg;
        }
    }
protected:
    unsigned adcPrev = 0;
};

/// @brief @ref tasks::Task that prints (via serial output) the audio sample rate
class SampleRateTask : public tasks::Task
{
public:
    unsigned intervalMicros() const { return 1'000'000; }

    void init() { }

    void execute()
    {
        HW::Sys::timeus_t tStop = HW::Sys::GetUsLong();
        unsigned count = programs.GetResetSampleCount();
        unsigned dt = tStop - tStart;
        tStart = tStop;
        float dtSecs = float(dt) / 1e6f;
        unsigned sps = unsigned(std::round(float(count) / dtSecs));
        daisy2::DebugLog::PrintLine("dt=%uus, count=%u, sps=%u", dt, count, sps);
    }

protected:
    HW::Sys::timeus_t tStart = 0;
};
