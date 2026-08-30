#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

#include "../include/actors.h"
#include "../include/audio.h"
#include "../include/cheats.h"
#include "../include/doors.h"
#include "../include/graphics.h"
#include "../include/level_loader.h"
#include "../include/physics.h"
#include "../include/player_teleport.h"
#include "../include/title_sequence.h"
#include "../include/ui_system.h"

// Game state
int comic_x = 20;
int comic_y = 2;
int8_t comic_y_vel = 0;
int8_t comic_x_momentum = 0;
uint8_t comic_facing = 1;  // 1 right, 0 left
uint8_t comic_animation = 0;
uint8_t comic_is_falling_or_jumping = 1;
uint8_t comic_jump_counter = 0;
uint8_t comic_jump_power = JUMP_POWER_DEFAULT;
uint8_t key_state_jump = 0;
uint8_t previous_key_state_jump = 0;
uint8_t key_state_left = 0;
uint8_t key_state_right = 0;
uint8_t key_state_open = 0;               // Open key for doors
uint8_t previous_key_state_open = 0;      // Track previous state for edge-triggered activation
uint8_t key_state_fire = 0;               // Fire key (Left Ctrl)
uint8_t key_state_teleport = 0;           // Teleport key (CapsLock-style binding)
uint8_t previous_key_state_teleport = 0;  // Track previous teleport key state for edge trigger
int camera_x = 0;

// Smooth renderer interpolation state: snapshot of positions at the end of
// the previous physics tick.  The render loop lerps between these and the
// current values using the tick accumulator fraction.
float prev_camera_x = 0.0f;
float prev_comic_x = 0.0f;
float prev_comic_y = 0.0f;

// Item collection state
uint8_t comic_has_door_key = 0;  // 1 if player has door key, 0 otherwise
uint8_t comic_num_lives = 0;  // Number of lives remaining (counts up from 0 to 5, then subtracts 1)
uint8_t lives_sequence_counter = 5;  // Lives count-up sequence: award 5 lives, then subtract 1
uint8_t lives_sequence_delay = 1;  // Delay counter for lives animation (1 tick between each award)
bool lives_sequence_complete = false;        // Flag to stop lives sequence after completion
uint8_t comic_hp = 0;                        // Current health points (0-6)
uint8_t comic_hp_pending_increase = MAX_HP;  // HP fills gradually from 0 to MAX_HP at game start
uint8_t score_bytes[3] = {0, 0, 0};          // Score in base-100 encoding
uint8_t score_10000_counter = 0;  // Count carries into ten-thousands digit; 5 -> extra life
// Note: Firepower, items, and treasures are managed by ActorSystem (authoritative state)

// Level/stage transition tracking
uint8_t current_level_number = 1;            // Current level (0=LAKE, 1=FOREST, etc.)
uint8_t current_stage_number = 0;            // Current stage (0-2 per level)
const level_t* current_level_ptr = nullptr;  // Pointer to current level data
int8_t source_door_level_number = -1;  // Set when entering via door for reciprocal positioning
int8_t source_door_stage_number = -1;

// Checkpoint position (for respawn and boundary crossing)
uint8_t comic_y_checkpoint = 12;  // Y position to respawn at
uint8_t comic_x_checkpoint = 14;  // X position to respawn at

// Game state
bool game_over_triggered =
    false;  // Set when life depletion should transition to game-over sequence

enum class GameState { Playing, Paused, Victory, GameOver, Exiting };

// Teleport state
bool comic_is_teleporting = false;
uint8_t teleport_animation = 0;
uint8_t teleport_source_x = 0;
uint8_t teleport_source_y = 0;
uint8_t teleport_destination_x = 0;
uint8_t teleport_destination_y = 0;
uint8_t teleport_camera_counter = 0;
int8_t teleport_camera_vel = 0;
bool teleport_skip_tick = false;

// Global system pointers (for access from other modules)
ActorSystem* g_actor_system = nullptr;  // Actor system pointer (for cheat access)

// Level names (indexed by level number)
static constexpr const char* level_names[] = {"lake", "forest", "space",  "base",
                                              "cave", "shed",   "castle", "comp"};

// Player animation state
Animation comic_idle_right;
Animation comic_idle_left;
Animation comic_run_right;
Animation comic_run_left;
Animation comic_jump_right;
Animation comic_jump_left;
Animation comic_death;
Animation* current_animation = nullptr;

// Phase 5: run cycle frame counter, advanced once per game tick unconditionally
// (mirrors assembly comic_run_cycle which increments at the top of every .tick)
int comic_run_cycle_frame = 0;

void process_door_input() {
    // Edge-triggered door activation: only trigger on rising edge of open key
    // This prevents the door from immediately re-triggering when entering a new stage
    // (since load_new_stage positions Comic at the reciprocal door location)
    if (comic_is_falling_or_jumping == 0 && key_state_open && !previous_key_state_open) {
        check_door_activation();
    }

    previous_key_state_open = key_state_open;
}

void clear_gameplay_key_states() {
    key_state_jump = 0;
    previous_key_state_jump = 0;
    key_state_left = 0;
    key_state_right = 0;
    key_state_open = 0;
    previous_key_state_open = 0;
    key_state_fire = 0;
    key_state_teleport = 0;
    previous_key_state_teleport = 0;
}

void begin_teleport() {
    constexpr uint8_t TELEPORT_DISTANCE = 6;

    uint8_t dest_x = static_cast<uint8_t>(comic_x);
    uint8_t dest_y = static_cast<uint8_t>(comic_y);
    const int camera_rel_x = static_cast<int>(comic_x) - camera_x;

    teleport_camera_counter = 0;

    if (comic_facing == COMIC_FACING_LEFT) {
        teleport_camera_vel = -1;

        if (camera_rel_x >= TELEPORT_DISTANCE) {
            dest_x = static_cast<uint8_t>(dest_x - TELEPORT_DISTANCE);

            const int dest_camera_rel = camera_rel_x - TELEPORT_DISTANCE;
            if (dest_camera_rel < (PLAYFIELD_WIDTH / 2 - 2)) {
                const int camera_movement = (PLAYFIELD_WIDTH / 2 - 2) - dest_camera_rel;
                teleport_camera_counter = static_cast<uint8_t>(std::min(camera_x, camera_movement));
            }
        }
    } else {
        teleport_camera_vel = 1;

        if (camera_rel_x < (PLAYFIELD_WIDTH - TELEPORT_DISTANCE - 1)) {
            dest_x = static_cast<uint8_t>(dest_x + TELEPORT_DISTANCE);

            const int dest_camera_rel = camera_rel_x + TELEPORT_DISTANCE;
            if (dest_camera_rel > (PLAYFIELD_WIDTH / 2)) {
                const int max_camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
                const int camera_movement = dest_camera_rel - (PLAYFIELD_WIDTH / 2);
                if (camera_x + camera_movement <= max_camera_x) {
                    teleport_camera_counter = static_cast<uint8_t>(camera_movement);
                } else {
                    teleport_camera_counter = static_cast<uint8_t>(max_camera_x - camera_x);
                }
            }
        }
    }

    // Round destination to an even tile boundary and search down the column
    // for a solid tile with two empty tiles above (safe landing).
    dest_x &= 0xFE;

    bool solid_found = false;
    uint8_t search_y = static_cast<uint8_t>(PLAYFIELD_HEIGHT - 2);

    while (search_y > 0 && !solid_found) {
        if (is_tile_solid(get_tile_at(dest_x, search_y))) {
            uint8_t nonsolid_count = 0;
            uint8_t probe_y = search_y;

            while (probe_y > 0) {
                probe_y = static_cast<uint8_t>(probe_y - 2);

                if (is_tile_solid(get_tile_at(dest_x, probe_y))) {
                    break;
                }

                nonsolid_count++;
                if (nonsolid_count >= 2) {
                    dest_y = probe_y;
                    solid_found = true;
                    break;
                }
            }
        }

        if (!solid_found) {
            search_y = static_cast<uint8_t>(search_y - 2);
        }
    }

    if (!solid_found) {
        dest_x = static_cast<uint8_t>(comic_x);
        dest_y = static_cast<uint8_t>(comic_y);
        teleport_camera_counter = 0;
    }

    teleport_animation = 0;
    teleport_source_x = static_cast<uint8_t>(comic_x);
    teleport_source_y = static_cast<uint8_t>(comic_y);
    teleport_destination_x = dest_x;
    teleport_destination_y = dest_y;
    comic_is_teleporting = true;
    teleport_skip_tick = true;

    play_game_sound(GameSound::TELEPORT);
}

