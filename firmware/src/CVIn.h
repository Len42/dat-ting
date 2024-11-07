#pragma once

/// @brief Handle analog CV inputs
template<daisy2::DaisySeed2& seed, HWType hwType>
class CVInBase
{
protected:
    class Gate;
    using Pins = PinDefs<hwType>;
    static constexpr bool isPrototype = (hwType == HWType::Prototype);

public:
    /// @brief Identifiers of the CV inputs
    enum ADC : uint8_t { CV1, CV2, Pot, _inCount };
    static constexpr unsigned Fixed = ADC::_inCount; // for PARAM_CVSOURCE
    static constexpr unsigned Button = 2; // for PARAM_GATESOURCE

    /// @brief Initialize all the ADC inputs for the CVs
    static void Init()
    {
        // ADC conversion speed is set to allow audio-rate modulation (to a
        // point) with decent accuracy
        static constexpr daisy::AdcChannelConfig::ConversionSpeed speed
            = daisy::AdcChannelConfig::SPEED_64CYCLES_5;
        daisy::AdcChannelConfig adcConfigs[std::size(inputs)];
        for (auto&& [input, config] : std::views::zip(inputs, adcConfigs)) {
            config.InitSingle(input.pin, speed);
        }
        seed.adc.Init(adcConfigs, std::size(adcConfigs));
        seed.adc.Start();
        InitGates();
    }

// CV Input
public:
    static uint16_t GetRaw(ADC input)
    {
        if constexpr (isPrototype) {
            if (input == ADC::CV2) {
                // The prototype hardware has no CV2 input so return a fixed value.
                return Pins::ADCGateMin - 1;
            }
        }
        return seed.adc.Get(input);
    }

    static std::optional<float> GetBipolar(unsigned input)
    {
        return GetRawOpt(input)
            .and_then([input](uint16_t cv) -> std::optional<float>
                { return ConvertCvBipolar(cv, input); });
    }

    static std::optional<float> GetUnipolar(unsigned input)
    {
        return GetRawOpt(input)
            .and_then([input](uint16_t cv) -> std::optional<float>
                 { return ConvertCvUnipolar(cv, input); });
    }

    static std::optional<float> GetUnipolarExp(unsigned input)
    {
        return GetRawOpt(input)
            .and_then([input](uint16_t cv) -> std::optional<float>
                 { return ConvertCvUniExp(cv, input); });
    }

    static float GetFrequency(ADC input)
    {
        return ConvertFreqCvValue(GetRaw(input));
    }

    static float GetNote(ADC input)
    {
        return NoteFromCV(GetRaw(input));
    }

protected:
    /// @brief A single CV input: its GPIO pin and last-read value
    struct Input
    {
        daisy::Pin pin;
        Gate gate;
    };

    /// @brief The list of CV inputs, specifying each one's input pin and gate status
    /// @note CV1 is duplicated at the end so that it gets read twice in a row,
    /// because the ADC is free-running and each reading interferes with the next.
    /// CV1 is often used for 1V/oct pitch input so it needs to be as accurate as possible.
    static constinit inline Input inputs[] = {
        /* CV1 */ { .pin = Pins::CVIn1 },
        /* CV2 */ { .pin = Pins::CVIn2 },
        /* Pot */ { .pin = Pins::PotIn },
        /* CV1 */ { .pin = Pins::CVIn1 } // duplicate, ignored
    };

    static std::optional<uint16_t> GetRawOpt(unsigned input)
    {
        // Check the input parameter because it may be out of range (for Program
        // params that are set to Fixed)
        if (input >= ADC::_inCount) {
            return std::nullopt;
        } else {
            return GetRaw(static_cast<ADC>(input));
        }
    }

    static constexpr float ConvertCvBipolar(uint16_t cv, unsigned input)
    {
        float val = (input == ADC::Pot) ? ConvertBipolarPotValue(cv) : ConvertBipolarCvValue(cv);
        return std::clamp(val, -1.f, +1.f);
    }

