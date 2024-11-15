#pragma once

namespace daisy2 {

/// @brief Customized version of @ref daisy::System with additional functionality
class System2 : public daisy::System
{
public:
    /// @brief System initialization
    static void Init()
    {
        InitTime();
    }

// Timekeeping
public:
    /// @brief Return elapsed time since startup in CPU ticks
    /// @return 
    /// @details Much more efficient than @ref daisy::System::GetTick
    static uint32_t GetTick() { return TIM2->CNT; }

    /// @brief Return elapsed time since startup in microseconds
    /// @return 
    /// @details Much more efficient than @ref daisy::System::GetUs
    /// @bug GetUs() wraps around after only 20 seconds or so (both this version
    /// and the implementation in daisy::System)
    static uint32_t GetUs() { return GetTick() / clockFreqAdj; }

    using timeus_t = uint64_t;

    /// @brief Fixed version of GetUs() that doesn't wrap around
    /// @return Microseconds since startup (64 bits)
    /// @details This returns an elapsed time value that doesn't wrap around
    /// every 21.5 seconds like @ref daisy::System::GetUs nor every 71.5 minutes
    /// like @ref GetUs.
    /// The return value is 64 bits so it doesn't wrap around basically ever.
    /// @note This function must be called at least once every 21.5 seconds
    /// to correctly keep track of when GetUs() wraps around.
    static timeus_t GetUsLong()
    {
        // NOTE: TODO: not interrupt-safe
        static uint32_t tShortSave = 0;
        static timeus_t tOffset = 0;
        // tWrap is the max value returned by GetUs()
        static constexpr uint32_t tWrap = uint32_t(0x100000000ull / 200);
        uint32_t tShort = GetUs();
        if (tShort < tShortSave) {
            tOffset += tWrap;
        }
        tShortSave = tShort;
        return tShort + tOffset;
    }

protected:
    /// @brief Initialize cached values to speed up timekeeping functions
    static void InitTime()
    {
        // TIM ticks run at 2x PClk
        // there is a switchable 1/2/4 prescalar available
        // that is not yet implemented.
        // Once it is, it should be taken into account here as well.
        uint32_t clkfreq_hz = (daisy::System::GetPClk1Freq() * 2);
        uint32_t clockFreq  = clkfreq_hz / (TIM2->PSC + 1);
        clockFreqAdj = clockFreq / 1000000;
    }

    static inline uint32_t clockFreqAdj = 0;
};

using DebugLog = daisy::Logger<daisy::LOGGER_INTERNAL>;

}
