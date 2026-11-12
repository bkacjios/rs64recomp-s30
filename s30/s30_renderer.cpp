// S30 renderer context: RT64 needs Vulkan and the Mali-400 has GLES2 only, so graphics tasks go to the
// cfw's N64 renderer (n64/src/gfx.c) with its Factor 5 microcode (gfx_f5.c). Runs on ultramodern's gfx thread,
// which owns the EGL context.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "ultramodern/renderer_context.hpp"
#include "ultramodern/ultramodern.hpp"

extern "C" {
#include "s30_display.h"
#include "s30_gfx.h"
}

extern "C" std::atomic<uint32_t> g_s30_dl_count;
std::atomic<uint32_t> g_s30_dl_count{0};
extern "C" std::atomic<uint32_t> g_s30_vi_count;
std::atomic<uint32_t> g_s30_vi_count{0};

namespace {

// Segment 0 (physical addresses) and every other segment: the RDRAM image
uint32_t s_segments[16];

int tex_elem_size(uintptr_t)
{
    return 4;
}

class S30RendererContext final : public ultramodern::renderer::RendererContext {
public:
    explicit S30RendererContext(uint8_t* rdram) : rdram_(rdram)
    {
        for (uint32_t& s : s_segments) {
            s = (uint32_t)(uintptr_t)rdram;
        }

        S30GfxConfig cfg{};

        cfg.ucode = S30_UCODE_F3DFACTOR5;
        cfg.segments = s_segments;
        cfg.floatMtx = 0;
        cfg.texElemSize = tex_elem_size;
        cfg.envPrefix = "RS64";
        cfg.overscan = 8; // the game scissors to 319 (and 8 lines in from the top): still the whole screen
        cfg.rdram = rdram;
        cfg.atlas = 1;
        // The game builds its next frame while this one waits for the flip (done at the next VI, update_screen)
        cfg.deferPresent = 1;
        S30Gfx_Configure(&cfg);
        S30Display_Init();
        S30Gfx_Init();
        setup_result = ultramodern::renderer::SetupResult::Success;
        chosen_api = ultramodern::renderer::GraphicsApi::Auto;
    }

    bool valid() override { return true; }
    bool update_config(const ultramodern::renderer::GraphicsConfig&, const ultramodern::renderer::GraphicsConfig&) override { return false; }
    void enable_instant_present() override {}

    void send_dl(const OSTask* task) override
    {
        const uint32_t n = g_s30_dl_count.fetch_add(1, std::memory_order_relaxed) + 1;
        static const int s_dump = getenv("RS64_DUMP_TASK") ? atoi(getenv("RS64_DUMP_TASK")) : 0;

        // RS64_DUMP_TASK=n: RDRAM (8 MB, native words) and the list address of graphics task n, for an offline walk
        if ((int)n == s_dump)
        {
            if (FILE* f = fopen("/tmp/rs64_rdram.bin", "wb")) {
                fwrite(rdram_, 1, 0x800000, f);
                fclose(f);
            }

            fprintf(stderr, "[s30] task %u dumped: data_ptr 0x%08X size 0x%X\n", n, (uint32_t)task->t.data_ptr, (uint32_t)task->t.data_size);
        }

        pc_process_displaylist(rdram_ + ((uint32_t)task->t.data_ptr & 0xFFFFFF));
    }

    void update_screen() override
    {
        g_s30_vi_count.fetch_add(1, std::memory_order_relaxed);
        S30Gfx_Present();

        // Where the VI puts the frame on the screen, relative to a full NTSC picture (H_VIDEO from 0x6C dots,
        // V_VIDEO from half-line 0x25): 640 dots and 240 lines across, one dot per X_SCALE/1024 frame pixels.
        // The high resolution mode's letterboxed 640x335 and the menus' 512-wide interlaced frame land where a TV shows them.
        const ultramodern::renderer::ViRegs* vi = ultramodern::renderer::get_vi_regs();
        const uint32_t hv = vi->VI_H_START_REG, vv = vi->VI_V_START_REG;
        const uint32_t xscale = vi->VI_X_SCALE_REG & 0xFFF, yscale = vi->VI_Y_SCALE_REG & 0xFFF;

        if (xscale == 0 || yscale == 0 || hv == 0 || vv == 0)
        {
            return;
        }

        const float x0 = ((float)((hv >> 16) & 0x3FF) - 0x6C) / 2.0f;
        const float y0 = ((float)((vv >> 16) & 0x3FF) - 0x25) / 2.0f;
        const float kx = 512.0f / xscale, ky = 1024.0f / yscale;
        const int stride = (int)(vi->VI_WIDTH_REG & 0xFFF);
        const uint64_t key = ((uint64_t)hv << 32) ^ vv ^ ((uint64_t)xscale << 20) ^ ((uint64_t)yscale << 8) ^ ((uint64_t)stride << 44);

        if (key != last_vi_)
        {
            last_vi_ = key;
            fprintf(stderr, "[s30] VI: h 0x%08X v 0x%08X x_scale 0x%X y_scale 0x%X width %d -> at %.1f,%.1f scale %.3f,%.3f\n",
                hv, vv, xscale, yscale, stride, x0, y0, kx, ky);
            S30Gfx_SetVi(x0, y0, kx, ky, stride);
        }
    }

    void shutdown() override {}
    uint32_t get_display_framerate() const override { return 60; }
    float get_resolution_scale() const override { return 1.0f; }

private:
    uint8_t* rdram_;
    uint64_t last_vi_ = 0;
};

}

namespace recomp {
    std::unique_ptr<ultramodern::renderer::RendererContext>
    create_render_context(uint8_t* rdram, ultramodern::renderer::WindowHandle, bool)
    {
        return std::make_unique<S30RendererContext>(rdram);
    }
}
