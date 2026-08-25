#include "../include/title_sequence.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../include/audio.h"
#include "../include/graphics.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Fade-in timing for palette-step transitions (original: wait_n_ticks(1) per step)
constexpr int FADE_STEP_DELAY_MS = 55;

// Title screen display time: wait_n_ticks(14) at ~55 ms/tick ≈ 770 ms
constexpr int TITLE_DISPLAY_MS = 770;

// Asset filenames (pre-converted PNGs from EGA originals)
static const char* TITLE_SCREEN_FILE = "sys000.ega.png";        // Title image
static const char* STORY_SCREEN_FILE = "sys001.ega.png";        // Story image
static const char* GAME_UI_FILE = "sys003.ega.png";             // In-game HUD background
static const char* ITEMS_SCREEN_FILE = "sys004.ega.png";        // Items/controls screen
static const char* HIGH_SCORES_SCREEN_FILE = "sys005.ega.png";  // Hall of Fame background

// High scores constants
static constexpr const char* HIGH_SCORES_FILENAME = "COMIC.HGH";
static constexpr const char* HIGH_SCORES_MAGIC = "CCHG1";
static constexpr int MAX_HIGH_SCORES = 10;
static constexpr int MAX_NAME_LENGTH = 15;

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

// HUD texture retained for gameplay rendering
static SDL_Texture* s_hud_texture = nullptr;

static const InputBindings DEFAULT_INPUT_BINDINGS = {SDLK_LEFT,  SDLK_RIGHT, SDLK_SPACE,
                                                     SDLK_LCTRL, SDLK_LALT,  SDLK_t};

static InputBindings s_input_bindings = DEFAULT_INPUT_BINDINGS;

static constexpr const char* KEY_BINDINGS_FILENAME = "KEYS.DEF";
static constexpr const char* KEY_BINDINGS_MAGIC = "CCKB1";
static constexpr const char* PREF_PATH_ORG = "jsandas";
static constexpr const char* PREF_PATH_APP = "comic-modernization";

// Paletted surfaces for title sequence screens (kept as-is, not converted to RGBA)
static SDL_Surface* s_title_surface = nullptr;
static SDL_Surface* s_story_surface = nullptr;
static SDL_Surface* s_items_surface = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Load a fullscreen EGA image (320x200 PNG) as a paletted SDL_Surface.
 * Preserves the indexed color palette from the original PNG file.
 * Returns nullptr on failure.
 */
static SDL_Surface* load_fullscreen_paletted_surface(GraphicsSystem* graphics,
                                                     const char* filename) {
    std::string path = graphics->get_asset_path(filename);

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Title sequence: failed to load " << filename << " (" << IMG_GetError() << ")"
                  << std::endl;
        return nullptr;
    }

    // If the surface has a palette, keep it as-is.
    // If not, this indicates an issue with the PNG format conversion.
    if (!surface->format->palette) {
        std::cerr << "Title sequence: " << filename
                  << " is not paletted (expected 8-bit indexed color)" << std::endl;
    }

    return surface;
}

/**
 * Create a texture from a paletted surface with optional palette modifications.
 */
static SDL_Texture* surface_to_texture(SDL_Renderer* renderer, SDL_Surface* surface) {
    // Convert paletted surface to RGBA for rendering
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!converted) {
        std::cerr << "Title sequence: failed to convert surface format: " << SDL_GetError()
                  << std::endl;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, converted);
    SDL_FreeSurface(converted);

    if (!texture) {
        std::cerr << "Title sequence: failed to create texture: " << SDL_GetError() << std::endl;
    }

    return texture;
}

// Note: Letterbox rect calculation now centralized in GraphicsSystem::compute_letterbox_rect()

/**
 * Display a paletted surface with the palette fade-in sequence from the original.
 *
 * Replicates palette_fade_in() from the DOS code with six rendered steps:
 *   Step 1: All three entries (2,10,12) → dark gray (0x18)
 *   Step 2: All three → light gray (0x07)
 *   Step 3: Background stays gray, items/title → white (0x1f)
 *   Step 4: Background→green (0x02), items→bright green (0x1a), title stays white
 *   Step 5: Title → bright red (0x1c), background/items restored
 *   Step 6: Restore all three entries to original colors and render final frame
 *
 * Each rendered step waits ~55ms (original: wait_n_ticks(1)).
 *
 * Returns false ONLY if SDL_QUIT was received. Format/palette errors are treated
 * as non-fatal (logs warning, skips fade effect, returns true to continue).
 */
