#include <algorithm>
#include <vector>

#include "test_cases.h"
#include "test_helpers.h"

static void setup_test_enemy(std::vector<enemy_t>& enemies, int index, uint8_t behavior) {
    enemy_t& enemy = enemies[index];
    enemy.state = ENEMY_STATE_DESPAWNED;
    enemy.spawn_timer_and_animation = 0;  // Ready to spawn
    enemy.x = 0;
    enemy.y = 0;
    enemy.x_vel = 0;
    enemy.y_vel = 0;
    enemy.behavior = behavior;
    enemy.num_animation_frames = 2;
    enemy.facing = ENEMY_FACING_LEFT;
    enemy.restraint = (behavior & ENEMY_BEHAVIOR_FAST) ? ENEMY_RESTRAINT_MOVE_EVERY_TICK
                                                       : ENEMY_RESTRAINT_MOVE_THIS_TICK;
    enemy.sprite_descriptor = nullptr;  // Tests don't need actual level data
    enemy.animation_data = nullptr;     // Tests don't need actual sprite data
}

static void reset_actor_state(const ActorSystem& actor_system) {
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());
    for (auto& enemy : enemies) {
        enemy.state = ENEMY_STATE_DESPAWNED;
    }
    auto& fireballs = const_cast<std::vector<fireball_t>&>(actor_system.get_fireballs());
    for (auto& fireball : fireballs) {
        fireball.x = FIREBALL_DEAD;
        fireball.y = FIREBALL_DEAD;
    }
}

void test_actor_spawn_one_per_tick() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    // Set up 4 enemies ready to spawn
    for (int i = 0; i < MAX_NUM_ENEMIES; i++) {
        setup_test_enemy(enemies, i, ENEMY_BEHAVIOR_BOUNCE);
    }

    std::vector<uint8_t> tiles(128 * 10, 0x00);
    for (int tile_y = 4; tile_y < 10; ++tile_y) {
        for (int tile_x = 0; tile_x < 128; ++tile_x) {
            tiles[tile_y * 128 + tile_x] = 0x40;
        }
    }

    // First update - should spawn exactly 1 enemy
    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x);

    const int spawned =
        static_cast<int>(std::count_if(enemies.begin(), enemies.end(), [](const enemy_t& enemy) {
            return enemy.state == ENEMY_STATE_SPAWNED;
        }));
    check(spawned == 1, "actor_spawn: should spawn exactly 1 enemy per tick");
}

void test_actor_spawn_offset_cycling() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    std::vector<uint8_t> tiles(128 * 10, 0x00);
    for (int tile_y = 4; tile_y < 10; ++tile_y) {
        for (int tile_x = 0; tile_x < 128; ++tile_x) {
            tiles[tile_y * 128 + tile_x] = 0x40;
        }
    }
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    // Spawn offset should cycle: 24→26→28→30→24...
    std::vector<uint8_t> spawn_positions;

    for (int spawn_cycle = 0; spawn_cycle < 5; spawn_cycle++) {
        actor_system.reset_for_stage();
        setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);

        // Trigger spawn
        actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x);

        if (enemies[0].state == ENEMY_STATE_SPAWNED) {
            spawn_positions.push_back(enemies[0].x);
        }
    }

    // Verify spawn positions vary (offset cycling working)
    check(spawn_positions.size() >= 3, "actor_spawn_offset: should spawn multiple times");
    const bool has_variation = std::any_of(spawn_positions.begin() + 1, spawn_positions.end(),
                                           [first_position = spawn_positions[0]](uint8_t position) {
                                               return position != first_position;
                                           });
    check(has_variation, "actor_spawn_offset: spawn positions should vary due to offset cycling");
}

void test_actor_despawn_distance() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    // Manually spawn enemy
    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);
    enemies[0].state = ENEMY_STATE_SPAWNED;
    enemies[0].x = comic_x;
    enemies[0].y = static_cast<uint8_t>(comic_y - 2);
    enemies[0].restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;

    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);
    check(enemies[0].state == ENEMY_STATE_SPAWNED,
          "actor_despawn: enemy should remain spawned when close");

    // Move Comic far away (> ENEMY_DESPAWN_RADIUS = 30)
    comic_x += 35;
    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);

    check(enemies[0].state == ENEMY_STATE_DESPAWNED,
          "actor_despawn: enemy should despawn when far from Comic");

    delete[] tiles;
}

