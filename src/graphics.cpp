#include "../include/graphics.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "../include/cheats.h"
#include "../include/level.h"
#include "../include/physics.h"

// External game state (defined in main.cpp)
extern int comic_x;
extern int comic_y;
extern int8_t comic_y_vel;
extern int8_t comic_x_momentum;
extern uint8_t comic_facing;
extern uint8_t current_level_number;
extern uint8_t current_stage_number;
extern int camera_x;

// Global graphics system
GraphicsSystem* g_graphics = nullptr;

GraphicsSystem::GraphicsSystem(SDL_Renderer* renderer)
    : renderer(renderer), img_inited(false), ttf_inited(false), debug_font(nullptr) {}

GraphicsSystem::~GraphicsSystem() { cleanup(); }

bool GraphicsSystem::initialize() {
    // Initialize SDL_image
    int flags = IMG_INIT_PNG;
    if ((IMG_Init(flags) & flags) != flags) {
        std::cerr << "SDL_image initialization failed: " << IMG_GetError() << std::endl;
        return false;
    }
    img_inited = true;

    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        std::cerr << "SDL_ttf initialization failed: " << TTF_GetError() << std::endl;
        IMG_Quit();
        img_inited = false;
        return false;
    }
    ttf_inited = true;

    // Try to load a monospace font for debug overlay
    // Try multiple possible font paths and names
    std::string font_candidates[] = {
        // macOS system fonts
        "/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Courier.ttc",
        "/System/Library/Fonts/SFNSMono.ttf", "/Library/Fonts/Menlo.ttc",

        // Linux fonts
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",

        // Windows fonts
        "C:\\Windows\\Fonts\\lucidaconsole.ttf", "C:\\Windows\\Fonts\\consola.ttf",

        // Bundled fonts
        "assets/fonts/monospace.ttf"};

    const int debug_font_size = 12;
    for (const auto& font_path : font_candidates) {
        debug_font = TTF_OpenFont(font_path.c_str(), debug_font_size);
        if (debug_font != nullptr) {
            break;
        }
    }

    if (debug_font == nullptr) {
        std::cerr
            << "Warning: Could not load debug font, debug overlay will not display coordinates"
            << std::endl;
        std::cerr << "  Tried: Menlo, Courier, DejaVuSansMono, LiberationMono, and others"
                  << std::endl;
    }

    return true;
}

std::string GraphicsSystem::get_asset_path(const std::string& filename) {
    // Depending on the asset type, files are now organized into subdirectories
    // (graphics, maps, shp, sounds, sprites, tiles).  We attempt to guess the
    // appropriate folder based on naming conventions and file extensions.
    std::vector<std::string> prefixes;

    // sprite assets
    if (filename.rfind("sprite-", 0) == 0) {
        prefixes.push_back("assets/sprites/");
        prefixes.push_back("../assets/sprites/");
        prefixes.push_back("../../assets/sprites/");
    }
    // sound effects
    else if (filename.rfind("sound-", 0) == 0) {
        prefixes.push_back("assets/sounds/");
        prefixes.push_back("../assets/sounds/");
        prefixes.push_back("../../assets/sounds/");
    }
    // map files (.pt.png)
    else if (filename.find(".pt.png") != std::string::npos) {
        prefixes.push_back("assets/maps/");
        prefixes.push_back("../assets/maps/");
        prefixes.push_back("../../assets/maps/");
    }
    // tilesets (.tt2-XX.png)
    else if (filename.find(".tt2-") != std::string::npos) {
        prefixes.push_back("assets/tiles/");
        prefixes.push_back("../assets/tiles/");
        prefixes.push_back("../../assets/tiles/");
    }
    // shp-based enemy frames
    else if (filename.find(".shp") != std::string::npos) {
        prefixes.push_back("assets/shp/");
        prefixes.push_back("../assets/shp/");
        prefixes.push_back("../../assets/shp/");
    }
    // raw graphics (sys000.ega etc)
    else if (filename.rfind("sys", 0) == 0) {
        prefixes.push_back("assets/graphics/");
        prefixes.push_back("../assets/graphics/");
        prefixes.push_back("../../assets/graphics/");
    }

    // always try the generic assets/ location as a fallback
    prefixes.push_back("assets/");
    prefixes.push_back("../assets/");
    prefixes.push_back("../../assets/");

    for (const auto& prefix : prefixes) {
        std::string path = prefix + filename;
        std::ifstream f(path);
        if (f.good()) {
            return path;
        }
    }

    // last resort return base location (may be wrong)
    return std::string("assets/") + filename;
}

