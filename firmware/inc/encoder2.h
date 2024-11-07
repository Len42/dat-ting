#pragma once

namespace daisy2
{

/// @brief Handler for a rotary encoder with two quadrature switches and optional pushbutton
/// @remarks Interrupts are used to track encoder movement with switch debouncing.
/// Encoder position changes can be tracked at interrupt time via a Callback,
/// or polled by calling Getchange().
/// An optional pushbutton switch is also supported.
class Encoder
{
public:
    /// @brief Notification callback interface
    /// @remarks Abstract base class
    /// @note Methods may be called in an interrupt context
    class CallbackInterface
    {
    public:
        virtual void OnEncoderChange(int change) = 0;
        virtual void OnSwitchChange(bool fOn) = 0;
    };

    /// @brief Encoder configuration options
    /// @remarks Note that all 3 GPIOs must have the same pullup configuration.
    struct Config
    {
        daisy::Pin pinEncA;                     ///< GPIO pin for the encoder A input
        daisy::Pin pinEncB;                     ///< GPIO pin for the encoder B input
        daisy::Pin pinSwitch;                   ///< GPIO pin for the switch input (optional)
        Switch::Polarity polarity = Switch::Polarity::onHigh;   ///< On/off polarity (for all GPIO)
        GPIO::Pull pull = GPIO::Pull::NOPULL;   ///< GPIO pullup/down configuration (for all GPIO)
        CallbackInterface* pcallback = nullptr; ///< Callback for state-change notifications. May be null.
    };

    /// @brief Initialize a rotary encoder device
    /// @param cfg Encoder configuration options
    void Init(const Config& cfg)
    {
        config = cfg;
        gpioEncA.Init(config.pinEncA, GPIO::Mode::INT_BOTH, config.pull, GPIO::Speed::LOW, &irqHandler);
        gpioEncB.Init(config.pinEncB, GPIO::Mode::INT_BOTH, config.pull, GPIO::Speed::LOW, &irqHandler);
        // pinSwitch will be PinInvalid if the encoder has no push-switch.
        fHasPushbutton = config.pinSwitch.IsValid();
        if (fHasPushbutton) {
            pushButton.Init(config.pinSwitch, config.polarity, config.pull, &switchHandler);
        }
        UpdateEncoderState(); // make sure the current state is initialized reasonably
    }

    /// @brief Initialize a rotary encoder device
    /// @param pinEncA 
    /// @param pinEncB 
    /// @param pinSwitch 
    /// @param polarity 
    /// @param pull 
    /// @param pcallback 
    void Init(daisy::Pin pinEncA, daisy::Pin pinEncB, daisy::Pin pinSwitch,
            Switch::Polarity polarity, GPIO::Pull pull, CallbackInterface* pcallback)
    {
        Init({ .pinEncA=pinEncA, .pinEncB=pinEncB, .pinSwitch=pinSwitch,
             .polarity=polarity, .pull=pull, .pcallback=pcallback});
    }

    /// @brief Get the cumulative change in the encoder's position since the
    /// last time this was called
    /// @return The change in encoder position. Positive for clockwise, negative
    /// for counter-clockwise.
    int GetChange()
    {
        return encoderChange.exchange(0);
    }

    /// @brief Get the cumulative change in the encoder's position, with acceleration
    /// @return The change in encoder position. Positive for clockwise, negative
    /// for counter-clockwise.
    /// @remarks If the encoder position changes several times in a row, jump in
    /// larger incremments.
    /// Depends on this function being called regularly at appropriate intervals.
    int GetChangeAccel()
    {
        static constexpr unsigned fastCountThreshold = 3;
        static constexpr unsigned fastFactor = 5;
        int change = GetChange();
        if (change == 0) {
            fastCount = 0;
        } else if (++fastCount > fastCountThreshold) {
            change *= fastFactor;
        }
        return change;
    }

    /// @brief Return true if the pushbutton switch is currently on
    /// @return 
    bool IsPressed() {
        return fHasPushbutton ? pushButton.IsOn() : false;
    }

