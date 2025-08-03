#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <mutex>

// Include required headers instead of forward declarations for member variables
#include "ArgParser.h"     // For Config struct
#include "GameResult.h"    // For GameResult struct

// Forward declarations for pointers/references only
class SatelliteView;
class ThreadPool;
class AbstractGameManager;

// Result structures
struct ComparativeEntry {
    std::string gmName;
    GameResult  res;
    std::string finalState;

    ComparativeEntry(std::string g, GameResult r, std::string fs);
};

struct CompetitionEntry {
    std::string mapFile;
    std::string a1, a2;
    GameResult  res;
    
    CompetitionEntry(std::string m, std::string x, std::string y, GameResult r);
};

struct MapData {
    std::unique_ptr<SatelliteView> view;
    size_t rows, cols;
    size_t maxSteps, numShells;
};

class Simulator {
public:
    // Constructor
    explicit Simulator(const Config& config);
    
    // Destructor
    ~Simulator();
    
    // Main execution methods
    int run();
    
    // Mode-specific execution
    int runComparative();
    int runCompetition();
    
    // Configuration access
    const Config& getConfig() const { return config_; }
    
    // Statistics
    size_t getTotalGamesPlayed() const { return totalGamesPlayed_; }
    size_t getSuccessfullyLoadedAlgorithms() const { return loadedAlgorithms_; }
    size_t getSuccessfullyLoadedGameManagers() const { return loadedGameManagers_; }

private:
    // Core data
    Config config_;
    std::unique_ptr<ThreadPool> threadPool_;
    
    // Statistics
    size_t totalGamesPlayed_ = 0;
    size_t loadedAlgorithms_ = 0;
    size_t loadedGameManagers_ = 0;
    
    // Dynamic library handles
    std::vector<void*> algorithmHandles_;
    std::vector<void*> gameManagerHandles_;
    
    // Loaded paths (successful only)
    std::vector<std::string> validAlgorithmPaths_;
    std::vector<std::string> validGameManagerPaths_;
    
    // Thread safety
    mutable std::mutex resultsMutex_;
    std::vector<ComparativeEntry> comparativeResults_;
    std::vector<CompetitionEntry> competitionResults_;
    
    // Utility methods
    MapData loadMapWithParams(const std::string& path) const;
    std::string stripSoExtension(const std::string& path) const;
    std::string currentTimestamp() const;
    
    // Plugin loading
    bool loadAlgorithmPlugins();
    bool loadGameManagerPlugins();
    bool loadSingleGameManager();
    
    // Cleanup
    void cleanup();
    
    // File writing
    bool writeComparativeFile(const std::vector<ComparativeEntry>& entries) const;
    bool writeCompetitionFile(const std::vector<CompetitionEntry>& entries) const;
    
    // Result formatting
    std::string outcomeMessage(int winner, GameResult::Reason reason) const;
    
    // Map preprocessing for competition mode
    std::vector<std::shared_ptr<SatelliteView>> preloadMaps(
        const std::vector<std::string>& mapFiles,
        std::vector<size_t>& mapRows,
        std::vector<size_t>& mapCols, 
        std::vector<size_t>& mapMaxSteps,
        std::vector<size_t>& mapNumShells) const;
    
    // Enhanced map preprocessing that tracks which maps were successfully loaded
    std::vector<std::shared_ptr<SatelliteView>> preloadMapsAndTrackValid(
        const std::vector<std::string>& mapFiles,
        std::vector<std::string>& validMapFiles,
        std::vector<size_t>& mapRows,
        std::vector<size_t>& mapCols, 
        std::vector<size_t>& mapMaxSteps,
        std::vector<size_t>& mapNumShells) const;
    
    // Task dispatching
    void dispatchComparativeTasks();
    void dispatchCompetitionTasks();
    
    // Map error tracking
    struct MapLoadError {
        std::string mapPath;
        std::string errorReason;
        std::string timestamp;
    };
    mutable std::vector<MapLoadError> mapLoadErrors_;
    
    // Map processing helpers
    std::vector<std::string> normalizeGrid(const std::vector<std::string>& rawGrid, 
                                          size_t targetRows, size_t targetCols) const;
    char normalizeCell(char c) const;
    void writeMapErrors() const;
    
    // Logging helpers
    void logInfo(const std::string& component, const std::string& function, const std::string& message) const;
    void logDebug(const std::string& component, const std::string& function, const std::string& message) const;
    void logWarn(const std::string& component, const std::string& function, const std::string& message) const;
    void logError(const std::string& component, const std::string& function, const std::string& message) const;
    
    // Error logging
    static std::mutex debugMutex_;
    static std::ofstream errorLog_;
    static void initErrorLog();
};