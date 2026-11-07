#ifndef TEXT_SCREEN_UTILS_H
#define TEXT_SCREEN_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <vector>

/**
 * text_screen_utils.h - Shared helpers for the DOS text-mode style screens
 * (80x25 layout rendered with TTF, used by the startup notice, keyboard
 * setup menu, modal text screens, and the high scores screen).
 */

// One rendered line of text (nullptr texture = vertical spacer).
struct RenderedTextLine {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

// DOS text-mode layout within the current renderer output size.
struct DosTextLayout {
    int left = 0;
    int top = 0;
    int max_width = 0;
    int line_spacing = 0;
};

// Compute an 80x25 DOS text-mode layout centered in the renderer output.
DosTextLayout compute_dos_text_layout(SDL_Renderer* renderer, TTF_Font* font);

// Destroy all textures held by lines and clear the vector.
void destroy_text_lines(std::vector<RenderedTextLine>& lines);

// Render text lines into textures.  Empty strings become vertical spacers.
// Returns an empty vector on the first rendering failure (lines are destroyed).
std::vector<RenderedTextLine> build_text_lines(SDL_Renderer* renderer, TTF_Font* font,
                                               const std::vector<std::string>& lines,
                                               SDL_Color color, int max_line_width);

// Clear the renderer in the background color, blit lines horizontally
// centered from top_margin, and present.
void render_text_lines_centered(SDL_Renderer* renderer, const std::vector<RenderedTextLine>& lines,
                                int top_margin, int line_spacing, SDL_Color background_color);

// Open the shared monospace fallback font used by the DOS-style screens.
// Returns nullptr if no candidate font could be opened.
TTF_Font* open_startup_notice_font();

#endif  // TEXT_SCREEN_UTILS_H
