#pragma once

/// @brief Handler for display and encoder interactions
/// @remarks This is a bit of a mess because (1) I want to put everything in a
/// class so I can inject dependencies via template paramaters, but (2) I can't
/// put everything in a class because certain compilers won't let me specialize
/// a template inside a class. sigh.
namespace UIImpl {

/// @brief User interface state IDs
/// @remarks Each state has corresponding initState<> and doState<>
/// functions (see below)
enum class State { Warmup, Idle, Sleep, Message, SelectProg, SelectParam, SelectValue };

template<State state, typename UI> class StateImpl;

template<typename PROGRAM_LIST, PROGRAM_LIST& PROGS>
class UIBase
{
public:
    static constexpr PROGRAM_LIST& GetPrograms() { return PROGS; }

    /// @brief Display a message and enter Message state
    /// @param str Null-terminated string
    static void showMessage(std::string_view line1, std::string_view line2)
    {
        HW::display.Fill(false);
        HW::display.SetCursor(0, 0);
        HW::display.WriteString(line1, true);
        uint16_t y = HW::display.GetFont()->FontHeight;
        HW::display.SetCursor(0, y);
        HW::display.WriteString(line2, true);
        HW::display.Update();
        setState<State::Message>();
    }

    /// @brief User interface task
    class Task : public tasks::Task<Task>
    {
    public:
        unsigned intervalMicros() const { return 50'000; }

        void init() { setState<State::Warmup>(); }

        void execute() { stateExecFunction(); }
    };

protected:
    template<State state, typename UI> friend class StateImpl;

    template<typename ITEM_T, typename SUB, typename UI> friend class SelectItemBase;

    // Timeouts for various states
    static constexpr unsigned timeoutWarmup = 5'000;    ///< Splash screen timeout (ms)
    static constexpr unsigned timeoutIdle = 30'000;     ///< Idle screen timeout (ms)
    static constexpr unsigned timeoutSelect = 3'000;    ///< Select mode timeout (ms)
    static constexpr unsigned timeoutMessage = 3'000;   ///< Message timeout (ms)

    /// @brief The current state, represented as a function pointer
    /// @details Called periodically to handle input and update the UI state.
    static inline auto stateExecFunction = +[](){}; // that's C++, baby!

    /// @brief Set the UI to a different State
    template<State state>
    static void setState()
    {
        // Stop animation if it's running - presumably the new state won't want it!
        AnimationTask::StopAnim();
        stateExecFunction = StateImpl<state, UIBase>::exec;
        StateImpl<state, UIBase>::init();
    }

    /// @brief When the current timeout period will expire
    static inline uint32_t timeout = 0;

    /// @brief Set the timeout expiration time
    /// @param delayMs Milliseconds from now
    static void setTimeout(uint32_t delayMs)
    {
        timeout = HW::Sys::GetNow() + delayMs;
    }

    /// @brief Check if the current timeout period has expired
    /// @return Yes or no
    static bool checkTimeout()
    {
        return HW::Sys::GetNow() >= timeout;
    }

    /// @brief Check if the rotary encoder has been turned or pressed
    /// @details This clears the encoder state.
    /// @return Yes or no
    static bool checkEncoderActivity()
    {
        return HW::encoder.WasPressed() || HW::encoder.GetChangeAccel();
    }

    static inline bool buttonSaved = false;
    static inline unsigned potSaved = 0;

    static void saveButtonPotValue()
    {
        buttonSaved = HW::button.IsOn();
        potSaved = HW::CVIn::GetRaw(HW::CVIn::Pot);
    }

    static bool checkButtonPotActivity()
    {
        if (HW::button.IsOn() != buttonSaved) {
            return true;
        }
        int diff = int(HW::CVIn::GetRaw(HW::CVIn::Pot)) - int(potSaved);
        if (std::abs(diff) > 100) {
            return true;
        }
        return false;
    }

