#include "actors.h"

#include <iostream>

#include "audio.h"
#include "level.h"
#include "physics.h"

// Global door key flag defined in main.cpp (used by doors.cpp)
extern uint8_t comic_has_door_key;
extern uint8_t comic_hp;
extern uint8_t comic_hp_pending_increase;
extern uint8_t comic_num_lives;

// Score bytes from main.cpp for award_points()
extern uint8_t score_bytes[3];
extern uint8_t score_10000_counter;

/**
 * award_points - Award points to the player's score
 *
 * Adds points to the player's score using base-100 arithmetic with carry propagation.
 * Score is stored as three base-100 bytes where:
 *   score_bytes[0] = ones/tens (0-99)
 *   score_bytes[1] = hundreds/thousands (0-99)
 *   score_bytes[2] = ten-thousands/hundred-thousands (0-99)
 * Total score = byte[0] + (byte[1] * 100) + (byte[2] * 10000), max 999,999
 *
 * Input points are base-100 units.
 * Example: award_points(3) adds 300 displayed points.
 */
void award_points(uint16_t points) {
    // Points map to the least-significant base-100 byte and carry upward.
    uint16_t sum0 = static_cast<uint16_t>(score_bytes[0]) + points;
    score_bytes[0] = static_cast<uint8_t>(sum0 % 100);

    uint16_t carry = sum0 / 100;
    if (carry > 0) {
        uint16_t sum1 = static_cast<uint16_t>(score_bytes[1]) + carry;
        score_bytes[1] = static_cast<uint8_t>(sum1 % 100);

        // Assembly parity: each carry from score byte 0 into byte 1 is one
        // ten-thousand increment; on 5 increments reset counter, then award life.
        for (uint16_t i = 0; i < carry; ++i) {
            const uint8_t next = static_cast<uint8_t>(score_10000_counter + 1);
            if (next >= 5) {
                score_10000_counter = 0;
                award_extra_life();
            } else {
                score_10000_counter = next;
            }
        }

        carry = sum1 / 100;
        if (carry == 0) {
            return;
        }

        uint16_t high = static_cast<uint16_t>(score_bytes[2]) + carry;
        if (high >= 100) {
            // Saturate to max representable score.
            score_bytes[0] = 99;
            score_bytes[1] = 99;
            score_bytes[2] = 99;
        } else {
            score_bytes[2] = static_cast<uint8_t>(high);
        }
    }
}

void award_extra_life() {
    constexpr uint8_t MAX_NUM_LIVES = 5;

    play_game_sound(GameSound::EXTRA_LIFE);

    if (comic_num_lives >= MAX_NUM_LIVES) {
        // Match comic-c: full lives converts shield life-award into HP refill + bonus points.
        comic_hp_pending_increase = MAX_HP;
        award_points(75);
        award_points(75);
        award_points(75);
        return;
    }

    comic_num_lives++;
}

/**
 * ActorSystem constructor
 */
ActorSystem::ActorSystem()
    : comic_firepower(0),
      comic_has_corkscrew(0),
      fireball_meter(0),
      comic_has_boots(0),
      comic_has_lantern(0),
      comic_has_door_key(0),
      comic_has_teleport_wand(0),
      comic_has_gems(0),
      comic_has_crown(0),
      comic_has_gold(0),
      comic_num_treasures(0),
      enemies(MAX_NUM_ENEMIES),
      fireballs(MAX_NUM_FIREBALLS),
      fireball_meter_counter(FIREBALL_METER_COUNTER_INIT),
      item_animation_counter(0),
      current_item_type(ITEM_UNUSED),
      current_item_x(0),
      current_item_y(0),
      current_tiles(nullptr),
      current_map_width_tiles(128),
      current_map_height_tiles(10),
      tileset_last_passable(0x3E),
      current_level_index(0),
      current_stage_index(0),
      spawned_this_tick(0),
      spawn_offset_cycle(PLAYFIELD_WIDTH),
      enemy_respawn_counter_cycle(RESPAWN_TIMER_MIN),
      g_comic_x(0),
      g_comic_y(0),
      g_comic_facing(COMIC_FACING_LEFT),
      g_camera_x(0) {
    for (auto& enemy : enemies) {
        enemy.state = ENEMY_STATE_DESPAWNED;
        enemy.spawn_timer_and_animation = 100;
        enemy.sprite_descriptor = nullptr;
        enemy.animation_data = nullptr;
    }
    fireball_sprite[0] = nullptr;
    fireball_sprite[1] = nullptr;
    for (auto& spark_set : spark_sprites) {
        for (auto& spark_frame : spark_set) {
            spark_frame = nullptr;
        }
    }
    for (auto& fb : fireballs) {
        fb.x = FIREBALL_DEAD;
        fb.y = FIREBALL_DEAD;
    }
    // Initialize item sprites to nullptr
    for (int i = 0; i < 15; i++) {
        item_sprites[i][0] = nullptr;
        item_sprites[i][1] = nullptr;
    }
    // Initialize items_collected array to 0 (not collected)
    for (int level = 0; level < 8; level++) {
        for (int stage = 0; stage < 3; stage++) {
            items_collected[level][stage] = 0;
        }
    }
}

/**
 * ActorSystem destructor
 */
ActorSystem::~ActorSystem() {
    // Cleanup handled by SDL/graphics system
}

/**
 * Initialize the actor system
 */
bool ActorSystem::initialize() {
    // Actor system is initialized on per-stage basis via setup_enemies_for_stage
    return true;
}

/**
 * Reset all enemies for a new stage
 */
void ActorSystem::reset_for_stage() {
    for (auto& enemy : enemies) {
        enemy.state = ENEMY_STATE_DESPAWNED;
        enemy.spawn_timer_and_animation = enemy_respawn_counter_cycle;
    }
    spawned_this_tick = 0;
    // NOTE: spawn_offset_cycle is intentionally NOT reset here.
    // In the original game (jsandas/comic-c: src/actors.c maybe_spawn_enemy),
    // it is a static local variable initialized once at program start and never
    // cleared on stage load, so it naturally persists across stage transitions.
    // Persisting it here matches that behavior and adds variety to spawn positions
    // across stage changes.
}

/**
 * Setup enemies for a stage from level data
 */
