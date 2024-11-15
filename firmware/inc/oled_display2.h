#pragma once

namespace daisy2 {

/// @brief Support for a monochrome OLED display
/// @details This is a re-implementation of daisy::OledDisplay with features added.
/// It's done by copying the whole class, not subclassing, due to member access issues.
/// The member functions simply call the corresponding functions in the DisplayDriver.
/// @tparam DisplayDriver Display driver class
/// @remarks It's inconsistent that some functions are virtual and some (mine) are not.
/// It would be quite messy to change that since the base class is a couple levels up.
template <typename DisplayDriver>
class OledDisplay2 : public OneBitGraphicsDisplayImpl2<OledDisplay2<DisplayDriver>>
{
    using Base = OneBitGraphicsDisplayImpl2<OledDisplay2<DisplayDriver>>;

public:
    virtual ~OledDisplay2() {}

    using Config = DisplayDriver::Config;

    void Init(Config config) { driver_.Init(config); }

    uint16_t Height() const override { return driver_.Height(); }

    uint16_t Width() const override { return driver_.Width(); }

    std::span<uint8_t> GetBuffer() { return driver_.GetBuffer(); }

    constexpr unsigned GetBufSize() { return driver_.GetBufSize(); }

    void SaveBuf(std::ranges::range auto& buf) { return driver_.SaveBuf(buf); }

    void RestoreBuf(std::ranges::range auto& buf) { return driver_.RestoreBuf(buf); }

    void MergeBuf(std::ranges::range auto& buf) { return driver_.MergeBuf(buf); }

    void Fill(bool on) override { driver_.Fill(on); }

    void FillStatic(bool on) { driver_.FillStatic(on); }

    void DrawPixel(uint_fast8_t x, uint_fast8_t y, bool on) override {
        driver_.DrawPixel(x, y, on);
    }

    void Update() override { driver_.Update(); }

protected:
    void Reset() { driver_.Reset(); };

    void SendCommand(uint8_t cmd) { driver_.SendCommand(cmd); };

    void SendData(uint8_t* buff, size_t size) { driver_.SendData(buff, size); };

protected:
    DisplayDriver driver_;
};

} // namespace daisy2
