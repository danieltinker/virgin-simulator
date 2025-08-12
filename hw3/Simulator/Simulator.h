#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <mutex>
#include <set> 
#include <filesystem>
namespace fs = std::filesystem;
// Include required headers instead of forward declarations for member variables
#include "ArgParser.h"     // For Config struct
#include "GameResult.h"    // For GameResult struct
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
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

// Competition setup struct
struct CompetitionSetup {
    std::vector<std::string> allMapFiles;
    std::vector<std::string> validMapFiles;
    std::vector<std::shared_ptr<SatelliteView>> mapViews;
    std::vector<size_t> mapRows, mapCols, mapMaxSteps, mapNumShells;
};


struct MapData {
    std::unique_ptr<SatelliteView> view;
    size_t rows, cols;
    size_t maxSteps, numShells;
};

struct MapParameters {
    size_t rows = 0, cols = 0, maxSteps = 0, numShells = 0;
    bool foundRows = false, foundCols = false, foundMaxSteps = false, foundNumShells = false;
    std::vector<std::string> rawGridLines;
    std::string path; 
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
    // In Simulator.h, add to the private section:
    std::string baseName(const std::string& path) const;
    // Parameter parsing helpers
    void validateParameterStart(const std::string& line, const std::string& paramName, const std::string& path) const;
    size_t findEqualsSign(const std::string& line, const std::string& path) const;
    std::string extractAndTrimBeforeEquals(const std::string& line, size_t eqPos) const;
    void validateParameterName(const std::string& beforeEquals, const std::string& paramName,
                            const std::string& line, const std::string& path) const;
    std::string extractAndTrimAfterEquals(const std::string& line, size_t eqPos) const;
    void parseParameterValue(const std::string& afterEquals, const std::string& paramName,
                            size_t& value, const std::string& line, const std::string& path) const;
    // Task dispatching helpers
    MapData loadComparativeMap();
    void enqueueComparativeTasks(const MapData& md);
    void executeComparativeGame(const auto& gmEntry, const auto& A, const auto& B,
                            const MapData& md, SatelliteView& realMap,
                            const std::string& mapFile, const std::string& algo1Name,
                            const std::string& algo2Name, size_t gi);
    GameResult runComparativeGame(const auto& gmEntry, const auto& A, const auto& B,
                                const MapData& md, SatelliteView& realMap,
                                const std::string& mapFile, const std::string& algo1Name,
                                const std::string& algo2Name);
    std::string buildFinalMapString(const GameResult& gr, const MapData& md);
    CompetitionSetup prepareCompetitionData();
    void enqueueCompetitionTasks(const CompetitionSetup& setup);
    void enqueueMapTasks(const CompetitionSetup& setup, size_t mi, const auto& gmEntry, AlgorithmRegistrar& algoReg);
    void executeCompetitionGame(AlgorithmRegistrar& algoReg, const auto& gmEntry,
                            SatelliteView& realMap, size_t cols, size_t rows,
                            size_t mSteps, size_t nShells, const std::string& mapFile,
                            size_t i, size_t j);
    GameResult runCompetitionGame(AlgorithmRegistrar& algoReg, const auto& gmEntry,
                                SatelliteView& realMap, size_t cols, size_t rows,
                                size_t mSteps, size_t nShells, const std::string& mapFile,
                                const std::string& algo1Name, const std::string& algo2Name,
                                size_t i, size_t j);
    size_t calculateTotalGames(size_t numMaps);
    void finalizeTaskExecution();

