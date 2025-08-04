#include "Simulator.h"
#include "ArgParser.h"
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
#include "ThreadPool.h"
#include "SatelliteView.h"
#include "GameResult.h"

#include "ErrorLogger.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <dlfcn.h>
#include <stdexcept>

namespace fs = std::filesystem;

// Static member definitions
std::mutex Simulator::debugMutex_;
std::ofstream Simulator::errorLog_;

// Result structure implementations
ComparativeEntry::ComparativeEntry(std::string g, GameResult r, std::string fs)
    : gmName(std::move(g)), res(std::move(r)), finalState(std::move(fs)) {}

CompetitionEntry::CompetitionEntry(std::string m, std::string x, std::string y, GameResult r)
    : mapFile(std::move(m)), a1(std::move(x)), a2(std::move(y)), res(std::move(r)) {}

// Constructor
Simulator::Simulator(const Config& config) 
    : config_(config) {
    // Initialize ErrorLogger first
    ErrorLogger::instance().init();
    
    // initErrorLog();
    logInfo("SIMULATOR", "constructor", "Initializing Simulator in " + 
        std::string(config_.modeComparative ? "comparative" : "competition") + " mode");
    
    threadPool_ = std::make_unique<ThreadPool>(config_.numThreads);
    logInfo("SIMULATOR", "constructor", "Created ThreadPool with " + 
        std::to_string(config_.numThreads) + " threads");
}

// Destructor
Simulator::~Simulator() {
    
    logInfo("SIMULATOR", "destructor", "Cleaning up Simulator");
    writeMapErrors(); // Write any accumulated map errors before cleanup
    // cleanup();
}

// Main execution method
int Simulator::run() {
    logInfo("SIMULATOR", "run", "Starting simulation execution");
    
    int result = config_.modeComparative ? runComparative() : runCompetition();
    
    logInfo("SIMULATOR", "run", "Simulation completed with exit code " + std::to_string(result));
    return result;
}

