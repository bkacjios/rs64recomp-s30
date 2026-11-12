// Host hooks the S30 build does not carry yet (menus, language mods, mod levels, RT64 view scaling).
// Each returns the value that keeps the game's own behaviour.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include "recomp.h"
#include "game/lang/lang.h"
#include "host_api/host_api.h"
#include "lockstep/lockstep_core.h"
#include "video/video_config.h"

extern "C" {
volatile unsigned g_f5_task_hops = 0;
volatile unsigned g_f5_task_faces = 0;
volatile long long g_rs64_pdl_us = 0;
std::atomic<uint32_t> g_rs64_frame_tris{0};
std::atomic<uint32_t> g_rs64_frame_draws{0};
std::atomic<uint32_t> g_rs64_frame_texloads{0};
}
std::atomic<uint64_t> g_rs64_cpu_cleared_wid{0};
std::atomic<uint64_t> g_rs64_last_workload_id{0};
std::atomic<uint32_t> g_rs64_cpu_cleared_fb{0};

extern "C" {
// The S30's 3:2 panel over the N64's 4:3 (gfx.c's widescreen): the game's culling and HUD placement widen by it
float rs64_aspect_scale() { return 1.125f; }
uint32_t rs64_asset_manifest_to_free(uint32_t a0) { return a0; }
void rs64_asset_overrides_patch(uint8_t* a0, uint32_t a1) {}
int rs64_atst_head(float* a0, float* a1) { return 0; }
void rs64_camera_shots_frame(uint8_t* a0, recomp_context* a1) {}
void rs64_camera_shots_mission_start(uint8_t* a0) {}
void rs64_camera_shots_trigger(uint8_t* a0, uint32_t a1, uint32_t a2) {}
void rs64_cine_pause_point(uint8_t* a0) {}
void rs64_controls_force_luke(uint8_t* a0) {}
void rs64_controls_load_models(uint8_t* a0, recomp_context* a1) {}
int rs64_controls_menu_active() { return 0; }
int rs64_crawl_camera(uint8_t* a0, recomp_context* a1) { return 0; }
void rs64_crawl_drop(uint8_t* a0, recomp_context* a1) {}
int rs64_crawl_playing() { return 0; }
// Debug: /tmp/s30_toggles ("widen placement", re-read every 600 calls) overrides the culling widening and HUD placement live
static float s_toggle[2] = { 1.125f, 1.0f };

static void read_toggles()
{
    static unsigned s_calls = 0;

    if ((s_calls++ % 600) != 0) {
        return;
    }

    if (FILE* f = fopen("/tmp/s30_toggles", "r")) {
        float w = 0.0f;
        float p = 0.0f;

        if (fscanf(f, "%f %f", &w, &p) == 2) {
            s_toggle[0] = w;
            s_toggle[1] = p;
        }

        fclose(f);
    }
}

float rs64_cull_widen()
{
    read_toggles();
    return s_toggle[0];
}
void rs64_f5_cable_submit(uint32_t a0, uint32_t a1, uint32_t a2) {}
void rs64_f5_map_node(unsigned a0, unsigned a1) {}
void rs64_f5_map_sprite(unsigned a0, unsigned a1) {}
unsigned rs64_gof_override(uint8_t* a0, unsigned a1) { return 0; }
int rs64_hook(uint32_t a0, uint8_t* a1, recomp_context* a2) { return 0; }
void rs64_lang_font_load(uint8_t* a0, unsigned a1) {}
void rs64_lang_font_loaded(uint8_t* a0, unsigned a1, unsigned a2) {}
void rs64_lang_record_tables(uint8_t* a0) {}
void rs64_lang_record_tables_release(uint8_t* a0) {}
void rs64_lang_slot_tables(uint8_t* a0) {}
void rs64_lang_slot_tables_release(uint8_t* a0) {}
uint32_t rs64_lang_text(uint8_t* a0, uint32_t a1, uint32_t a2) { return 0; }
void rs64_lang_texture_pool(uint8_t* a0) {}
void rs64_lang_txt_file(uint8_t* a0, uint32_t a1, int a2) {}
void rs64_lang_voice_id(uint32_t a0) {}
void rs64_lang_voice_text(uint8_t* a0) {}
void rs64_menu_confirm_main(uint8_t* a0) {}
void rs64_menu_confirm_sound(uint8_t* a0, recomp_context* a1) {}
void rs64_menu_hires_apply(uint8_t* a0) {}
void rs64_menu_input_state(uint8_t* a0, recomp_context* a1) {}
int rs64_menu_install_main(uint8_t* a0) { return -1; }
int rs64_menu_sfx_swap(recomp_context* a0) { return 0; }
void rs64_menu_text_language(int a0) {}
void rs64_mod_levels_brief_rate(recomp_context* a0) {}
void rs64_mod_levels_brief_stream(uint8_t* a0, recomp_context* a1) {}
int rs64_mod_levels_craft(uint32_t* a0) { return -1; }
int rs64_mod_levels_crawl_id(int a0) { return -2; }
uint32_t rs64_mod_levels_crawl_text(uint8_t* a0) { return 0; }
void rs64_mod_levels_debrief_begin(uint8_t* a0, uint32_t a1) {}
void rs64_mod_levels_debrief_end(uint8_t* a0) {}
int rs64_mod_levels_debriefing() { return 0; }
void rs64_mod_levels_entry_level(uint32_t* a0) {}
int rs64_mod_levels_entry_medal(uint8_t* a0, uint32_t a1) { return -1; }
uint32_t rs64_mod_levels_lookup(uint8_t* a0, uint32_t a1) { return a1; }
void rs64_mod_levels_medal_earned(int a0) {}
void rs64_mod_levels_menu(int a0) {}
void rs64_mod_levels_music(uint8_t* a0) {}
int rs64_mod_levels_objectives_friendlies(uint8_t* a0, recomp_context* a1) { return 0; }
int rs64_mod_levels_objectives_init(uint8_t* a0) { return 0; }
int rs64_mod_levels_objectives_tick(uint8_t* a0, recomp_context* a1) { return 0; }
void rs64_mod_levels_objective_text(uint8_t* a0, recomp_context* a1) {}
void rs64_mod_levels_pause_voice(uint8_t* a0, recomp_context* a1) {}
int rs64_mod_levels_playing() { return 0; }
void rs64_mod_levels_select_close() {}
void rs64_mod_levels_select_confirm(uint8_t* a0, recomp_context* a1) {}
void rs64_mod_levels_select_game(uint8_t* a0, recomp_context* a1) {}
void rs64_mod_levels_select_open(uint8_t* a0, recomp_context* a1) {}
void rs64_mod_levels_text(uint8_t* a0, recomp_context* a1, int a2) {}
void rs64_mod_levels_title(uint8_t* a0, recomp_context* a1) {}
void rs64_mod_levels_voice(uint8_t* a0, recomp_context* a1) {}
void rs64_nav_boot_cutscene(uint8_t* a0, uint32_t* a1, uint32_t* a2) {}
void rs64_nav_boot_cutscene_done() {}
void rs64_passcode_row(uint8_t* a0, recomp_context* a1) {}
unsigned rs64_pause_array_for(uint8_t* a0, unsigned a1, unsigned a2) { return a2; }
void rs64_pause_back_begin(uint8_t* a0, unsigned a1) {}
void rs64_pause_back_end(uint8_t* a0, unsigned a1) {}
void rs64_pause_confirm(uint8_t* a0, unsigned a1) {}
void rs64_pause_frame_point() {}
void rs64_pause_hover_sound(uint8_t* a0, recomp_context* a1) {}
void rs64_pause_install(uint8_t* a0, recomp_context* a1) {}
unsigned rs64_pause_label_for(uint8_t* a0, unsigned a1, unsigned a2) { return 0; }
void rs64_pause_slider_bar(uint8_t* a0, recomp_context* a1) {}
void rs64_pause_slider_ends(uint8_t* a0, recomp_context* a1) {}
void rs64_pause_slider_load(uint8_t* a0, recomp_context* a1) {}
void rs64_pause_slider_store(uint8_t* a0, recomp_context* a1) {}
float rs64_res_scale() { return 1.0f; }
void rs64_rules_area(uint8_t* a0, uint32_t a1, uint32_t a2) {}
void rs64_rules_destroyed(uint8_t* a0, recomp_context* a1) {}
void rs64_rules_frame(uint8_t* a0, recomp_context* a1) {}
void rs64_rules_mission_start(uint8_t* a0) {}
void rs64_rules_path(uint8_t* a0, uint32_t a1) {}
void rs64_rules_voice_text(uint8_t* a0) {}
void rs64_rules_voice_text_id(recomp_context* a0) {}
float rs64_shadow_cull_margin() { return 1.0f; }
float rs64_shadow_keep_dist() { return 0.0f; }
void rs64_slider_redirect(uint8_t* a0, unsigned a1) {}
float rs64_throttle_hook(uint8_t* a0, float a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6) { return a1; }
void rs64_wheel_typed(uint8_t* a0, recomp_context* a1) {}
}

namespace rs64::host { bool ram_ok(uint32_t) { return true; } }
namespace rs64::lang { std::vector<std::string> codes() { return { "en" }; } const Language* active() { return nullptr; } }
namespace rs64::ls { bool cutscene_skipped(bool, uint32_t, uint32_t, bool) { return false; } }
namespace rs64::video {
float draw_distance() { return 1.0f; }
float effective_draw_distance(float, bool, uint32_t) { return 1.0f; }
float hud_placement() { read_toggles(); return s_toggle[1]; }
bool keep_cutscene_draw_distance() { return false; }
bool legacy_terrain() { return false; }
std::string language() { return ""; }
void set_language(const std::string&) {}
}
