#pragma once

// TODO: Simplify/remove this! Need a better way for Programs to get the CV inputs
struct ProcessArgs
{
    daisy2::AudioInBuf inbuf;
    daisy2::AudioOutBuf outbuf;
    unsigned cvIn1;
    unsigned cvIn2;
    unsigned cvInPot;
    bool fGateOn1;
    bool fGateOn2;
    bool fGateOnBut;
    bool fGateOff1;
    bool fGateOff2;
    bool fGateOffBut;

// TODO: REMOVE
    constexpr std::optional<unsigned> GetCV(unsigned input) const
    {
        switch (input) {
            using enum HW::CVIn::ADC;
            case CV1:   return cvIn1;
            case CV2:   return cvIn2;
            case Pot:   return cvInPot;
            default:    return std::nullopt;
        }
    }

    constexpr bool GateOn(unsigned input) const
    {
        switch (input) {
            case HW::CVIn::CV1:     return fGateOn1;
            case HW::CVIn::CV2:     return fGateOn2;
            case HW::CVIn::Button:  return fGateOnBut;
            default:                return false;
        }
    }

    constexpr bool GateOff(unsigned input) const
    {
        switch (input) {
            case HW::CVIn::CV1:     return fGateOff1;
            case HW::CVIn::CV2:     return fGateOff2;
            case HW::CVIn::Button:  return fGateOffBut;
            default:                return false;
        }
    }
};

class Animation;

class Program
{
public:
    // Program parameters
    union ParamVal
    {
        bool b;
        unsigned n;
        float fl;
    };

    enum class PType { None, Bool, Num, Float, CVSource };

    using ParamValPtr = ParamVal Program::*;
    #define PARAM_CAST(param) static_cast<ParamValPtr>(&this_t::param)

    struct ParamDesc
    {
        std::string_view name;
        PType type = PType::Bool;
        ParamValPtr pvalue = nullptr; // pointer to value member
        std::span<const std::string_view> valueNames;
    };

    // Virtual functions for program implementation

    virtual constexpr std::string_view GetName() const = 0;

    // default implementation - no program parameters
    virtual constexpr std::span<const ParamDesc> GetParams() const
    {
        return {};
    }

    virtual void Init() = 0;

    // TODO: Instead of virtual Process(), could have virtual GetProcessFunc()
    // that returns a static function pointer - that would be more efficient in
    // AudioCallback
    virtual void Process(ProcessArgs& args) = 0;

    virtual Animation* GetAnimation() const = 0;

    unsigned GetParamValue(const ParamDesc* param)
    {
        if (param && param->pvalue) {
            ParamVal& paramVal = this->*(param->pvalue);
            switch (param->type) {
                using enum PType;
                case Bool:      return unsigned(paramVal.b);
                case Num:       return paramVal.n;
                case Float:     return std::clamp(unsigned(paramVal.fl * 100.f), 0u, 100u);
                case CVSource:  return paramVal.n;
                default:        return 0;
            }
        } else {
            return 0;
        }
    }

    void SetParamValue(const ParamDesc* param, unsigned n)
    {
        if (param && param->pvalue) {
            ParamVal& paramVal = this->*(param->pvalue);
            switch (param->type) {
                using enum PType;
                case Bool:  paramVal.b = bool(n); break;
                case Num:   paramVal.n = n; break;
                case Float: paramVal.fl = float(n) / 100.f; break;
                case CVSource:
                    paramVal.n = n;
                    FixCVSources(param->pvalue);
                    break;
                default:    break;
            }
        }
    }

