# Captain Comic Modernization Plan

## Project Overview

This project modernizes the classic 1988 DOS game *The Adventures of Captain Comic* for modern systems. The goal is to recreate the game using modern technologies while maintaining behavioral fidelity to the original.

**Original Game:**
- Platform: DOS (x86-16 assembly)
- Graphics: EGA 320x200, 16 colors
- Input: Keyboard/joystick
- Audio: PC speaker

**Target Platforms:**
- Windows, macOS, Linux
- Modern resolutions with optional scaling
- Modern input devices
- Cross-platform audio

## Technology Stack

### Core Libraries
- **SDL2**: Graphics, audio, input handling
  - Chosen for performance, broad compatibility, and low-level control
  - Better for faithful DOS emulation than SFML
  - Extensive features and mature ecosystem

- **C++17**: Modern language features while maintaining C compatibility
  - Easier to port from C refactor
  - Object-oriented design for maintainability

- **CMake**: Cross-platform build system
  - Simple configuration
  - IDE integration

### Reference Materials

Primary reference: [jsandas/comic-c](https://github.com/jsandas/comic-c)
- C refactor of the original assembly (Phase 4 complete)
- Structured, readable game logic
- Complete implementation of core systems
- Documents: REFACTOR_PLAN.md, GAME_LOOP_FLOW.md, CODING_STANDARDS.md

Secondary reference: Assembly disassembly
- Low-level details for fidelity
- Exact constants and edge cases
- Located in reference/disassembly/R5sw1991.asm

## Reference Code Structure

From jsandas/comic-c, key modules to port:

### Core Systems (src/)
- `game_main.c` - Entry point, game loop, level loading
- `physics.c` - Gravity, jumping, movement, collision
- `actors.c` - Enemies, fireballs, items
- `graphics.c` - Rendering, sprites, video buffers
- `sound.c` / `music.c` - PC speaker sound, music playback
- `doors.c` - Door system, level transitions
- `level_data.c` - All 8 levels: LAKE (0), FOREST (1 - first playable), SPACE, BASE, CAVE, SHED, CASTLE, COMP
- `file_loaders.c` - Asset loading (.PT, .TT2, .SHP, .EGA files)

### Headers (include/)
- `globals.h` - Shared constants and state
- `physics.h` - Physics constants (GRAVITY=5, ACCELERATION=7, etc.)
- `actors.h` - Enemy behaviors, fireball mechanics
- `level_data.h` - Level structures and data

## Implementation Phases

### Phase 1: Foundation
**Status:** Complete
**Completion Date:** 2026-02-12
**Goal:** Set up development environment and basic infrastructure

- [x] Initialize git repository
- [x] Create CMake build system
- [x] Install SDL2 dependencies
- [x] Create basic window and event loop
- [x] Implement input handling (keyboard)
- [x] Verify SDL2 linkage and execution

**Deliverable:** Application opens window, processes input, exits cleanly

---

### Phase 2: Core Physics
**Status:** Complete
**Completion Date:** 2026-02-12
**Goal:** Implement player movement and physics

**Tasks:**
- [x] Port physics constants from physics.h (GRAVITY, TERMINAL_VELOCITY, JUMP_ACCELERATION)
- [x] Implement `handle_fall_or_jump()` from physics.c
  - [x] Gravity application
  - [x] Jump counter logic
  - [x] Velocity integration
  - [x] Ceiling collision
  - [x] Ground collision with tile detection
- [x] Implement `move_left()` and `move_right()` from physics.c
  - [x] Wall collision detection
  - [x] Camera following
  - [x] Movement mechanics (stage transitions deferred to Phase 4)
- [x] Port tile map system
  - [x] `address_of_tile_at_coordinates()` (implemented as `get_tile_at()`)
  - [x] `is_tile_solid()`
  - [x] Tile data structures
  - [x] Test level with platforms and walls

**Reference Code:**
- `src/physics.c`: Lines 59-398 (handle_fall_or_jump, movement functions)
- `include/physics.h`: Constants and declarations
- `include/globals.h`: MAP_WIDTH, PLAYFIELD_WIDTH, etc.

**Success Criteria:**
- [x] Player falls realistically with gravity
- [x] Jumping height matches original (constants validated)
- [x] Wall collision prevents movement through walls
- [x] Floor detection works accurately
- [x] Camera follows player smoothly
- [x] Ceiling collision stops upward movement
- [ ] Stage transitions at boundaries (deferred to Phase 4)

---

### Phase 3: Rendering System
**Status:** Complete
**Completion Date:** 2026-02-12
**Goal:** Display game graphics

**Tasks:**
- [x] Port tile rendering from graphics.c
  - [x] Load PNG tileset files (converted from .TT2)
  - [x] Render tile maps to screen
  - [x] Implemented with SDL2 textures (hardware-accelerated)
- [x] Port sprite system from graphics.c
  - [x] Load PNG sprite files
  - [x] Blit sprites with proper rendering
  - [x] Animation frame handling with frame timing
- [x] Implement camera system
  - [x] Viewport scrolling with camera position
  - [x] Player-centered camera (camera follows player)
  - [x] Tile culling for performance (only render visible tiles)
- [x] Port EGA palette to SDL
  - [x] PNG files already in RGB format (no conversion needed)
- [x] Render player sprite
  - [x] Standing animation (idle)
  - [x] Running animation (3-frame cycle)
  - [x] Jumping animation
  - [x] Direction facing based on input

**Implementation Details:**
- Created `graphics.h` and `graphics.cpp` with `GraphicsSystem` class
- `GraphicsSystem` handles texture loading, sprite management, and animation
- Asset loader supports multiple search paths for flexibility
- SDL2_image (libpng) for PNG loading
- Animation system with frame-based timing
- Tile culling: only renders tiles within camera viewport

**Reference Code Used:**
- Asset organization from jsandas/comic-c assets
- Sprite naming conventions: `sprite-{name}_{direction}.png`
- Tileset naming: `{level}.tt2-{id}.png` (0x00-0x3F per level)

**Data Files Used:**
- Pre-converted PNG tilesets for all 8 levels (lake, forest, space, base, cave, shed, castle, comp)
- PNG sprite files for player animations (left/right facing)
- Player sprites: standing, running (3 frames), jumping

**Success Criteria:**
- [x] Tiles render correctly from PNG files
- [x] Sprites display with proper positioning
- [x] Animations play smoothly with configurable frame timing
- [x] Camera follows player appropriately, showing 24x20 game unit viewport
- [x] Player sprite changes based on state (idle/running/jumping)
- [x] Player sprite faces direction of movement
- [x] No rendering artifacts or flicker (hardware-accelerated)

---

### Phase 4: Level System
**Status:** Complete
**Completion Date:** 2026-02-12
**Goal:** Load and manage game levels

**Tasks:**
- [x] Port level data structures from level_data.h
  - [x] `level_t` structure (with filenames, door tiles, sprite descriptors)
  - [x] `stage_t` structure (with items, exits, doors, enemies)
  - [x] Door definitions
  - [x] Enemy record definitions
  - [x] Sprite descriptor (`shp_t`) definitions
- [x] Implement level loading from level_data.c
  - [x] All 8 levels (LAKE through COMP) with complete data
  - [x] Stage data (3 stages per level) with exact values
  - [x] Enemy spawn data for all stages
  - [x] Item placement for all stages
- [x] Port file loaders from file_loaders.c (NO LONGER NEEDED)
  - [x] ~~.PT file loader~~ → Tile maps compiled as C++ hex arrays
  - [x] ~~.TT2 file loader~~ → Tilesets converted to PNG
  - [x] ~~.SHP file loader~~ → Sprites converted to PNG/GIF
  - [x] ~~.EGA file loader~~ → System graphics converted to PNG
  - **Note:** All binary formats pre-converted to modern formats (PNG/GIF)
  - **Note:** Tile data embedded in executable as C++ arrays (stage 2 migration)
- [x] Implement door system from doors.c
  - [x] Door collision detection
  - [x] Key checking
  - [x] Level/stage transitions (stubs for now)

**Reference Code:**
- `src/level_data.c`: All level definitions
- `src/file_loaders.c`: Asset loading
- `src/doors.c`: Door mechanics
- `src/physics.c`: Stage boundary transition logic

**Success Criteria:**
- [x] All 8 levels load correctly
- [x] Stage transitions work (left/right boundaries)
- [x] Doors function properly with keys
- [x] Level data matches original exactly

**Implementation Notes:**
- Door collision detection checks Y coordinate (exact) and X coordinate (within 3 units)
- Door Key requirement implemented
- Door transitions call load_new_level() or load_new_stage() as appropriate
- Source door tracking for reciprocal positioning (entering target stage via door)
- **Unit-test helper:** `g_skip_load_on_door` flag allows activate_door() to update
  state without actually loading levels/stages, keeping tests deterministic
  and avoiding reciprocal-door warnings when using minimal test levels.
- Alt key (SDL KMOD_ALT modifier) opens doors; 'K' key grants door key (debug)
- Level/stage loading system integrated:
  - load_new_level() sets current_level_ptr and loads tileset graphics
  - load_new_stage() loads stage tiles into physics system (current_tiles)
  - Reciprocal door search finds matching door in destination stage
  - Camera positioned relative to Comic on stage load
  - Initial spawn: Forest stage 0 at position (14, 12)
- Stage boundary transitions:
  - Triggered at left edge (comic_x == 0) or right edge (comic_x >= 254)
  - Checks stage->exit_l or stage->exit_r for target stage
  - If EXIT_UNUSED (0xFF), stops movement
  - Otherwise, transitions to new stage at opposite edge
  - Checkpoint updated to preserve Y position and edge position
  - source_door_level_number = -1 marks as boundary (not door) transition

---

## Developer Tools & Debug Features

**Status:** Complete
**Implementation Date:** 2026-02-15
**Goal:** Provide development and debugging aids for testing and validation

### CheatSystem Class
A centralized system for managing debug cheats and development tools. Activated via `--debug` command-line flag; all cheats remain silently disabled without it.

**Architecture:**
- Implemented as `CheatSystem` class following RAII pattern (matching `GraphicsSystem`)
- Global instance `g_cheats` initialized at startup
- Input processed during main game loop after regular keyboard handling
- Command-line parsing in `main()` for debug mode activation

**Features:**

1. **Noclip Mode (F1)**
   - Disables collision detection entirely
   - Player walks through all solid tiles and obstacles
   - Useful for: Testing level layouts, debugging physics edge cases, exploring level design

2. **Level Warp (F2)**
   - Interactive two-step menu: select level (0-7), then stage (0-2)
   - Levels: LAKE (0), FOREST (1), SPACE (2), BASE (3), CAVE (4), SHED (5), CASTLE (6), COMP (7)
   - Resets player position to safe spawn (20, 14) on warp
   - Clears velocity and jump state
   - Useful for: Testing specific levels without playing through, accessing late-game content

3. **Position Warp (F4)**
   - Teleport player to specific X,Y coordinates
   - Input validation: X (0-255), Y (0-19)
   - Auto-adjusts camera to keep player on-screen
   - Resets velocity
   - Useful for: Testing collision at specific tiles, placing player near objectives, testing boundaries

4. **Debug Overlay (F3)**
   - Semi-transparent overlay in top-left corner displaying:
     - Player X,Y coordinates (in cyan text)
     - Current level and stage (in cyan text)
     - Visual velocity bar (red, vertical)
     - Visual momentum bar (blue, horizontal)
     - Noclip indicator (green square when active)
   - Uses SDL2_ttf for text rendering with system font fallback chain
   - Useful for: Verifying position during complex maneuvers, monitoring physics state, visual feedback on cheat status

5. **Item Granting (F5)**
   - Interactive menu to select and grant any item (0-9)
   - Items available:
     - 0 = Corkscrew (fireball vertical oscillation)
     - 1 = Door Key (unlock doors)
     - 2 = Boots (increased jump power)
     - 3 = Lantern (castle lighting)
     - 4 = Teleport Wand (special teleport)
     - 5 = Gems (treasure 1/3)
     - 6 = Crown (treasure 2/3)
     - 7 = Gold (treasure 3/3)
     - 8 = Blastola Cola (increase firepower)
     - 9 = Shield (HP refill)
   - Useful for: Testing item effects without playing through levels, debugging item-dependent mechanics, validating item collection logic

**Console Logging:**
- All cheat activations logged to stdout with `[CHEAT]` prefix
- Input validation feedback (e.g., invalid coordinates)
- Level/stage warp confirmations
- Door key grant/removal notifications
- Users can see cheat activity in terminal even if overlay disabled

**Command-Line Interface:**
```bash
./captain_comic --debug    # Enable all cheats and F-key controls
./captain_comic --help     # Display help message
./captain_comic            # Normal mode (cheats disabled)
```

**Implementation Details:**
- Multi-step input handling for complex cheats (level warp, position warp, item granting)
- Input buffering for coordinate entry with backspace support
- Console prompts guide users through multi-step operations
- Proper cleanup of system resources (SDL_ttf font) in destructor
- Font loading with cross-platform fallback (Menlo, Courier on macOS; DejaVuSansMono on Linux; lucidaconsole on Windows)
- Item granting directly calls `ActorSystem::apply_item_effect()` for all 10 item types

**Technical Specs:**
- Dependencies: SDL2_ttf (added to CMakeLists.txt)
- Code Coverage:
  - New files: `include/cheats.h`, `src/cheats.cpp`
  - Modified: `src/main.cpp` (command-line parsing, cheat input integration)
  - Modified: `src/physics.cpp`, `include/physics.h` (noclip flag implementation)
  - Modified: `src/graphics.cpp`, `include/graphics.h` (debug overlay rendering, text support)
  - Modified: `CMakeLists.txt` (cheats.cpp, SDL2_ttf)

**Testing:**
- [x] Builds without warnings or errors
- [x] All existing tests pass
- [x] Cheats silently ignored when --debug not specified
- [x] Debug overlay font loads successfully
- [x] Position validation prevents out-of-bounds warping

**Future Enhancements:**
- God mode (invincibility) - deferred to when damage/HP system exists
- Enemy spawner/despawner controls
- Speed-up/slow-down time controls
- Collision visualization (show collision boxes)

---

### Phase 5: Actor System
**Status:** Complete
**Completion Date:** 2026-02-22
**Goal:** Implement enemies, fireballs, and items

**Phase 5.1: Enemy System Core**
**Objective:** Port complete enemy spawning, AI behaviors, collision, and despawning systems from reference C code

**Completed:**
- [x] Created `ActorSystem` class with core architecture
- [x] All 5 AI behaviors fully implemented: Bounce, Leap, Roll, Seek, Shy
- [x] Enemy-player collision detection and death animation states
- [x] Enemy spawning/despawning and respawn cycling

**Phase 5.2: Sprite/Animation Loading**
- [x] Implemented `load_enemy_sprite()` in `GraphicsSystem` with GIF-based asset loading
- [x] Handle horizontal duplication and separate facing frames
- [x] Support for loop and ping-pong animation sequences

**Phase 5.3: Enemy Rendering**
- [x] Implemented `render_enemies()` in `ActorSystem`
- [x] Camera culling and proper draw order

**Phase 5.4: Integration**
- [x] `ActorSystem` initialized in `main()` and updated each physics tick
- [x] Stage/level transition handling

**Phase 5.5: Fireball System**
- [x] Fireball spawning and movement (including corkscrew)
- [x] Fireball-enemy collision and meter management

**Phase 5.6: Item System**
- [x] Item collection tracking and effects (Blastola Cola, Boots, etc.)
- [x] Item rendering and two-frame animation

**Reference Code:**
- `src/actors.c`: All actor systems
- `include/actors.h`: Data structures and constants

**Success Criteria:**
- [x] All enemy behaviors work correctly
- [x] Enemies spawn at proper distances
- [x] Fireballs kill enemies on contact
- [x] Items grant correct abilities
- [x] Treasure collection triggers win sequence

---

### Phase 6: Audio System
**Status:** Complete
**Completion Date:** 2026-02-27
**Goal:** Implement sound effects and music

**Phase 6.1: Audio Foundation**
- [x] Set up SDL_mixer for audio
- [x] Synthesized PC-speaker-style square-wave SFX generation
- [x] Single-channel SFX priority system

**Phase 6.2: Sound Definitions & Event Mapping**
- [x] Ported 10 core PC-speaker sound definitions (Fire, Item Collect, Door Open, etc.)
- [x] Integrated sound triggers into game events

**Phase 6.3: Music System**
- [x] Ported title music melody with full looping support
- [x] Dedicated music channel independent from SFX
- [x] Music control API and note frequency coverage

**Technical Details:**
- Audio frequency conversion: Original PIT divisors → modern Hz values
- Multi-frequency synthesis for note sequences
- Priority system: Higher priority sounds interrupt lower priority ones

**Reference Implementation:**
- Based on jsandas/comic-c sound.c and sound_data.c
- PC speaker square-wave synthesis recreated in SDL2 audio

---

### Phase 7: UI and Menus
**Status:** Complete
**Completion Date (7.1):** 2026-03-07
**Completion Date (7.2):** 2026-03-07
**Goal:** Implement game menus and HUD

**Phase 7.1: Title Sequence System**
**Objective:** Port complete title sequence from original game with proper visual transitions and asset loading

**Completed:**
- [x] Title sequence function structure
  - [x] Load and display title screen (SYS000.EGA) with palette fade-in
  - [x] Load and display story screen (SYS001.EGA) with palette fade-in
  - [x] Load game UI background (SYS003.EGA) into HUD texture for gameplay
  - [x] Load and display items/controls screen (SYS004.EGA)
  - [x] Proper blocking on keypresses between screens
  - [x] Title music playback (integrated with Phase 6 audio system)
- [x] Palette fade effects
  - [x] 6-step palette transition sequence mimicking DOS palette_fade_in()
  - [x] EGA color conversion (6-bit → 8-bit RGB)
  - [x] Multi-step color animation for smooth visual transitions
  - [x] Non-fatal handling for missing/invalid palette data (continue without fade)
- [x] Asset loading
  - [x] Load paletted PNG files (320×200 EGA graphics)
  - [x] Preserve indexed color format with palette
  - [x] Convert to RGBA textures for SDL rendering
  - [x] Proper error handling and logging
- [x] Timing and input handling
  - [x] Title screen auto-advances after ~770 ms (14 ticks × 55 ms)
  - [x] Story and items screens wait for keypress (blocking)
  - [x] Event polling during fade sequences allows SDL_QUIT detection
  - [x] Letterbox rendering for 320×200 content on modern resolutions
- [x] Integration with game loop
  - [x] `run_title_sequence()` called at startup
  - [x] `get_hud_texture()` returns preserved SYS003.EGA for in-game HUD rendering
  - [x] HUD texture rendered as background layer during gameplay
  - [x] Playfield viewport (8,8 offset, 192×160 size) matches original HUD layout
  - [x] `--skip-title` command-line flag bypasses sequence for development speed
  - [x] `cleanup_title_sequence()` properly releases loaded surfaces and textures
- [x] Created `include/title_sequence.h` and `src/title_sequence.cpp`
- [x] Created `TITLE_SEQUENCE.md` with detailed documentation

**Technical Implementation:**
- Paletted PNG loading preserves EGA palette data (8-bit indexed color)
- Letterbox calculation centralized in `GraphicsSystem::compute_letterbox_rect()`
- Fade sequence renders 6 frames with 55ms delays (matching original tick rate)
- Multi-step palette color transitions:
  - Step 1: All colors → dark gray (0x18)
  - Step 2: All colors → light gray (0x07)
  - Step 3: Background stays light gray, items/title → white (0x1f)
  - Step 4: Background → dark green (0x02), items → bright green (0x1a)
  - Step 5: Title → bright red (0x1c), background/items restored
  - Step 6: All restored to original colors
- Surface format conversion: paletted → RGBA32 for SDL rendering

**Reference Implementation:**
- Based on jsandas/comic-c `game_main.c:title_sequence()`
- Original palette fade sequence fully replicated
- Asset timing matches original (~770ms title display, instant story/items)

**Phase 7.2: HUD/UI System**
**Objective:** Port complete HUD rendering system with score, lives, meters, and inventory from reference implementation

**Completed:**
- [x] Created `UISystem` class with comprehensive HUD management
  - [x] Score display rendering (6-digit base-100 encoded system)
  - [x] Lives counter with bright/dark icon states (0-5 lives)
  - [x] HP meter with 6 cells (0-6 HP visualization)
  - [x] Fireball meter with 6 cells (0-12 value mapping with half/full states)
  - [x] Complete inventory grid rendering (3x3 layout)
- [x] Score system implementation
  - [x] Base-100 to decimal conversion (3 bytes → 6 digits)
  - [x] Score digit sprite loading (0-9)
  - [x] Proper rendering alignment (right-aligned at X=232-280)
  - [x] Static helper `score_bytes_to_digits()` for testing
- [x] Lives display implementation
  - [x] Life icon sprites (bright/dark variants)
  - [x] 5-icon display with active/inactive states
  - [x] Proper spacing (24px intervals starting at X=48)
- [x] HP meter implementation
  - [x] 6-cell meter with full/empty sprite states
  - [x] Right-aligned at Y=82, X=248-296
  - [x] HP value directly controls cell fill count
- [x] Fireball meter implementation
  - [x] 6-cell meter with full/half/empty sprite states
  - [x] 0-12 meter value mapped to cell states
  - [x] Right-aligned at Y=54, X=248-296
  - [x] Static helper `fireball_meter_to_cell_state()` for cell mapping logic
- [x] Inventory system implementation
  - [x] 9 inventory items with sprite loading
    - [x] Blastola Cola (firepower indicator, animated)
    - [x] Corkscrew (fireball oscillation upgrade)
    - [x] Door Key (unlock doors)
    - [x] Boots (increased jump power, static helper `has_boots()`)
    - [x] Lantern (castle lighting)
    - [x] Teleport Wand (special teleport)
    - [x] Gems (treasure 1/3)
    - [x] Crown (treasure 2/3)
    - [x] Gold (treasure 3/3)
  - [x] Two-frame animation system (even/odd frames)
  - [x] Animation counter toggling each game tick
  - [x] 3x3 grid layout (Y=112/136/160, X=232/256/280)
- [x] Sprite asset loading
  - [x] Score digit sprites (`score_digit_0` through `score_digit_9`)
  - [x] Life icon sprites (`life_icon_bright`, `life_icon_dark`)
  - [x] Meter sprites (`meter_full`, `meter_half`, `meter_empty`)
  - [x] Item sprites with animation frames (`<item>_even`, `<item>_odd`)
  - [x] Non-directional sprite loading (empty direction parameter)
- [x] Integration with game loop
  - [x] `UISystem` instantiated in `main()`
  - [x] `initialize()` called at startup (loads all sprites)
  - [x] `update()` called each game tick (toggles animation counter)
  - [x] `render_hud()` called each frame with complete game state
  - [x] Passes all player stats: score, lives, HP, fireball meter, firepower, inventory flags
- [x] Testing and validation
  - [x] Unit test: `test_ui_score_base100_encoding` (score conversion accuracy)
  - [x] Unit test: `test_ui_fireball_meter_cell_mapping` (meter cell state logic)
  - [x] Unit test: `test_ui_boots_detection` (boots detection from jump power)
  - [x] All tests passing
- [x] Created `include/ui_system.h` and `src/ui_system.cpp`
- [x] Updated `CMakeLists.txt` to build ui_system.cpp

**Technical Implementation:**
- Score encoding: 3 base-100 bytes represent 0-999,999 scores
  - Each byte (0-99) converts to 2 decimal digits
  - Rightmost byte is least significant (ones/tens)
  - Rendering order: most significant → least significant (left to right)
- Lives icons: 16×16px sprites scaled to render coordinates
  - Bright icon for active lives, dark icon for lost/unavailable lives
- HP Meter: 6 cells at 8×16px each, right-aligned
  - Simple threshold: cell N filled if HP > N
- Fireball Meter: 6 cells representing 0-12 meter value
  - Cell 0 = meter 1-2, Cell 1 = 3-4, ..., Cell 5 = 11-12
  - Odd values show half-filled cell, even values show full cell
- Inventory animation: toggles between even/odd frame each tick (~9 Hz)
- Sprite rendering: uses `render_sprite_scaled()` with fixed sizes (16×16 for items, 8×16 for meters/digits)
- RAII pattern: sprites owned by GraphicsSystem, UISystem holds non-owning pointers
  - `cleanup()` only clears vectors, does not destroy textures

**Reference Code:**
- Based on jsandas/comic-c `game_main.c` (HUD rendering logic)
- Based on jsandas/comic-c `graphics.c` (sprite positioning)

**Success Criteria:**
- [x] Score displays correctly with 6 digits
- [x] Lives icons show active/inactive states
- [x] HP meter accurately reflects player health
- [x] Fireball meter shows charge state with half/full cells
- [x] Inventory items appear when collected
- [x] Inventory animations toggle smoothly
- [x] All sprites load without errors
- [x] Unit tests pass for helper functions
- [x] HUD renders every frame at correct positions
- [x] No rendering artifacts or performance issues

---

**Remaining Tasks (Phase 7.3+):**
- [x] Implement menus from game_main.c
  - [x] Startup notice (initial acknowledgement screen)
  - [x] High scores screen (leaderboard display)
  - [x] Keyboard setup menu (control configuration)
  - [x] Pause menu (in-game pause functionality)
- [x] Port special sequences
  - [x] Beam-in animation (entering level/stage)
  - [x] Beam-out animation (exiting level/stage to victory)
  - [x] Death animation (player death sequence)
  - [x] Victory sequence (treasure collection celebration)
  - [x] Game over screen (game-over state display)

**Reference Code:**
- `src/game_main.c`: UI and menu functions
- `src/graphics.c`: Text rendering, fullscreen graphics

**Success Criteria Phase 7.1:**
- [x] Title sequence displays in correct order
- [x] Palette fades smoothly (6 steps at 55ms each)
- [x] All screens display correctly
- [x] Title music plays during title screen
- [x] Input advances story and items screens
- [x] HUD texture preserved for gameplay rendering
- [x] No crashes or memory leaks
- [x] Seamless transition to gameplay

**Success Criteria (Full Phase 7):**
- [x] HUD displays correctly during gameplay with all elements
  - [x] Score (6 digits) renders at correct position
  - [x] Lives (0-5 icons) show active/inactive states
  - [x] HP meter (6 cells) reflects player health
  - [x] Fireball meter (6 cells) shows charge with half/full states
  - [x] Inventory items (9 items) appear when collected with animations
- [x] All menus are functional and match original
- [x] Startup notice and high scores screens work
- [x] Pause menu functions properly
- [x] Beam-in/beam-out animations play correctly
- [x] Death and victory sequences work
- [x] Game state transitions are smooth

---

### Phase 8: Game Loop Integration
**Status:** Complete
**Completion Date:** 2026-05-17
**Goal:** Complete game flow and state management

**Tasks:**
- [x] Port main game loop from game_main.c
  - [x] Tick-based timing (engine uses DOS-faithful ~9.1 Hz logic ticks + 60 FPS render cap)
  - [x] Input processing order
  - [x] Physics update
  - [x] Actor update
  - [x] Rendering
- [x] Implement game states
  - [x] Menu state
  - [x] Playing state
  - [x] Paused state
  - [x] Game over state
  - [x] Victory state

**Implementation Progress Notes:**
- Main loop uses an explicit runtime `GameState` enum (`Playing`, `Paused`, `Victory`, `GameOver`, `Exiting`).
- Pause/resume and terminal flow (victory/game-over sequence handoff) transition through `GameState`.
- Teleportation, stage boundaries, and door transitions are fully integrated into the tick pipeline.
- DOS-faithful victory countdown and beam-out sequence implemented.

**Success Criteria:**
- Game loop runs at consistent 60 FPS
- All game states transition correctly
- Full playthrough possible from start to end
- Win/lose conditions work properly

---

### Phase 9: Polish and Testing
**Status:** To be started
**Current Stage:** Phase 9.1 - Validation and Testing
**Completion Date:** TBD
**Goal:** Ensure quality and faithfulness

**Tasks:**
- [X] Validate physics against original
  - [X] Jump height measurements
  - [x] Fall speed verification
  - [x] Movement speed checks
- [x] Test all levels and stages
  - [x] Completability
  - [x] No softlocks
  - [x] Item accessibility

**Testing Resources:**
- DOSBox for side-by-side comparison
- Save states for scenario testing
- Assembly reference for exact behavior

**Success Criteria:**
- Game feels identical to original
- No major bugs or crashes
- Performance meets targets
- Playable on all target platforms

---

### Phase 10: Documentation and Release
**Status:** Not Started
**Completion Date:** TBD
**Goal:** Prepare for distribution

**Tasks:**
- [x] Write comprehensive README
  - [x] Building instructions
  - [x] Running the game
  - [x] Controls
  - [x] Credits
- [x] Create build scripts for all platforms
  - [x] Windows — handled by `release.yml` via vcpkg + Visual Studio 17 2022
  - [x] macOS — handled by `release.yml` via Homebrew SDL2 packages
  - [x] Linux — handled by `release.yml` via apt SDL2 packages
  - Note: No standalone shell scripts are needed; local builds normally use `cmake --preset default`, which requires CMake 3.23+; README should call out that minimum version and also document a non-preset configure/build path for older CMake versions. CI/release builds live in `.github/workflows/`
- [x] Package assets
  - Original game assets are copyrighted and cannot be bundled
  - [x] Asset extraction tooling (`tools/extract_assets.py` + `tools/requirements.txt`) ships in every release archive
  - [x] `ASSET_EXTRACTION.md` is generated into each release package with step-by-step instructions
  - [x] README documents the extraction workflow for local builds
- [x] License considerations
  - [x] Code licensing: MIT — permissive license covering the C++17 source code
    in this repository. See `LICENSE` in the repo root.
    - Alternative considered: GPL v2/v3 (used by Chocolate Doom, Quakespasm,
      etc.) would require derivative works to remain open-source. MIT was chosen
      for simplicity and maximum reuse.
  - [x] Asset ownership: All original game assets (graphics, sounds, level data,
    tile maps) are Copyright 1990 by Michael A. Denio. This project does not
    distribute those assets. Users must supply original game files; see README
    and the extraction tooling in `tools/`.
- [x] Distribution
  - [x] GitHub releases — `release.yml` builds and publishes binaries on `v*` tag push
  - [x] Binaries for each platform — linux-x64, macos-arm64, windows-x64
  - [x] Installation instructions — covered in README (Quick Start + Assets sections)

**Deliverables:**
- Complete, playable game
- Source code on GitHub
- Binary releases
- Documentation

---

## Development Workflow

### Iteration Strategy
1. Implement small, testable modules
2. Validate against C refactor reference
3. Compare behavior to original (DOSBox)
4. Fix discrepancies before moving on
5. Commit working increments frequently

### Testing Approach
- **Unit Testing:** Individual functions (physics, collision)
- **Integration Testing:** Subsystems working together
- **Scenario Testing:** Specific game situations
- **Playthrough Testing:** Full level completion
- **Regression Testing:** Ensure fixes don't break existing features

### Reference Workflow
1. Read function in `src/*.c` from comic-c
2. Understand logic and data structures
3. Port to modern C++ with SDL2
4. Cross-reference assembly for edge cases
5. Test behavior matches original

## Key Design Decisions

### Architecture
- **Modular Design:** Separate physics, rendering, actors, audio
- **Data-Driven:** Levels defined in data structures, not code
- **Modern Idioms:** Use C++ features for clarity (classes, vectors)
- **Compatibility Layer:** Abstract DOS-specific code (video memory, interrupts)

### Rendering
- **Native Resolution:** Render at original 320x200 internally
- **Scaling:** Scale up to modern resolutions (640x480, 1280x960, etc.)
- **Filtering:** Nearest-neighbor for pixel-perfect look
- **SDL_Renderer:** Use hardware acceleration when available

### Input
- **Keyboard Primary:** Arrow keys + Space (jump), Alt (open), etc.
- **Configurable:** Allow remapping
- **Gamepad Support:** Optional, map to modern controllers

### Audio
- **SDL_mixer:** Modern audio library
- **Synthesized Sounds:** Recreate PC speaker beeps/tones
- **Music Playback:** Port original music data or recreate

### Performance
- **Target:** Solid 60 FPS on modest hardware
- **Optimization:** Only where needed (profiling-driven)
- **Memory:** Keep original footprint small, modern extras optional

## File Organization

```
comic-modernization/
├── CMakeLists.txt           # Build configuration
├── README.md                # User documentation
├── docs/                    # Project documentation
│   ├── MODERNIZATION_PLAN.md # This file
│   └── ...
├── .gitignore               # Git ignore rules
│
├── src/                     # Source code
│   ├── main.cpp             # Entry point and game loop
│   ├── physics.cpp          # Physics engine (COMPLETE)
│   ├── game.cpp             # Main game class (TODO)
│   ├── actors.cpp           # Enemies, fireballs, items (TODO)
│   ├── graphics.cpp         # Rendering (TODO)
│   ├── audio.cpp            # Sound and music (TODO)
│   ├── level.cpp            # Level loading and management (TODO)
│   ├── input.cpp            # Input handling (TODO)
│   └── ...                  # Additional modules
│
├── include/                 # Header files
│   ├── physics.h            # Physics constants and functions (COMPLETE)
│   ├── game.h
│   ├── actors.h
│   ├── graphics.h
│   ├── audio.h
│   ├── level.h
│   ├── input.h
│   └── ...
│
├── assets/                  # Game assets
│   ├── levels/              # Level data files
│   ├── graphics/            # Tilesets, sprites
│   ├── audio/               # Sound effects, music
│   └── ...
│
├── build/                   # Build output (generated)
│   └── captain_comic        # Executable
│
└── docs/                    # Additional documentation
    ├── PHYSICS.md           # Physics system details
    ├── RENDERING.md         # Rendering system details
    └── ...
```

## Success Criteria

### Functional Requirements
- [x] Game launches on Windows, macOS, Linux
- [ ] All 8 levels playable from start to finish
- [ ] Physics matches original (jump height, fall speed, movement)
- [ ] All enemies behave correctly (5 AI types)
- [x] All items work (firepower, treasures, keys, etc.)
- [x] Sound effects and music play
- [x] Menus and UI function properly
- [x] Win/lose conditions trigger correctly

### Quality Requirements
- [ ] Runs at 60 FPS on target hardware
- [x] No crashes or major bugs
- [x] Clean, maintainable code
- [ ] Comprehensive documentation

### Fidelity Requirements
- [x] Game looks like original (pixel-perfect at native res)
- [ ] Game feels like original (physics constants exact)
- [x] Game sounds like original (PC speaker recreation)
- [x] Level layouts identical
- [ ] Enemy behavior identical

## Resources

### Primary References
- [jsandas/comic-c](https://github.com/jsandas/comic-c) - C refactor (main reference)
- Assembly disassembly in reference/ folder - Low-level details

### Documentation
- [docs/REFACTOR_PLAN.md](https://github.com/jsandas/comic-c/blob/main/docs/REFACTOR_PLAN.md)
- [docs/GAME_LOOP_FLOW.md](https://github.com/jsandas/comic-c/blob/main/docs/GAME_LOOP_FLOW.md)
- [docs/CODING_STANDARDS.md](https://github.com/jsandas/comic-c/blob/main/docs/CODING_STANDARDS.md)

### Libraries
- [SDL2](https://www.libsdl.org/) - Graphics, audio, input
- [SDL_mixer](https://www.libsdl.org/projects/SDL_mixer/) - Audio playback
- [CMake](https://cmake.org/) - Build system

### Tools
- DOSBox - Running original for comparison
- Compiler: GCC/Clang/MSVC (C++17)
- IDE: VS Code, Visual Studio, Xcode, CLion

## Notes

### Design Philosophy
- **Fidelity First:** Match original behavior exactly before adding enhancements
- **Modern Standards:** Use modern C++ practices, not C-style code
- **Incremental Progress:** Small, tested changes over large refactors
- **Documentation:** Comment why, not just what

### Known Challenges
- PC speaker sound recreation requires synthesis
- EGA palette conversion to modern color spaces
- Timing differences between DOS and modern systems
- Handling DOS-specific file formats

### Future Enhancements (Post-MVP)
- Save/load system
- Level editor
- Mod support
- Enhanced graphics mode (HD sprites)
- Network multiplayer (?)
- Speedrun timer
- Achievements

## Timeline Estimate

**Phase 2 (Core Physics):** 1 week
**Phase 3 (Rendering):** 2 weeks  
**Phase 4 (Level System):** 1 week
**Phase 5 (Actor System):** 2 weeks
**Phase 6 (Audio):** 1 week
**Phase 7 (UI/Menus):** 1 week
**Phase 8 (Game Loop):** 1 week
**Phase 9 (Polish/Testing):** 2 weeks
**Phase 10 (Documentation):** 1 week

**Total Estimated Time:** ~12 weeks (3 months)

*Note: Estimates assume part-time development (10-15 hours/week)*

## Contributing

This is a learning/modernization project. Contributions welcome after MVP completion.

### How to Contribute
1. Read this plan and understand the architecture
2. Check current status and pick a task
3. Reference the C refactor code for implementation
4. Write tests for your changes
5. Submit pull request with clear description

### Coding Standards
- Follow C++17 best practices
- Use consistent naming (snake_case for functions, PascalCase for classes)
- Comment complex logic
- Keep functions focused and small
- Write self-documenting code

---

**Last Updated:** 2024-02-07  
**Author:** Development Team  
**Project:** Captain Comic Modernization
