#include "../include/physics.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "../include/audio.h"
#include "../include/level_loader.h"

// Cheat system flags
bool cheat_noclip = false;

// External game state (defined in main.cpp)
extern int comic_x;
extern int comic_y;
extern int8_t comic_y_vel;
extern int8_t comic_x_momentum;
extern uint8_t comic_facing;
extern uint8_t comic_is_falling_or_jumping;
extern uint8_t comic_jump_counter;
extern uint8_t comic_jump_power;
extern uint8_t key_state_jump;
extern uint8_t previous_key_state_jump;
extern uint8_t key_state_left;
extern uint8_t key_state_right;
extern int camera_x;

// Level/stage tracking (defined in main.cpp)
extern uint8_t current_level_number;
extern uint8_t current_stage_number;
extern const level_t* current_level_ptr;
extern int8_t source_door_level_number;
extern int8_t source_door_stage_number;
extern uint8_t comic_hp;
extern uint8_t comic_num_lives;

// Game-over flag from main.cpp
extern bool game_over_triggered;

// Checkpoint position (defined in main.cpp)
extern uint8_t comic_y_checkpoint;
extern uint8_t comic_x_checkpoint;

// Tile map data
static uint8_t current_tiles[MAP_WIDTH_TILES * MAP_HEIGHT_TILES];
static uint8_t tileset_last_passable = 0x3F;  // Tiles > this are solid

// Ceiling stick flag
static bool ceiling_stick_flag = false;

// Player death sequence state
static bool player_is_dying = false;
static bool player_death_too_bad_phase = false;
static bool player_death_show_animation = true;
static bool player_death_fall_clip_render = false;
static uint8_t player_death_ticks_remaining = 0;
constexpr uint8_t PLAYER_DEATH_ANIMATION_TICKS = 8;
constexpr uint8_t PLAYER_DEATH_TOO_BAD_TICKS = 15;

bool is_player_dying() { return player_is_dying; }

bool should_show_player_death_animation() {
    return player_is_dying && !player_death_too_bad_phase && player_death_show_animation;
}

bool should_clip_player_death_render() {
    return player_is_dying && !player_death_too_bad_phase && player_death_fall_clip_render;
}

void trigger_player_death(bool show_animation, bool fall_clip_render) {
    if (player_is_dying) {
        return;
    }

    player_is_dying = true;
    player_death_too_bad_phase = false;
    player_death_show_animation = show_animation;
    player_death_fall_clip_render = fall_clip_render;
    player_death_ticks_remaining = show_animation ? PLAYER_DEATH_ANIMATION_TICKS : 0;

    comic_y_vel = 0;
    comic_x_momentum = 0;
    comic_is_falling_or_jumping = 0;

    if (show_animation) {
        play_game_sound(GameSound::PLAYER_DIE);
    }
}

void update_player_death_sequence() {
    if (!player_is_dying) {
        return;
    }

    if (player_death_ticks_remaining > 0) {
        player_death_ticks_remaining--;
    }

    if (player_death_ticks_remaining == 0) {
        if (!player_death_too_bad_phase) {
            // Animation done — play "too bad" jingle and wait before respawning.
            // This matches comic_dies() in the reference: play_sound(SOUND_TOO_BAD, 2)
            // followed by wait_n_ticks(15) before losing a life / respawning.
            player_death_too_bad_phase = true;
            player_death_ticks_remaining = PLAYER_DEATH_TOO_BAD_TICKS;
            play_game_sound(GameSound::TOO_BAD);
            return;
        }

        if (comic_num_lives == 0) {
            player_is_dying = false;
            player_death_too_bad_phase = false;
            player_death_show_animation = true;
            player_death_fall_clip_render = false;
            game_over_triggered = true;
            return;
        }

        // Match DOS flow (comic_dies): if lives are nonzero, respawn and then
        // subtract one life. This allows one more play when lives reaches 0.
        comic_num_lives--;

        player_is_dying = false;
        player_death_too_bad_phase = false;
        player_death_show_animation = true;
        player_death_fall_clip_render = false;

        // Respawn at checkpoint after the death animation completes.
        comic_x = comic_x_checkpoint;
        comic_y = comic_y_checkpoint;

        // Recenter camera on the respawned player, clamped to stage bounds.
        camera_x = comic_x - (PLAYFIELD_WIDTH / 2);
        if (camera_x < 0) {
            camera_x = 0;
        }
        if (camera_x > MAP_WIDTH - PLAYFIELD_WIDTH) {
            camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
        }

        comic_y_vel = 0;
        comic_x_momentum = 0;
        comic_is_falling_or_jumping = 1;
        comic_jump_counter = comic_jump_power;
        comic_hp = MAX_HP;

        // Clear jump key state so holding jump during death does not trigger an
        // immediate jump on the first tick after respawn.
        key_state_jump = 0;
        previous_key_state_jump = 0;
        game_over_triggered = false;
    }
}

