#pragma once

template<HWType type> struct PinDefs;

template<>
struct PinDefs<HWType::Prototype>
{
    // Analog CV ADC inputs
    static constexpr daisy::Pin CVIn1 = daisy::seed::A1;
    static constexpr daisy::Pin CVIn2 = daisy::seed::A5;
    static constexpr daisy::Pin PotIn = daisy::seed::A2;

    static constexpr uint16_t ADCGateMin = 30000; // nearly 5V in [0, 10] range

    // Rotary encoder
    static constexpr daisy::Pin EncoderA = daisy::seed::D12;
    static constexpr daisy::Pin EncoderB = daisy::seed::D13;
    static constexpr daisy::Pin EncoderSw = daisy::seed::D11;

    // Pushbutton
    static constexpr daisy::Pin Button = daisy::seed::D18;

    // OLED display
    static constexpr daisy::Pin DisplayDC = daisy::seed::D9;
    static constexpr daisy::Pin DisplayReset = daisy::seed::D30;
};


template<>
struct PinDefs<HWType::Module>
{
    // Analog CV ADC inputs
    static constexpr daisy::Pin CVIn1 = daisy::seed::A5;
    static constexpr daisy::Pin CVIn2 = daisy::seed::A1;
    static constexpr daisy::Pin PotIn = daisy::seed::A6;

    static constexpr uint16_t ADCGateMin = 45000; // nearly 5V in [-12, 12] range

    // Rotary encoder
    static constexpr daisy::Pin EncoderA = daisy::seed::D12;
    static constexpr daisy::Pin EncoderB = daisy::seed::D13;
    static constexpr daisy::Pin EncoderSw = daisy::seed::D11;

    // Pushbutton
    static constexpr daisy::Pin Button = daisy::seed::D6;

    // OLED display
    static constexpr daisy::Pin DisplayDC = daisy::seed::D9;
    static constexpr daisy::Pin DisplayReset = daisy::seed::D30;
};
