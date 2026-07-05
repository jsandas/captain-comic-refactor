#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>

enum class GameSound : uint8_t {
    UNUSED_0 = 0,      // Reserved (no jump sound in original game)
    FIRE,              // Fireball launch
    ITEM_COLLECT,      // Item pickup (includes treasures and power-ups)
    DOOR_OPEN,         // Door opening
    STAGE_TRANSITION,  // Stage edge crossing
    ENEMY_HIT,         // Fireball hits enemy
    PLAYER_HIT,        // Player takes damage
    PLAYER_DIE,        // Player death
    TOO_BAD,           // Short death jingle after the player loses a life
    GAME_OVER,         // Final game-over screen melody
    EXTRA_LIFE,        // Extra life award ("we're in the money")
    MATERIALIZE,       // Beam in/out and materialize effects
    TELEPORT,          // Teleport action
    COUNT
};

/**
 * Game music tracks
 *
 * These are looping background music tracks, separate from sound effects.
 * Only one music track plays at a time; playing a new track stops the previous one.
 */
enum class GameMusic : uint8_t {
    NONE = 0xFF,  // No music playing
    TITLE = 0,    // Title sequence and end-game victory music
    COUNT
};

/**
 * Initialize the audio subsystem
 *
 * Sets up SDL_mixer and synthesizes all game sound effects.
 * Call once at startup before calling play_game_sound().
 *
 * @return true if initialization successful, false on error
 */
bool initialize_audio_system();

/**
 * Shutdown the audio subsystem
 *
 * Stops any playing sounds and frees all audio resources.
 * Call once at program shutdown.
 */
void shutdown_audio_system();

/**
 * Check if audio system is ready
 *
 * @return true if initialized and ready to play sounds
 */
bool is_audio_system_ready();

/**
 * Play a game sound effect
 *
 * Plays the specified sound effect. If a lower-priority sound is
 * already playing, it will be interrupted. If a higher-priority
 * sound is playing, the new sound will be ignored.
 *
 * @param sound The game sound to play
 * @return true if sound was played, false if rejected or error
 */
bool play_game_sound(GameSound sound);

/**
 * Play a music track
 *
 * Starts looping playback of the specified music track. If music is already
 * playing, the current track is stopped and the new track begins.
 * Music plays on a separate channel from sound effects.
 *
 * @param music The game music track to play
 * @return true if music started playing, false on error
 */
bool play_game_music(GameMusic music);

/**
 * Stop music playback
 *
 * Stops the currently playing music track (if any).
 */
void stop_game_music();

/**
 * Check if music is currently playing
 *
 * @return true if music is actively playing, false otherwise
 */
bool is_game_music_playing();

/**
 * Get the currently playing music track
 *
 * @return The currently playing music track, or GameMusic::NONE if no music is playing
 */
GameMusic get_current_music();

#endif  // AUDIO_H
