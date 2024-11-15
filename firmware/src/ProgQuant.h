#pragma once

/// @brief Quantizer program
/// @details This is a CV processor that quantizes the pitch CV on input CV1 to
/// a given scale and outputs it on output CV1.
/// @todo Moar scales!
class ProgQuant : public Program
{
    using this_t = ProgQuant;

    // Declare the configurable parameters of this program
    #define PARAM_VALUES(ITEM) \
        ITEM(None, "Untouched") \
        ITEM(Chromatic, "Chromatic") \
        ITEM(Major, "Major / Ionian") \
        ITEM(Minor, "Minor / Aeolian")
        // etc.
    DECL_PARAM_VALUES(Scale)
    #undef PARAM_VALUES
    #define PROG_PARAMS(ITEM) \
        PARAM_NUM(ITEM, Scale, "Scale", unsigned(Scale::Major)) \
        PARAM_KEY(ITEM, Key, "Key", 0)
    DECL_PROG_PARAMS
    #undef PROG_PARAMS

public:
    constexpr std::string_view GetName() const override { return "Quantize"sv; }

    void Init() override
    {
        noteSaved = -1;
        animation.SetScale(Scale(GetScale()), GetKey());
        animation.SetNote(69);
    }

    void Process(ProcessArgs& args) override
    {
        float note = HW::CVIn::GetNote(HW::CVIn::CV1);
        // Only quantize if the current pitch is sufficiently different from the
        // previous one, to reduce "flickering" between adjacent notes.
        if (IsDifferent(note, noteSaved)) {
            noteSaved = note;
            note = Quantize(note);
            HW::CVOut::SetNote(HW::CVOut::Channel::ONE, note);
            animation.SetNote(note);
        }
        animation.SetScale(Scale(GetScale()), GetKey());
    }

    Animation* GetAnimation() const override { return &animation; }

protected:
    static constexpr unsigned numSemis = 12;    ///< Number of semitones in an octave

    /// @brief Bit flags for the notes in an octave
    enum class NoteFlags : uint16_t {
        C = 0x0001,
        Cx = 0x0002, Db = 0x0002,
        D = 0x0004,
        Dx = 0x0008, Eb = 0x0008,
        E = 0x0010,
        F = 0x0020,
        Fx = 0x0040, Gb = 0x0040,
        G = 0x0080,
        Gx = 0x0100, Ab = 0x0100,
        A = 0x0200,
        Ax = 0x0400, Bb = 0x0400,
        B = 0x0800,
        none = 0
    };

    /// @brief A scale is a bit-set of @NoteFlags in a word specifying which
    /// notes are in the scale
    using ScaleNotes = NoteFlags;

    /// @brief A scale with no notes!
    static constexpr ScaleNotes scaleEmpty = ScaleNotes::none;

    /// Macro: Convenient abbreviation for the following declarations
    #define N_(note) uint16_t(NoteFlags::note)

    /// @brief The chromatic scale is all the notes in the octave
    static constexpr ScaleNotes scaleChromatic = ScaleNotes { 0x0FFF };

    /// @brief A major scale in the key of C
    static constexpr ScaleNotes scaleMajor = ScaleNotes {
        N_(C) | N_(D) | N_(E) | N_(F) | N_(G) | N_(A) | N_(B)
    };

    /// @brief A natural minor scale in the key of C
    static constexpr ScaleNotes scaleMinor = ScaleNotes {
        N_(C) | N_(D) | N_(Eb) | N_(F) | N_(G) | N_(Ab) | N_(Bb)
    };

    #undef N_

    float noteSaved = -1; ///< The last note that was quantized

    /// @brief Are two notes different?
    /// @details The notes must be sufficiently different to prevent "flickering"
    /// due to CV noise.
    /// @param note1 
    /// @param note2 
    /// @return 
    static constexpr bool IsDifferent(float note1, float note2)
    {
        static constexpr float minDiff = 0.2f;
        return isDifferent(note1, note2, minDiff);
    }

    /// @brief Is the given note in the given scale?
    /// @param note A MIDI note number (integral value)
    /// @param scale A scale, as a set of notes
    /// @return true if the note is in the scale, false otherwise
    static constexpr bool IsInScale(unsigned note, ScaleNotes scale)
    {
        return bool((1 << (note % numSemis)) & uint16_t(scale));
    }

    /// @brief Transpose a scale to a different key
    /// @details The given scale must be one that can be transposed, i.e. not
    /// empty or chromatic.
    /// @param scale A scale, as a set of notes
    /// @param key A key given as a semitone number from 0 (C) to 11 (B)
    /// @return The scale transposed to the given key
    static constexpr ScaleNotes TransposeScale(ScaleNotes scale, unsigned key)
    {
        // scale is in C. Transpose up to key (where key == 0 for C, 1 for C#, etc.)
        uint32_t scaleU = uint32_t(scale);
        uint32_t scaleDouble = (scaleU | (scaleU << numSemis));
        scaleU = ((scaleDouble >> (numSemis - key)) & uint16_t(scaleChromatic));
        return ScaleNotes(scaleU);
    }