void init_test_level() {
    // Initialize empty level
    std::memset(current_tiles, 0, sizeof(current_tiles));

    // Use tile ID 0x3F (last valid tile) for visible platforms
    // Valid tile range is 0x00-0x3F (64 tiles from tileset)
    // Update tileset_last_passable to mark 0x3F as solid for collision
    tileset_last_passable = 0x3E;

    // Create ground floor (row 9, bottom row)
    for (int x = 0; x < MAP_WIDTH_TILES; x++) {
        current_tiles[9 * MAP_WIDTH_TILES + x] = 0x3F;  // Solid platform tile
    }

    // Add some walls for testing
    // Left wall
    for (int y = 5; y < 9; y++) {
        current_tiles[y * MAP_WIDTH_TILES + 10] = 0x3F;
    }

    // Right wall
    for (int y = 5; y < 9; y++) {
        current_tiles[y * MAP_WIDTH_TILES + 30] = 0x3F;
    }

    // Platform in the middle
    for (int x = 15; x < 25; x++) {
        current_tiles[7 * MAP_WIDTH_TILES + x] = 0x3F;
    }
}

void reset_level_tiles() {
    // Reset physics module internal state (tile map and solidity threshold) to clean/empty state
    // This is useful for test cleanup to ensure one test doesn't affect the next
    std::memset(current_tiles, 0, sizeof(current_tiles));
    tileset_last_passable = 0x3F;  // Default threshold (tiles > 0x3F are solid)
    ceiling_stick_flag = false;
}

uint8_t get_tile_at(uint8_t x, uint8_t y) {
    // Convert game units to tile coordinates (divide by 2)
    uint8_t tile_x = x / 2;
    uint8_t tile_y = y / 2;

    // Bounds check
    if (tile_x >= MAP_WIDTH_TILES || tile_y >= MAP_HEIGHT_TILES) {
        return 0;  // Return passable tile if out of bounds
    }

    return current_tiles[tile_y * MAP_WIDTH_TILES + tile_x];
}

bool is_tile_solid(uint8_t tile_id) {
    // Cheat: noclip disables all collision detection
    if (cheat_noclip) {
        return false;
    }

    // Check if tile is a door or door frame tile (always passable)
    if (current_level_ptr != nullptr) {
        // Check door tiles
        if (tile_id == current_level_ptr->door_tile_ul ||
            tile_id == current_level_ptr->door_tile_ur ||
            tile_id == current_level_ptr->door_tile_ll ||
            tile_id == current_level_ptr->door_tile_lr) {
            return false;  // Door tiles are passable
        }

        // Check door frame tiles array
        for (int i = 0; i < 8; i++) {
            if (current_level_ptr->door_frame_tiles[i] != 0 &&
                tile_id == current_level_ptr->door_frame_tiles[i]) {
                return false;  // Door frame tiles are passable
            }
        }
    }

    // Normal solid tile check
    return tile_id > tileset_last_passable;
}

void process_jump_input() {
    if (comic_is_falling_or_jumping == 0 && key_state_jump && !previous_key_state_jump &&
        comic_jump_counter != 1) {
        comic_is_falling_or_jumping = 1;
        // Note: Original game had no jump sound
    }

    previous_key_state_jump = key_state_jump;
}

