#include <algorithm>
#include <cstring>

#include "test_cases.h"
#include "test_helpers.h"

void test_physics_tiles() {
    reset_physics_state();
    init_test_level();

    uint8_t ground_tile = get_tile_at(0, 18);  // tile_y = 9 (ground row)
    check(ground_tile == 0x3F, "ground tile should be 0x3F");

    uint8_t out_of_bounds = get_tile_at(255, 30);  // tile_y = 15 (out of bounds)
    check(out_of_bounds == 0, "out-of-bounds tile should be 0");

    check(is_tile_solid(0x3F), "tile 0x3F should be solid");
    check(!is_tile_solid(0x3E), "tile 0x3E should be passable");
}

void test_jump_edge_trigger() {
    reset_physics_state();

    key_state_jump = 1;
    simulate_tick();
    check(comic_is_falling_or_jumping == 1, "jump should start on key press edge");

    comic_is_falling_or_jumping = 0;
    comic_y = 14;
    comic_y_vel = 0;
    comic_jump_counter = comic_jump_power;
    key_state_jump = 1;
    simulate_tick();
    check(comic_is_falling_or_jumping == 0, "holding jump should not retrigger");
}

void test_jump_recharge() {
    reset_physics_state();

    comic_jump_counter = 1;
    key_state_jump = 0;
    simulate_tick();
    check(comic_jump_counter == comic_jump_power, "jump counter should recharge on release");
}

void test_jump_height() {
    reset_physics_state();
    int default_height = measure_jump_height(JUMP_POWER_DEFAULT);
    int boots_height = measure_jump_height(JUMP_POWER_WITH_BOOTS);

    check(default_height == 7,
          "default jump height should be 7 units (got " + std::to_string(default_height) + ")");
    check(boots_height == 9,
          "boots jump height should be 9 units (got " + std::to_string(boots_height) + ")");
}

void test_space_level_uses_lower_gravity() {
    reset_physics_state();
    comic_x = 4;
    comic_y = 6;
    comic_is_falling_or_jumping = 1;
    comic_jump_counter = 1;
    comic_y_vel = 0;
    key_state_jump = 0;
    key_state_left = 0;
    key_state_right = 0;

    current_level_number = LEVEL_NUMBER_FOREST;
    handle_fall_or_jump();
    check(comic_y_vel == COMIC_GRAVITY, "normal levels should apply COMIC_GRAVITY");

    reset_physics_state();
    comic_x = 4;
    comic_y = 6;
    comic_is_falling_or_jumping = 1;
    comic_jump_counter = 1;
    comic_y_vel = 0;
    key_state_jump = 0;
    key_state_left = 0;
    key_state_right = 0;

    current_level_number = LEVEL_NUMBER_SPACE;
    handle_fall_or_jump();
    check(comic_y_vel == COMIC_GRAVITY_SPACE, "space level should apply COMIC_GRAVITY_SPACE");
}

void test_jump_top_clamped_to_playfield() {
    reset_physics_state();
    comic_x = 4;
    comic_y = 0;
    comic_is_falling_or_jumping = 1;
    comic_jump_counter = 2;
    comic_y_vel = -16;
    key_state_jump = 1;
    key_state_left = 0;
    key_state_right = 0;
    current_level_number = LEVEL_NUMBER_FOREST;

    handle_fall_or_jump();

    check(comic_y == 0, "jump top clamp: player y should stay at top boundary (0)");
    check(!is_player_dying(), "jump top clamp: top clamp should not trigger death state");
}

