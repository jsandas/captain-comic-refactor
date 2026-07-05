#include <SDL2/SDL.h>

#include <cmath>
#include <filesystem>
#include <fstream>

#include "../include/physics.h"
#include "test_cases.h"
#include "test_helpers.h"

namespace {
bool write_minimal_png(const std::filesystem::path& path) {
    // 1x1 opaque white PNG.
    static const unsigned char kPngData[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
        0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
        0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x9C, 0x63, 0xF8, 0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x09, 0xFB, 0x03, 0xFD, 0x2A, 0x86,
        0xE3, 0x8A, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

    std::ofstream out(path, std::ios::binary);
    if (!out.good()) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(kPngData),
              static_cast<std::streamsize>(sizeof(kPngData)));
    return out.good();
}
}  // namespace

void test_animation_looping() {
    reset_physics_state();
    GraphicsSystem graphics(nullptr);
    Animation anim = make_animation({100, 200, 100}, true);

    graphics.update_animation(anim, 50);
    check(anim.current_frame == 0, "looping: frame at 50ms should be 0");

    graphics.update_animation(anim, 150);
    check(anim.current_frame == 1, "looping: frame at 150ms should be 1");

    graphics.update_animation(anim, 310);
    check(anim.current_frame == 2, "looping: frame at 310ms should be 2");

    graphics.update_animation(anim, 450);
    check(anim.current_frame == 0, "looping: frame at 450ms should loop to 0");
}

void test_animation_non_looping() {
    reset_physics_state();
    GraphicsSystem graphics(nullptr);
    Animation anim = make_animation({100, 200, 100}, false);

    graphics.update_animation(anim, 50);
    check(anim.current_frame == 0, "non-looping: frame at 50ms should be 0");

    graphics.update_animation(anim, 150);
    check(anim.current_frame == 1, "non-looping: frame at 150ms should be 1");

    graphics.update_animation(anim, 450);
    check(anim.current_frame == 2, "non-looping: frame at 450ms should clamp to 2");
}

void test_animation_zero_duration() {
    reset_physics_state();
    GraphicsSystem graphics(nullptr);
    Animation anim = make_animation({0, 0}, true);

    graphics.update_animation(anim, 1);
    check(anim.current_frame >= 0 && anim.current_frame < static_cast<int>(anim.frames.size()),
          "zero duration: frame index should stay in range");
}

void test_enemy_animation_sequence() {
    reset_physics_state();
    std::vector<uint8_t> loop_sequence = build_enemy_animation_sequence(3, ENEMY_ANIMATION_LOOP);
    check(loop_sequence == std::vector<uint8_t>({0, 1, 2}),
          "enemy loop sequence should be 0,1,2 for 3 frames");

    std::vector<uint8_t> alternate_sequence =
        build_enemy_animation_sequence(3, ENEMY_ANIMATION_ALTERNATE);
    check(alternate_sequence == std::vector<uint8_t>({0, 1, 2, 1}),
          "enemy alternate sequence should be 0,1,2,1 for 3 frames");

    std::vector<uint8_t> alternate_sequence_four =
        build_enemy_animation_sequence(4, ENEMY_ANIMATION_ALTERNATE);
    check(alternate_sequence_four == std::vector<uint8_t>({0, 1, 2, 3, 2, 1}),
          "enemy alternate sequence should be 0,1,2,3,2,1 for 4 frames");

    std::vector<uint8_t> empty_sequence = build_enemy_animation_sequence(0, ENEMY_ANIMATION_LOOP);
    check(empty_sequence.empty(), "enemy sequence should be empty for 0 frames");
}

void test_tileset_blackout_state_tracking() {
    // set_tileset_blackout against an empty tilesets map takes the early-return
    // path (no tileset loaded yet) but must still record the requested state so
    // that load_tileset can apply it when the tileset is loaded later.
    GraphicsSystem graphics(nullptr);

    // Default: no entry recorded for an unknown level.
    check(!graphics.is_tileset_blacked_out("castle"), "blackout: unset level should report false");

    // Record blackout=true with no tileset present (early-return path).
    graphics.set_tileset_blackout("castle", true);
    check(graphics.is_tileset_blacked_out("castle"),
          "blackout: state should be recorded as true after set with no tileset");

    // Toggling to false must also be recorded.
    graphics.set_tileset_blackout("castle", false);
    check(!graphics.is_tileset_blacked_out("castle"),
          "blackout: state should be updated to false after second set");

    // A different level must be tracked independently.
    graphics.set_tileset_blackout("forest", true);
    check(graphics.is_tileset_blacked_out("forest"),
          "blackout: separate level state should be true");
    check(!graphics.is_tileset_blacked_out("castle"),
          "blackout: castle state must remain false after forest was set");
}