    static inline const Program::ParamDesc* currentParam = nullptr;
}; // class UIBase

/// @brief Template for each state's initialization and execution functions
/// @tparam state 
/// @remarks This is specialized for each State. The non-specialized version
/// is not defined.
/// @note All of this StateImpl stuff should be inside class UIBase, but as
/// mentioned above, that's not allowed.
template<State state, typename UI>
class StateImpl
{
public:
    static void init();
    static void exec();
};

/// @brief Warmup state: Show the startup splash screen until something happens or timeout
template<typename UI>
class StateImpl<State::Warmup, UI>
{
public:
    static void init()
    {
        UI::setTimeout(UI::timeoutWarmup);
        static WarmupAnimation animation;
        // TODO: AnimationTask dependency
        AnimationTask::StartAnim(&animation);
        // KLUDGE: Need to delay 10 ms for ADC values to work
        // TODO: Measure the actual ADC update rate!
        HW::Sys::Delay(10);
        UI::saveButtonPotValue();
    }

    static void exec()
    {
        // Exit this state after timeout or if something happens
        if (UI::checkTimeout() || UI::checkEncoderActivity() || UI::checkButtonPotActivity()) {
            UI::template setState<State::SelectProg>(); // that's C++, baby!
        }
    }

protected:
    class WarmupAnimDot : public Animation
    {
    public:
        void Init() override
        {
            // Draw a dot!
            auto x = HW::display.Width() / 2 - 1;
            auto y = HW::display.Height() / 2 - 1;
            HW::display.Fill(false);
            HW::display.DrawRect(x, y, x + 1, y + 1, true, true);
            HW::display.Update();
        }

        bool Step(unsigned step) override
        {
            // Do nothing, just hold the display for a while.
            return step < 20;
        }
    };

    class WarmupAnimGrowStatic : public Animation
    {
    public:
        void Init() override
        {
            xHalf = HW::display.Width() / 2;
            yHalf = HW::display.Height() / 2;
        }

        bool Step(unsigned step) override
        {
            unsigned xStep = (step + 2) * 3;
            unsigned yStep = step + 2;
            if (xStep >= xHalf) {
                // Done - the screen is filled
                return false;
            } else {
                // Animation: A rectangle of "static" that grows from the centre.
                // The simplest way to do this efficiently is to fill the entire
                // display buffer with random data and then black out the parts
                // that shouldn't show static.
                HW::display.FillStatic(true);
                // Blank out the parts of the display that don't show static
                uint16_t x1 = xHalf - xStep;
                uint16_t x2 = xHalf + xStep;
                uint16_t y1 = (yStep < yHalf) ? yHalf - yStep : 0;
                uint16_t y2 = (yStep < yHalf) ? yHalf + yStep : 2 * yHalf;
                HW::display.DrawRect(0, 0, x1, HW::display.Height(), false, true);
                HW::display.DrawRect(x2, 0, HW::display.Width(), HW::display.Height(), false, true);
                HW::display.DrawRect(x1, 0, x2, y1, false, true);
                HW::display.DrawRect(x1, y2, x2, HW::display.Height(), false, true);
                HW::display.Update();
                return true;
            }
        }

    protected:
        static inline uint16_t xHalf = 0;
        static inline uint16_t yHalf = 0;
    };

    class WarmupAnimHoldStatic : public Animation
    {
    public:
        void Init() override { }

        bool Step(unsigned step) override
        {
            if (step > 10) {
                return false;
            } else {
                HW::display.FillStatic(true);
                HW::display.Update();
                return true;
            }
        }
    };

    class WarmupAnimFadeStatic : public Animation
    {
    public:
        void Init() override
        {
            // KLUDGE: To combine the text & static properly:
            // Write the text to the display, save it in a buffer, fill the
            // display with static, merge the text back onto the display.
            HW::display.Fill(false);
            HW::display.SetCursor(0, 0);
            HW::display.WriteString(VersionInfo::progName, true);
            HW::display.SetCursor(0, 16);
            HW::display.WriteString(VersionInfo::name, true);
            std::ranges::fill(buf, 0x0F);
            HW::display.SaveBuf(buf);
            // don't call Update() here - we just want the display buffer contents
        }

