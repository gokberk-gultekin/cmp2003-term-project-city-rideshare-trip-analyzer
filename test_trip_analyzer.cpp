#include "catch_amalgamated.hpp"
#include "analyzer.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <chrono>

namespace fs = std::filesystem;

// -------------------- helpers --------------------
static std::string zpad(int n, int width) {
    std::string s = std::to_string(n);
    if ((int)s.size() >= width) return s;
    return std::string(width - (int)s.size(), '0') + s;
}

// Environment-configurable limits (ms). Use generous defaults; tune in grading.
static long long envMs(const char* name, long long def) {
    if (const char* v = std::getenv(name)) {
        try { return std::stoll(v); } catch (...) { return def; }
    }
    return def;
}

static bool fastMode() {
    const char* e = std::getenv("FAST");
    return e && std::string(e) == "1";
}

static void requireZonesEq(const std::vector<ZoneCount>& got,
                           const std::vector<std::pair<std::string, long long>>& exp) {
    REQUIRE(got.size() == exp.size());
    for (size_t i = 0; i < exp.size(); i++) {
        INFO("Index " << i);
        REQUIRE(got[i].zone == exp[i].first);
        REQUIRE(got[i].count == exp[i].second);
    }
}

static void requireSlotsEq(const std::vector<SlotCount>& got,
                           const std::vector<std::tuple<std::string, int, long long>>& exp) {
    REQUIRE(got.size() == exp.size());
    for (size_t i = 0; i < exp.size(); i++) {
        INFO("Index " << i);
        REQUIRE(got[i].zone == std::get<0>(exp[i]));
        REQUIRE(got[i].hour == std::get<1>(exp[i]));
        REQUIRE(got[i].count == std::get<2>(exp[i]));
    }
}

// -------------------- fixture --------------------
struct TripsFixture {
    fs::path dir;
    fs::path oldCwd;

