#ifndef PLAYER_TELEPORT_H
#define PLAYER_TELEPORT_H

#include <cstdint>

/**
 * Teleport wand feature.
 *
 * The state machine's globals (comic_is_teleporting, teleport_animation,
 * teleport_source_*, teleport_destination_*, teleport_camera_*, and
 * teleport_skip_tick) live in game_state.h.  This module holds the code:
 *
 *   - begin_teleport: pick a safe landing spot ~6 tiles away and start the
 *     beam-out / beam-in animation (assembly Teleport routine).
 *   - process_teleport_input: per-tick edge-trigger activation.
 *   - handle_teleport_tick: advance the animation one tick.
 */

/**
 * Begin a teleport from Comic's current position.
 *
 * Computes a destination ~6 tiles toward Comic's facing direction, rounds it
 * to an even tile column, and searches down that column for a solid tile with
 * two empty tiles above it (safe landing).  If no spot is found the move is a
 * no-op that still plays the animation in place.
 */
void begin_teleport();

/**
 * Process teleport key input once per tick (edge-triggered).
 *
 * Activates begin_teleport() on the rising edge of the teleport key, but only
 * when the player owns the teleport wand.
 */
void process_teleport_input(bool comic_has_teleport_wand);

/**
 * Advance the teleport animation by one tick.
 *
 * Pans the camera toward the destination for the first ticks, then applies
 * the destination at animation frame 3 (see
 * apply_teleport_destination_if_ready).  No-op when not teleporting.
 */
void handle_teleport_tick();

// Apply the teleport destination on the frame where teleport movement should occur.
// This routine intentionally updates only the live player position (written to
// player_x_out / player_y_out), not respawn checkpoints.
void apply_teleport_destination_if_ready(uint8_t animation_frame, uint8_t destination_x,
                                         uint8_t destination_y, int& player_x_out,
                                         int& player_y_out);

#endif  // PLAYER_TELEPORT_H
