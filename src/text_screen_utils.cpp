#include "../include/text_screen_utils.h"

#include <algorithm>
#include <iostream>

DosTextLayout compute_dos_text_layout(SDL_Renderer* renderer, TTF_Font* font) {
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

void destroy_text_lines(std::vector<RenderedTextLine>& lines) {
    for (RenderedTextLine& line : lines) {
        if (line.texture) {
            SDL_DestroyTexture(line.texture);
            line.texture = nullptr;
        }
    }
    lines.clear();
}

std::vector<RenderedTextLine> build_text_lines(SDL_Renderer* renderer, TTF_Font* font,
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

void render_text_lines_centered(SDL_Renderer* renderer, const std::vector<RenderedTextLine>& lines,
                                int top_margin, int line_spacing, SDL_Color background_color) {
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

TTF_Font* open_startup_notice_font() {
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
