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
    using value_type = VALUE_T;
    using size_type = size_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = pointer;
    using const_iterator = const_pointer;

    static constexpr size_type numValues = NUM_VALUES;

    consteval DataTable()
    {
        for (auto&& [index, value] : std::views::enumerate(data)) {
            value = FUNC_CALC1(index, numValues);
        }
    }

    constexpr size_t size() const noexcept { return numValues; }

    constexpr auto& getArray() noexcept { return data; }

    constexpr const auto& getArray() const noexcept { return data; }

    constexpr value_type& operator[](size_t index) noexcept { return data[index]; }

    constexpr value_type operator[](size_t index) const noexcept { return data[index]; }

    constexpr reference front() noexcept { return data[0]; }

    constexpr const_reference front() const noexcept { return data[0]; }

    constexpr reference back() noexcept { return data[numValues-1]; }

    constexpr const_reference back() const noexcept { return data[numValues-1]; }

    constexpr iterator begin() noexcept { return std::begin(data); }
    
    constexpr iterator end() noexcept { return std::end(data); }

    constexpr const_iterator begin() const noexcept { return std::begin(data); }

    constexpr const_iterator end() const noexcept { return std::end(data); }

    constexpr iterator cbegin() const noexcept { return begin(); }

    constexpr iterator cend() const noexcept { return end(); }

private:
    value_type data[numValues];
};
