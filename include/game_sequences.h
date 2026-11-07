#ifndef GAME_SEQUENCES_H
#define GAME_SEQUENCES_H

#include <cstddef>
#include <cstdint>

#include "game_state.h"  // GameState

// Forward declarations keep this header SDL-free: game_sequences.h is only
// included by main.cpp, which already has the full definitions.
struct SDL_Renderer;
struct Sprite;
struct Tileset;
struct Animation;
class ActorSystem;
class GraphicsSystem;
class UISystem;

/**
 * game_sequences.h - Full-screen life-cycle sequences and per-tick award ramps.
 *
 * Extracted from main.cpp so that main stays bootstrap + tick pipeline.
 * Two kinds of logic live here:
 *
 *   1. Per-tick state machines advanced from the main tick loop:
 *        - update_lives_sequence_tick(): award 5 lives (1 tick apart), then
 *          subtract the life currently in use.
 *        - update_hp_fill_tick(): fill HP gradually to MAX_HP at startup.
 *
 *   2. Blocking full-screen sequences (beam-in intro, victory beam-out +
 *      score tally, game over) rendered straight to the SDL renderer.
 *      They run on the same cadence as gameplay ticks for faithful pacing.
 */

// --- Per-tick stat ramps (call once per game tick from the main loop) ---

// Lives count-up sequence: award lives_sequence_counter lives with a
// lives_sequence_delay-tick pause between each, then subtract 1 (the life
// currently in use).  Sets lives_sequence_complete when finished.
void update_lives_sequence_tick();

// Gradually fill HP from 0 to MAX_HP at game startup: each tick, consume one
// unit of comic_hp_pending_increase and raise comic_hp toward MAX_HP.
void update_hp_fill_tick();

// --- Blocking full-screen sequences ---

/**
 * Everything a full-screen sequence needs.  Owned by main() during bootstrap;
 * holds only non-owning pointers to systems and state it already manages.
 *
 *  - quit points at the main loop's quit flag so SDL_QUIT received during a
 *    sequence aborts it and exits the game.
 *  - tileset must be kept in sync by main (it rebinds it when the level
 *    changes; sequences never trigger a level change themselves).
 *  - current_animation should be assigned just before the sequence runs.
 */
struct GameSequenceContext {
    SDL_Renderer* renderer = nullptr;
    GraphicsSystem* graphics = nullptr;            // asset paths, tile/sprite rendering
    UISystem* ui = nullptr;                        // HUD rendering inside beam frames
    ActorSystem* actors = nullptr;                 // item/enemy rendering inside beam frames
    Tileset* tileset = nullptr;                    // current level tileset cache
    Animation* current_animation = nullptr;        // player animation state (mutable frame cursor)
    Sprite* const* materialize_sprites = nullptr;  // beam-in/out frames
    size_t materialize_sprite_count = 0;
    bool materialize_sprites_loaded = true;  // false when beam frames failed to load
    Sprite* game_over_sprite = nullptr;
    GameState* game_state = nullptr;
    bool* quit = nullptr;
    bool* pause_waiting_for_escape_release = nullptr;  // reset by game-over sequence
};

// Play the 15-tick beam-in intro shown when gameplay starts (Comic
// materializes onto the playfield).  Ends with one tick of Comic visible,
// then leaves key states cleared.
void play_intro_beam_in_sequence(GameSequenceContext& context);

// Victory: beam-out, 20,000-point base tally, 10,000 points per remaining
// life, victory screen, then the high scores screen.
void play_victory_sequence(GameSequenceContext& context);

// Game over: game-over sprite + sound, brief pause, then the high scores
// screen.  Restores GameState to Playing when control comes back.
void play_game_over_sequence(GameSequenceContext& context);

// Clear all gameplay key states (current and previous edge-trigger state).
// Called when control leaves normal play: pause, victory/game-over, intro.
void clear_gameplay_key_states();

#endif  // GAME_SEQUENCES_H
