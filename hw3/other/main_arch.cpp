#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <dlfcn.h>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <map>
#include <algorithm>
#include <thread>

#include "ArgParser.h"
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
#include "ThreadPool.h"
#include "SatelliteView.h"
#include "GameResult.h"

namespace fs = std::filesystem;

// ——————————————————————————————————————————————————————
// Professional Debug Logging System
// ——————————————————————————————————————————————————————

static std::mutex g_debug_mutex;
static std::ofstream errLog;

// Thread-safe debug print with component identification
#define DEBUG_PRINT(component, function, message, debug_flag) \
    do { \
        if (debug_flag) { \
            std::lock_guard<std::mutex> lock(g_debug_mutex); \
            std::cout << "[T" << std::this_thread::get_id() << "] [" << component << "] [" << function << "] " << message << std::endl; \
        } \
    } while(0)

// Convenience macro for when cfg is available
#define DEBUG_PRINT_CFG(component, function, message) DEBUG_PRINT(component, function, message, cfg.debug)

// Thread-safe info print for important operations
#define INFO_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cout << "[T" << std::this_thread::get_id() << "] [INFO] [" << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

// Thread-safe warning print
#define WARN_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

// Thread-safe error print
#define ERROR_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

// Call this once at program start to open errors.txt for appending
static void initErrorLog() {
    errLog.open("errors.txt", std::ios::app);
    INFO_PRINT("SIMULATOR", "initErrorLog", "Error logging initialized");
}

// Mirror every error to both stderr and errors.txt
// static void logError(const std::string& msg) {
//     ERROR_PRINT("SIMULATOR", "logError", msg);
//     if (errLog.is_open()) {
//         errLog << msg;
//         errLog.flush();
//     }
// }

// Returns "YYYYMMDD_HHMMSS" for file names
static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return ss.str();
}

//------------------------------------------------------------------------------
// MapLoader: parse your assignment‐style map file
//------------------------------------------------------------------------------
struct MapData {
    std::unique_ptr<SatelliteView> view;
    size_t rows, cols;
    size_t maxSteps, numShells;
};

static MapData loadMapWithParams(const std::string& path, bool debug) {
    DEBUG_PRINT("MAPLOADER", "loadMapWithParams", "Loading map from: " + path, debug);
    
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open map file: " + path);
    }

    size_t rows = 0, cols = 0, maxSteps = 0, numShells = 0;
    std::vector<std::string> gridLines;
    std::string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("Rows",      0) == 0) {
            rows = std::stoul(line.substr(line.find('=') + 1));
        } else if (line.rfind("Cols",      0) == 0) {
            cols = std::stoul(line.substr(line.find('=') + 1));
        } else if (line.rfind("MaxSteps",  0) == 0) {
            maxSteps = std::stoul(line.substr(line.find('=') + 1));
        } else if (line.rfind("NumShells", 0) == 0) {
            numShells = std::stoul(line.substr(line.find('=') + 1));
        } else {
            if (line.empty()) continue;
            bool isGrid = true;
            for (char c : line) {
                if (c!='.' && c!='#' && c!='@' && c!='1' && c!='2') {
                    isGrid = false;
                    break;
                }
            }
            if (isGrid) gridLines.push_back(line);
        }
    }

    DEBUG_PRINT("MAPLOADER", "loadMapWithParams", 
        "Parsed map parameters - rows=" + std::to_string(rows) + 
        ", cols=" + std::to_string(cols) + 
        ", maxSteps=" + std::to_string(maxSteps) + 
        ", numShells=" + std::to_string(numShells), debug);

    // Guard against missing lines
    size_t actualLines = gridLines.size();
    if (actualLines < rows) {
        WARN_PRINT("MAPLOADER", "loadMapWithParams", 
            "Only " + std::to_string(actualLines) + " grid lines found, expected " + std::to_string(rows));
    }

    if (debug) {
        for (size_t r = 0; r < actualLines && r < rows; ++r) {
            DEBUG_PRINT("MAPLOADER", "loadMapWithParams", "Grid row " + std::to_string(r) + ": " + gridLines[r], debug);
        }
    }

    if (rows==0 || cols==0)
        throw std::runtime_error("Missing Rows or Cols in map header");
    if (gridLines.size() != rows) {
        std::ostringstream os;
        os << "Expected " << rows << " grid lines but found " << gridLines.size();
        throw std::runtime_error(os.str());
    }
    for (size_t i = 0; i < rows; ++i) {
        if (gridLines[i].size() != cols) {
            std::ostringstream os;
            os << "Map row " << i << " length " << gridLines[i].size()
               << " != Cols=" << cols;
            throw std::runtime_error(os.str());
        }
    }

    // Build SatelliteView:
    class MapView : public SatelliteView {
    public:
        MapView(std::vector<std::string>&& rows)
          : rows_(std::move(rows)),
            width_(rows_.empty() ? 0 : rows_[0].size()),
            height_(rows_.size()) {}
        char getObjectAt(size_t x, size_t y) const override {
            return (y<height_ && x<width_) ? rows_[y][x] : ' ';
        }
        size_t width()  const { return width_;  }
        size_t height() const { return height_; }
    private:
        std::vector<std::string> rows_;
        size_t width_, height_;
    };

    MapData md;
    md.rows      = rows;
    md.cols      = cols;
    md.maxSteps  = maxSteps;
    md.numShells = numShells;
    md.view      = std::make_unique<MapView>(std::move(gridLines));
    
    DEBUG_PRINT("MAPLOADER", "loadMapWithParams", "Map loaded successfully", debug);
    return md;
}