void test_actor_player_collision() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    // Manually position enemy at Comic's location to trigger collision
    // Collision box: horizontal abs(enemy.x - comic.x) <= 1, vertical 0 <= (enemy.y - comic.y) < 4
    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);
    enemies[0].state = ENEMY_STATE_SPAWNED;
    enemies[0].x = comic_x;
    enemies[0].y = static_cast<uint8_t>(comic_y + 1);
    enemies[0].restraint = ENEMY_RESTRAINT_SKIP_THIS_TICK;

    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);

    check(enemies[0].state == ENEMY_STATE_RED_SPARK,
          "actor_collision: enemy should enter RED_SPARK state on collision");

    delete[] tiles;
}

void test_actor_death_animation() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);
    enemies[0].state = ENEMY_STATE_RED_SPARK;

    // Advance through animation frames (RED_SPARK: 8→9→10→11→12→13)
    for (int i = 0; i < 5; i++) {
        actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);
    }

    check(enemies[0].state != ENEMY_STATE_DESPAWNED,
          "actor_death_anim: should still be in death animation");

    // One more tick should complete the animation
    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);

    check(enemies[0].state == ENEMY_STATE_DESPAWNED,
          "actor_death_anim: should despawn after animation completes");

    delete[] tiles;
}

void test_actor_respawn_timer_cycling() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);

    // Complete death animation to trigger despawn
    enemies[0].state = ENEMY_STATE_RED_SPARK + 5;
    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);

    check(enemies[0].state == ENEMY_STATE_DESPAWNED,
          "actor_respawn_cycle: should despawn after death");
    uint8_t timer1 = enemies[0].spawn_timer_and_animation;

    // Trigger another death to advance the cycle
    enemies[0].state = ENEMY_STATE_RED_SPARK + 5;
    actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);
    uint8_t timer2 = enemies[0].spawn_timer_and_animation;

    // Timer should increase: 20→40→60→80→100→20
    check(timer2 > timer1 || (timer1 == 100 && timer2 == 20),
          "actor_respawn_cycle: respawn timer should cycle 20→40→60→80→100→20");

    delete[] tiles;
}

void test_actor_animation_frames() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);
    enemies[0].num_animation_frames = 4;  // 4-frame animation
    enemies[0].state = ENEMY_STATE_SPAWNED;
    enemies[0].spawn_timer_and_animation = 0;

    // Advance animation
    for (int i = 0; i < 5; i++) {
        actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);
    }

    // Animation should loop (frame 0, 1, 2, 3, 0...)
    check(enemies[0].spawn_timer_and_animation < 4,
          "actor_animation: frame index should stay within num_animation_frames");

    delete[] tiles;
}

void test_actor_behavior_bounce_movement() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    // Set up BOUNCE enemy
    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);
    enemies[0].state = ENEMY_STATE_SPAWNED;
    enemies[0].x = 10;
    enemies[0].y = 10;
    enemies[0].x_vel = 1;   // Moving right
    enemies[0].y_vel = -1;  // Moving up
    enemies[0].restraint = ENEMY_RESTRAINT_MOVE_THIS_TICK;

    comic_x = 0;
    comic_y = 0;

    uint8_t start_x = enemies[0].x;
    uint8_t start_y = enemies[0].y;

    // Run a few ticks
    for (int i = 0; i < 5; i++) {
        actor_system.update(comic_x, comic_y, comic_facing, tiles, camera_x);
    }

    // Enemy should have moved (BOUNCE behavior causes diagonal movement)
    bool moved = (enemies[0].x != start_x) || (enemies[0].y != start_y);
    check(moved, "actor_bounce: enemy should move in diagonal pattern");

    delete[] tiles;
}

void test_actor_restraint_throttling() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    const uint8_t* tiles = new uint8_t[128 * 10]();
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    // Slow enemy (no FAST flag)
    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);
    enemies[0].state = ENEMY_STATE_SPAWNED;

    // Fast enemy (FAST flag set)
    setup_test_enemy(enemies, 1, ENEMY_BEHAVIOR_BOUNCE | ENEMY_BEHAVIOR_FAST);
    enemies[1].state = ENEMY_STATE_SPAWNED;

    // Check restraint values
    check(enemies[0].restraint == ENEMY_RESTRAINT_MOVE_THIS_TICK,
          "actor_restraint: slow enemy should have MOVE_THIS_TICK");
    check(enemies[1].restraint == ENEMY_RESTRAINT_MOVE_EVERY_TICK,
          "actor_restraint: fast enemy should have MOVE_EVERY_TICK");

    delete[] tiles;
}

