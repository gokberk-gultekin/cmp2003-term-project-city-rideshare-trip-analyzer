#include "analyzer.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <vector>
#include <charconv>
#include <cctype>

using namespace std;

// Helper: Trim whitespace
static string_view trim(string_view s) {
    size_t start = 0;
    while (start < s.size() && 
        isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }

    size_t end = s.size();
    while (end > start && 
        isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }

    return s.substr(start, end - start);
}

// Helper: Robust Hour Extraction
static int extractHour(string_view dt) {
    dt = trim(dt);
    if (dt.empty()) {
        return -1;
    }
    // Find the colon separating HH:MM
    size_t colon = dt.find(':');
    if (colon == string_view::npos || colon == 0) {
        return -1;
    }

    // Look at the character immediately before ':'
    // Case 1: "08:00" -> '8' is at colon-1
    // Case 2: " 8:00" -> '8' is at colon-1
    
    size_t hourEnd = colon;
    size_t hourStart = colon - 1;

    // Move start back if we have 2 digits (e.g., "12:00")
    if (hourStart > 0 && 
            isdigit(static_cast<unsigned char>(dt[hourStart - 1]))) {
        hourStart--;
    }

    // Validate: Are characters between hourStart and hourEnd digits?
    // This handles "12:00" (valid) vs "a:00" (invalid)
    int hour = 0;
    auto res = std::from_chars(dt.data() + 
        hourStart, dt.data() + hourEnd, hour);
    
    if (res.ec != std::errc()) {
        return -1; // Parse failed
    }
    
    if (hour < 0 || hour > 23) {
        return -1; // Range check
    }

    return hour;
}

void TripAnalyzer::ingestFile(const string& csvPath) {
    // 1. Reserve memory to prevent rehashings
    zoneCountMap.reserve(150000);
    slotCountMap.reserve(150000);

    // 2. Large IO Buffer (64KB) for uninterrupted reads as possible
    char buffer[65536];
    ifstream inFile(csvPath);
    if (!inFile.is_open()) {
        return;
    }

    inFile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

    string line;
    line.reserve(128);
    string keyBuffer;
    keyBuffer.reserve(64);

    while (getline(inFile, line)) {
        if (line.empty()) {
            continue;
        }

        string_view row(line);
        
        // Manual parsing without allocation

        // 1. TripID
        size_t c1 = row.find(',');
        if (c1 == string_view::npos) {
            continue; // Missing Column
        }
        
        string_view tripId = trim(row.substr(0, c1));
        
        // Dirty Data Rule 1: TripID must be numeric
        bool validId = !tripId.empty();
        for (char c : tripId) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                validId = false; 
                break;
            }
        }
        if (!validId) {
            continue;
        }

        // 2. PickupZoneID
        size_t c2 = row.find(',', c1 + 1);
        if (c2 == string_view::npos) {
            continue; // Missing Column
        }
        string_view zoneId = trim(row.substr(c1 + 1, c2 - c1 - 1));
        // Dirty Data Rule 2: Empty Zone
        if (zoneId.empty()) {
            continue;
        }

        // 3. DropoffZoneID (Skip content, but check structure)
        size_t c3 = row.find(',', c2 + 1);
        if (c3 == string_view::npos) {
            continue; // Missing Column
        }

        // 4. PickupDateTime
        size_t c4 = row.find(',', c3 + 1);
        if (c4 == string_view::npos) {
            continue; // Missing Column
        }
        string_view timeView = row.substr(c3 + 1, c4 - c3 - 1);
        
        // Dirty Data Rule 3: Invalid Timestamp
        int hour = extractHour(timeView);
        if (hour == -1) {
            continue; 
        }

        // 5. DistanceKm (Check structure ONLY)
        // We assume if c5 found, the row is structurally valid
        size_t c5 = row.find(',', c4 + 1);
        if (c5 == string_view::npos) {
            continue; // Missing Column
        }

        // 6. FareAmount
        // We implicitly checked the structure because we found c5.

        // --- Data Aggregation ---

        // Optimization: Assign string_view to buffer to avoid malloc
        keyBuffer.assign(zoneId); 
        
        zoneCountMap[keyBuffer]++;
        
        // Map operator[] creates the vector if it doesn't exist.
        // For performance on dense maps, try find() then insert, but [] is cleaner.
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
    results.reserve(slotCountMap.size() * 24); 

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

