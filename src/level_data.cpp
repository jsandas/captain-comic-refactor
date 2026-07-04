/**
 * level_data.cpp - Level data definitions
 *
 * Contains the static data for all 8 levels in the game.
 * This data was originally in R3_levels.asm from the reference implementation.
 */

#include <algorithm>
#include <cctype>

#include "../include/level.h"

/* ===== LAKE Level Data ===== */
const level_t level_data_lake = {
    /* Filenames */
    .tt2_filename = "LAKE.TT2    ",
    .pt0_filename = "LAKE0.PT    ",
    .pt1_filename = "LAKE1.PT    ",
    .pt2_filename = "LAKE2.PT    ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x44,

    /* Door tiles */
    .door_tile_ul = 16,
    .door_tile_ur = 17,
    .door_tile_ll = 16,
    .door_tile_lr = 17,

    /* Door frame tiles */
    .door_frame_tiles = {0x13, 0x14, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{3, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_ALTERNATE, "fb.shp      "},
            {3, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_ALTERNATE, "bug.shp     "},
            {SHP_UNUSED, 0, 0, ""},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* lake0 */
               {.item_type = ITEM_BLASTOLA_COLA,
                .item_y = 12,
                .item_x = 112,
                .exit_l = 1,
                .exit_r = EXIT_UNUSED,
                .doors = {{10, 118, 1, 2}, {14, 248, 5, 0}, {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_UNUSED},
                            {0, ENEMY_BEHAVIOR_UNUSED},
                            {1, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_LEAP}}},
               /* lake1 */
               {.item_type = ITEM_SHIELD,
                .item_y = 10,
                .item_x = 178,
                .exit_l = 2,
                .exit_r = 0,
                .doors = {{DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_LEAP}}},
               /* lake2 */
               {.item_type = ITEM_BLASTOLA_COLA,
                .item_y = 4,
                .item_x = 124,
                .exit_l = EXIT_UNUSED,
                .exit_r = 1,
                .doors = {{14, 10, 4, 0}, {6, 110, 2, 0}, {8, 74, 3, 1}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_LEAP}}}}};

/* ===== FOREST Level Data ===== */
const level_t level_data_forest = {
    /* Filenames */
    .tt2_filename = "FOREST.TT2  ",
    .pt0_filename = "FOREST0.PT  ",
    .pt1_filename = "FOREST1.PT  ",
    .pt2_filename = "FOREST2.PT  ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x44,

    /* Door tiles */
    .door_tile_ul = 48,
    .door_tile_ur = 49,
    .door_tile_ll = 48,
    .door_tile_lr = 49,

    /* Door frame tiles */
    .door_frame_tiles = {0x40, 0x41, 0x42, 0x43, 0x44, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bird.shp    "},
            {3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bird2.shp   "},
            {SHP_UNUSED, 0, 0, ""},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* forest0 */
               {.item_type = ITEM_BLASTOLA_COLA,
                .item_y = 14,
                .item_x = 12,
                .exit_l = EXIT_UNUSED,
                .exit_r = 1,
                .doors = {{12, 12, 6, 0},  // Door to castle stage 0
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_UNUSED},
                            {0, ENEMY_BEHAVIOR_UNUSED}}},
               /* forest1 */
               {.item_type = ITEM_SHIELD,
                .item_y = 10,
                .item_x = 118,
                .exit_l = 0,
                .exit_r = 2,
                .doors = {{DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_UNUSED},
                            {1, ENEMY_BEHAVIOR_SHY}}},
               /* forest2 */
               {.item_type = ITEM_DOOR_KEY,
                .item_y = 2,
                .item_x = 160,
                .exit_l = 1,
                .exit_r = EXIT_UNUSED,
                .doors = {{12, 238, 0, 0},  // Door to lake stage 0
                          {14, 160, 7, 2},  // Door to comp stage 2
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_SHY},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_SHY}}}}};

