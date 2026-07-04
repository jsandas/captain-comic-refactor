#include "../include/player_teleport.h"

void apply_teleport_destination_if_ready(uint8_t teleport_animation, uint8_t teleport_destination_x,
                                         uint8_t teleport_destination_y, int& comic_x,
                                         int& comic_y) {
    // Match original behavior: player position changes at frame 3.
    if (teleport_animation == 3) {
        comic_x = teleport_destination_x;
        comic_y = teleport_destination_y;
    }
}