void handle_fall_or_jump() {
    if (player_is_dying) {
        return;
    }

    if (comic_is_falling_or_jumping) {
        // STEP 1: Decrement jump counter
        if (comic_jump_counter > 0) {
            comic_jump_counter--;
        }

        // STEP 2: Check if counter expired, set to 1 as sentinel
        if (comic_jump_counter == 0) {
            comic_jump_counter = 1;
            ceiling_stick_flag = false;
        }
        // STEP 3: Apply upward acceleration if counter > 0 and jump key held
        else if (key_state_jump) {
            comic_y_vel -= JUMP_ACCELERATION;
        } else {
            ceiling_stick_flag = false;
        }

        // STEP 4: Integrate velocity (divide by 8) and clamp top boundary
        int delta_y = comic_y_vel >> 3;
        int new_y = comic_y + delta_y;
        comic_y = std::max(0, new_y);

        // Apply ceiling stick (push down 1 unit if against ceiling)
        if (ceiling_stick_flag) {
            comic_y++;
            ceiling_stick_flag = false;
        }

        // Bounds check: death if too far down
        if (comic_y >= PLAYFIELD_HEIGHT - 3) {
            trigger_player_death(false, true);
            return;
        }

        // STEP 5: Apply gravity (reduced in space level)
        if (current_level_number == LEVEL_NUMBER_SPACE) {
            comic_y_vel += COMIC_GRAVITY_SPACE;
        } else {
            comic_y_vel += COMIC_GRAVITY;
        }
        if (comic_y_vel > TERMINAL_VELOCITY) {
            comic_y_vel = TERMINAL_VELOCITY;
        }

        // STEP 6: Handle mid-air momentum (simplified for now)
        if (key_state_left) {
            comic_x_momentum--;
            if (comic_x_momentum < -5) {
                comic_x_momentum = -5;
            }
        }

        if (key_state_right) {
            comic_x_momentum++;
            if (comic_x_momentum > 5) {
                comic_x_momentum = 5;
            }
        }

        // Apply horizontal movement with drag
        if (comic_x_momentum < 0) {
            comic_x_momentum++;  // Drag toward zero
            move_left();
        }

        if (comic_x_momentum > 0) {
            comic_x_momentum--;  // Drag toward zero
            move_right();
        }

        // STEP 7: Check ceiling collision (upward)
        if (comic_y_vel < 0) {
            uint8_t head_tile = get_tile_at(comic_x, comic_y);
            bool head_solid = is_tile_solid(head_tile);

            // Also check tile to the right if between tiles
            if (!head_solid && (comic_x & 1)) {
                head_tile = get_tile_at(comic_x + 1, comic_y);
                head_solid = is_tile_solid(head_tile);
            }

            if (head_solid) {
                // Hit ceiling: stick and reset velocity
                ceiling_stick_flag = true;
                comic_y_vel = 0;
            }
        }

        // STEP 8: Check ground collision (downward)
        if (comic_y_vel > 0) {
            // Check 1 unit below Comic's feet (comic_y + 5)
            uint8_t foot_y = comic_y + 5;
            uint8_t foot_tile = get_tile_at(comic_x, foot_y);
            bool foot_solid = is_tile_solid(foot_tile);

            // Also check tile to the right if between tiles
            if (!foot_solid && (comic_x & 1)) {
                foot_tile = get_tile_at(comic_x + 1, foot_y);
                foot_solid = is_tile_solid(foot_tile);
            }

            if (foot_solid) {
                // Landing: snap to nearest even boundary below foot probe
                // (matches assembly: clear low bit of comic_y + 1)
                comic_y = (comic_y + 1) & 0xFE;
                comic_is_falling_or_jumping = 0;
                comic_y_vel = 0;
                comic_x_momentum = 0;
                return;
            }
        }
    } else {
        // Recharge jump counter when on ground and not pressing jump
        // (jump initiation now happens in main loop with edge detection)
        if (!key_state_jump) {
            comic_jump_counter = comic_jump_power;
        }

        // Check if we should start falling (no ground beneath)
        // Assembly game_loop.check_for_floor probes at comic_y + 4
        uint8_t foot_y = comic_y + 4;
        uint8_t foot_tile = get_tile_at(comic_x, foot_y);
        bool foot_solid = is_tile_solid(foot_tile);

        if (!foot_solid && (comic_x & 1)) {
            foot_tile = get_tile_at(comic_x + 1, foot_y);
            foot_solid = is_tile_solid(foot_tile);
        }

        if (!foot_solid) {
            // Match original edge-walk behavior: immediately enter falling with
            // an initial downward speed (1 unit/tick) and depleted jump counter.
            comic_y_vel = 8;
            if (comic_x_momentum > 0) {
                comic_x_momentum = 2;
            } else if (comic_x_momentum < 0) {
                comic_x_momentum = -2;
            }
            comic_is_falling_or_jumping = 1;
            comic_jump_counter = 1;
        }
    }
}