void test_player_death_sequence_respawn() {
    reset_physics_state();
    game_over_triggered = false;
    comic_num_lives = 3;
    comic_hp = 1;
    comic_x = 50;
    comic_y = 9;
    comic_x_momentum = 3;
    comic_y_vel = 4;
    comic_is_falling_or_jumping = 0;
    comic_jump_counter = 0;
    key_state_jump = 1;
    previous_key_state_jump = 1;
    comic_x_checkpoint = 22;
    comic_y_checkpoint = 12;

    trigger_player_death(true);
    check(is_player_dying(), "death: player should enter dying state after trigger");
    check(should_show_player_death_animation(),
          "death: animation phase should be active immediately after trigger");

    int animation_ticks = 0;
    while (is_player_dying() && should_show_player_death_animation() && animation_ticks < 64) {
        update_player_death_sequence();
        animation_ticks++;
    }

    check(animation_ticks > 0, "death: animation phase should consume at least one tick");
    check(is_player_dying() && !should_show_player_death_animation(),
          "death: sequence should transition into too-bad wait phase");

    advance_death_sequence_until_complete();

    check(!is_player_dying(), "death: sequence should complete for respawn path");
    check(comic_num_lives == 2, "death: lives should decrement by one on respawn path");
    check(!game_over_triggered, "death: game_over flag should remain false when lives remain");
    check(comic_x == comic_x_checkpoint && comic_y == comic_y_checkpoint,
          "death: respawn path should restore checkpoint position");
    check(comic_hp == MAX_HP, "death: respawn path should restore full HP");
    check(comic_is_falling_or_jumping == 1,
          "death: respawn path should place player into falling state");
    check(comic_jump_counter == comic_jump_power,
          "death: respawn path should reset jump counter to jump power");
    check(key_state_jump == 0 && previous_key_state_jump == 0,
          "death: respawn path should clear jump input edge state");
}

void test_player_death_sequence_game_over() {
    reset_physics_state();
    game_over_triggered = false;
    comic_num_lives = 0;
    comic_hp = 2;
    comic_x = 41;
    comic_y = 8;
    comic_x_checkpoint = 20;
    comic_y_checkpoint = 12;

    trigger_player_death(false);
    check(is_player_dying(), "game_over: player should enter dying state");
    check(!should_show_player_death_animation(),
          "game_over: animation visibility should be off when trigger requested no animation");

    advance_death_sequence_until_complete();

    check(!is_player_dying(), "game_over: death sequence should finish");
    check(comic_num_lives == 0, "game_over: lives should remain zero");
    check(game_over_triggered, "game_over: zero lives should trigger game-over flag");
    check(comic_x == 41 && comic_y == 8,
          "game_over: zero-lives path should not respawn to checkpoint");
    check(comic_hp == 2, "game_over: zero-lives path should not reset HP");
}

void test_player_death_sequence_last_life_allows_one_more_play() {
    reset_physics_state();
    game_over_triggered = false;
    comic_num_lives = 1;
    comic_hp = 2;
    comic_x = 41;
    comic_y = 8;
    comic_x_checkpoint = 20;
    comic_y_checkpoint = 12;

    trigger_player_death(false);
    check(is_player_dying(), "last_life: player should enter dying state");

    advance_death_sequence_until_complete();

    check(!is_player_dying(), "last_life: death sequence should finish");
    check(comic_num_lives == 0, "last_life: one remaining life should be consumed on respawn path");
    check(!game_over_triggered,
          "last_life: game over should not trigger until the next death at zero lives");
    check(comic_x == comic_x_checkpoint && comic_y == comic_y_checkpoint,
          "last_life: player should respawn at checkpoint when starting death with one life");
    check(comic_hp == MAX_HP, "last_life: respawn should refill HP");
}

void test_teleport_does_not_update_respawn_checkpoint() {
    reset_physics_state();

    comic_x_checkpoint = 22;
    comic_y_checkpoint = 12;

    comic_x = 40;
    comic_y = 8;

    apply_teleport_destination_if_ready(3, 60, 6, comic_x, comic_y);

    check(comic_x == 60 && comic_y == 6,
          "teleport_checkpoint: teleport frame should move player to destination");
    check(comic_x_checkpoint == 22 && comic_y_checkpoint == 12,
          "teleport_checkpoint: teleport should not modify respawn checkpoint");
}

void test_door_activation_alignment_x() {
    reset_physics_state();
    reset_door_state();
    level_t* level = create_test_level_with_door(10, 8, 1, 1);
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;

    // Test activation at exact door position
    comic_x = 10;
    comic_y = 8;
    uint8_t result = check_door_activation();
    check(result == 1, "door should activate at exact position (x=10, y=8)");

    // Test activation 1 unit to the right
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;
    comic_x = 11;
    comic_y = 8;
    result = check_door_activation();
    check(result == 1, "door should activate 1 unit offset (x=11, y=8)");

    // Test activation 2 units to the right (boundary)
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;
    comic_x = 12;
    comic_y = 8;
    result = check_door_activation();
    check(result == 1, "door should activate 2 units offset (x=12, y=8)");

    // Test no activation 3 units away (too far)
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;
    comic_x = 13;
    comic_y = 8;
    result = check_door_activation();
    check(result == 0, "door should not activate 3 units away (x=13, y=8)");

    // Test no activation to the left
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;
    comic_x = 9;
    comic_y = 8;
    result = check_door_activation();
    check(result == 0, "door should not activate left of position (x=9, y=8)");
}

