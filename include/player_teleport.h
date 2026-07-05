#ifndef PLAYER_TELEPORT_H
#define PLAYER_TELEPORT_H

#include <cstdint>

// Apply the teleport destination on the frame where teleport movement should occur.
// This routine intentionally updates only the live player position, not respawn checkpoints.
void apply_teleport_destination_if_ready(uint8_t teleport_animation, uint8_t teleport_destination_x,
                                         uint8_t teleport_destination_y, int& comic_x,
                                         int& comic_y);

#endif  // PLAYER_TELEPORT_H
