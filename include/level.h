#ifndef LEVEL_H
#define LEVEL_H

/**
 * level.h - Level data exports (umbrella header)
 *
 * The header is layered:
 *   - level_format.h  ... the on-disk PT/SHP data model (shp_t, stage_t, level_t, ...)
 *   - level_enums.h   ... gameplay constants referenced by that model
 *                       (behaviors, item types, level numbers, ...)
 *   - this file       ... the compiled-in level data exports and lookup API
 *
 * Including level.h gives you everything, exactly as before the split.
 */

#include <cstdint>
#include <string>

#include "level_enums.h"
#include "level_format.h"

/* Array of pointers to all 8 levels */
extern const level_t* const level_data_pointers[8];

/* Individual level data exports */
extern const level_t level_data_lake;
extern const level_t level_data_forest;
extern const level_t level_data_space;
extern const level_t level_data_base;
extern const level_t level_data_cave;
extern const level_t level_data_shed;
extern const level_t level_data_castle;
extern const level_t level_data_comp;

/**
 * Get level data by name (case-insensitive)
 * Returns nullptr if level name not found
 */
const level_t* get_level_by_name(const std::string& level_name);

/**
 * Get level data by level number (0-7)
 * Returns nullptr if level number is invalid
 */
const level_t* get_level_by_number(int level_number);

#endif /* LEVEL_H */