TextureInfo GraphicsSystem::load_png(const std::string& filepath) {
    TextureInfo info = {nullptr, 0, 0};
    static std::unordered_set<std::string> logged_load_failures;

    // Try multiple possible paths
    std::string possible_paths[] = {filepath, "../" + filepath, "../../" + filepath};

    SDL_Surface* surface = nullptr;

    for (const auto& path : possible_paths) {
        // Check if file exists
        std::ifstream f(path);
        if (f.good()) {
            surface = IMG_Load(path.c_str());
            if (surface) {
                break;
            }
            if (logged_load_failures.insert(path).second) {
                std::cerr << "Warning: Failed to load PNG: " << path << " (" << IMG_GetError()
                          << ")" << std::endl;
            }
        }
    }

    if (surface == nullptr) {
        // Silently return null texture - caller will handle missing assets
        return info;
    }

    info.texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (info.texture == nullptr) {
        std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return info;
    }

    info.width = surface->w;
    info.height = surface->h;

    SDL_FreeSurface(surface);
    return info;
}

std::vector<uint8_t> build_enemy_animation_sequence(uint8_t num_distinct_frames,
                                                    uint8_t animation_type) {
    std::vector<uint8_t> sequence;
    if (num_distinct_frames == 0) {
        return sequence;
    }

    sequence.reserve(static_cast<size_t>(num_distinct_frames) * 2);
    for (uint8_t i = 0; i < num_distinct_frames; ++i) {
        sequence.push_back(i);
    }

    if (animation_type == ENEMY_ANIMATION_ALTERNATE && num_distinct_frames > 2) {
        for (int i = static_cast<int>(num_distinct_frames) - 2; i >= 1; --i) {
            sequence.push_back(static_cast<uint8_t>(i));
        }
    }

    return sequence;
}

std::vector<TextureInfo> GraphicsSystem::load_animation_frames(const std::string& base_path,
                                                               int expected_frames,
                                                               const std::string& label) {
    std::vector<TextureInfo> frames;
    if (renderer == nullptr) {
        std::cerr << "Warning: Renderer unavailable; cannot load animation: " << label << std::endl;
        return frames;
    }

    if (expected_frames <= 0) {
        return frames;
    }

    // Try multiple asset directory prefixes.  enemy frames live under assets/shp, so
    // include that directory first.  We keep the generic locations as fallback.
    static const std::string prefixes[] = {"assets/shp/", "../assets/shp/", "../../assets/shp/",
                                           "assets/",     "../assets/",     "../../assets/"};

    auto cleanup_frames = [](std::vector<TextureInfo>& f) {
        for (auto& info : f) {
            if (info.texture) {
                SDL_DestroyTexture(info.texture);
            }
        }
        f.clear();
    };

    for (int i = 0; i < expected_frames; ++i) {
        std::string filename = base_path + "-" + std::to_string(i) + ".png";
        SDL_Surface* surface = nullptr;

        for (const auto& prefix : prefixes) {
            std::string path = prefix + filename;
            std::ifstream f(path);
            if (!f.good()) {
                continue;
            }
            surface = IMG_Load(path.c_str());
            if (surface != nullptr) {
                break;
            }
            std::cerr << "Warning: Failed to load animation frame: " << path << " ("
                      << IMG_GetError() << ")" << std::endl;
        }

        if (surface == nullptr) {
            std::cerr << "Error: Could not find frame " << i << " for '" << label << "' (tried "
                      << filename << ")" << std::endl;
            cleanup_frames(frames);
            return frames;
        }

        int w = surface->w;
        int h = surface->h;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (texture == nullptr) {
            std::cerr << "Error: Failed to create animation texture for '" << label << "' frame "
                      << i << ": " << SDL_GetError() << std::endl;
            cleanup_frames(frames);
            return frames;
        }

        frames.push_back({texture, w, h});
    }

    return frames;
}

