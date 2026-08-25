#include "test_cases.h"
#include "test_helpers.h"

void test_ui_score_base100_encoding() {
    reset_physics_state();
    uint8_t digits[6];

    // Test zero score
    const uint8_t score_zero[3] = {0, 0, 0};
    UISystem::score_bytes_to_digits(score_zero, digits);
    check(digits[0] == 0 && digits[1] == 0 && digits[2] == 0 && digits[3] == 0 && digits[4] == 0 &&
              digits[5] == 0,
          "ui_score: zero should convert to all 0 digits");

    // Test score 99 (99, 0, 0)
    const uint8_t score_99[3] = {99, 0, 0};
    UISystem::score_bytes_to_digits(score_99, digits);
    check(digits[4] == 9 && digits[5] == 9,
          "ui_score: score 99 should have rightmost digits as 9,9");

    // Test score 123,456 = (56, 34, 12)
    const uint8_t score_123456[3] = {56, 34, 12};
    UISystem::score_bytes_to_digits(score_123456, digits);
    check(digits[0] == 1 && digits[1] == 2 && digits[2] == 3 && digits[3] == 4 && digits[4] == 5 &&
              digits[5] == 6,
          "ui_score: 123456 should convert to digits 1,2,3,4,5,6");

    // Test max score 999,999 = (99, 99, 99)
    const uint8_t score_max[3] = {99, 99, 99};
    UISystem::score_bytes_to_digits(score_max, digits);
    check(digits[0] == 9 && digits[1] == 9 && digits[2] == 9 && digits[3] == 9 && digits[4] == 9 &&
              digits[5] == 9,
          "ui_score: max score should convert to all 9 digits");
}

void test_hud_layout_positions() {
    reset_physics_state();
    const UISystem::HUDLayout layout = UISystem::get_hud_layout();

    check(layout.vertical_offset == 20, "hud_layout: vertical offset should remain 20");
    check(layout.score_y == 4, "hud_layout: score Y should be 4");
    check(layout.lives_y == 160, "hud_layout: lives Y should be 160");
    check(layout.hp_meter_y == 62, "hud_layout: HP meter Y should be 62");
    check(layout.fireball_meter_y == 34, "hud_layout: fireball meter Y should be 34");
}

void test_ui_fireball_meter_cell_mapping() {
    reset_physics_state();
    // Test empty meter (0)
    check(UISystem::fireball_meter_to_cell_state(0, 0) == 0,
          "ui_fireball: meter 0, cell 0 should be empty");
    check(UISystem::fireball_meter_to_cell_state(0, 5) == 0,
          "ui_fireball: meter 0, all cells should be empty");

    // Test meter value 1 (first cell half-filled)
    check(UISystem::fireball_meter_to_cell_state(1, 0) == 1,
          "ui_fireball: meter 1, cell 0 should be half");
    check(UISystem::fireball_meter_to_cell_state(1, 1) == 0,
          "ui_fireball: meter 1, cell 1 should be empty");

    // Test meter value 2 (first cell full)
    check(UISystem::fireball_meter_to_cell_state(2, 0) == 2,
          "ui_fireball: meter 2, cell 0 should be full");
    check(UISystem::fireball_meter_to_cell_state(2, 1) == 0,
          "ui_fireball: meter 2, cell 1 should be empty");

    // Test meter value 7 (cells 0-2 full, cell 3 half, cells 4-5 empty)
    check(UISystem::fireball_meter_to_cell_state(7, 0) == 2,
          "ui_fireball: meter 7, cell 0 should be full");
    check(UISystem::fireball_meter_to_cell_state(7, 1) == 2,
          "ui_fireball: meter 7, cell 1 should be full");
    check(UISystem::fireball_meter_to_cell_state(7, 2) == 2,
          "ui_fireball: meter 7, cell 2 should be full");
    check(UISystem::fireball_meter_to_cell_state(7, 3) == 1,
          "ui_fireball: meter 7, cell 3 should be half");
    check(UISystem::fireball_meter_to_cell_state(7, 4) == 0,
          "ui_fireball: meter 7, cell 4 should be empty");

    // Test max meter value (12 - all cells full)
    for (uint8_t cell = 0; cell < 6; cell++) {
        check(UISystem::fireball_meter_to_cell_state(12, cell) == 2,
              "ui_fireball: meter 12, all cells should be full");
    }

    // Test invalid cell index
    check(UISystem::fireball_meter_to_cell_state(12, 6) == 0,
          "ui_fireball: invalid cell index should return 0");
}

