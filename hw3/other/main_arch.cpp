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

#include "ArgParser.h"
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
#include "ThreadPool.h"
#include "SatelliteView.h"
#include "GameResult.h"

namespace fs = std::filesystem;
// ——————————————————————————————————————————————————————
// Error‐logging and timestamp helpers
// ——————————————————————————————————————————————————————

static std::ofstream errLog;

// Call this once at program start to open errors.txt for appending
static void initErrorLog() {
    errLog.open("errors.txt", std::ios::app);
}

// Mirror every error to both stderr and errors.txt
static void logError(const std::string& msg) {
    std::cerr << msg;
    if (errLog.is_open()) {
        errLog << msg;
        errLog.flush();
    }
}

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
    // DEBUG: dump header
    if (debug) {

    std::cout << "[DEBUG] Parsed Map — rows=" << rows 
              << ", cols=" << cols 
              << ", maxSteps=" << maxSteps 
              << ", numShells=" << numShells << "\n";
    }
    // guard against missing lines
    size_t actualLines = gridLines.size();
    if (actualLines < rows) {
        std::cerr << "[WARN ] Only " << actualLines 
                  << " grid lines, but expected " << rows << "\n";
    }
    // only iterate over what's actually present
    if (debug) {

    for (size_t r = 0; r < actualLines && r < rows; ++r) {
        std::cout << "[DEBUG] row " << r << ": " << gridLines[r] << "\n";
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
    return md;
}

// strip “.so” and directory from a path
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
// Format the “who won / tie and why” message
// ————————————————————————————————————————————————————
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
    std::cout<<"got here! WritecompFile"<<std::endl;

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
    // 3) Build output path
    auto ts = currentTimestamp();
    fs::path outPath = fs::path(cfg.game_managers_folder)
                     / ("comparative_results_" + ts + ".txt");

    // 4) Open file (or fallback)
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        logError("Error: cannot create " + outPath.string() + "\n");
        // Fallback to stdout
        goto PRINT_STDOUT;
    }

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
    return true;

PRINT_STDOUT:
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
    // 1) Load map + params
    MapData md;
    try {
        md = loadMapWithParams(cfg.game_map, cfg.debug);
    } catch (const std::exception& ex) {
        std::cerr << "Error loading map: " << ex.what() << "\n";
        return 1;
    }
    SatelliteView& realMap = *md.view;

    // 2) Load Algorithms
    auto& algoReg = AlgorithmRegistrar::get();
    std::vector<void*> algoHandles;
    if (cfg.debug) {
        std::cout << "[DEBUG] About to load " << 2 << " algorithm plugins\n";
    }
    for (auto const& algPath : {cfg.algorithm1, cfg.algorithm2}) {
        if (cfg.debug) {
            std::cout << "[DEBUG]  Loading algorithm: " << algPath << "\n";
        }
        std::string name = stripSo(algPath);
        algoReg.createAlgorithmFactoryEntry(name);
        void* h = dlopen(algPath.c_str(), RTLD_NOW);
        if (cfg.debug) {
            std::cout << "[DEBUG]   dlopened " << name << " @ " << h << "\n";
        }
        if (!h) {
            std::cerr << "Error: dlopen Algo '" << name << "' failed: " << dlerror() << "\n";
            return 1;
        }
        try { algoReg.validateLastRegistration(); }
        catch (...) {
            std::cerr << "Error: Algo registration failed for '" << name << "'\n";
            algoReg.removeLast();
            dlclose(h);
            return 1;
        }
        if (cfg.debug) {
            std::cout << "[DEBUG]   validated registration for " << name << "\n";
        }
        algoHandles.push_back(h);
    }

    // 3) Load GameManagers
    auto& gmReg = GameManagerRegistrar::get();
    std::vector<std::string> gmPaths;
    for (auto& e : fs::directory_iterator(cfg.game_managers_folder))
        if (e.path().extension() == ".so")
            gmPaths.push_back(e.path().string());
    if (gmPaths.empty()) {
        std::cerr << "Error: no .so in game_managers_folder\n";
        return 1;
    }

    std::vector<void*> gmHandles;
