#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "../include/actors.h"
#include "../include/audio.h"
#include "../include/doors.h"
#include "../include/graphics.h"
#include "../include/level.h"
#include "../include/level_loader.h"
#include "../include/physics.h"
#include "../include/player_teleport.h"
#include "../include/title_sequence.h"
#include "../include/ui_system.h"

// Test case structure
struct TestCase {
    std::string name;
    void (*run)();
};

// Test globals defined in test_helpers.cpp
extern int failures;
extern int comic_x;
extern int comic_y;
extern int8_t comic_y_vel;
extern int8_t comic_x_momentum;
extern uint8_t comic_facing;
extern uint8_t comic_animation;
extern uint8_t comic_is_falling_or_jumping;
extern uint8_t comic_jump_counter;
extern uint8_t comic_jump_power;
extern uint8_t key_state_jump;
extern uint8_t previous_key_state_jump;
extern uint8_t key_state_left;
extern uint8_t key_state_right;
extern uint8_t key_state_open;
extern int camera_x;

extern uint8_t comic_has_door_key;
extern uint8_t current_level_number;
extern uint8_t current_stage_number;
extern const level_t* current_level_ptr;
extern int8_t source_door_level_number;
extern int8_t source_door_stage_number;

// Door animation globals defined in src/doors.cpp
extern bool g_skip_load_on_door;
extern DoorAnimationPhase g_door_anim_phase;
extern uint8_t g_door_anim_frame;

extern uint8_t comic_y_checkpoint;
extern uint8_t comic_x_checkpoint;

extern ActorSystem* g_actor_system;

extern bool game_over_triggered;
extern uint8_t comic_num_lives;

extern uint8_t comic_hp;
extern uint8_t comic_hp_pending_increase;

extern uint8_t score_bytes[3];
extern uint8_t score_10000_counter;

// Helper functions
void check(bool condition, const std::string& message);
Animation make_animation(const std::vector<int>& durations, bool looping);
void reset_physics_state();
void reset_door_state();
level_t* create_test_level_with_door(uint8_t door_x, uint8_t door_y, uint8_t target_level,
                                     uint8_t target_stage);
void simulate_tick();
int measure_jump_height(uint8_t jump_power);
void advance_death_sequence_until_complete(int max_ticks = 128);

#if defined(HAVE_SDL2_MIXER)
bool init_sdl_audio();
void quit_sdl_audio();
void wait_for_sfx_channel_idle(uint32_t timeout_ms = 1000);
#endif

#endif  // TEST_HELPERS_H