        bool Step(unsigned step) override
        {
            static constexpr unsigned nSteps = 7;
            if (step > nSteps) {
                // All done
                return false;
            } else {
                if (step < nSteps) {
                    // "Fade" the static by drawing a new random pattern and then
                    // randomly clearing some of the pixels - more as time goes on
                    HW::display.FillStatic(true);
                    for (unsigned i = 0; i <= step; ++i) {
                        HW::display.FillStatic(false);
                    }
                } else {
                    // Last time through - clear any remaining static
                    HW::display.Fill(false);
                }
                // Draw text on top of the static so it "shows through" the static
                HW::display.MergeBuf(buf);
                HW::display.Update();
                return true;
            }
        }

    protected:
        std::array<uint8_t, HW::display.GetBufSize()> buf;
    };

    using WarmupAnimation = AnimationSeq<WarmupAnimDot,
                                        WarmupAnimGrowStatic,
                                        WarmupAnimHoldStatic,
                                        WarmupAnimFadeStatic>;
};

/// @brief Idle state: Display idle animation until something happens or timeout
template<typename UI>
class StateImpl<State::Idle, UI>
{
public:
    static void init()
    {
        auto program = UI::GetPrograms().GetCurrentProgram();
        auto animation = program ? program->GetAnimation() : nullptr;
        if (animation) {
            AnimationTask::StartAnim(animation);
        }
        UI::setTimeout(UI::timeoutIdle);
        UI::saveButtonPotValue();
    }

    static void exec()
    {
        if (UI::checkEncoderActivity()) {
            UI::template setState<State::SelectProg>();
        } else if (UI::checkTimeout()) {
            UI::template setState<State::Sleep>();
        } else if (UI::checkButtonPotActivity()) {
            // Pot has moved - reset the timeout to keep displaying the waveform
            UI::setTimeout(UI::timeoutIdle);
            UI::saveButtonPotValue();
        }
    }
};

/// @brief Sleep state: Clear the screen to prevent burn-in then wait until something happens
template<typename UI>
class StateImpl<State::Sleep, UI>
{
public:
    static void init()
    {
        HW::display.Fill(false);
        HW::display.Update();
        UI::saveButtonPotValue();
    }

    static void exec()
    {
        if (UI::checkEncoderActivity()) {
            UI::template setState<State::SelectProg>();
        } else if (UI::checkButtonPotActivity()) {
            // go back to idle state to display the idle animation for a while
            UI::template setState<State::Idle>();
        }
    }
};

/// @brief Message state: Display a message until something happens or timeout
/// @note This state should be entered by calling showMessage(), not by calling
/// setState() directly.
template<typename UI>
class StateImpl<State::Message, UI>
{
public:
    static void init()
    {
        UI::setTimeout(UI::timeoutMessage);
        UI::saveButtonPotValue();
    }

    static void exec()
    {
        if (UI::checkEncoderActivity()) {
            UI::template setState<State::SelectProg>();
        } else if (UI::checkButtonPotActivity() || UI::checkTimeout()) {
            UI::template setState<State::Idle>();
        }
    }
};

/// @brief Base class for item-selection UI using the rotary encoder
/// @remarks Subclass GetList() method must return a std::span<ITEM_T>.
/// I tried using views/iterators instead but it was a huge mess.
/// @tparam SUB 
/// @tparam UI 
template<typename ITEM_T, typename SUB, typename UI>
class SelectItemBase
{
public:
    static void init()
    {
        list = SUB::GetList();
        // If list is empty, skip this state...
        if (list.empty()) {
            UI::template setState<State::Idle>();
        // ...so list can be assumed non-empty from here down.
        } else {
            iDisplayed = iSelected;
            showDisplayedItem();
            UI::setTimeout(UI::timeoutSelect);
        }
    }

