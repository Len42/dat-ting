#pragma once

/// @brief Hardware definitions base template
/// @tparam HWTYPE Hardware type (Prototype or Module)
template<HWType HWTYPE>
class HWBase
{
public:
    /// @brief Hardware type (Prototype or Module)
    static constexpr HWType hwType = HWTYPE;

    /// @brief Abbreviation for daisy2::System2 and daisy::System
    using Sys = daisy2::System2;

    /// @brief The Daisy Seed object
    static inline daisy2::DaisySeed2 seed;

    /// @brief GPIO pin definitions
    using Pins = PinDefs<hwType>;

    /// @brief The OLED display type
    using OledDisplay = daisy2::OledDisplay2<daisy2::FixedSSD13064WireSpi128x32Driver>;
    /// @brief The OLED display object
    static inline OledDisplay display;

    /// @brief The rotary encoder
    static inline daisy2::Encoder encoder;

    /// @brief The pushbutton
    static inline daisy2::Switch button;

    /// @brief Control voltage inputs
    using CVIn = CVInBase<seed, hwType>;

    /// @brief Control voltage outputs
    using CVOut = CVOutBase<seed>;

    /// @brief Sample rate for audio processing
    /// @details Compile-time constant that matches @ref daisy::DaisySeed::AudioSampleRate
    static constexpr unsigned sampleRate = 48000u;

    /// @brief Sample rate setting corresponding to @ref sampleRate
    static constexpr daisy::SaiHandle::Config::SampleRate sampleRateSetting =
        daisy::SaiHandle::Config::SampleRate::SAI_48KHZ;

    /// @brief Block size for audio processing
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
        dispConfig.transport_config.pin_config = {
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

        // Initialize the CV outputs
        CVOut::Init();
    }

    /// @brief Start audio processing
    static void StartProcessing(daisy2::AudioCallback processingCallback)
    {
        static_assert(audioBlockSize > 0);
        seed.SetAudioSampleRate(sampleRateSetting);
        seed.SetAudioBlockSize(audioBlockSize);
        if (seed.AudioSampleRate() != sampleRate) {
            daisy2::DebugLog::PrintLine("WARNING: Sample rate mismatch");
        }
        seed.StartAudio(processingCallback);
    }

    /// @brief Stop audio processing
    static void StopProcessing() { seed.StopAudio(); }
};

/// @brief Set the type of hardware being used
/// @note HW_TYPE must be #defined
static constexpr HWType HardwareType = HWType::HW_TYPE;

/// @brief Hardware definitions for the particular device in use
/// @see HardwareType
using HW = HWBase<HardwareType>;