bool GraphicsSystem::load_tileset(const std::string& level_name) {
    if (tilesets.find(level_name) != tilesets.end()) {
        return true;
    }

    Tileset tileset;
    int missing_count = 0;
    int loaded_count = 0;
    std::string first_missing;

    // Load all tiles (0x00-0x7F / 0-127) for the level
    // Some levels have up to 87 tiles, so we try to load 128 to be safe
    // Missing tiles beyond what exists is expected and not an error
    for (int i = 0; i < 128; i++) {
        char tile_name[64];
        std::snprintf(tile_name, sizeof(tile_name), "%s.tt2-%02x.png", level_name.c_str(), i);

        std::string filepath = get_asset_path(tile_name);
        TextureInfo texture = load_png(filepath);

        if (texture.texture != nullptr) {
            tileset.tiles[i] = texture;
            loaded_count++;
        } else {
            missing_count++;
            if (first_missing.empty()) {
                first_missing = tile_name;
            }
        }
    }

    if (tileset.tiles.empty()) {
        std::cerr << "Error: Failed to load any tiles for tileset: " << level_name << std::endl;
        return false;
    }

    if (missing_count > 0) {
        std::cerr << "Warning: Tileset '" << level_name << "' missing " << missing_count
                  << " tile(s)"
                  << " (loaded " << loaded_count << ")" << (first_missing.empty() ? "" : ", e.g. ")
                  << (first_missing.empty() ? "" : first_missing) << std::endl;
    }

    tilesets[level_name] = tileset;

    // Apply any previously configured blackout state to newly loaded tiles.
    auto blackout_it = tileset_blackout.find(level_name);
    if (blackout_it != tileset_blackout.end()) {
        set_tileset_blackout(level_name, blackout_it->second);
    }

    return true;
}

Tileset* GraphicsSystem::get_tileset(const std::string& level_name) {
    auto it = tilesets.find(level_name);
    if (it != tilesets.end()) {
        return &it->second;
    }
    return nullptr;
}

void GraphicsSystem::set_tileset_blackout(const std::string& level_name, bool blackout) {
    tileset_blackout[level_name] = blackout;

    auto tileset_it = tilesets.find(level_name);
    if (tileset_it == tilesets.end()) {
        return;
    }

    const uint8_t color = blackout ? 0 : 255;
    for (auto& tile_entry : tileset_it->second.tiles) {
        TextureInfo& tile_texture = tile_entry.second;
        if (tile_texture.texture != nullptr) {
            SDL_SetTextureColorMod(tile_texture.texture, color, color, color);
        }
    }
}

bool GraphicsSystem::is_tileset_blacked_out(const std::string& level_name) const {
    auto blackout_it = tileset_blackout.find(level_name);
    if (blackout_it == tileset_blackout.end()) {
        return false;
    }
    return blackout_it->second;
}

bool GraphicsSystem::load_sprite(const std::string& sprite_name, const std::string& direction) {
    std::string key = direction.empty() ? sprite_name : (sprite_name + "_" + direction);
    if (sprites.find(key) != sprites.end()) {
        return true;
    }

    std::string filename;
    if (direction.empty()) {
        filename = "sprite-" + sprite_name + ".png";
    } else {
        filename = "sprite-" + sprite_name + "_" + direction + ".png";
    }
    std::string filepath = get_asset_path(filename);

    TextureInfo texture = load_png(filepath);
    if (texture.texture == nullptr) {
        std::cerr << "Warning: Missing sprite asset: " << filename << std::endl;
        return false;
    }

    Sprite sprite;
    sprite.texture = texture;
    sprite.width = texture.width;
    sprite.height = texture.height;

    sprites[key] = sprite;
    return true;
}

Sprite* GraphicsSystem::get_sprite(const std::string& sprite_name, const std::string& direction) {
    std::string key = direction.empty() ? sprite_name : (sprite_name + "_" + direction);
    auto it = sprites.find(key);
    if (it != sprites.end()) {
        return &it->second;
    }
    return nullptr;
}

