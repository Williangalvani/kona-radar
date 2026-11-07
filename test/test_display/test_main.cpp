// Host-side tests for the panel brightness curve. Run with: pio test -e native
#include <unity.h>

#include "brightness.h"
#include "settings.h"

using radar::panelLevel;

void setUp() {}
void tearDown() {}

void test_off_stays_off_and_full_is_full() {
  TEST_ASSERT_EQUAL(0, panelLevel(0));
  TEST_ASSERT_EQUAL(255, panelLevel(100));
  TEST_ASSERT_EQUAL(255, panelLevel(200));
}

void test_low_percentages_stay_visible() {
  TEST_ASSERT_TRUE(panelLevel(1) >= radar::kPanelLevelFloor);
  // The old linear mapping gave 31 at 12 % and 90 at 35 %.
  TEST_ASSERT_TRUE(panelLevel(12) >= 60);
  TEST_ASSERT_TRUE(panelLevel(35) >= 115);
}

void test_curve_is_monotonic() {
  for (int p = 1; p <= 100; ++p) TEST_ASSERT_TRUE(panelLevel(p) > panelLevel(p - 1));
}

void test_night_default_is_readable_and_below_day() {
  const radar::Settings s = radar::Settings::defaults();
  TEST_ASSERT_EQUAL(35, s.nightBrightness);
  TEST_ASSERT_TRUE(s.nightBrightness < s.brightness);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_off_stays_off_and_full_is_full);
  RUN_TEST(test_low_percentages_stay_visible);
  RUN_TEST(test_curve_is_monotonic);
  RUN_TEST(test_night_default_is_readable_and_below_day);
  return UNITY_END();
}