// strip ".so" and directory from a path
static std::string stripSo(const std::string& path) {
    auto fname = fs::path(path).filename().string();
    if (auto pos = fname.rfind(".so"); pos != std::string::npos)
        return fname.substr(0, pos);
    return fname;
}

// ——————————————————————————————————————————————————————
// Entry type for comparative‑mode results
// ——————————————————————————————————————————————————————
struct ComparativeEntry {
    std::string gmName;      // plugin name
    GameResult  res;         // result: winner(0/1/2), reason, rounds
    std::string finalState;  // ASCII board snapshot (with '\n's)

    ComparativeEntry(std::string g,
                     GameResult  r,
                     std::string fs)
      : gmName(std::move(g))
      , res(std::move(r))
      , finalState(std::move(fs))
    {}
};

// Format the "who won / tie and why" message
static std::string outcomeMessage(int winner, GameResult::Reason reason) {
    std::string msg;
    if (winner == 0)           msg = "Tie: ";
    else if (winner == 1)      msg = "Player 1 won: ";
    else /* winner == 2 */     msg = "Player 2 won: ";

    switch (reason) {
      case GameResult::ALL_TANKS_DEAD:
        msg += "all opponent tanks dead";   break;
      case GameResult::MAX_STEPS:
        msg += "max steps reached";         break;
      case GameResult::ZERO_SHELLS:
        msg += "no shells remaining";       break;
    }
    return msg;
}