void test_fireball_meter_depletion_timing() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    std::vector<uint8_t> tiles(128 * 10, 0);
    actor_system.comic_firepower = 1;
    actor_system.fireball_meter = 3;

    comic_x = 10;
    comic_y = 10;
    comic_facing = COMIC_FACING_RIGHT;
    camera_x = 0;

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 1);
    check(actor_system.fireball_meter == 2,
          "fireball_meter: should decrement on first firing tick");

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 1);
    check(actor_system.fireball_meter == 2,
          "fireball_meter: should not decrement on second firing tick");

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 1);
    check(actor_system.fireball_meter == 1,
          "fireball_meter: should decrement on third firing tick");
}

void test_fireball_meter_recharge_timing() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    std::vector<uint8_t> tiles(128 * 10, 0);
    comic_x = 10;
    comic_y = 10;
    comic_facing = COMIC_FACING_RIGHT;
    camera_x = 0;

    // Advance once so the counter reaches the recharge phase.
    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 0);
    actor_system.fireball_meter = 10;

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 0);
    check(actor_system.fireball_meter == 11,
          "fireball_meter: should recharge every other tick when idle");
}

void test_fireball_offscreen_deactivates() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    std::vector<uint8_t> tiles(128 * 10, 0);
    actor_system.comic_firepower = 1;
    comic_x = 10;
    comic_y = 10;
    comic_facing = COMIC_FACING_LEFT;
    camera_x = 0;

    auto& fireballs = const_cast<std::vector<fireball_t>&>(actor_system.get_fireballs());
    fireballs[0].x = 1;
    fireballs[0].y = 5;
    fireballs[0].vel = -2;
    fireballs[0].corkscrew_phase = 2;
    fireballs[0].animation = 0;
    fireballs[0].num_animation_frames = FIREBALL_NUM_FRAMES;

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 0);
    check(fireballs[0].x == FIREBALL_DEAD && fireballs[0].y == FIREBALL_DEAD,
          "fireball_offscreen: should deactivate when leaving camera bounds");
}

void test_fireball_collision_sets_white_spark() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    std::vector<uint8_t> tiles(128 * 10, 0);
    actor_system.comic_firepower = 1;
    comic_x = 10;
    comic_y = 0;
    comic_facing = COMIC_FACING_RIGHT;
    camera_x = 0;

    auto& fireballs = const_cast<std::vector<fireball_t>&>(actor_system.get_fireballs());
    fireballs[0].x = 10;
    fireballs[0].y = 5;
    fireballs[0].vel = 0;
    fireballs[0].corkscrew_phase = 2;
    fireballs[0].animation = 0;
    fireballs[0].num_animation_frames = FIREBALL_NUM_FRAMES;

    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());
    enemies[0].state = ENEMY_STATE_SPAWNED;
    enemies[0].x = 10;
    enemies[0].y = 5;
    enemies[0].behavior = 0;
    enemies[0].num_animation_frames = 0;

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 0);
    check(enemies[0].state == ENEMY_STATE_WHITE_SPARK,
          "fireball_collision: enemy should enter WHITE_SPARK on hit");
    check(fireballs[0].x == FIREBALL_DEAD && fireballs[0].y == FIREBALL_DEAD,
          "fireball_collision: fireball should deactivate on hit");
}

void test_fireball_corkscrew_motion() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    std::vector<uint8_t> tiles(128 * 10, 0);
    actor_system.comic_firepower = 1;
    actor_system.comic_has_corkscrew = 1;
    comic_x = 10;
    comic_y = 10;
    comic_facing = COMIC_FACING_RIGHT;
    camera_x = 0;

    auto& fireballs = const_cast<std::vector<fireball_t>&>(actor_system.get_fireballs());
    fireballs[0].x = 10;
    fireballs[0].y = 5;
    fireballs[0].vel = 0;
    fireballs[0].corkscrew_phase = 2;
    fireballs[0].animation = 0;
    fireballs[0].num_animation_frames = FIREBALL_NUM_FRAMES;

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 0);
    check(fireballs[0].y == 6 && fireballs[0].corkscrew_phase == 1,
          "fireball_corkscrew: should move down and flip to phase 1");

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x, 0);
    check(fireballs[0].y == 5 && fireballs[0].corkscrew_phase == 2,
          "fireball_corkscrew: should move up and flip to phase 2");
}

