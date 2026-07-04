#include "test_helpers.h"

#include <cstring>

#if defined(HAVE_SDL2_MIXER)
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

// Global variables
int failures = 0;
int comic_x = 0;
int comic_y = 0;
int8_t comic_y_vel = 0;
int8_t comic_x_momentum = 0;
uint8_t comic_facing = 1;
uint8_t comic_animation = 0;
uint8_t comic_is_falling_or_jumping = 0;
uint8_t comic_jump_counter = 0;
uint8_t comic_jump_power = JUMP_POWER_DEFAULT;
uint8_t key_state_jump = 0;
uint8_t previous_key_state_jump = 0;
uint8_t key_state_left = 0;
uint8_t key_state_right = 0;
uint8_t key_state_open = 0;
int camera_x = 0;

uint8_t comic_has_door_key = 0;
uint8_t current_level_number = 1;
uint8_t current_stage_number = 0;
const level_t* current_level_ptr = nullptr;
int8_t source_door_level_number = -1;
int8_t source_door_stage_number = -1;

// Door animation globals are provided by doors.cpp via extern declarations in test_helpers.h

uint8_t comic_y_checkpoint = 12;
uint8_t comic_x_checkpoint = 14;

ActorSystem* g_actor_system = nullptr;

bool game_over_triggered = false;
uint8_t comic_num_lives = 0;

uint8_t comic_hp = MAX_HP;
uint8_t comic_hp_pending_increase = 0;

uint8_t score_bytes[3] = {0, 0, 0};
uint8_t score_10000_counter = 0;

// Helper implementations
void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        failures++;
    }
}

Animation make_animation(const std::vector<int>& durations, bool looping) {
    Animation anim;
    anim.looping = looping;
    anim.frame_start_time = 0;
    anim.current_frame = 0;
    anim.frames.clear();
    for (int duration : durations) {
        AnimationFrame frame;
        frame.duration_ms = duration;
        anim.frames.push_back(frame);
    }
    return anim;
}

void reset_physics_state() {
    init_test_level();
    comic_x = 4;
    comic_y = 14;
    comic_y_vel = 0;
    comic_x_momentum = 0;
    comic_facing = 1;  // RIGHT
    comic_animation = 0;
    comic_is_falling_or_jumping = 0;
    comic_jump_power = JUMP_POWER_DEFAULT;
    comic_jump_counter = comic_jump_power;
    key_state_jump = 0;
    previous_key_state_jump = 0;
    key_state_left = 0;
    key_state_right = 0;
    key_state_open = 0;
    camera_x = 0;

    // Reset door/level state too
    comic_has_door_key = 0;
    current_level_number = 1;
    current_stage_number = 0;
    current_level_ptr = nullptr;
    source_door_level_number = -1;
    source_door_stage_number = -1;

    game_over_triggered = false;
    comic_num_lives = 3;
    comic_hp = MAX_HP;
    comic_hp_pending_increase = 0;

    score_bytes[0] = 0;
    score_bytes[1] = 0;
    score_bytes[2] = 0;
    score_10000_counter = 0;
}

void reset_door_state() {
    comic_has_door_key = 0;
    key_state_open = 0;
    current_level_number = 1;
    current_stage_number = 0;
    current_level_ptr = nullptr;
    source_door_level_number = -1;
    source_door_stage_number = -1;
    g_door_anim_phase = DoorAnimationPhase::NONE;
    g_door_anim_frame = 0;
    g_skip_load_on_door = true;
}

level_t* create_test_level_with_door(uint8_t door_x, uint8_t door_y, uint8_t target_level,
                                     uint8_t target_stage) {
    static level_t test_level;
    std::memset(&test_level, 0, sizeof(test_level));
    test_level.stages[0].doors[0].x = door_x;
    test_level.stages[0].doors[0].y = door_y;
    test_level.stages[0].doors[0].target_level = target_level;
    test_level.stages[0].doors[0].target_stage = target_stage;
    test_level.stages[0].doors[1].x = DOOR_UNUSED;
    test_level.stages[0].doors[1].y = DOOR_UNUSED;
    test_level.stages[0].doors[2].x = DOOR_UNUSED;
    test_level.stages[0].doors[2].y = DOOR_UNUSED;
    test_level.door_tile_ul = 0x10;
    test_level.door_tile_ur = 0x11;
    test_level.door_tile_ll = 0x12;
    test_level.door_tile_lr = 0x13;
    return &test_level;
}

void simulate_tick() {
    process_jump_input();
    handle_fall_or_jump();
}

int measure_jump_height(uint8_t jump_power) {
    reset_physics_state();
    comic_jump_power = jump_power;
    comic_jump_counter = comic_jump_power;
    int start_y = comic_y;
    int min_y = comic_y;
    key_state_jump = 1;
    simulate_tick();
    for (int tick = 0; tick < 200; ++tick) {
        simulate_tick();
        min_y = std::min(min_y, comic_y);
        if (comic_is_falling_or_jumping == 0 && tick > 0) {
            break;
        }
    }
    return start_y - min_y;
}

void advance_death_sequence_until_complete(int max_ticks) {
    for (int i = 0; i < max_ticks && is_player_dying(); ++i) {
        update_player_death_sequence();
    }
}

#if defined(HAVE_SDL2_MIXER)
bool init_sdl_audio() {
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL_Init AUDIO failed: " << SDL_GetError() << std::endl;
            return false;
        }
    }
    return true;
}

void quit_sdl_audio() {
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void wait_for_sfx_channel_idle(uint32_t timeout_ms) {
    uint32_t start = SDL_GetTicks();
    while (Mix_Playing(0) && (SDL_GetTicks() - start) < timeout_ms) {
        SDL_Delay(5);
    }
}
#endif
