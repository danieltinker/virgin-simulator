#pragma once

#include <string>
#include <vector>

struct Config {
    bool   modeComparative   = false;
    bool   modeCompetition   = false;
    bool   verbose           = false;
    bool   debug           = false;
    int    numThreads        = 1;

    // comparative-only
    std::string game_map;
    std::string game_managers_folder;
    std::string algorithm1;
    std::string algorithm2;

    // competition-only
    std::string game_maps_folder;
    std::string game_manager;
    std::string algorithms_folder;
};

// Parses argv into cfg. On error, prints to stderr and returns false.
bool parseArguments(int argc, char* argv[], Config& cfg);

// Prints usage to stderr.
void printUsage(const char* prog);
