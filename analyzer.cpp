#include "analyzer.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <vector>
#include <charconv>
#include <cctype>

using namespace std;

// Trims whitespaces
static string_view trim(string_view s) 
{
    size_t start = 0;
    while (start < s.size() && 
        isspace(static_cast<unsigned char>
                (s[start])))
        start++;

    size_t end = s.size();
    while (end > start && 
        isspace(static_cast<unsigned char>
                (s[end - 1])))
        end--;

    return s.substr(start, end - start);
}

// Extracts Hour
static int extractHour(string_view str)
{
    str = trim(str);
    if (str.empty())
        return -1;

    size_t colon = str.find(':');
    if (colon == string_view::npos || colon == 0)
        return -1;

    size_t hourEnd = colon;
    size_t hourStart = colon - 1;

    if (hourStart > 0 && 
            isdigit(static_cast<unsigned char>
                    (str[hourStart - 1])))
        hourStart--;

    int hour = 0;
    auto res = std::from_chars(str.data() + 
        hourStart, str.data() + hourEnd, hour);

    if (res.ec != std::errc())
        return -1;

    if (hour < 0 || hour > 23)
        return -1;

    return hour;
}

void TripAnalyzer::ingestFile(const string& csvPath) 
{
    // Reserve memory to prevent rehashings
    zoneCountMap.reserve(150000);
    slotCountMap.reserve(150000);

    // Large IO Buffer (64KB) for uninterrupted reads as possible for optimization
    char buffer[65536];

    ifstream inFile(csvPath);
    if (!inFile.is_open())
        return;

    inFile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

    string line;
    line.reserve(128);
    string keyBuffer;
    keyBuffer.reserve(64);

    while (getline(inFile, line)) 
    {
        if (line.empty())
            continue;

        string_view row(line);
        
        // TripID
        size_t c1 = row.find(',');
        if (c1 == string_view::npos)
            continue;

        string_view tripId = trim(row.substr(0, c1));
        // Dirty Data Rule 1: Empty TripID
        if (tripId.empty())
            continue;

        // PickupZoneID
        size_t c2 = row.find(',', c1 + 1);
        if (c2 == string_view::npos)
            continue;

        string_view zoneId = trim(row.substr(c1 + 1, c2 - c1 - 1));
        // Dirty Data Rule 2: Empty Zone
        if (zoneId.empty())
            continue;

        // DropoffZoneID (Skip content, but check structure)
        size_t c3 = row.find(',', c2 + 1);
        if (c3 == string_view::npos)
            continue;

        // PickupDateTime
        size_t c4 = row.find(',', c3 + 1);
        if (c4 == string_view::npos)
            continue;

        string_view timeView = row.substr(c3 + 1, c4 - c3 - 1);
        // Dirty Data Rule 3: Invalid Timestamp
        int hour = extractHour(timeView);
        if (hour == -1)
            continue; 

        // 5. DistanceKm (Check structure ONLY)
        // We assume if c5 found, the row is structurally valid
        size_t c5 = row.find(',', c4 + 1);
        if (c5 == string_view::npos)
            continue;

        // 6. FareAmount
        // We implicitly checked the structure because we found c5.

        // --- Data Aggregation ---

        // Assign string_view to buffer to avoid malloc
        keyBuffer.assign(zoneId); 
        
        zoneCountMap[keyBuffer]++;

        vector<long long>& hours = slotCountMap[keyBuffer];
        if (hours.empty())
            hours.resize(24, 0);

        hours[hour]++;
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const 
{
    vector<ZoneCount> results;
    
    // Reserve upfront to avoid reallocations during push_back
    results.reserve(zoneCountMap.size());
    
    // Flatten unordered_map -> vector
    for (const auto& [zone, count] : zoneCountMap)
        results.push_back({ zone, count });

    if (k < 0 || results.empty())
        return {};

    size_t topK = min(static_cast<size_t>(k), 
                      results.size());
    
    // PARTIAL SORT:
    // - Ensures the first topK elements are the "best" ones
    // - Complexity: O(N log K), faster than full sort when K << N
    // - Elements beyond topK are left in unspecified order
    partial_sort(results.begin(), 
                 results.begin() + topK,
                 results.end(),
                 [](const ZoneCount& a, const ZoneCount& b) {
        
        // Primary sort key: total trip count (descending)
        if (a.count != b.count)
            return a.count > b.count; 
        
        // Secondary sort key: zone ID (ascending)
        return a.zone < b.zone; 
    });

    results.resize(topK);

    return results;
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const 
{
    vector<SlotCount> results;
    
    // Heuristic preallocation:
    // In practice, most zones are active in only a few hours (e.g., rush hours),
    // but this reserves an upper bound to reduce reallocations.
    // Worst-case: every zone active in all 24 hours.
    results.reserve(slotCountMap.size() * 24); 
    
    for (const auto& [zone, hours] : slotCountMap) 
    {
        // Iterate over all 24 possible hours
        for (int h = 0; h < 24; ++h) 
        {
            if (static_cast<size_t>(h) < hours.size() && hours[h] > 0)
                results.push_back({zone, h, hours[h]});
        }
    }

    if (k <= 0 || results.empty())
        return {};

    size_t topK = (min)(static_cast<size_t>(k), results.size());

    // OPTIMIZATION: partial_sort for Top K
    partial_sort(results.begin(), 
                      results.begin() + topK, 
                      results.end(), 
                      [](const SlotCount& a, const SlotCount& b) {
        
        // Primary key: trip count (descending)
        if (a.count != b.count)
            return a.count > b.count; 

        // Secondary key: zone ID (ascending)
        if (a.zone != b.zone)
            return a.zone < b.zone; 

        // Tertiary key: hour (ascending)
        return a.hour < b.hour; 
    });

    results.resize(topK);

    return results;

}