void ActorSystem::setup_enemies_for_stage(const level_t* level, int level_index, int stage_number,
                                          GraphicsSystem* graphics_system) {
    if (!level || stage_number < 0 || stage_number >= 3) {
        return;
    }

    const stage_t& stage = level->stages[stage_number];
    tileset_last_passable = level->tileset_last_passable;
    current_level_index = static_cast<uint8_t>(level_index);
    current_stage_index = static_cast<uint8_t>(stage_number);

    // Setup item for this stage
    current_item_type = stage.item_type;
    // Stage item coordinates are consumed here as game-unit positions, not tile indices.
    // Do not apply a tile-to-game-unit conversion here or elsewhere when reading
    // stage.item_x/stage.item_y, or the old ×2 placement bug will be reintroduced.
    current_item_x = stage.item_x;
    current_item_y = stage.item_y;

    // Initialize each enemy slot from stage data
    for (int i = 0; i < MAX_NUM_ENEMIES; i++) {
        const enemy_record_t& record = stage.enemies[i];
        enemy_t& enemy = enemies[i];

        // Check if slot is used
        if ((record.behavior & ~ENEMY_BEHAVIOR_FAST) >= ENEMY_BEHAVIOR_UNUSED) {
            enemy.behavior = ENEMY_BEHAVIOR_UNUSED;
            enemy.state = ENEMY_STATE_DESPAWNED;
            enemy.spawn_timer_and_animation = 100;
            enemy.sprite_descriptor = nullptr;
            enemy.num_animation_frames = 0;
            enemy.animation_data = nullptr;
            continue;
        }

        // Validate sprite descriptor index to avoid out-of-bounds access
        if (record.shp_index >= 4) {
            std::cerr << "Invalid sprite index " << static_cast<int>(record.shp_index)
                      << " for enemy slot " << i << " (max is 3)" << std::endl;
            enemy.state = ENEMY_STATE_DESPAWNED;
            enemy.spawn_timer_and_animation = 100;
            enemy.sprite_descriptor = nullptr;
            enemy.animation_data = nullptr;
            continue;
        }
        // Load animation data from sprite descriptor
        const shp_t& sprite_desc = level->shp[record.shp_index];
        enemy.sprite_descriptor = &sprite_desc;  // Store reference to source metadata
        enemy.num_animation_frames = sprite_desc.num_distinct_frames;  // Cache for performance
        enemy.behavior = record.behavior;

        // Load sprite animation from graphics system
        enemy.animation_data = graphics_system->load_enemy_sprite(sprite_desc);
        if (!enemy.animation_data) {
            std::string sprite_name = sprite_desc.filename;
            size_t null_pos = sprite_name.find('\0');
            if (null_pos != std::string::npos) {
                sprite_name.resize(null_pos);
            }
            while (!sprite_name.empty() && sprite_name.back() == ' ') {
                sprite_name.pop_back();
            }
            std::cerr << "Failed to load sprite animation data for "
                      << (sprite_name.empty() ? "<unknown>" : sprite_name) << std::endl;
            enemy.state = ENEMY_STATE_DESPAWNED;
            enemy.sprite_descriptor = nullptr;
            enemy.animation_data = nullptr;
            continue;
        }

        enemy.num_animation_frames =
            static_cast<uint8_t>(enemy.animation_data->frame_sequence.size());
        if (enemy.num_animation_frames == 0) {
            std::cerr << "Invalid animation sequence for enemy sprite" << std::endl;
            enemy.state = ENEMY_STATE_DESPAWNED;
            enemy.sprite_descriptor = nullptr;
            enemy.animation_data = nullptr;
            continue;
        }

        // Set initial velocities and facing
        // (state and spawn_timer_and_animation will be set by reset_for_stage())
        enemy.x_vel = 0;
        enemy.y_vel = 0;
        enemy.facing = ENEMY_FACING_LEFT;
        enemy.restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
    }

    reset_for_stage();
    reset_fireballs();
}

void ActorSystem::render_enemies(GraphicsSystem* graphics_system, int camera_x,
                                 int render_scale) const {
    if (!graphics_system) {
        return;
    }

    const int scale_factor = (render_scale * 2) / TILE_SIZE;
    if (scale_factor <= 0) {
        return;
    }

    for (const auto& enemy : enemies) {
        if (enemy.state == ENEMY_STATE_DESPAWNED) {
            continue;
        }

        // Cull enemies outside the visible viewport; 2-unit margin covers sprite width
        if (static_cast<int>(enemy.x) < camera_x - 2 ||
            static_cast<int>(enemy.x) >= camera_x + PLAYFIELD_WIDTH + 2) {
            continue;
        }

        if (!enemy.animation_data || enemy.animation_data->frames_left.empty()) {
            continue;
        }

        const auto& sequence = enemy.animation_data->frame_sequence;
        if (sequence.empty()) {
            continue;
        }

        uint8_t sequence_index = enemy.spawn_timer_and_animation % sequence.size();
        uint8_t frame_index = sequence[sequence_index];

        const TextureInfo* frame_info = nullptr;
        bool flip_h = false;

        if (enemy.sprite_descriptor &&
            enemy.sprite_descriptor->horizontal == ENEMY_HORIZONTAL_SEPARATE) {
            const auto& right_frames = enemy.animation_data->frames_right;
            if (enemy.facing == ENEMY_FACING_RIGHT && !right_frames.empty()) {
                frame_info = &right_frames[frame_index % right_frames.size()];
            } else {
                frame_info =
                    &enemy.animation_data
                         ->frames_left[frame_index % enemy.animation_data->frames_left.size()];
            }
        } else {
            frame_info = &enemy.animation_data
                              ->frames_left[frame_index % enemy.animation_data->frames_left.size()];
            flip_h = (enemy.facing == ENEMY_FACING_RIGHT);
        }

        const int enemy_screen_x =
            (static_cast<int>(enemy.x) - camera_x) * render_scale + render_scale;
        const int enemy_screen_y = static_cast<int>(enemy.y) * render_scale + render_scale;

        auto render_enemy_base = [&]() {
            if (!frame_info || !frame_info->texture) {
                return;
            }

            const int render_width = frame_info->width * scale_factor;
            const int render_height = frame_info->height * scale_factor;

            Sprite sprite;
            sprite.texture = *frame_info;
            sprite.width = frame_info->width;
            sprite.height = frame_info->height;

            graphics_system->render_sprite_centered_scaled(enemy_screen_x, enemy_screen_y, sprite,
                                                           render_width, render_height, flip_h);
        };

        if (enemy.state == ENEMY_STATE_SPAWNED) {
            render_enemy_base();
            continue;
        }

        // Pit-fall sentinel: render enemy clamped at bottom for one frame,
        // with no spark effect, then despawn on the next update tick.
        if (enemy.state == ENEMY_STATE_PIT_FALL_SENTINEL) {
            render_enemy_base();
            continue;
        }

        // Dying enemy states: white spark (2..7) or red spark (8..13).
        if (enemy.state >= ENEMY_STATE_WHITE_SPARK) {
            uint8_t normalized_state = enemy.state;
            if (enemy.state >= ENEMY_STATE_RED_SPARK) {
                normalized_state = static_cast<uint8_t>(
                    enemy.state - (ENEMY_STATE_RED_SPARK - ENEMY_STATE_WHITE_SPARK));
            }

            // Match original layering: draw enemy below spark for first 3 spark frames.
            if (normalized_state <= ENEMY_STATE_WHITE_SPARK + 2) {
                render_enemy_base();
            }

            const uint8_t spark_set = (enemy.state >= ENEMY_STATE_RED_SPARK) ? 1 : 0;
            const uint8_t spark_base =
                (spark_set == 0) ? ENEMY_STATE_WHITE_SPARK : ENEMY_STATE_RED_SPARK;
            const uint8_t spark_frame = static_cast<uint8_t>((enemy.state - spark_base) % 3);

            const Sprite* spark_sprite = spark_sprites[spark_set][spark_frame];
            if (!spark_sprite || !spark_sprite->texture.texture) {
                continue;
            }

            graphics_system->render_sprite_centered_scaled(
                enemy_screen_x, enemy_screen_y, *spark_sprite, render_scale * 2, render_scale * 2);
        }
    }
}

bool ActorSystem::load_effect_sprites(GraphicsSystem* graphics_system) {
    if (!graphics_system) {
        return false;
    }

    bool ok = true;
    const char* names[2] = {"white_spark", "red_spark"};
    for (int set = 0; set < 2; ++set) {
        for (int frame = 0; frame < 3; ++frame) {
            const std::string dir = std::to_string(frame);
            if (!graphics_system->load_sprite(names[set], dir)) {
                std::cerr << "Failed to load spark sprite " << names[set] << "_" << frame
                          << std::endl;
                ok = false;
            }
            spark_sprites[set][frame] = graphics_system->get_sprite(names[set], dir);
            if (!spark_sprites[set][frame]) {
                ok = false;
            }
        }
    }

    return ok;
}

/**
 * Main update function - called once per game tick
 */
