#include "analyzer.h"
#include <iostream>
#include <chrono>

static void printZones(const std::vector<ZoneCount>& v) {
    std::cout << "TOP_ZONES\n";
    for (auto& x : v)
        std::cout << x.zone << "," << x.count << "\n";
}

static void printSlots(const std::vector<SlotCount>& v) {
    std::cout << "TOP_SLOTS\n";
    for (auto& x : v)
        std::cout << x.zone << "," << x.hour << "," << x.count << "\n";
}

int main() {
    auto t0 = std::chrono::high_resolution_clock::now();

    TripAnalyzer analyzer;
    analyzer.ingestFile("SmallTrips.csv");

    printZones(analyzer.topZones(10));
    printSlots(analyzer.topBusySlots(10));

    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "EXEC_MS\n" << ms << "\n";
    return 0;
}
