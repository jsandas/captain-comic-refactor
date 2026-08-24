#include <iostream>
#include <string>
#include <vector>

#include "test_cases.h"
#include "test_helpers.h"

static const std::vector<TestCase>& test_registry() {
    static const std::vector<TestCase> tests = {
        // Physics & Tiles
        {"physics_tiles", test_physics_tiles},
        {"jump_edge_trigger", test_jump_edge_trigger},
        {"jump_recharge", test_jump_recharge},
        {"jump_height", test_jump_height},
        {"space_level_uses_lower_gravity", test_space_level_uses_lower_gravity},
        {"jump_top_clamped_to_playfield", test_jump_top_clamped_to_playfield},
        {"player_death_sequence_respawn", test_player_death_sequence_respawn},
        {"player_death_sequence_last_life_allows_one_more_play",
         test_player_death_sequence_last_life_allows_one_more_play},
        {"player_death_sequence_game_over", test_player_death_sequence_game_over},
        {"teleport_does_not_update_respawn_checkpoint",
         test_teleport_does_not_update_respawn_checkpoint},
        {"door_activation_alignment_x", test_door_activation_alignment_x},
        {"door_activation_alignment_y", test_door_activation_alignment_y},
        {"door_key_requirement", test_door_key_requirement},
        {"door_open_key_requirement", test_door_open_key_requirement},
        {"door_state_update_same_level", test_door_state_update_same_level},
        {"door_state_update_different_level", test_door_state_update_different_level},
        {"door_animation_phase_progression_and_render_state",
         test_door_animation_phase_progression_and_render_state},
        {"door_destination_load_deferred_until_entering_complete",
         test_door_destination_load_deferred_until_entering_complete},
        {"door_entry_sets_checkpoint_for_respawn", test_door_entry_sets_checkpoint_for_respawn},
        {"stage_left_exit_blocked", test_stage_left_exit_blocked},
        {"stage_right_exit_blocked", test_stage_right_exit_blocked},
        {"stage_left_edge_detection", test_stage_left_edge_detection},
        {"stage_right_edge_detection", test_stage_right_edge_detection},
        {"cave_level_solidity", test_cave_level_solidity},
        {"problematic_levels_have_solid_tiles", test_problematic_levels_have_solid_tiles},

        // Graphics & Assets
        {"animation_looping", test_animation_looping},
        {"animation_non_looping", test_animation_non_looping},
        {"animation_zero_duration", test_animation_zero_duration},
        {"animation_frames_initialize_sprite_state", test_animation_frames_initialize_sprite_state},
        {"enemy_animation_sequence", test_enemy_animation_sequence},
        {"tileset_blackout_state_tracking", test_tileset_blackout_state_tracking},
        {"asset_path_resolution", test_asset_path_resolution},
        {"tileset_blackout_state_tracks_unloaded_tileset",
         test_tileset_blackout_state_tracks_unloaded_tileset},
        {"runtime_level_tiles_populated", test_runtime_level_tiles_populated},
        {"playfield_viewport_height_matches_render_scale",
         test_playfield_viewport_height_matches_render_scale},

        // Actors & Items
        {"actor_spawn_one_per_tick", test_actor_spawn_one_per_tick},
        {"actor_spawn_offset_cycling", test_actor_spawn_offset_cycling},
        {"actor_despawn_distance", test_actor_despawn_distance},
        {"actor_player_collision", test_actor_player_collision},
        {"actor_death_animation", test_actor_death_animation},
        {"actor_respawn_timer_cycling", test_actor_respawn_timer_cycling},
        {"actor_animation_frames", test_actor_animation_frames},
        {"actor_behavior_bounce_movement", test_actor_behavior_bounce_movement},
        {"actor_restraint_throttling", test_actor_restraint_throttling},
        {"actor_door_key_sync", test_actor_door_key_sync},
        {"actor_spawn_avoids_solid_tiles", test_actor_spawn_avoids_solid_tiles},
        {"actor_pit_fall_despawns_without_spark", test_actor_pit_fall_despawns_without_spark},
        {"fireball_meter_depletion_timing", test_fireball_meter_depletion_timing},
        {"fireball_meter_recharge_timing", test_fireball_meter_recharge_timing},
        {"fireball_offscreen_deactivates", test_fireball_offscreen_deactivates},
        {"fireball_collision_sets_white_spark", test_fireball_collision_sets_white_spark},
        {"fireball_corkscrew_motion", test_fireball_corkscrew_motion},
        {"item_collection_tracking", test_item_collection_tracking},
        {"item_blastola_cola_firepower", test_item_blastola_cola_firepower},
        {"item_boots_jump_power", test_item_boots_jump_power},
        {"item_corkscrew_flag", test_item_corkscrew_flag},
        {"item_treasure_counting", test_item_treasure_counting},
        {"item_special_items", test_item_special_items},

        // Audio
        {"audio_init_shutdown_idempotency", test_audio_init_shutdown_idempotency},
        {"audio_graceful_failure_when_not_initialized",
         test_audio_graceful_failure_when_not_initialized},
        {"audio_priority_interrupt", test_audio_priority_interrupt},
        {"audio_priority_blocking", test_audio_priority_blocking},
        {"audio_enemy_hit_interrupts_fire", test_audio_enemy_hit_interrupts_fire},
        {"audio_all_sounds_playable", test_audio_all_sounds_playable},
        {"audio_music_playback", test_audio_music_playback},

        // UI & Score
        {"ui_score_base100_encoding", test_ui_score_base100_encoding},
        {"ui_fireball_meter_cell_mapping", test_ui_fireball_meter_cell_mapping},
        {"ui_boots_detection", test_ui_boots_detection},
        {"ui_score_edge_cases", test_ui_score_edge_cases},
        {"ui_meter_all_states", test_ui_meter_all_states},
        {"award_points_no_carry", test_award_points_no_carry},
        {"award_points_accumulates_in_byte0", test_award_points_accumulates_in_byte0},
        {"award_points_carry_into_byte1", test_award_points_carry_into_byte1},
        {"award_points_full_carry_amount", test_award_points_full_carry_amount},
        {"award_points_large_value_above_255", test_award_points_large_value_above_255},
        {"award_points_max_score_saturation", test_award_points_max_score_saturation},
        {"award_points_large_carry_saturation", test_award_points_large_carry_saturation},
        {"award_points_awards_extra_life_every_50000",
         test_award_points_awards_extra_life_every_50000},
        {"award_points_awards_extra_life_after_500_internal_units",
         test_award_points_awards_extra_life_after_500_internal_units},
        {"high_score_bytes_conversion", test_high_score_bytes_conversion}};
    return tests;
}