void ActorSystem::update(uint8_t comic_x, uint8_t comic_y, uint8_t comic_facing,
                         const uint8_t* tiles, int camera_x, uint8_t fire_key) {
    // Store global state for use by behavior functions
    g_comic_x = comic_x;
    g_comic_y = comic_y;
    g_comic_facing = comic_facing;
    g_camera_x = camera_x;
    current_tiles = tiles;

    auto despawn_enemy = [&](enemy_t& enemy) {
        enemy.state = ENEMY_STATE_DESPAWNED;
        enemy.spawn_timer_and_animation = enemy_respawn_counter_cycle;

        // Cycle respawn timer: 20→40→60→80→100→20
        enemy_respawn_counter_cycle += RESPAWN_TIMER_STEP;
        if (enemy_respawn_counter_cycle > RESPAWN_TIMER_MAX) {
            enemy_respawn_counter_cycle = RESPAWN_TIMER_MIN;
        }
    };

    spawned_this_tick = 0;

    for (int i = 0; i < MAX_NUM_ENEMIES; i++) {
        enemy_t& enemy = enemies[i];

        // Handle despawned state
        if (enemy.state == ENEMY_STATE_DESPAWNED) {
            if (enemy.spawn_timer_and_animation > 0) {
                enemy.spawn_timer_and_animation--;
            }
            if (enemy.spawn_timer_and_animation == 0) {
                maybe_spawn_enemy(i);
            }
            continue;
        }

        if (enemy.state == ENEMY_STATE_PIT_FALL_SENTINEL) {
            despawn_enemy(enemy);
            continue;
        }

        // Handle death animation states (white spark or red spark)
        if (enemy.state >= ENEMY_STATE_WHITE_SPARK) {
            // Still animating spark effect
            if ((enemy.state == ENEMY_STATE_WHITE_SPARK + DEATH_ANIMATION_LAST_FRAME) ||
                (enemy.state == ENEMY_STATE_RED_SPARK + DEATH_ANIMATION_LAST_FRAME)) {
                // Animation finished; despawn
                despawn_enemy(enemy);
            } else {
                // Advance animation frame
                enemy.state++;
            }
            continue;
        }

        // Handle spawned state
        if (enemy.state == ENEMY_STATE_SPAWNED) {
            update_enemy_animation(&enemy);
            handle_single_enemy(i);
            check_enemy_despawn(&enemy);

            if (enemy.state != ENEMY_STATE_SPAWNED) {
                continue;
            }

            check_enemy_player_collision(&enemy);
        }
    }

    // ---- Fireball system ----
    // Fire input: try to spawn a fireball when fire key pressed and meter available
    if (fire_key && fireball_meter > 0 && comic_firepower > 0) {
        try_to_fire();

        // Decrement meter every 2 ticks when firing (on the tick where counter==2)
        if (fireball_meter_counter == FIREBALL_METER_COUNTER_INIT) {
            if (fireball_meter > 0) {
                fireball_meter--;
            }
        }
    } else if (!fire_key) {
        // Recharge meter every 2 ticks when not firing (on counter wrap 1→0)
        if (fireball_meter_counter == 1) {
            if (fireball_meter < MAX_FIREBALL_METER) {
                fireball_meter++;
            }
        }
    }

    // Advance meter counter: 2→1→2→1...
    if (fireball_meter_counter > 1) {
        fireball_meter_counter--;
    } else {
        fireball_meter_counter = FIREBALL_METER_COUNTER_INIT;
    }

    // Move fireballs and check enemy collisions
    handle_fireballs();

    // ---- Item system ----
    // Toggle animation counter each tick
    item_animation_counter = (item_animation_counter == 0) ? 1 : 0;

    // Handle item collection and rendering
    handle_item();
}

/**
 * Update animation frame
 */
void ActorSystem::update_enemy_animation(enemy_t* enemy) {
    if (!enemy || enemy->num_animation_frames == 0) {
        return;
    }

    enemy->spawn_timer_and_animation++;
    if (enemy->spawn_timer_and_animation >= enemy->num_animation_frames) {
        enemy->spawn_timer_and_animation = 0;  // Loop animation
    }
}

/**
 * Try to spawn an enemy from despawned state
 */
bool ActorSystem::maybe_spawn_enemy(int enemy_index) {
    if (enemy_index < 0 || enemy_index >= MAX_NUM_ENEMIES) {
        return false;
    }

    // Only spawn one enemy per tick
    if (spawned_this_tick) {
        return false;
    }

    enemy_t& enemy = enemies[enemy_index];

    // Check if slot is used
    if ((enemy.behavior & ~ENEMY_BEHAVIOR_FAST) >= ENEMY_BEHAVIOR_UNUSED) {
        enemy.state = ENEMY_STATE_DESPAWNED;
        enemy.spawn_timer_and_animation = 100;
        return false;
    }

    // Determine spawn position
    spawn_offset_cycle += 2;
    if (spawn_offset_cycle >= PLAYFIELD_WIDTH + 7) {
        spawn_offset_cycle = PLAYFIELD_WIDTH;
    }

    // Use int for intermediate calculation to avoid overflow
    int spawn_x_temp;
    if (g_comic_facing == COMIC_FACING_RIGHT) {
        // Spawn to the right
        spawn_x_temp = g_camera_x + spawn_offset_cycle;
    } else {
        // Spawn to the left
        spawn_x_temp = g_camera_x - (spawn_offset_cycle - PLAYFIELD_WIDTH + 2);
    }

    // Clamp to valid uint8_t range [0, 255]
    if (spawn_x_temp < 0) {
        spawn_x_temp = 0;
    } else if (spawn_x_temp > 255) {
        spawn_x_temp = 255;
    }
    uint8_t spawn_x = static_cast<uint8_t>(spawn_x_temp);

    // Determine spawn Y by matching assembly behavior:
    // 1) Start at Comic's feet rounded to even boundary + 4.
    // 2) Scan upward until finding a solid tile.
    // 3) Continue scanning upward until finding first non-solid tile above it.
    int search_y = (static_cast<int>(g_comic_y) & 0xFE) + 4;
    if (search_y > PLAYFIELD_HEIGHT - 2) {
        search_y = PLAYFIELD_HEIGHT - 2;
    }

    bool solid_found = false;
    while (search_y > 0) {
        if (is_tile_solid(get_tile_at(spawn_x, static_cast<uint8_t>(search_y)))) {
            solid_found = true;
            break;
        }
        search_y -= 2;
    }

    if (!solid_found) {
        return false;
    }

    bool spawn_found = false;
    uint8_t spawn_y = 0;
    while (search_y >= 0) {
        search_y -= 2;
        if (search_y < 0) {
            break;
        }

        if (!is_tile_solid(get_tile_at(spawn_x, static_cast<uint8_t>(search_y)))) {
            spawn_y = static_cast<uint8_t>(search_y);
            spawn_found = true;
            break;
        }
    }

    if (!spawn_found) {
        return false;
    }

    // Initialize spawned enemy
    spawned_this_tick = 1;
    enemy.x = spawn_x;
    enemy.y = spawn_y;
    enemy.state = ENEMY_STATE_SPAWNED;
    enemy.spawn_timer_and_animation = 0;  // Start animation at frame 0

    // Initialize velocities based on behavior type
    uint8_t behavior_type = enemy.behavior & ~ENEMY_BEHAVIOR_FAST;
    switch (behavior_type) {
        case ENEMY_BEHAVIOR_BOUNCE:
        case ENEMY_BEHAVIOR_SHY:
            enemy.x_vel = -1;  // Start moving left
            enemy.y_vel = -1;  // Start moving up
            enemy.facing = ENEMY_FACING_LEFT;
            break;

        case ENEMY_BEHAVIOR_LEAP:
        case ENEMY_BEHAVIOR_ROLL:
        case ENEMY_BEHAVIOR_SEEK:
        default:
            enemy.x_vel = 0;
            enemy.y_vel = 0;
            enemy.facing = ENEMY_FACING_LEFT;
            break;
    }

    // Set restraint based on speed flag
    if (enemy.behavior & ENEMY_BEHAVIOR_FAST) {
        enemy.restraint = ENEMY_RESTRAINT_MOVE_EVERY_TICK;
    } else {
        enemy.restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
    }

    return true;
}

/**
 * Handle a single spawned enemy's AI update
 */
void ActorSystem::handle_single_enemy(int enemy_index) {
    if (enemy_index < 0 || enemy_index >= MAX_NUM_ENEMIES) {
        return;
    }

    enemy_t& enemy = enemies[enemy_index];
    uint8_t behavior_type = enemy.behavior & ~ENEMY_BEHAVIOR_FAST;

    switch (behavior_type) {
        case ENEMY_BEHAVIOR_BOUNCE:
            enemy_behavior_bounce(&enemy);
            break;
        case ENEMY_BEHAVIOR_LEAP:
            enemy_behavior_leap(&enemy);
            break;
        case ENEMY_BEHAVIOR_ROLL:
            enemy_behavior_roll(&enemy);
            break;
        case ENEMY_BEHAVIOR_SEEK:
            enemy_behavior_seek(&enemy);
            break;
        case ENEMY_BEHAVIOR_SHY:
            enemy_behavior_shy(&enemy);
            break;
        default:
            break;
    }
}

