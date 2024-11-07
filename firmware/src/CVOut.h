#pragma once

/// @brief Handle analog CV outputs
template<daisy2::DaisySeed2& seed>
class CVOutBase
{
public:
    using Channel = daisy::DacHandle::Channel;

    static constexpr unsigned maxValue = (1 << 12) - 1;

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

    static void SetRaw(Channel channel, unsigned value)
    {
        seed.dac.WriteValue(channel, value);
    }

    static void SetUnipolar(Channel channel, float val)
    {
        // Unipolar CV output range is nominally [0, +8] volts
        val *= 8.f/10.f;
        unsigned cv = std::round(val * cv10V);
        SetRaw(channel, cv);
    }

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
