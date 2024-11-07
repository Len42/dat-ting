#pragma once

#include <ranges>

/// @brief Class template for static tables of pre-calculated data
/// @tparam VALUE_T Type of table entries
/// @tparam NUM_VALUES Number of table entries
/// @tparam FUNC_CALC1 Function or lambda to calculate a single table entry
///
/// The data table is calculated and initialized at compile time.
/// The table contains NUM_VALUES values of type VALUE_T.
/// Each value is calculated by FUNC_CALC1 which has this signature:
/// @code
/// VALUE_T FUNC_CALC1(size_t index, size_t numValues)
/// @endcode
/// Acknowledgements
/// ----------------
/// Ashley Roll - https://github.com/AshleyRoll/cppcon21/blob/main/code/table_gen_1.cpp
///
/// Jason Turner - https://tinyurl.com/constexpr2021
template<typename VALUE_T,
         size_t NUM_VALUES,
         VALUE_T FUNC_CALC1(size_t index, size_t numValues)>
class DataTable
{
public:
    using value_t = VALUE_T;
    static constexpr size_t numValues = NUM_VALUES;

    consteval DataTable()
    {
        for (auto&& [index, value] : std::views::enumerate(data)) {
            value = FUNC_CALC1(index, numValues);
        }
    }

    constexpr size_t size() const { return numValues; }

    constexpr auto& getArray() { return data; }

    constexpr const auto& getArray() const { return data; }

    constexpr value_t& operator[](size_t index) { return data[index]; }

    constexpr value_t operator[](size_t index) const { return data[index]; }

private:
    value_t data[numValues];
};
