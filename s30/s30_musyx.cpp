// MusyX audio tasks through mupen64plus-rsp-hle's C version of the microcode (musyx_hle/).
// The RSPRecomp'd synth emulates the RSP's vector unit and takes ~80% of a Cortex-A7 core, halving the frame rate.
// S30_MUSYX_RECOMP=1 runs the recompiled synth instead (audio.cpp), for comparison.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audio/audio.h"
#include "librecomp/rsp.hpp"

extern "C" {
#include "hle_internal.h"
#include "hle_external.h"
void musyx_v1_task(struct hle_t* hle);
}

namespace {

// The task fields musyx_v1_task reads, as the RSP sees them at DMEM 0xFC0 (native u32 words).
alignas(4) unsigned char s_dmem[0x1000];

}

extern "C" void rsp_break(struct hle_t*, unsigned int) {}
extern "C" void HleVerboseMessage(void*, const char*, ...) {}
extern "C" void HleWarnMessage(void*, const char* message, ...)
{
    fprintf(stderr, "[musyx-hle] warning: %s\n", message);
}

static RspExitReason musyx_hle_runner(uint8_t* rdram, uint32_t)
{
    struct hle_t hle{};

    hle.dram = rdram;
    hle.dmem = s_dmem;
    musyx_v1_task(&hle);
    return RspExitReason::Broke;
}

RspUcodeFunc* s30_get_rsp_microcode(const OSTask* task)
{
    static const bool s_recomp = getenv("S30_MUSYX_RECOMP") != nullptr;

    if (s_recomp || (task->t.type != M_AUDTASK))
    {
        return rs64::audio::get_rsp_microcode(task);
    }

    static bool s_checked = false;

    if (!s_checked) {
        s_checked = true;
        fprintf(stderr, "[musyx-hle] audio tasks through the MusyX v1 HLE (ucode_data=0x%08X)\n", (uint32_t)task->t.ucode_data);
    }

    uint32_t data_ptr = (uint32_t)task->t.data_ptr;
    uint32_t data_size = (uint32_t)task->t.data_size;

    memcpy(s_dmem + 0xFF0, &data_ptr, 4);
    memcpy(s_dmem + 0xFF4, &data_size, 4);
    return &musyx_hle_runner;
}