    static constexpr float ConvertCvUnipolar(uint16_t cv, unsigned input)
    {
        float val = (input == ADC::Pot) ? ConvertUnipolarPotValue(cv) : ConvertUnipolarCvValue(cv);
        return std::clamp(val, 0.f, +1.f);
    }

    static constexpr float ConvertCvUniExp(uint16_t cv, unsigned input)
    {
        float val = (input == ADC::Pot) ? ConvertUniExpPotValue(cv) : ConvertUniExpCvValue(cv);
        return std::clamp(val, 0.f, +1.f);
    }

    static constexpr unsigned numCvBits = 16;

    // TODO: Values for HWType::Prototype not tested
    static constexpr uint16_t adcCvZero = isPrototype ? 93 : 31620;
    static constexpr uint16_t adcCvBiHi = isPrototype ? 31736 : 44890;
    static constexpr uint16_t adcCvUniHi = isPrototype ? 50777 : 52850;

    template<uint16_t inputLo, uint16_t inputHi, float outputLo, float outputHi>
    static constexpr float ConvertAdcValue(uint16_t adcValue)
    {
        // NOTE: adcValue may be outside the range of [inputLo, inputHi]
        // so take care with the artihmetic
        return outputLo
                + (outputHi - outputLo) * (float(adcValue) - float(inputLo))
                    / (float(inputHi) - float(inputLo));
    }

    static constexpr float ConvertBipolarCvValue(uint16_t adcValue)
    {
        // CV [-5, +5] volts -> [-1, +1]
        return ConvertAdcValue<adcCvZero, adcCvBiHi, 0.f, +1.f>(adcValue);
    }

    static constexpr float ConvertUnipolarCvValue(uint16_t adcValue)
    {
        // CV [0, +8] volts -> [0, +1]
        return ConvertAdcValue<adcCvZero, adcCvUniHi, 0.f, +1.f>(adcValue);
    }

    static constexpr float ConvertUniExpCvValue(uint16_t adcValue)
    {
        // CV [0, +8] volts -> [0, +1] with exponential response
        return CvExpTable::lookupInterpolate(adcValue);
    }

    static constexpr uint16_t adcPotLo = 10;
    static constexpr uint16_t adcPotHi = 63475;

    static constexpr float ConvertBipolarPotValue(uint16_t adcValue)
    {
        // Pot [0, +3.3] volts -> [-1, +1]
        return ConvertAdcValue<adcPotLo, adcPotHi, -1.f, +1.f>(adcValue);
    }

    static constexpr float ConvertUnipolarPotValue(uint16_t adcValue)
    {
        // Pot [0, +3.3] volts -> [0, +1]
        return ConvertAdcValue<adcPotLo, adcPotHi, 0.f, +1.f>(adcValue);
    }

    static constexpr float ConvertUniExpPotValue(uint16_t adcValue)
    {
        // Pot [0, +3.3] volts -> [0, +1] with exponential response
        return PotExpTable::lookupInterpolate(adcValue);
    }

    /// @brief Convert CV ADC reading to an ocillator frequency with 1 volt per octave scaling
    /// @param cv 
    /// @return Oscillator frequency in Hz
    static constexpr float ConvertFreqCvValue(uint16_t cv)
    {
        return CVFreqTable::lookupInterpolate(cv);
    }

    /// @brief Convert CV ADC reading to a MIDI note with 1 volt per octave scaling
    /// @note See below for calibration constants
    /// @param cv 
    /// @return MIDI note number (may be a fractional value)
    static constexpr float NoteFromCV(unsigned cv)
    {
        return minNote
            + numNotes * (float(int(cv) - int(adcCvFreqLo)) / float(adcCvFreqHi - adcCvFreqLo));
    }

    // Lookup table for ConvertFreqCvValue()
    static constexpr unsigned minNote = 12; // C0
    static constexpr unsigned numNotes = isPrototype ? (10 * 12) : (12 * 12);
    static constexpr unsigned adcCvFreqHi = isPrototype ? 63471 : 63460;
    static constexpr unsigned adcCvFreqLo = isPrototype ? 93 : 31620;

