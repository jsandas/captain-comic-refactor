#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include <cstdint>
#include <string>
#include <vector>

#include "graphics.h"
#include "physics.h"  // MAX_HP and other shared gameplay constants

/**
 * UISystem manages all HUD rendering during gameplay.
 * Displays score, lives, HP meter, fireball meter, and inventory items.
 *
 * Based on reference implementation from jsandas/comic-c (src/game_main.c, src/graphics.c)
 */
class UISystem {
public:
    UISystem();
    ~UISystem();

    // Initialize UI system and load all required sprites
    bool initialize();

    // Cleanup UI resources
    void cleanup();

    // Update animation state (call once per game tick)
    void update();

    // Render all UI elements to the screen
    void render_hud(const uint8_t score_bytes[3],  // 3 base-100 encoded digits
                    uint8_t num_lives,             // 0-5
                    uint8_t hp,                    // 0-6
                    uint8_t fireball_meter,        // 0-12
                    uint8_t firepower,             // 0-5 (shown as firepower colored blastola)
                    bool has_corkscrew, bool has_door_key, bool has_teleport_wand, bool has_lantern,
                    bool has_gems, bool has_crown, bool has_gold,
                    uint8_t jump_power  // For boots detection (> 4 means boots)
    );

    // Public testable helper functions for HUD logic
    static void score_bytes_to_digits(const uint8_t score_bytes[3], uint8_t digits[6]);
    static uint8_t fireball_meter_to_cell_state(uint8_t meter_value, uint8_t cell_index);
    static bool has_boots(uint8_t jump_power);

private:
    bool initialized;
    uint8_t inventory_animation_counter;

    // Score digit sprites (0-9)
    std::vector<Sprite*> score_digit_sprites;

    // Life icon sprites
    Sprite* life_icon_bright;
    Sprite* life_icon_dark;

    // Meter sprites (full, half, empty)
    Sprite* meter_full;
    Sprite* meter_half;
    Sprite* meter_empty;

    // Inventory item sprites
    std::vector<Sprite*> blastola_cola_sprites;  // Base even/odd frames
    std::vector<std::vector<Sprite*>>
        blastola_cola_inventory_sprites;         // Firepower 1-5, each even/odd
    std::vector<Sprite*> corkscrew_sprites;      // even/odd frames
    std::vector<Sprite*> door_key_sprites;       // even/odd frames
    std::vector<Sprite*> boots_sprites;          // even/odd frames
    std::vector<Sprite*> lantern_sprites;        // even/odd frames
    std::vector<Sprite*> teleport_wand_sprites;  // even/odd frames
    std::vector<Sprite*> gems_sprites;           // even/odd frames
    std::vector<Sprite*> crown_sprites;          // even/odd frames
    std::vector<Sprite*> gold_sprites;           // even/odd frames

    // Render individual UI components
    void render_score(const uint8_t score_bytes[3]);
    void render_lives(uint8_t num_lives);
    void render_hp_meter(uint8_t hp);
    void render_fireball_meter(uint8_t meter);
    void render_inventory(uint8_t firepower, bool has_corkscrew, bool has_door_key,
                          bool has_teleport_wand, bool has_lantern, bool has_gems, bool has_crown,
                          bool has_gold, uint8_t jump_power);

    // Helper to load sprite
    Sprite* load_ui_sprite(const std::string& sprite_name);

    // Helper to render sprite at position without direction
    void render_sprite_at(Sprite* sprite, int x, int y, int width, int height);
};

#endif  // UI_SYSTEM_H