void test_door_activation_alignment_y() {
    reset_physics_state();
    reset_door_state();
    level_t* level = create_test_level_with_door(10, 8, 1, 1);
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;

    // Test exact Y match
    comic_x = 10;
    comic_y = 8;
    uint8_t result = check_door_activation();
    check(result == 1, "door should activate at exact Y (y=8)");

    // Test Y one unit above
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;
    comic_x = 10;
    comic_y = 7;
    result = check_door_activation();
    check(result == 0, "door should not activate above door (y=7)");

    // Test Y one unit below
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    key_state_open = 1;
    comic_x = 10;
    comic_y = 9;
    result = check_door_activation();
    check(result == 0, "door should not activate below door (y=9)");
}

void test_door_key_requirement() {
    reset_physics_state();
    reset_door_state();
    level_t* level = create_test_level_with_door(10, 8, 1, 1);
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_x = 10;
    comic_y = 8;
    key_state_open = 1;

    // Test without key
    comic_has_door_key = 0;
    uint8_t result = check_door_activation();
    check(result == 0, "door should not activate without key");

    // Test with key
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    comic_x = 10;
    comic_y = 8;
    key_state_open = 1;
    result = check_door_activation();
    check(result == 1, "door should activate with key");
}

void test_door_open_key_requirement() {
    reset_physics_state();
    reset_door_state();
    level_t* level = create_test_level_with_door(10, 8, 1, 1);
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    comic_x = 10;
    comic_y = 8;

    // Test without open key pressed
    key_state_open = 0;
    uint8_t result = check_door_activation();
    check(result == 0, "check_door_activation should return 0 when open key not pressed");

    // Test with open key pressed
    reset_door_state();
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    comic_x = 10;
    comic_y = 8;
    key_state_open = 1;
    result = check_door_activation();
    check(result == 1, "check_door_activation should return 1 when open key pressed");
}

void test_door_state_update_same_level() {
    reset_physics_state();
    reset_door_state();
    level_t* level = create_test_level_with_door(10, 8, 1, 1);  // Target same level, stage 1
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    comic_x = 10;
    comic_y = 8;
    key_state_open = 1;

    // Before activation
    check(source_door_level_number == -1, "source_door_level_number should start as -1");
    check(source_door_stage_number == -1, "source_door_stage_number should start as -1");
    check(current_stage_number == 0, "current_stage_number should start as 0");

    // Activate door (will call load_new_stage due to same level)
    uint8_t result = check_door_activation();
    check(result == 1, "door activation to same level stage should succeed");

    // After activation, verify state was set
    check(source_door_level_number == 1, "source_door_level_number should be 1 (origin level)");
    check(source_door_stage_number == 0, "source_door_stage_number should be 0 (origin stage)");
    check(current_stage_number == 1, "current_stage_number should be 1 (target stage)");
    check(current_level_number == 1, "current_level_number should remain 1 (same level)");
}

void test_door_state_update_different_level() {
    reset_physics_state();
    reset_door_state();
    level_t* level = create_test_level_with_door(10, 8, 2, 1);  // Target different level
    current_level_ptr = level;
    current_level_number = 1;
    current_stage_number = 0;
    comic_has_door_key = 1;
    comic_x = 10;
    comic_y = 8;
    key_state_open = 1;

    // Before activation
    check(source_door_level_number == -1, "source_door_level_number should start as -1");
    check(source_door_stage_number == -1, "source_door_stage_number should start as -1");
    check(current_level_number == 1, "current_level_number should start as 1");

    // Activate door (will call load_new_level due to different level)
    uint8_t result = check_door_activation();
    check(result == 1, "door activation to different level should succeed");

    // After activation, verify state was set
    check(source_door_level_number == 1, "source_door_level_number should be 1 (origin level)");
    check(source_door_stage_number == 0, "source_door_stage_number should be 0 (origin stage)");
    check(current_level_number == 2, "current_level_number should be 2 (target level)");
    check(current_stage_number == 1, "current_stage_number should be 1 (target stage)");
}

