#include "analyzer.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

using namespace std;

// Helper function to extract hour [0-23] 
// without memory allocation 
static int extractHour(const std::string& datetime) {
    // Find the space separating Date and Time
    size_t spacePos = datetime.find(' ');
    if (spacePos == std::string::npos) {
        return -1;
    }

    // Find the colon separating Hour and Minute
    size_t colonPos = datetime.find(':', spacePos);
    if (colonPos == std::string::npos) {
        return -1;
    }

    // The hour string is between space and colon
    // Example: "2023-01-01 08:42" -> starts at index spacePos+1, length is colonPos - (spacePos+1)
    if (colonPos <= spacePos + 1) {
        return -1; 
    }

    size_t len = colonPos - (spacePos + 1);

    if (len == 0 || len > 2) {
        return -1;
    }

    int h = 0;

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

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    // TODO:
    // - open file (OK)
    // - skip header (OK)
    // - skip malformed rows (OK)
    // - extract PickupZoneID and pickup hour (OK)
    // - aggregate counts (OK)
    ifstream inFile(csvPath);

    if (!inFile.is_open()) {
        cerr << "Failed to open file\n";
        return;
    }

    string line;

    // Skip Header
    if (!std::getline(inFile, line)) {
        return; // Empty File
    }

    while (getline(inFile, line)) {
        if (line.empty()) {
            continue;
        }
        // CSV Schema:
        // TripID, PickupZoneID, DropoffZoneID, PickupDateTime, DistanceKm, FareAmount
        // Manual parsing for obtain PickupZoneID and PickupDateTime
        size_t  comma1 = line.find(',');
        if (comma1 == string::npos) {
            continue; // Dirty Data: Missing Column
        }

        bool validTripID = true; // Directly checks the TripID is numerical
        if (comma1 == 0) {
            validTripID = false; // Handle empty TripID (comma at start)
        }

        for (size_t i = 0; i < comma1; ++i) {
            if (!isdigit(line[i])) {
                validTripID = false;
                break;
            }
        }
        
        if (!validTripID) {
            continue; // Dirty Data: TripID contains non-numerical chars
        }

        size_t comma2 = line.find(',', comma1 + 1);
        if (comma2 == string::npos) {
            continue; // Dirty Data: Missing Column
        }

        // Obtain PickupZoneID
        string pickupZoneId = line.substr(comma1 + 1, comma2 - comma1 - 1);
        if (pickupZoneId.empty()) {
            continue; // PickupZoneID should not be empty
        }
        
        size_t comma3 = line.find(',', comma2 + 1);
        if (comma3 == string::npos) {
            continue;
        }

        size_t comma4 = line.find(',', comma3 + 1);
        if (comma4 == string::npos) {
            continue;
        }

        // Obtain PickupDateTime
        string timeStr = line.substr(comma3 + 1, comma4 - comma3 - 1);
        if (timeStr.empty()) {
            continue;
        }

        int hour = extractHour(timeStr);
        if (hour == -1) {
            continue;
        }

        size_t comma5 = line.find(',', comma4 + 1);
        if (comma5 == string::npos) {
            continue;
        }
        
        // Aggregate data using an unordered_map
        zoneCountMap[pickupZoneId]++;

        vector<long long>& hours = slotCountMap[pickupZoneId];
       if (hours.empty()) {
        hours.resize(24, 0);
       }
       hours[hour]++;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    // TODO:
    // - sort by count desc, zone asc (OK)
    // - return first k (OK)

    vector<ZoneCount> results;
    
    // Allocates memory to hold every element in zoneCountMap
    results.reserve(zoneCountMap.size());
    
    
    
    for (const auto& [zone, count] : zoneCountMap) {
        // Constructs a ZoneCount object using aggregate initialization
        results.push_back({ zone, count });
    }

    // Handle Edge Cases: k should not be negative
    if (k < 0 || results.empty()) {
        return {};
    }
    
    size_t topK = std::min(static_cast<size_t>(k), results.size());

    // Uses partial_sort for O(N log K) complexity 
    // instead of the O(N log N) cost of a full sort.
    partial_sort(results.begin(), // k should not be fewer than vector size
                 results.begin() + topK,
                 results.end(),
                 [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) {
            return a.count > b.count; // Higher count first
        }
        return a.zone < b.zone; // Lexicographic order in collisions
    });

        // Cuts off the end of the vector 
        // Keeps only the first k elements
        results.resize(topK);

        return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    // TODO:
    // - sort by count desc, zone asc, hour asc (OK)
    // - return first k (OK)
    
    std::vector<SlotCount> results;
    
    // Flatten the nested Map to a Vector
    // We expect (Zones * 24) potential slots, so reserving helps performance
    results.reserve(slotCountMap.size() * 24); 

    for (const auto& [zone, hours] : slotCountMap) {
        // Iterate hours 0-23
        for (int h = 0; h < 24; ++h) {
            // Only add slots that actually have trips
            if (static_cast<size_t>(h) < hours.size() && hours[h] > 0) {
                results.push_back({zone, h, hours[h]});
            }
        }
    }

    // Handle Edge Cases
    if (k <= 0 || results.empty()) {
        return {};
    }

    size_t topK = (std::min)(static_cast<size_t>(k), results.size());

    // Partial Sort for efficiency
    std::partial_sort(results.begin(), 
                      results.begin() + topK, 
                      results.end(), 
                      [](const SlotCount& a, const SlotCount& b) {
        
        // Trip Count (Desc)
        if (a.count != b.count) {
            return a.count > b.count; 
        }
        
        // Zone ID (Asc)
        if (a.zone != b.zone) {
            return a.zone < b.zone; 
        }

        // Hour (Asc)
        return a.hour < b.hour; 
    });

    // Truncate to Top K
    results.resize(topK);

    return results;
}