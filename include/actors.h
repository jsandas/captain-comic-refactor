#ifndef ACTORS_H
#define ACTORS_H

#include <cstdint>
#include <vector>

#include "graphics.h"
#include "level.h"
#include "physics.h"

/* Enemy state constants */
constexpr uint8_t ENEMY_STATE_DESPAWNED = 0;   /* Not yet spawned */
constexpr uint8_t ENEMY_STATE_SPAWNED = 1;     /* Active and moving */
constexpr uint8_t ENEMY_STATE_WHITE_SPARK = 2; /* Death animation (frames 2-7) */
constexpr uint8_t ENEMY_STATE_RED_SPARK = 8;   /* Death animation (frames 8-13) */
constexpr uint8_t ENEMY_STATE_PIT_FALL_SENTINEL =
    14;                                           /* Clamp at pit bottom for one frame, no spark */
constexpr uint8_t DEATH_ANIMATION_LAST_FRAME = 5; /* Offset from start to last frame */

/* Enemy restraint (movement throttle) */
constexpr uint8_t ENEMY_RESTRAINT_MOVE_THIS_TICK = 0;  /* Move, then skip next */
constexpr uint8_t ENEMY_RESTRAINT_SKIP_THIS_TICK = 1;  /* Skip, then move next */
constexpr uint8_t ENEMY_RESTRAINT_MOVE_EVERY_TICK = 2; /* Always move (fast) */

/* Enemy physics constants */
constexpr int ENEMY_DESPAWN_RADIUS = 30;   /* Game units from Comic */
constexpr int ENEMY_VELOCITY_SHIFT = 3;    /* Bit shift for 1/8 unit movement precision */
constexpr int8_t ENEMY_GRAVITY = 2;        /* Acceleration per tick (vs COMIC_GRAVITY = 5) */
constexpr int8_t ENEMY_JUMP_VELOCITY = -7; /* Initial upward velocity for LEAP behavior */

/* Respawn timer cycle: 20→40→60→80→100→20 */
constexpr uint8_t RESPAWN_TIMER_MIN = 20;
constexpr uint8_t RESPAWN_TIMER_MAX = 100;
constexpr uint8_t RESPAWN_TIMER_STEP = 20;

/* Fireball constants */
constexpr uint8_t MAX_NUM_FIREBALLS = 5;           /* Maximum firepower (Blastola Cola upgrades) */
constexpr uint8_t FIREBALL_DEAD = 0xFF;            /* Inactive marker in x/y fields */
constexpr int8_t FIREBALL_VELOCITY = 2;            /* Horizontal speed (±2 game units per tick) */
constexpr uint8_t MAX_FIREBALL_METER = 12;         /* Full fireball meter value */
constexpr uint8_t FIREBALL_METER_COUNTER_INIT = 2; /* Counter cycles 2→1→2→1 */
constexpr uint8_t FIREBALL_NUM_FRAMES = 2;         /* Two animation frames per fireball */

/* Enemy facing directions - frame offsets for sprite animation
 * Left-facing frames: 0-4, Right-facing frames: 5-9
 * These values are added to the current animation frame index */
constexpr uint8_t ENEMY_FACING_LEFT = 0;  /* Left-facing animation starts at frame 0 */
constexpr uint8_t ENEMY_FACING_RIGHT = 5; /* Right-facing animation starts at frame 5 */

/**
 * fireball_t - A single fireball projectile
 *
 * Fires horizontally at ±FIREBALL_VELOCITY per tick.
 * Corkscrew item adds vertical oscillation.
 * x==FIREBALL_DEAD && y==FIREBALL_DEAD means the slot is inactive.
 */
struct fireball_t {
    uint8_t y;                    /* Y position (FIREBALL_DEAD = inactive) */
    uint8_t x;                    /* X position (FIREBALL_DEAD = inactive) */
    int8_t vel;                   /* Horizontal velocity (+2 or -2) */
    uint8_t corkscrew_phase;      /* Vertical oscillation phase (1 or 2) */
    uint8_t animation;            /* Current animation frame (0 or 1) */
    uint8_t num_animation_frames; /* Always FIREBALL_NUM_FRAMES (2) */
};

