#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <cstdint>

// Forward declarations keep this header light: game_state.h is included by every
// game module (physics, doors, actors, cheats, graphics, level_loader, tests), and
// none of them needs the full definitions here - only pointers to these types.
struct level_t;
class ActorSystem;

/**
 * Lifetime states of the game loop. Owned conceptually by main()'s tick pipeline;
 * declared here so other modules can reference the enum without including main.cpp.
 */
enum class GameState { Playing, Paused, Victory, GameOver, Exiting };

/**
 * Game tick timing - single source of truth.
 *
 * The PC timer interrupt (IRQ 0) fires at 18.2065 Hz but game_tick_flag is only
 * set on *odd* interrupt cycles (irq0_parity toggles 0,1,0,1,...), so the actual
 * game logic runs at half the interrupt rate: ~9.1032 Hz. Based on analysis of
 * the original DOS timer interrupt handler (original disassembly is not vendored
 * in this repository).
 *
 * Every consumer of the game tick cadence (the main-loop accumulator and any
 * special sequence that must run on the same cadence as gameplay) must derive
 * its timing from these constants rather than re-deriving 18.2065/2.0 locally;
 * a local re-derivation can drift from the tick pipeline when the timing logic
 * changes.
 */
constexpr double GAME_TICK_RATE_HZ = 18.2065 / 2.0;  // ~9.1032 Hz - actual game tick rate
constexpr double MS_PER_GAME_TICK = 1000.0 / GAME_TICK_RATE_HZ;  // ~109.86 ms per tick

/**
 * game_state.h - Shared mutable game state
 *
 * This is the single home for the ~40 cross-module game-state globals. Every
 * variable below is DEFINED in src/game_state.cpp and must not be redefined
 * elsewhere (add new shared state here instead of adding extern declarations
 * in another module).
 *
 * The flat-global layout intentionally mirrors the original 1988 assembly
 * (see reference/ disassembly); grouping them here is the modernization step
 * that kills the copy-drift between main.cpp, per-module extern blocks, and
 * tests/test_helpers.cpp.
 */

// --- Player kinematics (position in game units; velocity in 1/8 units per tick) ---
extern int comic_x;                          // Comic's x position in game units
extern int comic_y;                          // Comic's y position in game units
extern int8_t comic_y_vel;                   // Vertical velocity
extern int8_t comic_x_momentum;              // Horizontal momentum
extern uint8_t comic_facing;                 // 1 right, 0 left (COMIC_FACING_*)
extern uint8_t comic_animation;              // Current animation slot
extern uint8_t comic_is_falling_or_jumping;  // 0 grounded, nonzero airborne
extern uint8_t comic_jump_counter;           // Remaining jump ticks; set from jump_power
extern uint8_t comic_jump_power;             // Jump power in tiles (JUMP_POWER_*)

// --- Input key state (current/previous pairs edge-trigger activations) ---
extern uint8_t key_state_jump;
extern uint8_t previous_key_state_jump;
extern uint8_t key_state_left;
extern uint8_t key_state_right;
extern uint8_t key_state_open;               // Open key for doors
extern uint8_t previous_key_state_open;      // Track previous state for edge-triggered activation
extern uint8_t key_state_fire;               // Fire key (Left Ctrl)
extern uint8_t key_state_teleport;           // Teleport key (CapsLock-style binding)
extern uint8_t previous_key_state_teleport;  // Track previous teleport key state for edge trigger

// --- Camera ---
extern int camera_x;  // Left edge of the visible playfield, in game units

// --- Items / lives / HP / score ---
// Player inventory flags live here once (like comic_has_door_key): collected
// by the item system in actors.cpp (apply_item_effect), read by doors,
// HUD, and sequences. ActorSystem carries only non-player bookkeeping.
extern uint8_t comic_has_door_key;         // 1 if player has door key, 0 otherwise
extern uint8_t comic_firepower;            // 0-5: number of fireball slots available
extern uint8_t comic_has_corkscrew;        // 1 if Comic has the Corkscrew item
extern uint8_t comic_has_boots;            // 1 if Comic has Boots (increased jump power)
extern uint8_t comic_has_lantern;          // 1 if Comic has Lantern (castle lighting)
extern uint8_t comic_has_teleport_wand;    // 1 if Comic has Teleport Wand
extern uint8_t comic_has_gems;             // 1 if Comic collected Gems
extern uint8_t comic_has_crown;            // 1 if Comic collected Crown
extern uint8_t comic_has_gold;             // 1 if Comic collected Gold
extern uint8_t comic_num_treasures;        // Count of treasures collected (0-3)
extern uint8_t comic_num_lives;            // Lives remaining (counts up 0..5, then -1)
extern uint8_t lives_sequence_counter;     // Lives count-up sequence: award 5, then -1
extern uint8_t lives_sequence_delay;       // Delay counter for lives animation (1 tick per award)
extern bool lives_sequence_complete;       // Stops the lives sequence after completion
extern uint8_t comic_hp;                   // Current health points (0-6)
extern uint8_t comic_hp_pending_increase;  // HP fills gradually from this value to MAX_HP
extern uint8_t score_bytes[3];             // Score in base-100 encoding
extern uint8_t score_10000_counter;        // Carries into ten-thousands digit; 5 -> extra life

// --- Level/stage transition tracking ---
extern uint8_t current_level_number;      // Current level (0=LAKE, 1=FOREST, etc.)
extern uint8_t current_stage_number;      // Current stage (0-2 per level)
extern const level_t* current_level_ptr;  // Pointer to current level data
extern int8_t source_door_level_number;   // Set when entering via door for reciprocal positioning
extern int8_t source_door_stage_number;

// --- Checkpoint position (for respawn and boundary crossing) ---
extern uint8_t comic_y_checkpoint;  // Y position to respawn at
extern uint8_t comic_x_checkpoint;  // X position to respawn at

// --- Game-over flag ---
extern bool game_over_triggered;  // Set when life depletion should start the game-over sequence

// --- Teleport state machine (used by begin_teleport/handle_teleport_tick in main) ---
extern bool comic_is_teleporting;
extern uint8_t teleport_animation;
extern uint8_t teleport_source_x;
extern uint8_t teleport_source_y;
extern uint8_t teleport_destination_x;
extern uint8_t teleport_destination_y;
extern uint8_t teleport_camera_counter;
extern int8_t teleport_camera_vel;
extern bool teleport_skip_tick;

// --- Global system pointers (for access from other modules) ---
extern ActorSystem* g_actor_system;  // Actor system pointer (for cheat access)

#endif  // GAME_STATE_H
