#include "../include/player_teleport.h"

#include <algorithm>

#include "../include/audio.h"
#include "../include/game_state.h"
#include "../include/physics.h"

void begin_teleport() {
    constexpr uint8_t TELEPORT_DISTANCE = 6;

    uint8_t dest_x = static_cast<uint8_t>(comic_x);
    uint8_t dest_y = static_cast<uint8_t>(comic_y);
    const int camera_rel_x = static_cast<int>(comic_x) - camera_x;

    teleport_camera_counter = 0;

    if (comic_facing == COMIC_FACING_LEFT) {
        teleport_camera_vel = -1;

        if (camera_rel_x >= TELEPORT_DISTANCE) {
            dest_x = static_cast<uint8_t>(dest_x - TELEPORT_DISTANCE);

            const int dest_camera_rel = camera_rel_x - TELEPORT_DISTANCE;
            if (dest_camera_rel < (PLAYFIELD_WIDTH / 2 - 2)) {
                const int camera_movement = (PLAYFIELD_WIDTH / 2 - 2) - dest_camera_rel;
                teleport_camera_counter = static_cast<uint8_t>(std::min(camera_x, camera_movement));
            }
        }
    } else {
        teleport_camera_vel = 1;

        if (camera_rel_x < (PLAYFIELD_WIDTH - TELEPORT_DISTANCE - 1)) {
            dest_x = static_cast<uint8_t>(dest_x + TELEPORT_DISTANCE);

            const int dest_camera_rel = camera_rel_x + TELEPORT_DISTANCE;
            if (dest_camera_rel > (PLAYFIELD_WIDTH / 2)) {
                const int max_camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
                const int camera_movement = dest_camera_rel - (PLAYFIELD_WIDTH / 2);
                if (camera_x + camera_movement <= max_camera_x) {
                    teleport_camera_counter = static_cast<uint8_t>(camera_movement);
                } else {
                    teleport_camera_counter = static_cast<uint8_t>(max_camera_x - camera_x);
                }
            }
        }
    }

    // Round destination to an even tile boundary and search down the column
    // for a solid tile with two empty tiles above (safe landing).
    dest_x &= 0xFE;

    bool solid_found = false;
    uint8_t search_y = static_cast<uint8_t>(PLAYFIELD_HEIGHT - 2);

    while (search_y > 0 && !solid_found) {
        if (is_tile_solid(get_tile_at(dest_x, search_y))) {
            uint8_t nonsolid_count = 0;
            uint8_t probe_y = search_y;

            while (probe_y > 0) {
                probe_y = static_cast<uint8_t>(probe_y - 2);

                if (is_tile_solid(get_tile_at(dest_x, probe_y))) {
                    break;
                }

                nonsolid_count++;
                if (nonsolid_count >= 2) {
                    dest_y = probe_y;
                    solid_found = true;
                    break;
                }
            }
        }

        if (!solid_found) {
            search_y = static_cast<uint8_t>(search_y - 2);
        }
    }

    if (!solid_found) {
        dest_x = static_cast<uint8_t>(comic_x);
        dest_y = static_cast<uint8_t>(comic_y);
        teleport_camera_counter = 0;
    }

    teleport_animation = 0;
    teleport_source_x = static_cast<uint8_t>(comic_x);
    teleport_source_y = static_cast<uint8_t>(comic_y);
    teleport_destination_x = dest_x;
    teleport_destination_y = dest_y;
    comic_is_teleporting = true;
    teleport_skip_tick = true;

    play_game_sound(GameSound::TELEPORT);
}

void process_teleport_input(bool comic_has_teleport_wand) {
    if (key_state_teleport && !previous_key_state_teleport && comic_has_teleport_wand) {
        begin_teleport();
    }

    previous_key_state_teleport = key_state_teleport;
}

void handle_teleport_tick() {
    if (!comic_is_teleporting) {
        return;
    }

    if (teleport_skip_tick) {
        teleport_skip_tick = false;
        return;
    }

    if (teleport_camera_counter > 0) {
        const int max_camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
        const int moved_camera_x = camera_x + teleport_camera_vel;
        camera_x = std::max(0, std::min(moved_camera_x, max_camera_x));
        teleport_camera_counter--;
    }

    teleport_animation++;

    apply_teleport_destination_if_ready(teleport_animation, teleport_destination_x,
                                        teleport_destination_y, comic_x, comic_y);

    if (teleport_animation >= 6) {
        comic_is_teleporting = false;
    }
}

void apply_teleport_destination_if_ready(uint8_t animation_frame, uint8_t destination_x,
                                         uint8_t destination_y, int& player_x_out,
                                         int& player_y_out) {
    // Match original behavior: player position changes at frame 3.
    if (animation_frame == 3) {
        player_x_out = destination_x;
        player_y_out = destination_y;
    }
}
