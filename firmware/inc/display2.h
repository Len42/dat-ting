#pragma once

namespace daisy2 {

/// @brief Subclass of OneBitGraphicsDisplayImpl with features added
/// @tparam ChildType 
template <class ChildType>
class OneBitGraphicsDisplayImpl2 : public daisy::OneBitGraphicsDisplayImpl<ChildType>
{
    using Base = daisy::OneBitGraphicsDisplayImpl<ChildType>;

public:
    // Overloads of text display functions that use the current font and take
    // string_view instead of char*

    /// @brief Set a current font to be used henceforth instead of specifying
    /// a font in every call
    /// @param font 
    void SetFont(const FontDef& font) { fontCurrent = &font; }

    /// @brief Return the currently-selected font
    /// @return 
    const FontDef* GetFont() { return fontCurrent; }

    /// @brief Overload of @ref WriteChar that uses the current font
    /// @param ch 
    /// @param on 
    /// @return 
    char WriteChar(char ch, bool on) {
        return WriteChar(ch, *fontCurrent, on);
    }

    /// @brief Overload of @ref WriteString that takes a string_view and
    /// uses the current font
    /// @param str 
    /// @param on 
    /// @return 
    char WriteString(std::string_view str, bool on)
    {
        for (char ch : str) {
            if (((ChildType*)(this))->ChildType::WriteChar(ch, on) != ch) {
                // Char could not be written
                return ch;
            }
        }
        return 0;
    }

    /// @brief Overload of @ref WriteStringAligned that takes a string_view and
    /// uses the current font
    /// @param str 
    /// @param boundingBox 
    /// @param alignment 
    /// @param on 
    /// @return 
    daisy::Rectangle WriteStringAligned(std::string_view str,
                                        daisy::Rectangle boundingBox,
                                        daisy::Alignment alignment,
                                        bool on)
    {
        const auto alignedRect
            = GetTextRect(str, *fontCurrent).AlignedWithin(boundingBox, alignment);
        SetCursor(alignedRect.GetX(), alignedRect.GetY());
        ((ChildType*)(this))->ChildType::WriteString(str, on);
        return alignedRect;
    }

    // Must re-define hidden functions in the base class

    char WriteChar(char ch, FontDef font, bool on) override {
        return Base::WriteChar(ch, font, on);
    }

    char WriteString(const char* str, FontDef font, bool on) override {
        return Base::WriteString(str, font, on);
    }

    daisy::Rectangle WriteStringAligned(const char* str,
                                        const FontDef& font,
                                        daisy::Rectangle boundingBox,
                                        daisy::Alignment alignment,
                                        bool on) override
    {
        return Base::WriteStringAligned(str, font, boundingBox, alignment, on);
    }

protected:
    // re-implementation of a function declared private in base class
    daisy::Rectangle GetTextRect(const char* text, const FontDef& font)
    {
        const auto numChars = strlen(text);
        return {int16_t(numChars * font.FontWidth), font.FontHeight};
    }

protected:
    const FontDef* fontCurrent = &Font_11x18; ///< Font to use for text display
};

} // namespace daisy2
