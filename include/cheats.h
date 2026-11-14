#ifndef CHEATS_H
#define CHEATS_H

#include <cstdint>
#include <string>

/**
 * CheatKey - SDL-independent key set for cheat input.
 *
 * cheats.h deliberately does not include SDL: cheat logic must stay testable
 * without a video backend. The platform layer (main.cpp SDL event loop)
 * translates real SDL_Keycode values into this enum before calling
 * CheatSystem::process_input().
 */
enum class CheatKey : uint8_t {
    None,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Q,
    Backspace,
    Return,
    KpEnter,
};

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
    void process_input(CheatKey key);

    // State queries
    [[maybe_unused]] bool is_debug_enabled() const { return debug_enabled; }
    [[maybe_unused]] bool is_noclip_active() const { return noclip_active; }
    [[maybe_unused]] bool should_show_debug_overlay() const { return debug_overlay_active; }
    [[maybe_unused]] bool is_awaiting_level_input() const { return awaiting_level_input; }
    [[maybe_unused]] bool is_awaiting_stage_input() const { return awaiting_stage_input; }
    [[maybe_unused]] bool is_awaiting_position_input() const {
        return awaiting_x_input || awaiting_y_input;
    }
    [[maybe_unused]] bool is_awaiting_item_input() const { return awaiting_item_input; }

    // Get current position input buffer for display
    [[maybe_unused]] const std::string& get_position_input_buffer() const;
    [[maybe_unused]] std::string get_level_warp_prompt() const;

private:
    void cleanup();

    // Toggle methods
    void toggle_noclip();
    void toggle_debug_overlay();
    static void toggle_smooth_rendering();
    void activate_level_warp();
    void activate_position_warp();
    void activate_item_grant();

    // Input handling for multi-step cheats
    void handle_level_warp_input(CheatKey key);
    void handle_position_warp_input(CheatKey key);
    void handle_item_grant_input(CheatKey key);

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

// Cheat flags. cheat_noclip is the raw fast-read flag physics checks to bypass
// collision detection when noclip is active; it lives here with the other
// cheats (not in physics.h) because it is cheat state, not physics tuning.
extern bool cheat_noclip;  // When true, disables collision detection

// Global cheat system instance
extern CheatSystem* g_cheats;

#endif  // CHEATS_H
