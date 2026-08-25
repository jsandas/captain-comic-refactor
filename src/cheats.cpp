#include "../include/cheats.h"

#include <cstdlib>
#include <iostream>

#include "../include/actors.h"
#include "../include/level.h"
#include "../include/level_loader.h"

namespace {

void print_cheat_menu() {
    std::cout << "[CHEAT] Debug mode enabled. Press F1-F5 for cheats:" << std::endl;
    std::cout << "  F1 - Toggle noclip (walk through walls)" << std::endl;
    std::cout << "  F2 - Level warp (choose level/stage)" << std::endl;
    std::cout << "  F3 - Toggle debug overlay" << std::endl;
    std::cout << "  F4 - Position warp (teleport to coordinates)" << std::endl;
    std::cout << "  F5 - Grant item (testing item effects)" << std::endl;
}

}  // namespace

// Global cheat system instance
CheatSystem* g_cheats = nullptr;

// External references to game state from main.cpp
extern int comic_x;
extern int comic_y;
extern int8_t comic_y_vel;
extern int8_t comic_x_momentum;
extern uint8_t comic_is_falling_or_jumping;
extern int camera_x;
extern uint8_t current_level_number;
extern uint8_t current_stage_number;
extern ActorSystem* g_actor_system;  // Actor system for item granting

// External reference to physics cheat flags
extern bool cheat_noclip;

CheatSystem::CheatSystem()
    : initialized(false),
      debug_enabled(false),
      noclip_active(false),
      debug_overlay_active(false),
      awaiting_level_input(false),
      awaiting_stage_input(false),
      target_level(-1),
      target_stage(-1),
      awaiting_x_input(false),
      awaiting_y_input(false),
      target_x(0),
      target_y(0),
      awaiting_item_input(false),
      target_item(-1) {}

CheatSystem::~CheatSystem() { cleanup(); }

bool CheatSystem::initialize(bool debug_mode) {
    if (initialized) {
        return true;
    }

    debug_enabled = debug_mode;

    if (debug_enabled) {
        print_cheat_menu();
    }

    initialized = true;
    return true;
}

void CheatSystem::cleanup() {
    if (!initialized) {
        return;
    }

    // Reset all cheat states
    noclip_active = false;
    debug_overlay_active = false;
    awaiting_level_input = false;
    awaiting_stage_input = false;
    awaiting_x_input = false;
    awaiting_y_input = false;
    awaiting_item_input = false;
    cheat_noclip = false;

    initialized = false;
}

void CheatSystem::process_input(SDL_Keycode key) {
    if (!debug_enabled) {
        // Silently ignore cheat keys when debug not enabled
        return;
    }

    // Handle multi-step input modes first
    if (awaiting_level_input || awaiting_stage_input) {
        handle_level_warp_input(key);
        return;
    }

    if (awaiting_x_input || awaiting_y_input) {
        handle_position_warp_input(key);
        return;
    }

    if (awaiting_item_input) {
        handle_item_grant_input(key);
        return;
    }

    // Handle direct cheat toggles
    switch (key) {
        case SDLK_F1:
            toggle_noclip();
            break;

        case SDLK_F2:
            activate_level_warp();
            break;

        case SDLK_F3:
            toggle_debug_overlay();
            break;

        case SDLK_F4:
            activate_position_warp();
            break;

        case SDLK_F5:
            activate_item_grant();
            break;

        default:
            // Not a cheat key
            break;
    }
}

void CheatSystem::toggle_noclip() {
    noclip_active = !noclip_active;
    cheat_noclip = noclip_active;

    std::cout << "[CHEAT] Noclip " << (noclip_active ? "enabled" : "disabled") << std::endl;
}

void CheatSystem::toggle_debug_overlay() {
    debug_overlay_active = !debug_overlay_active;
    std::cout << "[CHEAT] Debug overlay " << (debug_overlay_active ? "enabled" : "disabled")
              << std::endl;
}

void CheatSystem::activate_level_warp() {
    awaiting_level_input = true;
    awaiting_stage_input = false;
    target_level = -1;
    target_stage = -1;

    std::cout << "[CHEAT] Level warp activated. Press 0-7 to select level:" << std::endl;
    std::cout << "  0=LAKE, 1=FOREST, 2=SPACE, 3=BASE" << std::endl;
    std::cout << "  4=CAVE, 5=SHED, 6=CASTLE, 7=COMP" << std::endl;
    std::cout << "  Q to cancel" << std::endl;
}