/**
 * Check if enemy should despawn due to distance
 */
void ActorSystem::check_enemy_despawn(enemy_t* enemy) const {
    if (!enemy) return;

    int16_t x_diff = static_cast<int16_t>(static_cast<int>(enemy->x) - static_cast<int>(g_comic_x));
    if (x_diff < -ENEMY_DESPAWN_RADIUS || x_diff > ENEMY_DESPAWN_RADIUS) {
        enemy->state = ENEMY_STATE_DESPAWNED;
        enemy->spawn_timer_and_animation = enemy_respawn_counter_cycle;
    }
}

/**
 * Check collision between enemy and player
 */
void ActorSystem::check_enemy_player_collision(enemy_t* enemy) const {
    if (!enemy) return;

    if (enemy->state != ENEMY_STATE_SPAWNED) {
        return;
    }

    if (is_player_dying()) {
        return;
    }

    int16_t x_diff = static_cast<int16_t>(static_cast<int>(enemy->x) - static_cast<int>(g_comic_x));
    int16_t y_diff = static_cast<int16_t>(static_cast<int>(enemy->y) - static_cast<int>(g_comic_y));

    // Collision box: horizontal abs(enemy.x - comic.x) <= 1, vertical 0 <= (enemy.y - comic.y) < 4
    if (x_diff >= -1 && x_diff <= 1 && y_diff >= 0 && y_diff < 4) {
        // Collision! Start red spark death animation
        enemy->state = ENEMY_STATE_RED_SPARK;

        // Shield absorbs six hits (HP 6 -> 0); next hit at 0 HP kills Comic.
        if (comic_hp > 0) {
            comic_hp--;
            play_game_sound(GameSound::PLAYER_HIT);
        } else {
            trigger_player_death();
        }
    }
}

/**
 * Check if a tile is solid
 */
bool ActorSystem::is_tile_solid(uint8_t tile_id) const { return tile_id > tileset_last_passable; }

/**
 * Get tile at coordinates
 */
uint8_t ActorSystem::get_tile_at(uint8_t x, uint8_t y) const {
    if (!current_tiles) {
        return 0;  // Passable if no tile data available
    }

    // Each tile is 2 game units
    uint8_t tile_x = x / 2;
    uint8_t tile_y = y / 2;

    if (tile_x >= current_map_width_tiles || tile_y >= current_map_height_tiles) {
        return 0;  // Out of bounds = passable
    }

    uint16_t index = tile_y * current_map_width_tiles + tile_x;
    return current_tiles[index];
}

/**
 * Check horizontal collision for enemy at (x, y)
 * Matches assembly check_horizontal_enemy_map_collision:
 * Checks tile at (x, y). If y is odd (enemy spans two tiles vertically),
 * also checks (x, y+1).
 */
bool ActorSystem::check_horizontal_enemy_map_collision(uint8_t x, uint8_t y) const {
    if (is_tile_solid(get_tile_at(x, y))) {
        return true;
    }
    // If y is odd, enemy spans two vertical tiles - check the one below
    if (y & 1) {
        return is_tile_solid(get_tile_at(x, static_cast<uint8_t>(y + 1)));
    }
    return false;
}

/**
 * Check vertical collision for enemy at (x, y)
 * Matches assembly check_vertical_enemy_map_collision:
 * Checks tile at (x, y). If x is odd (enemy spans two tiles horizontally),
 * also checks (x+1, y).
 */
bool ActorSystem::check_vertical_enemy_map_collision(uint8_t x, uint8_t y) const {
    if (is_tile_solid(get_tile_at(x, y))) {
        return true;
    }
    // If x is odd, enemy spans two horizontal tiles - check the one to the right
    if (x & 1) {
        return is_tile_solid(get_tile_at(static_cast<uint8_t>(x + 1), y));
    }
    return false;
}

// ============================================================================
// AI BEHAVIORS
// ============================================================================

/**
 * BOUNCE behavior: Diagonal bouncing with independent x/y velocities
 * Used by: Fire Ball, Brave Bird
 */
void ActorSystem::enemy_behavior_bounce(enemy_t* enemy) const {
    if (!enemy) return;

    // Handle restraint (movement throttle)
    if (enemy->restraint == ENEMY_RESTRAINT_SKIP_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
        return;  // Skip this tick
    }

    if (enemy->restraint == ENEMY_RESTRAINT_MOVE_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;  // Skip next tick
    }

    // Horizontal movement
    uint8_t next_x;
    int16_t camera_rel_x;

    if (enemy->x_vel > 0) {
        // Moving right
        enemy->facing = ENEMY_FACING_RIGHT;
        next_x = static_cast<uint8_t>(enemy->x + 2);
        if (check_horizontal_enemy_map_collision(next_x, enemy->y)) {
            enemy->x_vel = -1;  // Bounce left
        } else {
            enemy->x = static_cast<uint8_t>(enemy->x + 1);
            camera_rel_x = static_cast<int16_t>(enemy->x) - static_cast<int16_t>(g_camera_x);
            if (camera_rel_x >= PLAYFIELD_WIDTH - 2) {
                enemy->x_vel = -1;  // Hit right edge, bounce
            }
        }
    } else {
        // Moving left
        enemy->facing = ENEMY_FACING_LEFT;
        if (enemy->x == 0) {
            enemy->x_vel = 1;  // Hit left edge, bounce
        } else {
            next_x = static_cast<uint8_t>(enemy->x - 1);
            if (check_horizontal_enemy_map_collision(next_x, enemy->y)) {
                enemy->x_vel = 1;  // Bounce right
            } else {
                enemy->x = next_x;
                camera_rel_x = static_cast<int16_t>(enemy->x) - static_cast<int16_t>(g_camera_x);
                if (camera_rel_x <= 0) {
                    enemy->x_vel = 1;  // Hit left edge, bounce
                }
            }
        }
    }

    // Vertical movement
    uint8_t next_y;

    if (enemy->y_vel > 0) {
        // Moving down
        if (enemy->y >= PLAYFIELD_HEIGHT - 2) {
            enemy->y_vel = -1;  // Hit bottom, bounce up
        } else {
            next_y = static_cast<uint8_t>(enemy->y + 2);
            if (check_vertical_enemy_map_collision(enemy->x, next_y)) {
                enemy->y_vel = -1;  // Hit solid tile, bounce
            } else {
                enemy->y = static_cast<uint8_t>(enemy->y + 1);
                if (enemy->y >= PLAYFIELD_HEIGHT - 2) {
                    enemy->y_vel = -1;
                }
            }
        }
    } else {
        // Moving up
        if (enemy->y == 0) {
            enemy->y_vel = 1;  // Hit top, bounce down
        } else {
            next_y = static_cast<uint8_t>(enemy->y - 1);
            if (check_vertical_enemy_map_collision(enemy->x, next_y)) {
                enemy->y_vel = 1;  // Hit solid tile, bounce
            } else {
                enemy->y = next_y;
                if (enemy->y == 0) {
                    enemy->y_vel = 1;
                }
            }
        }
    }
}

/**
 * LEAP behavior: Jump toward Comic with gravity
 * Used by: Bug-eyes, Blind Toad, Beach Ball
 *
 * Mirrors R5sw1991.asm:enemy_behavior_leap exactly.
 *
 * Assembly flow:
 *   y_vel < 0  (.moving_up):   delta = y_vel>>3 (arithmetic; e.g. -7>>3 = -1)
 *                               proposed_y += delta  (moves up 1 unit for y_vel=-7)
 *                               undo on ceiling collision or top-of-field underflow
 *   y_vel > 0  (.moving_down): delta = y_vel>>3 (e.g. 8>>3 = 1, move down 1)
 *                               despawn at PLAYFIELD_HEIGHT-2
 *                               undo on solid at proposed_y+1 (.start_falling)
 *   y_vel == 0: check y+2 for solid ground
 *               solid   → .begin_leap: set x_vel toward Comic, y_vel=-7, return
 *               no solid → .start_falling → .apply_gravity (fall through, NO early return)
 *   .apply_gravity: y_vel += 2, clamp to TERMINAL_VELOCITY  (runs for all paths except .begin_leap)
 *   Restraint only skips horizontal movement; gravity and .check_for_ground always run
 *   .check_for_ground: if y_vel>0, check y+3; on solid: snap y=(y+1)&0xfe, y_vel=0
 */
