// Rogue Squadron on the PocketGo S30: perf-test entry point.
// Runs the recompiled game with display lists dropped (s30_renderer.cpp) and MusyX audio through SDL/ALSA,
// and prints one line a second: game frames, display lists, audio tasks and per-thread CPU time.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <string>
#include <thread>
#include <unistd.h>

#include "SDL.h"
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "audio/audio.h"
#include "video/rt64_render_context.h"

#include <linux/input-event-codes.h>

extern "C" {
#include "s30_input.h"
}

extern "C" std::atomic<uint32_t> g_s30_dl_count;
extern "C" std::atomic<uint32_t> g_s30_vi_count;
extern "C" { volatile uint8_t* volatile g_recomp_rdram_for_wp_raw = nullptr; }
extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();
RspUcodeFunc* s30_get_rsp_microcode(const OSTask* task);
void rs64_register_overlays();
const char* const* rs64_register_patches();

static constexpr uint64_t kRomHash = 0x6B66A44153594DEAULL;
static constexpr const char8_t* kGameId = u8"rs64.n64.us.1.0";

extern "C" void s30_entrypoint(uint8_t* rdram, recomp_context* ctx)
{
    g_recomp_rdram_for_wp_raw = rdram;
    recomp_entrypoint(rdram, ctx);
}

static std::atomic<uint16_t> g_buttons{0};
static S30Pad g_pad;

// N64 layout on the S30: A fire, B secondary weapon, shoulders Z and R, X/Y/L2/SELECT the C buttons
static const S30ButtonMap kButtons[] = {
    { BTN_EAST, S30_N64_A },         { BTN_SOUTH, S30_N64_B },          { BTN_WEST, S30_N64_CLEFT },
    { BTN_NORTH, S30_N64_CDOWN },    { BTN_TL, S30_N64_Z },             { BTN_TR, S30_N64_R },
    { BTN_TL2, S30_N64_CUP },        { BTN_SELECT, S30_N64_CRIGHT },    { BTN_START, S30_N64_START },
    { BTN_DPAD_UP, S30_N64_DUP },    { BTN_DPAD_DOWN, S30_N64_DDOWN },  { BTN_DPAD_LEFT, S30_N64_DLEFT },
    { BTN_DPAD_RIGHT, S30_N64_DRIGHT },
};

static void poll_input()
{
    S30Input_Read(&g_pad);
}

static bool get_input(int controller_num, uint16_t* buttons, float* x, float* y)
{
    if (controller_num != 0)
    {
        return false;
    }

    *buttons = g_buttons.load() | g_pad.button;
    *x = g_pad.stick_x / 127.0f;
    *y = g_pad.stick_y / 127.0f;
    return true;
}

static void set_rumble(int, bool) {}

static ultramodern::input::connected_device_info_t get_device_info(int controller_num)
{
    if (controller_num == 0) {
        return { ultramodern::input::Device::Controller, ultramodern::input::Pak::None };
    }

    return { ultramodern::input::Device::None, ultramodern::input::Pak::None };
}

static ultramodern::gfx_callbacks_t::gfx_data_t create_gfx()
{
    // SIGTERM (the system menu's Exit game) ends the process: SDL would turn it into an SDL_QUIT nothing reads
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[s30] SDL_Init(audio) failed: %s\n", SDL_GetError());
    }

    return nullptr;
}

static ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t)
{
    return {};
}

static void update_gfx(ultramodern::gfx_callbacks_t::gfx_data_t) {}

// CPU time of each thread in this process, by name, from /proc (ticks of 1/100 s).
static void print_thread_cpu(double seconds)
{
    static std::vector<std::pair<int, unsigned long long>> s_last;
    std::vector<std::pair<int, unsigned long long>> now;
    std::string line;
    long hz = sysconf(_SC_CLK_TCK);
    DIR* d = opendir("/proc/self/task");

    if (!d)
    {
        return;
    }

    while (dirent* e = readdir(d)) {
        int tid = atoi(e->d_name);

        if (tid <= 0) {
            continue;
        }

        char path[64];
        char buf[512];
        snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);
        FILE* f = fopen(path, "r");

        if (!f) {
            continue;
        }

        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = 0;
        char* lp = strchr(buf, '(');
        char* rp = strrchr(buf, ')');

        if (!lp || !rp) {
            continue;
        }

        std::string name(lp + 1, rp);
        unsigned long long ut = 0;
        unsigned long long st = 0;
        sscanf(rp + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &ut, &st);
        unsigned long long t = ut + st;
        now.push_back({ tid, t });
        unsigned long long prev = 0;

        for (auto& p : s_last) {
            if (p.first == tid) {
                prev = p.second;
            }
        }

        double pct = (double)(t - prev) * 100.0 / (double)hz / seconds;

        if (pct >= 3.0)
        {
            char item[96];
            snprintf(item, sizeof(item), " %s=%.0f%%", name.c_str(), pct);
            line += item;
        }
    }

    closedir(d);
    s_last = std::move(now);
    fprintf(stderr, "[s30-cpu]%s\n", line.c_str());
}

