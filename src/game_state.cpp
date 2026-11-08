/**
 * game_state.cpp - Definitions for the shared game-state globals
 *
 * Declared in include/game_state.h; this file is the ONLY place these variables
 * are defined. Replace any per-module `extern` declarations and any test-side
 * redefinitions with `#include "game_state.h"`.
 */

#include "game_state.h"

#include "physics.h"  // JUMP_POWER_DEFAULT, MAX_HP (initial values only)

// --- Player kinematics ---
int comic_x = 20;
int comic_y = 2;
int8_t comic_y_vel = 0;
int8_t comic_x_momentum = 0;
uint8_t comic_facing = 1;  // 1 right, 0 left
uint8_t comic_animation = 0;
uint8_t comic_is_falling_or_jumping = 1;
uint8_t comic_jump_counter = 0;
uint8_t comic_jump_power = JUMP_POWER_DEFAULT;

// --- Input key state ---
uint8_t key_state_jump = 0;
uint8_t previous_key_state_jump = 0;
uint8_t key_state_left = 0;
uint8_t key_state_right = 0;
uint8_t key_state_open = 0;               // Open key for doors
uint8_t previous_key_state_open = 0;      // Track previous state for edge-triggered activation
uint8_t key_state_fire = 0;               // Fire key (Left Ctrl)
uint8_t key_state_teleport = 0;           // Teleport key (CapsLock-style binding)
uint8_t previous_key_state_teleport = 0;  // Track previous teleport key state for edge trigger

// --- Camera ---
int camera_x = 0;

// --- Items / lives / HP / score ---
uint8_t comic_has_door_key = 0;              // 1 if player has door key, 0 otherwise
uint8_t comic_firepower = 0;                 // 0-5: number of fireball slots available
uint8_t comic_has_corkscrew = 0;             // 1 if Comic has the Corkscrew item
uint8_t comic_has_boots = 0;                 // 1 if Comic has Boots (increased jump power)
uint8_t comic_has_lantern = 0;               // 1 if Comic has Lantern (castle lighting)
uint8_t comic_has_teleport_wand = 0;         // 1 if Comic has Teleport Wand
uint8_t comic_has_gems = 0;                  // 1 if Comic collected Gems
uint8_t comic_has_crown = 0;                 // 1 if Comic collected Crown
uint8_t comic_has_gold = 0;                  // 1 if Comic collected Gold
uint8_t comic_num_treasures = 0;             // Count of treasures collected (0-3)
uint8_t comic_num_lives = 0;                 // Lives remaining (counts up 0..5, then -1)
uint8_t lives_sequence_counter = 5;          // Lives count-up: award 5 lives, then subtract 1
uint8_t lives_sequence_delay = 1;            // Delay counter for lives animation (1 tick per award)
bool lives_sequence_complete = false;        // Stops the lives sequence after completion
uint8_t comic_hp = 0;                        // Current health points (0-6)
uint8_t comic_hp_pending_increase = MAX_HP;  // HP fills gradually from 0 to MAX_HP at game start
uint8_t score_bytes[3] = {0, 0, 0};          // Score in base-100 encoding
uint8_t score_10000_counter = 0;             // Carries into ten-thousands digit; 5 -> extra life

// --- Level/stage transition tracking ---
uint8_t current_level_number = 1;            // Current level (0=LAKE, 1=FOREST, etc.)
uint8_t current_stage_number = 0;            // Current stage (0-2 per level)
const level_t* current_level_ptr = nullptr;  // Pointer to current level data
int8_t source_door_level_number = -1;  // Set when entering via door for reciprocal positioning
int8_t source_door_stage_number = -1;

// --- Checkpoint position (for respawn and boundary crossing) ---
uint8_t comic_y_checkpoint = 12;  // Y position to respawn at
uint8_t comic_x_checkpoint = 14;  // X position to respawn at

// --- Game-over flag ---
bool game_over_triggered = false;  // Set when life depletion should start the game-over sequence

// --- Teleport state machine ---
bool comic_is_teleporting = false;
uint8_t teleport_animation = 0;
uint8_t teleport_source_x = 0;
uint8_t teleport_source_y = 0;
uint8_t teleport_destination_x = 0;
uint8_t teleport_destination_y = 0;
uint8_t teleport_camera_counter = 0;
int8_t teleport_camera_vel = 0;
bool teleport_skip_tick = false;

// --- Global system pointers ---
ActorSystem* g_actor_system = nullptr;  // Actor system pointer (for cheat access)
