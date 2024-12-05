#pragma once

namespace daisy2
{

/// @brief Handler for on/off switches (replaces daisy::Switch because I don't like it)
/// @details The input pin's GPIO interrupt is used to keep track of the switch
/// state so constant polling is not required. (Note that this limits the choice
/// of input pins for multiple switches.)
///
/// An optional callback can be given to receive notifications whenever the
/// switch state changes. Alternatively the Switch may be polled by calling IsOn().
///
/// The @ref Debouncer class is used to debounce the switch input, so there
/// won't be repeated notifications from a single button press.
class Switch
{
public:
    /// @brief Specifies whether a high input counts as "on" or "off"
    enum class Polarity { onLow, onHigh };

    /// @brief On/off callback interface
    /// @details Abstract base class
    /// @note OnChange may be called in an interrupt context
    class CallbackInterface
    {
    public:
        virtual void OnChange(bool fOn) = 0;
    };

    /// @brief Switch configuration options
    struct Config
    {
        daisy::Pin pin;                         ///< GPIO pin that the switch is connected to
        Polarity polarity = Polarity::onHigh;   ///< Switch polarity
        GPIO::Pull pull = GPIO::Pull::NOPULL;   ///< GPIO pullup/down configuration
        CallbackInterface* pcallback = nullptr; ///< Callback for switch-change notifications. May be null.
    };

    /// @brief Initialize a switch input on a GPIO pin
    /// @param cfg Switch configuration options
    /// @details This function initializes the given GPIO pin as an input.
    /// @todo Make interrupts optional and implement Process() to call Debounce()
    void Init(const Config& cfg)
    {
        config = cfg;
        // Initialize the GPIO pin with an interrupt handler
        gpio.Init(config.pin, GPIO::Mode::INT_BOTH, config.pull, GPIO::Speed::LOW, &irqHandler);
        // Call the debounce function to set the initial state properly
        // KLUDGE: Disable the callback until we are properly initialized
        auto pcallbackSave = getAndSet(config.pcallback, nullptr);
        Debounce();
        TurnedOn(); TurnedOff(); // eat this notification
        config.pcallback = pcallbackSave;
    }

    /// @brief Initialize a switch input on a GPIO pin
    /// @param pin GPIO pin that the switch is connected to
    /// @param polarity Switch polarity
    /// @param pull GPIO pullup/down configuration
    /// @param pcallback Callback target for switch-change notifications. May be null.
    void Init(daisy::Pin pin, Polarity polarity, GPIO::Pull pull, CallbackInterface* pcallback)
    {
        Init({ .pin = pin, .polarity = polarity, .pull = pull, .pcallback = pcallback });
    }

    /// @brief Initialize a switch input on a GPIO pin
    /// @param pin GPIO pin that the switch is connected to
    /// @param polarity Switch polarity
    /// @param pull GPIO pullup/down configuration
    void Init(daisy::Pin pin, Polarity polarity, GPIO::Pull pull)
    {
        Init({ .pin = pin, .polarity = polarity, .pull = pull });
    }

    /// @brief Return true if the switch is currently on
    /// @return 
    bool IsOn() {
        return OnOffFromHighLow(debouncer.GetValue());
    }

    /// @brief Return true if the switch turned on (transitioned off -> on)
    /// since the last time this was called
    /// @return 
    /// @details This is a helper for polling pushbuttons.
    bool TurnedOn() { return turnedOn.exchange(false); }

    /// @brief Return true if the switch turned off (transitioned on -> off)
    /// since the last time this was called
    /// @return 
    /// @details This is a helper for polling pushbuttons.
    bool TurnedOff() { return turnedOff.exchange(false); }

protected:
    /// @brief Interrupt handler to update the current switch state
    /// @see daisy2::GPIO::IrqHandlerInterface
    /// @note This function is called in an interrupt context.
    void OnInterrupt()
    {
        Debounce();
    }

    /// @brief Interface adapter for @ref GPIO::IrqHandlerInterface
    /// @remark Yes, this could be eliminated by simply having Switch inherit
    /// from GPIO::IrqHandlerInterface, but that doesn't always work (e.g.
    /// @ref Encoder) so it's done this way here for consistency.
    class IrqHandler : public GPIO::IrqHandlerInterface
    {
    public:
        IrqHandler(Switch* sw) { owner = sw; }
        void OnInterrupt() override { owner->OnInterrupt(); }
    protected:
        Switch* owner;
    };

    /// @brief Switch debouncing using the @ref Debouncer class
    /// @details This must be called periodically, either from a GPIO interrupt
    /// when the input changes state or from the AudioCallback.
    /// @return Is the switch on?
    /// @note This function is called in an interrupt context.
    bool Debounce()
    {
        int updown = (gpio.Read() ? +1 : -1);
        auto [fHigh, fChanged] = debouncer.Process(updown);
        bool fIsOn = OnOffFromHighLow(fHigh);
        if (fChanged) {
            if (fIsOn) {
                turnedOn = true;
            } else {
                turnedOff = true;
            }
            if (config.pcallback) {
                config.pcallback->OnChange(fIsOn);
            }
        }
        return fIsOn;
    }

    /// @brief Convert a digital high/low input value to a logical switch on/off value
    /// @param fHigh Is the digital input high?
    /// @return Is the switch on?
    /// @details This is where the Polarity is accounted for.
    bool OnOffFromHighLow(bool fHigh) {
        return (fHigh == (config.polarity == Polarity::onHigh));
    }

protected:
    Config config;
    GPIO gpio;
    IrqHandler irqHandler = IrqHandler(this);
    Debouncer debouncer;
    std::atomic<bool> turnedOn = false;
    std::atomic<bool> turnedOff = false;
};

} // namespace daisy2
