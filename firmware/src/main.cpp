#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
using namespace std::string_view_literals;
#include <utility>

#include "version.h"
#pragma message "Building " PROG_NAME " version " BUILD_VERSION
#include "sysutils.h"

#include "daisy_seed2.h"
#include "daisysp.h"
#include "tasks.h"
#include "ringbuf.h"
#include "datatable.h"
#include "lookup.h"

// Set the type of hardware being used.
enum class HWType { Prototype, Module };
#ifndef HW_TYPE
#define HW_TYPE Prototype
#endif
#pragma message "Building for " sym_to_string(HW_TYPE) " hardware"

#include "PinDefs.h"
#include "CVIn.h"
#include "CVOut.h"

#include "Hardware.h"

#include "Graphics.h"
#include "Animation.h"
#include "Program.h"
#include "ProgList.h"
#include "MiscTasks.h"
#include "UITask.h"

/// @brief The list of tasks to execute
/// @remarks Excludes the AudioCallback and related timing-critical tasks
static constexpr tasks::TaskList<
    AnimationTask
    ,UIImpl::UIBase<ProgramList, programs>::Task
    //,BlinkTask
    //,ButtonLedTask
    //,GateLedTask
    //,AdcOutputTask
    //,AdcCalibrateTask
    //,SampleRateTask
    //,ProgReverb::DebugTask
> taskList;

int main()
{
    // Initialize the hardware
    HW::Init();

    ProgramList::StartProcessing();
    // TODO: Get the previously-running program from saved settings
    programs.RunProgram(programs.GetList().front());

    // Run all tasks, forever.
    taskList.initAll();
    for (;;) {
        taskList.runAll();
    }

    return 0;
}
