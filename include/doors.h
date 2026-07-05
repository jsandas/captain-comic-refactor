#ifndef DOORS_H
#define DOORS_H

#include <cstdint>

#include "level.h"

/**
 * doors.h - Door system for level transitions
 *
 * Implements door collision detection, key requirement checking, and
 * level/stage transitions via doors.
 *
 * Matches the original assembly behavior where doors are positioned on the map
 * and require the Door Key to open. Entering a door transitions to a new level
 * or stage.
 *
 * Reference: jsandas/comic-c src/doors.c
 */

/**
 * check_door_activation - Check if Comic is in front of a door and the open key is pressed
 *
 * When the player presses the open key:
 * 1. Iterate through all doors in the current stage
 * 2. For each door, check:
 *    - Comic's Y coordinate matches door Y coordinate exactly (both in game units)
 *    - Comic's X coordinate is within 3 units of door X (0, 1, or 2 units offset)
 * 3. If a door matches and the player has the Door Key, activate the door
 *
 * Door positioning uses game units (same as Comic's position):
 * - X ranges from 0-256 (128 tiles * 2)
 * - Y ranges from 0-20 (10 tiles * 2)
 *
 * Input:
 *   Requires external variables:
 *   - comic_x, comic_y: Player position in game units
 *   - key_state_open: 1 if open key is pressed, 0 otherwise
 *   - comic_has_door_key: 1 if player has door key, 0 otherwise
 *   - current_level_ptr, current_stage_number: Current level/stage
 *
 * Output:
 *   Returns 1 if a door was activated, 0 otherwise
 *   If activated, may load new level/stage data
 */
uint8_t check_door_activation();

enum class DoorAnimationPhase : uint8_t { NONE = 0, ENTERING = 1, EXIT_DELAY = 2, EXITING = 3 };

enum class DoorAnimationRenderMode : uint8_t {
    NONE = 0,
    HALF_OPEN = 1,
    FULL_OPEN = 2,
    HALF_CLOSED = 3
};

/* Door animation state used by the main loop.
 * ENTERING frames: 0..3 (half-open, full-open, half-closed, closed)
 * EXITING frames:  0..4 (closed, half-open, full-open, half-closed, closed) */
extern DoorAnimationPhase g_door_anim_phase;
extern uint8_t g_door_anim_frame;

/* Advance the door animation by one game tick.
 * Handles deferred stage/level transition at the end of ENTERING. */
void update_door_animation_tick();

/* Return the active door render state for this frame.
 * world_x/world_y are the upper-left door tile coordinates in game units.
 * draw_overlay_in_front controls whether the half-door overlay is rendered
 * before or after Comic. player_visible indicates whether Comic should render. */
bool get_door_animation_render_state(uint8_t* world_x, uint8_t* world_y,
                                     DoorAnimationRenderMode* mode, bool* draw_overlay_in_front,
                                     bool* player_visible);

/* Testing hook: when set to true, activate_door will update the current
 * level/stage numbers and source_door_* flags but will NOT actually call
 * load_new_level()/load_new_stage(). This allows unit tests to exercise
 * door logic deterministically without dragging in asset loading or
 * clearing the source flags. Default is false in production. */
extern bool g_skip_load_on_door;

/**
 * activate_door - Perform door transition to target level/stage
 *
 * Input:
 *   door: pointer to door descriptor with target level/stage
 *
 * Marks the entry point as coming from a door, sets current level/stage
 * to the door's target, and loads the new level or stage as appropriate.
 *
 * If the target is in a different level, loads entire level data.
 * If the target is in the same level, loads only the stage data.
 */
void activate_door(const door_t* door);

#endif /* DOORS_H */