void CheatSystem::activate_position_warp() {
    awaiting_x_input = true;
    awaiting_y_input = false;
    position_input_buffer.clear();
    target_x = 0;
    target_y = 0;

    std::cout << "[CHEAT] Position warp activated. Enter X coordinate (0-255): ";
    std::cout.flush();
}

void CheatSystem::handle_level_warp_input(SDL_Keycode key) {
    // Cancel on Q
    if (key == SDLK_q) {
        awaiting_level_input = false;
        awaiting_stage_input = false;
        std::cout << "[CHEAT] Level warp cancelled" << std::endl;
        print_cheat_menu();
        return;
    }

    if (awaiting_level_input) {
        // Accept level input (0-7)
        if (key >= SDLK_0 && key <= SDLK_7) {
            target_level = key - SDLK_0;
            awaiting_level_input = false;
            awaiting_stage_input = true;

            std::cout << "[CHEAT] Level " << static_cast<int>(target_level)
                      << " selected. Press 0-2 for stage (Q to cancel)" << std::endl;
        }
    } else if (awaiting_stage_input) {
        // Accept stage input (0-2)
        if (key >= SDLK_0 && key <= SDLK_2) {
            target_stage = key - SDLK_0;
            awaiting_stage_input = false;

            execute_level_warp();
        }
    }
}

void CheatSystem::handle_position_warp_input(SDL_Keycode key) {
    // Cancel on Q
    if (key == SDLK_q) {
        awaiting_x_input = false;
        awaiting_y_input = false;
        position_input_buffer.clear();
        std::cout << std::endl << "[CHEAT] Position warp cancelled" << std::endl;
        print_cheat_menu();
        return;
    }

    // Handle numeric input
    if (key >= SDLK_0 && key <= SDLK_9) {
        // Limit input to 3 digits (sufficient for 0-255 range)
        constexpr size_t MAX_INPUT_LENGTH = 3;
        if (position_input_buffer.length() < MAX_INPUT_LENGTH) {
            position_input_buffer += static_cast<char>('0' + (key - SDLK_0));
            std::cout << static_cast<char>('0' + (key - SDLK_0));
            std::cout.flush();
        }
        return;
    }

    // Handle backspace
    if (key == SDLK_BACKSPACE && !position_input_buffer.empty()) {
        position_input_buffer.pop_back();
        std::cout << "\b \b";
        std::cout.flush();
        return;
    }

    // Handle enter/return
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (position_input_buffer.empty()) {
            return;  // Need at least one digit
        }

        if (awaiting_x_input) {
            target_x = std::atoi(position_input_buffer.c_str());
            if (target_x < 0 || target_x > 255) {
                std::cout << std::endl
                          << "[CHEAT] Invalid X coordinate (must be 0-255)" << std::endl;
                awaiting_x_input = false;
                awaiting_y_input = false;
                position_input_buffer.clear();
                return;
            }

            awaiting_x_input = false;
            awaiting_y_input = true;
            position_input_buffer.clear();
            std::cout << std::endl << "[CHEAT] Enter Y coordinate (0-19): ";
            std::cout.flush();
        } else if (awaiting_y_input) {
            target_y = std::atoi(position_input_buffer.c_str());
            if (target_y < 0 || target_y > 19) {
                std::cout << std::endl
                          << "[CHEAT] Invalid Y coordinate (must be 0-19)" << std::endl;
                awaiting_x_input = false;
                awaiting_y_input = false;
                position_input_buffer.clear();
                return;
            }

            awaiting_y_input = false;
            position_input_buffer.clear();
            std::cout << std::endl;

            execute_position_warp();
        }
    }
}

void CheatSystem::execute_level_warp() {
    std::cout << "[CHEAT] Warping to level " << static_cast<int>(target_level) << ", stage "
              << static_cast<int>(target_stage) << std::endl;

    // Set level/stage numbers
    current_level_number = static_cast<uint8_t>(target_level);
    current_stage_number = static_cast<uint8_t>(target_stage);

    // Load the new level and stage
    // Note: load_new_level() and load_new_stage() are void functions that handle their
    // own error logging. They log warnings if assets are missing but continue gracefully.
    // If tileset loading fails, the renderer will display fallback behavior.
    load_new_level();
    load_new_stage();

    // Reset player to safe spawn position
    comic_x = 20;
    comic_y = 14;
    comic_y_vel = 0;
    comic_x_momentum = 0;
    comic_is_falling_or_jumping = 0;
    camera_x = 0;

    std::cout << "[CHEAT] Level warp complete" << std::endl;
}