    static ProcessArgs MakeProcessArgs(daisy2::AudioInBuf inbuf, daisy2::AudioOutBuf outbuf)
    {
        return {
            .inbuf = inbuf,
            .outbuf = outbuf,
            .cvIn1 = HW::CVIn::GetRaw(HW::CVIn::CV1),
            .cvIn2 = HW::CVIn::GetRaw(HW::CVIn::CV2),
            .cvInPot = HW::CVIn::GetRaw(HW::CVIn::Pot),
            .fGateOn1 = HW::CVIn::GateOn(HW::CVIn::CV1),
            .fGateOn2 = HW::CVIn::GateOn(HW::CVIn::CV2),
            .fGateOnBut = HW::button.TurnedOn(),
            .fGateOff1 = HW::CVIn::GateOff(HW::CVIn::CV1),
            .fGateOff2 = HW::CVIn::GateOff(HW::CVIn::CV2),
            .fGateOffBut = HW::button.TurnedOff()
        };
    }

protected:
    template<typename PROG, PType T, ParamVal PROG::* P>
    auto GetParamValue() const
    {
        const ParamVal& paramVal = static_cast<const PROG*>(this)->*(P);
        if constexpr (T == PType::Bool) {
            return paramVal.b;
        } else if constexpr (T == PType::Num || T == PType::CVSource) {
            return paramVal.n;
        } else if constexpr (T == PType::Float) {
            return paramVal.fl;
        } else {
            return 0;
        }
    }

    template<typename PROG, PType T, ParamVal PROG::* P>
    void SetParamValue(auto v)
    {
        ParamVal& paramVal = static_cast<PROG*>(this)->*(P);
        if constexpr (T == PType::Bool) {
            static_assert(std::is_same_v<decltype(v), bool>);
            paramVal.b = bool(v);
        } else if constexpr (T == PType::Num) {
            static_assert(std::is_same_v<decltype(v), unsigned>);
            paramVal.n = unsigned(v);
        } else if constexpr (T == PType::Float) {
            static_assert(std::is_same_v<decltype(v), float>);
            paramVal.fl = float(v);
        } else if constexpr (T == PType::CVSource) {
            static_assert(std::is_same_v<decltype(v), unsigned>);
            paramVal.n = unsigned(v);
            FixCVSources(static_cast<ParamValPtr>(P));
        } else {
            // ERROR
        }
    }

    void FixCVSources(ParamValPtr pvalue)
    {
        // KLUDGE: Any CV sources set to "Pot", other than the one given, are
        // reset to "Fixed". That makes menu-diving much easier.
        if ((this->*pvalue).n == HW::CVIn::Pot) {
            for (auto&& paramDesc : GetParams()) {
                if (paramDesc.type == PType::CVSource && paramDesc.pvalue != pvalue) {
                    ParamVal& paramVal = this->*(paramDesc.pvalue);
                    if (paramVal.n == HW::CVIn::Pot) {
                        paramVal.n = HW::CVIn::Fixed;
                    }
                }
            }
        }
    }

protected:
    /// @brief Empty std::optional, handy for use with .and_then()
    static constexpr std::optional<bool> emptyOpt {};

    /// @brief Value list for on/off parameters
    static constexpr std::array paramValuesBool = { "Off"sv, "On"sv };

    /// @brief Value list for float parameters (int values 0 to 100 -> float range [0, 1])
    static constexpr std::array paramValuesFloat = {
         "0"sv,  "1"sv,  "2"sv,  "3"sv,  "4"sv,  "5"sv,  "6"sv,  "7"sv,  "8"sv,  "9"sv,
        "10"sv, "11"sv, "12"sv, "13"sv, "14"sv, "15"sv, "16"sv, "17"sv, "18"sv, "19"sv,
        "20"sv, "21"sv, "22"sv, "23"sv, "24"sv, "25"sv, "26"sv, "27"sv, "28"sv, "29"sv,
        "30"sv, "31"sv, "32"sv, "33"sv, "34"sv, "35"sv, "36"sv, "37"sv, "38"sv, "39"sv,
        "40"sv, "41"sv, "42"sv, "43"sv, "44"sv, "45"sv, "46"sv, "47"sv, "48"sv, "49"sv,
        "50"sv, "51"sv, "52"sv, "53"sv, "54"sv, "55"sv, "56"sv, "57"sv, "58"sv, "59"sv,
        "60"sv, "61"sv, "62"sv, "63"sv, "64"sv, "65"sv, "66"sv, "67"sv, "68"sv, "69"sv,
        "70"sv, "71"sv, "72"sv, "73"sv, "74"sv, "75"sv, "76"sv, "77"sv, "78"sv, "79"sv,
        "80"sv, "81"sv, "82"sv, "83"sv, "84"sv, "85"sv, "86"sv, "87"sv, "88"sv, "89"sv,
        "90"sv, "91"sv, "92"sv, "93"sv, "94"sv, "95"sv, "96"sv, "97"sv, "98"sv, "99"sv,
        "100"sv
    };