static bool writeComparativeFile(
    const Config& cfg,
    const std::string& algo1,
    const std::string& algo2,
    const std::vector<ComparativeEntry>& entries)
{
    INFO_PRINT("FILEWRITER", "writeComparativeFile", "Writing comparative results file");

    // 1) Header key
    struct Outcome {
        int                       winner;
        GameResult::Reason        reason;
        size_t                    rounds;
        std::string               finalState;
        bool operator<(Outcome const& o) const {
            if (winner != o.winner)     return winner  < o.winner;
            if (reason != o.reason)     return reason  < o.reason;
            if (rounds != o.rounds)     return rounds  < o.rounds;
            return finalState < o.finalState;
        }
    };

    // 2) Group GMs by identical Outcome
    std::map<Outcome, std::vector<std::string>> groups;

    for (auto const& e : entries) {
        Outcome key{
            e.res.winner,
            e.res.reason,
            e.res.rounds,
            e.finalState
        };
        groups[key].push_back(e.gmName);
    }

    DEBUG_PRINT("FILEWRITER", "writeComparativeFile", 
        "Grouped results into " + std::to_string(groups.size()) + " outcome categories", cfg.debug);

    // 3) Build output path
    auto ts = currentTimestamp();
    fs::path outPath = fs::path(cfg.game_managers_folder)
                     / ("comparative_results_" + ts + ".txt");

    // 4) Open file (or fallback)
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        WARN_PRINT("FILEWRITER", "writeComparativeFile", "Cannot create file " + outPath.string() + ", falling back to stdout");
        goto PRINT_STDOUT;
    }

    INFO_PRINT("FILEWRITER", "writeComparativeFile", "Writing to file: " + outPath.string());

    // 5) Write header
    ofs << "game_map="   << cfg.game_map   << "\n";
    ofs << "algorithm1=" << algo1          << "\n";
    ofs << "algorithm2=" << algo2          << "\n\n";

    // 6) Iterate each group
    for (auto const& [outcome, gms] : groups) {
        // 5th line: comma‑separated GM names
        for (size_t i = 0; i < gms.size(); ++i) {
            ofs << gms[i] << (i + 1 < gms.size() ? "," : "") ;
        }
        ofs << "\n";
        // 6th: result message
        ofs << outcomeMessage(outcome.winner, outcome.reason) << "\n";
        // 7th: round number
        ofs << outcome.rounds << "\n";
        // 8th+: final ASCII map
        ofs << outcome.finalState << "\n\n";
    }
    
    INFO_PRINT("FILEWRITER", "writeComparativeFile", "Comparative results file written successfully");
    return true;

PRINT_STDOUT:
    INFO_PRINT("FILEWRITER", "writeComparativeFile", "Writing comparative results to stdout");
    std::cout << "game_map="   << cfg.game_map   << "\n";
    std::cout << "algorithm1=" << algo1          << "\n";
    std::cout << "algorithm2=" << algo2          << "\n\n";
    for (auto const& [outcome, gms] : groups) {
        for (size_t i = 0; i < gms.size(); ++i) {
            std::cout << gms[i] << (i + 1 < gms.size() ? "," : "") ;
        }
        std::cout << "\n";
        std::cout << outcomeMessage(outcome.winner, outcome.reason) << "\n";
        std::cout << outcome.rounds << "\n";
        std::cout << outcome.finalState << "\n\n";
    }
    return false;
}

