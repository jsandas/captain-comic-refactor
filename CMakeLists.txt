cmake_minimum_required(VERSION 3.16)
project(CaptainComic)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Designated initializers in level_data.cpp require C++20, so the whole
# project compiles uniformly as C++20 (no per-compiler special cases).

option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

if (ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if (NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR "ENABLE_CLANG_TIDY is ON, but clang-tidy was not found.")
    endif()

    # Use the generated compile database and honor .clang-tidy from the repo root.
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE};-p=${CMAKE_BINARY_DIR})
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
endif()

# Find SDL2 libraries with platform-aware fallback.
set(SDL2_TARGET "")
set(SDL2_IMAGE_TARGET "")
set(SDL2_TTF_TARGET "")
set(SDL2_MIXER_TARGET "")
set(SDL_INCLUDE_DIRS_FALLBACK "")

function(set_first_existing_target out_var)
    foreach(candidate ${ARGN})
        if (TARGET ${candidate})
            set(${out_var} ${candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

if (WIN32)
    # Prefer config packages on Windows (vcpkg-friendly).
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    find_package(SDL2_mixer CONFIG QUIET)

    set_first_existing_target(SDL2_TARGET SDL2::SDL2 SDL2::SDL2-static)
    set_first_existing_target(SDL2_IMAGE_TARGET SDL2_image::SDL2_image SDL2_image::SDL2_image-static)
    set_first_existing_target(SDL2_TTF_TARGET SDL2_ttf::SDL2_ttf SDL2_ttf::SDL2_ttf-static)

    set_first_existing_target(SDL2_MIXER_TARGET SDL2_mixer::SDL2_mixer SDL2_mixer::SDL2_mixer-static)
else()
    # Prefer pkg-config on Unix-like systems, but support environments without it.
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
        pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
        pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
        pkg_check_modules(SDL2_MIXER IMPORTED_TARGET SDL2_mixer)

        set(SDL2_TARGET PkgConfig::SDL2)
        set(SDL2_IMAGE_TARGET PkgConfig::SDL2_IMAGE)
        set(SDL2_TTF_TARGET PkgConfig::SDL2_TTF)

        if (SDL2_MIXER_FOUND)
            set(SDL2_MIXER_TARGET PkgConfig::SDL2_MIXER)
        endif()
    else()
        # Fallback to CMake package discovery when pkg-config is unavailable.
        find_package(SDL2 REQUIRED)
        find_package(SDL2_image REQUIRED)
        find_package(SDL2_ttf REQUIRED)
        find_package(SDL2_mixer QUIET)

        if (TARGET SDL2::SDL2)
            set(SDL2_TARGET SDL2::SDL2)
        else()
            set(SDL2_TARGET ${SDL2_LIBRARIES})
            list(APPEND SDL_INCLUDE_DIRS_FALLBACK ${SDL2_INCLUDE_DIRS})
        endif()

        if (TARGET SDL2_image::SDL2_image)
            set(SDL2_IMAGE_TARGET SDL2_image::SDL2_image)
        else()
            set(SDL2_IMAGE_TARGET ${SDL2_IMAGE_LIBRARIES})
            list(APPEND SDL_INCLUDE_DIRS_FALLBACK ${SDL2_IMAGE_INCLUDE_DIRS})
        endif()

        if (TARGET SDL2_ttf::SDL2_ttf)
            set(SDL2_TTF_TARGET SDL2_ttf::SDL2_ttf)
        else()
            set(SDL2_TTF_TARGET ${SDL2_TTF_LIBRARIES})
            list(APPEND SDL_INCLUDE_DIRS_FALLBACK ${SDL2_TTF_INCLUDE_DIRS})
        endif()

        if (TARGET SDL2_mixer::SDL2_mixer)
            set(SDL2_MIXER_TARGET SDL2_mixer::SDL2_mixer)
        elseif (DEFINED SDL2_MIXER_LIBRARIES AND SDL2_MIXER_LIBRARIES)
            set(SDL2_MIXER_TARGET ${SDL2_MIXER_LIBRARIES})
            list(APPEND SDL_INCLUDE_DIRS_FALLBACK ${SDL2_MIXER_INCLUDE_DIRS})
        endif()
    endif()
endif()

if (NOT SDL2_TARGET OR NOT SDL2_IMAGE_TARGET OR NOT SDL2_TTF_TARGET)
    message(FATAL_ERROR "Failed to discover required SDL2/SDL2_image/SDL2_ttf dependencies.")
endif()

# --- Core Game Logic Library ---
# Shared logic used by both the main game and the test suite.
# Sources are registered per-directory via target_sources() and grouped by
# module, so a new module adds one line next to its siblings instead of
# hunting down a flat global list.
add_library(comic_core STATIC)

target_sources(comic_core PRIVATE
    src/actors.cpp
)
target_sources(comic_core PRIVATE
    src/audio.cpp
)
target_sources(comic_core PRIVATE
    src/cheats.cpp
    src/doors.cpp
)
target_sources(comic_core PRIVATE
    src/game_sequences.cpp
    src/game_state.cpp
)
target_sources(comic_core PRIVATE
    src/graphics.cpp
    src/ui_system.cpp
)
target_sources(comic_core PRIVATE
    src/high_scores.cpp
    src/input_bindings.cpp
    src/text_screen_utils.cpp
    src/title_sequence.cpp
)
target_sources(comic_core PRIVATE
    src/level_data.cpp
    src/level_loader.cpp
    src/level_tiles.cpp
)
target_sources(comic_core PRIVATE
    src/physics.cpp
    src/player_teleport.cpp
)

target_include_directories(comic_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

if (SDL_INCLUDE_DIRS_FALLBACK)
    list(REMOVE_DUPLICATES SDL_INCLUDE_DIRS_FALLBACK)
    target_include_directories(comic_core PUBLIC ${SDL_INCLUDE_DIRS_FALLBACK})
endif()

target_link_libraries(comic_core PUBLIC
    ${SDL2_TARGET}
    ${SDL2_IMAGE_TARGET}
    ${SDL2_TTF_TARGET}
)

if (SDL2_MIXER_TARGET)
    target_link_libraries(comic_core PUBLIC ${SDL2_MIXER_TARGET})
    target_compile_definitions(comic_core PUBLIC HAVE_SDL2_MIXER=1)
else()
    message(WARNING "SDL2_mixer not found. Building without audio output support.")
endif()

# --- Main Executable ---
add_executable(captain_comic src/main.cpp)
target_link_libraries(captain_comic PRIVATE comic_core)
target_compile_definitions(captain_comic PRIVATE SDL_MAIN_HANDLED)

# --- Test Suite ---
enable_testing()

# Test sources are registered per-directory via target_sources(); each test
# file self-registers its cases via REGISTER_TEST(), so adding tests no longer
# touches build metadata.
add_executable(comic_tests)

target_sources(comic_tests PRIVATE tests/test_main.cpp tests/test_helpers.cpp)
target_sources(comic_tests PRIVATE tests/test_physics.cpp)
target_sources(comic_tests PRIVATE tests/test_graphics.cpp)
target_sources(comic_tests PRIVATE tests/test_actors.cpp)
target_sources(comic_tests PRIVATE tests/test_audio.cpp)
target_sources(comic_tests PRIVATE tests/test_ui.cpp)
target_link_libraries(comic_tests PRIVATE comic_core)
target_compile_definitions(comic_tests PRIVATE SDL_MAIN_HANDLED)

# Test Registrations
add_test(NAME comic_tests_all COMMAND comic_tests)
add_test(NAME comic_tests_physics_tiles COMMAND comic_tests --filter physics_tiles)
add_test(NAME comic_tests_animation_looping COMMAND comic_tests --filter animation_looping)
add_test(NAME comic_tests_animation_non_looping COMMAND comic_tests --filter animation_non_looping)
add_test(NAME comic_tests_animation_zero_duration COMMAND comic_tests --filter animation_zero_duration)
add_test(NAME comic_tests_jump_edge_trigger COMMAND comic_tests --filter jump_edge_trigger)
add_test(NAME comic_tests_jump_recharge COMMAND comic_tests --filter jump_recharge)
add_test(NAME comic_tests_jump_height COMMAND comic_tests --filter jump_height)
add_test(NAME comic_tests_space_level_uses_lower_gravity COMMAND comic_tests --filter space_level_uses_lower_gravity)
add_test(NAME comic_tests_jump_top_clamped_to_playfield COMMAND comic_tests --filter jump_top_clamped_to_playfield)
add_test(NAME comic_tests_high_score_bytes_conversion COMMAND comic_tests --filter high_score_bytes_conversion)
add_test(NAME comic_tests_door_animation_phase_progression_and_render_state COMMAND comic_tests --filter door_animation_phase_progression_and_render_state)
add_test(NAME comic_tests_door_destination_load_deferred_until_entering_complete COMMAND comic_tests --filter door_destination_load_deferred_until_entering_complete)