void test_ui_boots_detection() {
    reset_physics_state();
    // JUMP_POWER_DEFAULT is 4, WITH_BOOTS is 5
    check(!UISystem::has_boots(4), "ui_boots: jump power 4 (default) should not indicate boots");
    check(UISystem::has_boots(5), "ui_boots: jump power 5 (with boots) should indicate boots");
    check(!UISystem::has_boots(0), "ui_boots: jump power 0 should not indicate boots");
    check(!UISystem::has_boots(3), "ui_boots: jump power 3 should not indicate boots");
}

void test_ui_score_edge_cases() {
    reset_physics_state();
    uint8_t digits[6];

    // Test single digit in each position
    const uint8_t score_1[3] = {1, 0, 0};
    UISystem::score_bytes_to_digits(score_1, digits);
    check(digits[5] == 1, "ui_score_edge: score 1 should have rightmost digit as 1");

    uint8_t score_100[3] = {0, 1, 0};
    UISystem::score_bytes_to_digits(score_100, digits);
    check(digits[3] == 1 && digits[4] == 0 && digits[5] == 0,
          "ui_score_edge: score 100 should convert to 0,0,0,1,0,0");

    uint8_t score_10000[3] = {0, 0, 1};
    UISystem::score_bytes_to_digits(score_10000, digits);
    check(digits[1] == 1 && digits[2] == 0, "ui_score_edge: score 10000 should have digits[1]=1");
}

void test_ui_meter_all_states() {
    reset_physics_state();
    const uint8_t expected_states[13][6] = {
        {0, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0}, {2, 0, 0, 0, 0, 0}, {2, 1, 0, 0, 0, 0},
        {2, 2, 0, 0, 0, 0}, {2, 2, 1, 0, 0, 0}, {2, 2, 2, 0, 0, 0}, {2, 2, 2, 1, 0, 0},
        {2, 2, 2, 2, 0, 0}, {2, 2, 2, 2, 1, 0}, {2, 2, 2, 2, 2, 0}, {2, 2, 2, 2, 2, 1},
        {2, 2, 2, 2, 2, 2}};

    for (uint8_t meter = 0; meter <= 12; meter++) {
        for (uint8_t cell = 0; cell < 6; cell++) {
            uint8_t state = UISystem::fireball_meter_to_cell_state(meter, cell);
            check(state == expected_states[meter][cell],
                  "ui_meter_all_states: meter value mismatch");
        }
    }
}

static void reset_score_bytes() {
    score_bytes[0] = 0;
    score_bytes[1] = 0;
    score_bytes[2] = 0;
}

void test_award_points_no_carry() {
    reset_physics_state();
    reset_score_bytes();
    award_points(50);
    check(score_bytes[0] == 50 && score_bytes[1] == 0 && score_bytes[2] == 0,
          "award_points: simple add no carry should be 50,0,0");
}

void test_award_points_accumulates_in_byte0() {
    reset_physics_state();
    reset_score_bytes();
    award_points(20);
    award_points(30);
    check(score_bytes[0] == 50 && score_bytes[1] == 0 && score_bytes[2] == 0,
          "award_points: accumulation in byte 0 should be 50");
}

void test_award_points_carry_into_byte1() {
    reset_physics_state();
    reset_score_bytes();
    award_points(80);
    award_points(40);
    // 80+40 = 120 = 20 + 1*100
    check(score_bytes[0] == 20 && score_bytes[1] == 1 && score_bytes[2] == 0,
          "award_points: carry into byte 1 should be 20,1,0");
}