void ActorSystem::enemy_behavior_leap(enemy_t* enemy) const {
    if (!enemy) return;

    // proposed_y tracks vertical changes before committing (mirrors ax.lo in assembly)
    uint8_t proposed_y = enemy->y;

    if (enemy->y_vel < 0) {
        // === .moving_up ===
        // Assembly: sar y_vel 3x (arithmetic) → y_vel/8, still negative (e.g. -7>>3 = -1)
        //           neg → positive upward delta; sub al, delta → proposed_y += delta (negative =
        //           up)
        // Simplified: proposed_y += (int8_t)(y_vel / 8);
        int16_t new_y = static_cast<int16_t>(proposed_y) + static_cast<int16_t>(enemy->y_vel / 8);
        if (new_y < 0) {
            // Unsigned underflow → hit top of playfield (.undo_position_change)
            // cppcheck-suppress redundantAssignment
            // Intentional reassignment to match .undo_position_change assembly label semantics:
            // explicitly signals the "undo" path even though proposed_y was already enemy->y
            proposed_y = enemy->y;  // restore original (no change)
        } else {
            uint8_t target_y = static_cast<uint8_t>(new_y);
            if (!check_vertical_enemy_map_collision(enemy->x, target_y)) {
                proposed_y = target_y;  // accept upward movement
            }
            // else: ceiling collision → .undo_position_change (proposed_y stays = enemy->y)
        }
        // fall through to .apply_gravity

    } else if (enemy->y_vel > 0) {
        // === .moving_down ===
        // Assembly: sar y_vel 3x → y_vel/8; proposed_y += that; e.g. y_vel=8 → move down 1
        int8_t vel_over_8 = static_cast<int8_t>(enemy->y_vel >> ENEMY_VELOCITY_SHIFT);
        uint8_t new_y = static_cast<uint8_t>(proposed_y + vel_over_8);

        // Despawn at or below the bottom of the playfield
        if (new_y >= PLAYFIELD_HEIGHT - 2) {
            enemy->state = ENEMY_STATE_PIT_FALL_SENTINEL;
            enemy->y = PLAYFIELD_HEIGHT - 2;
            return;
        }

        // .keep_moving_down: look-ahead check at new_y+1
        // Assembly: inc al; check_vertical; dec al; jnc .apply_gravity (accept move)
        //            else: .start_falling → .undo_position_change (restore original pos)
        if (check_vertical_enemy_map_collision(enemy->x, static_cast<uint8_t>(new_y + 1))) {
            // cppcheck-suppress redundantAssignment
            // Intentional reassignment to match .undo_position_change assembly label semantics:
            // explicitly signals the "undo" path even though proposed_y was already enemy->y
            proposed_y = enemy->y;  // collision: restore original (.undo_position_change)
        } else {
            proposed_y = new_y;  // no collision: accept downward movement
        }
        // fall through to .apply_gravity

    } else {
        // === y_vel == 0 ===
        // Assembly: ax = [si+enemy.y]; al += 2; check_vertical;
        //           if solid → .begin_leap; else → .start_falling
        if (check_vertical_enemy_map_collision(enemy->x, static_cast<uint8_t>(enemy->y + 2))) {
            // .begin_leap: solid ground — initiate jump toward Comic
            // Assembly: cmp comic_x(ah), enemy.x(dh); jae (.unsigned >=) → x_vel=+1 else -1
            // jae means: if comic_x >= enemy.x → enemy is at/left-of Comic → move right (+1)
            enemy->x_vel = (g_comic_x >= enemy->x) ? 1 : -1;
            enemy->y_vel = ENEMY_JUMP_VELOCITY;  // -7
            // Assembly jumps directly to .done: stores position (unchanged), no gravity this tick
            return;
        }
        // .start_falling → .undo_position_change → .apply_gravity
        // proposed_y stays = enemy->y; y_vel stays = 0 (gravity below will set it to 2)
        // CRITICAL: do NOT return here — must fall through to gravity + horizontal + ground check
    }

    // === .apply_gravity ===
    // Runs for all paths except .begin_leap (which returned above).
    // Assembly: dl += 2; clamp to TERMINAL_VELOCITY; store y_vel
    {
        int16_t new_vel = static_cast<int16_t>(enemy->y_vel) + ENEMY_GRAVITY;  // +2
        enemy->y_vel =
            static_cast<int8_t>(new_vel > TERMINAL_VELOCITY ? TERMINAL_VELOCITY : new_vel);
    }

    // === Restraint — only gates horizontal movement; gravity and ground-check always run ===
    bool skip_horizontal = false;
    if (enemy->restraint == ENEMY_RESTRAINT_SKIP_THIS_TICK) {
        // .skip_this_tick: set restraint to MOVE, then fall to .check_for_ground (skip horizontal)
        enemy->restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
        skip_horizontal = true;
    } else if (enemy->restraint == ENEMY_RESTRAINT_MOVE_THIS_TICK) {
        // Slow enemy: transition MOVE → SKIP
        enemy->restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;
    }
    // ENEMY_RESTRAINT_MOVE_EVERY_TICK: no state change

    // === Horizontal movement (skipped when restraint was SKIP_THIS_TICK) ===
    if (!skip_horizontal) {
        int16_t camera_rel_x;
        if (enemy->x_vel > 0) {
            // Moving right: check tile at x+2, advance x by 1, bounce at right playfield edge
            uint8_t next_x = static_cast<uint8_t>(enemy->x + 2);
            if (check_horizontal_enemy_map_collision(next_x, proposed_y)) {
                enemy->x_vel = -1;  // wall → bounce left
            } else {
                enemy->x = static_cast<uint8_t>(enemy->x + 1);
                camera_rel_x = static_cast<int16_t>(enemy->x) - static_cast<int16_t>(g_camera_x);
                if (camera_rel_x >= PLAYFIELD_WIDTH - 2) {
                    enemy->x_vel = -1;  // right playfield edge
                }
            }
        } else if (enemy->x_vel < 0) {
            // Moving left: check tile at x-1, advance x by -1, bounce at left playfield edge
            if (enemy->x == 0) {
                enemy->x_vel = 1;  // already at left edge
            } else {
                uint8_t next_x = static_cast<uint8_t>(enemy->x - 1);
                if (check_horizontal_enemy_map_collision(next_x, proposed_y)) {
                    enemy->x_vel = 1;  // wall → bounce right
                } else {
                    enemy->x = next_x;
                    camera_rel_x =
                        static_cast<int16_t>(enemy->x) - static_cast<int16_t>(g_camera_x);
                    if (camera_rel_x <= 0) {
                        enemy->x_vel = 1;  // left playfield edge
                    }
                }
            }
        }
    }

    // === Commit vertical position (.done in assembly: "mov [si+enemy.y], dx") ===
    enemy->y = proposed_y;

    // === .check_for_ground: landing detection ===
    // Assembly: dx = ax; if y_vel <= 0: goto .done (still rising/hovering)
    //           al += 3; check_vertical; if solid: .landed → inc dl; and dl, 0xfe; y_vel=0
    if (enemy->y_vel > 0) {
        if (check_vertical_enemy_map_collision(enemy->x, static_cast<uint8_t>(enemy->y + 3))) {
            // .landed: snap to even tile boundary, stop vertical movement
            enemy->y = static_cast<uint8_t>((enemy->y + 1) & 0xFE);
            enemy->y_vel = 0;
        }
    }
}

/**
 * ROLL behavior: Ground-following movement
 * Used by: Glow Globe
 */