// -----------------------------
// Comparative mode
// -----------------------------
static int runComparative(const Config& cfg) {
    INFO_PRINT("SIMULATOR", "runComparative", "Starting comparative mode");

    // 1) Load map + params
    MapData md;
    try {
        md = loadMapWithParams(cfg.game_map, cfg.debug);
    } catch (const std::exception& ex) {
        ERROR_PRINT("SIMULATOR", "runComparative", "Error loading map: " + std::string(ex.what()));
        return 1;
    }
    SatelliteView& realMap = *md.view;

    // 2) Load Algorithms
    auto& algoReg = AlgorithmRegistrar::get();
    std::vector<void*> algoHandles;
    DEBUG_PRINT_CFG("SIMULATOR", "runComparative", "Loading 2 algorithm plugins");
    
    for (auto const& algPath : {cfg.algorithm1, cfg.algorithm2}) {
        std::string name = stripSo(algPath);
        DEBUG_PRINT_CFG("PLUGINLOADER", "runComparative", "Loading algorithm: " + algPath);
        
        algoReg.createAlgorithmFactoryEntry(name);
        void* h = dlopen(algPath.c_str(), RTLD_NOW);
        
        if (!h) {
            ERROR_PRINT("PLUGINLOADER", "runComparative", "dlopen failed for algorithm '" + name + "': " + std::string(dlerror()));
            return 1;
        }
        
        DEBUG_PRINT_CFG("PLUGINLOADER", "runComparative", "Algorithm '" + name + "' loaded at address " + std::to_string(reinterpret_cast<uintptr_t>(h)));
        
        try { 
            algoReg.validateLastRegistration(); 
        }
        catch (...) {
            ERROR_PRINT("PLUGINLOADER", "runComparative", "Registration validation failed for algorithm '" + name + "'");
            algoReg.removeLast();
            dlclose(h);
            return 1;
        }
        
        DEBUG_PRINT_CFG("PLUGINLOADER", "runComparative", "Algorithm '" + name + "' registration validated");
        algoHandles.push_back(h);
    }

    // 3) Load GameManagers
    auto& gmReg = GameManagerRegistrar::get();
    std::vector<std::string> gmPaths;
    for (auto& e : fs::directory_iterator(cfg.game_managers_folder))
        if (e.path().extension() == ".so")
            gmPaths.push_back(e.path().string());
    if (gmPaths.empty()) {
        ERROR_PRINT("SIMULATOR", "runComparative", "No .so files found in game_managers_folder");
        return 1;
    }

    std::vector<void*> gmHandles;
    std::vector<std::string> validGmPaths; // Track only successfully loaded GMs
    
    DEBUG_PRINT_CFG("SIMULATOR", "runComparative", "Loading GameManager plugins from '" + cfg.game_managers_folder + "'");
    
    for (auto const& gmPath : gmPaths) {
        std::string name = stripSo(gmPath);
        DEBUG_PRINT_CFG("PLUGINLOADER", "runComparative", "Loading GameManager: " + gmPath);
        
        gmReg.createGameManagerEntry(name);
        void* h = dlopen(gmPath.c_str(), RTLD_NOW);
        if (!h) {
            WARN_PRINT("PLUGINLOADER", "runComparative", "dlopen failed for GameManager '" + name + "': " + std::string(dlerror()));
            gmReg.removeLast(); // Clean up the registry entry
            continue; // Skip this GM and continue with the next one
        }
        
        DEBUG_PRINT_CFG("PLUGINLOADER", "runComparative", "GameManager '" + name + "' loaded at address " + std::to_string(reinterpret_cast<uintptr_t>(h)));
        
        try { 
            gmReg.validateLastRegistration(); 
        }
        catch (...) {
            WARN_PRINT("PLUGINLOADER", "runComparative", "Registration validation failed for GameManager '" + name + "'");
            gmReg.removeLast();
            dlclose(h);
            continue; // Skip this GM and continue with the next one
        }
        
        DEBUG_PRINT_CFG("PLUGINLOADER", "runComparative", "GameManager '" + name + "' registration validated");
        gmHandles.push_back(h);
        validGmPaths.push_back(gmPath); // Only add successfully loaded GMs
    }

    if (validGmPaths.empty()) {
        ERROR_PRINT("SIMULATOR", "runComparative", "No valid GameManager plugins could be loaded");
        return 1;
    }

    INFO_PRINT("SIMULATOR", "runComparative", "Successfully loaded " + std::to_string(validGmPaths.size()) + " GameManager(s)");

    // 4) Dispatch tasks
    ThreadPool pool(cfg.numThreads);
    std::mutex mtx;
    std::vector<ComparativeEntry> results;

    INFO_PRINT("THREADPOOL", "runComparative", "Starting game execution with " + std::to_string(cfg.numThreads) + " threads");

    // Update the task dispatching loop to use validGmPaths instead of gmPaths
    for (size_t gi = 0; gi < validGmPaths.size(); ++gi) {
        auto& gmEntry = *(gmReg.begin() + gi);
        auto& A = *(algoReg.begin() + 0);
        auto& B = *(algoReg.begin() + 1);

        pool.enqueue([&, gi] {
            DEBUG_PRINT_CFG("GAMETHREAD", "worker", "Starting game execution for GM index " + std::to_string(gi));
            
            auto gm = gmEntry.factory(cfg.verbose);
            auto p1 = A.createPlayer(0, md.rows, md.cols, md.maxSteps, md.numShells);
            auto a1 = A.createTankAlgorithm(0, 0);
            auto p2 = B.createPlayer(1, md.rows, md.cols, md.maxSteps, md.numShells);
            auto a2 = B.createTankAlgorithm(1, 0);

            DEBUG_PRINT_CFG("GAMETHREAD", "worker", "Created players and algorithms, starting game run");

            GameResult gr = gm->run(
                md.cols, md.rows,
                realMap,
                cfg.game_map,
                md.maxSteps, md.numShells,
                *p1, stripSo(cfg.algorithm1),
                *p2, stripSo(cfg.algorithm2),
                [&](int pi,int ti){ return A.createTankAlgorithm(pi,ti); },
                [&](int pi,int ti){ return B.createTankAlgorithm(pi,ti); }
            );
            
            DEBUG_PRINT_CFG("GAMETHREAD", "worker", "Game execution completed for GM index " + std::to_string(gi));
            
            std::ostringstream ss;
            auto* state = gr.gameState.get();
            
            DEBUG_PRINT_CFG("GAMETHREAD", "worker", "Building final map state for GM index " + std::to_string(gi));
            for (size_t y = 0; y < md.rows; ++y) {
                for (size_t x = 0; x < md.cols; ++x) {
                    ss << state->getObjectAt(x, y);
                }
                ss << '\n';
            }
            
            std::string finalMap = ss.str();
            DEBUG_PRINT_CFG("GAMETHREAD", "worker", "Final map state built, storing results for GM index " + std::to_string(gi));
            
            std::lock_guard<std::mutex> lock(mtx);
            results.emplace_back(
                stripSo(validGmPaths[gi]),  // gmName
                std::move(gr),              // GameResult
                std::move(finalMap)         // the ASCII snapshot
            );
        });
    }
    
    INFO_PRINT("THREADPOOL", "runComparative", "All tasks enqueued, waiting for completion");
    pool.shutdown();
    
    INFO_PRINT("SIMULATOR", "runComparative", "All games completed, writing results");
    writeComparativeFile(
        cfg,
        stripSo(cfg.algorithm1),
        stripSo(cfg.algorithm2),
        results
    );

    // 5) Report & cleanup
    INFO_PRINT("SIMULATOR", "runComparative", "Comparative Results Summary:");
    for (auto& e : results) {
        INFO_PRINT("RESULTS", "runComparative", 
            "GM=" + e.gmName + 
            " winner=" + std::to_string(e.res.winner) + 
            " reason=" + std::to_string(static_cast<int>(e.res.reason)) + 
            " rounds=" + std::to_string(e.res.rounds));
    }
    
    return 0;
}