static bool fade_in_paletted_surface(SDL_Renderer* renderer, SDL_Surface* surface,
                                     const char* filename) {
    if (!surface || !surface->format || !surface->format->palette) {
        std::cerr << "Warning: " << filename << ": surface is not paletted, skipping fade effect"
                  << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    SDL_Palette* pal = surface->format->palette;
    SDL_Rect dst = GraphicsSystem::compute_letterbox_rect(renderer);

    // Palette entry indices (from original DOS code)
    constexpr int PALETTE_REG_BACKGROUND = 2;
    constexpr int PALETTE_REG_ITEMS = 10;
    constexpr int PALETTE_REG_TITLE = 12;

    constexpr int REQUIRED_PALETTE_SIZE = PALETTE_REG_TITLE + 1;
    if (pal->ncolors < REQUIRED_PALETTE_SIZE) {
        std::cerr << "Warning: " << filename << ": palette has only " << pal->ncolors
                  << " colors (need at least " << REQUIRED_PALETTE_SIZE << "), skipping fade effect"
                  << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    // Store original palette colors for restoration.
    const SDL_Color orig_colors[3] = {surface->format->palette->colors[PALETTE_REG_BACKGROUND],
                                      surface->format->palette->colors[PALETTE_REG_ITEMS],
                                      surface->format->palette->colors[PALETTE_REG_TITLE]};

    // Helper: Convert 6-bit EGA color to 8-bit RGB
    auto ega_to_rgb = [](uint8_t ega_6bit) -> uint8_t { return (ega_6bit << 2) | (ega_6bit >> 4); };

    // Define palette values in 6-bit EGA format, converted to 8-bit
    constexpr uint8_t DARK_GRAY_6BIT = 0x18;     // ~39% gray
    constexpr uint8_t LIGHT_GRAY_6BIT = 0x07;    // ~11% gray
    constexpr uint8_t WHITE_6BIT = 0x1f;         // 100% white
    constexpr uint8_t GREEN_6BIT = 0x02;         // dark green
    constexpr uint8_t BRIGHT_GREEN_6BIT = 0x1a;  // 110010 in binary
    constexpr uint8_t BRIGHT_RED_6BIT = 0x1c;    // 011100 in binary

    const uint8_t dark_gray = ega_to_rgb(DARK_GRAY_6BIT);
    const uint8_t light_gray = ega_to_rgb(LIGHT_GRAY_6BIT);
    const uint8_t white = ega_to_rgb(WHITE_6BIT);
    const uint8_t green = ega_to_rgb(GREEN_6BIT);
    const uint8_t bright_green = ega_to_rgb(BRIGHT_GREEN_6BIT);
    const uint8_t bright_red = ega_to_rgb(BRIGHT_RED_6BIT);

    SDL_Event e;
    // Step 1: All three to dark gray
    if (PALETTE_REG_BACKGROUND < pal->ncolors) {
        pal->colors[PALETTE_REG_BACKGROUND] = {dark_gray, dark_gray, dark_gray, 255};
    }
    if (PALETTE_REG_ITEMS < pal->ncolors) {
        pal->colors[PALETTE_REG_ITEMS] = {dark_gray, dark_gray, dark_gray, 255};
    }
    if (PALETTE_REG_TITLE < pal->ncolors) {
        pal->colors[PALETTE_REG_TITLE] = {dark_gray, dark_gray, dark_gray, 255};
    }

    SDL_Texture* tex = surface_to_texture(renderer, surface);
    if (!tex) {
        std::cerr << "Warning: " << filename
                  << ": texture creation failed at step 1, skipping fade effect" << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    // Render step 1
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);
    }

    // Process events during wait
    auto wait_with_events = [&](int ms) -> bool {
        uint32_t start = SDL_GetTicks();
        while (SDL_GetTicks() - start < static_cast<uint32_t>(ms)) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) return false;
            }
            SDL_Delay(16);
        }
        return true;
    };

    // Wait after step 1
    if (!wait_with_events(FADE_STEP_DELAY_MS)) return false;

    // Step 2: All three to light gray
    if (PALETTE_REG_BACKGROUND < pal->ncolors) {
        pal->colors[PALETTE_REG_BACKGROUND] = {light_gray, light_gray, light_gray, 255};
    }
    if (PALETTE_REG_ITEMS < pal->ncolors) {
        pal->colors[PALETTE_REG_ITEMS] = {light_gray, light_gray, light_gray, 255};
    }
    if (PALETTE_REG_TITLE < pal->ncolors) {
        pal->colors[PALETTE_REG_TITLE] = {light_gray, light_gray, light_gray, 255};
    }

    tex = surface_to_texture(renderer, surface);
    if (!tex) {
        std::cerr << "Warning: " << filename
                  << ": texture creation failed at step 2, skipping fade effect" << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);
    }

    if (!wait_with_events(FADE_STEP_DELAY_MS)) return false;

    // Step 3: Background stays light gray, items and title go white
    if (PALETTE_REG_BACKGROUND < pal->ncolors) {
        pal->colors[PALETTE_REG_BACKGROUND] = {light_gray, light_gray, light_gray, 255};
    }
    if (PALETTE_REG_ITEMS < pal->ncolors) {
        pal->colors[PALETTE_REG_ITEMS] = {white, white, white, 255};
    }
    if (PALETTE_REG_TITLE < pal->ncolors) {
        pal->colors[PALETTE_REG_TITLE] = {white, white, white, 255};
    }

    tex = surface_to_texture(renderer, surface);
    if (!tex) {
        std::cerr << "Warning: " << filename
                  << ": texture creation failed at step 3, skipping fade effect" << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);
    }

    if (!wait_with_events(FADE_STEP_DELAY_MS)) return false;

    // Step 4: Background → green, items → bright green, title stays white
    if (PALETTE_REG_BACKGROUND < pal->ncolors) {
        pal->colors[PALETTE_REG_BACKGROUND] = {0, green, 0, 255};
    }
    if (PALETTE_REG_ITEMS < pal->ncolors) {
        pal->colors[PALETTE_REG_ITEMS] = {0, bright_green, 0, 255};
    }
    if (PALETTE_REG_TITLE < pal->ncolors) {
        pal->colors[PALETTE_REG_TITLE] = {white, white, white, 255};
    }

    tex = surface_to_texture(renderer, surface);
    if (!tex) {
        std::cerr << "Warning: " << filename
                  << ": texture creation failed at step 4, skipping fade effect" << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);
    }

    if (!wait_with_events(FADE_STEP_DELAY_MS)) return false;

    // Step 5: Title goes bright red, restore to original colors
    if (PALETTE_REG_BACKGROUND < pal->ncolors) {
        pal->colors[PALETTE_REG_BACKGROUND] = orig_colors[0];
    }
    if (PALETTE_REG_ITEMS < pal->ncolors) {
        pal->colors[PALETTE_REG_ITEMS] = orig_colors[1];
    }
    if (PALETTE_REG_TITLE < pal->ncolors) {
        pal->colors[PALETTE_REG_TITLE] = {bright_red, 0, 0, 255};
    }

    tex = surface_to_texture(renderer, surface);
    if (!tex) {
        std::cerr << "Warning: " << filename
                  << ": texture creation failed at step 5, skipping fade effect" << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);
    }

    if (!wait_with_events(FADE_STEP_DELAY_MS)) return false;

    // Step 6: Restore all to original colors
    if (PALETTE_REG_BACKGROUND < pal->ncolors) {
        pal->colors[PALETTE_REG_BACKGROUND] = orig_colors[0];
    }
    if (PALETTE_REG_ITEMS < pal->ncolors) {
        pal->colors[PALETTE_REG_ITEMS] = orig_colors[1];
    }
    if (PALETTE_REG_TITLE < pal->ncolors) {
        pal->colors[PALETTE_REG_TITLE] = orig_colors[2];
    }

    tex = surface_to_texture(renderer, surface);
    if (!tex) {
        std::cerr << "Warning: " << filename
                  << ": texture creation failed at step 6, skipping fade effect" << std::endl;
        return true;  // Non-fatal: skip fade, continue sequence
    }

    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);
    }

    return true;
}