std::vector<std::string> validGmPaths; // Track only successfully loaded GMs
if (cfg.debug) {
    std::cout << "[DEBUG] About to load GameManager plugins from '" << cfg.game_managers_folder << "'\n";
}
for (auto const& gmPath : gmPaths) {
    if (cfg.debug) {
        std::cout << "[DEBUG]  Loading GM: " << gmPath << "\n";
    }
    std::string name = stripSo(gmPath);
    gmReg.createGameManagerEntry(name);
    void* h = dlopen(gmPath.c_str(), RTLD_NOW);
    if (!h) {
        std::cerr << "Warning: dlopen GM '" << name << "' failed: " << dlerror() << "\n";
        gmReg.removeLast(); // Clean up the registry entry
        continue; // Skip this GM and continue with the next one
    }
    if (cfg.debug) {
        std::cout << "[DEBUG]   dlopened GM " << name << " @ " << h << "\n";
    }
    try { 
        gmReg.validateLastRegistration(); 
    }
    catch (...) {
        std::cerr << "Warning: GM registration failed for '" << name << "'\n";
        gmReg.removeLast();
        dlclose(h);
        continue; // Skip this GM and continue with the next one
    }
    if (cfg.debug) {
        std::cout << "[DEBUG]   validated GM registration for " << name << "\n";
    }
    gmHandles.push_back(h);
    validGmPaths.push_back(gmPath); // Only add successfully loaded GMs
}


    // 4) Dispatch tasks
    ThreadPool pool(cfg.numThreads);
    std::mutex mtx;
    struct Entry {
        std::string gm, a1, a2;
        GameResult res;
        Entry(std::string g, std::string x, std::string y, GameResult r)
          : gm(std::move(g)), a1(std::move(x)), a2(std::move(y)), res(std::move(r)) {}
    };
    // std::vector<Entry> results;
    std::vector<ComparativeEntry> results;


    if (validGmPaths.empty()) {
    std::cerr << "Error: no valid GameManager plugins could be loaded\n";
    return 1;
}

// Update the task dispatching loop to use validGmPaths instead of gmPaths
for (size_t gi = 0; gi < validGmPaths.size(); ++gi) {
    auto& gmEntry = *(gmReg.begin() + gi);
    auto& A = *(algoReg.begin() + 0);
    auto& B = *(algoReg.begin() + 1);

    pool.enqueue([&, gi] {
        auto gm = gmEntry.factory(cfg.verbose);
        auto p1 = A.createPlayer(0, 0, 0, md.maxSteps, md.numShells);
        auto a1 = A.createTankAlgorithm(0, 0);
        auto p2 = B.createPlayer(1, 0, 0, md.maxSteps, md.numShells);
        auto a2 = B.createTankAlgorithm(1, 0);

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
        std::cout <<"main, finished GM->run " << std::endl;
        std::ostringstream ss;
        auto* state = gr.gameState.get();
        std::cout <<"main, got statež" << std::endl;
        for (size_t y = 0; y < md.rows; ++y) {
            for (size_t x = 0; x < md.cols; ++x) {
                std::cout <<"building... my final map" << std::endl;
                ss << state->getObjectAt(x, y);
            }
            ss << '\n';
        }
        std::cout <<"finished building my final map" << std::endl;
        std::string finalMap = ss.str();
        std::lock_guard<std::mutex> lock(mtx);
        std::cout <<"results " << std::endl;
        results.emplace_back(
            stripSo(validGmPaths[gi]),  // gmName
             std::move(gr),              // GameResult
            std::move(finalMap)         // the ASCII snapshot
        );
    });
}
    std::cout << "main : pool shutdown " << std::endl;
    pool.shutdown();
    
    
    std::cout << "main : write comp file " << std::endl;
    writeComparativeFile(
        cfg,
        stripSo(cfg.algorithm1),
        stripSo(cfg.algorithm2),
        results
    );
    std::cout << "report results " << std::endl;
    // 5) Report & cleanup
    std::cout << "[Simulator] Comparative Results:\n";
    for (auto& e : results) {
        std::cout << "  GM="    << e.gmName
          << "  winner="  << e.res.winner
          << "  reason="  << static_cast<int>(e.res.reason)
          << "  rounds=" << e.res.rounds
          << "\n";
    }
    
    // for (auto h : gmHandles)   dlclose(h);
    // for (auto h : algoHandles) dlclose(h);
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
        logError("Error: cannot create " + outPath.string() + "\n");
        // fallback print
        std::cout << "game_maps_folder=" << cfg.game_maps_folder << "\n";
        std::cout << "game_manager="     << cfg.game_manager      << "\n\n";
        for (auto const& p : sorted) {
            std::cout << p.first << " " << p.second << "\n";
        }
        return false;
    }

    // 5) Write contents
    ofs << "game_maps_folder=" << cfg.game_maps_folder << "\n";
    ofs << "game_manager="     << cfg.game_manager      << "\n\n";
    for (auto const& p : sorted) {
        ofs << p.first << " " << p.second << "\n";
    }

    return true;
}


