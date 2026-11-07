#include "../include/high_scores.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "../include/graphics.h"
#include "../include/pref_paths.h"
#include "../include/text_screen_utils.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr const char* HIGH_SCORES_FILENAME = "COMIC.HGH";
static constexpr const char* HIGH_SCORES_MAGIC = "CCHG1";
static constexpr int MAX_HIGH_SCORES = 10;
static constexpr int MAX_NAME_LENGTH = 15;

static const char* HIGH_SCORES_SCREEN_FILE = "sys005.ega.png";  // Hall of Fame background

// ---------------------------------------------------------------------------
// Leaderboard model + COMIC.HGH persistence
// ---------------------------------------------------------------------------

uint32_t score_bytes_to_uint32(const uint8_t score_bytes[3]) {
    return static_cast<uint32_t>(score_bytes[2]) * 10000u +
           static_cast<uint32_t>(score_bytes[1]) * 100u + static_cast<uint32_t>(score_bytes[0]);
}

static std::string get_pref_high_scores_path() {
    char* pref_path = SDL_GetPrefPath(PREF_PATH_ORG, PREF_PATH_APP);
    if (!pref_path) {
        return std::string();
    }
    std::string path = std::string(pref_path) + HIGH_SCORES_FILENAME;
    SDL_free(pref_path);
    return path;
}

static std::vector<HighScoreEntry> load_high_scores() {
    std::vector<HighScoreEntry> scores;

    const std::string path = get_pref_high_scores_path();
    if (path.empty()) {
        return scores;
    }

    std::ifstream f(path);
    if (!f.good()) {
        return scores;
    }

    std::string magic;
    std::getline(f, magic);
    if (!magic.empty() && magic.back() == '\r') {
        magic.pop_back();
    }
    if (magic != HIGH_SCORES_MAGIC) {
        std::cerr << "High scores: invalid format in " << path << std::endl;
        return scores;
    }

    std::string line;
    while (std::getline(f, line) && static_cast<int>(scores.size()) < MAX_HIGH_SCORES) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        const size_t sep = line.find(' ');
        if (sep == std::string::npos) {
            continue;
        }

        try {
            const uint32_t sc = static_cast<uint32_t>(std::stoul(line.substr(0, sep)));
            std::string nm = line.substr(sep + 1);
            if (static_cast<int>(nm.size()) > MAX_NAME_LENGTH) {
                nm.resize(static_cast<size_t>(MAX_NAME_LENGTH));
            }
            scores.push_back({nm, sc});
        } catch (...) {
            // Malformed line — skip
        }
    }

    return scores;
}

static bool save_high_scores(const std::vector<HighScoreEntry>& scores) {
    const std::string path = get_pref_high_scores_path();
    if (path.empty()) {
        std::cerr << "High scores: SDL_GetPrefPath failed" << std::endl;
        return false;
    }

    std::ofstream f(path, std::ios::trunc);
    if (!f.good()) {
        std::cerr << "High scores: could not open " << path << " for writing" << std::endl;
        return false;
    }

    f << HIGH_SCORES_MAGIC << "\n";
    for (const HighScoreEntry& entry : scores) {
        f << entry.score << " " << entry.name << "\n";
    }

    if (!f.good()) {
        std::cerr << "High scores: write error to " << path << std::endl;
        return false;
    }
    return true;
}

// Return 0-based rank where player_score belongs (scores sorted descending).
// Returns MAX_HIGH_SCORES if the score doesn't qualify for the top 10.
static int find_insertion_rank(const std::vector<HighScoreEntry>& scores, uint32_t player_score) {
    if (player_score == 0) {
        return MAX_HIGH_SCORES;
    }
    const auto it = std::find_if(scores.begin(), scores.end(),
                                 [player_score](const HighScoreEntry& score_entry) {
                                     return player_score > score_entry.score;
                                 });
    if (it != scores.end()) {
        return static_cast<int>(std::distance(scores.begin(), it));
    }
    if (static_cast<int>(scores.size()) < MAX_HIGH_SCORES) {
        return static_cast<int>(scores.size());
    }
    return MAX_HIGH_SCORES;
}