/**
 * Display a texture (no fade) and render one frame.
 */
static void show_texture(SDL_Renderer* renderer, SDL_Texture* texture) {
    SDL_Rect dst = GraphicsSystem::compute_letterbox_rect(renderer);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_RenderPresent(renderer);
}

/**
 * Wait for a fixed number of milliseconds while processing events.
 * Returns false if SDL_QUIT received.
 */
static bool wait_ms(int duration_ms) {
    uint32_t start = SDL_GetTicks();
    SDL_Event e;
    while (SDL_GetTicks() - start < static_cast<uint32_t>(duration_ms)) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return false;
        }
        SDL_Delay(16);
    }
    return true;
}

/**
 * Wait until any key is pressed (blocking).
 * Returns false if SDL_QUIT received.
 */
static bool wait_for_keypress(SDL_Renderer* renderer, SDL_Texture* texture) {
    SDL_Rect dst = GraphicsSystem::compute_letterbox_rect(renderer);
    SDL_Event e;

    while (true) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return false;
            if (e.type == SDL_KEYDOWN) return true;
        }

        // Keep presenting the current screen while waiting
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }
}

struct RenderedTextLine {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

struct DosTextLayout {
    int left = 0;
    int top = 0;
    int max_width = 0;
    int line_spacing = 0;
};

static DosTextLayout compute_dos_text_layout(SDL_Renderer* renderer, TTF_Font* font) {
    int output_w = 0;
    int output_h = 0;
    SDL_GetRendererOutputSize(renderer, &output_w, &output_h);

    int char_w = 8;
    int char_h = 16;
    if (font) {
        TTF_SizeText(font, "M", &char_w, &char_h);
    }

    constexpr int DOS_COLS = 80;
    constexpr int DOS_ROWS = 25;

    DosTextLayout layout;
    layout.max_width = std::min(output_w - 20, char_w * DOS_COLS);
    if (layout.max_width < 120) {
        layout.max_width = std::max(120, output_w - 20);
    }

    layout.left = (output_w - layout.max_width) / 2;
    if (layout.left < 0) {
        layout.left = 0;
    }

    layout.line_spacing = std::max(0, char_h / 5);

    const int row_height = char_h + layout.line_spacing;
    const int text_mode_height = row_height * DOS_ROWS;
    layout.top = (output_h - text_mode_height) / 2;
    if (layout.top < 10) {
        layout.top = 10;
    }

    return layout;
}

static void destroy_text_lines(std::vector<RenderedTextLine>& lines) {
    for (RenderedTextLine& line : lines) {
        if (line.texture) {
            SDL_DestroyTexture(line.texture);
            line.texture = nullptr;
        }
    }
    lines.clear();
}

static std::vector<RenderedTextLine> build_text_lines(SDL_Renderer* renderer, TTF_Font* font,
                                                      const std::vector<std::string>& lines,
                                                      SDL_Color color, int max_line_width) {
    std::vector<RenderedTextLine> rendered_lines;
    rendered_lines.reserve(lines.size());

    for (const std::string& line_text : lines) {
        if (line_text.empty()) {
            // SDL_ttf cannot render empty strings ("Text has zero width").
            // Treat empty lines as vertical spacers.
            RenderedTextLine spacer;
            spacer.texture = nullptr;
            spacer.width = 0;
            spacer.height = TTF_FontLineSkip(font);
            rendered_lines.push_back(spacer);
            continue;
        }

        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, line_text.c_str(), color,
                                                              static_cast<Uint32>(max_line_width));
        if (!surface) {
            std::cerr << "Startup notice: failed to render text line: " << TTF_GetError()
                      << std::endl;
            destroy_text_lines(rendered_lines);
            return rendered_lines;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) {
            std::cerr << "Startup notice: failed to create text texture: " << SDL_GetError()
                      << std::endl;
            SDL_FreeSurface(surface);
            destroy_text_lines(rendered_lines);
            return rendered_lines;
        }

        RenderedTextLine rendered;
        rendered.texture = texture;
        rendered.width = surface->w;
        rendered.height = surface->h;
        rendered_lines.push_back(rendered);

        SDL_FreeSurface(surface);
    }

