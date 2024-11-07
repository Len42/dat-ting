#pragma once

class Graphics
{
public:
    // Keyboard-drawing functions
    // TODO: better place to put these?

    static void DrawKeyboard(uint8_t left, uint8_t top)
    {
        for (auto&& rect : keyboardRects) {
            rect.draw(HW::display, left, top, false);
        }
        for (auto&& line : keyboardLines) {
            line.drawLine(HW::display, left, top);
        }
    }

    static void FillKey(unsigned n, uint8_t left, uint8_t top)
    {
        keyRects[n].drawKey(HW::display, left, top);
    }

    static void HighlightKey(unsigned n, uint8_t left, uint8_t top)
    {
        Rect r;
        switch (n) {
        case 1: case 3: case 6: case 8: case 10:
            // black keys
            r = keyRects[n].rect1;
            r.b = r.t + 3;
            break;
        default:
            // white keys
            r = keyRects[n].rect2;
            r.t = r.b - 3;
            break;
        }
        r.draw(HW::display, left, top, true);
    }

protected:
    struct Rect {
        uint8_t l, t, r, b;
        constexpr bool isEmpty() const { return (l >= r) || (t >= b); }
        void draw(auto& display, uint8_t left, uint8_t top, bool fill) const {
            if (!isEmpty()) display.DrawRect(left+l, top+t, left+r, top+b, true, fill);
        }
        void drawLine(auto& display, uint8_t left, uint8_t top) const {
            display.DrawLine(left+l, top+t, left+r, top+b, true);
        }
    };

    static constexpr Rect keyboardRects[] = {
        { 0, 0, 63, 26 },   { 5, 0, 11, 15 },   { 16, 0, 22, 15 },
        { 32, 0, 38, 15 },  { 42, 0, 48, 15 },  { 52, 0, 58, 15 }
    };

    static constexpr Rect keyboardLines[] = {
        { 9, 16, 9, 25 },   { 18, 16, 18, 25 }, { 27, 1, 27, 25 },
        { 36, 16, 36, 25 }, { 45, 16, 45, 25 }, { 54, 16, 54, 25 },
    };

    struct KeyRects {
        Rect rect1;
        Rect rect2;
        void drawKey(auto& display, uint8_t left, uint8_t top) const {
            rect1.draw(display, left, top, true);
            rect2.draw(display, left, top, true);
        }
    };

    static constexpr KeyRects keyRects[12] = {
        { { 1, 1, 4, 15 }, { 1, 16, 8, 25 } },
            { { 6, 1, 10, 14 }, { 0 } },
        { { 12, 1, 15, 15 }, { 10, 16, 17, 25 } },
            { { 17, 1, 21, 14 }, { 0 } },
        { { 23, 1, 26, 15 }, { 19, 16, 26, 25 } },
        { { 28, 1, 31, 15 }, { 28, 16, 35, 25 } },
            { { 33, 1, 37, 14 }, { 0 } },
        { { 39, 1, 41, 15 }, { 37, 16, 44, 25 } },
            { { 43, 1, 47, 14 }, { 0 } },
        { { 47, 1, 51, 15} , { 46, 16, 53, 25 } },
            { { 53, 1, 57, 14 }, { 0 } },
        { { 59, 1, 62, 15 }, { 55, 16, 62, 25 } },
    };
};