/* ===== SPACE Level Data ===== */
const level_t level_data_space = {
    /* Filenames */
    .tt2_filename = "SPACE.TT2   ",
    .pt0_filename = "SPACE0.PT   ",
    .pt1_filename = "SPACE1.PT   ",
    .pt2_filename = "SPACE2.PT   ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x47,

    /* Door tiles */
    .door_tile_ul = 51,
    .door_tile_ur = 52,
    .door_tile_ll = 51,
    .door_tile_lr = 52,

    /* Door frame tiles */
    .door_frame_tiles = {0, 0, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "rock.shp    "},
            {4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "saucer.shp  "},
            {SHP_UNUSED, 0, 0, ""},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* space0 */
               {.item_type = ITEM_BLASTOLA_COLA,
                .item_y = 16,
                .item_x = 10,
                .exit_l = EXIT_UNUSED,
                .exit_r = 1,
                .doors = {{12, 2, 0, 2},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE}}},
               /* space1 */
               {.item_type = ITEM_SHIELD,
                .item_y = 8,
                .item_x = 114,
                .exit_l = 0,
                .exit_r = 2,
                .doors = {{DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_BOUNCE}}},
               /* space2 */
               {.item_type = ITEM_GEMS,
                .item_y = 6,
                .item_x = 198,
                .exit_l = 1,
                .exit_r = EXIT_UNUSED,
                .doors = {{12, 228, 3, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST}}}}};

/* ===== BASE Level Data ===== */
const level_t level_data_base = {
    /* Filenames */
    .tt2_filename = "BASE.TT2    ",
    .pt0_filename = "BASE0.PT    ",
    .pt1_filename = "BASE1.PT    ",
    .pt2_filename = "BASE2.PT    ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x3b,

    /* Door tiles */
    .door_tile_ul = 9,
    .door_tile_ur = 10,
    .door_tile_ll = 11,
    .door_tile_lr = 12,

    /* Door frame tiles */
    .door_frame_tiles = {0, 0, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{3, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_ALTERNATE, "bug.shp     "},
            {4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "globe.shp   "},
            {SHP_UNUSED, 0, 0, ""},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* base0 */
               {.item_type = ITEM_SHIELD,
                .item_y = 4,
                .item_x = 252,
                .exit_l = 1,
                .exit_r = 2,
                .doors = {{14, 240, 3, 2}, {14, 114, 3, 1}, {8, 138, 2, 2}},
                .enemies = {{0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_ROLL}}},
               /* base1 */
               {.item_type = ITEM_CORKSCREW,
                .item_y = 16,
                .item_x = 0,
                .exit_l = EXIT_UNUSED,
                .exit_r = 0,
                .doors = {{6, 0, 0, 2}, {14, 112, 3, 0}, {0, 0, 3, 2}},
                .enemies = {{0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_ROLL}}},
               /* base2 */
               {.item_type = ITEM_BOOTS,
                .item_y = 16,
                .item_x = 230,
                .exit_l = 0,
                .exit_r = EXIT_UNUSED,
                .doors = {{2, 36, 3, 0}, {14, 218, 3, 1}, {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_ROLL},
                            {1, ENEMY_BEHAVIOR_ROLL | ENEMY_BEHAVIOR_FAST}}}}};

