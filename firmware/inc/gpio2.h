#pragma once

namespace daisy2
{

/// @brief A null or invalid GPIO pin specifier
static constexpr daisy::Pin PinNull = daisy::Pin();

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t pinBit);

/// @brief General Purpose I/O control 
/// @remarks This is a re-implementation of daisy::GPIO with more features added.
/// It's done by copying the whole class, not subclassing, due to member access issues.
///
/// GPIO interrupts are supported on input pins. The caller may poll an input
/// pin by calling Read() or by receiving interrupt-time notifications via a
/// Callback or both.
class GPIO
{
public:
    /// @brief Mode of operation for the specified GPIO
    enum class Mode
    {
        INPUT,      ///< Input for reading state of pin
        OUTPUT,     ///< Output w/ push-pull configuration
        OPEN_DRAIN, ///< Output w/ open-drain configuration
        ANALOG,     ///< Analog for connection to ADC or DAC peripheral
        INT_RISING, ///< Interrupt on rising edge (also input)
        INT_FALLING,///< Interrupt on falling edge (also input)
        INT_BOTH,   ///< Interrupt on rising and falling edge (also input)
    };

    /// @brief Configures whether an internal Pull up or Pull down resistor is used. 
    /// @remarks
    /// Internal Pull up/down resistors are typically 40k ohms, and will be between
    /// 30k and 50k.
    /// When the Pin is configured in Analog mode, the pull up/down resistors are
    /// disabled by hardware. 
    enum class Pull
    {
        NOPULL,   ///< No pull up resistor
        PULLUP,   ///< Internal pull up enabled
        PULLDOWN, ///< Internal pull down enabled
    };

    /// @brief Output speed controls the drive strength, and slew rate of the pin
    enum class Speed
    {
        LOW,
        MEDIUM,
        HIGH,
        VERY_HIGH,
    };

    /// @brief GPIO interrupt callback interface
    /// @remarks Abstract base class
    class IrqHandlerInterface
    {
    public:
        virtual void OnInterrupt() = 0;
    };

    /// @brief Configuration for a given GPIO
    struct Config
    {
        daisy::Pin pin;
        Mode  mode = Mode::INPUT;
        Pull  pull = Pull::NOPULL;
        Speed speed = Speed::LOW;
        IrqHandlerInterface* pirqHandler = nullptr;
    };

    GPIO() {}

    /// @brief Initialize the GPIO from a Config struct 
    /// @param cfg reference to a Config struct populated with the desired settings
    void Init(const Config &cfg) {
        cfg_ = cfg;

        if(!cfg_.pin.IsValid())
            return;

        GPIO_InitTypeDef ginit;
        switch(cfg_.mode)
        {
            case Mode::INPUT:       ginit.Mode = GPIO_MODE_INPUT; break;
            case Mode::OUTPUT:      ginit.Mode = GPIO_MODE_OUTPUT_PP; break;
            case Mode::OPEN_DRAIN:  ginit.Mode = GPIO_MODE_OUTPUT_OD; break;
            case Mode::ANALOG:      ginit.Mode = GPIO_MODE_ANALOG; break;
            case Mode::INT_RISING:  ginit.Mode = GPIO_MODE_IT_RISING; break;
            case Mode::INT_FALLING: ginit.Mode = GPIO_MODE_IT_FALLING; break;
            case Mode::INT_BOTH:    ginit.Mode = GPIO_MODE_IT_RISING_FALLING; break;
            default:                ginit.Mode = GPIO_MODE_INPUT; break;
        }
        switch(cfg_.pull)
        {
            case Pull::PULLUP: ginit.Pull = GPIO_PULLUP; break;
            case Pull::PULLDOWN: ginit.Pull = GPIO_PULLDOWN; break;
            case Pull::NOPULL:
            default: ginit.Pull = GPIO_NOPULL;
        }
        switch(cfg_.speed)
        {
            case Speed::VERY_HIGH: ginit.Speed = GPIO_SPEED_FREQ_VERY_HIGH; break;
            case Speed::HIGH: ginit.Speed = GPIO_SPEED_FREQ_HIGH; break;
            case Speed::MEDIUM: ginit.Speed = GPIO_SPEED_FREQ_MEDIUM; break;
            case Speed::LOW:
            default: ginit.Speed = GPIO_SPEED_FREQ_LOW;
        }

        port_base_addr_ = GetGPIOBaseRegister();
        // Start the relevant GPIO clock
        switch(cfg_.pin.port)
        {
            using enum daisy::GPIOPort;
            case PORTA: __HAL_RCC_GPIOA_CLK_ENABLE(); break;
            case PORTB: __HAL_RCC_GPIOB_CLK_ENABLE(); break;
            case PORTC: __HAL_RCC_GPIOC_CLK_ENABLE(); break;
            case PORTD: __HAL_RCC_GPIOD_CLK_ENABLE(); break;
            case PORTE: __HAL_RCC_GPIOE_CLK_ENABLE(); break;
            case PORTF: __HAL_RCC_GPIOF_CLK_ENABLE(); break;
            case PORTG: __HAL_RCC_GPIOG_CLK_ENABLE(); break;
            case PORTH: __HAL_RCC_GPIOH_CLK_ENABLE(); break;
            case PORTI: __HAL_RCC_GPIOI_CLK_ENABLE(); break;
            case PORTJ: __HAL_RCC_GPIOJ_CLK_ENABLE(); break;
            case PORTK: __HAL_RCC_GPIOK_CLK_ENABLE(); break;
            default: break;
        }
        // Set pin based on stm32 schema
        ginit.Pin = (1 << cfg_.pin.pin);
        HAL_GPIO_Init(port_base_addr_, &ginit);

        if (ginit.Mode & EXTI_IT) {
            // Turn on interrupt handling
            if (!IsIrqAvailable(cfg_.pin)) {
                daisy::Logger<daisy::LOGGER_INTERNAL>::PrintLine(
                    "WARNING: GPIO interrupt for pin %u/%u already used "
                    "for pin %u on a different port",
                    unsigned(cfg_.pin.port), cfg_.pin.pin, cfg_.pin.pin);
            }
            irqHandlers[cfg_.pin.pin] = cfg_.pirqHandler;
            IRQn_Type irq = mapGpiopinIrqtype[cfg_.pin.pin];
            HAL_NVIC_SetPriority(irq, 0, 0);
            HAL_NVIC_EnableIRQ(irq);
        }
    }

