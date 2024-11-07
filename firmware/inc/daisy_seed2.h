#pragma once

#include <atomic>
#include <random>
#include <ranges>
#include <span>
#include <string_view>
using namespace std::string_view_literals;
#include <utility>

#include "daisy_seed.h"

#include "system2.h"
#include "gpio2.h"
#include "debounce.h"
#include "switch2.h"
#include "encoder2.h"
#include "audiobuf.h"
#include "display2.h"
#include "oled_display2.h"
#include "oled_ssd130x2.h"
#include "oled_fonts2.h"

namespace daisy2 {

/// @brief Customized version of @ref daisy::DaisySeed with additional functionality
class DaisySeed2 : public daisy::DaisySeed
{
// Initialization
public:
    /// @brief Initialize the Daisy Seed hardware
    /// @remarks Calls the base class Init() and does additional initialization
    void Init() {
        daisy::DaisySeed::Init();
        System2::Init();
    }

// Debug log enhancements
public:
    /// @brief Flush the debug log output
    /// @note This takes a bit of time to execute (~100us)
    void PrintFlush() {
        System2::DelayUs(500);
        Print("");
    }

// CheckBoardVersion() fix
public:
    /// @brief Seed hardware versions
    /// @remarks Like @ref daisy::DaisySeed::BoardVersion but with DAISY_SEED_REV7 added.
    /// The values match @ref daisy::DaisySeed::BoardVersion so they are not in release order.
    enum class BoardVersion {
        /** Daisy Seed 1 (Rev4) - original version, AK4556 codec */
        DAISY_SEED,
        /** Daisy Seed 1.1 (Rev5) - WM8731 codec */
        DAISY_SEED_1_1,
        /** Daisy Seed 2 - DFM improvements, PCM3060 codec */
        DAISY_SEED_2_DFM,
        /** Daisy Seed Rev7 - pre-DFM, PCM3060 codec */
        DAISY_SEED_REV7,
    };

    /// @brief Fixed version of @ref daisy::DaisySeed::CheckBoardVersion
    /// @return 
    BoardVersion CheckBoardVersion() {
        daisy::GPIO detect;
        detect.Init(daisy::Pin(daisy::PORTD, 5), daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
        if (!detect.Read()) {
            return BoardVersion::DAISY_SEED_REV7;
        } else {
            return (BoardVersion)daisy::DaisySeed::CheckBoardVersion(); // cast is OK
        }
    }

// Audio processing enhancements
public:
    /// @brief Begin audio processing
    /// @remarks This is an overload of @ref daisy::DaisySeed::StartAudio with nicer arguments.
    /// @param proc Audio processing callback
    void StartAudio(AudioCallback proc)
    {
        audioCallback = proc;
        daisy::DaisySeed::StartAudio(AudioCallbackWrapper);
    }

    /// @brief Re-implementation of @ref daisy::DaisySeed::StartAudio because it's hidden
    /// @param cb Audio processing callback
    void StartAudio(daisy::AudioHandle::InterleavingAudioCallback cb)
    {
        daisy::DaisySeed::StartAudio(cb);
    }

    /// @brief Set the audio processing callback
    /// @remarks This is an overload of @ref daisy::DaisySeed::ChangeAudioCallback with nicer arguments.
    /// @param proc Audio processing callback
    void ChangeAudioCallback(AudioCallback proc)
    {
        audioCallback = proc;
        daisy::DaisySeed::ChangeAudioCallback(AudioCallbackWrapper);
    }

    /// @brief Re-implementation of @ref daisy::DaisySeed::ChangeAudioCallback because it's hidden
    /// @param cb Audio processing callback
    void ChangeAudioCallback(daisy::AudioHandle::InterleavingAudioCallback cb)
    {
        daisy::DaisySeed::ChangeAudioCallback(cb);
    }

protected:
    static inline AudioCallback audioCallback = nullptr;

    static void AudioCallbackWrapper(daisy::AudioHandle::InterleavingInputBuffer inbuf,
                                     daisy::AudioHandle::InterleavingOutputBuffer outbuf,
                                     size_t bufsize)
    {
        AudioInBuf inbufS = convertAudioInBuf(inbuf, bufsize);
        AudioOutBuf outbufS = convertAudioOutBuf(outbuf, bufsize);
        audioCallback(inbufS, outbufS);
    }
};

}
