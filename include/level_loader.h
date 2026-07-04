#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include <string>

#include "level.h"

/**
 * Initialize all level data
 *
 * Populates runtime level structures with pre-generated compiled-in tile data.
 * Call this once at game startup before using any level data.
 *
 */
void initialize_level_data();

/**
 * Get runtime-initialized level data by name (case-insensitive)
 * Returns nullptr if level not found
 *
 * The returned level_t contains fully loaded tile data for all stages.
 */
level_t* get_level_data(const std::string& level_name);

/**
 * load_new_level - Load a new level's data and assets
 *
 * Initializes the tileset for the current level and loads the first stage.
 * Called when a door leads to a different level.
 *
 * Requires current_level_number to be set before calling.
 */
void load_new_level();

/**
 * load_new_stage - Load a new stage within the current level
 *
 * Loads the stage tile map and positions Comic appropriately.
 * Handles door entry positioning and camera setup.
 * Called for stage transitions within same level or at startup.
 *
 * Requires current_level_ptr and current_stage_number to be set before calling.
 */
void load_new_stage();

#endif /* LEVEL_LOADER_H */
