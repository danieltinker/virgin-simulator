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

// Internal parsing helpers
void parseArgumentsList(int argc, char* argv[], Config& cfg, std::vector<std::string>& unsupported);
bool processArgument(const std::string& arg, Config& cfg);
bool validateArguments(const Config& cfg, const std::vector<std::string>& unsupported, const char* prog);
bool checkUnsupportedArgs(const std::vector<std::string>& unsupported, const char* prog);
bool checkModeSelection(const Config& cfg, const char* prog);
bool checkRequiredArgs(const Config& cfg, const char* prog);
void collectMissingArgs(const Config& cfg, std::vector<std::string>& missing);
bool validatePaths(const Config& cfg);
bool validateComparativePaths(const Config& cfg);
bool validateCompetitionPaths(const Config& cfg);
bool mustBeDir(const std::string& path, const char* name);
bool mustBeFile(const std::string& path, const char* name);
bool checkSoFiles(const std::string& dirPath, const char* name);
std::string stripKey(const std::string& arg, const std::string& key);
