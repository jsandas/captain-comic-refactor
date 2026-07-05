#include "../include/doors.h"

#include <cstdint>

#include "../include/audio.h"
#include "../include/physics.h"

// Testing hook defined in header; see comment there.
bool g_skip_load_on_door = false;

DoorAnimationPhase g_door_anim_phase = DoorAnimationPhase::NONE;
uint8_t g_door_anim_frame = 0;
static uint8_t g_door_exit_delay_ticks = 0;

static uint8_t g_door_anim_world_x = DOOR_UNUSED;
static uint8_t g_door_anim_world_y = DOOR_UNUSED;
static uint8_t g_door_pending_level = 0;
static uint8_t g_door_pending_stage = 0;

/**
 * doors.cpp - Door system implementation
 *
 * Handles door collision detection, key checking, and level/stage transitions.
 * Matches the original assembly behavior for door mechanics.
 *
 * Reference: jsandas/comic-c src/doors.c
 */

/* External game state variables (defined in main.cpp) */
extern int comic_x;
extern int comic_y;
extern uint8_t comic_has_door_key;
extern uint8_t key_state_open;
extern uint8_t current_level_number;
extern uint8_t current_stage_number;
extern const level_t* current_level_ptr;

/* Source door tracking for proper reciprocal door positioning */
extern int8_t source_door_level_number;
extern int8_t source_door_stage_number;

/* External functions for level loading (defined in level_loader.cpp) */
extern void load_new_level();
extern void load_new_stage();

static void load_pending_door_destination() {
    const uint8_t target_level = g_door_pending_level;
    current_stage_number = g_door_pending_stage;
    current_level_number = target_level;

    if (target_level != static_cast<uint8_t>(source_door_level_number)) {
        load_new_level();
    } else {
        load_new_stage();
    }

    // Keep door overlay position in bounds if reciprocal spawn resolution fails.
    if (comic_x <= 0) {
        g_door_anim_world_x = 0;
    } else if (comic_x > 255) {
        g_door_anim_world_x = 255;
    } else {
        g_door_anim_world_x = static_cast<uint8_t>(comic_x - 1);
    }
    g_door_anim_world_y = static_cast<uint8_t>(comic_y);
}

/**
 * check_door_activation - Check if Comic is in front of a door and open key is pressed
 *
 * Scans all doors in the current stage looking for one that matches:
 * - Comic's Y coordinate (exact match)
 * - Comic's X coordinate (within 3 units)
 * - Player has the Door Key
 *
 * If found, activates the door and returns 1. Otherwise returns 0.
 *
 * Assembly reference: R5sw1991.asm:3952-3973
 */
uint8_t check_door_activation() {
    /* Validate preconditions */
    if (!current_level_ptr) {
        return 0; /* No level loaded */
    }

    if (current_stage_number >= 3) {
        return 0; /* Invalid stage number */
    }

    if (key_state_open != 1) {
        return 0; /* Open key not pressed */
    }

    if (g_door_anim_phase != DoorAnimationPhase::NONE) {
        return 0; /* Ignore re-entry while a door sequence is active */
    }

    /* Get current stage data */
    const stage_t* current_stage = &current_level_ptr->stages[current_stage_number];

    /* Check all doors in the current stage */
    for (int i = 0; i < MAX_NUM_DOORS; i++) {
        const door_t* door = &current_stage->doors[i];

        /* Door is unused if x or y is 0xff (DOOR_UNUSED) */
        if (door->x == DOOR_UNUSED || door->y == DOOR_UNUSED) {
            continue;
        }

        /* Check Y coordinate: must be exact match
         * Both comic_y and door->y are in game units (same coordinate system).
         * Door activation only occurs when Comic's Y exactly equals the door's Y. */
        if (comic_y != door->y) {
            continue;
        }

        /* Check X coordinate: must be within 3 units
         * Both comic_x and door->x are in game units
         * Comic is 2 units wide, so allows adjacent positioning */
        int x_offset = comic_x - door->x;
        if (x_offset < 0 || x_offset > 2) {
            continue;
        }

        /* Check if player has the Door Key */
        if (comic_has_door_key != 1) {
            continue; /* Door is locked, skip to next door */
        }

        /* All checks passed: activate this door */
        activate_door(door);
        return 1; /* Door was activated */
    }

    /* No doors activated */
    return 0;
}