static void check_door_render_state(DoorAnimationRenderMode expected_mode,
                                    bool expected_draw_overlay_in_front,
                                    bool expected_player_visible, const std::string& context) {
    uint8_t world_x = 0;
    uint8_t world_y = 0;
    DoorAnimationRenderMode mode = DoorAnimationRenderMode::NONE;
    bool draw_overlay_in_front = false;
    bool player_visible = false;

    bool has_render_state = get_door_animation_render_state(
        &world_x, &world_y, &mode, &draw_overlay_in_front, &player_visible);

    check(has_render_state, context + ": expected active door render state");
    check(mode == expected_mode, context + ": unexpected render mode");
    check(draw_overlay_in_front == expected_draw_overlay_in_front,
          context + ": unexpected overlay layering");
    check(player_visible == expected_player_visible, context + ": unexpected player visibility");
}

void test_door_animation_phase_progression_and_render_state() {
    reset_physics_state();
    reset_door_state();
    initialize_level_data();

    current_level_number = LEVEL_NUMBER_FOREST;
    current_stage_number = 0;
    current_level_ptr = get_level_by_number(current_level_number);
    comic_x = 10;
    comic_y = 8;
    g_skip_load_on_door = false;

    door_t door{};
    door.x = 10;
    door.y = 8;
    door.target_level = LEVEL_NUMBER_LAKE;
    door.target_stage = 1;

    activate_door(&door);

    check(g_door_anim_phase == DoorAnimationPhase::ENTERING,
          "door_anim_progression: phase should start in ENTERING");
    check(g_door_anim_frame == 0, "door_anim_progression: frame should start at 0");
    check_door_render_state(DoorAnimationRenderMode::HALF_OPEN, false, true,
                            "door_anim_progression entering frame 0");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::ENTERING,
          "door_anim_progression: phase should remain ENTERING at frame 1");
    check(g_door_anim_frame == 1, "door_anim_progression: frame should advance to 1");
    check_door_render_state(DoorAnimationRenderMode::FULL_OPEN, false, true,
                            "door_anim_progression entering frame 1");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::ENTERING,
          "door_anim_progression: phase should remain ENTERING at frame 2");
    check(g_door_anim_frame == 2, "door_anim_progression: frame should advance to 2");
    check_door_render_state(DoorAnimationRenderMode::HALF_CLOSED, true, true,
                            "door_anim_progression entering frame 2");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::ENTERING,
          "door_anim_progression: phase should remain ENTERING at frame 3");
    check(g_door_anim_frame == 3, "door_anim_progression: frame should advance to 3");
    check_door_render_state(DoorAnimationRenderMode::NONE, false, false,
                            "door_anim_progression entering frame 3");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::EXIT_DELAY,
          "door_anim_progression: phase should transition to EXIT_DELAY");
    check(g_door_anim_frame == 0, "door_anim_progression: frame should reset to 0 at EXIT_DELAY");
    check_door_render_state(DoorAnimationRenderMode::NONE, false, false,
                            "door_anim_progression exit delay");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::EXIT_DELAY,
          "door_anim_progression: phase should remain EXIT_DELAY tick 1");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::EXIT_DELAY,
          "door_anim_progression: phase should remain EXIT_DELAY tick 2");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::EXITING,
          "door_anim_progression: phase should transition to EXITING");
    check(g_door_anim_frame == 0, "door_anim_progression: EXITING should start at frame 0");
    check_door_render_state(DoorAnimationRenderMode::NONE, false, false,
                            "door_anim_progression exiting frame 0");

    update_door_animation_tick();
    check(g_door_anim_frame == 1, "door_anim_progression: EXITING frame should advance to 1");
    check_door_render_state(DoorAnimationRenderMode::HALF_OPEN, true, true,
                            "door_anim_progression exiting frame 1");

    update_door_animation_tick();
    check(g_door_anim_frame == 2, "door_anim_progression: EXITING frame should advance to 2");
    check_door_render_state(DoorAnimationRenderMode::FULL_OPEN, false, true,
                            "door_anim_progression exiting frame 2");

    update_door_animation_tick();
    check(g_door_anim_frame == 3, "door_anim_progression: EXITING frame should advance to 3");
    check_door_render_state(DoorAnimationRenderMode::HALF_CLOSED, false, true,
                            "door_anim_progression exiting frame 3");

    update_door_animation_tick();
    check(g_door_anim_frame == 4, "door_anim_progression: EXITING frame should advance to 4");
    check_door_render_state(DoorAnimationRenderMode::NONE, false, true,
                            "door_anim_progression exiting frame 4");

    update_door_animation_tick();
    check(g_door_anim_phase == DoorAnimationPhase::NONE,
          "door_anim_progression: phase should end at NONE");
    check(g_door_anim_frame == 0, "door_anim_progression: frame should reset to 0 when complete");

    uint8_t world_x = 0;
    uint8_t world_y = 0;
    DoorAnimationRenderMode mode = DoorAnimationRenderMode::NONE;
    bool draw_overlay_in_front = false;
    bool player_visible = false;
    check(!get_door_animation_render_state(&world_x, &world_y, &mode, &draw_overlay_in_front,
                                           &player_visible),
          "door_anim_progression: render state should be inactive after completion");

    g_skip_load_on_door = true;
}

