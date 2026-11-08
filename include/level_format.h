#ifndef LEVEL_FORMAT_H
#define LEVEL_FORMAT_H

#include <cstdint>
#include <string>

/**
 * level_format.h - The on-disk PT/SHP file model
 *
 * These structs mirror the original 1988 DOS data files exactly (tile maps,
 * stage descriptors, .SHP sprite descriptors). They are the data contract of
 * the game files and must stay layout-faithful; gameplay constants and level
 * data exports live in level_enums.h / level.h.
 */

/* Maximum counts */
constexpr int MAX_NUM_ENEMIES = 4;
constexpr int MAX_NUM_DOORS = 3;

/* Exit and door unused markers (stored in the file bytes) */
constexpr uint8_t EXIT_UNUSED = 0xff;
constexpr uint8_t DOOR_UNUSED = 0xff;

/* SHP (sprite) file markers */
constexpr uint8_t SHP_UNUSED = 0x00;

/**
 * shp_t - Sprite (.SHP file) descriptor
 *
 * Describes properties of an enemy sprite sheet file, including animation
 * parameters and the filename to load.
 */
struct shp_t {
    uint8_t num_distinct_frames; /* Number of animation frames in file (0=unused) */
    uint8_t horizontal;          /* ENEMY_HORIZONTAL_DUPLICATED or ENEMY_HORIZONTAL_SEPARATE */
    uint8_t animation;           /* ENEMY_ANIMATION_LOOP or ENEMY_ANIMATION_ALTERNATE */
    char filename[14];           /* .SHP filename (null-padded or space-padded to 14 bytes) */
};

/**
 * door_t - Door descriptor
 *
 * Describes a door that connects stages within the game. Doors are
 * transitions between different stage maps, either within the same level
 * or to different levels entirely.
 */
struct door_t {
    uint8_t y;            /* Y coordinate in game units */
    uint8_t x;            /* X coordinate in game units */
    uint8_t target_level; /* Target level number (0-7) */
    uint8_t target_stage; /* Target stage number within target level (0-2) */
};

/**
 * enemy_record_t - Enemy spawn record
 *
 * Defines an enemy that spawns in a stage, referencing a sprite file
 * and behavior type.
 */
struct enemy_record_t {
    uint8_t shp_index; /* Index into level.shp array (0-3) */
    uint8_t behavior;  /* ENEMY_BEHAVIOR_* constant (may include FAST flag) */
};

/**
 * stage_t - Single stage descriptor
 *
 * A stage is a single 128×10 map tile. Each level contains 3 stages.
 * Stages are connected by exits (left/right boundaries) and doors
 * (special transitions).
 */
struct stage_t {
    uint8_t item_type;                       /* ITEM_* constant (item to collect in this stage) */
    uint8_t item_y;                          /* Item Y coordinate in tiles */
    uint8_t item_x;                          /* Item X coordinate in tiles */
    uint8_t exit_l;                          /* Left exit: target stage number or EXIT_UNUSED */
    uint8_t exit_r;                          /* Right exit: target stage number or EXIT_UNUSED */
    door_t doors[MAX_NUM_DOORS];             /* Up to 3 doors per stage */
    enemy_record_t enemies[MAX_NUM_ENEMIES]; /* Up to 4 enemies per stage */
    uint8_t tiles[128 * 10];                 /* 128×10 tile map (1280 bytes) */
};

/**
 * level_t - Complete level descriptor
 *
 * A level groups together 3 stages that share the same tileset and
 * enemy sprite files. For example, "FOREST" is one level containing
 * forest0, forest1, and forest2 stages.
 */
struct level_t {
    /* Asset filenames */
    char tt2_filename[14]; /* Tileset graphics filename */
    char pt0_filename[14]; /* Stage 0 map filename */
    char pt1_filename[14]; /* Stage 1 map filename */
    char pt2_filename[14]; /* Stage 2 map filename */

    /* Tile solidity threshold */
    uint8_t tileset_last_passable; /* Tile IDs > this value are solid obstacles */

    /* Door tile appearance */
    uint8_t door_tile_ul; /* Door tile: upper-left */
    uint8_t door_tile_ur; /* Door tile: upper-right */
    uint8_t door_tile_ll; /* Door tile: lower-left */
    uint8_t door_tile_lr; /* Door tile: lower-right */

    /* Door frame tiles (decorative tiles around doors that should be passable) */
    uint8_t door_frame_tiles[8]; /* Array of up to 8 additional passable door frame tiles (0 =
                                    unused) */

    /* Enemy sprite files */
    shp_t shp[4]; /* Sprite descriptors for enemies in this level */

    /* Stage data */
    stage_t stages[3]; /* Three stages per level */
};

#endif /* LEVEL_FORMAT_H */
