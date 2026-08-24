#include "test_cases.h"
#include "test_helpers.h"

#if defined(HAVE_SDL2_MIXER)
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

void test_audio_init_shutdown_idempotency() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_idempotency: SDL audio init should succeed");
    check(initialize_audio_system(), "audio_idempotency: first initialization should succeed");
    check(is_audio_system_ready(), "audio_idempotency: system should be ready after init");
    check(initialize_audio_system(), "audio_idempotency: second initialization should succeed");
    check(is_audio_system_ready(), "audio_idempotency: system should still be ready");
    shutdown_audio_system();
    check(!is_audio_system_ready(), "audio_idempotency: system should not be ready after shutdown");
    shutdown_audio_system();
    check(!is_audio_system_ready(),
          "audio_idempotency: system should remain not ready after second shutdown");
    quit_sdl_audio();
}

void test_audio_graceful_failure_when_not_initialized() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_uninitialized: SDL audio init should succeed");
    shutdown_audio_system();
    check(!is_audio_system_ready(), "audio_uninitialized: system should not be ready");
    check(!play_game_sound(GameSound::FIRE),
          "audio_uninitialized: play should fail when not initialized");
    check(!play_game_sound(GameSound::ENEMY_HIT),
          "audio_uninitialized: enemy hit sound should fail when not initialized");
    quit_sdl_audio();
}

void test_audio_priority_interrupt() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_priority_interrupt: SDL audio init should succeed");
    check(initialize_audio_system(), "audio_priority_interrupt: initialization should succeed");
    check(play_game_sound(GameSound::FIRE),
          "audio_priority_interrupt: lower priority sound should play");
    check(play_game_sound(GameSound::ENEMY_HIT),
          "audio_priority_interrupt: higher priority sound should interrupt");
    check(play_game_sound(GameSound::PLAYER_HIT),
          "audio_priority_interrupt: higher priority sound should interrupt");
    check(play_game_sound(GameSound::PLAYER_DIE),
          "audio_priority_interrupt: high priority sound should interrupt");
    shutdown_audio_system();
    quit_sdl_audio();
}

void test_audio_priority_blocking() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_priority_blocking: SDL audio init should succeed");
    check(initialize_audio_system(), "audio_priority_blocking: initialization should succeed");
    check(play_game_sound(GameSound::PLAYER_DIE),
          "audio_priority_blocking: high priority sound should play");
    bool blocked = !play_game_sound(GameSound::ENEMY_HIT);
    check(blocked,
          "audio_priority_blocking: lower priority sound should be blocked by active higher "
          "priority");
    wait_for_sfx_channel_idle(1000);
    check(play_game_sound(GameSound::ENEMY_HIT),
          "audio_priority_blocking: sound should play after previous completes");
    shutdown_audio_system();
    quit_sdl_audio();
}

void test_audio_enemy_hit_interrupts_fire() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_enemy_hit_interrupts_fire: SDL audio init should succeed");
    check(initialize_audio_system(),
          "audio_enemy_hit_interrupts_fire: initialization should succeed");
    check(play_game_sound(GameSound::FIRE),
          "audio_enemy_hit_interrupts_fire: fire should start playing");
    const Mix_Chunk* fire_chunk = Mix_GetChunk(0);
    check(fire_chunk != nullptr,
          "audio_enemy_hit_interrupts_fire: fire chunk should be available on the SFX channel");
    int mixer_frequency = 0;
    Uint16 mixer_format = 0;
    int mixer_channels = 0;
    check(Mix_QuerySpec(&mixer_frequency, &mixer_format, &mixer_channels) != 0,
          "audio_enemy_hit_interrupts_fire: Mix_QuerySpec should succeed");
    const uint32_t expected_frames_per_tick =
        static_cast<uint32_t>((static_cast<uint64_t>(mixer_frequency) * 55) / 1000);
    const uint32_t expected_frames = expected_frames_per_tick * 2;
    const uint32_t expected_bytes =
        expected_frames * static_cast<uint32_t>(mixer_channels) * sizeof(int16_t);
    check(fire_chunk->alen == expected_bytes,
          "audio_enemy_hit_interrupts_fire: FIRE chunk size should match the queried mixer spec");
    check(Mix_Playing(0) != 0,
          "audio_enemy_hit_interrupts_fire: fire should occupy the SFX channel");
    check(play_game_sound(GameSound::ENEMY_HIT),
          "audio_enemy_hit_interrupts_fire: enemy hit should interrupt fire");
    check(Mix_Playing(0) != 0,
          "audio_enemy_hit_interrupts_fire: enemy hit should still be playing on the SFX channel");
    check(!play_game_sound(GameSound::FIRE),
          "audio_enemy_hit_interrupts_fire: lower priority fire should be blocked while enemy hit "
          "plays");
    shutdown_audio_system();
    quit_sdl_audio();
}