// ——————————————————————————————————————————————————————
// Entry type for competitive‐mode results
// ——————————————————————————————————————————————————————
struct CompetitionEntry {
    std::string mapFile;   // which map
    std::string a1, a2;    // algorithm names
    GameResult  res;       // its outcome
    CompetitionEntry(std::string m,
                     std::string x,
                     std::string y,
                     GameResult  r)
      : mapFile(std::move(m))
      , a1(std::move(x))
      , a2(std::move(y))
      , res(std::move(r))
    {}
};

// Writes out the competition_<timestamp>.txt file.
// Returns true on success; on failure it falls back to printing to stdout.
static bool writeCompetitionFile(
    const Config& cfg,
    const std::vector<CompetitionEntry>& results)
{
    INFO_PRINT("FILEWRITER", "writeCompetitionFile", "Writing competition results file");

    // 1) Tally scores
    std::map<std::string,int> scores;
    for (auto const& entry : results) {
        const auto& a1 = entry.a1;
        const auto& a2 = entry.a2;
        if (!scores[a1]){
            scores[a1] = 0;
        }
        if (!scores[a2]){
            scores[a2] = 0;
        }
        int w = entry.res.winner;  // 0=tie, 1=a1, 2=a2

        if (w == 1) {
            scores[a1] += 3;
        }
        else if (w == 2) {
            scores[a2] += 3;
        }
        else {
            // tie
            scores[a1] += 1;
            scores[a2] += 1;
        }
    }

    DEBUG_PRINT("FILEWRITER", "writeCompetitionFile", "Calculated scores for " + std::to_string(scores.size()) + " algorithms", cfg.debug);

    // 2) Sort by descending score
    std::vector<std::pair<std::string,int>> sorted(scores.begin(), scores.end());
    std::sort(sorted.begin(), sorted.end(),
        [](auto const& L, auto const& R){
            return L.second > R.second;
        });

    // 3) Build output path
    auto ts = currentTimestamp();
    fs::path outPath = fs::path(cfg.algorithms_folder)
                     / ("competition_" + ts + ".txt");

    // 4) Open file (or fallback to stdout)
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        WARN_PRINT("FILEWRITER", "writeCompetitionFile", "Cannot create file " + outPath.string() + ", falling back to stdout");
        // fallback print
        std::cout << "game_maps_folder=" << cfg.game_maps_folder << "\n";
        std::cout << "game_manager="     << cfg.game_manager      << "\n\n";
        for (auto const& p : sorted) {
            std::cout << p.first << " " << p.second << "\n";
        }
        return false;
    }

    INFO_PRINT("FILEWRITER", "writeCompetitionFile", "Writing to file: " + outPath.string());

    // 5) Write contents
    ofs << "game_maps_folder=" << cfg.game_maps_folder << "\n";
    ofs << "game_manager="     << cfg.game_manager      << "\n\n";
    for (auto const& p : sorted) {
        ofs << p.first << " " << p.second << "\n";
    }

    INFO_PRINT("FILEWRITER", "writeCompetitionFile", "Competition results file written successfully");
    return true;
}