static void stats_thread()
{
    // Game frames: each osViSwapBuffer with a new framebuffer shows up as a VI-origin change on the screen updates.
    uint32_t last_dl = 0;
    uint32_t last_vi = 0;
    auto last = std::chrono::steady_clock::now();

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto t = std::chrono::steady_clock::now();
        double s = std::chrono::duration<double>(t - last).count();
        uint32_t dl = g_s30_dl_count.load();
        uint32_t vi = g_s30_vi_count.load();

        fprintf(stderr, "[s30-perf] gfx tasks %.1f/s  screen updates %.1f/s\n", (dl - last_dl) / s, (vi - last_vi) / s);
        print_thread_cpu(s);
        fflush(stderr);
        last_dl = dl;
        last_vi = vi;
        last = t;
    }
}

// S30_PRESS="ms:mask,ms:mask,...": hold N64 buttons from each time (ms since start) until the next entry, to get past the title headless.
static void press_thread()
{
    const char* seq = getenv("S30_PRESS");

    if (!seq)
    {
        return;
    }

    auto start = std::chrono::steady_clock::now();

    for (const char* p = seq; *p;) {
        unsigned ms = 0;
        unsigned mask = 0;

        if (sscanf(p, "%u:%x", &ms, &mask) != 2) {
            break;
        }

        std::this_thread::sleep_until(start + std::chrono::milliseconds(ms));
        g_buttons.store((uint16_t)mask);
        fprintf(stderr, "[s30-press] %u ms: 0x%04X\n", ms, mask);
        const char* c = strchr(p, ',');

        if (!c) {
            break;
        }

        p = c + 1;
    }
}

static std::string thread_name(const OSThread* t)
{
    return "n64-" + std::to_string(t->id);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IOLBF, 0);

    // The renderer reads live RDRAM within the task, so ultramodern's 8 MB copy of RDRAM per graphics task
    // (RT64's slow parse needed it) is half the CPU time here; the chunk releases wait for the parse instead.
    setenv("ROGUESQ_PARSE_SNAPSHOT", "0", 0);

    // Graphics done when the task arrives: the game's next frame overlaps this one's drawing, its display-list reuse waiting for it
    setenv("ROGUESQ_GFX_DONE_BEFORE_PARSE", "1", 0);

    // The retrace handler (buffer swaps) takes over at the game's next function call when its VI arrives, as the N64's interrupt
    // would: ultramodern otherwise lets it run only when the game thread blocks, a frame's logic later, so every frame missed a VI
    setenv("S30_PREEMPT", "1", 0);
    if (argc > 1 && chdir(argv[1]) != 0)
    {
        fprintf(stderr, "[s30] can't enter %s\n", argv[1]);
        return 1;
    }

    rs64_register_overlays();
    rs64_register_patches();
    recomp::register_config_path(std::filesystem::current_path());
    recomp::register_game(recomp::GameEntry{
        .rom_hash = kRomHash,
        .internal_name = "ROGUE SQUADRON",
        .game_id = kGameId,
        .mod_game_id = "rs64",
        .save_type = recomp::SaveType::Eep4k,
        .is_enabled = true,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = s30_entrypoint,
    });

    std::u8string game_id = kGameId;
    recomp::check_all_stored_roms();

    if (!recomp::is_rom_valid(game_id)) {
        recomp::select_rom(std::filesystem::current_path() / "rogue_squadron.z64", game_id);
        recomp::check_all_stored_roms();
    }

    if (!recomp::is_rom_valid(game_id))
    {
        fprintf(stderr, "[s30] rogue_squadron.z64 (USA v1.0) not found or wrong ROM\n");
        return 1;
    }

    S30Input_Init(kButtons, sizeof(kButtons) / sizeof(kButtons[0]));
    std::thread(stats_thread).detach();
    std::thread(press_thread).detach();

    recomp::start(recomp::Configuration{
        .project_version = { 0, 1, 0 },
        .window_handle = {},
        .rsp_callbacks = { .get_rsp_microcode = s30_get_rsp_microcode },
        .renderer_callbacks = { .create_render_context = recomp::create_render_context },
        .audio_callbacks = {
            .queue_samples = rs64::audio::queue_samples,
            .get_frames_remaining = rs64::audio::get_frames_remaining,
            .set_frequency = rs64::audio::set_frequency,
        },
        .input_callbacks = {
            .poll_input = poll_input,
            .get_input = get_input,
            .set_rumble = set_rumble,
            .get_connected_device_info = get_device_info,
        },
        .gfx_callbacks = { .create_gfx = create_gfx, .create_window = create_window, .update_gfx = update_gfx },
        .events_callbacks = {
            .vi_callback = nullptr,
            .gfx_init_callback = []() { recomp::start_game(std::u8string(kGameId)); },
        },
        .error_handling_callbacks = {
            .message_box = [](const char* msg) {
                fprintf(stderr, "[Error] %s\n", msg);
                fflush(stderr);
            },
        },
        .threads_callbacks = { .get_game_thread_name = thread_name },
        .message_queue_control = {},
    });

    return 0;
}
