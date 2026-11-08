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
#include "../include/game_state.h"
#include "../include/graphics.h"
#include "../include/high_scores.h"
#include "../include/level.h"
#include "../include/level_loader.h"
#include "../include/physics.h"
#include "../include/player_teleport.h"
#include "../include/title_sequence.h"
#include "../include/ui_system.h"
#include "test_cases.h"

// Shared game state is declared in game_state.h and defined in src/game_state.cpp
// (part of comic_core); the tests link against the real definitions instead of
// redefining them.

// Test-only globals defined in test_helpers.cpp
extern int failures;

// Door animation globals are declared in doors.h (no local re-declaration needed).

// Helper functions
void check(bool condition, const std::string& message);
Animation make_animation(const std::vector<int>& durations, bool looping);
void reset_physics_state();
void reset_item_flags();  // Zero player inventory globals (comic_has_*/comic_num_treasures/...)
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
