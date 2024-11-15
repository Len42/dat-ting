#pragma once

#include "Program.h"
#include "ProgVarOsc.h"
#include "ProgAutoPan.h"
#include "ProgSynthDrums.h"
#include "ProgDelay.h"
#include "ProgReverb.h"
#include "ProgBitcrush.h"
#include "ProgQuant.h"

/// @brief @ref Program runner
/// @details Contains a list of the available programs
/// @tparam ...PROGS 
template<typename... PROGS>
class ProgramListBase
{
public:
    /// @brief Compile-time constructor that initializes the list of programs
    consteval ProgramListBase()
    {
        int i = 0;
        ((progs[i++] = &progInstance<PROGS>), ...);
    }

    /// @brief Return the list of programs as a range
    /// @return 
    std::span<Program*> GetList()
    {
        return std::span<Program*>(std::begin(progs), std::size(progs));
    }

    /// @brief Run a program
    /// @details Initializes the @ref Program and sets it as the currently-running program.
    /// @param prog 
    static void RunProgram(Program* prog)
    {
        currentProgram = nullptr;
        if (prog) {
            prog->Init();
        }
        currentProgram = prog;
    }

    /// @brief Return the currently-running program
    /// @return 
    static Program* GetCurrentProgram() { return currentProgram; }

    // DEBUG
    static unsigned GetResetSampleCount() { return sampleCount.exchange(0); }

    /// @brief Audio processing callback that calls the current @ref Program
    /// @param inbuf Audio input buffer
    /// @param outbuf Audio output buffer
    static void ProcessingCallback(daisy2::AudioInBuf inbuf, daisy2::AudioOutBuf outbuf)
    {
        // Update the gate inputs at the sample rate
        // TODO: Use the analog watchdog feature to make gates interrupt-driven
        // like switches are
        HW::CVIn::UpdateGates();

        // Call the current program's Process function
        if (currentProgram) {
            ProcessArgs args = Program::MakeProcessArgs(inbuf, outbuf);
            currentProgram->Process(args);
            /*DEBUG*/sampleCount += std::size(outbuf);
        }
    }

protected:
    /// @brief List of all available programs
    Program* progs[sizeof...(PROGS)];

    /// @brief Current running program
    static inline Program* currentProgram = nullptr;

    /// @brief Template for a static instance of each @ref Program type
    /// @tparam PROG_T 
    template<typename PROG_T>
    static inline PROG_T progInstance;

    // DEBUG
    static inline std::atomic<unsigned> sampleCount = 0;
};

/// @brief List of available programs
using ProgramList = ProgramListBase<
    ProgVariableOsc
    ,ProgSynthDrums
    ,ProgAutoPan
    ,ProgDelay
    ,ProgReverb
    ,ProgBitcrush
    ,ProgQuant
>;

/// @brief @ref Program runner
static constinit ProgramList programs;
