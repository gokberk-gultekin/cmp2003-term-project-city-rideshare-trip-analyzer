#include "analyzer.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <vector>

using namespace std;

// Helper function to extract hour [0-23]
// Refactored for compatibility with string_view 
static int extractHour(string_view datetime) {
    // Find the space separating Date and Time
    size_t spacePos = datetime.find(' ');
    if (spacePos == string_view::npos) {
        return -1;
    }

    // Find the colon separating Hour and Minute
    size_t colonPos = datetime.find(':', spacePos);
    if (colonPos == string_view::npos) {
        return -1;
    }

    // The hour string is between space and colon
    if (colonPos <= spacePos + 1) {
        return -1; 
    }

    size_t len = colonPos - (spacePos + 1);

    if (len == 0 || len > 2) {
        return -1;
    }

    int h = 0;

    // Fast character math to parse integer
    char c1 = datetime[spacePos + 1];
    if (c1 < '0' || c1 > '9') {
        return -1;
    }

    h = c1 - '0';
    
    if (len == 2) {
        char c2 = datetime[spacePos + 2];
        if (c2 < '0' || c2 > '9') {
            return -1;
        }
        h = h * 10 + (c2 - '0');
    }

    if (h >= 0 && h <= 23) {
        return h;
    }

    return -1;
}

void TripAnalyzer::ingestFile(const string& csvPath) {
    // Pre-allocate map buckets to prevent rehashing.
    // Handles the "High Cardinality" test (100k unique zones) efficiently.
    zoneCountMap.reserve(100000); 
    slotCountMap.reserve(100000);

    // Increase file buffer to 64KB to reduce system calls during file reading
    char buffer[65536];

    ifstream inFile(csvPath);
    if (!inFile.is_open()) {
        cerr << "Failed to open file\n";
        return;
    }

    inFile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

    string line;

    // OPTIMIZATION: Reserve memory to avoid costly reallocations.
    // Avg data length is ~64 chars, so we doubled it (128) as a safety margin.
    line.reserve(128); 

    // OPTIMIZATION: Create a reusable buffer to prevent heap allocations inside the loop.
    // We reserve 64 bytes once so we can copy each row's ZoneID here without requesting new memory.
    string keyBuffer;
    keyBuffer.reserve(64);

    // Skip Header
    if (!getline(inFile, line)) {
        return; // Empty File check
    }

    while (getline(inFile, line)) {
        if (line.empty()) {
            continue;
        }

        // OPTIMIZATION: Create a "view" of the line (Pointer + Length). Zero allocation.
        string_view row(line);

        // CSV Schema: TripID, PickupZoneID, DropoffZoneID, PickupDateTime, DistanceKm, FareAmount
        
        // Manual CSV parsing for best performance under millions of rows
        
        // 1. Find TripID delimiter
        size_t comma1 = row.find(',');
        if (comma1 == string_view::npos) continue;

        // Dirty Data Check: Validate TripID is numeric
        bool validTripID = true; 
        if (comma1 == 0) validTripID = false;
        for (size_t i = 0; i < comma1; ++i) {
            if (!isdigit(row[i])) {
                validTripID = false;
                break;
            }
        }
        if (!validTripID) continue;

        // 2. Find PickupZoneID delimiter
        size_t comma2 = row.find(',', comma1 + 1);
        if (comma2 == string_view::npos) continue;

        // Extract ZoneID as a view
        string_view zoneIdView = row.substr(comma1 + 1, comma2 - comma1 - 1);
        if (zoneIdView.empty()) continue;
        
        // 3. Skip DropoffZoneID
        size_t comma3 = row.find(',', comma2 + 1);
        if (comma3 == string_view::npos) continue;

        // 4. Find PickupDateTime delimiter
        size_t comma4 = row.find(',', comma3 + 1);
        if (comma4 == string_view::npos) continue;

        // Extract DateTime as a view
        string_view timeView = row.substr(comma3 + 1, comma4 - comma3 - 1);
        if (timeView.empty()) continue;

        // Parse Hour
        int hour = extractHour(timeView);
        if (hour == -1) continue;

        // OPTIMIZATION: Reuse keyBuffer.
        // assign() copies characters into the existing capacity. No malloc called (if within capacity).
        keyBuffer.assign(zoneIdView);

        // Aggregate Data
        // zoneCountMap uses keyBuffer to hash and lookup.
        zoneCountMap[keyBuffer]++;

        // slotCountMap uses keyBuffer to hash and lookup.
        vector<long long>& hours = slotCountMap[keyBuffer];
        if (hours.empty()) {
            hours.resize(24, 0);
        }
        hours[hour]++;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    vector<ZoneCount> results;
    results.reserve(zoneCountMap.size());
    
    // Flatten map to vector
    for (const auto& [zone, count] : zoneCountMap) {
        results.push_back({ zone, count });
    }

    if (k < 0 || results.empty()) {
        return {};
    }
    
    size_t topK = min(static_cast<size_t>(k), results.size());

    // OPTIMIZATION: partial_sort is O(N log K), significantly faster than O(N log N) full sort.
    partial_sort(results.begin(), 
                 results.begin() + topK,
                 results.end(),
                 [](const ZoneCount& a, const ZoneCount& b) {
        // Primary Sort: Count Descending
        if (a.count != b.count) {
            return a.count > b.count; 
        }
        // Secondary Sort: ZoneID Ascending (Lexicographical) - Deterministic Tie-Breaker
        return a.zone < b.zone; 
    });

    results.resize(topK);
    return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    vector<SlotCount> results;
    // Heuristic reserve: assume average zone is active in ~5 distinct hours (e.g. rushes, lunch)
    results.reserve(slotCountMap.size() * 5); 

    for (const auto& [zone, hours] : slotCountMap) {
        for (int h = 0; h < 24; ++h) {
            if (static_cast<size_t>(h) < hours.size() && hours[h] > 0) {
                results.push_back({zone, h, hours[h]});
            }
        }
    }

    if (k <= 0 || results.empty()) {
        return {};
    }

    size_t topK = (min)(static_cast<size_t>(k), results.size());

    // OPTIMIZATION: partial_sort for Top K
    partial_sort(results.begin(), 
                      results.begin() + topK, 
                      results.end(), 
                      [](const SlotCount& a, const SlotCount& b) {
        
        // Primary: Count Descending
        if (a.count != b.count) {
            return a.count > b.count; 
        }
        
        // Secondary: ZoneID Ascending
        if (a.zone != b.zone) {
            return a.zone < b.zone; 
        }

        // Tertiary: Hour Ascending
        return a.hour < b.hour; 
    });

    results.resize(topK);
    return results;
}