void ActorSystem::enemy_behavior_roll(enemy_t* enemy) const {
    if (!enemy) return;

    uint8_t next_x;

    // Vertical movement: falling or on ground
    if (enemy->y_vel > 0) {
        // Falling: check if near bottom and despawn
        if (enemy->y + 1 >= PLAYFIELD_HEIGHT - 3) {
            enemy->state = ENEMY_STATE_PIT_FALL_SENTINEL;
            enemy->y = PLAYFIELD_HEIGHT - 2;
            return;
        }
        // Move down one unit (maintains horizontal momentum from before falling)
        enemy->y = static_cast<uint8_t>(enemy->y + 1);
    } else {
        // On ground: update direction toward Comic
        if (enemy->x < g_comic_x) {
            enemy->x_vel = 1;
        } else if (enemy->x > g_comic_x) {
            enemy->x_vel = -1;
        } else {
            enemy->x_vel = 0;
        }
    }

    // Handle restraint (applies to both falling and rolling)
    if (enemy->restraint == ENEMY_RESTRAINT_SKIP_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
        return;
    }

    if (enemy->restraint == ENEMY_RESTRAINT_MOVE_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;
    }

    // Horizontal movement
    if (enemy->x_vel == 0) {
        enemy->restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
        return;
    }

    if (enemy->x_vel > 0) {
        // Moving right
        next_x = static_cast<uint8_t>(enemy->x + 2);
        if (!check_horizontal_enemy_map_collision(next_x, enemy->y)) {
            enemy->x = static_cast<uint8_t>(enemy->x + 1);
        }
    } else {
        // Moving left
        if (enemy->x == 0) {
            enemy->x_vel = 1;  // Hit left edge, reverse direction
        } else {
            next_x = static_cast<uint8_t>(enemy->x - 1);
            if (!check_horizontal_enemy_map_collision(next_x, enemy->y)) {
                enemy->x = next_x;
            }
        }
    }

    // Check for ground below
    if (!check_vertical_enemy_map_collision(enemy->x, static_cast<uint8_t>(enemy->y + 3))) {
        // No ground - start falling
        enemy->y_vel = 1;
        return;
    }

    // On ground
    enemy->y_vel = 0;
}

/**
 * SEEK behavior: Pathfinding toward player
 * Used by: Killer Bee
 */
void ActorSystem::enemy_behavior_seek(enemy_t* enemy) const {
    if (!enemy) return;

    // Handle restraint
    if (enemy->restraint == ENEMY_RESTRAINT_SKIP_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
        return;
    }

    if (enemy->restraint == ENEMY_RESTRAINT_MOVE_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;
    }

    // Horizontal movement toward Comic (prioritized)
    if (enemy->x != g_comic_x) {
        if (enemy->x < g_comic_x) {
            // Move right
            uint8_t next_x = static_cast<uint8_t>(enemy->x + 1);
            bool collision =
                check_horizontal_enemy_map_collision(static_cast<uint8_t>(next_x + 1), enemy->y);

            if (!collision) {
                enemy->x = next_x;
                enemy->x_vel = 1;
            } else {
                enemy->x_vel = -1;  // Blocked, try left next time
            }
        } else {
            // Move left
            if (enemy->x == 0) {
                enemy->x_vel = 1;  // Hit left edge, reverse direction
            } else {
                uint8_t next_x = static_cast<uint8_t>(enemy->x - 1);
                bool collision = check_horizontal_enemy_map_collision(next_x, enemy->y);

                if (!collision) {
                    enemy->x = next_x;
                    enemy->x_vel = -1;
                } else {
                    enemy->x_vel = 1;  // Blocked, try right next time
                }
            }
        }

        enemy->facing = (enemy->x_vel < 0) ? ENEMY_FACING_LEFT : ENEMY_FACING_RIGHT;
        return;  // Continue to next tick after x movement
    }

    // Vertical movement when x is aligned
    if (enemy->y != g_comic_y) {
        if (enemy->y < g_comic_y) {
            // Move down
            uint8_t next_y = static_cast<uint8_t>(enemy->y + 1);
            bool collision =
                check_vertical_enemy_map_collision(enemy->x, static_cast<uint8_t>(next_y + 1));

            if (!collision) {
                enemy->y = next_y;
                enemy->y_vel = 1;
            } else {
                enemy->y_vel = -1;
            }
        } else {
            // Move up
            uint8_t next_y = static_cast<uint8_t>(enemy->y - 1);
            bool collision = check_vertical_enemy_map_collision(enemy->x, next_y);

            if (!collision) {
                enemy->y = next_y;
                enemy->y_vel = -1;
            } else {
                enemy->y_vel = 1;
            }
        }
    }

    enemy->facing = (enemy->x_vel < 0) ? ENEMY_FACING_LEFT : ENEMY_FACING_RIGHT;
}

/**
 * SHY behavior: Flees when Comic is facing toward, approaches otherwise
 * Used by: Shy Bird, Spinner
 */
void ActorSystem::enemy_behavior_shy(enemy_t* enemy) const {
    if (!enemy) return;

    uint8_t next_x, next_y;
    int8_t comic_facing_enemy;
    bool collision;
    int16_t camera_rel_x;

    // Handle restraint (movement throttle)
    if (enemy->restraint == ENEMY_RESTRAINT_SKIP_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;
        return;  // Skip this tick
    }

    if (enemy->restraint == ENEMY_RESTRAINT_MOVE_THIS_TICK) {
        enemy->restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;  // Skip next tick
    }
    // Determine if Comic is facing this enemy
    if (g_comic_facing == COMIC_FACING_RIGHT && enemy->x > g_comic_x) {
        comic_facing_enemy = 1;  // Comic facing enemy on right
    } else if (g_comic_facing == COMIC_FACING_LEFT && enemy->x < g_comic_x) {
        comic_facing_enemy = 1;  // Comic facing enemy on left
    } else {
        comic_facing_enemy = 0;  // Comic facing away
    }

    // Horizontal movement
    if (enemy->x_vel > 0) {
        // Moving right
        enemy->facing = ENEMY_FACING_RIGHT;
        next_x = static_cast<uint8_t>(enemy->x + 2);
        collision = check_horizontal_enemy_map_collision(next_x, enemy->y);
        if (collision) {
            enemy->x_vel = -1;
        } else {
            enemy->x = static_cast<uint8_t>(enemy->x + 1);
            camera_rel_x = static_cast<int16_t>(enemy->x) - static_cast<int16_t>(g_camera_x);
            if (camera_rel_x >= PLAYFIELD_WIDTH - 2) {
                enemy->x_vel = -1;  // Hit right edge
            }
        }
    } else {
        // Moving left
        enemy->facing = ENEMY_FACING_LEFT;
        if (enemy->x == 0) {
            enemy->x_vel = 1;
        } else {
            next_x = static_cast<uint8_t>(enemy->x - 1);
            collision = check_horizontal_enemy_map_collision(next_x, enemy->y);
            if (collision) {
                enemy->x_vel = 1;
            } else {
                enemy->x = next_x;
                camera_rel_x = static_cast<int16_t>(enemy->x) - static_cast<int16_t>(g_camera_x);
                if (camera_rel_x <= 0) {
                    enemy->x_vel = 1;  // Hit left edge
                }
            }
        }
    }

    // Vertical movement depends on whether Comic is facing this enemy
    if (comic_facing_enemy) {
        // Comic is facing this enemy - always move up (flee upward unconditionally)
        enemy->y_vel = -1;
    } else {
        // Comic is facing away - move toward Comic's y (approach)
        if (enemy->y < g_comic_y) {
            enemy->y_vel = 1;  // Move down
        } else if (enemy->y > g_comic_y) {
            enemy->y_vel = -1;  // Move up
        } else {
            enemy->y_vel = 0;  // Aligned
        }
    }

    // Apply vertical movement
    if (enemy->y_vel > 0) {
        // Moving down
        next_y = static_cast<uint8_t>(enemy->y + 2);
        collision = check_vertical_enemy_map_collision(enemy->x, next_y);
        if (collision) {
            enemy->y_vel = -1;  // Bounce up
        } else {
            enemy->y = static_cast<uint8_t>(enemy->y + 1);
            if (enemy->y >= PLAYFIELD_HEIGHT - 2) {
                enemy->y_vel = -1;
            }
        }
    } else if (enemy->y_vel < 0) {
        // Moving up
        if (enemy->y == 0) {
            enemy->y_vel = 1;  // Bounce down from top
        } else {
            next_y = static_cast<uint8_t>(enemy->y - 1);
            collision = check_vertical_enemy_map_collision(enemy->x, next_y);
            if (collision) {
                // Hit a solid tile above - bounce back down
                enemy->y_vel = 1;
            } else {
                enemy->y = next_y;
                // Hit the top of the playfield after moving - bounce back down
                if (enemy->y == 0) {
                    enemy->y_vel = 1;
                }
            }
        }
    }
}

