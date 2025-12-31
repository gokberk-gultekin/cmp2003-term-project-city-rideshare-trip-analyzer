#pragma once // prevents multiple inclusions
#include <string>
#include <vector>
#include <unordered_map> // Hash table-based container for its time efficiency 

// Holds a zone ID and total trip count
// To identify high density traffic zones
struct ZoneCount {
    std::string zone;
    long long count;
};

// Holds a zone ID, hour (0–23), and trip count
// To identify peak operational hours
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
    // Key: PickupZoneID
    // Value: TotalTripCount
    std::unordered_map<std::string, long long> zoneCountMap;

    // Key: PickupZoneID
    // Value: Vector of counts per hour (index 0-23)
    std::unordered_map<std::string, std::vector<long long>> slotCountMap;
};