#pragma once

#include "datatable.h"

template<typename VALUE_T, unsigned BITS_IN, unsigned BITS_TABLE,
         VALUE_T FUNC_CALC1(size_t index, size_t numValues)>
class LookupTable
{
public:
    using value_t = VALUE_T;
    static constexpr unsigned nBitsIn = BITS_IN;
    static constexpr unsigned nBitsTable = BITS_TABLE;
    static_assert(nBitsIn >= nBitsTable);
    static_assert(nBitsIn - nBitsTable >= 3); // else interpolation code below is wrong
    static constexpr unsigned nBitsShift = nBitsIn - nBitsTable;
    static constexpr size_t tableSize = (1 << nBitsTable);

    static VALUE_T lookupInterpolate(unsigned n)
    {
        unsigned index = (n >> nBitsShift) % tableSize; // TODO: check this is optimized
        // Find the nearest pair of values in the lookup table
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
    // One extra table entry to help with interpolation
    static constexpr DataTable<VALUE_T, tableSize+1, FUNC_CALC1> lookupTable =
        DataTable<VALUE_T, tableSize+1, FUNC_CALC1>();
};