SpriteAnimationData* GraphicsSystem::load_enemy_sprite(const shp_t& sprite_desc) {
    if (sprite_desc.num_distinct_frames == 0) {
        return nullptr;
    }

    std::string sprite_name = sprite_desc.filename;
    size_t null_pos = sprite_name.find('\0');
    if (null_pos != std::string::npos) {
        sprite_name = sprite_name.substr(0, null_pos);
    }
    while (!sprite_name.empty() && sprite_name.back() == ' ') {
        sprite_name.pop_back();
    }
    if (sprite_name.empty()) {
        return nullptr;
    }

    std::ostringstream cache_key;
    cache_key << sprite_name << ":" << static_cast<int>(sprite_desc.num_distinct_frames) << ":"
              << static_cast<int>(sprite_desc.horizontal) << ":"
              << static_cast<int>(sprite_desc.animation);

    // Check if already loaded
    auto it = enemy_sprites.find(cache_key.str());
    if (it != enemy_sprites.end()) {
        return it->second;
    }

    // Load per-frame PNGs: {sprite_name}-left-0.png, -1.png, etc.
    auto* animation_data = new SpriteAnimationData();
    animation_data->frames_left = load_animation_frames(
        sprite_name + "-left", sprite_desc.num_distinct_frames, sprite_name + ":left");

    if (sprite_desc.horizontal == ENEMY_HORIZONTAL_SEPARATE) {
        animation_data->frames_right = load_animation_frames(
            sprite_name + "-right", sprite_desc.num_distinct_frames, sprite_name + ":right");
    }

    if (animation_data->frames_left.empty()) {
        delete animation_data;
        return nullptr;
    }

    if (sprite_desc.horizontal == ENEMY_HORIZONTAL_SEPARATE &&
        animation_data->frames_right.empty()) {
        for (auto& frame : animation_data->frames_left) {
            if (frame.texture) {
                SDL_DestroyTexture(frame.texture);
            }
        }
        delete animation_data;
        return nullptr;
    }

    size_t distinct_frames = animation_data->frames_left.size();
    if (sprite_desc.horizontal == ENEMY_HORIZONTAL_SEPARATE) {
        distinct_frames = std::min(distinct_frames, animation_data->frames_right.size());
        if (animation_data->frames_left.size() != animation_data->frames_right.size()) {
            std::cerr << "Warning: Animation frame count mismatch for " << sprite_name
                      << " (left=" << animation_data->frames_left.size()
                      << ", right=" << animation_data->frames_right.size() << "), truncating to "
                      << distinct_frames << std::endl;
            // Destroy excess textures before truncating to avoid leaks
            for (size_t i = distinct_frames; i < animation_data->frames_left.size(); ++i) {
                if (animation_data->frames_left[i].texture) {
                    SDL_DestroyTexture(animation_data->frames_left[i].texture);
                    animation_data->frames_left[i].texture = nullptr;
                }
            }
            for (size_t i = distinct_frames; i < animation_data->frames_right.size(); ++i) {
                if (animation_data->frames_right[i].texture) {
                    SDL_DestroyTexture(animation_data->frames_right[i].texture);
                    animation_data->frames_right[i].texture = nullptr;
                }
            }
            animation_data->frames_left.resize(distinct_frames);
            animation_data->frames_right.resize(distinct_frames);
        }
    }

    // Clamp to uint8_t range before cast — enemy animations are defined in
    // level_data.cpp with num_distinct_frames as uint8_t (max 255), so this
    // is a safety guard rather than an expected runtime condition.
    animation_data->frame_sequence = build_enemy_animation_sequence(
        static_cast<uint8_t>(std::min(distinct_frames, static_cast<size_t>(255))),
        sprite_desc.animation);

    if (animation_data->frame_sequence.empty()) {
        for (auto& frame : animation_data->frames_left) {
            if (frame.texture) {
                SDL_DestroyTexture(frame.texture);
            }
        }
        for (auto& frame : animation_data->frames_right) {
            if (frame.texture) {
                SDL_DestroyTexture(frame.texture);
            }
        }
        delete animation_data;
        return nullptr;
    }

    enemy_sprites[cache_key.str()] = animation_data;
    return animation_data;
}

Animation GraphicsSystem::create_animation(const std::vector<std::string>& sprite_names,
                                           const std::string& direction, int frame_duration_ms,
                                           bool looping) {
    Animation anim;
    anim.looping = looping;
    anim.frame_start_time = SDL_GetTicks();
    int safe_duration_ms = frame_duration_ms > 0 ? frame_duration_ms : 1;

    for (const auto& sprite_name : sprite_names) {
        if (load_sprite(sprite_name, direction)) {
            Sprite* sprite = get_sprite(sprite_name, direction);
            if (sprite) {
                AnimationFrame frame;
                frame.sprite = *sprite;
                frame.duration_ms = safe_duration_ms;
                anim.frames.push_back(frame);
            }
        }
    }

    return anim;
}

AnimationFrame* GraphicsSystem::get_current_frame(Animation& anim) {
    if (anim.frames.empty()) {
        return nullptr;
    }
    return &anim.frames[anim.current_frame];
}