    /// @brief Initialize the GPIO with a Configuration struct, and explicit pin 
    /// @param p Pin specifying the physical connection on the hardware
    /// @param cfg reference to a Config struct populated with the desired settings. 
    ///        Config::pin will be overwritten
    void Init(daisy::Pin p, const Config &cfg) { cfg_ = cfg; cfg_.pin = p; Init(cfg_); }

    /// @brief Explicity initialize all configuration for the GPIO 
    /// @param p Pin specifying the physical connection on the hardware
    /// @param m Mode specifying the behavior of the GPIO (input, output, etc.). Defaults to Mode::INPUT
    /// @param pu Pull up/down state for the GPIO. Defaults to Pull::NOPULL
    /// @param sp Speed setting for drive strength/slew rate. Defaults to Speed::Slow
    void Init(daisy::Pin pin, Mode mode, Pull pull, Speed speed) {
        Init({ .pin=pin, .mode=mode, .pull=pull, .speed=speed, .pirqHandler = nullptr });
    }

    /// @brief Explicity initialize all configuration for the GPIO 
    /// @param p Pin specifying the physical connection on the hardware
    /// @param m Mode specifying the behavior of the GPIO (input, output, etc.). Defaults to Mode::INPUT
    /// @param pu Pull up/down state for the GPIO. Defaults to Pull::NOPULL
    /// @param sp Speed setting for drive strength/slew rate. Defaults to Speed::Slow
    /// @param ph IRQ handler
    void Init(daisy::Pin pin, Mode mode, Pull pull, Speed speed, IrqHandlerInterface* pirqHandler) {
        Init({ .pin=pin, .mode=mode, .pull=pull, .speed=speed, .pirqHandler = pirqHandler });
    }

    /// @brief Deinitializes the GPIO pin */
    void DeInit() const {
        if (cfg_.pin.IsValid()) {
            HAL_GPIO_DeInit(port_base_addr_, (1 << cfg_.pin.pin));
            HAL_NVIC_DisableIRQ(mapGpiopinIrqtype[cfg_.pin.pin]);
        }
    }

    /// @brief Reads the state of the GPIO.
    /// @return State of the GPIO unless Mode is set to Mode::Analog, then always false
    bool Read() const {
        return HAL_GPIO_ReadPin(port_base_addr_, (1 << cfg_.pin.pin));
    }

    /// @brief Changes the state of the GPIO hardware when configured as an OUTPUT. 
    /// @param state setting true writes an output HIGH, while setting false writes an output LOW.
    void Write(bool state) const {
        HAL_GPIO_WritePin(port_base_addr_,
                        (1 << cfg_.pin.pin),
                        (GPIO_PinState)state);
    }

    /// @brief flips the current state of the GPIO. 
    /// If it was HIGH, it will go LOW, and vice versa.
    void Toggle() const {
        HAL_GPIO_TogglePin(port_base_addr_, (1 << cfg_.pin.pin));
    }

    /// Return a reference to the internal Config struct */
    Config &GetConfig() { return cfg_; }

    /// @brief Enable interrupt handling on this GPIO (but only if it was appropriately initialized) */
    void EnableIrq() const {
        if (cfg_.pin.IsValid() && cfg_.pirqHandler) {
            HAL_NVIC_EnableIRQ(mapGpiopinIrqtype[cfg_.pin.pin]);
        }
    }