// -----------------------------
// Competition mode
// -----------------------------
static int runCompetition(const Config& cfg) {
    // 1) Gather maps
    std::vector<std::string> maps;
    for (auto& e : fs::directory_iterator(cfg.game_maps_folder))
        if (e.is_regular_file())
            maps.push_back(e.path().string());
    if (maps.empty()) {
        std::cerr << "Error: no files in game_maps_folder\n";
        return 1;
    }

    // 2) Load GM (CRITICAL - must succeed)
    auto& gmReg = GameManagerRegistrar::get();
    std::string gmName = stripSo(cfg.game_manager);
    gmReg.createGameManagerEntry(gmName);
    void* gmH = dlopen(cfg.game_manager.c_str(), RTLD_NOW);
    if (!gmH) {
        std::cerr << "Error: dlopen GM failed: " << dlerror() << "\n";
        gmReg.removeLast();
        return 1;
    }
    try { 
        gmReg.validateLastRegistration(); 
    }
    catch (...) {
        std::cerr << "Error: GM registration failed for '" << gmName << "'\n";
        gmReg.removeLast();
        dlclose(gmH);
        return 1;
    }

    // 3) Load Algorithms (resilient loading)
    auto& algoReg = AlgorithmRegistrar::get();
    std::vector<void*> algoHandles;
    std::vector<std::string> algoPaths; // Track only successfully loaded algorithms
    
    for (auto& e : fs::directory_iterator(cfg.algorithms_folder)) {
        if (e.path().extension() == ".so") {
            std::string path = e.path().string();
            algoReg.createAlgorithmFactoryEntry(stripSo(path));
            void* h = dlopen(path.c_str(), RTLD_NOW);
            if (!h) {
                std::cerr << "Warning: dlopen Algo '" << path << "' failed\n";
                algoReg.removeLast();
                continue; // Skip this algorithm and continue with the next one
            }
            try { 
                algoReg.validateLastRegistration(); 
            }
            catch (...) {
                std::cerr << "Warning: Algo registration failed for '" << path << "'\n";
                algoReg.removeLast();
                dlclose(h);
                continue; // Skip this algorithm and continue with the next one
            }
            algoHandles.push_back(h);
            algoPaths.push_back(path);
        }
    }
    
    // Check if we have minimum required algorithms
    if (algoPaths.size() < 2) {
        std::cerr << "Error: need at least 2 algorithms in folder\n";
        dlclose(gmH);
        for (auto h : algoHandles) {
            dlclose(h);
        }
        return 1;
    }

    // 4) Preload maps into shared_ptrs so lambdas can capture safely
    std::vector<std::shared_ptr<SatelliteView>> mapViews;
    std::vector<size_t>                         mapRows, mapCols, mapMaxSteps, mapNumShells;
    for (auto const& mapFile : maps) {
        try {
            MapData md = loadMapWithParams(mapFile,cfg.debug);
            mapViews.emplace_back(std::move(md.view));
            mapCols .push_back(md.cols);
            mapRows .push_back(md.rows);
            mapMaxSteps.push_back(md.maxSteps);
            mapNumShells.push_back(md.numShells);
        } catch (const std::exception& ex) {
            std::cerr << "Warning: skipping map '" << mapFile << "': " << ex.what() << "\n";
        }
    }
    if (mapViews.empty()) {
        std::cerr << "Error: no valid maps to run\n";
        dlclose(gmH);
        return 1;
    }

    // 5) Dispatch tasks
    ThreadPool pool(cfg.numThreads);
    std::mutex mtx;
    struct Entry {
        std::string mapFile, a1, a2;
        GameResult res;
        Entry(std::string m, std::string x, std::string y, GameResult r)
          : mapFile(std::move(m)), a1(std::move(x)), a2(std::move(y)), res(std::move(r)) {}
    };
    // std::vector<Entry> results;
    std::vector<CompetitionEntry> results;
    auto& gmEntry = *gmReg.begin();

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
                    auto gm = gmEntry.factory(cfg.verbose);
                    auto& A = *(algoReg.begin() + i);
                    auto& B = *(algoReg.begin() + j);
                    auto p1 = A.createPlayer(0,0,0,mSteps,nShells);
                    auto a1 = A.createTankAlgorithm(0,0);
                    auto p2 = B.createPlayer(1,0,0,mSteps,nShells);
                    auto a2 = B.createTankAlgorithm(1,0);

                    GameResult gr = gm->run(
                        cols, rows,
                        realMap,
                        mapFile,
                        mSteps, nShells,
                        *p1, stripSo(algoPaths[i]),
                        *p2, stripSo(algoPaths[j]),
                        [&](int pi,int ti){ return A.createTankAlgorithm(pi,ti); },
                        [&](int pi,int ti){ return B.createTankAlgorithm(pi,ti); }
                    );

                    std::lock_guard<std::mutex> lock(mtx);
                    results.emplace_back(
                        mapFile,
                        stripSo(algoPaths[i]),
                        stripSo(algoPaths[j]),
                        std::move(gr)
                    );
                });
            }
        }
    }

    pool.shutdown();
    writeCompetitionFile(cfg, results);

    // 6) Report & cleanup
    std::cout << "[Simulator] Competition Results:\n";
    for (auto& e : results) {
        std::cout << "  map=" << e.mapFile
                  << "  A1=" << e.a1
                  << "  A2=" << e.a2
                  << " => winner=" << e.res.winner
                  << "  reason=" << static_cast<int>(e.res.reason)
                  << "  rounds=" << e.res.rounds << "\n";
    }

    // dlclose(gmH);
    // for (auto h : algoHandles) dlclose(h);
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
    // ─── Initialize our error log ────────────────────────────────
    initErrorLog();
    return cfg.modeComparative
        ? runComparative(cfg)
        : runCompetition(cfg);
}