void test_door_destination_load_deferred_until_entering_complete() {
    reset_physics_state();
    reset_door_state();
    initialize_level_data();

    current_level_number = LEVEL_NUMBER_FOREST;
    current_stage_number = 0;
    current_level_ptr = get_level_by_number(current_level_number);
    comic_x = 10;
    comic_y = 8;
    g_skip_load_on_door = false;

    door_t door{};
    door.x = 10;
    door.y = 8;
    door.target_level = LEVEL_NUMBER_LAKE;
    door.target_stage = 1;

    activate_door(&door);

    check(current_level_number == LEVEL_NUMBER_FOREST,
          "door_deferred_load: target level should not load immediately");
    check(current_stage_number == 0,
          "door_deferred_load: target stage should not load immediately");

    update_door_animation_tick();
    check(current_level_number == LEVEL_NUMBER_FOREST,
          "door_deferred_load: level should remain source during entering frame 1");
    check(current_stage_number == 0,
          "door_deferred_load: stage should remain source during entering frame 1");

    update_door_animation_tick();
    check(current_level_number == LEVEL_NUMBER_FOREST,
          "door_deferred_load: level should remain source during entering frame 2");
    check(current_stage_number == 0,
          "door_deferred_load: stage should remain source during entering frame 2");

    update_door_animation_tick();
    check(current_level_number == LEVEL_NUMBER_FOREST,
          "door_deferred_load: level should remain source during entering frame 3");
    check(current_stage_number == 0,
          "door_deferred_load: stage should remain source during entering frame 3");

    update_door_animation_tick();
    check(current_level_number == LEVEL_NUMBER_LAKE,
          "door_deferred_load: target level should load after entering completes");
    check(current_stage_number == 1,
          "door_deferred_load: target stage should load after entering completes");
    check(g_door_anim_phase == DoorAnimationPhase::EXIT_DELAY,
          "door_deferred_load: phase should move to EXIT_DELAY after deferred load");

    g_skip_load_on_door = true;
}

void test_door_entry_sets_checkpoint_for_respawn() {
    reset_physics_state();
    initialize_level_data();

    current_level_number = LEVEL_NUMBER_LAKE;
    current_stage_number = 0;
    source_door_level_number = LEVEL_NUMBER_FOREST;
    source_door_stage_number = 2;

    comic_x_checkpoint = 14;
    comic_y_checkpoint = 12;

    load_new_level();

    check(comic_x == 119 && comic_y == 10,
          "door_entry_checkpoint: player should spawn at reciprocal door position");

    check(comic_x_checkpoint == 119 && comic_y_checkpoint == 10,
          "door_entry_checkpoint: checkpoint should update to reciprocal door spawn");

    check(source_door_level_number == -1 && source_door_stage_number == -1,
          "door_entry_checkpoint: source door markers should reset after stage load");
}

void test_stage_left_exit_blocked() {
    reset_physics_state();
    comic_x = 10;
    comic_y = 12;
    comic_x_momentum = 0;
    move_left();
    check(comic_x == 9, "Comic should move left normally when not at edge");
}

void test_stage_right_exit_blocked() {
    reset_physics_state();
    comic_x = MAP_WIDTH - 10;
    comic_y = 12;
    comic_x_momentum = 0;
    move_right();
    check(comic_x == MAP_WIDTH - 9, "Comic should move right normally when not at edge");
}

