# C++ Structure Review & Refactoring Plan

> Review of the code structure in this project (a C++17 SDL2 recreation of the 1988 DOS game
> *Captain Comic*), with a prioritized plan for fixes.
>
> Date: 2026-11-01

---

## 1. Current shape

```
include/   12 headers roughly 1:1 with src/ modules
src/       comic_core static lib (12 modules) + main.cpp exe (1,518 lines)
tests/     custom test framework, 5 test files + shared helpers
tools/     Python asset extraction (extract_assets.py)
```

### What is already right

- Module *boundaries* are sensible: `physics`, `doors`, `actors`, `level_loader`, `audio`,
  `ui_system`, `cheats` each have a header and a cpp.
- `comic_core` as a static library shared by the executable **and** the tests is the right instinct.
- Tile data is auto-generated (`level_tiles.cpp`), and level metadata is hand-authored
  designated-initializer tables (`level_data.cpp`, from `reference/R3_levels.asm`).
- Good supporting hygiene: `.clang-format`, opt-in `.clang-tidy` via `ENABLE_CLANG_TIDY`,
  `CODING_STANDARDS.md`, a test guide under `tests/`, faithfulness comments pointing at the
  `reference/` disassembly.
- Most game modules (`physics`, `doors`, `actors`, `level_loader`) do **not** use SDL directly —
  SDL visibility comes transitively through `graphics.h`. Good discipline at the call sites.

### File size inventory

| File | Lines | Note |
|---|---|---|
| `src/level_tiles.cpp` | 2,006 | auto-generated |
| `src/actors.cpp` | 1,791 | |
| `src/title_sequence.cpp` | 1,775 | mixes 3 concerns (see §5) |
| `src/main.cpp` | 1,518 | does 4 jobs (see §2) |
| `src/graphics.cpp` | 811 | |
| `src/audio.cpp` | 661 | |
| `src/ui_system.cpp` | 569 | |
| `src/level_data.cpp` | 562 | hand-written tables |
| `src/physics.cpp` | 554 | |
| `src/cheats.cpp` | 427 | |
| `src/doors.cpp` | 309 | |
| `src/level_loader.cpp` | 230 | |
| `src/player_teleport.cpp` | 11 | a single function of a feature stranded in main.cpp |

---

## 2. Problem 1 — tangled shared-globals cluster (biggest issue)

Game state is not a module. ~25 mutable globals **defined in `main.cpp:21-101`** (player
position/velocity, input key states, score, lives/HP, level/stage tracking, checkpoints,
teleport state) are consumed by 6 other modules, each re-declaring the subset it wants with
hand-written `extern` blocks:

- `physics.cpp:14-37`, `doors.cpp:30-40`, `level_loader.cpp:20-32`, `actors.cpp:12-18`,
  `graphics.cpp:20-27`, `cheats.cpp:27-38` — several near-duplicate copies of the same extern
  inventory.
- `tests/test_helpers.cpp:11-49` **redefines the entire cluster** so the test link succeeds
  (it swaps in for main.cpp).
- Only `cheat_noclip` (`physics.h:31`), `g_door_anim_phase`/`g_door_anim_frame`/
  `g_skip_load_on_door` (`doors.h`), and the `g_*` system pointers have header homes.

### Coupling map

| Module | Globals touched (read/write) |
|---|---|
| `physics.cpp` | `comic_x/y`, `comic_y_vel`, `comic_x_momentum`, `comic_facing`, `comic_is_falling_or_jumping`, `comic_jump_counter/power`, `key_state_jump/left/right` (+prev), `camera_x`, level/stage numbers+ptr, `source_door_*`, `comic_hp`, `comic_num_lives`, `game_over_triggered`, checkpoints; defines `cheat_noclip` |
| `level_loader.cpp` | `comic_x/y`, `comic_y_vel`, level/stage numbers+ptr, `source_door_*`, checkpoints, `camera_x`, `g_graphics`, `g_actor_system` |
| `doors.cpp` | `comic_x`, `key_state_open`, level/stage ptr+numbers, `source_door_*`; owns `g_door_anim_phase/frame`, `g_skip_load_on_door`; reads `comic_has_door_key` |
| `actors.cpp` | `comic_has_door_key`, `comic_hp(+pending)`, `comic_num_lives`, `score_bytes`, `score_10000_counter`; reads `comic_x/y/facing`, `current_level_ptr` tiles |
| `graphics.cpp` | `comic_x/y`, `comic_y_vel`, `comic_x_momentum`, `comic_facing`, level numbers, `camera_x`, `cheat_noclip` — **the renderer reads gameplay state for its debug overlay** |
| `cheats.cpp` | `comic_x/y/y_vel/x_momentum/is_falling_or_jumping`, `camera_x`, level+stage, `g_actor_system`, `cheat_noclip` — writes all of them |
| `main.cpp` | defines everything; orchestration only |
| `title_sequence.cpp`, `ui_system.cpp` | clean-ish: take `score_bytes` as parameters; only reach out to `g_graphics` |

