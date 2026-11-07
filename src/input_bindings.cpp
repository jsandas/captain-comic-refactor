#include "../include/input_bindings.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>

#include "../include/pref_paths.h"

// ---------------------------------------------------------------------------
// Constants + module state
// ---------------------------------------------------------------------------

static constexpr const char* KEY_BINDINGS_FILENAME = "KEYS.DEF";
static constexpr const char* KEY_BINDINGS_MAGIC = "CCKB1";

const InputBindings DEFAULT_INPUT_BINDINGS = {SDLK_LEFT,  SDLK_RIGHT, SDLK_SPACE,
                                              SDLK_LCTRL, SDLK_LALT,  SDLK_t};

static InputBindings s_input_bindings = DEFAULT_INPUT_BINDINGS;

// ---------------------------------------------------------------------------
// Binding access
// ---------------------------------------------------------------------------

const InputBindings& get_input_bindings() { return s_input_bindings; }

[[maybe_unused]] void reset_input_bindings_to_defaults() {
    s_input_bindings = DEFAULT_INPUT_BINDINGS;
}

void set_input_bindings(const InputBindings& bindings) { s_input_bindings = bindings; }

// ---------------------------------------------------------------------------
// Binding edit/validation helpers (shared with the keyboard setup menu)
// ---------------------------------------------------------------------------

SDL_Keycode canonicalize_binding_key(SDL_Keycode key) {
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

bool key_is_already_assigned(const InputBindings& bindings, SDL_Keycode key,
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

void set_binding_for_action(InputBindings& bindings, int action_index, SDL_Keycode key) {
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

bool input_bindings_equal(const InputBindings& lhs, const InputBindings& rhs) {
    return lhs.move_left == rhs.move_left && lhs.move_right == rhs.move_right &&
           lhs.jump == rhs.jump && lhs.fire == rhs.fire && lhs.open_door == rhs.open_door &&
           lhs.teleport == rhs.teleport;
}

bool validate_input_bindings(const InputBindings& bindings, std::string* error_message) {
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

// ---------------------------------------------------------------------------
// Runtime key matching (main loop)
// ---------------------------------------------------------------------------

bool key_matches_binding(SDL_Keycode key, SDL_Keycode binding) {
    if (key == binding) {
        return true;
    }

    // Treat left/right modifier variants as one DOS-style key class.
    switch (binding) {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            return key == SDLK_LCTRL || key == SDLK_RCTRL;
        case SDLK_LALT:
        case SDLK_RALT:
            return key == SDLK_LALT || key == SDLK_RALT;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            return key == SDLK_LSHIFT || key == SDLK_RSHIFT;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// KEYS.DEF persistence
// ---------------------------------------------------------------------------

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
