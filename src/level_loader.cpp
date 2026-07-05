/**
 * level_loader.cpp - Runtime level data initialization
 *
 * Initializes level data with pre-generated tile data from level_tiles.cpp.
 * Tile data is compiled directly into the executable as C++ hex arrays,
 * eliminating all runtime file I/O dependencies.
 */

#include "../include/level_loader.h"

#include <cstring>
#include <iostream>

#include "../include/actors.h"
#include "../include/graphics.h"
#include "../include/level_tiles.h"
#include "../include/physics.h"

/* External game state (defined in main.cpp) */
extern int comic_x;
extern int comic_y;
extern int8_t comic_y_vel;
extern uint8_t current_level_number;
extern uint8_t current_stage_number;
extern const level_t* current_level_ptr;
extern int8_t source_door_level_number;
extern int8_t source_door_stage_number;
extern uint8_t comic_x_checkpoint;
extern uint8_t comic_y_checkpoint;
extern int camera_x;
extern GraphicsSystem* g_graphics;
extern ActorSystem* g_actor_system;

/* Runtime mutable copies of level data */
static level_t runtime_levels[8];
static bool levels_initialized = false;

/* Names corresponding to level numbers */
static const char* level_names[] = {"lake", "forest", "space",  "base",
                                    "cave", "shed",   "castle", "comp"};

/* Pointers to source const data */
static const level_t* source_levels[8] = {&level_data_lake,   &level_data_forest, &level_data_space,
                                          &level_data_base,   &level_data_cave,   &level_data_shed,
                                          &level_data_castle, &level_data_comp};

/* Pre-generated tile data arrays (from level_tiles.cpp) */
static const uint8_t* tile_arrays[8][3] = {
    {lake_stage_0_tiles, lake_stage_1_tiles, lake_stage_2_tiles},
    {forest_stage_0_tiles, forest_stage_1_tiles, forest_stage_2_tiles},
    {space_stage_0_tiles, space_stage_1_tiles, space_stage_2_tiles},
    {base_stage_0_tiles, base_stage_1_tiles, base_stage_2_tiles},
    {cave_stage_0_tiles, cave_stage_1_tiles, cave_stage_2_tiles},
    {shed_stage_0_tiles, shed_stage_1_tiles, shed_stage_2_tiles},
    {castle_stage_0_tiles, castle_stage_1_tiles, castle_stage_2_tiles},
    {comp_stage_0_tiles, comp_stage_1_tiles, comp_stage_2_tiles}};

void initialize_level_data() {
    if (levels_initialized) {
        return;
    }

    /* Copy const level data into runtime structures and populate with tile data */
    for (int level_num = 0; level_num < 8; level_num++) {
        /* Copy entire const level data into runtime mutable copy */
        std::memcpy(&runtime_levels[level_num], source_levels[level_num], sizeof(level_t));

        /* Overwrite tile data with pre-generated arrays for each of the 3 stages */
        for (int stage_num = 0; stage_num < 3; stage_num++) {
            std::memcpy(runtime_levels[level_num].stages[stage_num].tiles,
                        tile_arrays[level_num][stage_num], 128 * 10);
        }
    }

    levels_initialized = true;
}

level_t* get_level_data(const std::string& level_name) {
    if (!levels_initialized) {
        std::cerr << "Error: Level data not initialized. Call initialize_level_data() first."
                  << std::endl;
        return nullptr;
    }

    const level_t* result = get_level_by_name(level_name);
    if (!result) {
        std::cerr << "Error: Unknown level: " << level_name << std::endl;
        return nullptr;
    }

    /* Find which level this is */
    for (int i = 0; i < 8; i++) {
        if (source_levels[i] == result) {
            return &runtime_levels[i];
        }
    }

    return nullptr;
}

/**
 * load_new_level - Load a new level's data and assets
 *
 * Called when transitioning to a different level (via door or level change).
 * Loads the level's tileset graphics and initializes the first stage.
 */