    /// @brief Disable interrupt handling on this GPIO */
    void DisableIrq() const {
        if (cfg_.pin.IsValid()) {
            HAL_NVIC_DisableIRQ(mapGpiopinIrqtype[cfg_.pin.pin]);
        }
    }

    /// @brief Return true if the GPIO interrupt corresponding to the given pin is unused,
    /// false if it's been initialized already (for a pin on a different port).
    /// @param pin 
    /// @return 
    static bool IsIrqAvailable(daisy::Pin pin) {
        return pin.IsValid() && irqHandlers[pin.pin] == nullptr;
    }

protected:
    friend void HAL_GPIO_EXTI_Callback(uint16_t pinBit);

    /// @brief Call the interrupt handler for the given pin
    /// @param pin Pin number (relative to the port the pin is in)
    /// @remarks Helper function called from HAL_GPIO_EXTI_Callback
    static void CallIrqHandler(unsigned pin)
    {
        IrqHandlerInterface* phandler = nullptr;
        if (pin < 16) { // just in case
            phandler = irqHandlers[pin];
        }
        if (phandler) {
            phandler->OnInterrupt();
        }
    }

    /// @brief Return the base address of the relevent GPIO register
    /// @return 
    GPIO_TypeDef *GetGPIOBaseRegister() {
        switch(cfg_.pin.port)
        {
            using enum daisy::GPIOPort;
            case PORTA: return GPIOA;
            case PORTB: return GPIOB;
            case PORTC: return GPIOC;
            case PORTD: return GPIOD;
            case PORTE: return GPIOE;
            case PORTF: return GPIOF;
            case PORTG: return GPIOG;
            case PORTH: return GPIOH;
            case PORTI: return GPIOI;
            case PORTJ: return GPIOJ;
            case PORTK: return GPIOK;
            default: return NULL;
        }
    }

    /// @brief Internal copy of the pin configuration
    Config cfg_;

    /// @brief Internal pointer to base address of the relevent GPIO register */
    GPIO_TypeDef* port_base_addr_;

    /// @brief IRQ handlers for all the GPIO interrupts
    static inline IrqHandlerInterface* irqHandlers[16] = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 
    };

    /// @brief Map from pin number to interrupt number (because having one interrupt per pin
    /// would make too much sense)
    static constexpr IRQn_Type mapGpiopinIrqtype[16] = {
    #if defined(STM32H7)
        EXTI0_IRQn,
        EXTI1_IRQn,
        EXTI2_IRQn,
        EXTI3_IRQn,
        EXTI4_IRQn,
        EXTI9_5_IRQn,
        EXTI9_5_IRQn,
        EXTI9_5_IRQn,
        EXTI9_5_IRQn,
        EXTI9_5_IRQn,
        EXTI15_10_IRQn,
        EXTI15_10_IRQn,
        EXTI15_10_IRQn,
        EXTI15_10_IRQn,
        EXTI15_10_IRQn,
        EXTI15_10_IRQn,
    #else
        #error Unsupported STM version
    #endif
    };
};

/// @brief Simple interrupt handler that can be used to poll for interrupts
class BasicIrqHandler : public GPIO::IrqHandlerInterface
{
public:
    void OnInterrupt() override { fTriggered = true; }

    /// @brief Return and reset the flag inicating whether an interrupt has
    /// occurred since the last check
    /// @return 
    bool CheckTriggered() { return fTriggered.exchange(false); }

protected:
    std::atomic<bool> fTriggered = false;
};

} // namespace daisy2

// HAL callbacks for interrupt handling

extern "C" {

namespace daisy2 {

/// @brief Callback from HAL interrupt handler
/// @param pinBit 
/// @note Must be in namespace daisy2 for the friend declaration in daisy2::GPIO to work
void HAL_GPIO_EXTI_Callback(uint16_t pinBit)
{
    unsigned pin;
    for (pin = 0; pinBit != 1; ++pin, pinBit >>= 1)
        ;
    daisy2::GPIO::CallIrqHandler(pin);
}

}

void EXTI0_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0); }

void EXTI1_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1); }

void EXTI2_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2); }

void EXTI3_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3); }

void EXTI4_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4); }

void EXTI9_5_IRQHandler()
{
    for (uint32_t pin = GPIO_PIN_5; pin <= GPIO_PIN_9; pin = pin << 1) {
        HAL_GPIO_EXTI_IRQHandler(pin);
    }
}

void EXTI15_10_IRQHandler()
{
    for (uint32_t pin = GPIO_PIN_10; pin <= GPIO_PIN_15; pin = pin << 1) {
        HAL_GPIO_EXTI_IRQHandler(pin);
    }
}

} // extern "C"