    return rendered_lines;
}

static void render_text_lines_centered(SDL_Renderer* renderer,
                                       const std::vector<RenderedTextLine>& lines, int top_margin,
                                       int line_spacing, SDL_Color background_color) {
    SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g, background_color.b,
                           background_color.a);
    SDL_RenderClear(renderer);

    int output_w = 0, output_h_unused = 0;
    SDL_GetRendererOutputSize(renderer, &output_w, &output_h_unused);

    int y = top_margin;

    for (const RenderedTextLine& line : lines) {
        SDL_Rect dst;
        dst.w = line.width;
        dst.h = line.height;
        dst.x = (output_w - line.width) / 2;
        if (dst.x < 0) dst.x = 0;
        dst.y = y;
        if (line.texture) {
            SDL_RenderCopy(renderer, line.texture, nullptr, &dst);
        }
        y += line.height + line_spacing;
    }

    SDL_RenderPresent(renderer);
}

static TTF_Font* open_startup_notice_font() {
    if (TTF_WasInit() == 0) {
        std::cerr << "Startup notice: SDL_ttf is not initialized" << std::endl;
        return nullptr;
    }

    auto try_open_font_candidate = [](const std::string& path, int size,
                                      bool try_indices) -> TTF_Font* {
        TTF_Font* font = TTF_OpenFont(path.c_str(), size);
        if (font) {
            return font;
        }

        if (!try_indices) {
            return nullptr;
        }

        // Many system fonts on macOS are TrueType collections (.ttc).
        // Try a few face indices in case the default face cannot be opened.
        constexpr long MAX_FONT_COLLECTION_INDEX = 8;
        for (long index = 0; index < MAX_FONT_COLLECTION_INDEX; ++index) {
            font = TTF_OpenFontIndex(path.c_str(), size, index);
            if (font) {
                return font;
            }
        }

        return nullptr;
    };

    const std::vector<std::string> font_candidates = {
        // macOS common monospace/system fonts
        "/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Courier.ttc",
        "/System/Library/Fonts/Monaco.ttf", "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Menlo.ttc",

        // Linux common monospace fonts
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",

        // Windows common monospace fonts
        "C:\\Windows\\Fonts\\lucidaconsole.ttf", "C:\\Windows\\Fonts\\consola.ttf"};

    constexpr int STARTUP_NOTICE_FONT_SIZE = 24;
    for (const std::string& path : font_candidates) {
        const bool is_collection = path.size() >= 4 && path.substr(path.size() - 4) == ".ttc";
        TTF_Font* font = try_open_font_candidate(path, STARTUP_NOTICE_FONT_SIZE, is_collection);
        if (font) {
            return font;
        }
    }

    std::cerr << "Startup notice: unable to load any fallback font" << std::endl;
    return nullptr;
}

static bool run_modal_text_screen(SDL_Renderer* renderer, TTF_Font* font,
                                  const std::vector<std::string>& lines) {
    constexpr SDL_Color TEXT_COLOR = {170, 170, 170, 255};
    constexpr SDL_Color BACKGROUND = {0, 0, 0, 255};

    const DosTextLayout layout = compute_dos_text_layout(renderer, font);

    std::vector<RenderedTextLine> rendered_lines =
        build_text_lines(renderer, font, lines, TEXT_COLOR, layout.max_width);
    if (rendered_lines.empty()) {
        return true;
    }

    SDL_Event e;
    while (true) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                destroy_text_lines(rendered_lines);
                return false;
            }
            if (e.type == SDL_KEYDOWN) {
                destroy_text_lines(rendered_lines);
                return true;
            }
        }

        render_text_lines_centered(renderer, rendered_lines, layout.top, layout.line_spacing,
                                   BACKGROUND);
        SDL_Delay(16);
    }
}