void GraphicsSystem::update_animation(Animation& anim, uint32_t current_time) {
    if (anim.frames.empty()) {
        return;
    }

    int total_duration = 0;
    for (const auto& frame : anim.frames) {
        total_duration += frame.duration_ms > 0 ? frame.duration_ms : 1;
    }

    if (total_duration <= 0) {
        anim.current_frame = 0;
        return;
    }

    uint32_t elapsed = current_time - anim.frame_start_time;
    if (anim.looping) {
        elapsed %= static_cast<uint32_t>(total_duration);
    } else if (elapsed >= static_cast<uint32_t>(total_duration)) {
        anim.current_frame = static_cast<int>(anim.frames.size()) - 1;
        return;
    }

    uint32_t cursor = 0;
    for (size_t i = 0; i < anim.frames.size(); i++) {
        int frame_duration = anim.frames[i].duration_ms > 0 ? anim.frames[i].duration_ms : 1;
        cursor += static_cast<uint32_t>(frame_duration);
        if (elapsed < cursor) {
            anim.current_frame = static_cast<int>(i);
            return;
        }
    }

    anim.current_frame = static_cast<int>(anim.frames.size()) - 1;
}

void GraphicsSystem::render_tile(int screen_x, int screen_y, Tileset* tileset, uint8_t tile_id,
                                 int scale) {
    if (tileset == nullptr) return;

    auto it = tileset->tiles.find(tile_id);
    if (it == tileset->tiles.end()) return;

    const TextureInfo& texture = it->second;
    int pixel_size = scale * 2;  // 2 game units per tile
    SDL_Rect dst_rect = {screen_x, screen_y, pixel_size, pixel_size};
    SDL_RenderCopy(renderer, texture.texture, nullptr, &dst_rect);
}

void GraphicsSystem::render_sprite(int screen_x, int screen_y, const Sprite& sprite, bool flip_h) {
    SDL_Rect dst_rect = {screen_x, screen_y, sprite.width, sprite.height};
    SDL_RendererFlip flip = flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, sprite.texture.texture, nullptr, &dst_rect, 0, nullptr, flip);
}

void GraphicsSystem::render_sprite_scaled(int screen_x, int screen_y, const Sprite& sprite,
                                          int width, int height, bool flip_h) {
    SDL_Rect dst_rect = {screen_x, screen_y, width, height};
    SDL_RendererFlip flip = flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, sprite.texture.texture, nullptr, &dst_rect, 0, nullptr, flip);
}

void GraphicsSystem::render_sprite_centered(int screen_x, int screen_y, const Sprite& sprite,
                                            bool flip_h) {
    int x = screen_x - sprite.width / 2;
    int y = screen_y - sprite.height / 2;
    render_sprite(x, y, sprite, flip_h);
}

void GraphicsSystem::render_sprite_centered_scaled(int screen_x, int screen_y, const Sprite& sprite,
                                                   int width, int height, bool flip_h) {
    int x = screen_x - width / 2;
    int y = screen_y - height / 2;
    render_sprite_scaled(x, y, sprite, width, height, flip_h);
}

void GraphicsSystem::render_sprite_top_clip_scaled(int screen_x, int screen_y, const Sprite& sprite,
                                                   int width, int full_height, int clip_height,
                                                   bool flip_h) {
    if (clip_height <= 0 || full_height <= 0 || sprite.texture.texture == nullptr) {
        return;
    }

    const int clamped_clip_height = std::min(clip_height, full_height);

    // Anchor at the top-left corner of where the full sprite would be drawn, so the
    // bottom edge is clipped rather than the whole image being squashed.
    const int dest_x = screen_x - width / 2;
    const int dest_y = screen_y - full_height / 2;  // top of full sprite, unchanged

    // Proportional source crop: take only the top fraction of the texture.
    int tex_w = 0;
    int tex_h = 0;
    SDL_QueryTexture(sprite.texture.texture, nullptr, nullptr, &tex_w, &tex_h);
    if (tex_w <= 0 || tex_h <= 0) {
        return;
    }

    // clip_height / full_height == fraction of texture rows to read from the top.
    int src_h = (tex_h * clamped_clip_height) / full_height;
    if (src_h <= 0) {
        return;
    }
    if (src_h > tex_h) {
        src_h = tex_h;
    }

    SDL_Rect src_rect = {0, 0, tex_w, src_h};
    SDL_Rect dst_rect = {dest_x, dest_y, width, clamped_clip_height};
    SDL_RendererFlip flip = flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, sprite.texture.texture, &src_rect, &dst_rect, 0, nullptr, flip);
}