void process_teleport_input(bool comic_has_teleport_wand) {
    if (key_state_teleport && !previous_key_state_teleport && comic_has_teleport_wand) {
        begin_teleport();
    }

    previous_key_state_teleport = key_state_teleport;
}

void handle_teleport_tick() {
    if (!comic_is_teleporting) {
        return;
    }

    if (teleport_skip_tick) {
        teleport_skip_tick = false;
        return;
    }

    if (teleport_camera_counter > 0) {
        const int max_camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
        const int moved_camera_x = camera_x + teleport_camera_vel;
        camera_x = std::max(0, std::min(moved_camera_x, max_camera_x));
        teleport_camera_counter--;
    }

    teleport_animation++;

    apply_teleport_destination_if_ready(teleport_animation, teleport_destination_x,
                                        teleport_destination_y, comic_x, comic_y);

    if (teleport_animation >= 6) {
        comic_is_teleporting = false;
    }
}

static bool key_matches_binding(SDL_Keycode key, SDL_Keycode binding) {
    if (key == binding) {
        return true;
    }

    // Treat left/right modifier variants as one DOS-style key class.
    switch (binding) {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            return key == SDLK_LCTRL || key == SDLK_RCTRL;
        case SDLK_LALT:
        case SDLK_RALT:
            return key == SDLK_LALT || key == SDLK_RALT;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            return key == SDLK_LSHIFT || key == SDLK_RSHIFT;
        default:
            return false;
    }
}