    /// @brief The frequency mapping table has 8192 entries (13-bit index)
    static constexpr unsigned numFreqTableBits = 13;

    // NOTE: This table should really be in SDRAM but there's currently no way
    // to get static initialized data into SDRAM.
    using CVFreqTable = LookupTable<float, numCvBits, numFreqTableBits,
        [](size_t index, size_t numValues) {
            unsigned cv = (index << (numCvBits - numFreqTableBits));
            float note = NoteFromCV(cv);
            return powf(2, (note - 69.0f) / 12.0f) * 440.0f; // daisysp::mtof(note);
        }>;

    /// @brief Map the interval [0,1] to itself with an exponential function
    /// @details This gives an exponential response for a CV or potentiometer
    /// controlling a parameter which works better exponentially, e.g. time.
    /// It maps 0.5 to 0.1 approximately.
    /// @param in float in [0,1]
    /// @return float in [0,1]
    static constexpr float ExpResponse(float in)
    {
        constexpr float factor = 0.0129f;
        constexpr float expFactor = 6.3f;
        return factor * (exp2(in * expFactor) - 1);
    }

    /// @brief The exponential-response mapping tables have 128 entries (7-bit index)
    static constexpr unsigned numExpMapBits = 7;

    using PotExpTable = LookupTable<float, numCvBits, numExpMapBits,
        [](size_t index, size_t numValues) {
            constexpr auto step = (1 << (numCvBits-numExpMapBits));
            auto n = index * step;
            auto fl = ConvertUnipolarPotValue(n);
            fl = ExpResponse(fl);
            return fl;
        }>;

    using CvExpTable = LookupTable<float, numCvBits, numExpMapBits,
        [](size_t index, size_t numValues) {
            constexpr auto step = (1 << (numCvBits-numExpMapBits));
            auto n = index * step;
            auto fl = ConvertUnipolarCvValue(n);
            fl = ExpResponse(fl);
            return fl;
        }>;

// Gate Input
public:
    /// @brief Update the on/off state of all the gates
    /// @remarks This must be called frequently (typically from the AudioCallback)
    /// TODO: Use analog watchdog interrupts instead of polling
    static void UpdateGates()
    {
        for (auto&& input : inputs) {
            input.gate.Update();
        }
    }

    /// @brief Return true if the gate is currently on (high) and false if it is off (low)
    /// @return 
    static bool IsGateOn(ADC cvIn) { return inputs[cvIn].gate.GetState(); }

    /// @brief Return true if the gate has changed state since the last time
    /// this was called
    /// @return 
    static bool GateOn(ADC cvIn) { return inputs[cvIn].gate.TurnedOn(); }

    static bool GateOff(ADC cvIn) { return inputs[cvIn].gate.TurnedOff(); }

protected:
    static void InitGates()
    {
        for (auto&& [i, input] : inputs | std::views::enumerate) {
            input.gate.Init(ADC(i));
        }
    }

    class Gate
    {
    public:
        void Init(ADC in) { input = in; Update(); TurnedOn(); TurnedOff(); }

        void Update()
        {
            bool isHigh = (GetRaw(input) >= Pins::ADCGateMin);
            if (isHigh != wasHigh) {
                wasHigh = isHigh;
                auto [fHigh, fChanged] = debouncer.Debounce(isHigh ? +1 : -1);
                if (fChanged) {
                    if (isHigh) turnedOn = true;
                    else turnedOff = true;
                }
            }
        }

        bool GetState() { return debouncer.GetValue(); }

        bool TurnedOn() { return turnedOn.exchange(false); }

        bool TurnedOff() { return turnedOff.exchange(false); }

    protected:
        ADC input = ADC(0);
        daisy2::Debouncer debouncer;
        bool wasHigh = false;
        std::atomic<bool> turnedOn = false;
        std::atomic<bool> turnedOff = false;
    };
};
