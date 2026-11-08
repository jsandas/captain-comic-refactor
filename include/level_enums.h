#ifndef LEVEL_ENUMS_H
#define LEVEL_ENUMS_H

#include <cstdint>

/**
 * level_enums.h - Gameplay constants referenced by the level data model
 *
 * Values stored inside stage_t/level_t fields (enemy behaviors, item types,
 * level numbers, sprite attributes) so consumers can interpret file contents
 * without pulling in the format structs themselves.
 */

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

#endif /* LEVEL_ENUMS_H */
