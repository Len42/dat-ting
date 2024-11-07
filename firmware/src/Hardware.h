#pragma once

/// @brief Hardware definitions base template
template<HWType HWTYPE>
class HWBase
{
public:
    static constexpr HWType hwType = HWTYPE;

    using Sys = daisy2::System2;

    static inline daisy2::DaisySeed2 seed;

    using Pins = PinDefs<hwType>;

    using OledDisplay = daisy2::OledDisplay2<daisy2::FixedSSD13064WireSpi128x32Driver>;
    static inline OledDisplay display;

    static inline daisy2::Encoder encoder;

    static inline daisy2::Switch button;

    using CVIn = CVInBase<seed, hwType>;

    using CVOut = CVOutBase<seed>;

    static constexpr size_t audioBlockSize = 4;

public:
    /// @brief Initialize the Daisy Seed hardware and various attached devices
    static void Init()
    {
        // Configure and Initialize the Daisy Seed
        seed.Configure();
        seed.Init();

        // Initialize USB-serial output
        seed.StartLog(false);
        Sys::Delay(500); // short delay for serial terminal auto-connect
        seed.PrintLine("%s version %s", VersionInfo::progName, VersionInfo::name);
        unsigned version = unsigned(seed.CheckBoardVersion());
        seed.PrintLine("Daisy Seed version %u", version);
        version = unsigned(Sys::GetBootloaderVersion());
        seed.PrintLine("Bootloader version %u, %s", version, sym_to_string(BOOT_TYPE));
        seed.PrintLine("Hardware type %s", hwType == HWType::Module ? "Module" : "Prototype");
        // NOTE: Sometimes it's necessary to flush the serial output. Do this:
        seed.PrintFlush();

        // Initialize the OLED display
        OledDisplay::Config dispConfig;
        dispConfig.driver_config.transport_config.pin_config = {
            .dc = Pins::DisplayDC,
            .reset = Pins::DisplayReset
        };
        display.Init(dispConfig);
        display.SetFont(daisy2::Font::Dina_r400_10);

        // Initialize the rotary encoder
        encoder.Init({
            .pinEncA = Pins::EncoderA,
            .pinEncB = Pins::EncoderB,
            .pinSwitch = Pins::EncoderSw,
            .polarity = daisy2::Switch::Polarity::onLow,
            .pull = daisy2::GPIO::Pull::PULLUP,
            .pcallback = nullptr
        });

        // Initialize the pushbutton
        button.Init({
            .pin = Pins::Button,
            .polarity = daisy2::Switch::Polarity::onLow,
            .pull = daisy2::GPIO::Pull::PULLUP,
            .pcallback = nullptr
        });

        // Initialize the CV inputs
        CVIn::Init();

        // Initialize the CV output
        CVOut::Init();
    }
};


/// @brief Set the type of hardware being used
/// @note HW_TYPE must be #defined
static constexpr HWType HardwareType = HWType::HW_TYPE;

/// @brief Hardware definitions for the particular device in use
/// @see HardwareType
using HW = HWBase<HardwareType>;
