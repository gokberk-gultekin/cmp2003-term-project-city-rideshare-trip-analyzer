#pragma once // prevents multiple inclusions
#include <string>
#include <vector>
#include <unordered_map>

// Holds a zone ID and total trip count
struct ZoneCount {
    std::string zone;           
    long long count;
};

// Holds a zone ID, hour (0–23), and trip count
struct SlotCount {
    std::string zone;
    int hour;              // 0–23
    long long count;
};

class TripAnalyzer {
public:
    // Parse Trips.csv, skip dirty rows, never crash
    void ingestFile(const std::string& csvPath);

    // Top K zones: count desc, zone asc
    std::vector<ZoneCount> topZones(int k = 10) const;

    // Top K slots: count desc, zone asc, hour asc
    std::vector<SlotCount> topBusySlots(int k = 10) const;

private:
    // Using unordered_map for O(1) average time complexity for insertions and lookups.
    std::unordered_map<std::string, long long> zoneCounts_;

    // For (zone, hour) pairs, a nested map is a simple and efficient approach.
    // The outer map keys are zone IDs, and the inner map keys are hours (0-23).
    std::unordered_map<std::string, std::unordered_map<int, long long>> zoneHourCounts_;
};