    // GameManager loading helpers
    void loadGameManagersFromDirectory(GameManagerRegistrar& gmReg);
    void loadSingleGameManager(GameManagerRegistrar& gmReg, const std::string& path);
    bool createGameManagerEntry(GameManagerRegistrar& gmReg, const std::string& name);
    void* loadGameManagerLibrary(GameManagerRegistrar& gmReg, const std::string& path, const std::string& name);
    bool validateGameManagerRegistration(GameManagerRegistrar& gmReg, void* handle, const std::string& name);
    void finalizeGameManagerLoad(void* handle, const std::string& path, const std::string& name);
    // Algorithm loading helpers
    bool loadComparativeAlgorithms(AlgorithmRegistrar& algoReg);
    bool loadCompetitionAlgorithms(AlgorithmRegistrar& algoReg);
    bool loadSingleAlgorithm(AlgorithmRegistrar& algoReg, const std::string& algPath, bool failOnError);
    bool validateAlgorithmFile(const std::string& algPath);
    bool createAlgorithmEntry(AlgorithmRegistrar& algoReg, const std::string& name, bool failOnError);
    void* loadAlgorithmLibrary(const std::string& algPath, const std::string& name, AlgorithmRegistrar& algoReg, bool failOnError);
    bool validateAlgorithmRegistration(AlgorithmRegistrar& algoReg, const std::string& name, void* handle, bool failOnError);
    void handleValidationError(const std::string& errorMsg, AlgorithmRegistrar& algoReg, void* handle, bool failOnError);
    void finalizeAlgorithmLoad(void* handle, const std::string& algPath, const std::string& name);
    // Map loading helpers
    std::ifstream openMapFile(const std::string& path) const;
    MapParameters parseMapParameters(std::ifstream& in, const std::string& path) const;
    bool processMapLine(const std::string& line, MapParameters& params, int lineNumber, const std::string& path) const;
    bool tryParseParameter(const std::string& line, const std::string& paramName, size_t& value, bool& found, const std::string& path) const;
    bool looksLikeParameter(const std::string& line) const;
    void validateMapParameters(const MapParameters& params, const std::string& path) const;
    void checkRequiredHeaders(const MapParameters& params, const std::string& path) const;
    void validateDimensions(const MapParameters& params, const std::string& path) const;
    void checkDimensionMismatches(const MapParameters& params, const std::string& path) const;
    std::vector<std::string> createNormalizedGrid(const MapParameters& params) const;
    MapData buildMapData(const MapParameters& params, std::vector<std::string>&& normalizedGrid) const;
    void cleanLine(std::string& line) const;
    void logNormalizedGrid(const std::vector<std::string>& normalizedGrid) const;
    // Competition file writing helpers
    std::map<std::string, int> calculateScores(const std::vector<CompetitionEntry>& results) const;
    std::vector<std::pair<std::string, int>> sortScoresByDescending(const std::map<std::string, int>& scores) const;
    fs::path buildOutputPath() const;
    bool writeToFile(const fs::path& outPath, const std::vector<std::pair<std::string, int>>& sorted) const;
    bool writeToStdout(const std::vector<std::pair<std::string, int>>& sorted) const;
    void writeContent(std::ostream& os, const std::vector<std::pair<std::string, int>>& sorted) const;

    // Grid normalization helpers
    void processGridRows(const std::vector<std::string>& rawGrid, 
                        std::vector<std::string>& normalized,
                        size_t targetRows, size_t targetCols, 
                        std::set<char>& invalidCharsFound) const;
    void processExistingRow(const std::string& sourceRow, std::string& normalizedRow,
                        size_t targetCols, std::set<char>& invalidCharsFound) const;
    void logExtraRowsIgnored(const std::vector<std::string>& rawGrid, size_t targetRows) const;
    void logInvalidCharacters(const std::set<char>& invalidCharsFound, const std::string& path) const;
    std::string buildInvalidCharsString(const std::set<char>& invalidChars) const;

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
    
    // Helper method to parse parameter lines with flexible spacing around '='
    bool parseParameter(const std::string& line, const std::string& paramName, 
                       size_t& value, const std::string& path) const;
    
    // Enhanced grid normalization with character cleaning and flexible dimensions
    std::vector<std::string> cleanAndNormalizeGrid(const std::vector<std::string>& rawGrid, 
                                                   size_t targetRows, size_t targetCols, 
                                                   const std::string& path) const;
    
    // Character cleaning - only keep valid game objects, replace invalid with space
    char cleanCharacter(char c) const;
    
    // Map processing helpers
    char normalizeCell(char c) const;
    
    // Logging helpers
    void logInfo(const std::string& component, const std::string& function, const std::string& message) const;
    void logDebug(const std::string& component, const std::string& function, const std::string& message) const;
    void logWarn(const std::string& component, const std::string& function, const std::string& message) const;
    void logError(const std::string& component, const std::string& function, const std::string& message) const;
    
    // Thread safety for logging
    static std::mutex debugMutex_;
};