void test_item_collection_tracking() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    // Initially, no items collected
    check(actor_system.comic_firepower == 0, "item_collection: firepower should start at 0");
    check(actor_system.comic_has_boots == 0, "item_collection: should not have boots initially");

    // Simulate collecting Blastola Cola
    actor_system.apply_item_effect(ITEM_BLASTOLA_COLA);
    check(actor_system.comic_firepower == 1,
          "item_collection: Blastola Cola should increase firepower to 1");

    // Collect second Blastola Cola
    actor_system.apply_item_effect(ITEM_BLASTOLA_COLA);
    check(actor_system.comic_firepower == 2,
          "item_collection: second Blastola Cola should increase firepower to 2");

    // Collect Boots
    actor_system.apply_item_effect(ITEM_BOOTS);
    check(actor_system.comic_has_boots == 1, "item_collection: should have boots after collection");
}

void test_actor_door_key_sync() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    // Reset global state
    comic_has_door_key = 0;

    actor_system.apply_item_effect(ITEM_DOOR_KEY);
    check(actor_system.comic_has_door_key == 1, "actor system should have door key flag set");
    check(comic_has_door_key == 1, "global door key variable should be synced by ActorSystem");
}

void test_actor_spawn_avoids_solid_tiles() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    std::vector<uint8_t> tiles(128 * 10, 0x40);  // solid by default (0x40 > 0x3e)
    const int spawn_tile_x = 13;                 // spawn_x ~= 26 on first right-facing spawn cycle

    // Carve a non-solid space above a solid floor in the spawn column.
    for (int tile_y = 0; tile_y <= 3; ++tile_y) {
        tiles[tile_y * 128 + spawn_tile_x] = 0x00;
    }
    for (int tile_y = 4; tile_y < 10; ++tile_y) {
        tiles[tile_y * 128 + spawn_tile_x] = 0x40;
    }

    comic_x = 10;
    comic_y = 10;
    comic_facing = COMIC_FACING_RIGHT;
    camera_x = 0;

    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());
    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_BOUNCE);

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x);

    check(enemies[0].state == ENEMY_STATE_SPAWNED,
          "actor_spawn_solidity: enemy should spawn when a valid spawn column exists");
    check(!actor_system.is_tile_solid(actor_system.get_tile_at(enemies[0].x, enemies[0].y)),
          "actor_spawn_solidity: spawned enemy should not be placed inside a solid tile");
}

void test_actor_pit_fall_despawns_without_spark() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();
    reset_actor_state(actor_system);

    std::vector<uint8_t> tiles(128 * 10, 0x00);  // passable map so pit behavior is reachable
    auto& enemies = const_cast<std::vector<enemy_t>&>(actor_system.get_enemies());

    setup_test_enemy(enemies, 0, ENEMY_BEHAVIOR_ROLL);
    enemies[0].state = ENEMY_STATE_SPAWNED;
    enemies[0].x = 12;
    enemies[0].y = static_cast<uint8_t>(PLAYFIELD_HEIGHT - 3);
    enemies[0].y_vel = 1;
    enemies[0].restraint = ENEMY_RESTRAINT_MOVE_EVERY_TICK;

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x);

    check(enemies[0].state == ENEMY_STATE_PIT_FALL_SENTINEL,
          "actor_pit_fall: enemy should enter pit-fall sentinel (no spark) at bottom edge");
    check(enemies[0].y == PLAYFIELD_HEIGHT - 2,
          "actor_pit_fall: enemy should clamp to the bottom of the playfield before despawn");

    actor_system.update(comic_x, comic_y, comic_facing, tiles.data(), camera_x);

    check(enemies[0].state == ENEMY_STATE_DESPAWNED,
          "actor_pit_fall: enemy should despawn on the tick after bottom-edge clamp");
}

void test_item_blastola_cola_firepower() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    // Initially no firepower
    check(actor_system.comic_firepower == 0, "blastola_cola: firepower should start at 0");

    // Collect 5 Blastola Colas (max firepower)
    for (int i = 0; i < 5; i++) {
        actor_system.apply_item_effect(ITEM_BLASTOLA_COLA);
    }
    check(actor_system.comic_firepower == 5, "blastola_cola: should reach max firepower of 5");

    // Try to collect a 6th (should not exceed max)
    actor_system.apply_item_effect(ITEM_BLASTOLA_COLA);
    check(actor_system.comic_firepower == 5, "blastola_cola: firepower should not exceed 5");
}

void test_item_boots_jump_power() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    // Initially no boots
    int default_jump_power = actor_system.get_jump_power();
    check(default_jump_power == JUMP_POWER_DEFAULT,
          "boots_jump: should have default jump power initially");

    // Collect Boots
    actor_system.apply_item_effect(ITEM_BOOTS);
    int boots_jump_power = actor_system.get_jump_power();
    check(boots_jump_power == JUMP_POWER_WITH_BOOTS,
          "boots_jump: should have increased jump power with boots");
    check(boots_jump_power > default_jump_power,
          "boots_jump: boots jump power should be greater than default");
}