void test_audio_all_sounds_playable() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_all_sounds: SDL audio init should succeed");
    check(initialize_audio_system(), "audio_all_sounds: initialization should succeed");
    bool ok;
    ok = play_game_sound(GameSound::GAME_OVER);
    check(ok, "audio_all_sounds: GAME_OVER should play");
    wait_for_sfx_channel_idle(2000);
    ok = play_game_sound(GameSound::STAGE_TRANSITION);
    check(ok, "audio_all_sounds: STAGE_TRANSITION should play");
    wait_for_sfx_channel_idle(2500);
    ok = play_game_sound(GameSound::ENEMY_HIT);
    check(ok, "audio_all_sounds: ENEMY_HIT should play");
    wait_for_sfx_channel_idle(500);
    ok = play_game_sound(GameSound::FIRE);
    check(ok, "audio_all_sounds: FIRE should play");
    wait_for_sfx_channel_idle(500);
    ok = play_game_sound(GameSound::DOOR_OPEN);
    check(ok, "audio_all_sounds: DOOR_OPEN should play");
    wait_for_sfx_channel_idle(1000);
    ok = play_game_sound(GameSound::ITEM_COLLECT);
    check(ok, "audio_all_sounds: ITEM_COLLECT should play");
    wait_for_sfx_channel_idle(1000);
    ok = play_game_sound(GameSound::EXTRA_LIFE);
    check(ok, "audio_all_sounds: EXTRA_LIFE should play");
    wait_for_sfx_channel_idle(1500);
    ok = play_game_sound(GameSound::TELEPORT);
    check(ok, "audio_all_sounds: TELEPORT should play");
    wait_for_sfx_channel_idle(1000);
    ok = play_game_sound(GameSound::PLAYER_HIT);
    check(ok, "audio_all_sounds: PLAYER_HIT should play");
    wait_for_sfx_channel_idle(1000);
    ok = play_game_sound(GameSound::PLAYER_DIE);
    check(ok, "audio_all_sounds: PLAYER_DIE should play");
    wait_for_sfx_channel_idle(1000);
    check(!play_game_sound(GameSound::UNUSED_0),
          "audio_all_sounds: UNUSED_0 should not play (no jump sound)");
    shutdown_audio_system();
    quit_sdl_audio();
}

void test_audio_music_playback() {
    reset_physics_state();
    check(init_sdl_audio(), "audio_music: SDL audio init should succeed");
    check(initialize_audio_system(), "audio_music: initialization should succeed");
    bool ok = play_game_music(GameMusic::TITLE);
    check(ok, "audio_music: title music should start");
    check(is_game_music_playing(), "audio_music: music should report playing");
    ok = play_game_sound(GameSound::FIRE);
    check(ok, "audio_music: SFX should play while music active");
    check(Mix_Playing(0) != 0, "audio_music: sfx channel should be active");
    stop_game_music();
    check(!is_game_music_playing(), "audio_music: music should stop");
    wait_for_sfx_channel_idle(500);
    ok = play_game_sound(GameSound::ITEM_COLLECT);
    check(ok, "audio_music: sfx should still play after music stopped");
    shutdown_audio_system();
    quit_sdl_audio();
}

#else

void test_audio_init_shutdown_idempotency() {
    reset_physics_state();
    check(!initialize_audio_system(), "audio_no_mixer: init should fail without SDL2_mixer");
    check(!is_audio_system_ready(),
          "audio_no_mixer: system should not be ready without SDL2_mixer");
}

void test_audio_graceful_failure_when_not_initialized() {
    reset_physics_state();
    check(!play_game_sound(GameSound::FIRE), "audio_no_mixer: play should fail without SDL2_mixer");
}

void test_audio_priority_interrupt() { reset_physics_state(); }

void test_audio_priority_blocking() { reset_physics_state(); }

void test_audio_enemy_hit_interrupts_fire() { reset_physics_state(); }

void test_audio_all_sounds_playable() { reset_physics_state(); }

void test_audio_music_playback() { reset_physics_state(); }

#endif