### Consequences in practice

- **Any new state variable must be written in 3 places** (main.cpp, test_helpers.cpp, plus
  extern blocks in each consumer) — a classic source of "it links but tests see different
  state" bugs.
- Order-of-update coupling is invisible (e.g. renderer/simulation entanglement above).
- The `GameState { Playing, Paused, Victory, GameOver, Exiting }` enum is stranded in
  `main.cpp:68`.

### Plan

- [x] **P1.** Create `include/game_state.h` + `src/game_state.cpp` — one place for all shared
  game-state declarations and definitions. Even if the state stays global for now (to stay
  faithful to the original), a single header removes the copy-drift immediately.
- [x] **P1.** Move the `GameState` enum into `game_state.h`.
- [ ] **P2.** Delete every per-cpp `extern` block in favor of `#include "game_state.h"`.
- [ ] **P2.** Rework `tests/test_helpers.cpp` to define/zero a single state object (
  `reset_physics_state()` becomes "reset one struct").
- [ ] **P3 (aspirational).** Wrap the state in a `GameState` struct owned by main and passed by
  reference.

---

## 3. Problem 2 — `main.cpp` does four jobs (1,518 lines)

Contents today:

1. **All game-state definitions** (see §2) — `main.cpp:21-101`.
2. **Game-loop orchestration** — `int main()` from line 271, loop from ~706: event pump →
   fixed-timestep ticks (~9.1 Hz, mirroring DOS 18.2 Hz / 2) → per-tick pipeline → render →
   FPS cap.
3. **Whole features with no module home:**
   - teleport state machine (`begin_teleport`, `handle_teleport_tick`,
     `process_teleport_input` + 11 teleport globals) — while `player_teleport.cpp` (11 lines)
     holds one pure function of that same feature
   - victory beam-out + score tally, game-over orchestration — giant lambdas inside `main()`
   - lives count-up / HP-fill sequences
   - walk-off-edge floor probing (~lines 985-1010)
4. **Raw SDL texture loading/rendering** for full-screen victory sequences.

### Plan

- [ ] **P2.** Move the teleport state machine (state + `begin_teleport` /
  `handle_teleport_tick` / `process_teleport_input`) into `player_teleport.{h,cpp}`, joining
  the stranded `apply_teleport_destination_if_ready`.
- [ ] **P2.** Move victory/game-over/lives/HP sequences into a new `gameplay.cpp` (or
  `game_sequences.{h,cpp}`).
- [ ] **P3.** Move full-screen sequence texture loading/rendering into `GraphicsSystem`
  helpers so main.cpp keeps no raw SDL.
- [ ] Target: `main.cpp` becomes thin bootstrap + tick pipeline.

---

## 4. Problem 3 — `ActorSystem` is a state container, not a system

`actors.h:97-240` is a 140-line class whose public section is mostly **raw public data
members** (`comic_firepower`, `comic_has_corkscrew`, … and even actor context like
`g_comic_x`, `current_tiles`). Issues:

- Player inventory/HP flags (`comic_has_boots`, `comic_has_lantern`, …) belong to the *player*,
  not the enemy/item manager — which is why `main.cpp:42` keeps its own duplicate
  `comic_has_door_key` alongside `ActorSystem::comic_has_door_key` (`actors.h:135`).
  **Two sources of truth for the same flag.**
- `protected:` data + private helpers suggests encapsulation was started but not finished.

### Plan

- [ ] **P2.** Make inventory flags single-source: either `GameState` or a `Player` struct owns
  items; `ActorSystem` reads it.
- [ ] **P3.** Make `ActorSystem` data private; expose behavior (`take_damage()`,
  `collect_item()`, `get_jump_power()`, …) instead of fields.

---

## 5. Problem 4 — SDL coupling leaks to fix

- `cheats.h` includes `<SDL2/SDL.h>` just for `SDL_Keycode` — cheats should be testable
  without SDL.
- `title_sequence.h` **exposes SDL in a presentation module's public API**
  (`SDL_Texture* get_hud_texture()`), and `title_sequence.cpp` (1,775 lines) mixes three
  concerns:
  1. title/story/items-screen presentation (EGA-palette surfaces)
  2. **input-binding persistence** (`KEYS.DEF`, magic `CCKB1`) — `InputBindings`,
     `DEFAULT_INPUT_BINDINGS`
  3. **high-score persistence** (`COMIC.HGH`, magic `CCHG1`, top-10)
- `GraphicsSystem` owns raw `TTF_Font*` / `SDL_Texture*` / `std::map<std::string,
  SpriteAnimationData*>` (owning raw pointers) with a manual `cleanup()` called from the
  destructor — correct, but fragile; error paths in `load_enemy_sprite` hand-delete textures
  (`graphics.cpp:455-483`).
- Inconsistent access: `ActorSystem` and title functions pass `GraphicsSystem*` explicitly
  (good), while `ui_system.cpp:35,262-274` grabs `g_graphics` directly.

### Plan

