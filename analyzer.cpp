#include "analyzer.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

// Students may use ANY data structure internally

void TripAnalyzer::ingestFile(const std::string& csvPath) {

    zoneCounts_.clear();
    zoneHourCounts_.clear();

    std::ifstream in(csvPath);
    if (!in.is_open()) 

        return;
    }
    // TODO:
    // - open file
    // - skip header
    // - skip malformed rows
    // - extract PickupZoneID and pickup hour
    // - aggregate counts
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    // TODO:
    // - sort by count desc, zone asc
    // - return first k
    return {};
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    // TODO:
    // - sort by count desc, zone asc, hour asc
    // - return first k
    return {};
}