int main(int argc, const char* argv[]) {
    // Parse command-line arguments
    bool debug_mode = false;
    bool skip_title = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
            std::cout << "Debug mode enabled" << std::endl;
        } else if (std::strcmp(argv[i], "--skip-title") == 0) {
            skip_title = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Captain Comic - Usage:" << std::endl;
            std::cout << "  --debug       Enable debug mode and cheat keys" << std::endl;
            std::cout << "  --skip-title  Skip the title sequence" << std::endl;
            std::cout << "  --help        Show this help message" << std::endl;
            return 0;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    auto cleanup_and_exit = [&](int code) {
        g_actor_system = nullptr;

        if (g_cheats) {
            delete g_cheats;
            g_cheats = nullptr;
        }

        cleanup_title_sequence();

        if (g_graphics) {
            delete g_graphics;
            g_graphics = nullptr;
        }

        shutdown_audio_system();

        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (window) {
            SDL_DestroyWindow(window);
        }

        SDL_Quit();
        return code;
    };

    window = SDL_CreateWindow("Captain Comic", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              640, 480, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return cleanup_and_exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return cleanup_and_exit(1);
    }

    // Initialize graphics system
    g_graphics = new GraphicsSystem(renderer);
    if (!g_graphics->initialize()) {
        std::cerr << "Graphics system initialization failed!" << std::endl;
        return cleanup_and_exit(1);
    }

    if (!initialize_audio_system()) {
        std::cerr << "Warning: Audio system initialization failed. Continuing without sound."
                  << std::endl;
    }

    // Load persisted key bindings if KEYS.DEF is present.
    load_input_bindings_from_file();

    // Run startup notice and title sequence.
    // Both are skipped with --skip-title for faster iteration during development.
    if (!skip_title) {
        if (!run_startup_notice(renderer, g_graphics)) {
            return cleanup_and_exit(0);
        }

        if (!run_title_sequence(renderer, g_graphics)) {
            // User quit during title sequence
            return cleanup_and_exit(0);
        }
    }

    // Initialize cheat system
    g_cheats = new CheatSystem();
    g_cheats->initialize(debug_mode);

    ActorSystem actor_system;
    actor_system.initialize();
    if (debug_mode) {
        actor_system.comic_firepower = 3;  // Start with 3 fireball slots in debug mode for testing
    }

    // Make actor system accessible to cheat system
    g_actor_system = &actor_system;

    // Initialize UI system
    UISystem ui_system;
    if (!ui_system.initialize()) {
        std::cerr << "Warning: UI system initialization failed. HUD will not display properly."
                  << std::endl;
    }
    // Pre-load player sprites and create animations
    const char* sprite_names[] = {"comic_standing", "comic_running_1", "comic_running_2",
                                  "comic_running_3", "comic_jumping"};
    const char* directions[] = {"right", "left"};

    for (const char* sprite : sprite_names) {
        const auto missing_dir =
            std::find_if(std::begin(directions), std::end(directions),
                         [&](const char* dir) { return !g_graphics->load_sprite(sprite, dir); });
        if (missing_dir != std::end(directions)) {
            std::cerr << "Failed to load sprite: " << sprite << " (" << *missing_dir << ")"
                      << std::endl;
            return cleanup_and_exit(1);
        }
    }

    for (int i = 0; i < 8; ++i) {
        std::string death_sprite = "comic_death_" + std::to_string(i);
        if (!g_graphics->load_sprite(death_sprite, "")) {
            std::cerr << "Failed to load sprite: " << death_sprite << std::endl;
            return cleanup_and_exit(1);
        }
    }

    std::array<Sprite*, 12> materialize_sprites = {};
    bool materialize_sprites_loaded = true;
    for (size_t i = 0; i < materialize_sprites.size(); ++i) {
        std::string materialize_name = "materialize_" + std::to_string(i);
        if (!g_graphics->load_sprite(materialize_name, "")) {
            std::cerr << "Warning: Could not load beam-in sprite: " << materialize_name
                      << std::endl;
            materialize_sprites_loaded = false;
        }
        materialize_sprites[i] = g_graphics->get_sprite(materialize_name, "");
        if (!materialize_sprites[i]) {
            materialize_sprites_loaded = false;
        }
    }

    if (!g_graphics->load_sprite("teleport_0", "") || !g_graphics->load_sprite("teleport_1", "") ||
        !g_graphics->load_sprite("teleport_2", "")) {
        std::cerr << "Warning: Could not load one or more teleport sprites." << std::endl;
    }

    std::array<Sprite*, 5> teleport_sprites = {
        g_graphics->get_sprite("teleport_0", ""), g_graphics->get_sprite("teleport_1", ""),
        g_graphics->get_sprite("teleport_2", ""), g_graphics->get_sprite("teleport_1", ""),
        g_graphics->get_sprite("teleport_0", "")};

    if (!g_graphics->load_sprite("pause", "")) {
        std::cerr << "Warning: Could not load pause sprite (sprite-pause.png)." << std::endl;
    }
    Sprite* pause_sprite = g_graphics->get_sprite("pause", "");

    if (!g_graphics->load_sprite("game_over", "")) {
        std::cerr << "Warning: Could not load game-over sprite (sprite-game_over.png)."
                  << std::endl;
    }
    Sprite* game_over_sprite = g_graphics->get_sprite("game_over", "");

    // Load fireball sprites
    if (!actor_system.load_fireball_sprites(g_graphics)) {
        std::cerr << "Warning: Could not load fireball sprites. Fireballs will not render."
                  << std::endl;
    }

    if (!actor_system.load_effect_sprites(g_graphics)) {
        std::cerr << "Warning: Could not load one or more enemy spark sprites." << std::endl;
    }

    // Create animations
    comic_idle_right = g_graphics->create_animation({"comic_standing"}, "right", 100, true);
    comic_idle_left = g_graphics->create_animation({"comic_standing"}, "left", 100, true);
    comic_run_right = g_graphics->create_animation(
        {"comic_running_1", "comic_running_2", "comic_running_3"}, "right", 100, true);
    comic_run_left = g_graphics->create_animation(
        {"comic_running_1", "comic_running_2", "comic_running_3"}, "left", 100, true);
    comic_jump_right = g_graphics->create_animation({"comic_jumping"}, "right", 100, true);
    comic_jump_left = g_graphics->create_animation({"comic_jumping"}, "left", 100, true);
    comic_death = g_graphics->create_animation(
        {"comic_death_0", "comic_death_1", "comic_death_2", "comic_death_3", "comic_death_4",
         "comic_death_5", "comic_death_6", "comic_death_7"},
        "", 110, false);

    current_animation = &comic_idle_right;

    bool quit = false;
    GameState game_state = GameState::Playing;
    bool pause_waiting_for_escape_release = false;
    bool beam_out_sequence_played = false;
    uint8_t win_counter = 0;
    SDL_Event e;

    // Initialize all level data (tile data is compiled-in as hex arrays)
    initialize_level_data();

    // Load the first playable level (FOREST = level 1, stage 0)
    // Level numbers: 0=LAKE, 1=FOREST, 2=SPACE, 3=BASE, 4=CAVE, 5=SHED, 6=CASTLE, 7=COMP
    current_level_number = LEVEL_NUMBER_FOREST;  // Forest is the first playable level
    current_stage_number = 0;
    source_door_level_number = -1;  // Not entering via door

    // Set initial spawn position
    comic_x = 14;
    comic_y = 12;
    comic_y_vel = 0;

    // Load the level and stage
    load_new_level();

    if (!current_level_ptr) {
        std::cerr << "Failed to load game level. Falling back to test level." << std::endl;
        init_test_level();  // Fall back to test level if loading fails
    }

    // Cache for tileset to avoid per-frame lookups
    uint8_t cached_level_number = current_level_number;
    uint8_t cached_stage_number = current_stage_number;
    Tileset* cached_tileset = nullptr;
    if (current_level_number < 8) {
        cached_tileset = g_graphics->get_tileset(level_names[current_level_number]);
    }

    if (current_level_ptr) {
        actor_system.setup_enemies_for_stage(current_level_ptr, current_level_number,
                                             current_stage_number, g_graphics);
    }

    // Load item sprites
    if (!actor_system.load_item_sprites(g_graphics)) {
        std::cerr << "Warning: Some item sprites failed to load" << std::endl;
    }

    // Tick timing - match original game's ~9.1 Hz tick rate.
    // The PC timer interrupt (IRQ 0) fires at 18.2065 Hz but game_tick_flag is
    // only set on *odd* interrupt cycles (irq0_parity toggles 0,1,0,1,...),
    // so the actual game logic runs at half the interrupt rate: ~9.1032 Hz.
    // Based on analysis of the original DOS timer interrupt handler
    // (original disassembly is not vendored in this repository).
    constexpr double TICK_RATE = 18.2065 / 2.0;         // ~9.1032 Hz - actual game tick rate
    constexpr double MS_PER_TICK = 1000.0 / TICK_RATE;  // ~109.86 ms per tick
    constexpr int MAX_TICKS_PER_FRAME = 5;
    constexpr double MAX_ACCUMULATED_MS = MS_PER_TICK * MAX_TICKS_PER_FRAME;
    uint32_t last_tick_time = SDL_GetTicks();
    double tick_accumulator = 0.0;

    // Keep special sequences on the same cadence as gameplay logic for
    // faithful pacing in this implementation.
    constexpr double ANIMATION_TICK_MS = MS_PER_TICK;
    auto wait_animation_ticks = [&](int ticks) -> bool {
        const uint32_t duration_ms = static_cast<uint32_t>(ANIMATION_TICK_MS * ticks + 0.5);
        uint32_t start = SDL_GetTicks();
        while (SDL_GetTicks() - start < duration_ms) {
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                    return false;
                }
            }
            SDL_Delay(1);
        }
        return true;
    };

    auto render_beam_in_frame = [&](bool show_comic, Sprite* materialize_sprite,
                                    bool present_frame = true) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_Rect gameplay_frame_rect = g_graphics->compute_letterbox_rect(renderer);

        SDL_Texture* hud_texture = get_hud_texture();
        if (hud_texture) {
            SDL_RenderCopy(renderer, hud_texture, nullptr, &gameplay_frame_rect);
        }

        const float letterbox_scale = static_cast<float>(gameplay_frame_rect.w) / EGA_WIDTH;
        const int render_scale =
            (letterbox_scale < 0.125f) ? 1 : static_cast<int>(8.0f * letterbox_scale + 0.5f);

        SDL_Rect playfield_viewport;
        playfield_viewport.x = gameplay_frame_rect.x + static_cast<int>(8 * letterbox_scale);
        playfield_viewport.y = gameplay_frame_rect.y + static_cast<int>(8 * letterbox_scale);
        playfield_viewport.w = render_scale * PLAYFIELD_WIDTH;
        playfield_viewport.h = render_scale * PLAYFIELD_HEIGHT;
        SDL_RenderSetViewport(renderer, &playfield_viewport);

        const int OFFSCREEN_MARGIN_UNITS = 2;
        const int min_visible_x = camera_x - OFFSCREEN_MARGIN_UNITS;
        const int max_visible_x = camera_x + PLAYFIELD_WIDTH + OFFSCREEN_MARGIN_UNITS;

        for (int ty = 0; ty < MAP_HEIGHT_TILES; ty++) {
            for (int tx = 0; tx < MAP_WIDTH_TILES; tx++) {
                int world_x = tx * 2;
                if (world_x + 2 > min_visible_x && world_x < max_visible_x) {
                    uint8_t tile = get_tile_at(tx * 2, ty * 2);
                    int screen_x = (world_x - camera_x) * render_scale;
                    int screen_y = ty * 2 * render_scale;
                    g_graphics->render_tile(screen_x, screen_y, cached_tileset, tile, render_scale);
                }
            }
        }

        actor_system.render_item(g_graphics, camera_x, render_scale);

        const int comic_screen_x = (comic_x - camera_x) * render_scale + render_scale;
        const int comic_screen_y = comic_y * render_scale + render_scale * 2;
        const int comic_width = render_scale * 2;
        const int comic_height = render_scale * 4;

        if (show_comic) {
            const AnimationFrame* frame = g_graphics->get_current_frame(*current_animation);
            if (frame) {
                g_graphics->render_sprite_centered_scaled(comic_screen_x, comic_screen_y,
                                                          frame->sprite, comic_width, comic_height);
            }
        }

        if (materialize_sprite) {
            g_graphics->render_sprite_centered_scaled(
                comic_screen_x, comic_screen_y, *materialize_sprite, comic_width, comic_height);
        }

        SDL_RenderSetViewport(renderer, &gameplay_frame_rect);
        SDL_RenderSetScale(renderer, letterbox_scale, letterbox_scale);
        ui_system.render_hud(score_bytes, comic_num_lives, comic_hp, actor_system.fireball_meter,
                             actor_system.comic_firepower, actor_system.comic_has_corkscrew != 0,
                             comic_has_door_key != 0, actor_system.comic_has_teleport_wand != 0,
                             actor_system.comic_has_lantern != 0, actor_system.comic_has_gems != 0,
                             actor_system.comic_has_crown != 0, actor_system.comic_has_gold != 0,
                             comic_jump_power);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_RenderSetViewport(renderer, nullptr);

        if (present_frame) {
            SDL_RenderPresent(renderer);
        }
    };

    auto play_beam_out_sequence = [&]() {
        if (!materialize_sprites_loaded || quit) {
            return;
        }

        play_game_sound(GameSound::MATERIALIZE);

        render_beam_in_frame(true, nullptr);
        if (!wait_animation_ticks(1)) {
            return;
        }

        for (size_t frame = materialize_sprites.size(); frame > 0; --frame) {
            const size_t sprite_index = frame - 1;
            const bool show_comic = sprite_index >= 6;
            render_beam_in_frame(show_comic, materialize_sprites[sprite_index]);
            if (!wait_animation_ticks(1)) {
                return;
            }
        }

        render_beam_in_frame(false, nullptr);
        if (!wait_animation_ticks(6)) {
            return;
        }
    };

    auto render_fullscreen_texture = [&](SDL_Texture* texture) {
        if (!texture) {
            return;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_Rect dst = g_graphics->compute_letterbox_rect(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);
    };

    auto load_fullscreen_texture = [&](const char* filename) -> SDL_Texture* {
        const std::string path = g_graphics->get_asset_path(filename);
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) {
            std::cerr << "Victory sequence: failed to load " << filename << " (" << IMG_GetError()
                      << ")" << std::endl;
            return nullptr;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) {
            std::cerr << "Victory sequence: failed to create texture for " << filename << " ("
                      << SDL_GetError() << ")" << std::endl;
            return nullptr;
        }
        return texture;
    };

    auto wait_for_new_keypress = [&]() -> bool {
        // Drain queued input so the player must press a fresh key, matching DOS flow.
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
                return false;
            }
        }

        while (!quit) {
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                    return false;
                }
                if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                    return true;
                }
            }
            SDL_Delay(1);
        }

        return false;
    };

    auto play_victory_sequence = [&]() {
        play_beam_out_sequence();
        if (quit) {
            return;
        }

        clear_gameplay_key_states();

        // Base victory bonus: 20,000 points as twenty 1,000-point tally steps.
        for (int step = 0; step < 20; ++step) {
            play_game_sound(GameSound::ITEM_COLLECT);
            award_points(10);
            render_beam_in_frame(false, nullptr);
            if (!wait_animation_ticks(1)) {
                break;
            }
        }

        // Remaining lives bonus: 10,000 points per life, then decrement one life icon.
        while (comic_num_lives > 0) {
            for (int step = 0; step < 10; ++step) {
                play_game_sound(GameSound::ITEM_COLLECT);
                award_points(10);
                render_beam_in_frame(false, nullptr);
                if (!wait_animation_ticks(1)) {
                    break;
                }
            }

            comic_num_lives--;
            render_beam_in_frame(false, nullptr);
            if (!wait_animation_ticks(3)) {
                break;
            }
        }

        play_game_music(GameMusic::TITLE);

        SDL_Texture* victory_texture = load_fullscreen_texture("sys002.ega.png");
        if (victory_texture) {
            render_fullscreen_texture(victory_texture);
            wait_for_new_keypress();
        }

        stop_game_music();

        if (!quit) {
            render_beam_in_frame(false, nullptr, false);

            if (game_over_sprite) {
                SDL_Rect gameplay_frame_rect = g_graphics->compute_letterbox_rect(renderer);
                const float letterbox_scale = static_cast<float>(gameplay_frame_rect.w) / EGA_WIDTH;

                const int game_over_x_ega = 40;
                const int game_over_y_ega = 64;
                const int game_over_width_ega = 128;
                const int game_over_height_ega = 48;

                SDL_Rect game_over_rect = {
                    gameplay_frame_rect.x + static_cast<int>(game_over_x_ega * letterbox_scale),
                    gameplay_frame_rect.y + static_cast<int>(game_over_y_ega * letterbox_scale),
                    static_cast<int>(game_over_width_ega * letterbox_scale),
                    static_cast<int>(game_over_height_ega * letterbox_scale)};

                SDL_RenderCopy(renderer, game_over_sprite->texture.texture, nullptr,
                               &game_over_rect);
                SDL_RenderPresent(renderer);
            }

            wait_for_new_keypress();
            if (!run_high_scores_screen(renderer, g_graphics, score_bytes)) {
                quit = true;
            }
        }

        if (victory_texture) {
            SDL_DestroyTexture(victory_texture);
        }
    };

    auto play_game_over_sequence = [&]() {
        clear_gameplay_key_states();
        game_state = GameState::Playing;
        pause_waiting_for_escape_release = false;

        render_beam_in_frame(false, nullptr, false);

        if (game_over_sprite && !quit) {
            SDL_Rect gameplay_frame_rect = g_graphics->compute_letterbox_rect(renderer);
            const float letterbox_scale = static_cast<float>(gameplay_frame_rect.w) / EGA_WIDTH;

            const int game_over_x_ega = 40;
            const int game_over_y_ega = 64;
            const int game_over_width_ega = 128;
            const int game_over_height_ega = 48;

            SDL_Rect game_over_rect = {
                gameplay_frame_rect.x + static_cast<int>(game_over_x_ega * letterbox_scale),
                gameplay_frame_rect.y + static_cast<int>(game_over_y_ega * letterbox_scale),
                static_cast<int>(game_over_width_ega * letterbox_scale),
                static_cast<int>(game_over_height_ega * letterbox_scale)};
            SDL_RenderCopy(renderer, game_over_sprite->texture.texture, nullptr, &game_over_rect);
            SDL_RenderPresent(renderer);
            play_game_sound(GameSound::GAME_OVER);
        }

        if (!quit) {
            wait_animation_ticks(1);
            wait_for_new_keypress();
        }
        if (!quit && !run_high_scores_screen(renderer, g_graphics, score_bytes)) {
            quit = true;
        }
    };

    if (materialize_sprites_loaded) {
        for (int frame = 0; frame < 15; ++frame) {
            render_beam_in_frame(false, nullptr);
            if (!wait_animation_ticks(1)) {
                break;
            }
        }

        if (!quit) {
            play_game_sound(GameSound::MATERIALIZE);

            for (size_t frame = 0; frame < materialize_sprites.size(); ++frame) {
                bool show_comic = frame >= 6;
                render_beam_in_frame(show_comic, materialize_sprites[frame]);
                if (!wait_animation_ticks(1)) {
                    break;
                }
            }

            render_beam_in_frame(true, nullptr);
            wait_animation_ticks(1);
        }

        clear_gameplay_key_states();
    }

    // Persists between render frames; updated each tick when movement is attempted.
    bool player_moved_last_tick = false;
    // True while airborne due to walking off an edge (not a jump).
    bool player_airborne_from_walk_off = false;

    while (!quit) {
        uint32_t current_time = SDL_GetTicks();
        uint32_t delta_time = current_time - last_tick_time;
        last_tick_time = current_time;
        tick_accumulator += delta_time;
        bool suppress_jump_animation_this_frame = false;
        if (tick_accumulator > MAX_ACCUMULATED_MS) {
            tick_accumulator = MAX_ACCUMULATED_MS;
        }

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                const InputBindings& bindings = get_input_bindings();
                const SDL_Keycode key = e.key.keysym.sym;

                if (game_state == GameState::Paused) {
                    if (key == SDLK_ESCAPE && e.key.repeat == 0 &&
                        pause_waiting_for_escape_release) {
                        continue;
                    }

                    if (pause_waiting_for_escape_release || e.key.repeat != 0) {
                        continue;
                    }

                    if (key == SDLK_q) {
                        quit = true;
                    }

                    game_state = GameState::Playing;
                    clear_gameplay_key_states();
                    tick_accumulator = 0.0;
                    continue;
                }

                if (key == SDLK_ESCAPE && e.key.repeat == 0) {
                    game_state = GameState::Paused;
                    pause_waiting_for_escape_release = true;
                    clear_gameplay_key_states();
                    tick_accumulator = 0.0;
                    continue;
                }

                // Process regular gameplay keys

                if (key_matches_binding(key, bindings.move_left)) {
                    key_state_left = 1;
                }
                if (key_matches_binding(key, bindings.move_right)) {
                    key_state_right = 1;
                }
                if (key_matches_binding(key, bindings.jump)) {
                    key_state_jump = 1;
                }
                if (key_matches_binding(key, bindings.fire)) {
                    key_state_fire = 1;
                }
                if (key_matches_binding(key, bindings.open_door)) {
                    key_state_open = 1;
                }
                if (key_matches_binding(key, bindings.teleport)) {
                    key_state_teleport = 1;
                }

                // F5 — toggle Enhanced Smooth / Classic renderer (edge-triggered, any game state)
                if (key == SDLK_F5 && e.key.repeat == 0) {
                    g_graphics->toggle_render_mode();
                    std::cout << "[Renderer] Switched to: " << g_graphics->render_mode_name()
                              << std::endl;
                }

                // Process cheat keys (only active if --debug flag set)
                g_cheats->process_input(e.key.keysym.sym);
            } else if (e.type == SDL_KEYUP) {
                const InputBindings& bindings = get_input_bindings();
                const SDL_Keycode key = e.key.keysym.sym;

                if (game_state == GameState::Paused) {
                    if (key == SDLK_ESCAPE) {
                        pause_waiting_for_escape_release = false;
                    }
                    continue;
                }

                if (key_matches_binding(key, bindings.move_left)) {
                    key_state_left = 0;
                }
                if (key_matches_binding(key, bindings.move_right)) {
                    key_state_right = 0;
                }
                if (key_matches_binding(key, bindings.jump)) {
                    key_state_jump = 0;
                }
                if (key_matches_binding(key, bindings.fire)) {
                    key_state_fire = 0;
                }
                if (key_matches_binding(key, bindings.open_door)) {
                    key_state_open = 0;
                }
                if (key_matches_binding(key, bindings.teleport)) {
                    key_state_teleport = 0;
                }
            }
        }

        // Process physics ticks at ~9.1 Hz (original game speed)
        // This decouples physics from rendering rate
        if (game_state == GameState::Playing) {
            int ticks_processed = 0;
            while (tick_accumulator >= MS_PER_TICK && ticks_processed < MAX_TICKS_PER_FRAME) {
                tick_accumulator -= MS_PER_TICK;
                ticks_processed++;

                // Per-tick movement result must be cleared before any early-continue
                // branches (death, door anim, teleport) to avoid stale run state.
                player_moved_last_tick = false;

                // Phase 5: advance run cycle unconditionally every tick before any
                // early-return branches, matching assembly .tick behavior.
                // Derive modulus from the actual run animation frame count so the
                // counter stays consistent if the animation data ever changes.
                {
                    const int run_frame_count = static_cast<int>(comic_run_right.frames.size());
                    if (run_frame_count > 0) {
                        comic_run_cycle_frame = (comic_run_cycle_frame + 1) % run_frame_count;
                    }
                }

                if (is_player_dying()) {
                    update_player_death_sequence();
                    ui_system.update();

                    if (game_over_triggered) {
                        game_state = GameState::GameOver;
                        break;
                    }
                    continue;
                }

                if (g_door_anim_phase != DoorAnimationPhase::NONE) {
                    update_door_animation_tick();
                    ui_system.update();
                    continue;
                }

                // Process jump input once per tick (edge-triggered)
                // Note: Jump input feeds comic_is_falling_or_jumping, so must be before physics
                process_jump_input();

                // Handle ongoing teleport animation (continues to next tick if active)
                if (comic_is_teleporting) {
                    handle_teleport_tick();

                    const uint8_t* tiles =
                        current_level_ptr ? current_level_ptr->stages[current_stage_number].tiles
                                          : nullptr;
                    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x,
                                        key_state_fire);
                    ui_system.update();
                    continue;
                }

                // Update jump power from item system (boots affect jump height)
                comic_jump_power = static_cast<uint8_t>(actor_system.get_jump_power());

                // Update physics (once per tick)
                const uint8_t was_falling_or_jumping = comic_is_falling_or_jumping;
                handle_fall_or_jump();

                // Detect landing this tick: was airborne, now grounded
                // Assembly: on landing, jmp game_loop.check_pause_input skips ALL
                // left/right movement AND the floor walk-off check for that tick.
                const bool just_landed =
                    (was_falling_or_jumping != 0) && (comic_is_falling_or_jumping == 0);
                if (just_landed) {
                    player_airborne_from_walk_off = false;
                }

                // If physics transitioned from grounded to airborne using the
                // no-floor path, suppress jump art for this render frame.
                if (!was_falling_or_jumping && comic_is_falling_or_jumping &&
                    comic_jump_counter == 1 && comic_y_vel == 8) {
                    suppress_jump_animation_this_frame = true;
                    player_airborne_from_walk_off = true;
                }

                // Ground movement (only when not in air AND did not just land this tick)
                // Skipping on landing matches assembly: landing jumps past the left/right block
                if (!comic_is_falling_or_jumping && !just_landed) {
                    if (key_state_left) {
                        player_moved_last_tick |= move_left();
                    }
                    if (key_state_right) {
                        player_moved_last_tick |= move_right();
                    }

                    // Match original game-loop floor check ordering: after
                    // horizontal movement, detect missing floor and begin
                    // falling immediately (no extra standing tick).
                    if (!comic_is_falling_or_jumping) {
                        const uint8_t foot_y = static_cast<uint8_t>(comic_y + 4);
                        uint8_t foot_tile = get_tile_at(static_cast<uint8_t>(comic_x), foot_y);
                        bool foot_solid = is_tile_solid(foot_tile);

                        if (!foot_solid && (comic_x & 1)) {
                            foot_tile = get_tile_at(static_cast<uint8_t>(comic_x + 1), foot_y);
                            foot_solid = is_tile_solid(foot_tile);
                        }

                        if (!foot_solid) {
                            comic_y_vel = 8;

                            if (comic_x_momentum > 0) {
                                comic_x_momentum = 2;
                            } else if (comic_x_momentum < 0) {
                                comic_x_momentum = -2;
                            } else if (key_state_right && !key_state_left) {
                                comic_x_momentum = 2;
                            } else if (key_state_left && !key_state_right) {
                                comic_x_momentum = -2;
                            }

                            comic_is_falling_or_jumping = 1;
                            comic_jump_counter = 1;
                            suppress_jump_animation_this_frame = true;
                            player_airborne_from_walk_off = true;
                        }
                    }
                }

                const uint8_t* tiles = current_level_ptr
                                           ? current_level_ptr->stages[current_stage_number].tiles
                                           : nullptr;
                actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x,
                                    key_state_fire);
                ui_system.update();

                // ========== PHASE 2: Door and Teleport Checks (After Physics/Actors) ==========
                // Assembly order: check doors, then teleports, after physics has resolved position
                // Process door input once per tick (edge-triggered)
                // Door activation happens AFTER physics resolves, not before
                process_door_input();

                // Process teleport input once per tick (edge-triggered)
                // Teleport activation happens AFTER physics resolves, not before
                if (!comic_is_falling_or_jumping && !comic_is_teleporting) {
                    process_teleport_input(actor_system.comic_has_teleport_wand != 0);
                } else {
                    previous_key_state_teleport = key_state_teleport;
                }

                if (!beam_out_sequence_played && actor_system.comic_num_treasures >= 3) {
                    beam_out_sequence_played = true;
                    win_counter = 20;
                }

                if (beam_out_sequence_played && win_counter > 0) {
                    win_counter--;
                    if (win_counter == 1) {
                        clear_gameplay_key_states();
                        tick_accumulator = 0.0;
                        game_state = GameState::Victory;
                        break;
                    }
                }

                // Lives count-up sequence: award 5 lives with 1-tick delay between each,
                // then subtract 1 (the life currently in use)
                if (!lives_sequence_complete) {
                    if (lives_sequence_counter > 0) {
                        lives_sequence_delay--;
                        if (lives_sequence_delay == 0) {
                            // Award a life
                            comic_num_lives++;
                            lives_sequence_counter--;

                            if (lives_sequence_counter > 0) {
                                // Still more lives to award (wait 1 tick)
                                lives_sequence_delay = 1;
                            } else {
                                // Awarded all 5, wait 3 ticks then subtract 1
                                lives_sequence_delay = 3;
                            }
                        }
                    } else if (lives_sequence_delay > 0) {
                        // Waiting 3 ticks after awarding all 5 lives
                        lives_sequence_delay--;
                        if (lives_sequence_delay == 0) {
                            // Subtract 1 life (currently in use: 5→4)
                            if (comic_num_lives > 0) {
                                comic_num_lives--;
                            }
                            lives_sequence_complete = true;
                        }
                    }
                }

                // Gradually fill HP from 0 to MAX_HP at game startup
                // Each tick, increment HP if pending increase is scheduled
                if (comic_hp_pending_increase > 0) {
                    comic_hp_pending_increase--;
                    if (comic_hp < MAX_HP) {
                        comic_hp++;
                    }
                }

                // Fireball meter charging is handled by ActorSystem::update()
                // (charges at 1 unit per 2 ticks when not firing)

                // Game-over is handled by physics when Comic hits the bottom of playfield
                // (sound triggered there).  No additional check needed here.

                // Snapshot positions for smooth-renderer interpolation: these are sampled
                // at the *end* of the tick so the next rendered frame can lerp between
                // prev (last tick) and current (this tick).
                prev_camera_x = static_cast<float>(camera_x);
                prev_comic_x = static_cast<float>(comic_x);
                prev_comic_y = static_cast<float>(comic_y);
            }
        } else {
            tick_accumulator = 0.0;
        }

        if (game_state == GameState::Victory) {
            play_victory_sequence();
            game_state = GameState::Exiting;
            quit = true;
        } else if (game_state == GameState::GameOver) {
            play_game_over_sequence();
            game_state = GameState::Exiting;
            quit = true;
        }

        if (quit) {
            break;
        }

        // Update animation based on state (updates every frame for smooth animation)
        if (game_state == GameState::Playing) {
            current_time = SDL_GetTicks();
            const Animation* previous_animation = current_animation;
            if (is_player_dying()) {
                if (should_show_player_death_animation()) {
                    current_animation = &comic_death;
                } else if (should_clip_player_death_render()) {
                    current_animation = comic_facing ? &comic_jump_right : &comic_jump_left;
                } else {
                    current_animation = nullptr;
                }
            } else if (comic_is_falling_or_jumping && !suppress_jump_animation_this_frame &&
                       !player_airborne_from_walk_off) {
                current_animation = comic_facing ? &comic_jump_right : &comic_jump_left;
            } else {
                if (player_moved_last_tick) {
                    current_animation = comic_facing ? &comic_run_right : &comic_run_left;
                } else {
                    current_animation = comic_facing ? &comic_idle_right : &comic_idle_left;
                }
            }

            // Reset animation state when switching to a new animation
            if (current_animation && current_animation != previous_animation) {
                current_animation->current_frame = 0;
                current_animation->frame_start_time = current_time;
            }

            if (current_animation) {
                const bool is_run_anim =
                    (current_animation == &comic_run_right || current_animation == &comic_run_left);
                if (is_run_anim) {
                    // Phase 5: run animation advances exactly once per game tick via
                    // comic_run_cycle_frame, not by wall-clock comparison.
                    current_animation->current_frame = comic_run_cycle_frame;
                } else {
                    g_graphics->update_animation(*current_animation, current_time);
                }
            }
        }

        // Clear screen with black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Keep gameplay aligned with the same letterboxed 320x200 frame used by HUD.
        SDL_Rect gameplay_frame_rect = g_graphics->compute_letterbox_rect(renderer);

        // Render HUD background (sys003.ega) - should be behind all game elements
        SDL_Texture* hud_texture = get_hud_texture();
        if (hud_texture) {
            SDL_RenderCopy(renderer, hud_texture, nullptr, &gameplay_frame_rect);
        }

        // Calculate the letterbox scale factor to keep playfield dimensions correct
        // as the window size changes.
        const float letterbox_scale = static_cast<float>(gameplay_frame_rect.w) / EGA_WIDTH;

        // Derive per-frame render scale from letterbox scale.
        // Each game unit = 8 EGA pixels (192 EGA pixels / 24 game units),
        // so gameplay scales consistently with the HUD.
        // Round to nearest int and clamp to minimum 1 to prevent zero-sized rendering.
        const int render_scale =
            (letterbox_scale < 0.125f) ? 1 : static_cast<int>(8.0f * letterbox_scale + 0.5f);

        // Render gameplay only inside the HUD playfield window
        // (C code: top-left at 8,8 EGA pixels; size 192x160 EGA pixels).
        SDL_Rect playfield_viewport;
        playfield_viewport.x = gameplay_frame_rect.x + static_cast<int>(8 * letterbox_scale);
        playfield_viewport.y = gameplay_frame_rect.y + static_cast<int>(8 * letterbox_scale);
        playfield_viewport.w = render_scale * PLAYFIELD_WIDTH;
        playfield_viewport.h = render_scale * PLAYFIELD_HEIGHT;
        SDL_RenderSetViewport(renderer, &playfield_viewport);

        bool level_changed = current_level_number != cached_level_number;
        bool stage_changed = current_stage_number != cached_stage_number;

        // Update tileset cache if level changed
        if (level_changed) {
            cached_level_number = current_level_number;
            if (current_level_number < 8) {
                cached_tileset = g_graphics->get_tileset(level_names[current_level_number]);
            }
        }

        if (level_changed || stage_changed) {
            cached_stage_number = current_stage_number;
            if (current_level_ptr) {
                actor_system.setup_enemies_for_stage(current_level_ptr, current_level_number,
                                                     current_stage_number, g_graphics);
            }
        }

        // Floating-point render scale for the enhanced smooth renderer.
        // Using the exact scale factor avoids the rounding error that causes
        // tile-boundary snapping in the classic integer path.
        const float render_scale_f = (letterbox_scale < 0.125f) ? 1.0f : 8.0f * letterbox_scale;

        // -----------------------------------------------------------------
        // Enhanced Smooth Renderer: compute interpolation alpha.
        // alpha = fraction of the current physics tick that has elapsed,
        // in [0, 1].  At alpha=0 we show the previous tick state; at alpha=1
        // we show the fully-advanced current tick state.  Values in between
        // give smooth sub-tick positions at any monitor refresh rate.
        // -----------------------------------------------------------------
        const bool smooth_mode =
            (g_graphics->get_render_mode() == GraphicsSystem::RenderMode::EnhancedSmooth);
        const float alpha =
            smooth_mode ? std::clamp(static_cast<float>(tick_accumulator / MS_PER_TICK), 0.0f, 1.0f)
                        : 1.0f;  // Classic mode: always snap to current-tick position

        // Interpolated render positions (identical to integer positions in classic mode).
        const float render_camera_x =
            smooth_mode ? (prev_camera_x + alpha * (static_cast<float>(camera_x) - prev_camera_x))
                        : static_cast<float>(camera_x);
        const float render_comic_x =
            smooth_mode ? (prev_comic_x + alpha * (static_cast<float>(comic_x) - prev_comic_x))
                        : static_cast<float>(comic_x);
        const float render_comic_y =
            smooth_mode ? (prev_comic_y + alpha * (static_cast<float>(comic_y) - prev_comic_y))
                        : static_cast<float>(comic_y);

        Tileset* tileset = cached_tileset;

        // Render tiles with offscreen margin for seamless scrolling.
        // The C code pre-renders into a 256-byte-wide buffer; we render tiles
        // slightly outside the visible playfield and let viewport clipping
        // hide any offscreen portions.
        const int OFFSCREEN_MARGIN_UNITS = 2;
        const int min_visible_x = static_cast<int>(render_camera_x) - OFFSCREEN_MARGIN_UNITS;
        const int max_visible_x =
            static_cast<int>(render_camera_x) + PLAYFIELD_WIDTH + OFFSCREEN_MARGIN_UNITS;

        for (int ty = 0; ty < MAP_HEIGHT_TILES; ty++) {
            for (int tx = 0; tx < MAP_WIDTH_TILES; tx++) {
                const int world_x = tx * 2;  // Tile x in game units

                // Render tiles within visible range (with left margin for scrolling).
                // Each tile is 2 units wide, so render if tile overlaps the range.
                if (world_x + 2 > min_visible_x && world_x < max_visible_x) {
                    const uint8_t tile = get_tile_at(tx * 2, ty * 2);

                    if (smooth_mode) {
                        // Enhanced Smooth: floating-point screen position
                        const float screen_x =
                            (static_cast<float>(world_x) - render_camera_x) * render_scale_f;
                        const float screen_y = static_cast<float>(ty * 2) * render_scale_f;
                        g_graphics->render_tile_f(screen_x, screen_y, tileset, tile,
                                                  render_scale_f);
                    } else {
                        // Classic: integer screen position (original behaviour)
                        const int screen_x = (world_x - camera_x) * render_scale;
                        const int screen_y = ty * 2 * render_scale;
                        g_graphics->render_tile(screen_x, screen_y, tileset, tile, render_scale);
                    }
                }
            }
        }

        uint8_t door_world_x = 0;
        uint8_t door_world_y = 0;
        DoorAnimationRenderMode door_render_mode = DoorAnimationRenderMode::NONE;
        bool door_overlay_in_front = false;
        bool door_player_visible = true;
        const bool door_anim_active =
            get_door_animation_render_state(&door_world_x, &door_world_y, &door_render_mode,
                                            &door_overlay_in_front, &door_player_visible);

        auto render_door_animation_overlay = [&](bool draw_front_layer) {
            if (!door_anim_active || door_render_mode == DoorAnimationRenderMode::NONE) {
                return;
            }

            if (door_overlay_in_front != draw_front_layer) {
                return;
            }

            const int tile_w = render_scale * 2;
            const int tile_h = render_scale * 2;
            const int screen_x = (static_cast<int>(door_world_x) - camera_x) * render_scale;
            const int screen_y = static_cast<int>(door_world_y) * render_scale;

            const bool can_draw_tiles =
                tileset != nullptr && current_level_ptr != nullptr && tile_w >= 2;

            Uint8 prev_r = 0;
            Uint8 prev_g = 0;
            Uint8 prev_b = 0;
            Uint8 prev_a = 0;
            SDL_GetRenderDrawColor(renderer, &prev_r, &prev_g, &prev_b, &prev_a);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

            SDL_Rect door_rect = {screen_x, screen_y, tile_w * 2, tile_h * 2};
            SDL_RenderFillRect(renderer, &door_rect);

            if (can_draw_tiles) {
                const uint8_t tile_ul = current_level_ptr->door_tile_ul;
                const uint8_t tile_ur = current_level_ptr->door_tile_ur;
                const uint8_t tile_ll = current_level_ptr->door_tile_ll;
                const uint8_t tile_lr = current_level_ptr->door_tile_lr;

                auto redraw_world_tile = [&](int world_tile_x, int world_tile_y) {
                    if (world_tile_x < 0 || world_tile_y < 0) {
                        return;
                    }

                    if (world_tile_x >= MAP_WIDTH_TILES * 2 ||
                        world_tile_y >= MAP_HEIGHT_TILES * 2) {
                        return;
                    }

                    const int tile_screen_x = (world_tile_x - camera_x) * render_scale;
                    const int tile_screen_y = world_tile_y * render_scale;
                    const uint8_t tile_id = get_tile_at(world_tile_x, world_tile_y);
                    g_graphics->render_tile(tile_screen_x, tile_screen_y, tileset, tile_id,
                                            render_scale);
                };

                int shift = 0;
                if (door_render_mode == DoorAnimationRenderMode::HALF_OPEN ||
                    door_render_mode == DoorAnimationRenderMode::HALF_CLOSED) {
                    shift = tile_w / 4;
                } else if (door_render_mode == DoorAnimationRenderMode::FULL_OPEN) {
                    shift = tile_w / 2;
                }

                const int left_col_x = screen_x - shift;
                const int right_col_x = screen_x + tile_w + shift;

                // Each door leaf is one full tile column (2 game units wide).
                g_graphics->render_tile(left_col_x, screen_y, tileset, tile_ul, render_scale);
                g_graphics->render_tile(left_col_x, screen_y + tile_h, tileset, tile_ll,
                                        render_scale);
                g_graphics->render_tile(right_col_x, screen_y, tileset, tile_ur, render_scale);
                g_graphics->render_tile(right_col_x, screen_y + tile_h, tileset, tile_lr,
                                        render_scale);

                // Redraw the wall columns beside the doorway so the leaves slide
                // behind the surrounding tiles instead of overlapping them.
                redraw_world_tile(static_cast<int>(door_world_x) - 2,
                                  static_cast<int>(door_world_y));
                redraw_world_tile(static_cast<int>(door_world_x) - 2,
                                  static_cast<int>(door_world_y) + 2);
                redraw_world_tile(static_cast<int>(door_world_x) + 4,
                                  static_cast<int>(door_world_y));
                redraw_world_tile(static_cast<int>(door_world_x) + 4,
                                  static_cast<int>(door_world_y) + 2);
            }

            SDL_SetRenderDrawColor(renderer, prev_r, prev_g, prev_b, prev_a);
        };

        render_door_animation_overlay(false);

        actor_system.render_enemies(g_graphics, camera_x, render_scale);
        actor_system.render_fireballs(g_graphics, camera_x, render_scale);

        // Render player sprite
        if (current_animation && door_player_visible) {
            const AnimationFrame* frame = g_graphics->get_current_frame(*current_animation);
            if (frame) {
                if (smooth_mode) {
                    // Enhanced Smooth: sub-pixel interpolated player position
                    const float screen_x =
                        (render_comic_x - render_camera_x) * render_scale_f + render_scale_f;
                    const float screen_y = render_comic_y * render_scale_f + render_scale_f * 2.0f;
                    const float player_width = render_scale_f * 2.0f;
                    const float player_full_height = render_scale_f * 4.0f;
                    if (should_clip_player_death_render()) {
                        const float sprite_top_y = screen_y - player_full_height * 0.5f;
                        const float clip_h =
                            std::clamp(static_cast<float>(playfield_viewport.h) - sprite_top_y,
                                       0.0f, player_full_height);
                        if (clip_h > 0.0f) {
                            g_graphics->render_sprite_top_clip_scaled_f(screen_x, screen_y,
                                                                        frame->sprite, player_width,
                                                                        player_full_height, clip_h);
                        }
                    } else {
                        g_graphics->render_sprite_centered_scaled_f(
                            screen_x, screen_y, frame->sprite, player_width, player_full_height);
                    }
                } else {
                    // Classic: integer screen position (original behaviour)
                    const int screen_x = (comic_x - camera_x) * render_scale + render_scale;
                    const int screen_y = comic_y * render_scale + render_scale * 2;
                    const int player_width = render_scale * 2;
                    const int player_full_height = render_scale * 4;
                    if (should_clip_player_death_render()) {
                        const int sprite_top_y = screen_y - player_full_height / 2;
                        const int clip_h =
                            std::clamp(playfield_viewport.h - sprite_top_y, 0, player_full_height);
                        if (clip_h > 0) {
                            g_graphics->render_sprite_top_clip_scaled(screen_x, screen_y,
                                                                      frame->sprite, player_width,
                                                                      player_full_height, clip_h);
                        }
                    } else {
                        g_graphics->render_sprite_centered_scaled(screen_x, screen_y, frame->sprite,
                                                                  player_width, player_full_height);
                    }
                }
            }
        }

        render_door_animation_overlay(true);

        if (comic_is_teleporting) {
            const uint8_t last_teleport_frame = static_cast<uint8_t>(teleport_sprites.size() - 1);
            const uint8_t source_frame = std::min(teleport_animation, last_teleport_frame);
            if (teleport_sprites[source_frame]) {
                int source_screen_x =
                    (static_cast<int>(teleport_source_x) - camera_x) * render_scale + render_scale;
                int source_screen_y =
                    static_cast<int>(teleport_source_y) * render_scale + render_scale * 2;
                g_graphics->render_sprite_centered_scaled(source_screen_x, source_screen_y,
                                                          *teleport_sprites[source_frame],
                                                          render_scale * 2, render_scale * 4);
            }

            if (teleport_animation >= 1) {
                const uint8_t destination_phase = static_cast<uint8_t>(teleport_animation - 1);
                const uint8_t dest_frame = std::min(destination_phase, last_teleport_frame);
                if (teleport_sprites[dest_frame]) {
                    int destination_screen_x =
                        (static_cast<int>(teleport_destination_x) - camera_x) * render_scale +
                        render_scale;
                    int destination_screen_y =
                        static_cast<int>(teleport_destination_y) * render_scale + render_scale * 2;
                    g_graphics->render_sprite_centered_scaled(
                        destination_screen_x, destination_screen_y, *teleport_sprites[dest_frame],
                        render_scale * 2, render_scale * 4);
                }
            }
        }

        // Assembly-faithful order: items are rendered after Comic, so with painter's
        // algorithm they appear on top when overlapping.
        actor_system.render_item(g_graphics, camera_x, render_scale);

        // Restore full renderer viewport before rendering debug overlay.
        SDL_RenderSetViewport(renderer, nullptr);

        // Render debug overlay if enabled via F3 (in full window space)
        if (g_cheats->should_show_debug_overlay()) {
            g_graphics->render_debug_overlay();
        }

        // Render HUD (score, lives, HP, fireball meter, inventory)
        // Draw in the same 320x200 letterboxed coordinate space as SYS003.
        SDL_RenderSetViewport(renderer, &gameplay_frame_rect);
        SDL_RenderSetScale(renderer, letterbox_scale, letterbox_scale);
        ui_system.render_hud(score_bytes, comic_num_lives, comic_hp, actor_system.fireball_meter,
                             actor_system.comic_firepower, actor_system.comic_has_corkscrew != 0,
                             comic_has_door_key != 0, actor_system.comic_has_teleport_wand != 0,
                             actor_system.comic_has_lantern != 0, actor_system.comic_has_gems != 0,
                             actor_system.comic_has_crown != 0, actor_system.comic_has_gold != 0,
                             comic_jump_power);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_RenderSetViewport(renderer, nullptr);

        if (game_state == GameState::Paused && pause_sprite) {
            const int pause_x_ega = 40;
            const int pause_y_ega = 64;
            const int pause_width_ega = 128;
            const int pause_height_ega = 48;

            SDL_Rect pause_rect = {
                gameplay_frame_rect.x + static_cast<int>(pause_x_ega * letterbox_scale),
                gameplay_frame_rect.y + static_cast<int>(pause_y_ega * letterbox_scale),
                static_cast<int>(pause_width_ega * letterbox_scale),
                static_cast<int>(pause_height_ega * letterbox_scale)};
            SDL_RenderCopy(renderer, pause_sprite->texture.texture, nullptr, &pause_rect);
        }

        // Present
        SDL_RenderPresent(renderer);

        // Cap rendering to ~60 FPS while physics runs at ~9.1 Hz
        if (tick_accumulator >= MS_PER_TICK) {
            SDL_Delay(0);
        } else {
            SDL_Delay(16);
        }
    }

    return cleanup_and_exit(0);
}