// ============================================================================
// FIREBALL SYSTEM
// ============================================================================

/**
 * Reset all fireballs to inactive state (called on stage load)
 */
void ActorSystem::reset_fireballs() {
    for (auto& fb : fireballs) {
        fb.x = FIREBALL_DEAD;
        fb.y = FIREBALL_DEAD;
    }
}

/**
 * Load fireball sprite frames from the graphics system.
 * Looks for sprite-fireball_0.png and sprite-fireball_1.png.
 * Returns true if both frames loaded successfully.
 */
bool ActorSystem::load_fireball_sprites(GraphicsSystem* graphics_system) {
    if (!graphics_system) {
        return false;
    }

    bool ok = true;
    for (uint8_t i = 0; i < FIREBALL_NUM_FRAMES; i++) {
        std::string dir = std::to_string(i);
        if (!graphics_system->load_sprite("fireball", dir)) {
            std::cerr << "Failed to load fireball sprite frame " << static_cast<int>(i)
                      << std::endl;
            ok = false;
        } else {
            fireball_sprite[i] = graphics_system->get_sprite("fireball", dir);
        }
    }
    return ok;
}

/**
 * Try to fire a fireball into an open slot (private, called from update()).
 * Spawn position: (comic_x, comic_y + 1).
 * Velocity: +FIREBALL_VELOCITY if facing right, -FIREBALL_VELOCITY if facing left.
 * Initialises corkscrew_phase=2 so the next tick applies the first corkscrew step.
 */
void ActorSystem::try_to_fire() {
    if (comic_firepower == 0) {
        return;
    }

    // Find the first open slot within the unlocked fireball count
    for (int i = 0; i < comic_firepower && i < static_cast<int>(fireballs.size()); i++) {
        fireball_t& fb = fireballs[i];
        if (fb.x == FIREBALL_DEAD && fb.y == FIREBALL_DEAD) {
            // Spawn fireball at Comic's chest height
            fb.y = static_cast<uint8_t>(g_comic_y + 1);
            fb.x = g_comic_x;
            fb.vel =
                (g_comic_facing == COMIC_FACING_RIGHT) ? FIREBALL_VELOCITY : -FIREBALL_VELOCITY;
            fb.corkscrew_phase = 2;
            fb.animation = 0;
            fb.num_animation_frames = FIREBALL_NUM_FRAMES;
            play_game_sound(GameSound::FIRE);
            return;  // Only one fireball per fire-input tick
        }
    }
}

/**
 * Move all active fireballs and check collisions with enemies.
 * Matches handle_fireballs() from jsandas/comic-c src/actors.c and R5sw1991.asm:5653.
 *
 * Movement loop (first comic_firepower slots):
 *   - Apply horizontal velocity (±FIREBALL_VELOCITY)
 *   - Deactivate if outside camera viewport
 *   - Apply corkscrew Y oscillation when comic_has_corkscrew is set
 *   - Advance animation frame
 *
 * Collision loop (all MAX_NUM_FIREBALLS slots):
 *   - For each active fireball × each SPAWNED enemy:
 *     - Vertical overlap: 0 ≤ (fb.y − enemy.y) ≤ 1
 *     - Horizontal overlap: |fb.x − enemy.x| ≤ 1
 *     - Hit: enemy → ENEMY_STATE_WHITE_SPARK, fireball → FIREBALL_DEAD
 */
void ActorSystem::handle_fireballs() {
    if (comic_firepower == 0) {
        return;
    }

    // --- Movement pass ---
    for (int i = 0; i < comic_firepower && i < static_cast<int>(fireballs.size()); i++) {
        fireball_t& fb = fireballs[i];

        if (fb.x == FIREBALL_DEAD && fb.y == FIREBALL_DEAD) {
            continue;
        }

        // Apply horizontal velocity
        int new_x = static_cast<int>(fb.x) + fb.vel;

        // Compute camera-relative x for bounds check
        int rel_x = new_x - g_camera_x;
        if (rel_x < 0 || rel_x > PLAYFIELD_WIDTH - 2) {
            fb.x = FIREBALL_DEAD;
            fb.y = FIREBALL_DEAD;
            continue;
        }
        fb.x = static_cast<uint8_t>(new_x);

        // Corkscrew vertical oscillation (when Comic has the Corkscrew item)
        if (comic_has_corkscrew) {
            // Phase 2: move down, transition to 1
            // Phase 1: move up, transition back to 2
            if (fb.corkscrew_phase == 2) {
                fb.y++;
                fb.corkscrew_phase = 1;
            } else if (fb.corkscrew_phase == 1) {
                fb.y--;
                fb.corkscrew_phase = 2;
            }
        }

        // Advance animation frame (wraps at num_animation_frames)
        fb.animation++;
        if (fb.animation >= fb.num_animation_frames) {
            fb.animation = 0;
        }
    }

    // --- Collision pass (checks all MAX_NUM_FIREBALLS slots vs all enemies) ---
    for (int i = 0; i < static_cast<int>(fireballs.size()); i++) {
        fireball_t& fb = fireballs[i];

        if (fb.x == FIREBALL_DEAD && fb.y == FIREBALL_DEAD) {
            continue;
        }

        for (int j = 0; j < MAX_NUM_ENEMIES; j++) {
            enemy_t& enemy = enemies[j];

            if (enemy.state != ENEMY_STATE_SPAWNED) {
                continue;
            }

            // Vertical: 0 ≤ (fb.y − enemy.y) ≤ 1
            int8_t y_diff = static_cast<int8_t>(static_cast<int>(fb.y) - static_cast<int>(enemy.y));
            if (y_diff < 0 || y_diff > 1) {
                continue;
            }

            // Horizontal: |fb.x − enemy.x| ≤ 1
            int8_t x_diff = static_cast<int8_t>(static_cast<int>(fb.x) - static_cast<int>(enemy.x));
            if (x_diff < -1 || x_diff > 1) {
                continue;
            }

            // Collision!
            enemy.state = ENEMY_STATE_WHITE_SPARK;
            fb.x = FIREBALL_DEAD;
            fb.y = FIREBALL_DEAD;
            award_points(3);  // Award 300 points for killing an enemy with a fireball
            play_game_sound(GameSound::ENEMY_HIT);
            break;  // Fireball consumed; check next fireball
        }
    }
}

/**
 * Render all active fireballs.
 * Fireball sprites are 16x8 px originals; rendered at 2× scale → 32x16 screen pixels.
 */
void ActorSystem::render_fireballs(GraphicsSystem* graphics_system, int camera_x,
                                   int render_scale) const {
    if (!graphics_system || comic_firepower == 0) {
        return;
    }

    // Scale factor: original sprites are 8 px per game unit; we use render_scale px per unit.
    // Sprites are 16x8 = 2 game units wide × 1 game unit tall at original resolution.
    const int scale = render_scale / 8;  // 16/8 = 2
    const int sprite_w = 16 * scale;
    const int sprite_h = 8 * scale;

    for (const auto& fb : fireballs) {
        if (fb.x == FIREBALL_DEAD && fb.y == FIREBALL_DEAD) {
            continue;
        }

        int rel_x = static_cast<int>(fb.x) - camera_x;
        if (rel_x < 0 || rel_x >= PLAYFIELD_WIDTH) {
            continue;
        }

        uint8_t frame_index = fb.animation % FIREBALL_NUM_FRAMES;
        const Sprite* sprite = fireball_sprite[frame_index];
        if (!sprite || !sprite->texture.texture) {
            continue;
        }

        // Screen position: same mapping as enemies (game unit × render_scale + render_scale offset)
        int screen_x = rel_x * render_scale + render_scale;
        int screen_y = static_cast<int>(fb.y) * render_scale + render_scale;

        graphics_system->render_sprite_centered_scaled(screen_x, screen_y, *sprite, sprite_w,
                                                       sprite_h);
    }
}
/* ============================================================================
 * ITEM SYSTEM
 * ============================================================================ */

