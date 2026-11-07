#include "../include/game_sequences.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <iostream>

#include "../include/actors.h"
#include "../include/audio.h"
#include "../include/game_state.h"
#include "../include/graphics.h"
#include "../include/high_scores.h"
#include "../include/physics.h"
#include "../include/title_sequence.h"
#include "../include/ui_system.h"

// ---------------------------------------------------------------------------
// Per-tick stat ramps
// ---------------------------------------------------------------------------

void update_lives_sequence_tick() {
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
}

void update_hp_fill_tick() {
    // Gradually fill HP from 0 to MAX_HP at game startup
    // Each tick, increment HP if pending increase is scheduled
    if (comic_hp_pending_increase > 0) {
        comic_hp_pending_increase--;
        if (comic_hp < MAX_HP) {
            comic_hp++;
        }
    }
}

// ---------------------------------------------------------------------------
// Full-screen sequences
// ---------------------------------------------------------------------------

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

// Keep special sequences on the same cadence as gameplay logic: sequences wait
// one game tick (MS_PER_GAME_TICK, game_state.h) per animation step.

// Game-over sprite placement in EGA playfield coordinates (shared by the
// victory and game-over sequences).
constexpr int GAME_OVER_X_EGA = 40;
constexpr int GAME_OVER_Y_EGA = 64;
constexpr int GAME_OVER_WIDTH_EGA = 128;
constexpr int GAME_OVER_HEIGHT_EGA = 48;

// Wait ~MS_PER_GAME_TICK per tick while pumping SDL events.
// Returns false when SDL_QUIT is received; note that despite the const
// reference, *context.quit is written through the pointer (trip flag).
static bool wait_animation_ticks(const GameSequenceContext& context, int ticks) {
    const uint32_t duration_ms = static_cast<uint32_t>(MS_PER_GAME_TICK * ticks + 0.5);
    uint32_t start = SDL_GetTicks();
    SDL_Event e;
    while (SDL_GetTicks() - start < duration_ms) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                *context.quit = true;
                return false;
            }
        }
        SDL_Delay(1);
    }
    return true;
}

// Render one beam animation frame: full playfield (tiles + items + comic +
// beam sprite) inside the letterboxed HUD frame.
static void render_beam_in_frame(GameSequenceContext& context, bool show_comic,
                                 const Sprite* materialize_sprite, bool present_frame = true) {
    SDL_Renderer* renderer = context.renderer;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Rect gameplay_frame_rect = GraphicsSystem::compute_letterbox_rect(renderer);

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
                context.graphics->render_tile(screen_x, screen_y, context.tileset, tile,
                                              render_scale);
            }
        }
    }

    context.actors->render_item(context.graphics, camera_x, render_scale);

    const int comic_screen_x = (comic_x - camera_x) * render_scale + render_scale;
    const int comic_screen_y = comic_y * render_scale + render_scale * 2;
    const int comic_width = render_scale * 2;
    const int comic_height = render_scale * 4;

    if (show_comic && context.current_animation) {
        const AnimationFrame* frame = GraphicsSystem::get_current_frame(*context.current_animation);
        if (frame) {
            context.graphics->render_sprite_centered_scaled(
                comic_screen_x, comic_screen_y, frame->sprite, comic_width, comic_height);
        }
    }

    if (materialize_sprite) {
        context.graphics->render_sprite_centered_scaled(
            comic_screen_x, comic_screen_y, *materialize_sprite, comic_width, comic_height);
    }

    SDL_RenderSetViewport(renderer, &gameplay_frame_rect);
    SDL_RenderSetScale(renderer, letterbox_scale, letterbox_scale);
    context.ui->render_hud(
        score_bytes, comic_num_lives, comic_hp, context.actors->fireball_meter,
        context.actors->comic_firepower, context.actors->comic_has_corkscrew != 0,
        comic_has_door_key != 0, context.actors->comic_has_teleport_wand != 0,
        context.actors->comic_has_lantern != 0, context.actors->comic_has_gems != 0,
        context.actors->comic_has_crown != 0, context.actors->comic_has_gold != 0,
        comic_jump_power);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderSetViewport(renderer, nullptr);

    if (present_frame) {
        SDL_RenderPresent(renderer);
    }
}

// Play the beam-out animation that precedes the victory tally (reverse of
// the intro beam-in).  No-op when beam sprites failed to load.
static void play_beam_out_sequence(GameSequenceContext& context) {
    if (!context.materialize_sprites_loaded || *context.quit) {
        return;
    }

    play_game_sound(GameSound::MATERIALIZE);

    render_beam_in_frame(context, true, nullptr);
    if (!wait_animation_ticks(context, 1)) {
        return;
    }

    for (size_t frame = context.materialize_sprite_count; frame > 0; --frame) {
        const size_t sprite_index = frame - 1;
        const bool show_comic = sprite_index >= 6;
        render_beam_in_frame(context, show_comic, context.materialize_sprites[sprite_index]);
        if (!wait_animation_ticks(context, 1)) {
            return;
        }
    }

    render_beam_in_frame(context, false, nullptr);
    if (!wait_animation_ticks(context, 6)) {
        return;
    }
}

