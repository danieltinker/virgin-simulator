#include "ArgParser.h"
#include "ErrorLogger.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
using namespace UserCommon_315634022;
namespace fs = std::filesystem;

void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  Comparative mode:\n"
              << "    " << prog << " --comparative \\\n"
              << "      game_map=<file> \\\n"
              << "      game_managers_folder=<dir> \\\n"
              << "      algorithm1=<so> \\\n"
              << "      algorithm2=<so> \\\n"
              << "      [num_threads=<N>] [--verbose]\n\n"
              << "  Competition mode:\n"
              << "    " << prog << " --competition \\\n"
              << "      game_maps_folder=<dir> \\\n"
              << "      game_manager=<so> \\\n"
              << "      algorithms_folder=<dir> \\\n"
              << "      [num_threads=<N>] [--verbose]\n";
}

bool parseArguments(int argc, char* argv[], Config& cfg) {
    std::vector<std::string> unsupported;
    parseArgumentsList(argc, argv, cfg, unsupported);
    
    if (!validateArguments(cfg, unsupported, argv[0])) {
        return false;
    }
    
    return validatePaths(cfg);
}

void parseArgumentsList(int argc, char* argv[], Config& cfg, std::vector<std::string>& unsupported) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!processArgument(arg, cfg)) {
            unsupported.push_back(arg);
        }
    }
}

bool processArgument(const std::string& arg, Config& cfg) {
    if      (arg == "--comparative")            { cfg.modeComparative = true; return true; }
    else if (arg == "--competition")            { cfg.modeCompetition = true; return true; }
    else if (arg == "--verbose")                { cfg.verbose = true; return true; }
    else if (arg == "--debug")                  { cfg.debug = true; return true; }
    else if (arg.rfind("num_threads=", 0) == 0) { cfg.numThreads = std::stoi(stripKey(arg, "num_threads=")); return true; }
    else if (arg.rfind("game_map=", 0) == 0)    { cfg.game_map = stripKey(arg, "game_map="); return true; }
    else if (arg.rfind("game_managers_folder=",0)==0) { cfg.game_managers_folder = stripKey(arg, "game_managers_folder="); return true; }
    else if (arg.rfind("algorithm1=",0) == 0)   { cfg.algorithm1 = stripKey(arg, "algorithm1="); return true; }
    else if (arg.rfind("algorithm2=",0) == 0)   { cfg.algorithm2 = stripKey(arg, "algorithm2="); return true; }
    else if (arg.rfind("game_maps_folder=",0)==0) { cfg.game_maps_folder = stripKey(arg, "game_maps_folder="); return true; }
    else if (arg.rfind("game_manager=",0) == 0) { cfg.game_manager = stripKey(arg, "game_manager="); return true; }
    else if (arg.rfind("algorithms_folder=",0)==0) { cfg.algorithms_folder = stripKey(arg, "algorithms_folder="); return true; }
    
    return false;
}

bool validateArguments(const Config& cfg, const std::vector<std::string>& unsupported, const char* prog) {
    return checkUnsupportedArgs(unsupported, prog) && 
           checkModeSelection(cfg, prog) && 
           checkRequiredArgs(cfg, prog);
}

bool checkUnsupportedArgs(const std::vector<std::string>& unsupported, const char* prog) {
    if (!unsupported.empty()) {
        std::cerr << "Error: unsupported arguments:";
        for (const auto& u : unsupported) std::cerr << " " << u;
        std::cerr << "\n\n";
        printUsage(prog);
        return false;
    }
    return true;
}

bool checkModeSelection(const Config& cfg, const char* prog) {
    if (cfg.modeComparative == cfg.modeCompetition) {
        std::cerr << "Error: must specify exactly one of --comparative or --competition\n\n";
        printUsage(prog);
        return false;
    }
    return true;
}

bool checkRequiredArgs(const Config& cfg, const char* prog) {
    std::vector<std::string> missing;
    collectMissingArgs(cfg, missing);
    
    if (!missing.empty()) {
        std::cerr << "Error: missing arguments:";
        for (const auto& m : missing) std::cerr << " " << m;
        std::cerr << "\n\n";
        printUsage(prog);
        return false;
    }
    return true;
}

void collectMissingArgs(const Config& cfg, std::vector<std::string>& missing) {
    if (cfg.modeComparative) {
        if (cfg.game_map.empty())               missing.push_back("game_map");
        if (cfg.game_managers_folder.empty())   missing.push_back("game_managers_folder");
        if (cfg.algorithm1.empty())             missing.push_back("algorithm1");
        if (cfg.algorithm2.empty())             missing.push_back("algorithm2");
    } else {
        if (cfg.game_maps_folder.empty())       missing.push_back("game_maps_folder");
        if (cfg.game_manager.empty())           missing.push_back("game_manager");
        if (cfg.algorithms_folder.empty())      missing.push_back("algorithms_folder");
    }
}

bool validatePaths(const Config& cfg) {
    if (cfg.modeComparative) {
        return validateComparativePaths(cfg);
    } else {
        return validateCompetitionPaths(cfg);
    }
}

bool validateComparativePaths(const Config& cfg) {
    return mustBeFile(cfg.game_map, "game_map") &&
           mustBeDir(cfg.game_managers_folder, "game_managers_folder") &&
           mustBeFile(cfg.algorithm1, "algorithm1") &&
           mustBeFile(cfg.algorithm2, "algorithm2") &&
           checkSoFiles(cfg.game_managers_folder, "game_managers_folder");
}

bool validateCompetitionPaths(const Config& cfg) {
    return mustBeDir(cfg.game_maps_folder, "game_maps_folder") &&
           mustBeFile(cfg.game_manager, "game_manager") &&
           mustBeDir(cfg.algorithms_folder, "algorithms_folder") &&
           checkSoFiles(cfg.algorithms_folder, "algorithms_folder");
}

bool mustBeDir(const std::string& path, const char* name) {
    if (!fs::is_directory(path)) {
        std::string errorMsg = "Error: " + std::string(name) + " not a directory: " + path;
        ErrorLogger::instance().log(errorMsg);  // Log to file instead
        return false;
    }
    return true;
}

bool checkSoFiles(const std::string& dirPath, const char* name) {
    for (const auto& e : fs::directory_iterator(dirPath)) {
        if (e.path().extension() == ".so") {
            return true;
        }
    }
    std::string errorMsg = "Error: " + std::string(name) + " contains no .so files";
    ErrorLogger::instance().log(errorMsg);  // Log to file instead
    return false;
}

bool mustBeFile(const std::string& path, const char* name) {
    if (!fs::is_regular_file(path)) {
        std::string errorMsg = "Error: " + std::string(name) + " not a file: " + path;
        ErrorLogger::instance().log(errorMsg);  // Log to file instead
        return false;
    }
    return true;
}



std::string stripKey(const std::string& arg, const std::string& key) {
    return arg.substr(key.size());
}