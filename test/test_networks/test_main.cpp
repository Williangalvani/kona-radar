// Host-side tests for the saved Wi-Fi network list. Run with: pio test -e native
#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "known_networks.h"

using radar::KnownNetworks;
using radar::ScanResult;

void setUp() {}
void tearDown() {}

static std::string ssidAt(const KnownNetworks& k, size_t i) { return k.list()[i].ssid; }

void test_add_puts_newest_first() {
  KnownNetworks k;
  TEST_ASSERT_TRUE(k.add("Home", "password1"));
  TEST_ASSERT_TRUE(k.add("Cafe", ""));
  TEST_ASSERT_EQUAL(2, k.size());
  TEST_ASSERT_EQUAL_STRING("Cafe", ssidAt(k, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("Home", ssidAt(k, 1).c_str());
}

void test_add_dedupes_and_updates_password() {
  KnownNetworks k;
  k.add("Home", "password1");
  k.add("Office", "password2");
  TEST_ASSERT_TRUE(k.add("Home", "newpassword"));
  TEST_ASSERT_EQUAL(2, k.size());
  TEST_ASSERT_EQUAL_STRING("Home", ssidAt(k, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("newpassword", k.find("Home")->pass.c_str());
  // SSIDs are case-sensitive.
  k.add("home", "password3");
  TEST_ASSERT_EQUAL(3, k.size());
}

void test_add_rejects_invalid() {
  KnownNetworks k;
  TEST_ASSERT_FALSE(k.add("", "password1"));
  TEST_ASSERT_FALSE(k.add(std::string(33, 'x'), "password1"));
  TEST_ASSERT_FALSE(k.add("Home", "short"));
  TEST_ASSERT_FALSE(k.add("Home", std::string(64, 'z')));
  TEST_ASSERT_TRUE(k.add(std::string(32, 'x'), std::string(63, 'p')));
  TEST_ASSERT_TRUE(k.add("Hex", std::string(64, 'a')));
  TEST_ASSERT_EQUAL(2, k.size());
  // A rejected update leaves the old password.
  k.add("Home", "password1");
  TEST_ASSERT_FALSE(k.add("Home", "bad"));
  TEST_ASSERT_EQUAL_STRING("password1", k.find("Home")->pass.c_str());
}

void test_remove() {
  KnownNetworks k;
  k.add("A", "password1");
  k.add("B", "password2");
  TEST_ASSERT_TRUE(k.remove("A"));
  TEST_ASSERT_FALSE(k.remove("A"));
  TEST_ASSERT_EQUAL(1, k.size());
  TEST_ASSERT_NULL(k.find("A"));
  k.clear();
  TEST_ASSERT_TRUE(k.empty());
}

void test_cap_drops_least_recently_used() {
  KnownNetworks k;
  for (size_t i = 0; i < KnownNetworks::kMax; ++i) k.add("N" + std::to_string(i), "password");
  TEST_ASSERT_EQUAL(KnownNetworks::kMax, k.size());
  // N0 is the oldest; using it saves it from eviction, N1 goes instead.
  TEST_ASSERT_TRUE(k.markUsed("N0"));
  TEST_ASSERT_FALSE(k.markUsed("N0"));
  TEST_ASSERT_FALSE(k.markUsed("missing"));
  k.add("New", "password");
  TEST_ASSERT_EQUAL(KnownNetworks::kMax, k.size());
  TEST_ASSERT_NULL(k.find("N1"));
  TEST_ASSERT_NOT_NULL(k.find("N0"));
  TEST_ASSERT_EQUAL_STRING("New", ssidAt(k, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("N0", ssidAt(k, 1).c_str());
}

void test_join_order_strongest_seen_then_unseen() {
  KnownNetworks k;
  k.add("Hidden", "password");  // oldest
  k.add("Far", "password");
  k.add("Near", "password");
  k.add("Gone", "password");  // most recent, not in range
  std::vector<ScanResult> scan = {{"Neighbor", -40}, {"Far", -80}, {"Near", -55}, {"Far", -70}};
  std::vector<size_t> order = k.joinOrder(scan);
  TEST_ASSERT_EQUAL(4, order.size());
  TEST_ASSERT_EQUAL_STRING("Near", ssidAt(k, order[0]).c_str());
  TEST_ASSERT_EQUAL_STRING("Far", ssidAt(k, order[1]).c_str());  // best of its two BSSIDs, still weaker
  TEST_ASSERT_EQUAL_STRING("Gone", ssidAt(k, order[2]).c_str());
  TEST_ASSERT_EQUAL_STRING("Hidden", ssidAt(k, order[3]).c_str());

  order = k.joinOrder(scan, false);
  TEST_ASSERT_EQUAL(2, order.size());
  TEST_ASSERT_TRUE(k.joinOrder({}, false).empty());
}

void test_join_order_ties_prefer_recent() {
  KnownNetworks k;
  k.add("Old", "password");
  k.add("Recent", "password");
  std::vector<size_t> order = k.joinOrder({{"Old", -60}, {"Recent", -60}});
  TEST_ASSERT_EQUAL_STRING("Recent", ssidAt(k, order[0]).c_str());
}

void test_json_round_trip_and_hygiene() {
  KnownNetworks k;
  k.add("Open", "");
  k.add("Quote\"Net", "pa\"ss\\word");
  KnownNetworks r;
  const std::string json = k.toJson();
  TEST_ASSERT_TRUE(r.fromJson(json.data(), json.size()));
  TEST_ASSERT_EQUAL(2, r.size());
  TEST_ASSERT_EQUAL_STRING("Quote\"Net", ssidAt(r, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("pa\"ss\\word", r.find("Quote\"Net")->pass.c_str());

  const char* messy = R"([{"s":"A","p":"password"},{"s":"A","p":"other123"},{"s":"","p":""},{"s":"B","p":"bad"},
    {"s":"C"},{"x":1},{"s":"D","p":"password"},{"s":"E","p":""},{"s":"F","p":""},{"s":"G","p":""},
    {"s":"H","p":""},{"s":"I","p":""},{"s":"J","p":""}])";
  TEST_ASSERT_TRUE(r.fromJson(messy, std::strlen(messy)));
  TEST_ASSERT_EQUAL(KnownNetworks::kMax, r.size());
  TEST_ASSERT_EQUAL_STRING("password", r.find("A")->pass.c_str());
  TEST_ASSERT_NULL(r.find("B"));
  TEST_ASSERT_NOT_NULL(r.find("C"));

  TEST_ASSERT_FALSE(r.fromJson("{bad", 4));
  TEST_ASSERT_EQUAL(KnownNetworks::kMax, r.size());
}

void test_public_json_has_no_passwords() {
  KnownNetworks k;
  k.add("Home", "supersecret");
  k.add("Cafe", "");
  const std::string pub = k.toPublicJson();
  TEST_ASSERT_EQUAL_STRING(R"([{"ssid":"Cafe","open":true},{"ssid":"Home","open":false}])", pub.c_str());
  TEST_ASSERT_TRUE(pub.find("supersecret") == std::string::npos);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_add_puts_newest_first);
  RUN_TEST(test_add_dedupes_and_updates_password);
  RUN_TEST(test_add_rejects_invalid);
  RUN_TEST(test_remove);
  RUN_TEST(test_cap_drops_least_recently_used);
  RUN_TEST(test_join_order_strongest_seen_then_unseen);
  RUN_TEST(test_join_order_ties_prefer_recent);
  RUN_TEST(test_json_round_trip_and_hygiene);
  RUN_TEST(test_public_json_has_no_passwords);
  return UNITY_END();
}