- [ ] **P2.** Split `title_sequence.cpp` into three modules: presentation (stays),
  `input_bindings.{h,cpp}`, `high_scores.{h,cpp}`. Persistence is data-domain logic; this
  cuts ~500 lines out of the presentation file.
- [ ] **P3.** Own textures/fonts with `std::unique_ptr` + custom deleters in `GraphicsSystem`;
  fix hand-deletion on error paths.
- [ ] **P3.** Replace `SDL_Keycode` in `cheats.h` with the project's own enum.
- [ ] **P3.** Make `UISystem` take `GraphicsSystem*` explicitly like every other module.
- [ ] **P3.** Hide `TTF_Font*` behind a pimpl so `graphics.h` stops pulling `SDL_ttf.h` for
  every consumer.

---

## 6. Problem 5 — data & build metadata

- **Missing generator:** `level_tiles.cpp:1-10` says it is generated by
  `tools/generate_tiles.cpp` with regeneration instructions — **that file does not exist**
  (`tools/` only has `extract_assets.py`, tests, requirements). Instructions also reference
  the old project name `comic-modernization`.
- CMake forces `comic_core` to C++20 on MSVC only, because `level_data.cpp` uses designated
  initializers (`CMakeLists.txt:123-130`) — a per-platform standard mismatch smell.
- `CMakeLists.txt` hardcodes every source and ~15 `add_test(... --filter ...)` lines
  (175-187+).
- `player_teleport.{h,cpp}` is 11/12 lines — not wrong per se; it is evidence for §3.

### Plan

- [ ] **P1.** Restore or remove the missing tile generator: either re-create
  `tools/generate_tiles.cpp` or fold tile export into `extract_assets.py`; fix the stale
  banner and old project name in `level_tiles.cpp:1-10`.
- [ ] **P2.** Compile `comic_core` uniformly as C++20; drop the MSVC-only special case.
- [ ] **P3.** Move test files to `target_sources()` per directory; keep the explicit
  source list per module rather than a flat hardcoded list.

---

## 7. Problem 6 — test framework friction

The hand-rolled `TestCase { name, fn }` + `--filter` registry is fine at this size, and the
per-test CTest registrations are a nice touch. Friction points:

- The biggest structural cost is the **global-cluster duplication** (§2); a state header makes
  resets trivial.
- `test_main.cpp:8-108` is a manually maintained static registry: each new test requires
  edits in three places (`test_cases.h`, the test file, the registry).

### Plan

- [ ] **P3.** Add a self-registering macro (`REGISTER_TEST(test_name)`) so registering a test
  is a one-line, one-file change.
- [ ] Unblocks automatically once §2 lands.

---

## 8. Problem 7 — header hygiene (small stuff)

- `ui_system.h:9` includes `physics.h` only for `MAX_HP` — move shared constants (HP/lives
  limits) into a game-constants header.
- `level.h` mixes three layers: *file-format structs* (`shp_t`, `stage_t` — the on-disk
  PT/SHP model; well-commented, solid), *gameplay enums* (behaviors, items, level numbers),
  and *global data exports*. Consider splitting data model from enums/exports.
- `physics.h:31` declares `extern bool cheat_noclip` — a cheat flag living in the physics
  header; it belongs with the other cheats.

### Plan

- [ ] **P3.** Add `game_constants.h` (MAX_HP, lives limits, …); slim `ui_system.h`.
- [ ] **P3.** Split `level.h` into format model / enums / data exports (or at least comment
  the sections).
- [ ] **P3.** Move `cheat_noclip` declaration to `cheats.h` once it stops being read directly
  by physics paths.

---

## 9. Priority order (summary)

| Priority | Item | Effort | Payoff |
|---|---|---|---|
| **P1** | `game_state.h` — single home for all shared state | Low | Kills extern copy-drift; unblocks test resets |
| **P1** | Fix/replace missing `generate_tiles.cpp` | Low | Data pipeline reproducible again |
| **P2** | Move teleport + victory/game-over/lives out of `main.cpp` | Medium | main.cpp → thin bootstrap + tick pipeline |
| **P2** | Single source of truth for inventory flags (`comic_has_door_key` duplication) | Low | Removes a likely bug source |
| **P2** | Split `title_sequence` (presentation / input bindings / high scores) | Medium | ~500 lines out; SDL out of persistence |
| **P2** | Uniform C++20 for `comic_core` | Trivial | Removes MSVC-only special case |
| **P3** | SDL out of `cheats.h` / `title_sequence.h`; `GraphicsSystem` unique_ptrs; explicit `GraphicsSystem*` in `UISystem` | Medium | Testability + ownership safety |
| **P3** | Self-registering tests; `game_constants.h`; split `level.h`; pimpl fonts | Low | Ongoing hygiene |

**Suggested sequencing:** do both P1 items first (each is small), then P2 in the order listed
(teleport extraction and the ActorSystem/state true-up pair well with the `game_state.h` work),
then P3 as opportunistic hygiene while touching nearby code.