// Load a fullscreen texture straight from PNG (victory screen).
static SDL_Texture* load_fullscreen_texture(GameSequenceContext& context, const char* filename) {
    const std::string path = GraphicsSystem::get_asset_path(filename);
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Victory sequence: failed to load " << filename << " (" << IMG_GetError()
                  << ")" << std::endl;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(context.renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        std::cerr << "Victory sequence: failed to create texture for " << filename << " ("
                  << SDL_GetError() << ")" << std::endl;
        return nullptr;
    }
    return texture;
}

static void render_fullscreen_texture(GameSequenceContext& context, SDL_Texture* texture) {
    if (!texture) {
        return;
    }

    SDL_SetRenderDrawColor(context.renderer, 0, 0, 0, 255);
    SDL_RenderClear(context.renderer);
    SDL_Rect dst = GraphicsSystem::compute_letterbox_rect(context.renderer);
    SDL_RenderCopy(context.renderer, texture, nullptr, &dst);
    SDL_RenderPresent(context.renderer);
}

// Drain queued input so the player must press a fresh key, matching DOS flow.
// Returns false when the window is closed (SDL_QUIT); despite the const
// reference, *context.quit is written through the pointer (trip flag).
static bool wait_for_new_keypress(const GameSequenceContext& context) {
    SDL_Event e;

    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *context.quit = true;
            return false;
        }
    }

    while (!*context.quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                *context.quit = true;
                return false;
            }
            if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                return true;
            }
        }
        SDL_Delay(1);
    }

    return false;
}

// Draw the game-over sprite over the letterboxed gameplay frame.
static void draw_game_over_sprite(GameSequenceContext& context, const Sprite* game_over_sprite) {
    SDL_Renderer* renderer = context.renderer;
    const SDL_Rect gameplay_frame_rect = GraphicsSystem::compute_letterbox_rect(renderer);
    const float letterbox_scale = static_cast<float>(gameplay_frame_rect.w) / EGA_WIDTH;

    const SDL_Rect game_over_rect = {
        gameplay_frame_rect.x + static_cast<int>(GAME_OVER_X_EGA * letterbox_scale),
        gameplay_frame_rect.y + static_cast<int>(GAME_OVER_Y_EGA * letterbox_scale),
        static_cast<int>(GAME_OVER_WIDTH_EGA * letterbox_scale),
        static_cast<int>(GAME_OVER_HEIGHT_EGA * letterbox_scale)};
    SDL_RenderCopy(renderer, game_over_sprite->texture.texture, nullptr, &game_over_rect);
    SDL_RenderPresent(renderer);
}

void play_intro_beam_in_sequence(GameSequenceContext& context) {
    if (!context.materialize_sprites_loaded) {
        return;
    }

    for (int frame = 0; frame < 15; ++frame) {
        render_beam_in_frame(context, false, nullptr);
        if (!wait_animation_ticks(context, 1)) {
            break;
        }
    }

    if (!*context.quit) {
        play_game_sound(GameSound::MATERIALIZE);

        for (size_t frame = 0; frame < context.materialize_sprite_count; ++frame) {
            const bool show_comic = frame >= 6;
            render_beam_in_frame(context, show_comic, context.materialize_sprites[frame]);
            if (!wait_animation_ticks(context, 1)) {
                break;
            }
        }

        render_beam_in_frame(context, true, nullptr);
        wait_animation_ticks(context, 1);
    }

    clear_gameplay_key_states();
}

void play_victory_sequence(GameSequenceContext& context) {
    play_beam_out_sequence(context);
    if (*context.quit) {
        return;
    }

    clear_gameplay_key_states();

    // Base victory bonus: 20,000 points as twenty 1,000-point tally steps.
    for (int step = 0; step < 20; ++step) {
        play_game_sound(GameSound::ITEM_COLLECT);
        award_points(10);
        render_beam_in_frame(context, false, nullptr);
        if (!wait_animation_ticks(context, 1)) {
            break;
        }
    }

    // Remaining lives bonus: 10,000 points per life, then decrement one life icon.
    while (comic_num_lives > 0) {
        for (int step = 0; step < 10; ++step) {
            play_game_sound(GameSound::ITEM_COLLECT);
            award_points(10);
            render_beam_in_frame(context, false, nullptr);
            if (!wait_animation_ticks(context, 1)) {
                break;
            }
        }

        comic_num_lives--;
        render_beam_in_frame(context, false, nullptr);
        if (!wait_animation_ticks(context, 3)) {
            break;
        }
    }

    if (*context.quit) {
        stop_game_music();
        return;
    }

    play_game_music(GameMusic::TITLE);

    SDL_Texture* victory_texture = load_fullscreen_texture(context, "sys002.ega.png");
    if (victory_texture) {
        render_fullscreen_texture(context, victory_texture);
        if (!wait_for_new_keypress(context)) {
            SDL_DestroyTexture(victory_texture);
            return;
        }
    }

    stop_game_music();

    if (!*context.quit) {
        render_beam_in_frame(context, false, nullptr, false);

        if (context.game_over_sprite) {
            draw_game_over_sprite(context, context.game_over_sprite);
        }

        wait_for_new_keypress(context);
        if (!run_high_scores_screen(context.renderer, context.graphics, score_bytes)) {
            *context.quit = true;
        }
    }

    if (victory_texture) {
        SDL_DestroyTexture(victory_texture);
    }
}

void play_game_over_sequence(GameSequenceContext& context) {
    clear_gameplay_key_states();
    *context.game_state = GameState::Playing;
    *context.pause_waiting_for_escape_release = false;

    render_beam_in_frame(context, false, nullptr, false);

    if (context.game_over_sprite && !*context.quit) {
        draw_game_over_sprite(context, context.game_over_sprite);
        play_game_sound(GameSound::GAME_OVER);
    }

    if (!*context.quit) {
        wait_animation_ticks(context, 1);
        wait_for_new_keypress(context);
    }
    if (!*context.quit &&
        !run_high_scores_screen(context.renderer, context.graphics, score_bytes)) {
        *context.quit = true;
    }
}