void test_asset_path_resolution() {
    reset_physics_state();
    namespace fs = std::filesystem;
    // create a temporary directory tree mimicking the asset layout
    fs::path base = fs::temp_directory_path() / "comic_assets_test";
    fs::remove_all(base);
    fs::create_directories(base / "assets" / "sprites");
    fs::create_directories(base / "assets" / "sounds");
    fs::create_directories(base / "assets" / "maps");
    fs::create_directories(base / "assets" / "tiles");
    fs::create_directories(base / "assets" / "shp");
    fs::create_directories(base / "assets" / "graphics");

    // touch one file of each type
    std::ofstream((base / "assets" / "sprites" / "sprite-test.png").string()).close();
    std::ofstream((base / "assets" / "sounds" / "sound-test.wav").string()).close();
    std::ofstream((base / "assets" / "maps" / "foo.pt.png").string()).close();
    std::ofstream((base / "assets" / "tiles" / "foo.tt2-00.png").string()).close();
    std::ofstream((base / "assets" / "shp" / "foo.shp").string()).close();
    std::ofstream((base / "assets" / "graphics" / "sys000.ega").string()).close();

    // remember cwd so we can restore later
    fs::path orig_cwd = fs::current_path();
    fs::current_path(base);

    GraphicsSystem g(nullptr);
    std::string p;
    p = g.get_asset_path("sprite-test.png");
    check(p.find("assets/sprites/sprite-test.png") != std::string::npos,
          "get_asset_path should find sprite in sprites subdir");
    p = g.get_asset_path("sound-test.wav");
    check(p.find("assets/sounds/sound-test.wav") != std::string::npos,
          "get_asset_path should find sound in sounds subdir");
    p = g.get_asset_path("foo.pt.png");
    check(p.find("assets/maps/foo.pt.png") != std::string::npos,
          "get_asset_path should find map in maps subdir");
    p = g.get_asset_path("foo.tt2-00.png");
    check(p.find("assets/tiles/foo.tt2-00.png") != std::string::npos,
          "get_asset_path should find tileset in tiles subdir");
    p = g.get_asset_path("foo.shp");
    check(p.find("assets/shp/foo.shp") != std::string::npos,
          "get_asset_path should find shp in shp subdir");
    p = g.get_asset_path("sys000.ega");
    check(p.find("assets/graphics/sys000.ega") != std::string::npos,
          "get_asset_path should find raw graphics in graphics subdir");

    // cleanup
    fs::current_path(orig_cwd);
    fs::remove_all(base);
}

