#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include <cstdint>
/**
 * game_constants.h - Shared gameplay limits
 *
 * Constants here are cross-module gameplay tuning values (HP, lives, ...).
 * They live apart from physics.h's movement physics so headers that need a
 * gameplay limit do not have to pull in the physics pipeline
 * (e.g. ui_system.h needs MAX_HP, not physics).
 */

// --- Player resources ---
constexpr uint8_t MAX_HP = 6;         // Maximum health points (HP meter cells)
constexpr uint8_t MAX_NUM_LIVES = 5;  // Largest life count worth awarding (bonus at max)

#endif  // GAME_CONSTANTS_H
