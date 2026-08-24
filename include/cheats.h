#ifndef CHEATS_H
#define CHEATS_H

#include <SDL2/SDL.h>

#include <cstdint>
#include <string>

/**
 * CheatSystem - Manages debug cheats and development tools
 *
 * Provides noclip, level warping, position warping, and debug overlay.
 * Activated by --debug command-line flag. All cheats use F-keys to avoid
 * conflicts with gameplay controls.
 */
class CheatSystem {
public:
    CheatSystem();
    ~CheatSystem();

    // Initialize with debug mode enabled or disabled
    bool initialize(bool debug_mode);

    // Process keyboard input for cheat activation
    void process_input(SDL_Keycode key);

    // State queries
    bool is_debug_enabled() const { return debug_enabled; }
    bool is_noclip_active() const { return noclip_active; }
    bool should_show_debug_overlay() const { return debug_overlay_active; }
    bool is_awaiting_level_input() const { return awaiting_level_input; }
    bool is_awaiting_stage_input() const { return awaiting_stage_input; }
    bool is_awaiting_position_input() const { return awaiting_x_input || awaiting_y_input; }
    bool is_awaiting_item_input() const { return awaiting_item_input; }

    // Get current position input buffer for display
    const std::string& get_position_input_buffer() const;
    std::string get_level_warp_prompt() const;

private:
    void cleanup();

    // Toggle methods
    void toggle_noclip();
    void toggle_debug_overlay();
    void activate_level_warp();
    void activate_position_warp();
    void activate_item_grant();

    // Input handling for multi-step cheats
    void handle_level_warp_input(SDL_Keycode key);
    void handle_position_warp_input(SDL_Keycode key);
    void handle_item_grant_input(SDL_Keycode key);

    // Execute warps
    void execute_level_warp();
    void execute_position_warp();
    void execute_item_grant();

    // State
    bool initialized;
    bool debug_enabled;

    // Cheat toggles
    bool noclip_active;
    bool debug_overlay_active;

    // Level warp state
    bool awaiting_level_input;
    bool awaiting_stage_input;
    int8_t target_level;
    int8_t target_stage;

    // Position warp state
    bool awaiting_x_input;
    bool awaiting_y_input;
    std::string position_input_buffer;
    int16_t target_x;
    int16_t target_y;

    // Item grant state
    bool awaiting_item_input;
    int8_t target_item;
};

// Global cheat system instance
extern CheatSystem* g_cheats;

#endif  // CHEATS_H