static std::vector<std::string> build_keyboard_setup_lines(const InputBindings& draft,
                                                           int action_index,
                                                           const std::string& status_line,
                                                           bool is_confirm_mode) {
    const std::vector<std::string> action_names = {"Move Left", "Move Right", "Jump",
                                                   "Fireball",  "Open Door",  "Teleport"};

    const std::vector<SDL_Keycode> action_keys = {
        draft.move_left, draft.move_right, draft.jump, draft.fire, draft.open_door, draft.teleport};

    std::vector<std::string> lines;
    lines.push_back("Keyboard Setup");
    lines.push_back("");

    const int total_actions = static_cast<int>(action_names.size());
    const bool prompt_for_action =
        !is_confirm_mode && action_index >= 0 && action_index < total_actions;

    if (!prompt_for_action) {
        lines.push_back("Review bindings");
        lines.push_back("Press Y to accept, N to reconfigure, ESC to cancel");
    } else {
        lines.push_back("Press a key for: " + action_names[action_index]);
        lines.push_back("ESC cancels and returns");
    }

    lines.push_back("");
    for (size_t i = 0; i < action_names.size(); ++i) {
        lines.push_back(action_names[i] + ": " + SDL_GetKeyName(action_keys[i]));
    }

    if (!status_line.empty()) {
        lines.push_back("");
        lines.push_back(status_line);
    }

    return lines;
}

static SDL_Keycode canonicalize_binding_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_RCTRL:
            return SDLK_LCTRL;
        case SDLK_RALT:
            return SDLK_LALT;
        case SDLK_RSHIFT:
            return SDLK_LSHIFT;
        default:
            return key;
    }
}

static bool key_is_already_assigned(const InputBindings& bindings, SDL_Keycode key,
                                    int current_action_index) {
    const SDL_Keycode canonical_key = canonicalize_binding_key(key);

    const SDL_Keycode keys[] = {bindings.move_left, bindings.move_right, bindings.jump,
                                bindings.fire,      bindings.open_door,  bindings.teleport};

    constexpr int NUM_ACTIONS = static_cast<int>(sizeof(keys) / sizeof(keys[0]));
    for (int i = 0; i < NUM_ACTIONS; ++i) {
        if (i == current_action_index) {
            continue;
        }
        if (canonicalize_binding_key(keys[i]) == canonical_key) {
            return true;
        }
    }

    return false;
}

static void set_binding_for_action(InputBindings& bindings, int action_index, SDL_Keycode key) {
    switch (action_index) {
        case 0:
            bindings.move_left = key;
            break;
        case 1:
            bindings.move_right = key;
            break;
        case 2:
            bindings.jump = key;
            break;
        case 3:
            bindings.fire = key;
            break;
        case 4:
            bindings.open_door = key;
            break;
        case 5:
            bindings.teleport = key;
            break;
        default:
            break;
    }
}

static bool input_bindings_equal(const InputBindings& lhs, const InputBindings& rhs) {
    return lhs.move_left == rhs.move_left && lhs.move_right == rhs.move_right &&
           lhs.jump == rhs.jump && lhs.fire == rhs.fire && lhs.open_door == rhs.open_door &&
           lhs.teleport == rhs.teleport;
}

static bool validate_input_bindings(const InputBindings& bindings, std::string* error_message) {
    const SDL_Keycode keys[] = {bindings.move_left, bindings.move_right, bindings.jump,
                                bindings.fire,      bindings.open_door,  bindings.teleport};

    if (std::any_of(std::begin(keys), std::end(keys),
                    [](SDL_Keycode key) { return key == SDLK_UNKNOWN; })) {
        if (error_message) {
            *error_message = "contains unknown keycode";
        }
        return false;
    }

    constexpr size_t NUM_BINDINGS = sizeof(keys) / sizeof(keys[0]);
    for (size_t i = 0; i < NUM_BINDINGS; ++i) {
        const SDL_Keycode canonical_i = canonicalize_binding_key(keys[i]);
        for (size_t j = i + 1; j < NUM_BINDINGS; ++j) {
            if (canonical_i == canonicalize_binding_key(keys[j])) {
                if (error_message) {
                    *error_message = "contains duplicate key assignments";
                }
                return false;
            }
        }
    }

    return true;
}

