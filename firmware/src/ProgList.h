#pragma once

#include "Program.h"
#include "ProgVarOsc.h"
#include "ProgAutoPan.h"
#include "ProgSynthDrums.h"
#include "ProgDelay.h"
#include "ProgReverb.h"
#include "ProgBitcrush.h"
#include "ProgQuant.h"

template<typename... PROGS>
class ProgramListBase
{
public:
    consteval ProgramListBase()
    {
        int i = 0;
        ((progs[i++] = &progInstance<PROGS>), ...);
    }

    std::span<Program*> GetList()
    {
        return std::span<Program*>(std::begin(progs), std::size(progs));
    }

    static void RunProgram(Program* prog)
    {
        currentProgram = nullptr;
        prog->Init();
        currentProgram = prog;
    }

    static Program* GetCurrentProgram() { return currentProgram; }

    static void StartProcessing()
    {
        static_assert(HW::audioBlockSize > 0);
        HW::seed.SetAudioBlockSize(HW::audioBlockSize);
        HW::seed.StartAudio(ProcessingCallback);
    }

    static void StopProcessing() { HW::seed.StopAudio(); }

    // DEBUG
    static unsigned GetResetSampleCount() { return sampleCount.exchange(0); }

protected:
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
    Program* progs[sizeof...(PROGS)];

    static inline Program* currentProgram = nullptr;

    template<typename PROG_T>
    static inline PROG_T progInstance;

    // DEBUG
    static inline std::atomic<unsigned> sampleCount = 0;
};

using ProgramList = ProgramListBase<
    ProgVariableOsc
    ,ProgSynthDrums
    ,ProgAutoPan
    ,ProgDelay
    ,ProgReverb
    ,ProgBitcrush
    ,ProgQuant
>;

static constinit ProgramList programs;