void test_stage_left_edge_detection() {
    reset_physics_state();
    comic_x = 0;
    comic_y = 12;
    comic_x_momentum = -1;
    current_level_ptr = nullptr;
    uint8_t initial_stage = current_stage_number;
    move_left();
    check(current_stage_number == initial_stage,
          "stage should not change when current_level_ptr is null");
    check(comic_x_momentum == 0, "momentum should be cleared when blocked at edge");
}

void test_stage_right_edge_detection() {
    reset_physics_state();
    comic_x = MAP_WIDTH - 2;
    comic_y = 12;
    comic_x_momentum = 1;
    current_level_ptr = nullptr;
    uint8_t initial_stage = current_stage_number;
    move_right();
    check(current_stage_number == initial_stage,
          "stage should not change when current_level_ptr is null");
    check(comic_x_momentum == 0, "momentum should be cleared when blocked at edge");
}

void test_cave_level_solidity() {
    reset_physics_state();
    initialize_level_data();
    current_level_number = LEVEL_NUMBER_CAVE;
    current_stage_number = 0;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    current_level_ptr = nullptr;

    load_new_level();
    check(current_level_ptr != nullptr, "current_level_ptr should be set after load_new_level");
    check(current_level_ptr->tileset_last_passable == 0x09,
          "cave level tileset_last_passable should be 0x09");

    check(is_tile_solid(0x0a), "cave tile 0x0a should be solid");
    check(is_tile_solid(0x14), "cave tile 0x14 should be solid");
    check(!is_tile_solid(0x09), "cave tile 0x09 should be passable");
    check(!is_tile_solid(0x00), "cave tile 0x00 should be passable");

    reset_door_state();
    reset_level_tiles();
}

void test_problematic_levels_have_solid_tiles() {
    reset_physics_state();
    initialize_level_data();

    const level_t* shed = get_level_by_number(LEVEL_NUMBER_SHED);
    check(shed != nullptr, "shed level should exist");
    check(shed->tileset_last_passable == 0x17, "shed level tileset_last_passable should be 0x17");

    current_level_number = LEVEL_NUMBER_SHED;
    current_stage_number = 0;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    current_level_ptr = nullptr;
    load_new_level();
    check(is_tile_solid(0x18), "shed tile 0x18 should be solid (> 0x17)");
    check(!is_tile_solid(0x17), "shed tile 0x17 should be passable (<= 0x17)");
    reset_level_tiles();

    const level_t* base = get_level_by_number(LEVEL_NUMBER_BASE);
    check(base != nullptr, "base level should exist");
    check(base->tileset_last_passable == 0x3b, "base level tileset_last_passable should be 0x3b");

    current_level_number = LEVEL_NUMBER_BASE;
    current_stage_number = 0;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    current_level_ptr = nullptr;
    load_new_level();
    check(is_tile_solid(0x3c), "base tile 0x3c should be solid (> 0x3b)");
    check(!is_tile_solid(0x3b), "base tile 0x3b should be passable (<= 0x3b)");
    reset_level_tiles();

    const level_t* comp = get_level_by_number(LEVEL_NUMBER_COMP);
    check(comp != nullptr, "comp level should exist");
    check(comp->tileset_last_passable == 0x1d, "comp level tileset_last_passable should be 0x1d");

    current_level_number = LEVEL_NUMBER_COMP;
    current_stage_number = 0;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    current_level_ptr = nullptr;
    load_new_level();
    check(is_tile_solid(0x1e), "comp tile 0x1e should be solid (> 0x1d)");
    check(!is_tile_solid(0x1d), "comp tile 0x1d should be passable (<= 0x1d)");
    reset_level_tiles();

    const level_t* cave = get_level_by_number(LEVEL_NUMBER_CAVE);
    check(cave != nullptr, "cave level should exist");
    check(cave->tileset_last_passable == 0x09, "cave level tileset_last_passable should be 0x09");

    current_level_number = LEVEL_NUMBER_CAVE;
    current_stage_number = 0;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    current_level_ptr = nullptr;
    load_new_level();
    check(is_tile_solid(0x0a), "cave tile 0x0a should be solid (> 0x09)");
    check(!is_tile_solid(0x09), "cave tile 0x09 should be passable (<= 0x09)");
    reset_level_tiles();
}