    /// @brief Value list for the source of a gate signal
    static constexpr std::array paramValuesGateSource = {
        // The order must match CVIn::ADC
        "CV1"sv, "CV2"sv, "Button"sv
    };

    /// @brief Value list for the source of a CV parameter
    static constexpr std::array paramValuesCVSource = {
        // The order must match CVIn::ADC
        "CV1"sv, "CV2"sv, "Fingers"sv, "Fixed"sv
    };

    /// @brief Value list for notes / key signatures
    static constexpr std::array paramValuesKey = {
        "C"sv, "C#/Db"sv, "D"sv, "D#/Eb"sv, "E"sv, "F"sv,
        "F#/Gb"sv, "G"sv, "G#/Ab"sv, "A"sv, "A#/Bb"sv, "B"sv
    };
};

// X-macro helpers for declaring a Program's configurable parameters

#define DECL_PARAM_VAR(name, desc, defVal, ptype, defField, ...) \
    ParamVal param##name = {.defField=defVal};
#define DECL_PARAM_DESC(name, desc, defVal, ptype, defField, valList, ...) \
    { desc##sv, PType::ptype, PARAM_CAST(param##name), valList },
#define DECL_PARAM_GET_SET(name, desc, defVal, ptype, ...) \
    auto Get##name() const { return GetParamValue<this_t, PType::ptype, &this_t::param##name>(); } \
    void Set##name(auto v) { SetParamValue<this_t, PType::ptype, &this_t::param##name>(v); }

#define PARAM_BOOL(ITEM, name, desc, defVal) \
    ITEM(name, desc, defVal, Bool, b, paramValuesBool)

#define PARAM_FLOAT(ITEM, name, desc, defVal) \
    ITEM(name, desc, defVal, Float, fl, paramValuesFloat)

#define PARAM_NUM(ITEM, name, desc, defVal) \
    ITEM(name, desc, defVal, Num, n, GetParamValues##name())

#define PARAM_KEY(ITEM, name, desc, defVal) \
    ITEM(name, desc, defVal, Num, n, paramValuesKey)

#define PARAM_GATESOURCE(ITEM, name, desc, defVal) \
    ITEM(name, desc, HW::CVIn::defVal, Num, n, paramValuesGateSource)

#define PARAM_CVSOURCE(ITEM, name, desc, defVal) \
    ITEM(name, desc, HW::CVIn::defVal, CVSource, n, paramValuesCVSource)

#define DECL_PROG_PARAMS \
    private: \
        PROG_PARAMS(DECL_PARAM_VAR) \
    public: \
        constexpr std::span<const ParamDesc> GetParams() const override { \
            static constexpr ParamDesc params[] = { PROG_PARAMS(DECL_PARAM_DESC) }; \
            return {std::begin(params), std::size(params)}; \
        } \
        PROG_PARAMS(DECL_PARAM_GET_SET)

#define DECL_PARAM_VALUE_STRING(name, desc, ...) desc##sv,

#define DECL_PARAM_VALUE_ENUM(name, desc, ...) name,

#define DECL_PARAM_VALUES(name) \
    private: static constexpr std::array paramValues##name = { \
        PARAM_VALUES(DECL_PARAM_VALUE_STRING) \
    }; \
    enum class name { PARAM_VALUES(DECL_PARAM_VALUE_ENUM) }; \
    static consteval std::span<const std::string_view> GetParamValues##name() { \
        return {paramValues##name}; \
    }
