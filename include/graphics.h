#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <SDL2/SDL.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Original EGA resolution (used for letterbox scaling)
constexpr int EGA_WIDTH = 320;
constexpr int EGA_HEIGHT = 200;

// Forward declarations
struct SdlFontHandle;  // Opaque pimpl for the debug TTF font (defined in graphics.cpp);
                       // SDL_ttf.h is NOT included here and its type spells differ between
                       // SDL2 versions (_TTF_Font vs TTF_Font), so no SDL font names here.
struct Texture;
struct Tileset;
struct SpriteSheet;

// Tile size in pixels (16x16 in original EGA)
constexpr int TILE_SIZE = 16;

// Texture rendering structure.
//
// `texture` is a std::shared_ptr with the SDL_DestroyTexture custom deleter
// attached at construction time (see graphics.cpp), so every TextureInfo
// copy shares one owning reference and the texture is released automatically
// when the last holder disappears. TextureInfo is copied by value into
// Sprite/Animation structures, so shared ownership (not unique_ptr) is the
// correct shape here; still, keep treat it as immutable - the SDL_Texture*
// itself is never changed after load.
struct TextureInfo {
    std::shared_ptr<SDL_Texture> texture;
    int width = 0;
    int height = 0;

    // The raw pointer for SDL calls: implicit conversion does not exist for
    // shared_ptr, call sites can use .get().
    SDL_Texture* get() const { return texture.get(); }
};

// Tileset: contains multiple tile graphics (16x16 pixels each)
struct Tileset {
    std::map<uint8_t, TextureInfo> tiles;  // tile_id -> TextureInfo
    void cleanup();
};

// Sprite structure for player and actors
struct Sprite {
    TextureInfo texture{};
    int width = 0;
    int height = 0;
};

// Sprite animation frame
struct AnimationFrame {
    Sprite sprite{};
    int duration_ms = 0;  // How long to display this frame
};

// Animation sequence
struct Animation {
    std::vector<AnimationFrame> frames;
    int current_frame;
    uint32_t frame_start_time;
    bool looping;

    Animation() : current_frame(0), frame_start_time(0), looping(true) {}
};

// Enemy sprite animation data
// Note: Metadata (num_frames, horizontal orientation) is stored in the level's shp_t descriptor.
// This struct only contains the loaded texture data.
struct SpriteAnimationData {
    std::vector<TextureInfo> frames_left;  /* Left-facing animation frames */
    std::vector<TextureInfo> frames_right; /* Right-facing animation frames */
    std::vector<uint8_t> frame_sequence;   /* Animation order (indexes into frames_*) */
};

// Build animation frame sequence for enemy sprites
std::vector<uint8_t> build_enemy_animation_sequence(uint8_t num_distinct_frames,
                                                    uint8_t animation_type);

// Graphics system
class GraphicsSystem {
public:
    explicit GraphicsSystem(SDL_Renderer* renderer);
    ~GraphicsSystem();

    // Initialize - must be called after construction
    bool initialize();

    // Tileset loading/management
    bool load_tileset(const std::string& level_name);
    Tileset* get_tileset(const std::string& level_name);
    void set_tileset_blackout(const std::string& level_name, bool blackout);
    bool is_tileset_blacked_out(const std::string& level_name) const;

    // Sprite loading
    bool load_sprite(const std::string& sprite_name, const std::string& direction);
    Sprite* get_sprite(const std::string& sprite_name, const std::string& direction);

    // Enemy sprite loading
    SpriteAnimationData* load_enemy_sprite(const struct shp_t& sprite_desc);

    // Animation management
    Animation create_animation(const std::vector<std::string>& sprite_names,
                               const std::string& direction, int frame_duration_ms,
                               bool looping = true);
    static AnimationFrame* get_current_frame(Animation& anim);
    static void update_animation(Animation& anim, uint32_t current_time);

    // Rendering
    void render_tile(int screen_x, int screen_y, Tileset* tileset, uint8_t tile_id, int scale);
    void render_sprite(int screen_x, int screen_y, const Sprite& sprite, bool flip_h = false);
    void render_sprite_scaled(int screen_x, int screen_y, const Sprite& sprite, int width,
                              int height, bool flip_h = false);
    [[maybe_unused]] void render_sprite_centered(int screen_x, int screen_y, const Sprite& sprite,
                                                 bool flip_h = false);
    void render_sprite_centered_scaled(int screen_x, int screen_y, const Sprite& sprite, int width,
                                       int height, bool flip_h = false);
    // Render a sprite cropped to its top `clip_height` screen-pixels, anchored so the
    // top of the sprite stays at the same position as a full-height blit would.
    // `full_height` is the screen height the complete sprite occupies (used for centering
    // and to compute the proportional source rect). `clip_height` must be <= full_height.
    void render_sprite_top_clip_scaled(int screen_x, int screen_y, const Sprite& sprite, int width,
                                       int full_height, int clip_height, bool flip_h = false);

    // Render a full-screen-sequence sprite stretched over a sub-rect of the
    // letterboxed 320x200 EGA playfield frame (pause overlay, game-over sprite, ...).
    // `(x_ega, y_ega, w_ega, h_ega)` are EGA playfield coordinates; set
    // `present` when this is the last draw of the frame.
    void render_sprite_in_ega_frame(const Sprite& sprite, int x_ega, int y_ega, int w_ega,
                                    int h_ega, bool present = false);

    // Text rendering
    void render_text(int screen_x, int screen_y, const std::string& text, SDL_Color color);

    // Debug rendering
    void render_debug_overlay();

    // Utility: Compute letterboxed destination rect for 320x200 EGA content
    static SDL_Rect compute_letterbox_rect(SDL_Renderer* renderer);

    // Cleanup

private:
    void cleanup();
    SDL_Renderer* renderer;
    bool img_inited;
    bool ttf_inited;
    // Opaque shared handle to the debug font (its TTF_Font is owned inside
    // the pimpl defined in graphics.cpp, attached with a TTF_CloseFont
    // deleter); the header stays SDL_ttf-free on every SDL2 version.
    std::shared_ptr<SdlFontHandle> debug_font;
    std::map<std::string, Tileset> tilesets;
    std::map<std::string, bool> tileset_blackout;
    std::map<std::string, Sprite> sprites;
    std::map<std::string, SpriteAnimationData> enemy_sprites;  // Owns the loaded data

    // Helper functions
    TextureInfo load_png(const std::string& filepath);
    // Load N individual PNG frames: {base_path}-0.png, {base_path}-1.png, ...
    std::vector<TextureInfo> load_animation_frames(const std::string& base_path,
                                                   int expected_frames, const std::string& label);

public:  // made public for testing convenience
    // Determine where an asset file lives on disk by prefixing with the
    // appropriate subdirectory.  This is used internally and also verified by
    // unit tests to prevent regressions when reorganizing the asset tree.
    static std::string get_asset_path(const std::string& filename);

private:
};

// Global graphics system
extern GraphicsSystem* g_graphics;

#endif  // GRAPHICS_H