static bool matches_filter(const std::string& name, const std::string& filter) {
    if (filter.empty()) {
        return true;
    }
    return name.find(filter) != std::string::npos;
}

static int run_tests(const std::string& filter) {
    initialize_level_data();

    int tests_run = 0;
    for (const auto& test : test_registry()) {
        if (!matches_filter(test.name, filter)) {
            continue;
        }
        tests_run++;
        test.run();
    }

    if (tests_run == 0) {
        std::cerr << "No tests match filter: " << filter << std::endl;
        return 1;
    }

    if (failures == 0) {
        std::cout << "All tests passed." << std::endl;
        return 0;
    }

    std::cerr << failures << " test(s) failed." << std::endl;
    return 1;
}

static void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [--list] [--filter NAME]" << std::endl;
}

int main(int argc, char** argv) {
    std::string filter;
    bool list_only = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list") {
            list_only = true;
        } else if (arg == "--filter") {
            if (i + 1 < argc) {
                filter = argv[++i];
            } else {
                std::cerr << "--filter requires a value" << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        } else if (arg.rfind("--filter=", 0) == 0) {
            filter = arg.substr(std::string("--filter=").size());
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if (list_only) {
        for (const auto& test : test_registry()) {
            std::cout << test.name << std::endl;
        }
        return 0;
    }

    return run_tests(filter);
}