/* ===== CAVE Level Data ===== */
const level_t level_data_cave = {
    /* Filenames */
    .tt2_filename = "CAVE.TT2    ",
    .pt0_filename = "CAVE0.PT    ",
    .pt1_filename = "CAVE1.PT    ",
    .pt2_filename = "CAVE2.PT    ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x09,

    /* Door tiles */
    .door_tile_ul = 5,
    .door_tile_ur = 5,
    .door_tile_ll = 5,
    .door_tile_lr = 5,

    /* Door frame tiles */
    .door_frame_tiles = {0, 0, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{3, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_ALTERNATE, "frog.shp    "},
            {3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bee.shp     "},
            {SHP_UNUSED, 0, 0, ""},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* cave0 */
               {.item_type = ITEM_SHIELD,
                .item_y = 6,
                .item_x = 41,
                .exit_l = EXIT_UNUSED,
                .exit_r = 1,
                .doors = {{12, 8, 0, 2}, {4, 40, 4, 1}, {2, 154, 4, 2}},
                .enemies = {{0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_SEEK}}},
               /* cave1 */
               {.item_type = ITEM_GOLD,
                .item_y = 4,
                .item_x = 228,
                .exit_l = 0,
                .exit_r = 2,
                .doors = {{14, 236, 4, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_SEEK},
                            {1, ENEMY_BEHAVIOR_SEEK}}},
               /* cave2 */
               {.item_type = ITEM_TELEPORT_WAND,
                .item_y = 10,
                .item_x = 64,
                .exit_l = 1,
                .exit_r = EXIT_UNUSED,
                .doors = {{14, 236, 4, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_LEAP},
                            {0, ENEMY_BEHAVIOR_LEAP | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_SEEK},
                            {1, ENEMY_BEHAVIOR_SEEK}}}}};

/* ===== SHED Level Data ===== */
const level_t level_data_shed = {
    /* Filenames */
    .tt2_filename = "SHED.TT2    ",
    .pt0_filename = "SHED0.PT    ",
    .pt1_filename = "SHED1.PT    ",
    .pt2_filename = "SHED2.PT    ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x17,

    /* Door tiles */
    .door_tile_ul = 10,
    .door_tile_ur = 11,
    .door_tile_ll = 10,
    .door_tile_lr = 11,

    /* Door frame tiles */
    .door_frame_tiles = {0, 0, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bird.shp    "},
            {3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bee.shp     "},
            {4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "ball.shp    "},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* shed0 */
               {.item_type = ITEM_SHIELD,
                .item_y = 12,
                .item_x = 162,
                .exit_l = EXIT_UNUSED,
                .exit_r = 1,
                .doors = {{12, 4, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_SEEK},
                            {2, ENEMY_BEHAVIOR_LEAP}}},
               /* shed1 */
               {.item_type = ITEM_BLASTOLA_COLA,
                .item_y = 4,
                .item_x = 250,
                .exit_l = 0,
                .exit_r = EXIT_UNUSED,
                .doors = {{4, 240, 5, 2},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_SEEK},
                            {2, ENEMY_BEHAVIOR_LEAP},
                            {2, ENEMY_BEHAVIOR_LEAP}}},
               /* shed2 */
               {.item_type = ITEM_SHIELD,
                .item_y = 10,
                .item_x = 246,
                .exit_l = EXIT_UNUSED,
                .exit_r = EXIT_UNUSED,
                .doors = {{14, 4, 5, 1}, {8, 242, 7, 0}, {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_SEEK},
                            {2, ENEMY_BEHAVIOR_LEAP},
                            {2, ENEMY_BEHAVIOR_LEAP | ENEMY_BEHAVIOR_FAST}}}}};

/* ===== CASTLE Level Data ===== */
const level_t level_data_castle = {
    /* Filenames */
    .tt2_filename = "CASTLE.TT2  ",
    .pt0_filename = "CASTLE0.PT  ",
    .pt1_filename = "CASTLE1.PT  ",
    .pt2_filename = "CASTLE2.PT  ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x48,

    /* Door tiles */
    .door_tile_ul = 26,
    .door_tile_ur = 27,
    .door_tile_ll = 26,
    .door_tile_lr = 27,

    /* Door frame tiles */
    .door_frame_tiles = {0, 0, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bird.shp    "},
            {4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "ball.shp    "},
            {4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "cube.shp    "},
            {3, ENEMY_HORIZONTAL_SEPARATE, ENEMY_ANIMATION_ALTERNATE, "bee.shp     "}},

    /* Three stages */
    .stages = {/* castle0 */
               {.item_type = ITEM_CROWN,
                .item_y = 2,
                .item_x = 244,
                .exit_l = EXIT_UNUSED,
                .exit_r = EXIT_UNUSED,
                .doors = {{14, 246, 1, 0}, {4, 228, 6, 2}, {8, 8, 6, 1}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_LEAP | ENEMY_BEHAVIOR_FAST},
                            {3, ENEMY_BEHAVIOR_SEEK}}},
               /* castle1 */
               {.item_type = ITEM_SHIELD,
                .item_y = 14,
                .item_x = 66,
                .exit_l = EXIT_UNUSED,
                .exit_r = EXIT_UNUSED,
                .doors = {{14, 8, 6, 0}, {2, 8, 6, 2}, {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_LEAP},
                            {1, ENEMY_BEHAVIOR_LEAP | ENEMY_BEHAVIOR_FAST},
                            {2, ENEMY_BEHAVIOR_SHY}}},
               /* castle2 */
               {.item_type = ITEM_SHIELD,
                .item_y = 8,
                .item_x = 44,
                .exit_l = EXIT_UNUSED,
                .exit_r = EXIT_UNUSED,
                .doors = {{8, 8, 6, 1}, {4, 246, 6, 0}, {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{1, ENEMY_BEHAVIOR_LEAP},
                            {2, ENEMY_BEHAVIOR_SHY | ENEMY_BEHAVIOR_FAST},
                            {3, ENEMY_BEHAVIOR_SEEK},
                            {3, ENEMY_BEHAVIOR_SEEK | ENEMY_BEHAVIOR_FAST}}}}};

/* ===== COMP Level Data ===== */
const level_t level_data_comp = {
    /* Filenames */
    .tt2_filename = "COMP.TT2    ",
    .pt0_filename = "COMP0.PT    ",
    .pt1_filename = "COMP1.PT    ",
    .pt2_filename = "COMP2.PT    ",

    /* Tile solidity threshold (tiles > this ID are solid) */
    .tileset_last_passable = 0x1d,

    /* Door tiles */
    .door_tile_ul = 2,
    .door_tile_ur = 3,
    .door_tile_ll = 4,
    .door_tile_lr = 5,

    /* Door frame tiles */
    .door_frame_tiles = {0, 0, 0, 0, 0, 0, 0, 0},

    /* Enemy sprite files */
    .shp = {{4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "star1.shp   "},
            {4, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_LOOP, "star2.shp   "},
            {3, ENEMY_HORIZONTAL_DUPLICATED, ENEMY_ANIMATION_ALTERNATE, "star3.shp   "},
            {SHP_UNUSED, 0, 0, ""}},

    /* Three stages */
    .stages = {/* comp0 */
               {.item_type = ITEM_SHIELD,
                .item_y = 2,
                .item_x = 140,
                .exit_l = EXIT_UNUSED,
                .exit_r = 1,
                .doors = {{8, 6, 5, 2},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE},
                            {1, ENEMY_BEHAVIOR_BOUNCE},
                            {2, ENEMY_BEHAVIOR_BOUNCE},
                            {0, ENEMY_BEHAVIOR_UNUSED}}},
               /* comp1 */
               {.item_type = ITEM_SHIELD,
                .item_y = 10,
                .item_x = 70,
                .exit_l = 0,
                .exit_r = 2,
                .doors = {{DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {2, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {0, ENEMY_BEHAVIOR_UNUSED}}},
               /* comp2 */
               {.item_type = ITEM_LANTERN,
                .item_y = 2,
                .item_x = 166,
                .exit_l = 1,
                .exit_r = EXIT_UNUSED,
                .doors = {{14, 244, 1, 2},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0},
                          {DOOR_UNUSED, DOOR_UNUSED, 0, 0}},
                .enemies = {{0, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {1, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {2, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST},
                            {0, ENEMY_BEHAVIOR_UNUSED}}}}};

/* ===== Level Data Pointer Array ===== */
/**
 * Array of pointers to all 8 level data structures.
 * Indexed by level number (0-7): LAKE, FOREST, SPACE, BASE, CAVE, SHED, CASTLE, COMP
 */
const level_t* const level_data_pointers[8] = {
    &level_data_lake, &level_data_forest, &level_data_space,  &level_data_base,
    &level_data_cave, &level_data_shed,   &level_data_castle, &level_data_comp};

/* ===== Level Accessor Functions ===== */

const level_t* get_level_by_name(const std::string& level_name) {
    std::string lower_name = level_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower_name == "lake") return &level_data_lake;
    if (lower_name == "forest") return &level_data_forest;
    if (lower_name == "space") return &level_data_space;
    if (lower_name == "base") return &level_data_base;
    if (lower_name == "cave") return &level_data_cave;
    if (lower_name == "shed") return &level_data_shed;
    if (lower_name == "castle") return &level_data_castle;
    if (lower_name == "comp") return &level_data_comp;

    return nullptr;
}

const level_t* get_level_by_number(int level_number) {
    if (level_number < 0 || level_number >= 8) {
        return nullptr;
    }
    return level_data_pointers[level_number];
}