static bool run_keyboard_setup_menu(SDL_Renderer* renderer, TTF_Font* font) {
    constexpr SDL_Color TEXT_COLOR = {170, 170, 170, 255};
    constexpr SDL_Color BACKGROUND = {0, 0, 0, 255};

    InputBindings draft = s_input_bindings;
    int action_index = 0;
    std::string status_line;
    SDL_Event e;

    std::vector<RenderedTextLine> cached_rendered_lines;
    DosTextLayout cached_layout;
    bool cache_valid = false;
    InputBindings cached_draft = draft;
    int cached_action_index = -1;
    std::string cached_status_line;
    bool cached_confirm_mode = false;

    auto clear_cached_lines = [&]() {
        destroy_text_lines(cached_rendered_lines);
        cache_valid = false;
    };

    while (true) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                clear_cached_lines();
                return false;
            }

            if (e.type != SDL_KEYDOWN) {
                continue;
            }

            const SDL_Keycode key = e.key.keysym.sym;
            const bool is_confirm_mode = action_index >= 6;

            if (key == SDLK_ESCAPE) {
                clear_cached_lines();
                return true;
            }

            if (is_confirm_mode) {
                if (key == SDLK_y) {
                    set_input_bindings(draft);
                    const bool saved_ok = save_input_bindings_to_file();
                    if (saved_ok) {
                        if (!run_modal_text_screen(
                                renderer, font,
                                {"Keyboard Setup", "", "Bindings saved to KEYS.DEF", "",
                                 "Press any key to continue"})) {
                            clear_cached_lines();
                            return false;
                        }
                    } else {
                        if (!run_modal_text_screen(
                                renderer, font,
                                {"Keyboard Setup", "", "Error: could not save KEYS.DEF", "",
                                 "Press any key to continue"})) {
                            clear_cached_lines();
                            return false;
                        }
                    }
                    clear_cached_lines();
                    return true;
                }
                if (key == SDLK_n) {
                    draft = s_input_bindings;
                    action_index = 0;
                    status_line.clear();
                }
                continue;
            }

            if (key_is_already_assigned(draft, key, action_index)) {
                status_line = "That key is already assigned. Choose a different key.";
                continue;
            }

            set_binding_for_action(draft, action_index, key);
            action_index++;
            status_line.clear();
        }

        const bool is_confirm_mode = action_index >= 6;

        const bool should_rebuild_cache = !cache_valid || cached_action_index != action_index ||
                                          cached_confirm_mode != is_confirm_mode ||
                                          cached_status_line != status_line ||
                                          !input_bindings_equal(cached_draft, draft);

        if (should_rebuild_cache) {
            std::vector<std::string> lines =
                build_keyboard_setup_lines(draft, action_index, status_line, is_confirm_mode);

            cached_layout = compute_dos_text_layout(renderer, font);

            std::vector<RenderedTextLine> rendered =
                build_text_lines(renderer, font, lines, TEXT_COLOR, cached_layout.max_width);
            if (rendered.empty()) {
                clear_cached_lines();
                return true;
            }

            clear_cached_lines();
            cached_rendered_lines = std::move(rendered);
            cache_valid = true;
            cached_draft = draft;
            cached_action_index = action_index;
            cached_status_line = status_line;
            cached_confirm_mode = is_confirm_mode;
        }

        render_text_lines_centered(renderer, cached_rendered_lines, cached_layout.top,
                                   cached_layout.line_spacing, BACKGROUND);
        SDL_Delay(16);
    }
}

static std::string get_base_key_bindings_path() {
    char* base_path = SDL_GetBasePath();
    if (!base_path) {
        return std::string(KEY_BINDINGS_FILENAME);
    }

    std::string path = std::string(base_path) + KEY_BINDINGS_FILENAME;
    SDL_free(base_path);
    return path;
}

static std::string get_pref_key_bindings_path() {
    char* pref_path = SDL_GetPrefPath(PREF_PATH_ORG, PREF_PATH_APP);
    if (!pref_path) {
        return std::string();
    }

    std::string path = std::string(pref_path) + KEY_BINDINGS_FILENAME;
    SDL_free(pref_path);
    return path;
}

// ---------------------------------------------------------------------------
// High Scores
// ---------------------------------------------------------------------------

struct HighScoreEntry {
    std::string name;
    uint32_t score = 0;
};

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

const InputBindings& get_input_bindings() { return s_input_bindings; }

[[maybe_unused]] void reset_input_bindings_to_defaults() {
    s_input_bindings = DEFAULT_INPUT_BINDINGS;
}

void set_input_bindings(const InputBindings& bindings) { s_input_bindings = bindings; }

bool load_input_bindings_from_file() {
    const std::string pref_path = get_pref_key_bindings_path();
    const std::string base_path = get_base_key_bindings_path();

    std::string path;
    std::ifstream input;

    // Prefer per-user writable storage. If no file exists there, try the
    // legacy executable directory path for backward compatibility.
    if (!pref_path.empty()) {
        input.open(pref_path);
        if (input.good()) {
            path = pref_path;
        } else {
            input.close();
        }
    }

    if (!input.good()) {
        input.open(base_path);
        if (input.good()) {
            path = base_path;
        }
    }

    if (!input.good()) {
        return false;
    }

    std::string magic;
    std::getline(input, magic);
    if (!magic.empty() && magic.back() == '\r') {
        magic.pop_back();
    }
    if (magic != KEY_BINDINGS_MAGIC) {
        std::cerr << "Input bindings: invalid format in " << path << std::endl;
        return false;
    }

    int32_t move_left = 0;
    int32_t move_right = 0;
    int32_t jump = 0;
    int32_t fire = 0;
    int32_t open_door = 0;
    if (!(input >> move_left >> move_right >> jump >> fire >> open_door)) {
        std::cerr << "Input bindings: invalid key data in " << path << std::endl;
        return false;
    }

    // Backward compatibility: older files store only five bindings.
    int32_t teleport = static_cast<int32_t>(DEFAULT_INPUT_BINDINGS.teleport);
    if (!(input >> teleport)) {
        if (!input.eof()) {
            std::cerr << "Input bindings: invalid teleport key data in " << path << std::endl;
            return false;
        }
    }

    InputBindings loaded = {
        static_cast<SDL_Keycode>(move_left), static_cast<SDL_Keycode>(move_right),
        static_cast<SDL_Keycode>(jump),      static_cast<SDL_Keycode>(fire),
        static_cast<SDL_Keycode>(open_door), static_cast<SDL_Keycode>(teleport)};

    std::string validation_error;
    if (!validate_input_bindings(loaded, &validation_error)) {
        std::cerr << "Input bindings: invalid mapping in " << path << " (" << validation_error
                  << ")" << std::endl;
        return false;
    }

    set_input_bindings(loaded);
    return true;
}