    static void exec()
    {
        if (UI::checkTimeout()) {
            UI::template setState<State::Idle>();
        } else {
            bool fButtonPressed = HW::encoder.WasPressed();
            int selectionChange = HW::encoder.GetChangeAccel();
            if (fButtonPressed) {
                iSelected = iDisplayed;
                SUB::OnSelect(iSelected, list[iSelected]);
            } else if (selectionChange) {
                iDisplayed = std::clamp(iDisplayed + selectionChange, 0, std::ssize(list) - 1);
                showDisplayedItem();
                UI::setTimeout(UI::timeoutSelect);
            }
        }
    }

protected:
    static inline std::span<ITEM_T> list;

    static inline int iSelected = 0;

    static inline int iDisplayed = 0;

    static void showDisplayedItem()
    {
        auto name = SUB::GetItemName(list[iDisplayed]);
        HW::display.Fill(false);
        HW::display.SetCursor(0, 0);
        HW::display.WriteString(SUB::Prompt(), true);
        HW::display.WriteString(":", true);
        HW::display.SetCursor(0, HW::display.GetFont()->FontHeight);
        HW::display.WriteString(name, true);
        HW::display.Update();
    }
};

/// @brief Select state: Select a program using the rotary encoder
/// @tparam UI 
template<typename UI>
class StateImpl<State::SelectProg, UI>
    : public SelectItemBase<Program*, StateImpl<State::SelectProg, UI>, UI>
{
public:
    static constexpr std::string_view Prompt() { return "Run Program"sv; }

    static std::span<Program*> GetList() { return UI::GetPrograms().GetList(); }

    static std::string_view GetItemName(Program*& prog) { return prog->GetName(); }

    static void OnSelect(int i, Program*& prog) {
        UI::GetPrograms().RunProgram(prog);
        UI::template setState<State::SelectParam>();
    }
};

/// @brief Select parameter: Select a program parameter to edit
/// @tparam UI 
template<typename UI>
class StateImpl<State::SelectParam, UI>
    : public SelectItemBase<const Program::ParamDesc,
                            StateImpl<State::SelectParam, UI>,
                            UI>
{
public:
    static constexpr std::string_view Prompt()
    {
        auto program = UI::GetPrograms().GetCurrentProgram();
        return program ? program->GetName() : "?"sv;
    }

    static std::span<const Program::ParamDesc> GetList()
    {
        auto program = UI::GetPrograms().GetCurrentProgram();
        return program ? program->GetParams()
                         : std::span<const Program::ParamDesc>();
    }

    static std::string_view GetItemName(const Program::ParamDesc& param) { return param.name; }

    static void OnSelect(int i, const Program::ParamDesc& param) {
        UI::currentParam = &param;
        // BUG: Need to initialize with the current value of the parameter somehow
        UI::template setState<State::SelectValue>();
    }
};

// TODO: index-based impl for SelectValue?

/// @brief Select value: Set the value of a program parameter
/// @tparam UI 
template<typename UI>
class StateImpl<State::SelectValue, UI>
    : public SelectItemBase<const std::string_view,
                            StateImpl<State::SelectValue, UI>,
                            UI>
{
protected:
    using BASE = SelectItemBase<const std::string_view,
                                StateImpl<State::SelectValue, UI>,
                                UI>;
public:
    static constexpr std::string_view Prompt()
    {
        auto param = UI::currentParam;
        return param ? param->name : "?"sv;
    }

    static std::span<const std::string_view> GetList()
    {
        auto param = UI::currentParam;
        if (param == nullptr) {
            return std::span<const std::string_view>{};
        } else {
            // KLUDGE: Unlike other states, the selected item must be initialized
            // each time from the currently-selected parameter value.
            auto program = UI::GetPrograms().GetCurrentProgram();
            if (program) {
                BASE::iSelected = program->GetParamValue(UI::currentParam);
            }
            return param->valueNames;
        }
    }

    static std::string_view GetItemName(const std::string_view& item) { return item; }

    static void OnSelect(int i, const std::string_view& param)
    {
        // Set parameter value
        auto program = UI::GetPrograms().GetCurrentProgram();
        if (program) {
            program->SetParamValue(UI::currentParam, unsigned(i));
        }
        UI::template setState<State::SelectParam>();
    }
};

} // namespace UIImpl