bool move_left() {
    // Check if at left edge of stage
    if (comic_x == 0) {
        // Guard against NULL level pointer
        if (current_level_ptr == nullptr) {
            comic_x_momentum = 0;
            return false;
        }

        // Validate stage number is within bounds (0-2)
        if (current_stage_number >= 3) {
            comic_x_momentum = 0;
            return false;
        }

        const stage_t* stage = &current_level_ptr->stages[current_stage_number];

        // Check if there's a left exit
        if (stage->exit_l == EXIT_UNUSED) {
            // No exit here, stop moving
            comic_x_momentum = 0;
            return false;
        }

        // Stage transition to the left
        play_game_sound(GameSound::STAGE_TRANSITION);

        current_stage_number = stage->exit_l;
        comic_y_vel = 0;

        // Update checkpoint for spawn position on new stage
        comic_y_checkpoint = comic_y;
        comic_x_checkpoint = MAP_WIDTH - 2;  // Far right of new stage (254)

        // Position at far right edge of new stage
        comic_x = MAP_WIDTH - 2;

        // Mark as boundary transition (not door)
        source_door_level_number = -1;

        // Load the new stage
        load_new_stage();
        return true;
    }

    int new_x = comic_x - 1;
    uint8_t check_y = comic_y + 3;  // Check at knees

    // Check if we'd hit a wall
    uint8_t tile_id = get_tile_at(new_x, check_y);
    if (is_tile_solid(tile_id)) {
        comic_x_momentum = 0;
        return false;
    }

    // Can move left
    comic_x = new_x;
    comic_facing = 0;  // Left

    // Move camera left if appropriate
    int relative_x = comic_x - camera_x;
    if (camera_x > 0 && relative_x < (PLAYFIELD_WIDTH / 2 - 2)) {
        camera_x--;
    }
    return true;
}

bool move_right() {
    // Check if at right edge of stage
    if (comic_x >= MAP_WIDTH - 2) {
        // Guard against NULL level pointer
        if (current_level_ptr == nullptr) {
            comic_x_momentum = 0;
            return false;
        }

        // Validate stage number is within bounds (0-2)
        if (current_stage_number >= 3) {
            comic_x_momentum = 0;
            return false;
        }

        const stage_t* stage = &current_level_ptr->stages[current_stage_number];

        // Check if there's a right exit
        if (stage->exit_r == EXIT_UNUSED) {
            // No exit here, stop moving
            comic_x_momentum = 0;
            return false;
        }

        // Stage transition to the right
        play_game_sound(GameSound::STAGE_TRANSITION);

        current_stage_number = stage->exit_r;
        comic_y_vel = 0;

        // Update checkpoint for spawn position on new stage
        comic_y_checkpoint = comic_y;
        comic_x_checkpoint = 0;  // Far left of new stage

        // Position at far left edge of new stage
        comic_x = 0;

        // Mark as boundary transition (not door)
        source_door_level_number = -1;

        // Load the new stage
        load_new_stage();
        return true;
    }

    int new_x = comic_x + 1;
    uint8_t check_y = comic_y + 3;     // Check at knees
    uint8_t check_tile_x = new_x + 1;  // Check right edge (player is 2 units wide)

    // Check if we'd hit a wall
    uint8_t tile_id = get_tile_at(check_tile_x, check_y);
    if (is_tile_solid(tile_id)) {
        comic_x_momentum = 0;
        return false;
    }

    // Can move right
    comic_x = new_x;
    comic_facing = 1;  // Right

    // Move camera right if appropriate
    int max_camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
    int relative_x = comic_x - camera_x;
    if (camera_x < max_camera_x && relative_x > (PLAYFIELD_WIDTH / 2)) {
        camera_x++;
    }
    return true;
}
bool load_stage_tiles(const std::string& level_name, int stage_number) {
    // Get the level data (which has been pre-loaded with tiles)
    level_t* level = get_level_data(level_name);
    if (!level) {
        std::cerr << "Failed to load level: " << level_name << std::endl;
        return false;
    }

    // Validate stage number
    if (stage_number < 0 || stage_number >= 3) {
        std::cerr << "Invalid stage number: " << stage_number << " (must be 0-2)" << std::endl;
        return false;
    }

    // Copy tile data from the stage structure to the current map
    const stage_t& stage = level->stages[stage_number];
    std::memcpy(current_tiles, stage.tiles, sizeof(current_tiles));

    // Load level-specific tileset_last_passable threshold from compiled-in level metadata
    // This value comes from level_data.cpp (initialized via initialize_level_data())
    // and determines which tiles are solid obstacles
    tileset_last_passable = level->tileset_last_passable;

    return true;
}