    TripsFixture() {
        oldCwd = fs::current_path();
        auto base = fs::temp_directory_path();
        dir = base / ("cmp2003_trip_tests_" +
                      std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        fs::create_directories(dir);
        fs::current_path(dir);
    }

    ~TripsFixture() {
        fs::current_path(oldCwd);
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    void writeTripsCsv(const std::string& content) {
        std::ofstream out("Trips.csv", std::ios::binary);
        REQUIRE(out.good());
        out << content;
        out.close();
        REQUIRE(fs::exists("Trips.csv"));
    }
};

// =============================================================
// CATEGORY A (15%): Robustness
// =============================================================
TEST_CASE_METHOD(TripsFixture, "A1 (5%) Empty file => no crash, empty results", "[A][70]") {
    writeTripsCsv("TripID,PickupZoneID,PickupTime\n");

    TripAnalyzer a;
    REQUIRE_NOTHROW(a.ingestFile("Trips.csv"));
    REQUIRE(a.topZones(10).empty());
    REQUIRE(a.topBusySlots(10).empty());
}

TEST_CASE_METHOD(TripsFixture, "A2 (5%) Dirty data => skip malformed rows safely", "[A][70]") {
    std::string csv =
        "TripID,PickupZoneID,PickupTime\n"
        "1,Z1,2024-01-01 10:30\n"
        "BAD,LINE\n"
        "2,Z1,2024-01-01 10:45\n"
        "3,Z2,NOT_A_TIME\n"
        "4,,2024-01-01 11:00\n"
        "5,Z9,\n"
        "6,Z2,2024-01-01 11:05\n";

    writeTripsCsv(csv);

    TripAnalyzer a;
    REQUIRE_NOTHROW(a.ingestFile("Trips.csv"));

    requireZonesEq(a.topZones(10), {{"Z1", 2}, {"Z2", 1}});
    requireSlotsEq(a.topBusySlots(10), {{"Z1", 10, 2}, {"Z2", 11, 1}});
}

TEST_CASE_METHOD(TripsFixture, "A3 (5%) Boundary hours: 00:00->0, 23:59->23", "[A][70]") {
    std::string csv =
        "TripID,PickupZoneID,PickupTime\n"
        "1,Z1,2024-01-01 00:00\n"
        "2,Z1,2024-01-01 23:59\n";
    writeTripsCsv(csv);

    TripAnalyzer a;
    a.ingestFile("Trips.csv");

    requireZonesEq(a.topZones(10), {{"Z1", 2}});
    requireSlotsEq(a.topBusySlots(10), {{"Z1", 0, 1}, {"Z1", 23, 1}});
}

// =============================================================
// CATEGORY B (20%): Sorting + tie-break determinism
// =============================================================
TEST_CASE_METHOD(TripsFixture, "B1 (10%) Tie-break zones: count desc, zone asc", "[B][70]") {
    std::string csv =
        "TripID,PickupZoneID,PickupTime\n"
        "1,B,2024-01-01 10:00\n"
        "2,A,2024-01-01 10:00\n";
    writeTripsCsv(csv);

    TripAnalyzer a;
    a.ingestFile("Trips.csv");

    requireZonesEq(a.topZones(10), {{"A", 1}, {"B", 1}});
    requireSlotsEq(a.topBusySlots(10), {{"A", 10, 1}, {"B", 10, 1}});
}

TEST_CASE_METHOD(TripsFixture, "B2 (5%) Tie-break slots: count desc, zone asc, hour asc", "[B][70]") {
    std::string csv =
        "TripID,PickupZoneID,PickupTime\n"
        "1,Z1,2024-01-01 10:00\n"
        "2,Z1,2024-01-01 10:30\n"
        "3,Z1,2024-01-01 11:00\n"
        "4,Z1,2024-01-01 11:30\n";
    writeTripsCsv(csv);

    TripAnalyzer a;
    a.ingestFile("Trips.csv");

    requireSlotsEq(a.topBusySlots(10), {{"Z1", 10, 2}, {"Z1", 11, 2}});
}

TEST_CASE_METHOD(TripsFixture, "B3 (5%) Case sensitivity: 'zone' != 'ZONE'", "[B][70]") {
    std::string csv =
        "TripID,PickupZoneID,PickupTime\n"
        "1,zone,2024-01-01 10:00\n"
        "2,ZONE,2024-01-01 10:00\n";
    writeTripsCsv(csv);

    TripAnalyzer a;
    a.ingestFile("Trips.csv");

    requireZonesEq(a.topZones(10), {{"ZONE", 1}, {"zone", 1}});
    requireSlotsEq(a.topBusySlots(10), {{"ZONE", 10, 1}, {"zone", 10, 1}});
}

// =============================================================
// CATEGORY C (35%): Performance-gated correctness
//
// IMPORTANT: These tests WILL FAIL inefficient algorithms reliably.
// You can tune limits by env vars:
//   C1_LIMIT_MS, C2_LIMIT_MS, C3_LIMIT_MS
//
// Default limits are conservative but still kill O(n^2) solutions.
// Do NOT enable FAST in grading (FAST is only for developer laptops).
// =============================================================

TEST_CASE_METHOD(TripsFixture,
    "C1 (15%) High cardinality adversary: many unique zones (kills O(n^2))",
    "[C][70]") {

    // Core adversary:
    // - Every row has a unique PickupZoneID => naive vector linear-search counting becomes O(n^2).
    // - Correct output is deterministic: all counts=1 => topZones should be lexicographically smallest IDs.
    //
    // Size: pick a number that is safe for hash-map solutions, but deadly for O(n^2).
    const int N = fastMode() ? 20000 : 150000;

    std::string csv = "TripID,PickupZoneID,PickupTime\n";
    csv.reserve((size_t)N * 40);

    for (int i = 0; i < N; i++) {
        // Zone IDs chosen so lexicographic order matches numeric order
        // Z000000 ... Z149999
        csv += std::to_string(i + 1);
        csv += ",Z";
        csv += zpad(i, 6);
        csv += ",2024-01-01 01:00\n";
    }
    writeTripsCsv(csv);

    TripAnalyzer a;

    auto t0 = std::chrono::high_resolution_clock::now();
    a.ingestFile("Trips.csv");
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    INFO("C1 ingest ms=" << ms << " N=" << N);

    // correctness: top 10 should be Z000000..Z000009 each count=1
    std::vector<std::pair<std::string, long long>> exp;
    for (int i = 0; i < 10; i++) exp.push_back({"Z" + zpad(i, 6), 1});
    requireZonesEq(a.topZones(10), exp);

    // performance gate
    // Default chosen to kill O(n^2) (usually many seconds/minutes), but allow hash solutions.
    const long long limit = envMs("C1_LIMIT_MS", fastMode() ? 2500 : 4000);
    REQUIRE(ms < limit);
}

TEST_CASE_METHOD(TripsFixture,
    "C2 (10%) Big file throughput: few keys, lots of rows",
    "[C][70]") {

    // This checks parsing/aggregation throughput.
    // Few keys means even mediocre counting can be OK; we still time-gate.
    const int N = fastMode() ? 300000 : 2000000;

    std::string csv = "TripID,PickupZoneID,PickupTime\n";
    csv.reserve((size_t)N * 30);

    // 4 zones, hour cycles
    for (int i = 0; i < N; i++) {
        int z = i & 3;          // 0..3
        int h = i % 24;         // 0..23
        csv += std::to_string(i + 1);
        csv += ",Z";
        csv += std::to_string(z);
        csv += ",2024-01-01 ";
        if (h < 10) csv += "0";
        csv += std::to_string(h);
        csv += ":00\n";
    }
    writeTripsCsv(csv);

    TripAnalyzer a;

    auto t0 = std::chrono::high_resolution_clock::now();
    a.ingestFile("Trips.csv");
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    INFO("C2 ingest ms=" << ms << " N=" << N);

    // correctness: topZones(1) should be Z0 (because z cycles evenly, tie-break by zone asc)
    // With N large, differences at most 1. Z0 should be >= others and lexicographically first among ties.
    auto z = a.topZones(1);
    REQUIRE(z.size() == 1);
    REQUIRE(z[0].zone == "Z0");

    const long long limit = envMs("C2_LIMIT_MS", fastMode() ? 3500 : 8000);
    REQUIRE(ms < limit);
}

TEST_CASE_METHOD(TripsFixture,
    "C3 (10%) Mixed volume + dominant top slot: correctness + time gate",
    "[C][70]") {

    const int N = fastMode() ? 300000 : 2500000;
    const int BOOST = fastMode() ? 20000 : 200000;

    std::string csv = "TripID,PickupZoneID,PickupTime\n";
    csv.reserve((size_t)(N + BOOST) * 34);

    long long id = 1;

    // Boost Z2@07 to force a deterministic #1 slot
    for (int i = 0; i < BOOST; i++) {
        csv += std::to_string(id++) + ",Z2,2024-01-01 07:15\n";
    }

    // Spread remaining trips across 5 zones and 24 hours
    for (int i = 0; i < N; i++) {
        int z = i % 5;
        int h = i % 24;
        csv += std::to_string(id++) + ",Z" + std::to_string(z) + ",2024-01-01 ";
        if (h < 10) csv += "0";
        csv += std::to_string(h) + ":00\n";
    }

    writeTripsCsv(csv);

    TripAnalyzer a;

    auto t0 = std::chrono::high_resolution_clock::now();
    a.ingestFile("Trips.csv");
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    INFO("C3 ingest ms=" << ms << " N=" << N << " BOOST=" << BOOST);

    // correctness for top slot
    long long baseZ2H7 = 0;
    for (int i = 0; i < N; i++) {
        if (i % 5 == 2 && i % 24 == 7) baseZ2H7++;
    }
    long long expectedTop = (long long)BOOST + baseZ2H7;

    auto top = a.topBusySlots(1);
    REQUIRE(top.size() == 1);
    REQUIRE(top[0].zone == "Z2");
    REQUIRE(top[0].hour == 7);
    REQUIRE(top[0].count == expectedTop);

    const long long limit = envMs("C3_LIMIT_MS", fastMode() ? 3500 : 9000);
    REQUIRE(ms < limit);
}