bool save_input_bindings_to_file() {
    const std::string path = get_pref_key_bindings_path();
    if (path.empty()) {
        std::cerr << "Input bindings: SDL_GetPrefPath failed (" << SDL_GetError() << ")"
                  << std::endl;
        return false;
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.good()) {
        std::cerr << "Input bindings: could not open " << path << " for writing" << std::endl;
        return false;
    }

    const InputBindings& bindings = get_input_bindings();
    output << KEY_BINDINGS_MAGIC << "\n";
    output << static_cast<int32_t>(bindings.move_left) << " "
           << static_cast<int32_t>(bindings.move_right) << " "
           << static_cast<int32_t>(bindings.jump) << " " << static_cast<int32_t>(bindings.fire)
           << " " << static_cast<int32_t>(bindings.open_door) << " "
           << static_cast<int32_t>(bindings.teleport) << "\n";

    if (!output.good()) {
        std::cerr << "Input bindings: failed while writing " << path << std::endl;
        return false;
    }

    return true;
}

bool run_startup_notice(SDL_Renderer* renderer, GraphicsSystem* graphics) {
    (void)graphics;

    TTF_Font* font = open_startup_notice_font();
    if (!font) {
        // Non-fatal fallback: continue startup flow even if text cannot be rendered.
        return true;
    }

    const std::vector<std::string> startup_lines = {"Captain Comic",
                                                    "",
                                                    "Press K for Keyboard setup",
                                                    "Press J for Joystick calibration",
                                                    "Press R for Registration info",
                                                    "Press ESC to quit",
                                                    "Press any other key to continue"};

    constexpr SDL_Color TEXT_COLOR = {170, 170, 170, 255};
    constexpr SDL_Color BACKGROUND = {0, 0, 0, 255};

    const DosTextLayout layout = compute_dos_text_layout(renderer, font);

    std::vector<RenderedTextLine> rendered_startup =
        build_text_lines(renderer, font, startup_lines, TEXT_COLOR, layout.max_width);
    if (rendered_startup.empty()) {
        TTF_CloseFont(font);
        return true;
    }

    SDL_Event e;
    bool keep_running = true;
    while (keep_running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                destroy_text_lines(rendered_startup);
                TTF_CloseFont(font);
                return false;
            }

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        destroy_text_lines(rendered_startup);
                        TTF_CloseFont(font);
                        return false;
                    case SDLK_k:
                        if (!run_keyboard_setup_menu(renderer, font)) {
                            destroy_text_lines(rendered_startup);
                            TTF_CloseFont(font);
                            return false;
                        }
                        break;
                    case SDLK_j:
                        if (!run_modal_text_screen(
                                renderer, font,
                                {"Joystick Calibration", "", "Not yet implemented", "",
                                 "Press any key to return"})) {
                            destroy_text_lines(rendered_startup);
                            TTF_CloseFont(font);
                            return false;
                        }
                        break;
                    case SDLK_r:
                        if (!run_modal_text_screen(
                                renderer, font,
                                {"Registration", "", "Captain Comic Modernization",
                                 "Original game by Michael A. Denio", "",
                                 "Press any key to return"})) {
                            destroy_text_lines(rendered_startup);
                            TTF_CloseFont(font);
                            return false;
                        }
                        break;
                    default:
                        keep_running = false;
                        break;
                }
            }
        }

        if (keep_running) {
            render_text_lines_centered(renderer, rendered_startup, layout.top, layout.line_spacing,
                                       BACKGROUND);
            SDL_Delay(16);
        }
    }

    destroy_text_lines(rendered_startup);
    TTF_CloseFont(font);
    return true;
}