/**
 * enemy_t - Enemy state
 *
 * Tracks position, velocity, animation, and behavior state of a single enemy.
 */
struct enemy_t {
    uint8_t y;    /* Y position (game units) */
    uint8_t x;    /* X position (game units) */
    int8_t x_vel; /* Horizontal velocity */
    int8_t y_vel; /* Vertical velocity */

    /* Dual-use field: spawn timer when despawned, animation frame when spawned */
    uint8_t spawn_timer_and_animation;

    uint8_t num_animation_frames; /* Cached from sprite_descriptor for fast access */
    uint8_t behavior;             /* AI behavior type (see ENEMY_BEHAVIOR_*) */
    uint8_t state;                /* Current state (ENEMY_STATE_*) */
    uint8_t facing;               /* 0=left, 5=right (in units of animation frames) */
    uint8_t restraint;            /* Movement throttle (ENEMY_RESTRAINT_*) */

    /* References to sprite metadata and textures */
    const shp_t* sprite_descriptor; /* Source sprite metadata (horizontal, animation type, etc.) */
    SpriteAnimationData* animation_data; /* Loaded texture frames */
};

/**
 * ActorSystem - Manages all enemies, fireballs, and items
 *
 * Handles enemy spawning, AI behavior, collision detection, animation,
 * and cleanup.
 */
class ActorSystem {
public:
    ActorSystem();
    ~ActorSystem();

    /* Initialize the actor system */
    bool initialize();

    /* Update all actors for one game tick */
    void update(uint8_t comic_x, uint8_t comic_y, uint8_t comic_facing, const uint8_t* tiles,
                int camera_x, uint8_t fire_key = 0);

    /* Get array of enemies */
    const std::vector<enemy_t>& get_enemies() const { return enemies; }

    /* Get array of fireballs (read-only) */
    const std::vector<fireball_t>& get_fireballs() const { return fireballs; }

    /* Load fireball sprites from assets (call once after GraphicsSystem is ready) */
    bool load_fireball_sprites(GraphicsSystem* graphics_system);

    /* Load enemy spark effect sprites (white/red, 3 frames each). */
    bool load_effect_sprites(GraphicsSystem* graphics_system);

    /* Render all active fireballs */
    void render_fireballs(GraphicsSystem* graphics_system, int camera_x, int render_scale) const;

    /* Reset fireballs (called on stage load) */
    void reset_fireballs();

    /* Player firepower and powerup flags (set by item system) */
    uint8_t comic_firepower;     /* 0-5: number of fireball slots available */
    uint8_t comic_has_corkscrew; /* 1 if Comic has the Corkscrew item */
    uint8_t fireball_meter;      /* Current fireball meter (0–MAX_FIREBALL_METER) */

    /* Additional item-related player flags */
    uint8_t comic_has_boots;         /* 1 if Comic has Boots (increased jump power) */
    uint8_t comic_has_lantern;       /* 1 if Comic has Lantern (castle lighting) */
    uint8_t comic_has_door_key;      /* 1 if Comic has Door Key */
    uint8_t comic_has_teleport_wand; /* 1 if Comic has Teleport Wand */
    uint8_t comic_has_gems;          /* 1 if Comic collected Gems */
    uint8_t comic_has_crown;         /* 1 if Comic collected Crown */
    uint8_t comic_has_gold;          /* 1 if Comic collected Gold */
    uint8_t comic_num_treasures;     /* Count of treasures collected (0-3) */

    /* Get current jump power based on boots status */
    int get_jump_power() const;

    /* Load item sprites from assets (call once after GraphicsSystem is ready) */
    bool load_item_sprites(GraphicsSystem* graphics_system);

    /* Render the current stage's item if not yet collected */
    void render_item(GraphicsSystem* graphics_system, int camera_x, int render_scale) const;

