#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

#include "../include/actors.h"
#include "../include/audio.h"
#include "../include/cheats.h"
#include "../include/doors.h"
#include "../include/game_sequences.h"
#include "../include/game_state.h"
#include "../include/graphics.h"
#include "../include/high_scores.h"
#include "../include/input_bindings.h"
#include "../include/level_loader.h"
#include "../include/physics.h"
#include "../include/player_teleport.h"
#include "../include/title_sequence.h"
#include "../include/ui_system.h"

// Shared game state (player, input, score, level tracking, teleport) is now
// declared in include/game_state.h and defined in src/game_state.cpp.

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

// Translate an SDL key event into the SDL-independent CheatKey enum, so
// cheats.h does not need SDL at all (cheat logic stays testable without a
// video backend). Unmapped keys become CheatKey::None and are ignored.
static CheatKey cheat_key_from_sdl(const SDL_Keycode key) {
    switch (key) {
        case SDLK_F1:
            return CheatKey::F1;
        case SDLK_F2:
            return CheatKey::F2;
        case SDLK_F3:
            return CheatKey::F3;
        case SDLK_F4:
            return CheatKey::F4;
        case SDLK_F5:
            return CheatKey::F5;
        case SDLK_q:
            return CheatKey::Q;
        case SDLK_BACKSPACE:
            return CheatKey::Backspace;
        case SDLK_RETURN:
            return CheatKey::Return;
        case SDLK_KP_ENTER:
            return CheatKey::KpEnter;
        default:
            break;
    }
    if (key >= SDLK_0 && key <= SDLK_9) {
        return static_cast<CheatKey>(static_cast<int>(CheatKey::Digit0) + (key - SDLK_0));
    }
    return CheatKey::None;
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
        } else if (std::strcmp(argv[i], "--smooth") == 0) {
            // Smooth rendering: extrapolate positions between the ~9.1 Hz game
            // ticks for 60 Hz motion (see game_state.h). Gameplay logic and its
            // tick cadence are untouched.
            smooth_rendering_enabled = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Captain Comic - Usage:" << std::endl;
            std::cout << "  --debug       Enable debug mode and cheat keys" << std::endl;
            std::cout << "  --skip-title  Skip the title sequence" << std::endl;
            std::cout << "  --smooth      Smooth rendering (extrapolate motion between ticks)"
                      << std::endl;
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

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return cleanup_and_exit(1);
    }

    // Frame pacing: with a vsynced renderer, SDL_RenderPresent already blocks
    // until vblank and paces the loop - any extra fixed delay over-sleeps past
    // the next vblank and produces irregular ~30 FPS frame times (the whole
    // scene visibly snaps one frame). Only fall back to delay-based pacing
    // when vsync is absent.
    SDL_RendererInfo renderer_info;
    const bool vsync_active = (SDL_GetRendererInfo(renderer, &renderer_info) == 0) &&
                              (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;

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
        comic_firepower = 3;  // Player inventory is single-sourced in game_state.h
    }

    // Make actor system accessible to cheat system
    g_actor_system = &actor_system;

    // Initialize UI system
    UISystem ui_system(g_graphics);
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
    const Sprite* pause_sprite = g_graphics->get_sprite("pause", "");

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

    // Tick timing uses the shared GAME_TICK_RATE_HZ / MS_PER_GAME_TICK
    // constants (game_state.h), analyzed from the original DOS timer interrupt.
    constexpr int MAX_TICKS_PER_FRAME = 5;
    constexpr double MAX_ACCUMULATED_MS = MS_PER_GAME_TICK * MAX_TICKS_PER_FRAME;
    uint32_t last_tick_time = SDL_GetTicks();
    double tick_accumulator = 0.0;

    // Keep special sequences on the same cadence as gameplay logic for
    // faithful pacing in this implementation (handled in game_sequences.cpp).
    // Wiring for the extracted full-screen sequences (see game_sequences.cpp):
    // non-owning pointers into the main-owned state, with the flags shared so
    // SDL_QUIT during a sequence aborts it and exits the game.
    GameSequenceContext sequence_context;
    sequence_context.renderer = renderer;
    sequence_context.graphics = g_graphics;
    sequence_context.ui = &ui_system;
    sequence_context.actors = &actor_system;
    sequence_context.tileset = cached_tileset;
    sequence_context.current_animation = current_animation;
    sequence_context.materialize_sprites = materialize_sprites.data();
    sequence_context.materialize_sprite_count = materialize_sprites.size();
    sequence_context.materialize_sprites_loaded = materialize_sprites_loaded;
    sequence_context.game_over_sprite = game_over_sprite;
    sequence_context.game_state = &game_state;
    sequence_context.quit = &quit;
    sequence_context.pause_waiting_for_escape_release = &pause_waiting_for_escape_release;

    play_intro_beam_in_sequence(sequence_context);

    // Persists between render frames; updated each tick when movement is attempted.
    bool player_moved_last_tick = false;

    // Persistent smooth-render positions for player and camera (follow-
    // smoothing; see game_state.h). SMOOTH_RENDER_POS_UNSET makes the first
    // rendered frame snap to the extrapolated target.
    float smooth_render_comic_x = SMOOTH_RENDER_POS_UNSET;
    float smooth_render_comic_y = SMOOTH_RENDER_POS_UNSET;
    float smooth_render_camera_x = SMOOTH_RENDER_POS_UNSET;
    bool smooth_render_snapped_this_frame = false;

    while (!quit) {
        uint32_t current_time = SDL_GetTicks();
        uint32_t delta_time = current_time - last_tick_time;
        last_tick_time = current_time;
        smooth_frame_delta_ms = static_cast<float>(delta_time);
        tick_accumulator += delta_time;
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

                // Debug toggle for smooth rendering (extrapolation between
                // ticks). Requesting a snap resyncs the prev-tick snapshots to
                // the live positions so the first frame after toggling on
                // never extrapolates from stale state.
                if (debug_mode && key == SDLK_F6 && e.key.repeat == 0) {
                    smooth_rendering_enabled = !smooth_rendering_enabled;
                    smooth_render_snap_requested = true;
                }

                // Process cheat keys (only active if --debug flag set)
                g_cheats->process_input(cheat_key_from_sdl(e.key.keysym.sym));
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
            while (tick_accumulator >= MS_PER_GAME_TICK && ticks_processed < MAX_TICKS_PER_FRAME) {
                tick_accumulator -= MS_PER_GAME_TICK;
                ticks_processed++;

                // Smooth-render snapshots: record the pre-tick state so the
                // renderer can extrapolate between ticks. Must run on EVERY
                // tick (including the early-continue branches below) so the
                // deltas never go stale while positions rest.
                smooth_prev_comic_x = comic_x;
                smooth_prev_camera_x = camera_x;
                actor_system.capture_render_snapshots();

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
                    process_teleport_input(comic_has_teleport_wand != 0);
                } else {
                    previous_key_state_teleport = key_state_teleport;
                }

                if (!beam_out_sequence_played && comic_num_treasures >= 3) {
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

                // Lives count-up sequence and gradual HP fill (per-tick ramps
                // extracted to game_sequences.cpp).
                update_lives_sequence_tick();
                update_hp_fill_tick();

                // Fireball meter charging is handled by ActorSystem::update()
                // (charges at 1 unit per 2 ticks when not firing)

                // Game-over is handled by physics when Comic hits the bottom of playfield
                // (sound triggered there).  No additional check needed here.
            }
        } else {
            tick_accumulator = 0.0;
        }

        if (game_state == GameState::Victory) {
            sequence_context.current_animation = current_animation;
            sequence_context.tileset = cached_tileset;
            play_victory_sequence(sequence_context);
            game_state = GameState::Exiting;
            quit = true;
        } else if (game_state == GameState::GameOver) {
            sequence_context.current_animation = current_animation;
            sequence_context.tileset = cached_tileset;
            play_game_over_sequence(sequence_context);
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
            } else if (comic_is_falling_or_jumping) {
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

        // Consume a requested render snap (respawn, stage load, F6 toggle):
        // align the prev-tick snapshots with the live positions so the next
        // frame renders exactly the new state instead of extrapolating a
        // position jump.
        if (smooth_render_snap_requested) {
            smooth_prev_comic_x = comic_x;
            smooth_prev_camera_x = camera_x;
            actor_system.reset_render_snapshots();
            smooth_render_snap_requested = false;
            smooth_render_snapped_this_frame = true;
        }

        // Elapsed fraction of the current game tick (0 right after a tick
        // fires, approaching 1 just before the next one). With smooth
        // rendering disabled it stays 0 and every draw below renders the
        // exact integer game state, identical to the legacy renderer.
        float smooth_alpha = 0.0f;
        if (smooth_rendering_enabled) {
            smooth_alpha =
                static_cast<float>(std::clamp(tick_accumulator / MS_PER_GAME_TICK, 0.0, 1.0));
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

        Tileset* tileset = cached_tileset;

        // Extrapolated target positions for this frame (smooth rendering).
        // Player-x speculation is additionally gated on the sim's own
        // continuation signals, so it never outruns a stop:
        //
        // - Grounded motion continues only while a movement key is held in
        //   the same direction (ground movement is key-gated in the tick
        //   pipeline); when the key releases, speculation stops that frame.
        // - Airborne motion continues only while momentum carries it (air
        //   movement is momentum-driven with per-tick drag).
        // - Player y uses the sim's own velocity: handle_fall_or_jump
        //   integrates delta_y = comic_y_vel >> 3 per tick, so that value is
        //   the sim's next displacement. Grounded Comic is exactly at rest.
        // - The camera mirrors the player's x motion one tick at a time.
        //
        // smooth_alpha is 0 with smoothing disabled, collapsing everything to
        // the exact integer game state.
        const int comic_dx = comic_x - smooth_prev_comic_x;
        bool comic_moves = false;
        if (comic_dx != 0) {
            if (comic_is_falling_or_jumping != 0) {
                comic_moves = (comic_x_momentum != 0) && ((comic_x_momentum > 0) == (comic_dx > 0));
            } else {
                int move_intent = 0;
                if (key_state_right && !key_state_left) {
                    move_intent = 1;
                } else if (key_state_left && !key_state_right) {
                    move_intent = -1;
                }
                comic_moves = (move_intent == comic_dx);
            }
        }
        const float target_comic_x =
            static_cast<float>(comic_x) +
            (comic_moves ? static_cast<float>(comic_dx) * smooth_alpha : 0.0f);
        const int comic_dy = (comic_is_falling_or_jumping != 0 && !is_player_dying())
                                 ? (static_cast<int>(comic_y_vel) >> 3)
                                 : 0;
        const float target_comic_y =
            static_cast<float>(comic_y) + static_cast<float>(comic_dy) * smooth_alpha;
        const float target_camera_x =
            static_cast<float>(camera_x) +
            static_cast<float>(camera_x - smooth_prev_camera_x) * smooth_alpha;

        // Follow-smoothing: within-epsilon tracking renders exactly (zero
        // steady-state latency); when the render position is ahead of the sim
        // (stop, landing, reversal) it slides back at a fixed speed instead
        // of popping for a frame.
        const float render_comic_x = smooth_follow_render_pos(smooth_render_comic_x, target_comic_x,
                                                              smooth_render_snapped_this_frame);
        const float render_comic_y = smooth_follow_render_pos(
            smooth_render_comic_y, target_comic_y, smooth_render_snapped_this_frame,
            3.0f);  // vertical: up to ~2 units/tick at terminal velocity
        const float render_camera_x = smooth_follow_render_pos(
            smooth_render_camera_x, target_camera_x, smooth_render_snapped_this_frame);
        smooth_render_snapped_this_frame = false;

        // Render tiles with offscreen margin for seamless scrolling.
        // The C code pre-renders into a 256-byte-wide buffer; we render tiles
        // slightly outside the visible playfield and let viewport clipping
        // hide any offscreen portions.
        const int OFFSCREEN_MARGIN_UNITS = 2;
        const int min_visible_x = camera_x - OFFSCREEN_MARGIN_UNITS;
        const int max_visible_x = camera_x + PLAYFIELD_WIDTH + OFFSCREEN_MARGIN_UNITS;

        for (int ty = 0; ty < MAP_HEIGHT_TILES; ty++) {
            for (int tx = 0; tx < MAP_WIDTH_TILES; tx++) {
                int world_x = tx * 2;  // Tile x in game units

                // Render tiles within visible range (with left margin for scrolling).
                // Each tile is 2 units wide, so render if tile overlaps the range.
                if (world_x + 2 > min_visible_x && world_x < max_visible_x) {
                    uint8_t tile = get_tile_at(tx * 2, ty * 2);

                    const float screen_x = (world_x - render_camera_x) * render_scale;
                    const int screen_y = ty * 2 * render_scale;

                    g_graphics->render_tile_f(screen_x, static_cast<float>(screen_y), tileset, tile,
                                              render_scale);
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
            const float screen_x =
                (static_cast<float>(door_world_x) - render_camera_x) * render_scale;
            const float screen_y = static_cast<float>(door_world_y) * render_scale;

            const bool can_draw_tiles =
                tileset != nullptr && current_level_ptr != nullptr && tile_w >= 2;

            Uint8 prev_r = 0;
            Uint8 prev_g = 0;
            Uint8 prev_b = 0;
            Uint8 prev_a = 0;
            SDL_GetRenderDrawColor(renderer, &prev_r, &prev_g, &prev_b, &prev_a);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

            const SDL_FRect door_rect = {screen_x, screen_y, static_cast<float>(tile_w * 2),
                                         static_cast<float>(tile_h * 2)};
            SDL_RenderFillRectF(renderer, &door_rect);

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

                    const float tile_screen_x =
                        (static_cast<float>(world_tile_x) - render_camera_x) * render_scale;
                    const float tile_screen_y = static_cast<float>(world_tile_y) * render_scale;
                    const uint8_t tile_id = get_tile_at(world_tile_x, world_tile_y);
                    g_graphics->render_tile_f(tile_screen_x, tile_screen_y, tileset, tile_id,
                                              render_scale);
                };

                int shift = 0;
                if (door_render_mode == DoorAnimationRenderMode::HALF_OPEN ||
                    door_render_mode == DoorAnimationRenderMode::HALF_CLOSED) {
                    shift = tile_w / 4;
                } else if (door_render_mode == DoorAnimationRenderMode::FULL_OPEN) {
                    shift = tile_w / 2;
                }

                const float left_col_x = screen_x - shift;
                const float right_col_x = screen_x + tile_w + shift;

                // Each door leaf is one full tile column (2 game units wide).
                g_graphics->render_tile_f(left_col_x, screen_y, tileset, tile_ul, render_scale);
                g_graphics->render_tile_f(left_col_x, screen_y + tile_h, tileset, tile_ll,
                                          render_scale);
                g_graphics->render_tile_f(right_col_x, screen_y, tileset, tile_ur, render_scale);
                g_graphics->render_tile_f(right_col_x, screen_y + tile_h, tileset, tile_lr,
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

        actor_system.render_enemies(g_graphics, render_camera_x, render_scale, smooth_alpha);
        actor_system.render_fireballs(g_graphics, render_camera_x, render_scale, smooth_alpha);

        // Render player sprite
        if (current_animation && door_player_visible) {
            const AnimationFrame* frame = g_graphics->get_current_frame(*current_animation);
            if (frame) {
                // Center player on screen relative to camera
                // Player is 2 units wide, 4 units tall in game coords
                const float screen_x =
                    (render_comic_x - render_camera_x) * render_scale + render_scale;  // Center X
                const float screen_y =
                    render_comic_y * render_scale + render_scale * 2;  // Center Y
                const int player_width = render_scale * 2;
                const int player_full_height = render_scale * 4;
                if (should_clip_player_death_render()) {
                    // Compute how much of the sprite still fits within the playfield
                    // viewport so the sinking effect matches natural SDL clipping.
                    // sprite_top_y == render_comic_y * render_scale (center Y - half height).
                    const float sprite_top_y = screen_y - player_full_height / 2.0f;
                    const float clip_h = std::clamp(playfield_viewport.h - sprite_top_y, 0.0f,
                                                    static_cast<float>(player_full_height));
                    if (clip_h > 0.0f) {
                        g_graphics->render_sprite_top_clip_scaled_f(screen_x, screen_y,
                                                                    frame->sprite, player_width,
                                                                    player_full_height, clip_h);
                    }
                } else {
                    g_graphics->render_sprite_centered_scaled_f(screen_x, screen_y, frame->sprite,
                                                                player_width, player_full_height);
                }
            }
        }

        render_door_animation_overlay(true);

        if (comic_is_teleporting) {
            const uint8_t last_teleport_frame = static_cast<uint8_t>(teleport_sprites.size() - 1);
            const uint8_t source_frame = std::min(teleport_animation, last_teleport_frame);
            if (teleport_sprites[source_frame]) {
                const float source_screen_x =
                    (static_cast<float>(teleport_source_x) - render_camera_x) * render_scale +
                    render_scale;
                const float source_screen_y =
                    static_cast<float>(teleport_source_y) * render_scale + render_scale * 2;
                g_graphics->render_sprite_centered_scaled_f(source_screen_x, source_screen_y,
                                                            *teleport_sprites[source_frame],
                                                            render_scale * 2, render_scale * 4);
            }

            if (teleport_animation >= 1) {
                const uint8_t destination_phase = static_cast<uint8_t>(teleport_animation - 1);
                const uint8_t dest_frame = std::min(destination_phase, last_teleport_frame);
                if (teleport_sprites[dest_frame]) {
                    const float destination_screen_x =
                        (static_cast<float>(teleport_destination_x) - render_camera_x) *
                            render_scale +
                        render_scale;
                    const float destination_screen_y =
                        static_cast<float>(teleport_destination_y) * render_scale +
                        render_scale * 2;
                    g_graphics->render_sprite_centered_scaled_f(
                        destination_screen_x, destination_screen_y, *teleport_sprites[dest_frame],
                        render_scale * 2, render_scale * 4);
                }
            }
        }

        // Assembly-faithful order: items are rendered after Comic, so with painter's
        // algorithm they appear on top when overlapping.
        actor_system.render_item(g_graphics, render_camera_x, render_scale);

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
        ui_system.render_hud(
            score_bytes, comic_num_lives, comic_hp, actor_system.get_fireball_meter(),
            comic_firepower, comic_has_corkscrew != 0, comic_has_door_key != 0,
            comic_has_teleport_wand != 0, comic_has_lantern != 0, comic_has_gems != 0,
            comic_has_crown != 0, comic_has_gold != 0, comic_jump_power);
        SDL_RenderSetScale(renderer, 1.0f, 1.0f);
        SDL_RenderSetViewport(renderer, nullptr);

        if (game_state == GameState::Paused && pause_sprite) {
            // Pause overlay: fixed EGA playfield placement (40, 64, 128x48).
            g_graphics->render_sprite_in_ega_frame(*pause_sprite, 40, 64, 128, 48);
        }

        // Present
        SDL_RenderPresent(renderer);

        // Cap rendering to ~60 FPS while physics runs at ~9.1 Hz.
        // With vsync, SDL_RenderPresent above already blocks on vblank and
        // paces the loop; an extra delay here would over-sleep past the next
        // vblank and stutter the frame cadence. Without vsync, keep the
        // legacy delay-based cap.
        if (!vsync_active) {
            SDL_Delay(16);
        } else if (tick_accumulator >= MS_PER_GAME_TICK) {
            SDL_Delay(0);
        }
    }

    return cleanup_and_exit(0);
}
