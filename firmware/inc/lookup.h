#pragma once

#include "datatable.h"

/// @brief Table lookup and interpolation
/// @details Multiple LookupTable classes can be defined, each using a different
/// lookup table. The lookup table is defined by a static DataTable object.
/// The size of the table and the size of the input value range are powers of 2.
/// @tparam VALUE_T Type of values in the wavetable
/// @tparam BITS_IN Number of bits for unsigned input values
/// @tparam BITS_TABLE Number of bits for the table size
/// @tparam FUNC_CALC_1 Function/lambda to calculate one table entry
template<typename VALUE_T, unsigned BITS_IN, unsigned BITS_TABLE,
         VALUE_T FUNC_CALC_1(size_t index, size_t numValues)>
class LookupTable
{
public:
    using value_t = VALUE_T;
    static constexpr unsigned nBitsIn = BITS_IN;
    static constexpr unsigned nBitsTable = BITS_TABLE;
    static_assert(nBitsIn >= nBitsTable);
    static_assert(nBitsIn - nBitsTable >= 3); // else the interpolation code is wrong
    static constexpr unsigned nBitsShift = nBitsIn - nBitsTable;
    static constexpr size_t tableSize = (1 << nBitsTable);

    /// @brief Return the interpolated output value for the given input value
    /// @param n 
    /// @return 
    static VALUE_T lookupInterpolate(unsigned n)
    {
        // Find the nearest pair of values in the lookup table
        unsigned index = (n >> nBitsShift) % tableSize;
        VALUE_T entry0 = lookupTable[index];
        VALUE_T entry1 = lookupTable[index+1];
        // Interpolate between the two table values using the next 3 bits of n
        VALUE_T value = entry0;
        value = VALUE_T((value +
            ((n & (0x1 << (nBitsShift - 3))) ? entry1 : entry0)) / 2);
        value = VALUE_T((value +
            ((n & (0x1 << (nBitsShift - 2))) ? entry1 : entry0)) / 2);
        value = VALUE_T((value +
            ((n & (0x1 << (nBitsShift - 1))) ? entry1 : entry0)) / 2);
        return value;
    }

private:
    /// @brief @ref DataTable type containing calculated values
    /// @details The DataTable has one extra entry to help with interpolation.
    using table_t = DataTable<VALUE_T, tableSize+1, FUNC_CALC_1>;

    /// @brief Table containing calculated values
    static constexpr table_t lookupTable = table_t();
};
