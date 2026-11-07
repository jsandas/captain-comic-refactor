#ifndef HIGH_SCORES_H
#define HIGH_SCORES_H

#include <cstdint>
#include <string>
#include <vector>

// Forward declaration
class GraphicsSystem;
struct SDL_Renderer;

/**
 * high_scores.h - Hall-of-Fame leaderboard + COMIC.HGH persistence.
 *
 * Data-domain logic split out of the title presentation (title_sequence.cpp):
 * leaderboard top-10 model, score encoding, COMIC.HGH (magic "CCHG1")
 * load/save, and the on-screen leaderboard + name capture.
 */

struct HighScoreEntry {
    std::string name;
    uint32_t score = 0;
};

/**
 * Convert a 3-byte base-100 score encoding to a plain decimal uint32_t.
 *
 * score_bytes[0] is the rightmost decimal-digit pair (0–99),
 * score_bytes[1] is the middle pair, score_bytes[2] is the leftmost pair.
 * Result = bytes[2]*10000 + bytes[1]*100 + bytes[0].   Maximum: 999 999.
 */
uint32_t score_bytes_to_uint32(const uint8_t score_bytes[3]);

/**
 * Display the high scores screen (sys005.ega.png) with a live leaderboard.
 *
 * If score_bytes is non-null and the score qualifies for the top 10,
 * prompts the player to enter their name before revealing the final table.
 * The leaderboard is persisted to COMIC.HGH in the user preference directory.
 *
 * @param renderer    SDL renderer
 * @param graphics    Graphics system (for asset path resolution); may be nullptr
 * @param score_bytes 3-byte base-100 score array, or nullptr to show existing scores only
 * @return true to continue, false if the user closed the window (SDL_QUIT)
 */
bool run_high_scores_screen(SDL_Renderer* renderer, GraphicsSystem* graphics,
                            const uint8_t* score_bytes);

#endif  // HIGH_SCORES_H