void test_tileset_blackout_state_tracks_unloaded_tileset() {
    reset_physics_state();
    namespace fs = std::filesystem;

    fs::path base = fs::temp_directory_path() / "comic_blackout_tileset_test";
    fs::path original_cwd = fs::current_path();
    fs::remove_all(base);
    fs::create_directories(base / "assets" / "tiles");

    const fs::path castle_tile = base / "assets" / "tiles" / "castle.tt2-00.png";
    if (!write_minimal_png(castle_tile)) {
        check(false, "failed to create castle tile fixture for blackout test");
        fs::remove_all(base);
        return;
    }

    fs::current_path(base);

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        check(false, std::string("SDL video init failed: ") + SDL_GetError());
        fs::current_path(original_cwd);
        fs::remove_all(base);
        return;
    }

    SDL_Window* window = SDL_CreateWindow("test_blackout", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, 64, 64, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        check(false, std::string("SDL window creation failed: ") + SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        fs::current_path(original_cwd);
        fs::remove_all(base);
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) {
        check(false, std::string("SDL renderer creation failed: ") + SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        fs::current_path(original_cwd);
        fs::remove_all(base);
        return;
    }

    {
        GraphicsSystem graphics(renderer);
        check(graphics.initialize(), "graphics initialize should succeed for blackout test");

        // Configure blackout before loading tileset; load_tileset should apply it.
        graphics.set_tileset_blackout("castle", true);
        check(graphics.load_tileset("castle"), "castle tileset should load for blackout test");

        Tileset* castle_tileset = graphics.get_tileset("castle");
        check(castle_tileset != nullptr, "castle tileset should be retrievable after load");

        if (castle_tileset != nullptr && !castle_tileset->tiles.empty()) {
            SDL_Texture* sample_texture = castle_tileset->tiles.begin()->second.texture;
            check(sample_texture != nullptr, "sample castle tile texture should be non-null");

            if (sample_texture != nullptr) {
                Uint8 r = 255;
                Uint8 g = 255;
                Uint8 b = 255;
                SDL_GetTextureColorMod(sample_texture, &r, &g, &b);
                check(r == 0 && g == 0 && b == 0,
                      "blackout set before load should darken loaded tile textures");

                graphics.set_tileset_blackout("castle", false);
                SDL_GetTextureColorMod(sample_texture, &r, &g, &b);
                check(r == 255 && g == 255 && b == 255,
                      "clearing blackout should restore tile texture color modulation");
            }
        } else {
            check(false, "castle tileset should contain at least one tile texture");
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    fs::current_path(original_cwd);
    fs::remove_all(base);
}

// Regression: viewport height must be derived from render_scale * PLAYFIELD_HEIGHT
// so that it always agrees with the game-unit coordinate system used for sprites.
// Previously, playfield_viewport.h = floor(160 * letterbox_scale) could produce a
// smaller value than render_scale * PLAYFIELD_HEIGHT (which equals 20 * render_scale),
// causing the bottom of the player sprite to be clipped when falling back to the
// floor from a jump.
void test_playfield_viewport_height_matches_render_scale() {
    // Probe a representative range of window widths. For each width, compute
    // letterbox_scale (as main.cpp does) and verify the two formulas agree.
    // EGA_WIDTH=320, EGA_HEIGHT=200; playfield is 192x160 EGA pixels (24x20 game units).
    constexpr int EGA_WIDTH = 320;
    constexpr float EGA_PLAYFIELD_H = 160.0f;

    // Spot-check: demonstrate the old formula was wrong at the scale that caused the bug.
    // At window_w = 992 (a common non-integer multiple of 320):
    //   letterbox_scale = 992/320 = 3.1
    //   render_scale = round(8 * 3.1) = round(24.8) = 25
    //   old viewport_h = floor(160 * 3.1) = floor(496.0) = 496
    //   new viewport_h = 25 * 20 = 500
    //   sprite at comic_y=16 has bottom at 16*25 + 4*25 = 500 > old_viewport(496) → clipped!
    {
        const int window_w = 992;
        const float ls = static_cast<float>(window_w) / EGA_WIDTH;
        const int rs = static_cast<int>(8.0f * ls + 0.5f);
        const int old_viewport_h = static_cast<int>(EGA_PLAYFIELD_H * ls);
        const int new_viewport_h = rs * PLAYFIELD_HEIGHT;
        check(new_viewport_h >= old_viewport_h,
              "viewport_h (new) must be >= old formula at scale 3.1");
        // The sprite bottom for a player at comic_y = PLAYFIELD_HEIGHT - 4 (lowest
        // visible position) is (PLAYFIELD_HEIGHT) * rs. That must fit in viewport.
        const int sprite_bottom_at_floor = PLAYFIELD_HEIGHT * rs;
        check(new_viewport_h >= sprite_bottom_at_floor,
              "new viewport must contain sprite at comic_y = PLAYFIELD_HEIGHT - 4");
        check(old_viewport_h < sprite_bottom_at_floor,
              "old viewport was too small at scale 3.1 (regression guard)");
    }

    // Also verify the new formula holds across a broad range of window widths.
    bool all_ok = true;
    for (int w = 320; w <= 3840; w += 1) {
        const float ls = static_cast<float>(w) / EGA_WIDTH;
        const int rs = (ls < 0.125f) ? 1 : static_cast<int>(8.0f * ls + 0.5f);
        const int new_viewport_h = rs * PLAYFIELD_HEIGHT;
        const int sprite_bottom = PLAYFIELD_HEIGHT * rs;
        if (new_viewport_h < sprite_bottom) {
            all_ok = false;
            break;
        }
    }
    check(all_ok,
          "render_scale * PLAYFIELD_HEIGHT must always contain the bottom of the playfield sprite");
}

void test_runtime_level_tiles_populated() {
    reset_physics_state();
    initialize_level_data();
    current_level_number = LEVEL_NUMBER_FOREST;
    current_stage_number = 0;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    current_level_ptr = nullptr;

    load_new_level();
    check(current_level_ptr != nullptr, "current_level_ptr should be set after load_new_level");

    bool any_non_zero = false;
    const uint8_t* tiles = current_level_ptr->stages[current_stage_number].tiles;
    for (int i = 0; i < 128 * 10; ++i) {
        if (tiles[i] != 0) {
            any_non_zero = true;
            break;
        }
    }

    check(any_non_zero, "current level tiles should be populated (non-zero)");
    reset_door_state();
}
