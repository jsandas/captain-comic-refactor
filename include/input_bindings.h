#ifndef INPUT_BINDINGS_H
#define INPUT_BINDINGS_H

#include <SDL2/SDL.h>

#include <string>

/**
 * input_bindings.h - Gameplay input bindings + KEYS.DEF persistence.
 *
 * Data-domain logic split out of the title presentation (title_sequence.cpp):
 * the bindings themselves, their defaults, the shared helpers the keyboard
 * setup menu uses to edit/validate them, and KEYS.DEF (magic "CCKB1")
 * load/save.  Presence/ordering/aggregation is a pure function of the file
 * format so it stays testable without running the presentation.
 */

struct InputBindings {
    SDL_Keycode move_left;
    SDL_Keycode move_right;
    SDL_Keycode jump;
    SDL_Keycode fire;
    SDL_Keycode open_door;
    SDL_Keycode teleport;
};

/**
 * Get the current configured gameplay input bindings.
 */
const InputBindings& get_input_bindings();

/**
 * Restore default gameplay bindings.
 */
[[maybe_unused]] void reset_input_bindings_to_defaults();

/**
 * Replace gameplay bindings with the provided mapping.
 */
void set_input_bindings(const InputBindings& bindings);

/**
 * Load gameplay bindings from KEYS.DEF.
 *
 * Returns true if bindings were loaded and applied.
 * Returns false if the file is missing or invalid.
 */
bool load_input_bindings_from_file();

/**
 * Save gameplay bindings to KEYS.DEF.
 *
 * Returns true on success, false on write failure.
 */
bool save_input_bindings_to_file();

/**
 * Test a key event against a binding, treating the left/right variants of
 * Ctrl/Alt/Shift as one DOS-style key class.
 */
bool key_matches_binding(SDL_Keycode key, SDL_Keycode binding);

// --- Helpers shared with the keyboard setup menu (title_sequence.cpp) ---

// Fold right-hand modifier keys onto their left-hand equivalents.
SDL_Keycode canonicalize_binding_key(SDL_Keycode key);

// True when key (canonicalized) is already bound to another action.
bool key_is_already_assigned(const InputBindings& bindings, SDL_Keycode key,
                             int current_action_index);

// Assign key to the action at action_index (0..5, order of struct fields).
void set_binding_for_action(InputBindings& bindings, int action_index, SDL_Keycode key);

// Field-by-field equality of two binding sets.
bool input_bindings_equal(const InputBindings& lhs, const InputBindings& rhs);

// Reject unknown keycodes and duplicate assignments; on failure fills
// error_message (when non-null) with a short reason.
bool validate_input_bindings(const InputBindings& bindings, std::string* error_message);

#endif  // INPUT_BINDINGS_H
