#pragma once

#include "dev/oled_ssd130x.h"

namespace daisy2 {

/// @brief A version of @ref daisy::SSD130xDriver with bug fixes and added features
/// @tparam Transport 
/// @tparam width 
/// @tparam height 
template <size_t width, size_t height, typename Transport>
class FixedSSD1306Driver : public daisy::SSD130xDriver<width, height, Transport>
{
    using BASE = daisy::SSD130xDriver<width, height, Transport>;
    using BASE::transport_;
    using BASE::buffer_;

public:
    /// @brief Fill the display with random "static", the color of television
    /// tuned to a dead channel
    /// @param on 
    void FillStatic(bool on)
    {
        // KLUDGE: sneaky conversion from byte buffer to word buffer to make
        // random static generation more efficient
        unsigned sizeB = std::size(buffer_);
        unsigned sizeW = sizeB / sizeof(uint32_t);
        std::span<uint32_t> bufW(reinterpret_cast<uint32_t*>(buffer_), sizeW);
        // Fill the screen with random static
        static std::ranlux24_base randEngine;
        std::uniform_int_distribution<uint32_t> uniform_dist(0);
        for (auto&& w : bufW) {
            auto rand = uniform_dist(randEngine);
            if (on) {
                // Set the random pixels on the display
                w = rand;
            } else {
                // Clear display pixels where rand bits are 1
                w &= rand;
            }
        }
    }

    /// @brief Update the display
    /// @note Fixed version of @ref daisy::SSD130xDriver::Update
    void Update()
    {
        uint8_t i;
        uint8_t high_column_addr;
        switch(height)
        {
            // lmp: FIX: Removed this line. It's wrong, at least on my SSD1306.
            //case 32: high_column_addr = 0x12; break;

            default: high_column_addr = 0x10; break;
        }
        for(i = 0; i < (height / 8); i++)
        {
            transport_.SendCommand(0xB0 + i);
            transport_.SendCommand(0x00);
            transport_.SendCommand(high_column_addr);
            transport_.SendData(&buffer_[width * i], width);
        }
    }

    /// @brief Return the pixel buffer as a range of bytes
    /// @return 
    std::span<uint8_t> GetBuffer() { return std::span(buffer_); }

    /// @brief Return the size of the pixel buffer
    /// @return 
    constexpr unsigned GetBufSize() { return std::size(buffer_); }

    /// @brief Save a copy of the display pixel buffer
    /// @param buf Saved pixel buffer
    void SaveBuf(std::ranges::range auto& buf)
    {
        if (std::size(buf) != GetBufSize()) {
            DebugLog::PrintLine("ERROR: SaveBuf: incorrect buffer size");
        } else {
            std::ranges::copy(GetBuffer(), std::begin(buf));
        }
    }

    /// @brief Restore a saved copy of the pixel buffer to the display
    /// @param buf Saved pixel buffer
    void RestoreBuf(std::ranges::range auto& buf)
    {
        if (std::size(buf) != GetBufSize()) {
            DebugLog::PrintLine("ERROR: RestoreBuf: incorrect buffer size");
        } else {
            std::ranges::copy(buf, std::begin(GetBuffer()));
        }
    }

    /// @brief Combine a saved pixel buffer with the display buffer using bitwise or
    /// @param buf 
    void MergeBuf(std::ranges::range auto& buf)
    {
        if (std::size(buf) != GetBufSize()) {
            DebugLog::PrintLine("ERROR: MergeBuf: incorrect buffer size");
        } else {
            std::ranges::transform(buf, GetBuffer(), std::begin(GetBuffer()),
                [](auto&& b1, auto&& b2) { return b1 | b2; });
        }
    }
};

/// @brief Specialized driver for 128x32 SPI SSD1306 OLED display
using FixedSSD13064WireSpi128x32Driver
    = FixedSSD1306Driver<128, 32, daisy::SSD130x4WireSpiTransport>;

}
