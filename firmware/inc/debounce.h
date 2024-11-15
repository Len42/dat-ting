#pragma once

namespace daisy2
{

/// @brief Debouncing for two-state inputs such as digital GPIO or analog gate
class Debouncer
{
public:
    /// @brief Binary input debouncing using a state machine
    /// @param updown Specify whether the input is going high (updown > 0),
    /// low (< 0), or not changing (== 0).
    /// @return [fHigh, fChanged] Is the debounced input high or low, and has
    /// it changed?
    /// @details This should be called whenever the input goes high or low (when
    /// detected by interrupts) or whenever the input is read (by polling).
    std::pair<bool, bool> Debounce(int updown)
    {
        // FUBAR: std::lock_guard(mutex) required here, but std::mutex is
        // not supported by gcc stdlib in this environment. Sigh.
        // TODO: Make my own mutex out of std::atomic + a spin-lock.
        CheckSettled();
        bool fChanged = false;
        if (state == State::low && updown > 0) {
            state = State::highSettling;
            fChanged = true;
        } else if (state == State::high && updown < 0) {
            state = State::lowSettling;
            fChanged = true;
        }
        return { StateIsHigh(), fChanged };
    }

    /// @brief Return the current (debounced) high/low value
    /// @return Is the debounced input currently high?
    /// @details This function also checks the settling time and updates the state.
    /// It's equivalent to calling Debounce(0) but more efficient.
    bool GetValue()
    {
        // FUBAR: std::lock_guard(mutex) required here, but std::mutex is
        // not supported by gcc stdlib in this environment. Sigh.
        // TODO: Make my own mutex out of std::atomic + a spin-lock.
        CheckSettled();
        return StateIsHigh();
    }

protected:
    /// @brief Check if the input has settled down or if it's still bouncing,
    /// based on the time since it last changed.
    void CheckSettled()
    {
        uint32_t t = System2::GetUs();
        uint32_t dt = t - tLastCheck;
        if (dt >= dtSettlingTime) {
            // It's had time to settle down
            if (state == State::lowSettling) {
                state = State::low;
            } else if (state == State::highSettling) {
                state = State::high;
            }
        }
        tLastCheck = t;      
    }

    /// @brief Does the current internal State represent a logical high or low value?
    /// @return 
    bool StateIsHigh() const
    {
        return (state == State::high || state == State::highSettling);
    }

protected:
    /// @brief Debouncing states
    enum class State { low, lowSettling, high, highSettling };

    /// @brief Current state
    State state = State::low;

    /// @brief Keep track of the last time Debounce() was called
    uint32_t tLastCheck = 0;

    /// @brief Timeout for input settling
    /// @todo configurable
    static constexpr uint32_t dtSettlingTime = 2000; // us
};

} // namespace daisy2