/**
 * Get current jump power based on boots status
 */
int ActorSystem::get_jump_power() const {
    return comic_has_boots ? JUMP_POWER_WITH_BOOTS : JUMP_POWER_DEFAULT;
}

/**
 * Load item sprites from assets.
 * Item sprites are 16×16 pixels, and each item has two frames (even/odd) for animation.
 *
 * Expected file naming: sprite-<name>_<frame>.png
 * Example: sprite-corkscrew_even.png, sprite-corkscrew_odd.png
 */
bool ActorSystem::load_item_sprites(GraphicsSystem* graphics_system) {
    if (!graphics_system) {
        std::cerr << "Error: GraphicsSystem is null in load_item_sprites" << std::endl;
        return false;
    }

    // Item sprite names mapping (item_type → base filename)
    const char* item_names[15] = {
        "corkscrew",     // 0
        "doorkey",       // 1
        "boots",         // 2
        "lantern",       // 3
        "teleportwand",  // 4
        "gems",          // 5
        "crown",         // 6
        "gold",          // 7
        "cola",          // 8
        nullptr,         // 9 (unused)
        nullptr,         // 10 (unused)
        nullptr,         // 11 (unused)
        nullptr,         // 12 (unused)
        nullptr,         // 13 (unused)
        "shield"         // 14
    };

    const char* frame_names[2] = {"even", "odd"};

    bool all_loaded = true;

    for (int item_type = 0; item_type < 15; item_type++) {
        if (item_names[item_type] == nullptr) {
            continue;  // Skip unused slots
        }

        for (int frame = 0; frame < 2; frame++) {
            std::string sprite_name = item_names[item_type];
            std::string frame_name = frame_names[frame];

            if (!graphics_system->load_sprite(sprite_name, frame_name)) {
                std::cerr << "Warning: Failed to load item sprite: " << sprite_name << "_"
                          << frame_name << std::endl;
                all_loaded = false;
                continue;
            }

            // Get loaded sprite
            item_sprites[item_type][frame] = graphics_system->get_sprite(sprite_name, frame_name);
            if (!item_sprites[item_type][frame]) {
                std::cerr << "Warning: Failed to retrieve item sprite: " << sprite_name << "_"
                          << frame_name << std::endl;
                all_loaded = false;
            }
        }
    }

    return all_loaded;
}

/**
 * Handle item for the current stage: collision detection and rendering.
 * Called each tick from update().
 */
void ActorSystem::handle_item() {
    // Check if stage has an item
    if (current_item_type == ITEM_UNUSED) {
        return;
    }

    // Check if already collected
    if (current_level_index >= 8 || current_stage_index >= 3) {
        return;  // Out of bounds
    }
    if (items_collected[current_level_index][current_stage_index] != 0) {
        return;  // Already collected
    }

    // Check if item is visible in playfield (camera bounds check)
    int rel_x = static_cast<int>(current_item_x) - g_camera_x;
    if (rel_x < 0 || rel_x > PLAYFIELD_WIDTH) {
        return;  // Off-screen, don't check collision (but don't render either)
    }

    // Check collision with Comic
    // Horizontal: abs(item.x - comic_x) <= 1
    int16_t x_diff =
        static_cast<int16_t>(static_cast<int>(current_item_x) - static_cast<int>(g_comic_x));
    if (x_diff >= -1 && x_diff <= 1) {
        // Vertical: 0 <= (item.y - comic_y) < 4
        int16_t y_diff =
            static_cast<int16_t>(static_cast<int>(current_item_y) - static_cast<int>(g_comic_y));
        if (y_diff >= 0 && y_diff < 4) {
            // Collision detected!
            collect_item();
        }
    }
}

/**
 * Collect the current item: mark as collected and apply effects.
 */
void ActorSystem::collect_item() {
    // Mark as collected
    items_collected[current_level_index][current_stage_index] = 1;

    // Items award 2000 points.
    award_points(20);
    play_game_sound(GameSound::ITEM_COLLECT);

    // Apply item effect
    apply_item_effect(current_item_type);
}

/**
 * Apply the effect of collecting a specific item type.
 */
void ActorSystem::apply_item_effect(uint8_t item_type) {
    switch (item_type) {
        case ITEM_BLASTOLA_COLA:
            if (comic_firepower < MAX_NUM_FIREBALLS) {
                comic_firepower++;
            }
            break;

        case ITEM_CORKSCREW:
            comic_has_corkscrew = 1;
            break;

        case ITEM_BOOTS:
            comic_has_boots = 1;
            break;

        case ITEM_LANTERN:
            comic_has_lantern = 1;
            break;

        case ITEM_DOOR_KEY:
            comic_has_door_key = 1;
            // keep global state in sync so door system (which still reads the
            // global variable) sees the change. some code paths reference the
            // global directly rather than querying the ActorSystem.
            ::comic_has_door_key = 1;
            break;

        case ITEM_TELEPORT_WAND:
            comic_has_teleport_wand = 1;
            break;

        case ITEM_SHIELD:
            // Match original behavior: full HP grants a life; otherwise refill HP.
            if (comic_hp >= MAX_HP) {
                award_extra_life();
            } else {
                comic_hp_pending_increase = static_cast<uint8_t>(MAX_HP - comic_hp);
            }
            break;

        case ITEM_GEMS:
            if (!comic_has_gems) {
                award_extra_life();
                comic_has_gems = 1;
                comic_num_treasures++;
            }
            break;

        case ITEM_CROWN:
            if (!comic_has_crown) {
                award_extra_life();
                comic_has_crown = 1;
                comic_num_treasures++;
            }
            break;

        case ITEM_GOLD:
            if (!comic_has_gold) {
                award_extra_life();
                comic_has_gold = 1;
                comic_num_treasures++;
            }
            break;

        default:
            // Unknown item type
            break;
    }
}

/**
 * Render the current stage's item if not yet collected.
 * Item sprites are 16×16 pixels (2 game units × 2 game units).
 */
void ActorSystem::render_item(GraphicsSystem* graphics_system, int camera_x,
                              int render_scale) const {
    if (!graphics_system) {
        return;
    }

    // Check if stage has an item
    if (current_item_type == ITEM_UNUSED) {
        return;
    }

    // Check if already collected
    if (current_level_index >= 8 || current_stage_index >= 3) {
        return;  // Out of bounds
    }
    if (items_collected[current_level_index][current_stage_index] != 0) {
        return;  // Already collected, don't render
    }

    // Check if item is visible in playfield
    int rel_x = static_cast<int>(current_item_x) - camera_x;
    if (rel_x < 0 || rel_x > PLAYFIELD_WIDTH) {
        return;  // Off-screen
    }

    // Get sprite based on animation counter
    if (current_item_type >= 15) {
        return;  // Invalid item type
    }

    const Sprite* sprite = item_sprites[current_item_type][item_animation_counter];
    if (!sprite || !sprite->texture.texture) {
        return;  // Sprite not loaded
    }

    // Item sprites are 16×16 pixels = 2×2 game units
    // Scale to render_scale per game unit
    const int scale = render_scale / 8;  // 16/8 = 2
    const int sprite_w = 16 * scale;
    const int sprite_h = 16 * scale;

    // Screen position: game unit × render_scale + render_scale offset
    int screen_x = rel_x * render_scale + render_scale;
    int screen_y = static_cast<int>(current_item_y) * render_scale + render_scale;

    graphics_system->render_sprite_centered_scaled(screen_x, screen_y, *sprite, sprite_w, sprite_h);
}