bool run_title_sequence(SDL_Renderer* renderer, GraphicsSystem* graphics) {
    // ------------------------------------------------------------------
    // Step 1: Title screen (SYS000.EGA)
    //   - Load image, display with palette fade-in, play title music, wait ~770 ms
    // ------------------------------------------------------------------
    s_title_surface = load_fullscreen_paletted_surface(graphics, TITLE_SCREEN_FILE);
    if (!s_title_surface) {
        std::cerr << "Title sequence aborted: could not load title screen" << std::endl;
        return true;
    }

    // Palette fade-in (darkens registers 2,10,12 then restores with color effects)
    if (!fade_in_paletted_surface(renderer, s_title_surface, TITLE_SCREEN_FILE)) {
        SDL_FreeSurface(s_title_surface);
        s_title_surface = nullptr;
        return false;
    }

    // Start title music (loops until stopped)
    play_game_music(GameMusic::TITLE);

    // Create texture and display for ~770 ms (original: wait_n_ticks(14))
    SDL_Texture* title_tex = surface_to_texture(renderer, s_title_surface);
    if (!title_tex) {
        SDL_FreeSurface(s_title_surface);
        s_title_surface = nullptr;
        stop_game_music();
        return true;
    }

    show_texture(renderer, title_tex);
    SDL_DestroyTexture(title_tex);

    if (!wait_ms(TITLE_DISPLAY_MS)) {
        SDL_FreeSurface(s_title_surface);
        s_title_surface = nullptr;
        stop_game_music();
        return false;
    }

    SDL_FreeSurface(s_title_surface);
    s_title_surface = nullptr;

    // ------------------------------------------------------------------
    // Step 2: Story screen (SYS001.EGA)
    //   - Load image, display with palette fade-in, wait for keypress
    // ------------------------------------------------------------------
    s_story_surface = load_fullscreen_paletted_surface(graphics, STORY_SCREEN_FILE);
    if (!s_story_surface) {
        std::cerr << "Title sequence aborted: could not load story screen" << std::endl;
        stop_game_music();
        return true;
    }

    if (!fade_in_paletted_surface(renderer, s_story_surface, STORY_SCREEN_FILE)) {
        SDL_FreeSurface(s_story_surface);
        s_story_surface = nullptr;
        stop_game_music();
        return false;
    }

    // Create texture for keypress waiting
    SDL_Texture* story_tex = surface_to_texture(renderer, s_story_surface);
    if (!story_tex) {
        SDL_FreeSurface(s_story_surface);
        s_story_surface = nullptr;
        stop_game_music();
        return true;
    }

    if (!wait_for_keypress(renderer, story_tex)) {
        SDL_DestroyTexture(story_tex);
        SDL_FreeSurface(s_story_surface);
        s_story_surface = nullptr;
        stop_game_music();
        return false;
    }

    SDL_DestroyTexture(story_tex);
    SDL_FreeSurface(s_story_surface);
    s_story_surface = nullptr;

    // ------------------------------------------------------------------
    // Step 3: Game UI background (SYS003.EGA)
    //   - Load HUD graphic into a persistent texture for gameplay use.
    //   - Original loads into both gameplay buffers; here we keep one
    //     texture that the game loop composites under the playfield.
    // ------------------------------------------------------------------
    SDL_Surface* hud_surface = load_fullscreen_paletted_surface(graphics, GAME_UI_FILE);
    if (hud_surface) {
        SDL_Texture* hud_tex = surface_to_texture(renderer, hud_surface);
        SDL_FreeSurface(hud_surface);

        if (hud_tex) {
            // Retain for gameplay
            if (s_hud_texture) {
                SDL_DestroyTexture(s_hud_texture);
            }
            s_hud_texture = hud_tex;
        }
    } else {
        std::cerr << "Warning: Could not load game UI graphic" << std::endl;
    }

    // ------------------------------------------------------------------
    // Step 4: Items screen (SYS004.EGA)
    //   - Display WITHOUT palette fade effect, wait for keypress
    // ------------------------------------------------------------------
    s_items_surface = load_fullscreen_paletted_surface(graphics, ITEMS_SCREEN_FILE);
    if (!s_items_surface) {
        std::cerr << "Title sequence aborted: could not load items screen" << std::endl;
        stop_game_music();
        return true;
    }

    // Create texture directly (no fade effect for items screen)
    SDL_Texture* items_tex = surface_to_texture(renderer, s_items_surface);
    if (!items_tex) {
        SDL_FreeSurface(s_items_surface);
        s_items_surface = nullptr;
        stop_game_music();
        return true;
    }

    if (!wait_for_keypress(renderer, items_tex)) {
        SDL_DestroyTexture(items_tex);
        SDL_FreeSurface(s_items_surface);
        s_items_surface = nullptr;
        stop_game_music();
        return false;
    }

    SDL_DestroyTexture(items_tex);
    SDL_FreeSurface(s_items_surface);
    s_items_surface = nullptr;

    // ------------------------------------------------------------------
    // Step 5: Display HUD graphic as transition (SYS003.EGA)
    //   - Show the game UI background that will be used during gameplay
    //   - Convert and display the retained HUD texture before returning
    // ------------------------------------------------------------------
    if (s_hud_texture) {
        show_texture(renderer, s_hud_texture);
        // Hold HUD display briefly before transitioning to gameplay
        if (!wait_ms(300)) {
            stop_game_music();
            return false;
        }
    }

    // ------------------------------------------------------------------
    // Step 6: Cleanup and transition
    //   - Stop title music before entering gameplay
    // ------------------------------------------------------------------
    stop_game_music();

    return true;
}

SDL_Texture* get_hud_texture() { return s_hud_texture; }

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

void cleanup_title_sequence() {
    if (s_title_surface) {
        SDL_FreeSurface(s_title_surface);
        s_title_surface = nullptr;
    }
    if (s_story_surface) {
        SDL_FreeSurface(s_story_surface);
        s_story_surface = nullptr;
    }
    if (s_items_surface) {
        SDL_FreeSurface(s_items_surface);
        s_items_surface = nullptr;
    }
    if (s_hud_texture) {
        SDL_DestroyTexture(s_hud_texture);
        s_hud_texture = nullptr;
    }
}
