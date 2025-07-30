#include "ArgParser.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

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

static std::string stripKey(const std::string& arg, const std::string& key) {
    return arg.substr(key.size());
}

bool parseArguments(int argc, char* argv[], Config& cfg) {
    std::vector<std::string> unsupported;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--comparative")            cfg.modeComparative = true;
        else if (arg == "--competition")             cfg.modeCompetition = true;
        else if (arg == "--verbose")                 cfg.verbose = true;
        else if (arg == "--debug")                 cfg.debug = true;
        else if (arg.rfind("num_threads=", 0) == 0)  cfg.numThreads = std::stoi(stripKey(arg, "num_threads="));
        else if (arg.rfind("game_map=", 0) == 0)      cfg.game_map = stripKey(arg, "game_map=");
        else if (arg.rfind("game_managers_folder=",0)==0) cfg.game_managers_folder = stripKey(arg, "game_managers_folder=");
        else if (arg.rfind("algorithm1=",0) == 0)     cfg.algorithm1 = stripKey(arg, "algorithm1=");
        else if (arg.rfind("algorithm2=",0) == 0)     cfg.algorithm2 = stripKey(arg, "algorithm2=");
        else if (arg.rfind("game_maps_folder=",0)==0) cfg.game_maps_folder = stripKey(arg, "game_maps_folder=");
        else if (arg.rfind("game_manager=",0) == 0)   cfg.game_manager = stripKey(arg, "game_manager=");
        else if (arg.rfind("algorithms_folder=",0)==0)cfg.algorithms_folder = stripKey(arg, "algorithms_folder=");
        else                                         unsupported.push_back(arg);
    }

    // 1) Unsupported
    if (!unsupported.empty()) {
        std::cerr << "Error: unsupported arguments:";
        for (auto& u : unsupported) std::cerr << " " << u;
        std::cerr << "\n\n";
        printUsage(argv[0]);
        return false;
    }

    // 2) Exactly one mode
    if (cfg.modeComparative == cfg.modeCompetition) {
        std::cerr << "Error: must specify exactly one of --comparative or --competition\n\n";
        printUsage(argv[0]);
        return false;
    }

    // 3) Required args
    std::vector<std::string> missing;
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
    if (!missing.empty()) {
        std::cerr << "Error: missing arguments:";
        for (auto& m : missing) std::cerr << " " << m;
        std::cerr << "\n\n";
        printUsage(argv[0]);
        return false;
    }

    // 4) Existence checks
    auto mustBeDir = [&](const std::string& path, const char* name){
        if (!fs::is_directory(path)) {
            std::cerr << "Error: " << name << " not a directory: " << path << "\n";
            return false;
        }
        return true;
    };
    auto mustBeFile = [&](const std::string& path, const char* name){
        if (!fs::is_regular_file(path)) {
            std::cerr << "Error: " << name << " not a file: " << path << "\n";
            return false;
        }
        return true;
    };

    if (cfg.modeComparative) {
        if (!mustBeFile(cfg.game_map, "game_map")) return false;
        if (!mustBeDir(cfg.game_managers_folder, "game_managers_folder")) return false;
        if (!mustBeFile(cfg.algorithm1, "algorithm1")) return false;
        if (!mustBeFile(cfg.algorithm2, "algorithm2")) return false;

        // require at least one .so in game_managers_folder
        bool found = false;
        for (auto& e : fs::directory_iterator(cfg.game_managers_folder))
            if (e.path().extension()==".so") { found = true; break; }
        if (!found) {
            std::cerr << "Error: game_managers_folder contains no .so files\n";
            return false;
        }
    } else {
        if (!mustBeDir(cfg.game_maps_folder, "game_maps_folder")) return false;
        if (!mustBeFile(cfg.game_manager, "game_manager")) return false;
        if (!mustBeDir(cfg.algorithms_folder, "algorithms_folder")) return false;

        bool found = false;
        for (auto& e : fs::directory_iterator(cfg.algorithms_folder))
            if (e.path().extension()==".so") { found = true; break; }
        if (!found) {
            std::cerr << "Error: algorithms_folder contains no .so files\n";
            return false;
        }
    }

    return true;
}
