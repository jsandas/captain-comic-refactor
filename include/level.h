#ifndef LEVEL_H
#define LEVEL_H

#include <cstdint>
#include <string>

/* Maximum counts */
constexpr int MAX_NUM_ENEMIES = 4;
constexpr int MAX_NUM_DOORS = 3;

/* Enemy behavior constants */
constexpr uint8_t ENEMY_BEHAVIOR_BOUNCE = 1;
constexpr uint8_t ENEMY_BEHAVIOR_LEAP = 2;
constexpr uint8_t ENEMY_BEHAVIOR_ROLL = 3;
constexpr uint8_t ENEMY_BEHAVIOR_SEEK = 4;
constexpr uint8_t ENEMY_BEHAVIOR_SHY = 5;
constexpr uint8_t ENEMY_BEHAVIOR_UNUSED = 0x7f;
constexpr uint8_t ENEMY_BEHAVIOR_FAST = 0x80; /* Bitmask to combine with other behaviors */

/* Item type constants */
constexpr uint8_t ITEM_CORKSCREW = 0;
constexpr uint8_t ITEM_DOOR_KEY = 1;
constexpr uint8_t ITEM_BOOTS = 2;
constexpr uint8_t ITEM_LANTERN = 3;
constexpr uint8_t ITEM_TELEPORT_WAND = 4;
constexpr uint8_t ITEM_GEMS = 5;
constexpr uint8_t ITEM_CROWN = 6;
constexpr uint8_t ITEM_GOLD = 7;
constexpr uint8_t ITEM_BLASTOLA_COLA = 8;
constexpr uint8_t ITEM_SHIELD = 14;
constexpr uint8_t ITEM_UNUSED = 0xff;

/* Exit and door unused markers */
constexpr uint8_t EXIT_UNUSED = 0xff;
constexpr uint8_t DOOR_UNUSED = 0xff;

/* SHP (sprite) file markers */
constexpr uint8_t SHP_UNUSED = 0x00;

/* Enemy sprite orientation constants */
constexpr uint8_t ENEMY_HORIZONTAL_DUPLICATED = 1; /* Right-facing frames are mirrored copies */
constexpr uint8_t ENEMY_HORIZONTAL_SEPARATE = 2;   /* Right-facing frames are stored separately */

/* Enemy animation constants */
constexpr uint8_t ENEMY_ANIMATION_LOOP = 0;      /* Animation cycles: 0,1,2,3,0,1,... */
constexpr uint8_t ENEMY_ANIMATION_ALTERNATE = 1; /* Animation alternates: 0,1,2,1,0,1,... */

/* Level numbers */
constexpr uint8_t LEVEL_NUMBER_LAKE = 0;
constexpr uint8_t LEVEL_NUMBER_FOREST = 1;
constexpr uint8_t LEVEL_NUMBER_SPACE = 2;
constexpr uint8_t LEVEL_NUMBER_BASE = 3;
constexpr uint8_t LEVEL_NUMBER_CAVE = 4;
constexpr uint8_t LEVEL_NUMBER_SHED = 5;
constexpr uint8_t LEVEL_NUMBER_CASTLE = 6;
constexpr uint8_t LEVEL_NUMBER_COMP = 7;

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