// Build a temporary scores list for display during name entry,
// with a placeholder inserted at the given rank.
static std::vector<HighScoreEntry> build_preview_list(const std::vector<HighScoreEntry>& scores,
                                                      int rank, uint32_t player_score,
                                                      const std::string& current_input) {
    std::vector<HighScoreEntry> preview = scores;
    const std::string placeholder_name = current_input.empty() ? "_" : current_input + "_";
    preview.insert(preview.begin() + std::min(rank, static_cast<int>(preview.size())),
                   HighScoreEntry{placeholder_name, player_score});
    if (static_cast<int>(preview.size()) > MAX_HIGH_SCORES) {
        preview.resize(static_cast<size_t>(MAX_HIGH_SCORES));
    }
    return preview;
}

// Build the text lines to display on the high scores screen.
// highlight_rank: 0-based index of the new entry (>= MAX_HIGH_SCORES = no highlight).
// is_entering_name: true while the player is typing their name.
// prompt_suffix: current name being typed (with cursor if is_entering_name).
static std::vector<std::string> build_high_scores_lines(const std::vector<HighScoreEntry>& scores,
                                                        int highlight_rank, bool is_entering_name,
                                                        const std::string& prompt_suffix) {
    std::vector<std::string> lines;
    lines.push_back("HALL OF FAME");
    lines.push_back("");

    if (is_entering_name) {
        lines.push_back("Enter your name: " + prompt_suffix);
        lines.push_back("");
    }

    for (int i = 0; i < MAX_HIGH_SCORES; ++i) {
        char buf[80];
        if (i < static_cast<int>(scores.size())) {
            const bool is_new = (i == highlight_rank);
            std::snprintf(buf, sizeof(buf), "%s%2d. %06u  %-15s", is_new ? ">>" : "  ", i + 1,
                          scores[i].score, scores[i].name.c_str());
        } else {
            std::snprintf(buf, sizeof(buf), "   %2d. ------  ---------------", i + 1);
        }
        lines.push_back(buf);
    }

    lines.push_back("");
    if (is_entering_name) {
        lines.push_back("Press Enter to confirm, Esc to skip");
    } else {
        lines.push_back("Press any key to continue");
    }

    return lines;
}

// ---------------------------------------------------------------------------
// High scores screen rendering
// ---------------------------------------------------------------------------

struct HighScoreRenderedLine {
    SDL_Texture* tex = nullptr;
    int w = 0;
    int h = 0;
};

struct HighScoreTextCache {
    std::vector<std::string> lines;
    std::vector<HighScoreRenderedLine> rendered;
    int max_line_width = 0;
    int total_h = 0;
    int max_w = 0;

    void clear() {
        for (HighScoreRenderedLine& line : rendered) {
            if (line.tex) {
                SDL_DestroyTexture(line.tex);
                line.tex = nullptr;
            }
        }
        rendered.clear();
        lines.clear();
        max_line_width = 0;
        total_h = 0;
        max_w = 0;
    }

    ~HighScoreTextCache() { clear(); }
};