void GraphicsSystem::render_text(int screen_x, int screen_y, const std::string& text,
                                 SDL_Color color) {
    if (debug_font == nullptr) {
        return;  // Font not available
    }

    SDL_Surface* surface = TTF_RenderText_Solid(debug_font, text.c_str(), color);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst_rect = {screen_x, screen_y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst_rect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void GraphicsSystem::render_debug_overlay() {
    // Draw a semi-transparent debug indicator in top-left corner
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Background box
    SDL_Rect bg_rect = {5, 5, 200, 100};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);  // Semi-transparent black
    SDL_RenderFillRect(renderer, &bg_rect);

    // Border
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  // Yellow border
    SDL_RenderDrawRect(renderer, &bg_rect);

    // Draw noclip indicator if active
    if (cheat_noclip) {
        SDL_Rect noclip_indicator = {10, 10, 20, 20};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  // Green square for noclip
        SDL_RenderFillRect(renderer, &noclip_indicator);
    }

    // Draw simple bars to visualize velocity
    // Y velocity bar (vertical)
    int vel_bar_height = std::abs(comic_y_vel) * 2;
    if (vel_bar_height > 50) vel_bar_height = 50;
    SDL_Rect vel_bar = {40, 50 - vel_bar_height / 2, 10, vel_bar_height};
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  // Red for velocity
    SDL_RenderFillRect(renderer, &vel_bar);

    // X momentum bar (horizontal)
    int momentum_bar_width = std::abs(comic_x_momentum) * 3;
    if (momentum_bar_width > 50) momentum_bar_width = 50;
    SDL_Rect momentum_bar = {60, 40, momentum_bar_width, 10};
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);  // Blue for momentum
    SDL_RenderFillRect(renderer, &momentum_bar);

    // Reset blend mode
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Render coordinate information as text
    if (debug_font != nullptr) {
        std::ostringstream coord_text;
        coord_text << "X: " << static_cast<int>(comic_x) << " Y: " << static_cast<int>(comic_y);
        render_text(10, 70, coord_text.str(), {0, 255, 255, 255});  // Cyan text

        std::ostringstream level_text;
        level_text << "L" << static_cast<int>(current_level_number) << " S"
                   << static_cast<int>(current_stage_number);
        render_text(10, 85, level_text.str(), {0, 255, 255, 255});  // Cyan text
    }
}

SDL_Rect GraphicsSystem::compute_letterbox_rect(SDL_Renderer* renderer) {
    int win_w = 0;
    int win_h = 0;
    SDL_GetRendererOutputSize(renderer, &win_w, &win_h);

    const float scale_x = static_cast<float>(win_w) / EGA_WIDTH;
    const float scale_y = static_cast<float>(win_h) / EGA_HEIGHT;
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const int dst_w = static_cast<int>(EGA_WIDTH * scale);
    const int dst_h = static_cast<int>(EGA_HEIGHT * scale);

    SDL_Rect rect;
    rect.x = (win_w - dst_w) / 2;
    rect.y = (win_h - dst_h) / 2;
    rect.w = dst_w;
    rect.h = dst_h;
    return rect;
}

void GraphicsSystem::cleanup() {
    // Clean up tilesets
    for (auto& pair : tilesets) {
        pair.second.cleanup();
    }
    tilesets.clear();

    // Clean up sprites
    for (auto& pair : sprites) {
        if (pair.second.texture.texture) {
            SDL_DestroyTexture(pair.second.texture.texture);
        }
    }
    sprites.clear();

    // Clean up enemy sprites
    for (auto& pair : enemy_sprites) {
        if (pair.second) {
            for (auto& frame : pair.second->frames_left) {
                if (frame.texture) {
                    SDL_DestroyTexture(frame.texture);
                }
            }
            for (auto& frame : pair.second->frames_right) {
                if (frame.texture) {
                    SDL_DestroyTexture(frame.texture);
                }
            }
            delete pair.second;
        }
    }
    enemy_sprites.clear();

    // Clean up fonts
    if (debug_font != nullptr) {
        TTF_CloseFont(debug_font);
        debug_font = nullptr;
    }
    if (ttf_inited) {
        TTF_Quit();
        ttf_inited = false;
    }

    // Only quit SDL_image once to prevent double-quit errors
    if (img_inited) {
        IMG_Quit();
        img_inited = false;
    }
}

void Tileset::cleanup() {
    for (auto& pair : tiles) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
    tiles.clear();
}