void test_award_points_full_carry_amount() {
    reset_physics_state();
    reset_score_bytes();
    award_points(150);
    check(score_bytes[0] == 50 && score_bytes[1] == 1 && score_bytes[2] == 0,
          "award_points: 150 points should carry to 50,1,0");
}

void test_award_points_large_value_above_255() {
    reset_physics_state();
    reset_score_bytes();
    award_points(350);
    check(score_bytes[0] == 50 && score_bytes[1] == 3 && score_bytes[2] == 0,
          "award_points: 350 points should be 50,3,0");
}

void test_award_points_max_score_saturation() {
    reset_physics_state();
    score_bytes[0] = 90;
    score_bytes[1] = 99;
    score_bytes[2] = 99;
    award_points(20);  // Should hit 999999
    check(score_bytes[0] == 99 && score_bytes[1] == 99 && score_bytes[2] == 99,
          "award_points: should saturate at 99,99,99");
}

void test_award_points_large_carry_saturation() {
    reset_physics_state();
    // Large carry against a full top byte must saturate to max score.
    reset_score_bytes();
    score_bytes[0] = 99;
    score_bytes[1] = 99;
    score_bytes[2] = 99;
    award_points(300);
    check(score_bytes[0] == 99 && score_bytes[1] == 99 && score_bytes[2] == 99,
          "award_points: massive award should saturate at max");
}

void test_award_points_awards_extra_life_every_50000() {
    reset_physics_state();
    score_bytes[0] = 99;
    score_bytes[1] = 99;
    score_bytes[2] = 0;
    score_10000_counter = 4;
    comic_num_lives = 2;

    // This causes carry into the ten-thousands byte and should roll
    // score_10000_counter 4->5->0, awarding one life.
    award_points(1);

    check(score_bytes[0] == 0 && score_bytes[1] == 0 && score_bytes[2] == 1,
          "award_points_50000: score should carry into ten-thousands byte");
    check(score_10000_counter == 0,
          "award_points_50000: score_10000_counter should reset after extra life award");
    check(comic_num_lives == 3,
          "award_points_50000: extra life should be awarded every 50000 points");
}

void test_award_points_awards_extra_life_after_500_internal_units() {
    reset_physics_state();
    comic_num_lives = 2;

    for (int i = 0; i < 5; i++) {
        award_points(100);
    }

    check(score_bytes[0] == 0 && score_bytes[1] == 5 && score_bytes[2] == 0,
          "award_points_500_internal: score should reach 500 internal units");
    check(score_10000_counter == 0,
          "award_points_500_internal: counter should reset after 500 internal units");
    check(comic_num_lives == 3,
          "award_points_500_internal: extra life should trigger at 500 internal units");
}

void test_high_score_bytes_conversion() {
    reset_physics_state();
    // Zero score
    const uint8_t s0[3] = {0, 0, 0};
    check(score_bytes_to_uint32(s0) == 0u, "high_score: zero score should be 0");

    const uint8_t s1[3] = {99, 99, 99};
    check(score_bytes_to_uint32(s1) == 999999u,
          "high_score: max score {99,99,99} should be 999999");

    const uint8_t s2[3] = {50, 0, 0};
    check(score_bytes_to_uint32(s2) == 50u, "high_score: bytes={50,0,0} should be 50");

    const uint8_t s3[3] = {0, 50, 0};
    check(score_bytes_to_uint32(s3) == 5000u, "high_score: bytes={0,50,0} should be 5000");

    const uint8_t s4[3] = {0, 0, 10};
    check(score_bytes_to_uint32(s4) == 100000u, "high_score: bytes={0,0,10} should be 100000");

    const uint8_t s5[3] = {20, 10, 5};
    check(score_bytes_to_uint32(s5) == 51020u, "high_score: bytes={20,10,5} should be 51020");

    const uint8_t s6[3] = {0, 2, 0};
    check(score_bytes_to_uint32(s6) == 200u, "high_score: bytes={0,2,0} should be 200");
}