// cppcheck-suppress functionConst
void CheatSystem::execute_position_warp() {
    std::cout << "[CHEAT] Warping to position (" << target_x << ", " << target_y << ")"
              << std::endl;

    // Set player position
    comic_x = target_x;
    comic_y = target_y;
    comic_y_vel = 0;
    comic_x_momentum = 0;

    // Adjust camera to keep player visible
    // Camera follows player with some margin
    const int screen_width_units = 20;  // 320 pixels / 16 pixels per unit
    const int camera_margin = 5;        // Keep player at least 5 units from edge

    if (comic_x < camera_x + camera_margin) {
        camera_x = comic_x - camera_margin;
        if (camera_x < 0) camera_x = 0;
    } else if (comic_x > camera_x + screen_width_units - camera_margin) {
        camera_x = comic_x - screen_width_units + camera_margin;
    }

    std::cout << "[CHEAT] Position warp complete" << std::endl;
}

[[maybe_unused]] const std::string& CheatSystem::get_position_input_buffer() const {
    return position_input_buffer;
}

[[maybe_unused]] std::string CheatSystem::get_level_warp_prompt() const {
    if (awaiting_level_input) {
        return "Enter level (0-7):";
    } else if (awaiting_stage_input) {
        return "Enter stage (0-2):";
    }
    return "";
}

void CheatSystem::activate_item_grant() {
    awaiting_item_input = true;
    target_item = -1;

    std::cout << "[CHEAT] Item grant activated. Select item to grant:" << std::endl;
    std::cout << "  0 = Corkscrew (fireball vertical oscillation)" << std::endl;
    std::cout << "  1 = Door Key (unlock doors)" << std::endl;
    std::cout << "  2 = Boots (increased jump power)" << std::endl;
    std::cout << "  3 = Lantern (castle lighting)" << std::endl;
    std::cout << "  4 = Teleport Wand (special teleport)" << std::endl;
    std::cout << "  5 = Gems (treasure 1/3)" << std::endl;
    std::cout << "  6 = Crown (treasure 2/3)" << std::endl;
    std::cout << "  7 = Gold (treasure 3/3)" << std::endl;
    std::cout << "  8 = Blastola Cola (increase firepower)" << std::endl;
    std::cout << "  9 = Shield (HP refill)" << std::endl;
    std::cout << "  Q to cancel" << std::endl;
}

void CheatSystem::handle_item_grant_input(SDL_Keycode key) {
    // Cancel on Q
    if (key == SDLK_q) {
        awaiting_item_input = false;
        std::cout << "[CHEAT] Item grant cancelled" << std::endl;
        print_cheat_menu();
        return;
    }

    // Map numeric keys to item types
    // 0-8 map directly to ITEM constants
    // 9 maps to Shield (item type 14)
    if (key >= SDLK_0 && key <= SDLK_9) {
        int selection = key - SDLK_0;

        if (selection <= 8) {
            target_item = selection;  // Direct mapping for 0-8
        } else if (selection == 9) {
            target_item = ITEM_SHIELD;  // Shield is item type 14
        } else {
            std::cout << "[CHEAT] Invalid item selection" << std::endl;
            awaiting_item_input = false;
            return;
        }

        awaiting_item_input = false;
        execute_item_grant();
    }
}

void CheatSystem::execute_item_grant() {
    if (!g_actor_system) {
        std::cout << "[CHEAT] Error: Actor system not available" << std::endl;
        return;
    }

    // Item names for display
    static const char* item_names[] = {
        "Corkscrew",                                          // 0
        "Door Key",                                           // 1
        "Boots",                                              // 2
        "Lantern",                                            // 3
        "Teleport Wand",                                      // 4
        "Gems",                                               // 5
        "Crown",                                              // 6
        "Gold",                                               // 7
        "Blastola Cola",                                      // 8
        nullptr,         nullptr, nullptr, nullptr, nullptr,  // 9-13 unused
        "Shield"                                              // 14
    };

    const char* item_name = "Unknown";
    if (target_item >= 0 && target_item < 15 && item_names[target_item]) {
        item_name = item_names[target_item];
    }

    std::cout << "[CHEAT] Granting item: " << item_name << " (type "
              << static_cast<int>(target_item) << ")" << std::endl;

    // Apply the item effect
    g_actor_system->apply_item_effect(static_cast<uint8_t>(target_item));

    std::cout << "[CHEAT] Item granted successfully" << std::endl;
}
