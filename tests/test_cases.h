#ifndef TEST_CASES_H
#define TEST_CASES_H

// Physics & Doors
void test_physics_tiles();
void test_jump_edge_trigger();
void test_jump_recharge();
void test_jump_height();
void test_space_level_uses_lower_gravity();
void test_jump_top_clamped_to_playfield();
void test_player_death_sequence_respawn();
void test_player_death_sequence_last_life_allows_one_more_play();
void test_player_death_sequence_game_over();
void test_teleport_does_not_update_respawn_checkpoint();
void test_door_activation_alignment_x();
void test_door_activation_alignment_y();
void test_door_key_requirement();
void test_door_open_key_requirement();
void test_door_state_update_same_level();
void test_door_state_update_different_level();
void test_door_animation_phase_progression_and_render_state();
void test_door_destination_load_deferred_until_entering_complete();
void test_door_entry_sets_checkpoint_for_respawn();
void test_stage_left_exit_blocked();
void test_stage_right_exit_blocked();
void test_stage_left_edge_detection();
void test_stage_right_edge_detection();
void test_cave_level_solidity();
void test_problematic_levels_have_solid_tiles();

// Graphics & Assets
void test_animation_looping();
void test_animation_non_looping();
void test_animation_zero_duration();
void test_enemy_animation_sequence();
void test_tileset_blackout_state_tracking();
void test_asset_path_resolution();
void test_tileset_blackout_state_tracks_unloaded_tileset();
void test_runtime_level_tiles_populated();
void test_playfield_viewport_height_matches_render_scale();

// Actors & Items
void test_actor_spawn_one_per_tick();
void test_actor_spawn_offset_cycling();
void test_actor_despawn_distance();
void test_actor_player_collision();
void test_actor_death_animation();
void test_actor_respawn_timer_cycling();
void test_actor_animation_frames();
void test_actor_behavior_bounce_movement();
void test_actor_restraint_throttling();
void test_actor_door_key_sync();
void test_actor_spawn_avoids_solid_tiles();
void test_actor_pit_fall_despawns_without_spark();
void test_fireball_meter_depletion_timing();
void test_fireball_meter_recharge_timing();
void test_fireball_offscreen_deactivates();
void test_fireball_collision_sets_white_spark();
void test_fireball_corkscrew_motion();
void test_item_collection_tracking();
void test_item_blastola_cola_firepower();
void test_item_boots_jump_power();
void test_item_corkscrew_flag();
void test_item_treasure_counting();
void test_item_special_items();

// Audio
void test_audio_init_shutdown_idempotency();
void test_audio_graceful_failure_when_not_initialized();
void test_audio_priority_interrupt();
void test_audio_priority_blocking();
void test_audio_enemy_hit_interrupts_fire();
void test_audio_all_sounds_playable();
void test_audio_music_playback();

// UI & Score
void test_ui_score_base100_encoding();
void test_ui_fireball_meter_cell_mapping();
void test_ui_boots_detection();
void test_ui_score_edge_cases();
void test_ui_meter_all_states();
void test_award_points_no_carry();
void test_award_points_accumulates_in_byte0();
void test_award_points_carry_into_byte1();
void test_award_points_full_carry_amount();
void test_award_points_large_value_above_255();
void test_award_points_max_score_saturation();
void test_award_points_large_carry_saturation();
void test_award_points_awards_extra_life_every_50000();
void test_award_points_awards_extra_life_after_500_internal_units();
void test_high_score_bytes_conversion();

#endif  // TEST_CASES_H
