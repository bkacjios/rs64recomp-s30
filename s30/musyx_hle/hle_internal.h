/* Minimal stand-in for mupen64plus-rsp-hle's hle_internal.h: the MusyX task only needs RDRAM, DMEM and rsp_break. */
#ifndef HLE_INTERNAL_H
#define HLE_INTERNAL_H

#include <stdint.h>

struct hle_t
{
    unsigned char* dram;
    unsigned char* dmem;
    void* user_defined;
};

#define SP_STATUS_TASKDONE 0x200

void rsp_break(struct hle_t* hle, unsigned int setbits);

#endif