void load_new_level() {
    if (!levels_initialized) {
        std::cerr << "Error: Level data not initialized!" << std::endl;
        return;
    }

    /* Validate level number */
    if (current_level_number >= 8) {
        std::cerr << "Error: Invalid level number: " << static_cast<int>(current_level_number)
                  << std::endl;
        return;
    }

    /* Set current level pointer to runtime data (includes populated tiles) */
    current_level_ptr = &runtime_levels[current_level_number];

    /* Load tileset graphics for this level */
    if (g_graphics) {
        std::string level_name = level_names[current_level_number];
        if (!g_graphics->load_tileset(level_name)) {
            std::cerr << "Warning: Failed to load tileset for level: " << level_name << std::endl;
            /* Continue anyway - may work with existing tileset */
        }

        const bool has_lantern =
            (g_actor_system != nullptr) && (g_actor_system->comic_has_lantern == 1);
        const bool should_blackout_tiles =
            (current_level_number == LEVEL_NUMBER_CASTLE) && !has_lantern;
        g_graphics->set_tileset_blackout(level_name, should_blackout_tiles);
    }

    /* Load the current stage */
    load_new_stage();
}

/**
 * load_new_stage - Load a new stage within the current level
 *
 * Called when transitioning to a different stage (via door, stage boundary, or level load).
 * Loads the stage tile map into the physics system and positions Comic appropriately.
 */
void load_new_stage() {
    if (!levels_initialized) {
        std::cerr << "Error: Level data not initialized!" << std::endl;
        return;
    }

    if (!current_level_ptr) {
        std::cerr << "Error: No level loaded!" << std::endl;
        return;
    }

    /* Validate stage number */
    if (current_stage_number >= 3) {
        std::cerr << "Error: Invalid stage number: " << static_cast<int>(current_stage_number)
                  << std::endl;
        return;
    }

    /* Load stage tiles into physics system */
    std::string level_name = level_names[current_level_number];
    if (!load_stage_tiles(level_name, current_stage_number)) {
        std::cerr << "Error: Failed to load stage tiles for " << level_name << " stage "
                  << static_cast<int>(current_stage_number) << std::endl;
        return;
    }

    /* Handle entry positioning */
    if (source_door_level_number >= 0) {
        /* Entering via door - find reciprocal door and position Comic there */
        const stage_t& stage = current_level_ptr->stages[current_stage_number];

        /* Search for door that points back to source level/stage */
        bool found_door = false;
        for (int i = 0; i < MAX_NUM_DOORS; i++) {
            const door_t& door = stage.doors[i];

            /* Skip unused doors */
            if (door.x == DOOR_UNUSED || door.y == DOOR_UNUSED) {
                continue;
            }

            /* Check if this door links back to where we came from */
            if (door.target_level == source_door_level_number &&
                door.target_stage == source_door_stage_number) {
                /* Found reciprocal door - position Comic in front of it */
                comic_x = door.x + 1; /* Center Comic in door (door is 2 units wide) */
                comic_y = door.y;
                comic_y_vel = 0;

                /* Entering through a door establishes the new respawn checkpoint. */
                comic_x_checkpoint = static_cast<uint8_t>(comic_x);
                comic_y_checkpoint = static_cast<uint8_t>(comic_y);

                /* Set camera to follow Comic */
                camera_x = comic_x - (PLAYFIELD_WIDTH / 2);
                if (camera_x < 0) camera_x = 0;
                if (camera_x > MAP_WIDTH - PLAYFIELD_WIDTH) {
                    camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
                }

                found_door = true;
                break;
            }
        }

        if (!found_door) {
            std::cerr << "Warning: Could not find reciprocal door from level "
                      << static_cast<int>(source_door_level_number) << " stage "
                      << static_cast<int>(source_door_stage_number) << std::endl;
        }

        /* Clear door entry flag */
        source_door_level_number = -1;
        source_door_stage_number = -1;
    } else {
        /* Boundary transition or initial spawn - Comic position already set, update camera */
        camera_x = comic_x - (PLAYFIELD_WIDTH / 2);
        if (camera_x < 0) camera_x = 0;
        if (camera_x > MAP_WIDTH - PLAYFIELD_WIDTH) {
            camera_x = MAP_WIDTH - PLAYFIELD_WIDTH;
        }
    }
}
