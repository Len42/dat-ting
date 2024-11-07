#pragma once

#include <cmath>
#include <utility>

#define sym_to_string_helper(X) #X
#define sym_to_string(X) sym_to_string_helper(X)

/// @brief Just an alias for std::exchange because no-one can remember what it
/// does from its name
#define getAndSet std::exchange

template<typename T>
constexpr bool isDifferent(T v1, T v2, T minDiff)
{
    return std::abs(v1 - v2) > minDiff;
}

template<>
constexpr bool isDifferent(unsigned v1, unsigned v2, unsigned minDiff)
{
    unsigned diff = (v1 > v2) ? v1 - v2 : v2 - v1;
    return diff > minDiff;
}

/// @brief Split a floating-point number into whole-integer and fraction parts
/// @details The integer part is signed. The fraction is unsigned, implicitly
/// the same sign as the integer part. (This makes printing simpler.)
/// @note Return value is incorrect if the integer part doesn't fit into an int.
/// @param x 
/// @return 
constexpr std::pair<int, unsigned> splitFloat(float x, unsigned fracDigits)
{
    float flInt = 0;
    float flFrac = 0;
    flFrac = modf(x, &flInt);
    for (unsigned i = 0; i < fracDigits; ++i) {
        flFrac *= 10.f;
    }
    return { int(flInt), unsigned(std::abs(flFrac)) };
}

/// @brief Rescale a number linearly from one range to another
/// @details Uses float arithmetic. Arguments may be integer or floating point.
/// @tparam T 
/// @param in 
/// @param minIn 
/// @param maxIn 
/// @param minOut 
/// @param maxOut 
/// @return 
template<typename T>
T rescale(T in, T minIn, T maxIn, T minOut, T maxOut)
{
    float factor = (maxOut - minOut) / (maxIn - minIn);
    T out = T(minOut + (in - minIn) * factor);
    out = std::clamp(out, minOut, maxOut);
    return out;
}