#include <unity.h>
#include "Commands.h"

void test_clamp_bounds() {
    TEST_ASSERT_EQUAL_INT(100, clampPct(150));
    TEST_ASSERT_EQUAL_INT(-100, clampPct(-150));
    TEST_ASSERT_EQUAL_INT(42, clampPct(42));
}

void test_mix_forward() {
    DriveOutput o = mixDifferential(0, 80);
    TEST_ASSERT_EQUAL_INT(80, o.left);
    TEST_ASSERT_EQUAL_INT(80, o.right);
}

void test_mix_turn_right() {
    DriveOutput o = mixDifferential(50, 0);
    TEST_ASSERT_EQUAL_INT(50, o.left);
    TEST_ASSERT_EQUAL_INT(-50, o.right);
}

void test_mix_clamped() {
    DriveOutput o = mixDifferential(60, 80); // y+x=140 -> 100
    TEST_ASSERT_EQUAL_INT(100, o.left);
    TEST_ASSERT_EQUAL_INT(20, o.right);
}

void test_parse_drive() {
    Command c = parseCommand("D,50,-30");
    TEST_ASSERT_EQUAL(CmdType::Drive, c.type);
    TEST_ASSERT_EQUAL_INT(50, c.x);
    TEST_ASSERT_EQUAL_INT(-30, c.y);
}

void test_parse_pose() {
    Command c = parseCommand("P2");
    TEST_ASSERT_EQUAL(CmdType::Pose, c.type);
    TEST_ASSERT_EQUAL_INT(2, c.pose);
}

void test_parse_stop() {
    Command c = parseCommand("S");
    TEST_ASSERT_EQUAL(CmdType::Stop, c.type);
}

void test_parse_invalid() {
    Command c = parseCommand("xyz");
    TEST_ASSERT_EQUAL(CmdType::None, c.type);
}

void setUp() {} void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_clamp_bounds);
    RUN_TEST(test_mix_forward);
    RUN_TEST(test_mix_turn_right);
    RUN_TEST(test_mix_clamped);
    RUN_TEST(test_parse_drive);
    RUN_TEST(test_parse_pose);
    RUN_TEST(test_parse_stop);
    RUN_TEST(test_parse_invalid);
    return UNITY_END();
}
