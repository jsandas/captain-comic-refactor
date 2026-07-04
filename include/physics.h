#ifndef PHYSICS_H
#define PHYSICS_H

#include <cstdint>
#include <string>

// Physics constants (from jsandas/comic-c physics.h)
constexpr int COMIC_GRAVITY = 5;          // Gravity (units of 1/8 game units per tick)
constexpr int COMIC_GRAVITY_SPACE = 3;    // Reduced gravity in space level
constexpr int TERMINAL_VELOCITY = 23;     // Maximum downward velocity
constexpr int JUMP_POWER_DEFAULT = 4;     // Default jump power
constexpr int JUMP_POWER_WITH_BOOTS = 5;  // Jump power with Boots item
constexpr int JUMP_ACCELERATION = 7;      // Upward acceleration during jump

// Map dimensions (from jsandas/comic-c globals.h)
constexpr int MAP_WIDTH_TILES = 128;  // Map width in tiles
constexpr int MAP_HEIGHT_TILES = 10;  // Map height in tiles
constexpr int MAP_WIDTH = 256;        // Map width in game units (128 * 2)
constexpr int MAP_HEIGHT = 20;        // Map height in game units (10 * 2)
constexpr int PLAYFIELD_WIDTH = 24;   // Visible playfield width in game units
constexpr int PLAYFIELD_HEIGHT = 20;  // Visible playfield height in game units

// Player gameplay constants
constexpr uint8_t MAX_HP = 6;  // Maximum health points

// Comic facing directions (used in main.cpp and physics.cpp)
constexpr uint8_t COMIC_FACING_LEFT = 0;
constexpr uint8_t COMIC_FACING_RIGHT = 1;

// Cheat system flags
extern bool cheat_noclip;  // When true, disables collision detection

// Functions
void process_jump_input();
void handle_fall_or_jump();
bool move_left();
bool move_right();
void trigger_player_death(bool show_animation = true, bool fall_clip_render = false);
void update_player_death_sequence();
bool is_player_dying();
bool should_show_player_death_animation();
bool should_clip_player_death_render();

// Tile system
void init_test_level();
void reset_level_tiles();
bool load_stage_tiles(const std::string& level_name, int stage_number);
uint8_t get_tile_at(uint8_t x, uint8_t y);
bool is_tile_solid(uint8_t tile_id);

#endif  // PHYSICS_H