    /* Apply item effect (public for testing) */
    void apply_item_effect(uint8_t item_type);

    /* Reset all enemies (called when loading a new stage) */
    void reset_for_stage();

    /* Setup enemies for a stage from level data */
    void setup_enemies_for_stage(const class level_t* level, int level_index, int stage_number,
                                 GraphicsSystem* graphics_system);

    /* Render enemies for the current frame */
    void render_enemies(GraphicsSystem* graphics_system, int camera_x, int render_scale) const;

    /* Check if a specific tile is solid */
    bool is_tile_solid(uint8_t tile_id) const;

    /* Get tile at coordinates */
    uint8_t get_tile_at(uint8_t x, uint8_t y) const;

protected:
    /* Enemy array (max 4 enemies per stage) */
    std::vector<enemy_t> enemies;

    /* Fireball array (max MAX_NUM_FIREBALLS active projectiles) */
    std::vector<fireball_t> fireballs;

    /* Loaded fireball sprite frames (indexed 0/1, set by load_fireball_sprites) */
    Sprite* fireball_sprite[FIREBALL_NUM_FRAMES];

    /* Enemy spark effects: [0]=white, [1]=red; 3 animation frames each. */
    Sprite* spark_sprites[2][3];

    /* Fireball meter timing counter (cycles 2→1→2→1 each tick) */
    uint8_t fireball_meter_counter;

    /* Item system state */
    uint8_t item_animation_counter; /* Toggles 0→1→0→1 each tick for item sprite animation */
    uint8_t items_collected[8][3];  /* [level_index][stage_index] collection status */
    uint8_t current_item_type;      /* Item type for current stage (from stage data) */
    uint8_t current_item_x;         /* Item X position in game units */
    uint8_t current_item_y;         /* Item Y position in game units */

    /* Item sprite storage (all item types, even/odd frames) */
    Sprite* item_sprites[15][2]; /* [item_type][0=even, 1=odd] */

    /* Level and stage context */
    const uint8_t* current_tiles;
    int current_map_width_tiles;
    int current_map_height_tiles;
    uint8_t tileset_last_passable;
    uint8_t current_level_index; /* Current level number (0=LAKE, 1=FOREST, etc.) */
    uint8_t current_stage_index; /* Current stage number (0-2) */

    /* Spawning control */
    uint8_t spawned_this_tick;
    uint8_t spawn_offset_cycle;
    uint8_t enemy_respawn_counter_cycle;

    /* Global state updates (passed from game loop) */
    uint8_t g_comic_x;
    uint8_t g_comic_y;
    uint8_t g_comic_facing;
    int g_camera_x;

    /* Private helper functions */
    bool maybe_spawn_enemy(int enemy_index);
    void handle_single_enemy(int enemy_index);
    void update_enemy_animation(enemy_t* enemy);
    void check_enemy_despawn(enemy_t* enemy);
    void check_enemy_player_collision(enemy_t* enemy);

    /* Fireball helpers */
    void try_to_fire();
    void handle_fireballs();

    /* Item helpers */
    void handle_item();
    void collect_item();

    /* AI behavior functions */
    void enemy_behavior_bounce(enemy_t* enemy) const;
    void enemy_behavior_leap(enemy_t* enemy) const;
    void enemy_behavior_roll(enemy_t* enemy) const;
    void enemy_behavior_seek(enemy_t* enemy) const;
    void enemy_behavior_shy(enemy_t* enemy) const;

    /* Collision detection helpers */
    bool check_horizontal_enemy_map_collision(uint8_t x, uint8_t y) const;
    bool check_vertical_enemy_map_collision(uint8_t x, uint8_t y) const;
};

/* Award points to the player's score.
 * Input is base-100 units (e.g., award_points(3) adds 300 points).
 * Points are added into score_bytes[0] with full carry propagation into
 * score_bytes[1]/[2]. */
void award_points(uint16_t points);

/* Award an extra life.
 * If already at max lives, refill HP and award the comic-c bonus points.
 */
void award_extra_life();

#endif /* ACTORS_H */