    /// @brief Return true if the pushbutton switch turned on (i.e. it
    /// transitioned off -> on) since the last time this was called
    /// @return 
    bool WasPressed() {
        return fHasPushbutton ? pushButton.TurnedOn() : false;
    }

// Event handling
protected:
    /// @brief Interrupt handler for the encoder quadrature switches
    /// @remarks This updates the encoder state, handles switch debouncing,
    /// counts encoder "clicks", and calls this object's callback.
    void OnEncoderInterrupt()
    {
        // Update the encoder state and add the incremental change to the
        // accumulated changes
        int change = UpdateEncoderState();
        encoderChange += change;
        if (change && config.pcallback) {
            config.pcallback->OnEncoderChange(change);
        }
    }

    /// @brief Interface adapter for @ref GPIO::IrqHandlerInterface
    /// @remarks This is used for both the "A" and "B" encoder switches.
    class EncoderIrqHandler : public GPIO::IrqHandlerInterface
    {
    public:
        EncoderIrqHandler(Encoder* enc) { owner = enc; }
        void OnInterrupt() override { owner->OnEncoderInterrupt(); }
    private:
        Encoder* owner;
    };

    EncoderIrqHandler irqHandler = EncoderIrqHandler(this);

    /// @brief Event handler for the pushbutton
    /// @remarks It just forwards the event to this object's callback.
    /// @param fOn 
    void OnSwitchChange(bool fOn)
    {
        if (config.pcallback) {
            config.pcallback->OnSwitchChange(fOn);
        }
    }

    /// @brief Interface adapter for @ref Switch::CallbackInterface
    class SwitchHandler : public Switch::CallbackInterface
    {
    public:
        SwitchHandler(Encoder* enc) { owner = enc; }
        void OnChange(bool fOn) override { owner->OnSwitchChange(fOn); }
     private:
        Encoder* owner;
   };

    /// @brief Event handler instance for the pushbutton
    SwitchHandler switchHandler = SwitchHandler(this);

// State machine to track encoder movement
protected:
    /// @brief States for the state machine that tracks the encoder movements
    enum class State { start, cw1, plus, cw2, ccw1, minus, ccw2 };

    /// @brief Current encoder state
    State state = State::start;

    /// @brief State table to handle quadrature encoder transitions
    static constexpr State stateTable[][2][2] = {
        /* start */ { { State::start, State::ccw1 }, { State::cw1, State::start } },
        /* cw1 */   { { State::start, State::start }, { State::cw1, State::plus } },
        /* plus */  { { State::start, State::cw2 }, { State::cw1, State::plus } },
        /* cw2 */   { { State::start, State::cw2 }, { State::start, State::plus } },
        /* ccw1 */  { { State::start, State::ccw1 }, { State::start, State::minus } },
        /* minus */ { { State::start, State::ccw1 }, { State::ccw2, State::minus } },
        /* ccw2 */  { { State::start, State::start }, { State::ccw2, State::minus } }
    };

    /// @brief Update the encoder's current state and return the incremental change
    /// @remarks A state machine handles the quadrature encoding from the encoder
    /// switches and mitigates switch bounce.
    /// @note This function is called from an interrupt handler.
    /// @param fPinA On/off state of encoder switch A
    /// @param fPinB On/off state of encoder switch B
    /// @return Encoder position change: +1, -1 or 0
    int UpdateEncoderState()
    {
        // Get the current state of the encoder inputs
        bool fPinA = gpioEncA.Read();
        bool fPinB = gpioEncB.Read();
        if (config.polarity == daisy2::Switch::Polarity::onLow) {
            fPinA = !fPinA;
            fPinB = !fPinB;
        }
        // Update the encoder state and check for increments
        State statePrev = state;
        state = stateTable[int(state)][fPinA][fPinB];
        int change = 0;
        if (state == State::plus && statePrev == State::cw1)
            change = +1;
        else if (state == State::minus && statePrev == State::ccw1)
            change = -1;
        return change;
    }

// Data members
protected:
    Config config;
    GPIO gpioEncA;
    GPIO gpioEncB;
    std::atomic<int> encoderChange = 0;
    unsigned fastCount = 0;
    bool fHasPushbutton = false;
    Switch pushButton;
};

} // namespace daisy2