void test_item_corkscrew_flag() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    check(actor_system.comic_has_corkscrew == 0, "corkscrew: should not have corkscrew initially");

    actor_system.apply_item_effect(ITEM_CORKSCREW);
    check(actor_system.comic_has_corkscrew == 1,
          "corkscrew: should have corkscrew after collection");
}

void test_item_treasure_counting() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    // Initially no treasures
    check(actor_system.comic_num_treasures == 0, "treasures: treasure count should start at 0");
    check(actor_system.comic_has_gems == 0, "treasures: should not have gems initially");
    check(actor_system.comic_has_crown == 0, "treasures: should not have crown initially");
    check(actor_system.comic_has_gold == 0, "treasures: should not have gold initially");

    // Collect Gems
    actor_system.apply_item_effect(ITEM_GEMS);
    check(actor_system.comic_has_gems == 1, "treasures: should have gems after collection");
    check(actor_system.comic_num_treasures == 1,
          "treasures: treasure count should be 1 after gems");

    // Collect Crown
    actor_system.apply_item_effect(ITEM_CROWN);
    check(actor_system.comic_has_crown == 1, "treasures: should have crown after collection");
    check(actor_system.comic_num_treasures == 2,
          "treasures: treasure count should be 2 after crown");

    // Collect Gold
    actor_system.apply_item_effect(ITEM_GOLD);
    check(actor_system.comic_has_gold == 1, "treasures: should have gold after collection");
    check(actor_system.comic_num_treasures == 3,
          "treasures: treasure count should be 3 after gold");

    // Collecting same treasure again should not increase count
    actor_system.apply_item_effect(ITEM_GEMS);
    check(actor_system.comic_num_treasures == 3,
          "treasures: duplicate collection should not increase count");
}

void test_item_special_items() {
    reset_physics_state();
    ActorSystem actor_system;
    actor_system.initialize();

    // Door Key
    check(actor_system.comic_has_door_key == 0,
          "special_items: should not have door key initially");
    actor_system.apply_item_effect(ITEM_DOOR_KEY);
    check(actor_system.comic_has_door_key == 1,
          "special_items: should have door key after collection");

    // Teleport Wand
    check(actor_system.comic_has_teleport_wand == 0,
          "special_items: should not have teleport wand initially");
    actor_system.apply_item_effect(ITEM_TELEPORT_WAND);
    check(actor_system.comic_has_teleport_wand == 1,
          "special_items: should have teleport wand after collection");

    // Lantern
    check(actor_system.comic_has_lantern == 0, "special_items: should not have lantern initially");
    actor_system.apply_item_effect(ITEM_LANTERN);
    check(actor_system.comic_has_lantern == 1,
          "special_items: should have lantern after collection");

    // Shield (not full HP): schedule refill to MAX_HP
    comic_num_lives = 2;
    comic_hp = static_cast<uint8_t>(MAX_HP - 2);
    comic_hp_pending_increase = 0;
    actor_system.apply_item_effect(ITEM_SHIELD);
    check(comic_hp_pending_increase == 2,
          "special_items: shield should schedule refill when HP is not full");
    check(comic_num_lives == 2,
          "special_items: shield should not award extra life when HP is not full");

    // Shield (full HP, below cap): award extra life
    comic_hp = MAX_HP;
    comic_hp_pending_increase = 0;
    actor_system.apply_item_effect(ITEM_SHIELD);
    check(comic_num_lives == 3, "special_items: shield should award extra life when HP is full");
    check(comic_hp_pending_increase == 0,
          "special_items: full-HP shield below max lives should not schedule HP refill");

    // Shield (full HP, at cap): keep max lives and award comic-c bonus path.
    comic_num_lives = 5;
    comic_hp = MAX_HP;
    comic_hp_pending_increase = 0;
    score_bytes[0] = 0;
    score_bytes[1] = 0;
    score_bytes[2] = 0;
    actor_system.apply_item_effect(ITEM_SHIELD);
    check(comic_num_lives == 5,
          "special_items: shield at max lives should not increase life count");
    check(comic_hp_pending_increase == MAX_HP,
          "special_items: shield at max lives should set pending HP refill");
    check(score_bytes[0] == 25 && score_bytes[1] == 2 && score_bytes[2] == 0,
          "special_items: shield at max lives should award 22500 points");
}