// -----------------------------
// Competition mode
// -----------------------------
static int runCompetition(const Config& cfg) {
    INFO_PRINT("SIMULATOR", "runCompetition", "Starting competition mode");

    // 1) Gather maps
    std::vector<std::string> maps;
    for (auto& e : fs::directory_iterator(cfg.game_maps_folder))
        if (e.is_regular_file())
            maps.push_back(e.path().string());
    if (maps.empty()) {
        ERROR_PRINT("SIMULATOR", "runCompetition", "No files found in game_maps_folder");
        return 1;
    }

    DEBUG_PRINT_CFG("SIMULATOR", "runCompetition", "Found " + std::to_string(maps.size()) + " map files");

    // 2) Load GM (CRITICAL - must succeed)
    auto& gmReg = GameManagerRegistrar::get();
    std::string gmName = stripSo(cfg.game_manager);
    
    DEBUG_PRINT_CFG("PLUGINLOADER", "runCompetition", "Loading GameManager: " + cfg.game_manager);
    gmReg.createGameManagerEntry(gmName);
    void* gmH = dlopen(cfg.game_manager.c_str(), RTLD_NOW);
    if (!gmH) {
        ERROR_PRINT("PLUGINLOADER", "runCompetition", "dlopen failed for GameManager: " + std::string(dlerror()));
        gmReg.removeLast();
        return 1;
    }
    try { 
        gmReg.validateLastRegistration(); 
    }
    catch (...) {
        ERROR_PRINT("PLUGINLOADER", "runCompetition", "GameManager registration validation failed for '" + gmName + "'");
        gmReg.removeLast();
        dlclose(gmH);
        return 1;
    }

    DEBUG_PRINT_CFG("PLUGINLOADER", "runCompetition", "GameManager '" + gmName + "' loaded and validated successfully");

    // 3) Load Algorithms (resilient loading)
    auto& algoReg = AlgorithmRegistrar::get();
    std::vector<void*> algoHandles;
    std::vector<std::string> algoPaths; // Track only successfully loaded algorithms
    
    DEBUG_PRINT_CFG("SIMULATOR", "runCompetition", "Loading algorithm plugins from '" + cfg.algorithms_folder + "'");
    
    for (auto& e : fs::directory_iterator(cfg.algorithms_folder)) {
        if (e.path().extension() == ".so") {
            std::string path = e.path().string();
            std::string name = stripSo(path);
            
            DEBUG_PRINT_CFG("PLUGINLOADER", "runCompetition", "Loading algorithm: " + path);
            algoReg.createAlgorithmFactoryEntry(name);
            void* h = dlopen(path.c_str(), RTLD_NOW);
            if (!h) {
                WARN_PRINT("PLUGINLOADER", "runCompetition", "dlopen failed for algorithm '" + path + "': " + std::string(dlerror()));
                algoReg.removeLast();
                continue; // Skip this algorithm and continue with the next one
            }
            try { 
                algoReg.validateLastRegistration(); 
            }
            catch (...) {
                WARN_PRINT("PLUGINLOADER", "runCompetition", "Registration validation failed for algorithm '" + path + "'");
                algoReg.removeLast();
                dlclose(h);
                continue; // Skip this algorithm and continue with the next one
            }
            DEBUG_PRINT_CFG("PLUGINLOADER", "runCompetition", "Algorithm '" + name + "' loaded and validated successfully");
            algoHandles.push_back(h);
            algoPaths.push_back(path);
        }
    }
    
    // Check if we have minimum required algorithms
    if (algoPaths.size() < 2) {
        ERROR_PRINT("SIMULATOR", "runCompetition", "Need at least 2 algorithms in folder, found " + std::to_string(algoPaths.size()));
        gmReg.removeLast(); // Clean up the GameManager entry
        // algoHandles.clear();
        algoReg.clear(); // Clear the AlgorithmRegistrar
        algoPaths.clear();
        dlclose(gmH);
        for (auto h : algoHandles) {
            dlclose(h);
        }
        return 1;
    }

    INFO_PRINT("SIMULATOR", "runCompetition", "Successfully loaded " + std::to_string(algoPaths.size()) + " algorithm(s)");

    // 4) Preload maps into shared_ptrs so lambdas can capture safely
    std::vector<std::shared_ptr<SatelliteView>> mapViews;
    std::vector<size_t>                         mapRows, mapCols, mapMaxSteps, mapNumShells;
    
    DEBUG_PRINT_CFG("SIMULATOR", "runCompetition", "Preloading map data into shared structures");
    
    for (auto const& mapFile : maps) {
        try {
            MapData md = loadMapWithParams(mapFile, cfg.debug);
            mapViews.emplace_back(std::move(md.view));
            mapCols .push_back(md.cols);
            mapRows .push_back(md.rows);
            mapMaxSteps.push_back(md.maxSteps);
            mapNumShells.push_back(md.numShells);
            DEBUG_PRINT_CFG("MAPLOADER", "runCompetition", "Successfully preloaded map: " + mapFile);
        } catch (const std::exception& ex) {
            WARN_PRINT("MAPLOADER", "runCompetition", "Skipping invalid map '" + mapFile + "': " + std::string(ex.what()));
        }
    }
    if (mapViews.empty()) {
        ERROR_PRINT("SIMULATOR", "runCompetition", "No valid maps to run");
        dlclose(gmH);
        return 1;
    }

    INFO_PRINT("SIMULATOR", "runCompetition", "Successfully preloaded " + std::to_string(mapViews.size()) + " valid map(s)");

    // 5) Dispatch tasks
    ThreadPool pool(cfg.numThreads);
    std::mutex mtx;
    std::vector<CompetitionEntry> results;
    auto& gmEntry = *gmReg.begin();

    // Calculate total number of games
    size_t totalGames = 0;
    for (size_t i = 0; i + 1 < algoPaths.size(); ++i) {
        for (size_t j = i + 1; j < algoPaths.size(); ++j) {
            totalGames += mapViews.size();
        }
    }

    INFO_PRINT("THREADPOOL", "runCompetition", 
        "Starting execution of " + std::to_string(totalGames) + " total games with " + 
        std::to_string(cfg.numThreads) + " threads");

    for (size_t mi = 0; mi < mapViews.size(); ++mi) {
        auto mapViewPtr = mapViews[mi];
        size_t cols     = mapCols[mi],
               rows     = mapRows[mi],
               mSteps   = mapMaxSteps[mi],
               nShells  = mapNumShells[mi];
        const std::string mapFile = maps[mi];
        SatelliteView& realMap = *mapViewPtr;

        for (size_t i = 0; i + 1 < algoPaths.size(); ++i) {
            for (size_t j = i + 1; j < algoPaths.size(); ++j) {
                pool.enqueue([=,&realMap,&mtx,&results,&algoReg,&gmEntry]() {
                    std::string algo1Name = stripSo(algoPaths[i]);
                    std::string algo2Name = stripSo(algoPaths[j]);
                    
                    DEBUG_PRINT("GAMETHREAD", "worker", 
                        "Starting game: " + algo1Name + " vs " + algo2Name + " on map " + mapFile, cfg.debug);
                    
                    auto gm = gmEntry.factory(cfg.verbose);
                    auto& A = *(algoReg.begin() + i);
                    auto& B = *(algoReg.begin() + j);
                    auto p1 = A.createPlayer(0,rows,cols,mSteps,nShells);
                    auto a1 = A.createTankAlgorithm(0,0);
                    auto p2 = B.createPlayer(1,rows,cols,mSteps,nShells);
                    auto a2 = B.createTankAlgorithm(1,0);

                    DEBUG_PRINT("GAMETHREAD", "worker", "Created players and algorithms, executing game", cfg.debug);

                    GameResult gr = gm->run(
                        cols, rows,
                        realMap,
                        mapFile,
                        mSteps, nShells,
                        *p1, algo1Name,
                        *p2, algo2Name,
                        [&](int pi,int ti){ return A.createTankAlgorithm(pi,ti); },
                        [&](int pi,int ti){ return B.createTankAlgorithm(pi,ti); }
                    );

                    DEBUG_PRINT("GAMETHREAD", "worker", 
                        "Game completed: " + algo1Name + " vs " + algo2Name + 
                        " winner=" + std::to_string(gr.winner) + 
                        " rounds=" + std::to_string(gr.rounds), cfg.debug);

                    std::lock_guard<std::mutex> lock(mtx);
                    results.emplace_back(
                        mapFile,
                        algo1Name,
                        algo2Name,
                        std::move(gr)
                    );
                });
            }
        }
    }

    INFO_PRINT("THREADPOOL", "runCompetition", "All tasks enqueued, waiting for completion");
    pool.shutdown();
    
    INFO_PRINT("SIMULATOR", "runCompetition", "All games completed, writing results");
    writeCompetitionFile(cfg, results);

    // 6) Report & cleanup
    INFO_PRINT("SIMULATOR", "runCompetition", "Competition Results Summary:");
    for (auto& e : results) {
        INFO_PRINT("RESULTS", "runCompetition", 
            "Map=" + e.mapFile + 
            " A1=" + e.a1 + 
            " A2=" + e.a2 + 
            " => winner=" + std::to_string(e.res.winner) + 
            " reason=" + std::to_string(static_cast<int>(e.res.reason)) + 
            " rounds=" + std::to_string(e.res.rounds));
    }

    return 0;
}

// -----------------------------
// main()
// -----------------------------
int main(int argc, char* argv[]) {
    Config cfg;
    if (!parseArguments(argc, argv, cfg)) {
        return 1;
    }
    
    // Initialize our error log
    initErrorLog();
    
    INFO_PRINT("SIMULATOR", "main", "Starting simulator in " + 
        std::string(cfg.modeComparative ? "comparative" : "competition") + " mode");
    
    int result = cfg.modeComparative
        ? runComparative(cfg)
        : runCompetition(cfg);
        
    INFO_PRINT("SIMULATOR", "main", "Simulator finished with exit code " + std::to_string(result));
    
    return result;
}