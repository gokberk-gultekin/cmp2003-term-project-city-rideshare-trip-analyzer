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
};