static void build_high_score_text_cache(SDL_Renderer* renderer, TTF_Font* font,
                                        const std::vector<std::string>& lines, int max_line_width,
                                        HighScoreTextCache& cache) {
    constexpr SDL_Color COLOR_NORMAL = {200, 200, 200, 255};
    constexpr SDL_Color COLOR_TITLE = {255, 255, 0, 255};
    constexpr SDL_Color COLOR_NEW = {255, 200, 0, 255};
    constexpr int LINE_GAP = 2;

    cache.clear();
    cache.lines = lines;
    cache.max_line_width = max_line_width;
    cache.rendered.reserve(lines.size());

    for (size_t i = 0; i < lines.size(); ++i) {
        HighScoreRenderedLine rl;
        if (lines[i].empty()) {
            rl.h = TTF_FontLineSkip(font) / 2;
        } else {
            SDL_Color color = COLOR_NORMAL;
            if (i == 0) {
                color = COLOR_TITLE;
            } else if (lines[i][0] == '>') {
                color = COLOR_NEW;
            }
            SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(font, lines[i].c_str(), color,
                                                               static_cast<Uint32>(max_line_width));
            if (surf) {
                rl.tex = SDL_CreateTextureFromSurface(renderer, surf);
                rl.w = surf->w;
                rl.h = surf->h;
                SDL_FreeSurface(surf);
            }
        }
        cache.total_h += rl.h + LINE_GAP;
        cache.max_w = std::max(cache.max_w, rl.w);
        cache.rendered.push_back(rl);
    }
}

// Render one frame of the high scores screen.
// Clears the renderer, draws the background texture (if any), then overlays
// a semi-transparent block of text, and calls SDL_RenderPresent.
static void render_high_scores_frame(SDL_Renderer* renderer, SDL_Texture* bg_texture,
                                     TTF_Font* font, const std::vector<std::string>& lines,
                                     HighScoreTextCache* text_cache = nullptr) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (bg_texture) {
        const SDL_Rect dst = GraphicsSystem::compute_letterbox_rect(renderer);
        SDL_RenderCopy(renderer, bg_texture, nullptr, &dst);
    }

    if (!font) {
        SDL_RenderPresent(renderer);
        return;
    }

    int output_w = 0, output_h = 0;
    SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
    const int max_line_width = std::min(output_w - 40, 640);

    HighScoreTextCache local_cache;
    HighScoreTextCache* cache = text_cache ? text_cache : &local_cache;
    if (cache->rendered.empty() || cache->max_line_width != max_line_width ||
        cache->lines != lines) {
        build_high_score_text_cache(renderer, font, lines, max_line_width, *cache);
    }

    constexpr int LINE_GAP = 2;
    const int total_h = cache->total_h;
    const int max_w = cache->max_w;

    // Draw semi-transparent background box for readability.
    constexpr int PADDING = 12;
    int top_y = (output_h - total_h) / 2;
    if (top_y < 20) {
        top_y = 20;
    }

    SDL_Rect box = {(output_w - max_w) / 2 - PADDING, top_y - PADDING, max_w + PADDING * 2,
                    total_h + PADDING * 2};
    if (box.x < 0) box.x = 0;
    if (box.w > output_w) box.w = output_w;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Blit each text line, centered horizontally.
    int y = top_y;
    for (const HighScoreRenderedLine& rl : cache->rendered) {
        if (rl.tex) {
            SDL_Rect dst = {(output_w - rl.w) / 2, y, rl.w, rl.h};
            if (dst.x < 0) dst.x = 0;
            SDL_RenderCopy(renderer, rl.tex, nullptr, &dst);
        }
        y += rl.h + LINE_GAP;
    }

    SDL_RenderPresent(renderer);
}

struct NameCaptureResult {
    std::string name;
    bool user_quit = false;
};