void update_door_animation_tick() {
    if (g_door_anim_phase == DoorAnimationPhase::NONE) {
        return;
    }

    if (g_door_anim_phase == DoorAnimationPhase::ENTERING) {
        if (g_door_anim_frame < 3) {
            g_door_anim_frame++;
            return;
        }

        load_pending_door_destination();
        g_door_anim_phase = DoorAnimationPhase::EXIT_DELAY;
        g_door_anim_frame = 0;
        g_door_exit_delay_ticks = 3;
        return;
    }

    if (g_door_anim_phase == DoorAnimationPhase::EXIT_DELAY) {
        if (g_door_exit_delay_ticks > 0) {
            g_door_exit_delay_ticks--;
        }

        if (g_door_exit_delay_ticks == 0) {
            play_game_sound(GameSound::DOOR_OPEN);
            g_door_anim_phase = DoorAnimationPhase::EXITING;
            g_door_anim_frame = 0;
        }
        return;
    }

    if (g_door_anim_frame >= 4) {
        g_door_anim_phase = DoorAnimationPhase::NONE;
        g_door_anim_frame = 0;
        g_door_exit_delay_ticks = 0;
        g_door_anim_world_x = DOOR_UNUSED;
        g_door_anim_world_y = DOOR_UNUSED;
        return;
    }

    g_door_anim_frame++;
}

bool get_door_animation_render_state(uint8_t* world_x, uint8_t* world_y,
                                     DoorAnimationRenderMode* mode, bool* draw_overlay_in_front,
                                     bool* player_visible) {
    if (!world_x || !world_y || !mode || !draw_overlay_in_front || !player_visible ||
        g_door_anim_phase == DoorAnimationPhase::NONE) {
        return false;
    }

    if (g_door_anim_world_x == DOOR_UNUSED || g_door_anim_world_y == DOOR_UNUSED) {
        return false;
    }

    *world_x = g_door_anim_world_x;
    *world_y = g_door_anim_world_y;
    *mode = DoorAnimationRenderMode::NONE;
    *draw_overlay_in_front = false;
    *player_visible = true;

    if (g_door_anim_phase == DoorAnimationPhase::ENTERING) {
        switch (g_door_anim_frame) {
            case 0:
                *mode = DoorAnimationRenderMode::HALF_OPEN;
                *draw_overlay_in_front = false;
                *player_visible = true;
                return true;
            case 1:
                *mode = DoorAnimationRenderMode::FULL_OPEN;
                *draw_overlay_in_front = false;
                *player_visible = true;
                return true;
            case 2:
                *mode = DoorAnimationRenderMode::HALF_CLOSED;
                *draw_overlay_in_front = true;
                *player_visible = true;
                return true;
            default:
                *mode = DoorAnimationRenderMode::NONE;
                *player_visible = false;
                return true;
        }
    }

    if (g_door_anim_phase == DoorAnimationPhase::EXIT_DELAY) {
        *mode = DoorAnimationRenderMode::NONE;
        *player_visible = false;
        return true;
    }

    switch (g_door_anim_frame) {
        case 0:
            *mode = DoorAnimationRenderMode::NONE;
            *player_visible = false;
            return true;
        case 1:
            *mode = DoorAnimationRenderMode::HALF_OPEN;
            *draw_overlay_in_front = true;
            *player_visible = true;
            return true;
        case 2:
            *mode = DoorAnimationRenderMode::FULL_OPEN;
            *draw_overlay_in_front = false;
            *player_visible = true;
            return true;
        case 3:
            *mode = DoorAnimationRenderMode::HALF_CLOSED;
            *draw_overlay_in_front = false;
            *player_visible = true;
            return true;
        default:
            *mode = DoorAnimationRenderMode::NONE;
            *player_visible = true;
            return true;
    }
}

/**
 * activate_door - Perform door transition to target level/stage
 *
 * Plays door entry animation, updates level/stage numbers, and loads the
 * new level or stage data as appropriate.
 *
 * If the target is in a different level, calls load_new_level() which
 * loads tileset, enemy sprites, and all stage data for the new level.
 *
 * If the target is in the same level, calls load_new_stage() which
 * only loads the new stage tiles, enemies, and items.
 *
 * The source level/stage are saved so that the destination stage can
 * find the reciprocal door and position Comic there.
 *
 * Assembly reference: R5sw1991.asm:4618-4643
 */
void activate_door(const door_t* door) {
    /* Validate door target values before proceeding */
    if (door->target_stage >= 3) {
        return; /* Invalid target stage */
    }

    if (door->target_level >= 8) {
        return; /* Invalid target level */
    }

    /* Save source for reciprocal spawn after destination stage is loaded. */
    source_door_level_number = current_level_number;
    source_door_stage_number = current_stage_number;

    /* Shortcut for unit tests: avoid loading anything (which would clear
     * source_door_* and may print warnings if reciprocal door data is
     * missing). The tests still need to see the level/stage numbers update. */
    if (g_skip_load_on_door) {
        g_door_anim_phase = DoorAnimationPhase::NONE;
        g_door_anim_frame = 0;
        g_door_exit_delay_ticks = 0;
        current_stage_number = door->target_stage;
        current_level_number = door->target_level;
        return;
    }

    play_game_sound(GameSound::DOOR_OPEN);

    g_door_pending_level = door->target_level;
    g_door_pending_stage = door->target_stage;
    g_door_anim_world_x = door->x;
    g_door_anim_world_y = door->y;
    g_door_anim_phase = DoorAnimationPhase::ENTERING;
    g_door_anim_frame = 0;
    g_door_exit_delay_ticks = 0;
}
