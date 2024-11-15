#pragma once

/// @brief Handle analog control voltage outputs
/// @details There are two DAC output channels.
/// The full output voltage range is 0V to +10V. Bipolar CV output is not
/// supported.
template<daisy2::DaisySeed2& seed>
class CVOutBase
{
public:
    using Channel = daisy::DacHandle::Channel;

    /// @brief Maximum output value for @ref SetRaw
    static constexpr unsigned maxValue = (1 << 12) - 1;

    /// @brief Initialize the CV outputs
    static void Init()
    {
        daisy::DacHandle::Config dacConfig {
            .chn = daisy::DacHandle::Channel::BOTH,
            .mode = daisy::DacHandle::Mode::POLLING,
            .bitdepth = daisy::DacHandle::BitDepth::BITS_12,
            .buff_state = daisy::DacHandle::BufferState::DISABLED
        };
        seed.dac.Init(dacConfig);
    }

    /// @brief Set a CV output to a 12-bit value
    /// @param channel DAC output channel
    /// @param value 12-bit unsigned value corresponding to a voltage range of
    /// [0, +10] approximately
    static void SetRaw(Channel channel, unsigned value)
    {
        seed.dac.WriteValue(channel, value);
    }

    /// @brief Set a CV output to a unipolar floating-point value
    /// @param channel DAC output channel
    /// @param value float in [0, +1] corresponding to voltage range [0, +8] 
    static void SetUnipolar(Channel channel, float value)
    {
        value *= 8.f/10.f;
        unsigned cv = std::round(value * cv10V);
        SetRaw(channel, cv);
    }

    /// @brief Set a pitch CV output to a MIDI note value
    /// @param channel DAC output channel
    /// @param note MIDI note number (may be fractional)
    static void SetNote(Channel channel, float note)
    {
        int cv = std::round((note - float(minNote)) * cv10V / float(numNotes));
        SetRaw(channel, unsigned(std::clamp(cv, 0, int(maxValue))));
    }

protected:
    // Prototype and Module are approx. the same
    static constexpr unsigned minNote = 12; // C0
    static constexpr unsigned numNotes = 10 * 12; // 10 octave range from 0V to 10V
    static constexpr float cv10V = 4162.43; // nominal output value to give +10V
};