// Prompt the player to type their name (up to MAX_NAME_LENGTH printable ASCII chars).
// Renders the score preview with the name being typed each frame.
// Returns the confirmed name, "-" on Esc/empty input, and flags user_quit on SDL_QUIT.
static NameCaptureResult capture_name_input(SDL_Renderer* renderer, SDL_Texture* bg_texture,
                                            TTF_Font* font,
                                            const std::vector<HighScoreEntry>& scores, int rank,
                                            uint32_t player_score) {
    NameCaptureResult result;
    std::string name;
    HighScoreTextCache text_cache;
    SDL_StartTextInput();
    SDL_Event e;
    bool done = false;

    while (!done) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                result.user_quit = true;
                done = true;
                break;
            }

            if (e.type == SDL_TEXTINPUT) {
                for (unsigned char c : std::string(e.text.text)) {
                    if (c >= 32u && c < 127u && static_cast<int>(name.size()) < MAX_NAME_LENGTH) {
                        name += static_cast<char>(c);
                    }
                }
            }

            if (e.type == SDL_KEYDOWN) {
                const SDL_Keycode key = e.key.keysym.sym;
                if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                    if (!name.empty()) {
                        done = true;
                    }
                } else if (key == SDLK_BACKSPACE && !name.empty()) {
                    name.pop_back();
                } else if (key == SDLK_ESCAPE) {
                    name = "-";
                    done = true;
                }
            }
        }

        if (done) {
            break;
        }

        const std::vector<HighScoreEntry> preview =
            build_preview_list(scores, rank, player_score, name);
        const std::string cursor = name.empty() ? "_" : name + "_";
        const std::vector<std::string> display_lines =
            build_high_scores_lines(preview, rank, true, cursor);
        render_high_scores_frame(renderer, bg_texture, font, display_lines, &text_cache);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    if (result.user_quit) {
        return result;
    }

    if (name.empty()) {
        name = "-";
    }
    result.name = name;
    return result;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool run_high_scores_screen(SDL_Renderer* renderer, GraphicsSystem* graphics,
                            const uint8_t* score_bytes) {
    // Load background texture (sys005.ega.png).
    SDL_Texture* bg_texture = nullptr;
    if (graphics) {
        const std::string path = graphics->get_asset_path(HIGH_SCORES_SCREEN_FILE);
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (surface) {
            bg_texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        } else {
            std::cerr << "High scores: could not load background " << HIGH_SCORES_SCREEN_FILE
                      << std::endl;
        }
    }

    TTF_Font* font = open_startup_notice_font();

    // Load persisted high scores.
    std::vector<HighScoreEntry> scores = load_high_scores();

    // Determine the player's score and whether it qualifies.
    const uint32_t player_score = score_bytes ? score_bytes_to_uint32(score_bytes) : 0u;
    const int rank = find_insertion_rank(scores, player_score);
    const bool qualifies = (rank < MAX_HIGH_SCORES);

    // Prompt for name if the score makes the leaderboard.
    std::string player_name;
    if (qualifies && font) {
        const NameCaptureResult capture_result =
            capture_name_input(renderer, bg_texture, font, scores, rank, player_score);

        if (capture_result.user_quit) {
            if (bg_texture) {
                SDL_DestroyTexture(bg_texture);
            }
            TTF_CloseFont(font);
            return false;
        }

        player_name = capture_result.name;

        // Insert the new entry and save.
        HighScoreEntry entry{player_name, player_score};
        scores.insert(scores.begin() + std::min(rank, static_cast<int>(scores.size())), entry);
        if (static_cast<int>(scores.size()) > MAX_HIGH_SCORES) {
            scores.resize(static_cast<size_t>(MAX_HIGH_SCORES));
        }
        if (!save_high_scores(scores)) {
            std::cerr << "High scores: failed to save " << HIGH_SCORES_FILENAME << std::endl;
        }
    }

    // Display the final leaderboard and wait for a keypress.
    const int display_rank = qualifies ? rank : -1;
    const std::vector<std::string> display_lines =
        build_high_scores_lines(scores, display_rank, false, player_name);
    HighScoreTextCache text_cache;

    SDL_Event e;
    bool user_quit = false;
    bool any_key = false;

    // Drain stale events so the player must press a fresh key.
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            user_quit = true;
        }
    }

    while (!any_key && !user_quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                user_quit = true;
                break;
            }
            if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                any_key = true;
                break;
            }
        }
        if (!any_key && !user_quit) {
            render_high_scores_frame(renderer, bg_texture, font, display_lines, &text_cache);
            SDL_Delay(16);
        }
    }

    if (bg_texture) {
        SDL_DestroyTexture(bg_texture);
    }
    if (font) {
        TTF_CloseFont(font);
    }
    return !user_quit;
}
