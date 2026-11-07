#ifndef TITLE_SEQUENCE_H
#define TITLE_SEQUENCE_H

#include <SDL2/SDL.h>

/**
 * Title Sequence presentation module.
 *
 * Replicates the DOS title flow (SYS000-SYS004 screens, palette fades, music)
 * plus the startup notice/keyboard-setup menus.  Gameplay data domains are
 * split into sibling modules:
 *
 *   - include/input_bindings.h: gameplay key bindings + KEYS.DEF persistence
 *   - include/high_scores.h:    Hall of Fame leaderboard + COMIC.HGH persistence
 */

// Forward declaration
class GraphicsSystem;

/**
 * Run the startup notice/menu shown before the title sequence.
 *
 * This replicates the original startup text-mode menu behavior:
 *   - K: keyboard setup
 *   - J: joystick calibration (placeholder screen for now)
 *   - R: registration info (placeholder screen for now)
 *   - ESC: quit
 *   - Any other key: continue to title sequence
 *
 * Returns false if the user chooses to quit or closes the window.
 */
bool run_startup_notice(SDL_Renderer* renderer, GraphicsSystem* graphics);

/**
 * Title Sequence System
 *
 * Replicates the original DOS title_sequence() from game_main.c:
 *   1. Display title screen (SYS000.EGA) with fade-in + title music
 *   2. Display story screen (SYS001.EGA) with fade-in, wait for key
 *   3. Load game UI background (SYS003.EGA) into HUD texture
 *   4. Display items screen (SYS004.EGA), wait for key
 *   5. Stop music, transition to gameplay
 *
 * Palette fade effects:
 *   The original DOS code calls palette_darken() then palette_fade_in()
 *   to produce a smooth transition from black when each screen appears.
 *   In SDL2 this is replicated by performing a multi-step color/palette
 *   transition on the rendered content, emulating the original palette
 *   register fades rather than using a simple full-screen alpha overlay.
 * Timing:
 *   Original uses wait_n_ticks(14) ≈ 770 ms for the title screen delay.
 *   Each tick is ~55 ms (DOS 18.2 Hz timer).
 */

/**
 * Run the full title sequence.
 *
 * Blocks until the sequence completes (key presses advance screens).
 * Returns true if the sequence completed normally, false if the user
 * quit (SDL_QUIT event) during the sequence.
 *
 * @param renderer  The SDL renderer for drawing
 * @param graphics  The graphics system (for asset path resolution and texture loading)
 * @return true to continue to gameplay, false to exit the program
 */
bool run_title_sequence(SDL_Renderer* renderer, GraphicsSystem* graphics);

/**
 * Get the HUD background texture created during the title sequence.
 *
 * The title sequence loads SYS003.EGA as the in-game UI overlay.
 * This texture is kept alive for use by the game loop to render the HUD.
 * Returns nullptr if the title sequence has not been run or loading failed.
 *
 * Ownership: The caller must NOT destroy the returned texture; it is
 * managed by the title sequence module and freed at shutdown.
 */
SDL_Texture* get_hud_texture();

/**
 * Clean up title sequence resources.
 *
 * Destroys the HUD texture and any other retained resources.
 * Call once at program shutdown.
 */
void cleanup_title_sequence();

#endif  // TITLE_SEQUENCE_H