// Comparative mode implementation
int Simulator::runComparative() {
    logInfo("SIMULATOR", "runComparative", "Starting comparative mode");

    // 1) Load map + params
    MapData md;
    try {
        md = loadMapWithParams(config_.game_map);
    } catch (const std::exception& ex) {
        std::string errorMsg = "Error loading map: " + std::string(ex.what());
        logError("SIMULATOR", "runComparative", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }

    // 2) Load Algorithms (must succeed)
    if (!loadAlgorithmPlugins()) {
        std::string errorMsg = "Failed to load required algorithms";
        logError("SIMULATOR", "runComparative", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }
    
    // 3) Load GameManagers (resilient)
    if (!loadGameManagerPlugins()) {
        std::string errorMsg = "Failed to load any GameManager plugins";
        logError("SIMULATOR", "runComparative", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }
    
    logInfo("SIMULATOR", "runComparative", 
        "Successfully loaded " + std::to_string(loadedGameManagers_) + " GameManager(s)");
        
    // 4) Dispatch tasks
    dispatchComparativeTasks();
        
    // 5) Write results
    writeComparativeFile(comparativeResults_);

    // 6) Report summary
    logInfo("SIMULATOR", "runComparative", "Comparative Results Summary:");
    for (const auto& e : comparativeResults_) {
        logInfo("RESULTS", "runComparative", 
            "GM=" + e.gmName + 
            " winner=" + std::to_string(e.res.winner) + 
            " reason=" + std::to_string(static_cast<int>(e.res.reason)) + 
            " rounds=" + std::to_string(e.res.rounds));
    }

    return 0;
}

// Competition mode implementation
int Simulator::runCompetition() {
    logInfo("SIMULATOR", "runCompetition", "Starting competition mode");

    // 1) Gather maps
    std::vector<std::string> maps;
    for (auto& e : fs::directory_iterator(config_.game_maps_folder)) {
        if (e.is_regular_file()) {
            maps.push_back(e.path().string());
        }
    }
    if (maps.empty()) {
        std::string errorMsg = "No files found in game_maps_folder";
        logError("SIMULATOR", "runCompetition", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }

    logDebug("SIMULATOR", "runCompetition", "Found " + std::to_string(maps.size()) + " map files");

    // 2) Load GameManager (must succeed)
    if (!loadSingleGameManager()) {
        std::string errorMsg = "Failed to load GameManager";
        logError("SIMULATOR", "runCompetition", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }

    // 3) Load Algorithms (resilient, need at least 2)
    if (!loadAlgorithmPlugins()) {
        std::string errorMsg = "Failed to load sufficient algorithms";
        logError("SIMULATOR", "runCompetition", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }

    if (loadedAlgorithms_ < 2) {
        std::string errorMsg = "Need at least 2 algorithms, found " + std::to_string(loadedAlgorithms_);
        logError("SIMULATOR", "runCompetition", errorMsg);
        LOG_ERROR(errorMsg);
        return 1;
    }

    logInfo("SIMULATOR", "runCompetition", 
        "Successfully loaded " + std::to_string(loadedAlgorithms_) + " algorithm(s)");

    // 4) Dispatch tasks
    dispatchCompetitionTasks();

    // 5) Write results
    writeCompetitionFile(competitionResults_);

    // 6) Report summary
    logInfo("SIMULATOR", "runCompetition", "Competition Results Summary:");
    for (const auto& e : competitionResults_) {
        logInfo("RESULTS", "runCompetition", 
            "Map=" + e.mapFile + 
            " A1=" + e.a1 + 
            " A2=" + e.a2 + 
            " => winner=" + std::to_string(e.res.winner) + 
            " reason=" + std::to_string(static_cast<int>(e.res.reason)) + 
            " rounds=" + std::to_string(e.res.rounds));
    }

    return 0;
}

// Map loading with enhanced error handling and flexibility
MapData Simulator::loadMapWithParams(const std::string& path) const {
    logDebug("MAPLOADER", "loadMapWithParams", "Loading map from: " + path);
    
    std::ifstream in(path);
    if (!in.is_open()) {
        std::string errorMsg = "Failed to open map file: " + path;
        mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
        LOG_ERROR(errorMsg);
        throw std::runtime_error(errorMsg);
    }

    size_t rows = 0, cols = 0, maxSteps = 0, numShells = 0;
    std::vector<std::string> rawGridLines;
    std::string line;
    bool foundRows = false, foundCols = false, foundMaxSteps = false, foundNumShells = false;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        if (line.rfind("Rows", 0) == 0) {
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                std::string errorMsg = "Invalid Rows format in " + path + ": " + line + " (missing '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            // Check for spaces around '=' sign
            if (eqPos > 4 && line[eqPos - 1] == ' ') {
                std::string errorMsg = "Invalid Rows format in " + path + ": " + line + " (space before '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            if (eqPos + 1 < line.length() && line[eqPos + 1] == ' ') {
                std::string errorMsg = "Invalid Rows format in " + path + ": " + line + " (space after '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            try {
                rows = std::stoul(line.substr(eqPos + 1));
                foundRows = true;
            } catch (const std::exception& e) {
                std::string errorMsg = "Invalid Rows value in " + path + ": " + line;
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
        } else if (line.rfind("Cols", 0) == 0) {
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                std::string errorMsg = "Invalid Cols format in " + path + ": " + line + " (missing '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            // Check for spaces around '=' sign
            if (eqPos > 4 && line[eqPos - 1] == ' ') {
                std::string errorMsg = "Invalid Cols format in " + path + ": " + line + " (space before '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            if (eqPos + 1 < line.length() && line[eqPos + 1] == ' ') {
                std::string errorMsg = "Invalid Cols format in " + path + ": " + line + " (space after '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            try {
                cols = std::stoul(line.substr(eqPos + 1));
                foundCols = true;
            } catch (const std::exception& e) {
                std::string errorMsg = "Invalid Cols value in " + path + ": " + line;
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
        } else if (line.rfind("MaxSteps", 0) == 0) {
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                std::string errorMsg = "Invalid MaxSteps format in " + path + ": " + line + " (missing '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            // Check for spaces around '=' sign
            if (eqPos > 8 && line[eqPos - 1] == ' ') {
                std::string errorMsg = "Invalid MaxSteps format in " + path + ": " + line + " (space before '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            if (eqPos + 1 < line.length() && line[eqPos + 1] == ' ') {
                std::string errorMsg = "Invalid MaxSteps format in " + path + ": " + line + " (space after '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            try {
                maxSteps = std::stoul(line.substr(eqPos + 1));
                foundMaxSteps = true;
            } catch (const std::exception& e) {
                std::string errorMsg = "Invalid MaxSteps value in " + path + ": " + line;
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
        } else if (line.rfind("NumShells", 0) == 0) {
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                std::string errorMsg = "Invalid NumShells format in " + path + ": " + line + " (missing '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            // Check for spaces around '=' sign
            if (eqPos > 9 && line[eqPos - 1] == ' ') {
                std::string errorMsg = "Invalid NumShells format in " + path + ": " + line + " (space before '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            if (eqPos + 1 < line.length() && line[eqPos + 1] == ' ') {
                std::string errorMsg = "Invalid NumShells format in " + path + ": " + line + " (space after '=' sign)";
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
            try {
                numShells = std::stoul(line.substr(eqPos + 1));
                foundNumShells = true;
            } catch (const std::exception& e) {
                std::string errorMsg = "Invalid NumShells value in " + path + ": " + line;
                mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
                LOG_ERROR(errorMsg);
                throw std::runtime_error(errorMsg);
            }
        } else {
            if (!line.empty()) {
                // Check if this looks like a grid line (contains game characters)
                bool looksLikeGrid = false;
                for (char c : line) {
                    if (c == '.' || c == '#' || c == '@' || c == '1' || c == '2' || c == ' ') {
                        looksLikeGrid = true;
                        break;
                    }
                }
                if (looksLikeGrid) {
                    rawGridLines.push_back(line);
                    logDebug("MAPLOADER", "loadMapWithParams", "Added grid line: '" + line + "'");
                } else {
                    logDebug("MAPLOADER", "loadMapWithParams", "Ignoring non-grid line: '" + line + "'");
                }
            }
        }
    }

    // Check for missing headers
    std::vector<std::string> missingHeaders;
    if (!foundRows) missingHeaders.push_back("Rows");
    if (!foundCols) missingHeaders.push_back("Cols");
    if (!foundMaxSteps) missingHeaders.push_back("MaxSteps");
    if (!foundNumShells) missingHeaders.push_back("NumShells");
    
    if (!missingHeaders.empty()) {
        std::string errorMsg = "Missing required headers in " + path + ": ";
        for (size_t i = 0; i < missingHeaders.size(); ++i) {
            errorMsg += missingHeaders[i];
            if (i + 1 < missingHeaders.size()) errorMsg += ", ";
        }
        mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
        LOG_ERROR(errorMsg);
        throw std::runtime_error(errorMsg);
    }

    // Validate dimensions
    if (rows == 0 || cols == 0) {
        std::string errorMsg = "Invalid dimensions in " + path + 
                              ": rows=" + std::to_string(rows) + 
                              ", cols=" + std::to_string(cols);
        mapLoadErrors_.push_back({path, errorMsg, currentTimestamp()});
        LOG_ERROR(errorMsg);
        throw std::runtime_error(errorMsg);
    }

    logDebug("MAPLOADER", "loadMapWithParams", 
        "Parsed map parameters - rows=" + std::to_string(rows) + 
        ", cols=" + std::to_string(cols) + 
        ", maxSteps=" + std::to_string(maxSteps) + 
        ", numShells=" + std::to_string(numShells));

    // Normalize the grid to match header dimensions
    auto normalizedGrid = normalizeGrid(rawGridLines, rows, cols);

    if (config_.debug) {
        logDebug("MAPLOADER", "loadMapWithParams", "Normalized grid:");
        for (size_t r = 0; r < normalizedGrid.size(); ++r) {
            std::string debugStr = "Row " + std::to_string(r) + ": '";
            for (char c : normalizedGrid[r]) {
                if (c == ' ') debugStr += "_";  // Show spaces as underscores for clarity
                else debugStr += c;
            }
            debugStr += "'";
            logDebug("MAPLOADER", "loadMapWithParams", debugStr);
        }
    }

    // Build SatelliteView
    class MapView : public SatelliteView {
    public:
        MapView(std::vector<std::string>&& rows)
            : rows_(std::move(rows)),
              width_(rows_.empty() ? 0 : rows_[0].size()),
              height_(rows_.size()) {}
        char getObjectAt(size_t x, size_t y) const override {
            return (y < height_ && x < width_) ? rows_[y][x] : ' ';
        }
        size_t width() const { return width_; }
        size_t height() const { return height_; }
    private:
        std::vector<std::string> rows_;
        size_t width_, height_;
    };

    MapData md;
    md.rows = rows;
    md.cols = cols;
    md.maxSteps = maxSteps;
    md.numShells = numShells;
    md.view = std::make_unique<MapView>(std::move(normalizedGrid));
    
    logDebug("MAPLOADER", "loadMapWithParams", "Map loaded successfully");
    return md;
}

// Grid normalization to handle flexible dimensions
std::vector<std::string> Simulator::normalizeGrid(const std::vector<std::string>& rawGrid, 
                                                  size_t targetRows, size_t targetCols) const {
    std::vector<std::string> normalized;
    normalized.reserve(targetRows);
    
    logDebug("MAPLOADER", "normalizeGrid", 
        "Normalizing grid from " + std::to_string(rawGrid.size()) + " rows to " + 
        std::to_string(targetRows) + " rows, " + std::to_string(targetCols) + " cols");

    for (size_t row = 0; row < targetRows; ++row) {
        std::string normalizedRow;
        normalizedRow.reserve(targetCols);
        
        if (row < rawGrid.size()) {
            // Process existing row
            const std::string& sourceRow = rawGrid[row];
            
            for (size_t col = 0; col < targetCols; ++col) {
                if (col < sourceRow.size()) {
                    // Normalize the character
                    normalizedRow += normalizeCell(sourceRow[col]);
                } else {
                    // Fill missing columns with spaces
                    normalizedRow += ' ';
                }
            }
            
            // Log if we truncated columns
            if (sourceRow.size() > targetCols) {
                logDebug("MAPLOADER", "normalizeGrid", 
                    "Row " + std::to_string(row) + " truncated from " + 
                    std::to_string(sourceRow.size()) + " to " + std::to_string(targetCols) + " cols");
            }
            // Log if we padded columns
            else if (sourceRow.size() < targetCols) {
                logDebug("MAPLOADER", "normalizeGrid", 
                    "Row " + std::to_string(row) + " padded from " + 
                    std::to_string(sourceRow.size()) + " to " + std::to_string(targetCols) + " cols");
            }
        } else {
            // Fill missing rows with spaces
            normalizedRow = std::string(targetCols, ' ');
            logDebug("MAPLOADER", "normalizeGrid", 
                "Row " + std::to_string(row) + " created as empty (missing from source)");
        }
        
        normalized.push_back(std::move(normalizedRow));
    }
    
    // Log if we ignored extra rows
    if (rawGrid.size() > targetRows) {
        logDebug("MAPLOADER", "normalizeGrid", 
            "Ignored " + std::to_string(rawGrid.size() - targetRows) + " extra rows");
    }
    
    return normalized;
}

// Character normalization - only keep valid game objects
char Simulator::normalizeCell(char c) const {
    switch (c) {
        case '@':  // Mine
        case '#':  // Wall  
        case '1':  // Tank player 1
        case '2':  // Tank player 2
            logDebug("MAPLOADER", "normalizeCell", "Keeping character: '" + std::string(1, c) + "'");
            return c;
        default:
            if (config_.debug && c != '.' && c != ' ') {
                logDebug("MAPLOADER", "normalizeCell", "Converting '" + std::string(1, c) + "' to space");
            }
            return ' ';  // Empty cell (includes original '.' and any other chars)
    }
}

// Write map loading errors to file
void Simulator::writeMapErrors() const {
    if (mapLoadErrors_.empty()) {
        return; // No errors to write
    }
    
    auto ts = currentTimestamp();
    std::string errorFileName = "input_errors_" + ts + ".txt";
    
    std::ofstream errFile(errorFileName);
    if (!errFile.is_open()) {
        std::string warnMsg = "Cannot create map errors file: " + errorFileName + ", printing to stderr";
        logWarn("FILEWRITER", "writeMapErrors", warnMsg);
        LOG_ERROR(warnMsg);
        
        // Fallback to stderr
        std::cerr << "\n=== MAP LOADING ERRORS ===\n";
        for (const auto& error : mapLoadErrors_) {
            std::string errorLine = "Map: " + error.mapPath + " Time: " + error.timestamp + " Error: " + error.errorReason;
            std::cerr << errorLine << "\n";
            LOG_ERROR(errorLine);
        }
        return;
    }
    
    logInfo("FILEWRITER", "writeMapErrors", 
        "Writing " + std::to_string(mapLoadErrors_.size()) + 
        " map errors to: " + errorFileName);
    
    errFile << "=== MAP LOADING ERRORS ===\n";
    errFile << "Generated: " << ts << "\n\n";
    
    for (const auto& error : mapLoadErrors_) {
        errFile << "Map: " << error.mapPath << "\n";
        errFile << "Time: " << error.timestamp << "\n";
        errFile << "Error: " << error.errorReason << "\n";
        errFile << "----------------------------------------\n\n";
    }
    
    errFile.close();
    logInfo("FILEWRITER", "writeMapErrors", "Map errors file written successfully");
}

// Algorithm plugin loading - WITH TIMING PRESERVATION
bool Simulator::loadAlgorithmPlugins() {
    // Add small delay to preserve timing that prevents segfault
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    try {
        auto& algoReg = AlgorithmRegistrar::get();
        
        if (config_.modeComparative) {
            logDebug("PLUGINLOADER", "loadAlgorithmPlugins", "Loading 2 algorithm plugins for comparative mode");
            
            std::vector<std::string> algPaths = {config_.algorithm1, config_.algorithm2};
            
            for (size_t i = 0; i < algPaths.size(); ++i) {
                const auto& algPath = algPaths[i];
                
                // Check if file exists first
                if (!std::filesystem::exists(algPath)) {
                    std::string errorMsg = "Algorithm file does not exist: " + algPath;
                    logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
                    LOG_ERROR(errorMsg);
                    return false;
                }
                
                std::string name = stripSoExtension(algPath);
                logDebug("PLUGINLOADER", "loadAlgorithmPlugins", "Loading algorithm: " + algPath);
                
                try {
                    algoReg.createAlgorithmFactoryEntry(name);
                } catch (const std::exception& e) {
                    std::string errorMsg = "createAlgorithmFactoryEntry failed: " + std::string(e.what());
                    logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
                    LOG_ERROR(errorMsg);
                    return false;
                } catch (...) {
                    std::string errorMsg = "createAlgorithmFactoryEntry failed with unknown exception";
                    logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
                    LOG_ERROR(errorMsg);
                    return false;
                }
                
                void* h = dlopen(algPath.c_str(), RTLD_NOW);
                
                if (!h) {
                    const char* dlerr = dlerror();
                    std::string errorMsg = "dlopen failed for algorithm '" + name + "': " + std::string(dlerr ? dlerr : "unknown");
                    logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
                    LOG_ERROR(errorMsg);
                    
                    // Clean up registry entry
                    try {
                        algoReg.removeLast();
                    } catch (...) {
                        logWarn("PLUGINLOADER", "loadAlgorithmPlugins", "Failed to remove last registry entry");
                    }
                    return false;
                }
                
                try { 
                    algoReg.validateLastRegistration();
                } catch (const std::exception& e) {
                    std::string errorMsg = "Registration validation failed for algorithm '" + name + "': " + e.what();
                    logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
                    LOG_ERROR(errorMsg);
                    algoReg.removeLast();
                    dlclose(h);
                    return false;
                } catch (...) {
                    std::string errorMsg = "Registration validation failed for algorithm '" + name + "'";
                    logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
                    LOG_ERROR(errorMsg);
                    algoReg.removeLast();
                    dlclose(h);
                    return false;
                }
                
                logDebug("PLUGINLOADER", "loadAlgorithmPlugins", 
                    "Algorithm '" + name + "' loaded and validated successfully");
                algorithmHandles_.push_back(h);
                validAlgorithmPaths_.push_back(algPath);
                loadedAlgorithms_++;
            }
            
            return true;
        } else {
            // Competition mode - load all algorithms from folder
            logDebug("PLUGINLOADER", "loadAlgorithmPlugins", 
                "Loading algorithm plugins from '" + config_.algorithms_folder + "'");
            
            for (auto& e : fs::directory_iterator(config_.algorithms_folder)) {
                if (e.path().extension() == ".so") {
                    std::string path = e.path().string();
                    std::string name = stripSoExtension(path);
                    
                    logDebug("PLUGINLOADER", "loadAlgorithmPlugins", "Loading algorithm: " + path);
                    algoReg.createAlgorithmFactoryEntry(name);
                    void* h = dlopen(path.c_str(), RTLD_NOW);
                    if (!h) {
                        std::string warnMsg = "dlopen failed for algorithm '" + path + "': " + std::string(dlerror());
                        logWarn("PLUGINLOADER", "loadAlgorithmPlugins", warnMsg);
                        LOG_ERROR(warnMsg);
                        algoReg.removeLast();
                        continue;
                    }
                    try { 
                        algoReg.validateLastRegistration(); 
                    } catch (...) {
                        std::string warnMsg = "Registration validation failed for algorithm '" + path + "'";
                        logWarn("PLUGINLOADER", "loadAlgorithmPlugins", warnMsg);
                        LOG_ERROR(warnMsg);
                        algoReg.removeLast();
                        dlclose(h);
                        continue;
                    }
                    logDebug("PLUGINLOADER", "loadAlgorithmPlugins", 
                        "Algorithm '" + name + "' loaded and validated successfully");
                    algorithmHandles_.push_back(h);
                    validAlgorithmPaths_.push_back(path);
                    loadedAlgorithms_++;
                }
            }
            return loadedAlgorithms_ >= 2;
        }
        
    } catch (const std::exception& e) {
        std::string errorMsg = "Exception: " + std::string(e.what());
        logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        LOG_ERROR(errorMsg);
        return false;
    } catch (...) {
        std::string errorMsg = "Unknown exception";
        logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        LOG_ERROR(errorMsg);
        return false;
    }
}

// GameManager plugin loading
bool Simulator::loadGameManagerPlugins() {
    auto& gmReg = GameManagerRegistrar::get();
    
    logDebug("PLUGINLOADER", "loadGameManagerPlugins", 
        "Loading GameManager plugins from '" + config_.game_managers_folder + "'");
    
    for (auto& e : fs::directory_iterator(config_.game_managers_folder)) {
        if (e.path().extension() == ".so") {
            std::string path = e.path().string();
            std::string name = stripSoExtension(path);
            
            logDebug("PLUGINLOADER", "loadGameManagerPlugins", "Loading GameManager: " + path);
            gmReg.createGameManagerEntry(name);
            void* h = dlopen(path.c_str(), RTLD_NOW);
            if (!h) {
                std::string warnMsg = "dlopen failed for GameManager '" + name + "': " + std::string(dlerror());
                logWarn("PLUGINLOADER", "loadGameManagerPlugins", warnMsg);
                LOG_ERROR(warnMsg);
                gmReg.removeLast();
                continue;
            }
            
            try { 
                gmReg.validateLastRegistration(); 
            } catch (...) {
                std::string warnMsg = "Registration validation failed for GameManager '" + name + "'";
                logWarn("PLUGINLOADER", "loadGameManagerPlugins", warnMsg);
                LOG_ERROR(warnMsg);
                gmReg.removeLast();
                dlclose(h);
                continue;
            }
            
            logDebug("PLUGINLOADER", "loadGameManagerPlugins", 
                "GameManager '" + name + "' loaded and validated successfully");
            gameManagerHandles_.push_back(h);
            validGameManagerPaths_.push_back(path);
            loadedGameManagers_++;
        }
    }
    
    return loadedGameManagers_ > 0;
}

// Single GameManager loading for competition mode
bool Simulator::loadSingleGameManager() {
    auto& gmReg = GameManagerRegistrar::get();
    std::string gmName = stripSoExtension(config_.game_manager);
    
    logDebug("PLUGINLOADER", "loadSingleGameManager", "Loading GameManager: " + config_.game_manager);
    gmReg.createGameManagerEntry(gmName);
    void* gmH = dlopen(config_.game_manager.c_str(), RTLD_NOW);
    if (!gmH) {
        const char* dlerr = dlerror();
        std::string errorMsg = "dlopen failed for GameManager: " + std::string(dlerr ? dlerr : "unknown");
        LOG_ERROR_FMT("dlopen failed for GameManager %s: %s", config_.game_manager.c_str(), dlerr ? dlerr : "unknown");
        logError("PLUGINLOADER", "loadSingleGameManager", errorMsg);
        LOG_ERROR(errorMsg);
        gmReg.removeLast();
        return false;
    }
    try { 
        gmReg.validateLastRegistration(); 
    } catch (...) {
        std::string errorMsg = "GameManager registration validation failed for '" + gmName + "'";
        logError("PLUGINLOADER", "loadSingleGameManager", errorMsg);
        LOG_ERROR(errorMsg);
        gmReg.removeLast();
        dlclose(gmH);
        return false;
    }

    logDebug("PLUGINLOADER", "loadSingleGameManager", 
        "GameManager '" + gmName + "' loaded and validated successfully");
    gameManagerHandles_.push_back(gmH);
    loadedGameManagers_ = 1;
    return true;
}

// Task dispatching for comparative mode
void Simulator::dispatchComparativeTasks() {
    logInfo("THREADPOOL", "dispatchComparativeTasks", 
        "Starting game execution with " + std::to_string(config_.numThreads) + " threads");

    auto& gmReg = GameManagerRegistrar::get();
    auto& algoReg = AlgorithmRegistrar::get();
    
    // Load map data
    MapData md;
    try {
        md = loadMapWithParams(config_.game_map);
    } catch (const std::exception& ex) {
        std::string errorMsg = "Error loading map: " + std::string(ex.what());
        logError("SIMULATOR", "dispatchComparativeTasks", errorMsg);
        LOG_ERROR(errorMsg);
        return;
    }
    SatelliteView& realMap = *md.view;

    for (size_t gi = 0; gi < loadedGameManagers_; ++gi) {
        auto& gmEntry = *(gmReg.begin() + gi);
        auto& A = *(algoReg.begin() + 0);
        auto& B = *(algoReg.begin() + 1);

        threadPool_->enqueue([this, &gmEntry, &A, &B, &md, &realMap, gi] {
            logDebug("GAMETHREAD", "worker", "Starting game execution for GM index " + std::to_string(gi));
            
            auto gm = gmEntry.factory(config_.verbose);
            auto p1 = A.createPlayer(0, md.rows, md.cols, md.maxSteps, md.numShells);
            auto a1 = A.createTankAlgorithm(0, 0);
            auto p2 = B.createPlayer(1, md.rows, md.cols, md.maxSteps, md.numShells);
            auto a2 = B.createTankAlgorithm(1, 0);

            logDebug("GAMETHREAD", "worker", "Created players and algorithms, starting game run");

            GameResult gr = gm->run(
                md.cols, md.rows,
                realMap,
                config_.game_map,
                md.maxSteps, md.numShells,
                *p1, stripSoExtension(config_.algorithm1),
                *p2, stripSoExtension(config_.algorithm2),
                [&](int pi, int ti) { return A.createTankAlgorithm(pi, ti); },
                [&](int pi, int ti) { return B.createTankAlgorithm(pi, ti); }
            );
            
            logDebug("GAMETHREAD", "worker", "Game execution completed for GM index " + std::to_string(gi));
            
            std::ostringstream ss;
            auto* state = gr.gameState.get();
            
            logDebug("GAMETHREAD", "worker", "Building final map state for GM index " + std::to_string(gi));
            for (size_t y = 0; y < md.rows; ++y) {
                for (size_t x = 0; x < md.cols; ++x) {
                    ss << state->getObjectAt(x, y);
                }
                ss << '\n';
            }
            
            std::string finalMap = ss.str();
            logDebug("GAMETHREAD", "worker", "Final map state built, storing results for GM index " + std::to_string(gi));
            
            std::lock_guard<std::mutex> lock(resultsMutex_);
            
            auto* state_ptr = gr.gameState.get();
            std::fprintf(stderr,
                 "[Simulator] inserting ComparativeEntry with gameState ptr=%p for GM index %zu\n",
            static_cast<void*>(state_ptr), gi);
            comparativeResults_.emplace_back(
                stripSoExtension(validGameManagerPaths_[gi]),
                std::move(gr),
                std::move(finalMap)
            );
            totalGamesPlayed_++;
        });
    }
    
    logInfo("THREADPOOL", "dispatchComparativeTasks", "All tasks enqueued, waiting for completion");
    threadPool_->shutdown();
    threadPool_ = std::make_unique<ThreadPool>(config_.numThreads); // Reset for potential reuse
}

// Task dispatching for competition mode
void Simulator::dispatchCompetitionTasks() {
    // 1) Gather maps
    std::vector<std::string> allMapFiles;
    for (auto& e : fs::directory_iterator(config_.game_maps_folder)) {
        if (e.is_regular_file()) {
            allMapFiles.push_back(e.path().string());
        }
    }

    // 2) Preload maps and get only the valid ones
    std::vector<size_t> mapRows, mapCols, mapMaxSteps, mapNumShells;
    std::vector<std::string> validMapFiles; // Track which maps were successfully loaded
    auto mapViews = preloadMapsAndTrackValid(allMapFiles, validMapFiles, mapRows, mapCols, mapMaxSteps, mapNumShells);
    
    if (mapViews.empty()) {
        std::string errorMsg = "No valid maps to run";
        logError("SIMULATOR", "dispatchCompetitionTasks", errorMsg);
        LOG_ERROR(errorMsg);
        return;
    }

    auto& gmReg = GameManagerRegistrar::get();
    auto& algoReg = AlgorithmRegistrar::get();
    auto& gmEntry = *gmReg.begin();

    // Calculate total number of games using only valid maps
    size_t totalGames = 0;
    for (size_t i = 0; i + 1 < loadedAlgorithms_; ++i) {
        for (size_t j = i + 1; j < loadedAlgorithms_; ++j) {
            totalGames += mapViews.size();
        }
    }

    logInfo("THREADPOOL", "dispatchCompetitionTasks", 
        "Starting execution of " + std::to_string(totalGames) + " total games with " + 
        std::to_string(config_.numThreads) + " threads");

    for (size_t mi = 0; mi < mapViews.size(); ++mi) {
        auto mapViewPtr = mapViews[mi];
        size_t cols = mapCols[mi],
               rows = mapRows[mi],
               mSteps = mapMaxSteps[mi],
               nShells = mapNumShells[mi];
        const std::string mapFile = validMapFiles[mi]; // Use valid map file names
        SatelliteView& realMap = *mapViewPtr;

        for (size_t i = 0; i + 1 < loadedAlgorithms_; ++i) {
            for (size_t j = i + 1; j < loadedAlgorithms_; ++j) {
                threadPool_->enqueue([this, i, j, &realMap, &algoReg, &gmEntry, 
                                    cols, rows, mSteps, nShells, mapFile] {
                    std::string algo1Name = stripSoExtension(validAlgorithmPaths_[i]);
                    std::string algo2Name = stripSoExtension(validAlgorithmPaths_[j]);
                    
                    logDebug("GAMETHREAD", "worker", 
                        "Starting game: " + algo1Name + " vs " + algo2Name + " on map " + mapFile);
                    
                    auto gm = gmEntry.factory(config_.verbose);
                    auto& A = *(algoReg.begin() + i);
                    auto& B = *(algoReg.begin() + j);
                    auto p1 = A.createPlayer(0, rows, cols, mSteps, nShells);
                    auto a1 = A.createTankAlgorithm(0, 0);
                    auto p2 = B.createPlayer(1, rows, cols, mSteps, nShells);
                    auto a2 = B.createTankAlgorithm(1, 0);

                    logDebug("GAMETHREAD", "worker", "Created players and algorithms, executing game");

                    GameResult gr = gm->run(
                        cols, rows,
                        realMap,
                        mapFile,
                        mSteps, nShells,
                        *p1, algo1Name,
                        *p2, algo2Name,
                        [&](int pi, int ti) { return A.createTankAlgorithm(pi, ti); },
                        [&](int pi, int ti) { return B.createTankAlgorithm(pi, ti); }
                    );

                    logDebug("GAMETHREAD", "worker", 
                        "Game completed: " + algo1Name + " vs " + algo2Name + 
                        " winner=" + std::to_string(gr.winner) + 
                        " rounds=" + std::to_string(gr.rounds));

                    std::lock_guard<std::mutex> lock(resultsMutex_);
                    competitionResults_.emplace_back(
                        mapFile,
                        algo1Name,
                        algo2Name,
                        std::move(gr)
                    );
                    totalGamesPlayed_++;
                });
            }
        }
    }

    logInfo("THREADPOOL", "dispatchCompetitionTasks", "All tasks enqueued, waiting for completion");
    threadPool_->shutdown();
    threadPool_ = std::make_unique<ThreadPool>(config_.numThreads); // Reset for potential reuse
}

// Enhanced map preprocessing that tracks valid maps
std::vector<std::shared_ptr<SatelliteView>> Simulator::preloadMapsAndTrackValid(
    const std::vector<std::string>& mapFiles,
    std::vector<std::string>& validMapFiles,
    std::vector<size_t>& mapRows,
    std::vector<size_t>& mapCols, 
    std::vector<size_t>& mapMaxSteps,
    std::vector<size_t>& mapNumShells) const {
    
    std::vector<std::shared_ptr<SatelliteView>> mapViews;
    
    logDebug("SIMULATOR", "preloadMapsAndTrackValid", "Preloading map data into shared structures");
    
    for (const auto& mapFile : mapFiles) {
        try {
            MapData md = loadMapWithParams(mapFile);
            mapViews.emplace_back(std::move(md.view));
            mapCols.push_back(md.cols);
            mapRows.push_back(md.rows);
            mapMaxSteps.push_back(md.maxSteps);
            mapNumShells.push_back(md.numShells);
            validMapFiles.push_back(mapFile); // Only add if successful
            logDebug("MAPLOADER", "preloadMapsAndTrackValid", "Successfully preloaded map: " + mapFile);
        } catch (const std::exception& ex) {
            std::string warnMsg = "Skipping invalid map '" + mapFile + "': " + std::string(ex.what());
            logWarn("MAPLOADER", "preloadMapsAndTrackValid", warnMsg);
            LOG_ERROR(warnMsg);
            // Error already recorded in mapLoadErrors_ by loadMapWithParams
        }
    }
    
    logInfo("SIMULATOR", "preloadMapsAndTrackValid", "Successfully preloaded " + std::to_string(mapViews.size()) + " valid map(s)");
    
    // Write errors at the end of preloading if any accumulated
    if (!mapLoadErrors_.empty()) {
        writeMapErrors();
    }
    
    return mapViews;
}

// Map preprocessing for competition mode (legacy method, kept for comparative mode)
std::vector<std::shared_ptr<SatelliteView>> Simulator::preloadMaps(
    const std::vector<std::string>& mapFiles,
    std::vector<size_t>& mapRows,
    std::vector<size_t>& mapCols, 
    std::vector<size_t>& mapMaxSteps,
    std::vector<size_t>& mapNumShells) const {
    
    std::vector<std::string> validMapFiles; // Dummy - not used in this version
    return preloadMapsAndTrackValid(mapFiles, validMapFiles, mapRows, mapCols, mapMaxSteps, mapNumShells);
}

// Outcome message formatting
std::string Simulator::outcomeMessage(int winner, GameResult::Reason reason) const {
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

// Comparative results file writing
bool Simulator::writeComparativeFile(const std::vector<ComparativeEntry>& entries) const {
    logInfo("FILEWRITER", "writeComparativeFile", "Writing comparative results file");

    // Group GMs by identical outcome
    struct Outcome {
        int winner;
        GameResult::Reason reason;
        size_t rounds;
        std::string finalState;
        bool operator<(const Outcome& o) const {
            if (winner != o.winner) return winner < o.winner;
            if (reason != o.reason) return reason < o.reason;
            if (rounds != o.rounds) return rounds < o.rounds;
            return finalState < o.finalState;
        }
    };

    std::map<Outcome, std::vector<std::string>> groups;
    for (const auto& e : entries) {
        Outcome key{
            e.res.winner,
            e.res.reason,
            e.res.rounds,
            e.finalState
        };
        groups[key].push_back(e.gmName);
    }

    logDebug("FILEWRITER", "writeComparativeFile", 
        "Grouped results into " + std::to_string(groups.size()) + " outcome categories");

    // Build output path
    auto ts = currentTimestamp();
    fs::path outPath = fs::path(config_.game_managers_folder)
                     / ("comparative_results_" + ts + ".txt");

    // Open file (or fallback)
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        std::string warnMsg = "Cannot create file " + outPath.string() + ", falling back to stdout";
        logWarn("FILEWRITER", "writeComparativeFile", warnMsg);
        LOG_ERROR(warnMsg);
        
        // Fallback to stdout
        std::cout << "game_map="   << config_.game_map   << "\n";
        std::cout << "algorithm1=" << stripSoExtension(config_.algorithm1) << "\n";
        std::cout << "algorithm2=" << stripSoExtension(config_.algorithm2) << "\n\n";
        for (const auto& [outcome, gms] : groups) {
            for (size_t i = 0; i < gms.size(); ++i) {
                std::cout << gms[i] << (i + 1 < gms.size() ? "," : "");
            }
            std::cout << "\n";
            std::cout << outcomeMessage(outcome.winner, outcome.reason) << "\n";
            std::cout << outcome.rounds << "\n";
            std::cout << outcome.finalState << "\n\n";
        }
        return false;
    }

    logInfo("FILEWRITER", "writeComparativeFile", "Writing to file: " + outPath.string());

    // Write header
    ofs << "game_map="   << config_.game_map   << "\n";
    ofs << "algorithm1=" << stripSoExtension(config_.algorithm1) << "\n";
    ofs << "algorithm2=" << stripSoExtension(config_.algorithm2) << "\n\n";

    // Write grouped results
    for (const auto& [outcome, gms] : groups) {
        for (size_t i = 0; i < gms.size(); ++i) {
            ofs << gms[i] << (i + 1 < gms.size() ? "," : "");
        }
        ofs << "\n";
        ofs << outcomeMessage(outcome.winner, outcome.reason) << "\n";
        ofs << outcome.rounds << "\n";
        ofs << outcome.finalState << "\n\n";
    }
    
    logInfo("FILEWRITER", "writeComparativeFile", "Comparative results file written successfully");
    return true;
}

// Competition results file writing
bool Simulator::writeCompetitionFile(const std::vector<CompetitionEntry>& results) const {
    logInfo("FILEWRITER", "writeCompetitionFile", "Writing competition results file");

    // Tally scores
    std::map<std::string, int> scores;
    for (const auto& entry : results) {
        const auto& a1 = entry.a1;
        const auto& a2 = entry.a2;
        if (!scores.count(a1)) {
            scores[a1] = 0;
        }
        if (!scores.count(a2)) {
            scores[a2] = 0;
        }
        int w = entry.res.winner;  // 0=tie, 1=a1, 2=a2

        if (w == 1) {
            scores[a1] += 3;
        } else if (w == 2) {
            scores[a2] += 3;
        } else {
            // tie
            scores[a1] += 1;
            scores[a2] += 1;
        }
    }

    logDebug("FILEWRITER", "writeCompetitionFile", 
        "Calculated scores for " + std::to_string(scores.size()) + " algorithms");

    // Sort by descending score
    std::vector<std::pair<std::string, int>> sorted(scores.begin(), scores.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& L, const auto& R) {
            return L.second > R.second;
        });

    // Build output path
    auto ts = currentTimestamp();
    fs::path outPath = fs::path(config_.algorithms_folder)
                     / ("competition_" + ts + ".txt");

    // Open file (or fallback to stdout)
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        std::string warnMsg = "Cannot create file " + outPath.string() + ", falling back to stdout";
        logWarn("FILEWRITER", "writeCompetitionFile", warnMsg);
        LOG_ERROR(warnMsg);
        
        // Fallback print
        std::cout << "game_maps_folder=" << config_.game_maps_folder << "\n";
        std::cout << "game_manager="     << stripSoExtension(config_.game_manager) << "\n\n";
        for (const auto& p : sorted) {
            std::cout << p.first << " " << p.second << "\n";
        }
        return false;
    }

    logInfo("FILEWRITER", "writeCompetitionFile", "Writing to file: " + outPath.string());

    // Write contents
    ofs << "game_maps_folder=" << config_.game_maps_folder << "\n";
    ofs << "game_manager="     << stripSoExtension(config_.game_manager) << "\n\n";
    for (const auto& p : sorted) {
        ofs << p.first << " " << p.second << "\n";
    }

    logInfo("FILEWRITER", "writeCompetitionFile", "Competition results file written successfully");
    return true;
}

// Utility methods
std::string Simulator::stripSoExtension(const std::string& path) const {
    auto fname = fs::path(path).filename().string();
    if (auto pos = fname.rfind(".so"); pos != std::string::npos) {
        return fname.substr(0, pos);
    }
    return fname;
}

std::string Simulator::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return ss.str();
}

// Cleanup
void Simulator::cleanup() {
    logInfo("SIMULATOR", "cleanup", "Cleaning up dynamic library handles");
    if(config_.modeCompetition) {
    // Clear registrars
    AlgorithmRegistrar::get().clear();
    GameManagerRegistrar::get().clear();
    
    for (auto* handle : algorithmHandles_) {
        if (handle) dlclose(handle);
    }
    for (auto* handle : gameManagerHandles_) {
        if (handle) dlclose(handle);
    }
    
    algorithmHandles_.clear();
    gameManagerHandles_.clear();
}
    logInfo("SIMULATOR", "cleanup", "Cleanup completed");
}

// Logging methods
void Simulator::logInfo(const std::string& component, const std::string& function, const std::string& message) const {
    std::lock_guard<std::mutex> lock(debugMutex_);
    std::cout << "[T" << std::this_thread::get_id() << "] [INFO] [" << component << "] [" << function << "] " << message << std::endl;
}

void Simulator::logDebug(const std::string& component, const std::string& function, const std::string& message) const {
    if (config_.debug) {
        std::lock_guard<std::mutex> lock(debugMutex_);
        std::cout << "[T" << std::this_thread::get_id() << "] [" << component << "] [" << function << "] " << message << std::endl;
    }
}

void Simulator::logWarn(const std::string& component, const std::string& function, const std::string& message) const {
    std::lock_guard<std::mutex> lock(debugMutex_);
    std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" << component << "] [" << function << "] " << message << std::endl;
    // Also log warnings to ErrorLogger
    LOG_ERROR("WARN: [" + component + "] [" + function + "] " + message);
}

void Simulator::logError(const std::string& component, const std::string& function, const std::string& message) const {
    std::lock_guard<std::mutex> lock(debugMutex_);
    std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" << component << "] [" << function << "] " << message << std::endl;
    // Also log errors to ErrorLogger
    LOG_ERROR("ERROR: [" + component + "] [" + function + "] " + message);
}

// Static error log initialization
// void Simulator::initErrorLog() {
//     errorLog_.open("errors.txt", std::ios::app);
// }