    /// @brief Return a scale of the given type in the given key
    /// @details The key will be ignored if it's not relevant (e.g. chromatic scale)
    /// @param scale A scale, as a @ref Scale parameter value
    /// @param key A key given as a semitone number from 0 (C) to 11 (B)
    /// @return The scale as a @ref ScaleNotes, transposed to the given key if necessary
    static constexpr ScaleNotes NotesForScale(Scale scale, unsigned key)
    {
        ScaleNotes notes;
        switch (scale) {
        case Scale::None:       notes = scaleEmpty;     break;
        case Scale::Chromatic:  notes = scaleChromatic; break;
        case Scale::Major:      notes = scaleMajor;     break;
        case Scale::Minor:      notes = scaleMinor;     break;
        default:                notes = scaleEmpty;     break;
        }
        notes = TransposeScale(notes, key);
        return notes;
    }

    /// @brief Quantize a note by adjusting its pitch so that it is in tune with
    /// the currently-selected scale
    /// @param note A MIDI note number (may be fractional)
    /// @return A MIDI note number that is in the current scale
    float Quantize(float note)
    {
        Scale scale = Scale(GetScale());
        switch (scale) {
        case Scale::None:
            note = note; // There.
            break;
        case Scale::Chromatic:
            note = QuantizeSemitone(note);
            break;
        case Scale::Major:
        case Scale::Minor:
        // TODO: MORE
            note = QuantizeScale(note, NotesForScale(scale, GetKey()));
            break;
        }
        return note;
    }

    /// @brief Quantize a note so it is in the chromatic scale
    /// @details This could be done by QuantizeScale but this way is simpler
    /// and faster.
    /// @param note A MIDI note number (may be fractional)
    /// @return A MIDI note number that is in the scale
    static float QuantizeSemitone(float note) { return std::round(note); }

    /// @brief Quantize a note so it is in the given scale
    /// @param note A MIDI note number (may be fractional)
    /// @param scale A scale, as a set of notes
    /// @return A MIDI note number that is in the scale
    static float QuantizeScale(float note, ScaleNotes scale)
    {
        note = std::max(note, 0.f); // min supported MIDI note number == 0
        unsigned noteU = unsigned(std::round(note));
        // Make sure the scale contains any notes, otherwise loops won't terminate
        if (scale == scaleEmpty) {
            // arbitrary return value for empty scale
            return float(noteU);
        } else {
            unsigned noteHi;
            for (noteHi = noteU; !IsInScale(noteHi, scale); ++noteHi)
                ;
            float diffHi = noteHi - note;
            unsigned noteLo = 0;
            float diffLo;
            if (noteU == 0) {
                // this case isn't handled by the loop below
                diffLo = float(std::numeric_limits<unsigned>::max());
            } else {
                noteLo = noteU;
                do {
                    --noteLo;
                } while (!IsInScale(noteLo, scale) && noteLo != 0);
                diffLo = note - noteLo;
            }
            return float((diffHi < diffLo) ? noteHi : noteLo);
        }
    }

    /// @brief @ref Animation for @ref ProgQuant
    /// @details Displays a keyboard with the notes in the currently-selected
    /// scale highlighted and the currently-playing note filled in.
    class ProgAnimation : public Animation
    {
    public:
        ProgAnimation() { }

        void Init() override { }

        bool Step(unsigned step) override
        {
            // Display the current scale and output note
            static constexpr unsigned posX = 32;
            static constexpr unsigned posY = 2;
            HW::display.Fill(false);
            Graphics::DrawKeyboard(posX, posY);
            DrawScaleHighlights(posX, posY);
            Graphics::FillKey(unsigned(std::round(noteOut)) % numSemis, posX, posY);
            HW::display.Update();

            // never stop
            return true;
        }

        void SetScale(Scale scale, unsigned key)
        {
            currentScale = scale;
            currentKey = key;
        }

        void SetNote(float note) { noteOut = note; }

    protected:
        void DrawScaleHighlights(uint8_t left, uint8_t top)
        {
            ScaleNotes scaleNotes = NotesForScale(currentScale, currentKey);
            for (unsigned semi = 0; semi < numSemis; ++semi) {
                if (IsInScale(semi, scaleNotes)) {
                    Graphics::HighlightKey(semi, left, top);
                }
            }
        }

    protected:
        Scale currentScale = Scale::None;

        unsigned currentKey = 0;

        float noteOut = 0;
    };

    static inline ProgAnimation animation;
};
