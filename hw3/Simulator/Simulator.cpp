#include "Simulator.h"
#include "ArgParser.h"
#include "ThreadPool.h"
#include "SatelliteView.h"
#include "GameResult.h"
#include "ErrorLogger.h"

#include <set>
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

using namespace UserCommon_315634022;
namespace fs = std::filesystem;

// Static member definitions
std::mutex Simulator::debugMutex_;
// std::ofstream Simulator::errorLog_;

// Result structure implementations
ComparativeEntry::ComparativeEntry(std::string g, GameResult r, std::string fs)
    : gmName(std::move(g)), res(std::move(r)), finalState(std::move(fs)) {}

CompetitionEntry::CompetitionEntry(std::string m, std::string x, std::string y, GameResult r)
    : mapFile(std::move(m)), a1(std::move(x)), a2(std::move(y)), res(std::move(r)) {}

// Constructor
Simulator::Simulator(const Config& config) 
    : config_(config) {
    logInfo("SIMULATOR", "constructor", "Initializing Simulator in " + 
        std::string(config_.modeComparative ? "comparative" : "competition") + " mode");
    
    threadPool_ = std::make_unique<ThreadPool>(config_.numThreads);
    logInfo("SIMULATOR", "constructor", "Created ThreadPool with " + 
        std::to_string(config_.numThreads) + " threads");
}

// Destructor
Simulator::~Simulator() {
    logInfo("SIMULATOR", "destructor", "Cleaning up Simulator");
    // writeMapErrors(); // Write any accumulated map errors before cleanup
    // cleanup();
}

// Main execution method
int Simulator::run() {
    logInfo("SIMULATOR", "run", "Starting simulation execution");
    
    int result = config_.modeComparative ? runComparative() : runCompetition();
    
    logInfo("SIMULATOR", "run", "Simulation completed with exit code " + std::to_string(result));
    return result;
}
// ---------- small utils ----------
std::string Simulator::baseName(const std::string& path) const {
    std::string name = path;
    size_t last_slash = name.find_last_of("/\\");
    if (last_slash != std::string::npos) name = name.substr(last_slash + 1);
    size_t last_dot = name.find_last_of('.');
    if (last_dot != std::string::npos) name = name.substr(0, last_dot);
    return name;
}
// Comparative mode implementation
int Simulator::runComparative() {
    logInfo("SIMULATOR", "runComparative", "Starting comparative mode");
    MapData md;
    // try {
    //     md = loadMapWithParams(config_.game_map);
    // } catch (const std::exception& ex) {
    //     std::string errorMsg = "Error loading map: " + std::string(ex.what());
    //     logError("SIMULATOR", "runComparative", errorMsg);
    //     return 1;
    // }
    if (!loadAlgorithmPlugins()) {
        std::string errorMsg = "Failed to load required algorithms";
        logError("SIMULATOR", "runComparative", errorMsg);
        return 1;
    }
    if (!loadGameManagerPlugins()) {
        std::string errorMsg = "Failed to load any GameManager plugins";
        logError("SIMULATOR", "runComparative", errorMsg);
        return 1;
    }
    logInfo("SIMULATOR", "runComparative", "Successfully loaded " + std::to_string(loadedGameManagers_) + " GameManager(s)");
    dispatchComparativeTasks();
    writeComparativeFile(comparativeResults_);
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
    std::vector<std::string> maps;
    for (auto& e : fs::directory_iterator(config_.game_maps_folder)) {
        if (e.is_regular_file()) {maps.push_back(e.path().string());}
    }
    if (maps.empty()) {
        std::string errorMsg = "No files found in game_maps_folder";
        logError("SIMULATOR", "runCompetition", errorMsg);
        return 1;
    }
    logDebug("SIMULATOR", "runCompetition", "Found " + std::to_string(maps.size()) + " map files");
    if (!loadSingleGameManager()) {
        std::string errorMsg = "Failed to load GameManager";
        logError("SIMULATOR", "runCompetition", errorMsg);
        return 1;
    }
    if (!loadAlgorithmPlugins()) {
        std::string errorMsg = "Failed to load sufficient algorithms";
        logError("SIMULATOR", "runCompetition", errorMsg);
        return 1;
    }
    if (loadedAlgorithms_ < 2) {
        std::string errorMsg = "Need at least 2 algorithms, found " + std::to_string(loadedAlgorithms_);
        logError("SIMULATOR", "runCompetition", errorMsg);
        return 1;
    }
    logInfo("SIMULATOR", "runCompetition", "Successfully loaded " + std::to_string(loadedAlgorithms_) + " algorithm(s)");
    dispatchCompetitionTasks();
    writeCompetitionFile(competitionResults_);
    logInfo("SIMULATOR", "runCompetition", "Competition Results Summary:");
    for (const auto& e : competitionResults_) {
        logInfo("RESULTS", "runCompetition", "Map=" + e.mapFile + " A1=" + e.a1 + " A2=" + e.a2 + " => winner=" + std::to_string(e.res.winner) + " reason=" + std::to_string(static_cast<int>(e.res.reason)) + " rounds=" + std::to_string(e.res.rounds));
    }
    return 0;
}

// Helper method to parse parameter lines with flexible spacing around '='
bool Simulator::parseParameter(const std::string& line, const std::string& paramName, 
                              size_t& value, const std::string& path) const {
    validateParameterStart(line, paramName, path);
    auto eqPos = findEqualsSign(line, path);
    std::string beforeEquals = extractAndTrimBeforeEquals(line, eqPos);
    validateParameterName(beforeEquals, paramName, line, path);
    std::string afterEquals = extractAndTrimAfterEquals(line, eqPos);
    parseParameterValue(afterEquals, paramName, value, line, path);
    return true;
}

void Simulator::validateParameterStart(const std::string& line, const std::string& paramName, 
                                     const std::string& path) const {
    if (line.find(paramName) != 0) {
        std::string errorMsg = "Parameter line doesn't start with '" + paramName + "' in " + path + ": " + line;
        logError("SIMULATOR", "parseParameter", errorMsg);
        throw std::runtime_error(errorMsg);
    }
}

size_t Simulator::findEqualsSign(const std::string& line, const std::string& path) const {
    auto eqPos = line.find('=');
    if (eqPos == std::string::npos) {
        std::string errorMsg = "Missing '=' in parameter line in " + path + ": " + line;
        logError("SIMULATOR", "parseParameter", errorMsg);
        throw std::runtime_error(errorMsg);
    }
    return eqPos;
}

std::string Simulator::extractAndTrimBeforeEquals(const std::string& line, size_t eqPos) const {
    std::string beforeEquals = line.substr(0, eqPos);
    beforeEquals.erase(beforeEquals.find_last_not_of(" \t") + 1); // trim right
    return beforeEquals;
}

void Simulator::validateParameterName(const std::string& beforeEquals, const std::string& paramName,
                                     const std::string& line, const std::string& path) const {
    if (beforeEquals != paramName) {
        std::string errorMsg = "Invalid parameter name in " + path + ": expected '" + 
                              paramName + "', got '" + beforeEquals + "' in line: " + line;
        logError("SIMULATOR", "parseParameter", errorMsg);
        throw std::runtime_error(errorMsg);
    }
}

std::string Simulator::extractAndTrimAfterEquals(const std::string& line, size_t eqPos) const {
    std::string afterEquals = line.substr(eqPos + 1);
    afterEquals.erase(0, afterEquals.find_first_not_of(" \t")); // trim left
    afterEquals.erase(afterEquals.find_last_not_of(" \t") + 1); // trim right
    return afterEquals;
}

void Simulator::parseParameterValue(const std::string& afterEquals, const std::string& paramName,
                                   size_t& value, const std::string& line, const std::string& path) const {
    try {
        value = std::stoul(afterEquals);
    } catch (const std::exception& e) {
        std::string errorMsg = "Invalid " + paramName + " value in " + path + ": '" + 
                              afterEquals + "' in line: " + line;
        logError("SIMULATOR", "parseParameter", errorMsg);
        throw std::runtime_error(errorMsg);
    }
}

// Character cleaning - only keep valid game objects, replace invalid with space
char Simulator::cleanCharacter(char c) const {
    switch (c) {
        case '@':  // Mine
        case '#':  // Wall  
        case '1':  // Tank player 1
        case '2':  // Tank player 2
        case ' ':  // Empty space
            return c;
        default:
            // Replace any other character (including '.') with space
            return ' ';
    }
}

// Enhanced grid normalization with character cleaning and flexible dimensions
std::vector<std::string> Simulator::cleanAndNormalizeGrid(const std::vector<std::string>& rawGrid, 
                                                          size_t targetRows, size_t targetCols, 
                                                          const std::string& path) const {
    std::vector<std::string> normalized;
    normalized.reserve(targetRows);
    std::set<char> invalidCharsFound;

    processGridRows(rawGrid, normalized, targetRows, targetCols, invalidCharsFound);
    logExtraRowsIgnored(rawGrid, targetRows);
    logInvalidCharacters(invalidCharsFound, path);
    
    return normalized;
}

void Simulator::processGridRows(const std::vector<std::string>& rawGrid, 
                               std::vector<std::string>& normalized,
                               size_t targetRows, size_t targetCols, 
                               std::set<char>& invalidCharsFound) const {
    for (size_t row = 0; row < targetRows; ++row) {
        std::string normalizedRow;
        normalizedRow.reserve(targetCols);
        
        if (row < rawGrid.size()) {
            processExistingRow(rawGrid[row], normalizedRow, targetCols, invalidCharsFound);
        } else {
            normalizedRow = std::string(targetCols, ' ');
        }
        normalized.push_back(std::move(normalizedRow));
    }
}

void Simulator::processExistingRow(const std::string& sourceRow, std::string& normalizedRow,
                                  size_t targetCols, std::set<char>& invalidCharsFound) const {
    for (size_t col = 0; col < targetCols; ++col) {
        if (col < sourceRow.size()) {
            char originalChar = sourceRow[col];
            char cleanedChar = cleanCharacter(originalChar);
            
            if (originalChar != cleanedChar) {
                invalidCharsFound.insert(originalChar);
            }
            normalizedRow += cleanedChar;
        } else {
            normalizedRow += ' ';
        }
    }
}

void Simulator::logExtraRowsIgnored(const std::vector<std::string>& rawGrid, size_t targetRows) const {
    if (rawGrid.size() > targetRows) {
        logDebug("MAPLOADER", "cleanAndNormalizeGrid", 
            "Ignored " + std::to_string(rawGrid.size() - targetRows) + " extra rows");
    }
}

void Simulator::logInvalidCharacters(const std::set<char>& invalidCharsFound, const std::string& path) const {
    if (!invalidCharsFound.empty()) {
        std::string invalidCharsStr = buildInvalidCharsString(invalidCharsFound);
        std::string errorMsg = "Invalid characters found in " + path + 
                              " (replaced with spaces): " + invalidCharsStr;
        logWarn("MAPLOADER", "cleanAndNormalizeGrid", errorMsg);
    }
}

std::string Simulator::buildInvalidCharsString(const std::set<char>& invalidChars) const {
    std::string result;
    for (char c : invalidChars) {
        if (!result.empty()) result += ", ";
        result += (c == ' ') ? "' '" : "'" + std::string(1, c) + "'";
    }
    return result;
}
// Fixed Map loading with enhanced error handling and flexibility
MapData Simulator::loadMapWithParams(const std::string& path) const {
    logDebug("MAPLOADER", "loadMapWithParams", "Loading map from: " + path);
    
    std::ifstream in = openMapFile(path);
    MapParameters params = parseMapParameters(in, path);
    validateMapParameters(params, path);
    
    std::vector<std::string> normalizedGrid = createNormalizedGrid(params);
    if (config_.debug) {
        logNormalizedGrid(normalizedGrid);
    }
    
    return buildMapData(params, std::move(normalizedGrid));
}

std::ifstream Simulator::openMapFile(const std::string& path) const {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::string errorMsg = "Failed to open map file: " + path;
        ErrorLogger::instance().log(errorMsg);
        throw std::runtime_error(errorMsg);
    }
    return in;
}

MapParameters Simulator::parseMapParameters(std::ifstream& in, const std::string& path) const {
    MapParameters params;
    params.path = path; 
    std::string line;
    
    int lineNumber = 0;
    
    while (std::getline(in, line)) {
        lineNumber++;
        cleanLine(line);
        
        if (lineNumber == 1) {
            logDebug("MAPLOADER", "loadMapWithParams", "Map name/description: " + line);
            continue;
        }
        
        if (!processMapLine(line, params, lineNumber, path)) {
            break; // Start processing grid data
        }
    }
    
    // Continue reading grid data
    while (std::getline(in, line)) {
        cleanLine(line);
        params.rawGridLines.push_back(line);
    }
    
    return params;
}

bool Simulator::processMapLine(const std::string& line, MapParameters& params, 
                              int lineNumber, const std::string& path) const {
    if (tryParseParameter(line, "Rows", params.rows, params.foundRows, path) ||
        tryParseParameter(line, "Cols", params.cols, params.foundCols, path) ||
        tryParseParameter(line, "MaxSteps", params.maxSteps, params.foundMaxSteps, path) ||
        tryParseParameter(line, "NumShells", params.numShells, params.foundNumShells, path)) {
        return true;
    }
    
    // Check if all headers found or past expected header lines
    bool headersParsed = params.foundRows && params.foundCols && 
                        params.foundMaxSteps && params.foundNumShells;
    
    if (headersParsed || lineNumber > 5 || !looksLikeParameter(line)) {
        params.rawGridLines.push_back(line);
        return false; // Start grid processing
    }
    
    if (!line.empty()) {
        logWarn("MAPLOADER", "loadMapWithParams", 
               "Ignoring extra metadata line in " + path + ": " + line);
    }
    return true;
}

bool Simulator::tryParseParameter(const std::string& line, const std::string& paramName,
                                 size_t& value, bool& found, const std::string& path) const {
    if (line.find(paramName) != std::string::npos && line.find("=") != std::string::npos) {
        if (parseParameter(line, paramName, value, path)) {
            found = true;
            logDebug("MAPLOADER", "loadMapWithParams", 
                    "Parsed " + paramName + " = " + std::to_string(value));
        }
        return true;
    }
    return false;
}

bool Simulator::looksLikeParameter(const std::string& line) const {
    return !line.empty() && line.find("=") != std::string::npos &&
           (line.find("Rows") != std::string::npos ||
            line.find("Cols") != std::string::npos ||
            line.find("MaxSteps") != std::string::npos ||
            line.find("NumShells") != std::string::npos);
}

void Simulator::validateMapParameters(const MapParameters& params, const std::string& path) const {
    checkRequiredHeaders(params, path);
    validateDimensions(params, path);
    checkDimensionMismatches(params, path);
}

void Simulator::checkRequiredHeaders(const MapParameters& params, const std::string& path) const {
    std::vector<std::string> missingHeaders;
    if (!params.foundRows) missingHeaders.push_back("Rows");
    if (!params.foundCols) missingHeaders.push_back("Cols");
    if (!params.foundMaxSteps) missingHeaders.push_back("MaxSteps");
    if (!params.foundNumShells) missingHeaders.push_back("NumShells");
    
    if (!missingHeaders.empty()) {
        std::string errorMsg = "Missing required headers in " + path + ": ";
        for (size_t i = 0; i < missingHeaders.size(); ++i) {
            errorMsg += missingHeaders[i];
            if (i + 1 < missingHeaders.size()) errorMsg += ", ";
        }
        ErrorLogger::instance().log(errorMsg);
        throw std::runtime_error(errorMsg);
    }
}

std::vector<std::string> Simulator::createNormalizedGrid(const MapParameters& params) const {
    logDebug("MAPLOADER", "loadMapWithParams", 
            "Parsed map parameters - rows=" + std::to_string(params.rows) + 
            ", cols=" + std::to_string(params.cols) + 
            ", maxSteps=" + std::to_string(params.maxSteps) + 
            ", numShells=" + std::to_string(params.numShells));

    return cleanAndNormalizeGrid(params.rawGridLines, params.rows, params.cols, params.path);

    // return cleanAndNormalizeGrid(params.rawGridLines, params.rows, params.cols, "");
}

MapData Simulator::buildMapData(const MapParameters& params, 
                               std::vector<std::string>&& normalizedGrid) const {
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
    md.rows = params.rows;
    md.cols = params.cols;
    md.maxSteps = params.maxSteps;
    md.numShells = params.numShells;
    md.view = std::make_unique<MapView>(std::move(normalizedGrid));
    
    logDebug("MAPLOADER", "loadMapWithParams", "Map loaded successfully");
    return md;
}

// Missing helper function implementations

void Simulator::cleanLine(std::string& line) const {
    // Remove carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

void Simulator::validateDimensions(const MapParameters& params, const std::string& path) const {
    if (params.rows == 0 || params.cols == 0) {
        std::string errorMsg = "Invalid dimensions in " + path + 
                              ": rows=" + std::to_string(params.rows) + 
                              ", cols=" + std::to_string(params.cols);
        ErrorLogger::instance().log(errorMsg);
        throw std::runtime_error(errorMsg);
    }
}

void Simulator::checkDimensionMismatches(const MapParameters& params, const std::string& path) const {
    // Check row dimension mismatches
    if (params.rawGridLines.size() != params.rows) {
        std::string errorMsg = "Map dimension mismatch in " + path + 
                              ": expected " + std::to_string(params.rows) + 
                              " rows, found " + std::to_string(params.rawGridLines.size()) + 
                              " (will be adjusted automatically)";
        logWarn("MAPLOADER", "loadMapWithParams", errorMsg);
    }
    
    // Check column dimension mismatches
    size_t maxColsFound = 0;
    for (const auto& line : params.rawGridLines) {
        maxColsFound = std::max(maxColsFound, line.size());
    }
    
    if (maxColsFound != params.cols) {
        std::string errorMsg = "Map dimension mismatch in " + path + 
                              ": expected " + std::to_string(params.cols) + 
                              " cols, found max " + std::to_string(maxColsFound) + 
                              " (will be adjusted automatically)";
        logWarn("MAPLOADER", "loadMapWithParams", errorMsg);
    }
}

void Simulator::logNormalizedGrid(const std::vector<std::string>& normalizedGrid) const {
    logDebug("MAPLOADER", "loadMapWithParams", "Normalized grid:");
    for (size_t r = 0; r < normalizedGrid.size(); ++r) {
        std::string debugStr = "Row " + std::to_string(r) + ": '";
        for (char c : normalizedGrid[r]) {
            debugStr += (c == ' ') ? ' ' : c;  // Show spaces as underscores for clarity
        }
        debugStr += "'";
        logDebug("MAPLOADER", "loadMapWithParams", debugStr);
    }
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
// Algorithm plugin loading - WITH TIMING PRESERVATION
bool Simulator::loadAlgorithmPlugins() {
    // Add small delay to preserve timing that prevents segfault
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    try {
        auto& algoReg = AlgorithmRegistrar::get();
        
        if (config_.modeComparative) {
            return loadComparativeAlgorithms(algoReg);
        } else {
            return loadCompetitionAlgorithms(algoReg);
        }
        
    } catch (const std::exception& e) {
        logError("PLUGINLOADER", "loadAlgorithmPlugins", "Exception: " + std::string(e.what()));
        ErrorLogger::instance().log("Exception: " + std::string(e.what()));
        return false;
    } catch (...) {
        logError("PLUGINLOADER", "loadAlgorithmPlugins", "Unknown exception");
        ErrorLogger::instance().log("Unknown exception");
        return false;
    }
}

bool Simulator::loadComparativeAlgorithms(AlgorithmRegistrar& algoReg) {
    logDebug("PLUGINLOADER", "loadAlgorithmPlugins", "Loading 2 algorithm plugins for comparative mode");
    
    std::vector<std::string> algPaths = {config_.algorithm1, config_.algorithm2};
    
    for (const auto& algPath : algPaths) {
        if (!loadSingleAlgorithm(algoReg, algPath, true)) {
            return false;
        }
    }
    
    return true;
}

bool Simulator::loadCompetitionAlgorithms(AlgorithmRegistrar& algoReg) {
    logDebug("PLUGINLOADER", "loadAlgorithmPlugins", 
            "Loading algorithm plugins from '" + config_.algorithms_folder + "'");
    
    for (auto& e : fs::directory_iterator(config_.algorithms_folder)) {
        if (e.path().extension() == ".so") {
            std::string path = e.path().string();
            loadSingleAlgorithm(algoReg, path, false); // Don't fail on individual errors in competition mode
        }
    }
    
    return loadedAlgorithms_ >= 2;
}

bool Simulator::loadSingleAlgorithm(AlgorithmRegistrar& algoReg, const std::string& algPath, bool failOnError) {
    if (failOnError && !validateAlgorithmFile(algPath)) {
        return false;
    }
    
    std::string name = stripSoExtension(algPath);
    logDebug("PLUGINLOADER", "loadAlgorithmPlugins", "Loading algorithm: " + algPath);
    
    if (!createAlgorithmEntry(algoReg, name, failOnError)) {
        return false;
    }
    
    void* handle = loadAlgorithmLibrary(algPath, name, algoReg, failOnError);
    if (!handle) {
        return false;
    }
    
    if (!validateAlgorithmRegistration(algoReg, name, handle, failOnError)) {
        return false;
    }
    
    finalizeAlgorithmLoad(handle, algPath, name);
    return true;
}

bool Simulator::validateAlgorithmFile(const std::string& algPath) {
    if (!std::filesystem::exists(algPath)) {
        std::string errorMsg = "Algorithm file does not exist: " + algPath;
        logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        ErrorLogger::instance().log(errorMsg);
        return false;
    }
    return true;
}

bool Simulator::createAlgorithmEntry(AlgorithmRegistrar& algoReg, const std::string& name, bool failOnError) {
    try {
        algoReg.createAlgorithmFactoryEntry(name);
        return true;
    } catch (const std::exception& e) {
        std::string errorMsg = "createAlgorithmFactoryEntry failed: " + std::string(e.what());
        logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        ErrorLogger::instance().log(errorMsg);
        return !failOnError;
    } catch (...) {
        std::string errorMsg = "createAlgorithmFactoryEntry failed with unknown exception";
        logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        ErrorLogger::instance().log(errorMsg);
        return !failOnError;
    }
}

void* Simulator::loadAlgorithmLibrary(const std::string& algPath, const std::string& name, 
                                     AlgorithmRegistrar& algoReg, bool failOnError) {
    void* handle = dlopen(algPath.c_str(), RTLD_NOW);
    
    if (!handle) {
        const char* dlerr = dlerror();
        std::string errorMsg = "dlopen failed for algorithm '" + name + "': " + std::string(dlerr ? dlerr : "unknown");
        
        if (failOnError) {
            logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        } else {
            logWarn("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
        }
        ErrorLogger::instance().log(errorMsg);
        
        try {
            algoReg.removeLast();
        } catch (...) {
            logWarn("PLUGINLOADER", "loadAlgorithmPlugins", "Failed to remove last registry entry");
        }
    }
    
    return handle;
}

bool Simulator::validateAlgorithmRegistration(AlgorithmRegistrar& algoReg, const std::string& name, 
                                             void* handle, bool failOnError) {
    try { 
        algoReg.validateLastRegistration();
        return true;
    } catch (const std::exception& e) {
        std::string errorMsg = "Registration validation failed for algorithm '" + name + "': " + e.what();
        handleValidationError(errorMsg, algoReg, handle, failOnError);
        return false;
    } catch (...) {
        std::string errorMsg = "Registration validation failed for algorithm '" + name + "'";
        handleValidationError(errorMsg, algoReg, handle, failOnError);
        return false;
    }
}

void Simulator::handleValidationError(const std::string& errorMsg, AlgorithmRegistrar& algoReg, 
                                     void* handle, bool failOnError) {
    if (failOnError) {
        logError("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
    } else {
        logWarn("PLUGINLOADER", "loadAlgorithmPlugins", errorMsg);
    }
    ErrorLogger::instance().log(errorMsg);
    algoReg.removeLast();
    dlclose(handle);
}

void Simulator::finalizeAlgorithmLoad(void* handle, const std::string& algPath, const std::string& name) {
    logDebug("PLUGINLOADER", "loadAlgorithmPlugins", 
            "Algorithm '" + name + "' loaded and validated successfully");
    algorithmHandles_.push_back(handle);
    validAlgorithmPaths_.push_back(algPath);
    loadedAlgorithms_++;
}
// GameManager plugin loading
bool Simulator::loadGameManagerPlugins() {
    auto& gmReg = GameManagerRegistrar::get();
    
    logDebug("PLUGINLOADER", "loadGameManagerPlugins", 
        "Loading GameManager plugins from '" + config_.game_managers_folder + "'");
    
    loadGameManagersFromDirectory(gmReg);
    return loadedGameManagers_ > 0;
}

void Simulator::loadGameManagersFromDirectory(GameManagerRegistrar& gmReg) {
    for (auto& e : fs::directory_iterator(config_.game_managers_folder)) {
        if (e.path().extension() == ".so") {
            std::string path = e.path().string();
            loadSingleGameManager(gmReg, path);
        }
    }
}

void Simulator::loadSingleGameManager(GameManagerRegistrar& gmReg, const std::string& path) {
    std::string name = stripSoExtension(path);
    
    logDebug("PLUGINLOADER", "loadGameManagerPlugins", "Loading GameManager: " + path);
    
    if (!createGameManagerEntry(gmReg, name)) {
        return;
    }
    
    void* handle = loadGameManagerLibrary(gmReg, path, name);
    if (!handle) {
        return;
    }
    
    if (!validateGameManagerRegistration(gmReg, handle, name)) {
        return;
    }
    
    finalizeGameManagerLoad(handle, path, name);
}

bool Simulator::createGameManagerEntry(GameManagerRegistrar& gmReg, const std::string& name) {
    try {
        gmReg.createGameManagerEntry(name);
        return true;
    } catch (...) {
        logWarn("PLUGINLOADER", "loadGameManagerPlugins", 
               "Failed to create GameManager entry for: " + name);
        return false;
    }
}

void* Simulator::loadGameManagerLibrary(GameManagerRegistrar& gmReg, const std::string& path, 
                                       const std::string& name) {
    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        std::string warnMsg = "dlopen failed for GameManager '" + name + "': " + std::string(dlerror());
        logWarn("PLUGINLOADER", "loadGameManagerPlugins", warnMsg);
        gmReg.removeLast();
    }
    return handle;
}

bool Simulator::validateGameManagerRegistration(GameManagerRegistrar& gmReg, void* handle, 
                                               const std::string& name) {
    try { 
        gmReg.validateLastRegistration(); 
        return true;
    } catch (...) {
        std::string warnMsg = "Registration validation failed for GameManager '" + name + "'";
        logWarn("PLUGINLOADER", "loadGameManagerPlugins", warnMsg);
        gmReg.removeLast();
        dlclose(handle);
        return false;
    }
}

void Simulator::finalizeGameManagerLoad(void* handle, const std::string& path, const std::string& name) {
    logDebug("PLUGINLOADER", "loadGameManagerPlugins", 
            "GameManager '" + name + "' loaded and validated successfully");
    gameManagerHandles_.push_back(handle);
    validGameManagerPaths_.push_back(path);
    loadedGameManagers_++;
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
        logError("PLUGINLOADER", "loadSingleGameManager", errorMsg);
        gmReg.removeLast();
        return false;
    }
    try { 
        gmReg.validateLastRegistration(); 
    } catch (...) {
        std::string errorMsg = "GameManager registration validation failed for '" + gmName + "'";
        logError("PLUGINLOADER", "loadSingleGameManager", errorMsg);
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
void Simulator::dispatchComparativeTasks() {
    logInfo("THREADPOOL", "dispatchComparativeTasks", 
        "Starting game execution with " + std::to_string(config_.numThreads) + " threads");

    MapData md = loadComparativeMap();
    if (md.view == nullptr) return;

    enqueueComparativeTasks(md);
    finalizeTaskExecution();
}

MapData Simulator::loadComparativeMap() {
    try {
        return loadMapWithParams(config_.game_map);
    } catch (const std::exception& ex) {
        std::string errorMsg = "Error loading map: " + std::string(ex.what());
        logError("SIMULATOR", "dispatchComparativeTasks", errorMsg);
        ErrorLogger::instance().log(errorMsg);
        return MapData{}; // Return empty MapData
    }
}

void Simulator::enqueueComparativeTasks(const MapData& md) {
    auto& gmReg = GameManagerRegistrar::get();
    auto& algoReg = AlgorithmRegistrar::get();
    
    SatelliteView& realMap = *md.view;
    const std::string mapFile = config_.game_map;
    const std::string algo1Name = stripSoExtension(config_.algorithm1);
    const std::string algo2Name = stripSoExtension(config_.algorithm2);

    for (size_t gi = 0; gi < loadedGameManagers_; ++gi) {
        auto& gmEntry = *(gmReg.begin() + gi);
        auto& A = *(algoReg.begin() + 0);
        auto& B = *(algoReg.begin() + 1);

        threadPool_->enqueue([this, &gmEntry, &A, &B, &md, &realMap, mapFile, algo1Name, algo2Name, gi] {
            executeComparativeGame(gmEntry, A, B, md, realMap, mapFile, algo1Name, algo2Name, gi);
        });
    }
}

void Simulator::executeComparativeGame(const auto& gmEntry, const auto& A, const auto& B,
                                      const MapData& md, SatelliteView& realMap,
                                      const std::string& mapFile, const std::string& algo1Name,
                                      const std::string& algo2Name, size_t gi) {
    const std::string gmName = stripSoExtension(validGameManagerPaths_[gi]);
    try {
        GameResult gr = runComparativeGame(gmEntry, A, B, md, realMap, mapFile, algo1Name, algo2Name);
        std::string finalMap = buildFinalMapString(gr, md);
        
        std::lock_guard<std::mutex> lock(resultsMutex_);
        comparativeResults_.emplace_back(
            stripSoExtension(validGameManagerPaths_[gi]),
            std::move(gr),
            std::move(finalMap)
        );
        totalGamesPlayed_++;
    } catch (const std::exception& ex) {
        ErrorLogger::instance().logGameManagerError(mapFile, algo1Name, algo2Name,"GM='" + gmName + "': " + std::string(ex.what()));
    } catch (...) {
        ErrorLogger::instance().logGameManagerError(mapFile, algo1Name, algo2Name, "GM='" + gmName + "': Unknown error occurred.");
     }
    }


GameResult Simulator::runComparativeGame(const auto& gmEntry, const auto& A, const auto& B,
                                        const MapData& md, SatelliteView& realMap,
                                        const std::string& mapFile, const std::string& algo1Name,
                                        const std::string& algo2Name) {
    auto gm = gmEntry.factory(config_.verbose);
    auto p1 = A.createPlayer(1, md.cols, md.rows, md.maxSteps, md.numShells);
    auto p2 = B.createPlayer(2, md.cols, md.rows, md.maxSteps, md.numShells);

    return gm->run(
        md.cols, md.rows,
        realMap,
        baseName(mapFile),
        md.maxSteps, md.numShells,
        *p1, algo1Name,
        *p2, algo2Name,
        [&](int pi, int ti) { return A.createTankAlgorithm(pi, ti); },
        [&](int pi, int ti) { return B.createTankAlgorithm(pi, ti); }
    );
}

std::string Simulator::buildFinalMapString(const GameResult& gr, const MapData& md) {
    std::ostringstream ss;
    auto* state = gr.gameState.get();
    for (size_t y = 0; y < md.rows; ++y) {
        for (size_t x = 0; x < md.cols; ++x) {
            ss << state->getObjectAt(x, y);
        }
        ss << '\n';
    }
    return ss.str();
}

void Simulator::dispatchCompetitionTasks() {
    CompetitionSetup setup = prepareCompetitionData();
    if (setup.mapViews.empty()) return;

    enqueueCompetitionTasks(setup);
    finalizeTaskExecution();
}

CompetitionSetup Simulator::prepareCompetitionData() {
    CompetitionSetup setup;
    
    // Collect all map files
    for (auto& e : fs::directory_iterator(config_.game_maps_folder)) {
        if (e.is_regular_file()) {
            setup.allMapFiles.push_back(e.path().string());
        }
    }

    // Preload valid maps
    setup.mapViews = preloadMapsAndTrackValid(setup.allMapFiles, setup.validMapFiles, 
                                             setup.mapRows, setup.mapCols, 
                                             setup.mapMaxSteps, setup.mapNumShells);

    if (setup.mapViews.empty()) {
        logError("SIMULATOR", "dispatchCompetitionTasks", "No valid maps to run");
        ErrorLogger::instance().log("No valid maps to run");
    }

    return setup;
}

void Simulator::enqueueCompetitionTasks(const CompetitionSetup& setup) {
    size_t totalGames = calculateTotalGames(setup.mapViews.size());
    logInfo("THREADPOOL", "dispatchCompetitionTasks", 
           "Starting execution of " + std::to_string(totalGames) + " total games with " + 
           std::to_string(config_.numThreads) + " threads");

    auto& gmReg = GameManagerRegistrar::get();
    auto& algoReg = AlgorithmRegistrar::get();
    auto& gmEntry = *gmReg.begin();

    for (size_t mi = 0; mi < setup.mapViews.size(); ++mi) {
        enqueueMapTasks(setup, mi, gmEntry, algoReg);
    }
}

void Simulator::enqueueMapTasks(const CompetitionSetup& setup, size_t mi, 
                               const auto& gmEntry, AlgorithmRegistrar& algoReg) {
    SatelliteView& realMap = *setup.mapViews[mi];
    size_t cols = setup.mapCols[mi], rows = setup.mapRows[mi];
    size_t mSteps = setup.mapMaxSteps[mi], nShells = setup.mapNumShells[mi];
    const std::string mapFile = setup.validMapFiles[mi];

    for (size_t i = 0; i < loadedAlgorithms_; ++i) {
        size_t j = (i+1 + (mi % (loadedAlgorithms_ - 1 ))) % loadedAlgorithms_;
        if ( loadedAlgorithms_ % 2 == 0 && mi == loadedAlgorithms_/2 - 1 && i >= loadedAlgorithms_/2) {
            continue;
        }
         threadPool_->enqueue([this, &algoReg, &gmEntry, &realMap, cols, rows, mSteps, nShells, mapFile, i, j] {
                executeCompetitionGame(algoReg, gmEntry, realMap, cols, rows, mSteps, nShells, mapFile, i, j);
            });
        }

        // for (size_t j = i + 1; j < loadedAlgorithms_; ++j) {
        //     threadPool_->enqueue([this, &algoReg, &gmEntry, &realMap, cols, rows, mSteps, nShells, mapFile, i, j] {
        //         executeCompetitionGame(algoReg, gmEntry, realMap, cols, rows, mSteps, nShells, mapFile, i, j);
        //     });
        // }
    }

void Simulator::executeCompetitionGame(AlgorithmRegistrar& algoReg, const auto& gmEntry,
                                      SatelliteView& realMap, size_t cols, size_t rows,
                                      size_t mSteps, size_t nShells, const std::string& mapFile,
                                      size_t i, size_t j) {
    const std::string algo1Name = stripSoExtension(validAlgorithmPaths_[i]);
    const std::string algo2Name = stripSoExtension(validAlgorithmPaths_[j]);
    const std::string gmName = stripSoExtension(config_.game_manager);


    try {
        GameResult gr = runCompetitionGame(algoReg, gmEntry, realMap, cols, rows, 
                                          mSteps, nShells, mapFile, algo1Name, algo2Name, i, j);
        
        std::lock_guard<std::mutex> lock(resultsMutex_);
        competitionResults_.emplace_back(mapFile, algo1Name, algo2Name, std::move(gr));
        totalGamesPlayed_++;
    } catch (const std::exception& ex) {
        ErrorLogger::instance().logGameManagerError(mapFile, algo1Name, algo2Name, gmName + "': " + std::string(ex.what()));
    } catch (...) {
        ErrorLogger::instance().logGameManagerError(mapFile, algo1Name, algo2Name, gmName + "': Unknown error occurred.");
    }
}

GameResult Simulator::runCompetitionGame(AlgorithmRegistrar& algoReg, const auto& gmEntry,
                                        SatelliteView& realMap, size_t cols, size_t rows,
                                        size_t mSteps, size_t nShells, const std::string& mapFile,
                                        const std::string& algo1Name, const std::string& algo2Name,
                                        size_t i, size_t j) {
    auto gm = gmEntry.factory(config_.verbose);
    auto& A = *(algoReg.begin() + i);
    auto& B = *(algoReg.begin() + j);
    auto p1 = A.createPlayer(1, cols, rows, mSteps, nShells);
    auto p2 = B.createPlayer(2, cols, rows, mSteps, nShells);

    return gm->run(
        cols, rows,
        realMap,
        baseName(mapFile),
        mSteps, nShells,
        *p1, algo1Name,
        *p2, algo2Name,
        [&](int pi, int ti) { return A.createTankAlgorithm(pi, ti); },
        [&](int pi, int ti) { return B.createTankAlgorithm(pi, ti); }
    );
}

size_t Simulator::calculateTotalGames(size_t numMaps) {
    size_t totalGames = numMaps * loadedAlgorithms_;
    return totalGames;
}

void Simulator::finalizeTaskExecution() {
    logInfo("THREADPOOL", "dispatchComparativeTasks", "All tasks enqueued, waiting for completion");
    threadPool_->shutdown();
    threadPool_ = std::make_unique<ThreadPool>(config_.numThreads);
}

// // Comparative results file writing
// bool Simulator::writeComparativeFile(const std::vector<ComparativeEntry>& entries) const {
//     logInfo("FILEWRITER", "writeComparativeFile", "Writing comparative results file");

//     // Group GMs by identical outcome
//     struct Outcome {
//         int winner;
//         GameResult::Reason reason;
//         size_t rounds;
//         std::string finalState;
//         bool operator<(const Outcome& o) const {
//             if (winner != o.winner) return winner < o.winner;
//             if (reason != o.reason) return reason < o.reason;
//             if (rounds != o.rounds) return rounds < o.rounds;
//             return finalState < o.finalState;
//         }
//     };

//     std::map<Outcome, std::vector<std::string>> groups;
//     for (const auto& e : entries) {
//         Outcome key{
//             e.res.winner,
//             e.res.reason,
//             e.res.rounds,
//             e.finalState
//         };
//         groups[key].push_back(e.gmName);
//     }

//     logDebug("FILEWRITER", "writeComparativeFile", 
//         "Grouped results into " + std::to_string(groups.size()) + " outcome categories");

//     // Build output path
//     auto ts = currentTimestamp();
//     fs::path outPath = fs::path(config_.game_managers_folder)
//                      / ("comparative_results_" + ts + ".txt");

//     // Open file (or fallback)
//     std::ofstream ofs(outPath);
//     if (!ofs.is_open()) {
//         std::string warnMsg = "Cannot create file " + outPath.string() + ", falling back to stdout";
//         logWarn("FILEWRITER", "writeComparativeFile", warnMsg);
//         ErrorLogger::instance().log(warnMsg);
        
//         // Fallback to stdout
//         std::cout << "game_map="   << config_.game_map   << "\n";
//         std::cout << "algorithm1=" << stripSoExtension(config_.algorithm1) << "\n";
//         std::cout << "algorithm2=" << stripSoExtension(config_.algorithm2) << "\n\n";
//         for (const auto& [outcome, gms] : groups) {
//             for (size_t i = 0; i < gms.size(); ++i) {
//                 std::cout << gms[i] << (i + 1 < gms.size() ? "," : "");
//             }
//             std::cout << "\n";
//             std::cout << outcomeMessage(outcome.winner, outcome.reason) << "\n";
//             std::cout << outcome.rounds << "\n";
//             std::cout << outcome.finalState << "\n\n";
//         }
//         return false;
//     }

//     logInfo("FILEWRITER", "writeComparativeFile", "Writing to file: " + outPath.string());

//     // Write header
//     ofs << "game_map="   << config_.game_map   << "\n";
//     ofs << "algorithm1=" << stripSoExtension(config_.algorithm1) << "\n";
//     ofs << "algorithm2=" << stripSoExtension(config_.algorithm2) << "\n\n";

//     // Write grouped results
//     for (const auto& [outcome, gms] : groups) {
//         for (size_t i = 0; i < gms.size(); ++i) {
//             ofs << gms[i] << (i + 1 < gms.size() ? "," : "");
//         }
//         ofs << "\n";
//         ofs << outcomeMessage(outcome.winner, outcome.reason) << "\n";
//         ofs << outcome.rounds << "\n";
//         ofs << outcome.finalState << "\n\n";
//     }
    
//     logInfo("FILEWRITER", "writeComparativeFile", "Comparative results file written successfully");
//     return true;
// }

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
        Outcome key{ e.res.winner, e.res.reason, e.res.rounds, e.finalState };
        groups[key].push_back(e.gmName);
    }

    logDebug("FILEWRITER", "writeComparativeFile",
             "Grouped results into " + std::to_string(groups.size()) + " outcome categories");

    // Flatten and sort: DESC by group size; tie-break deterministically by Outcome fields
    std::vector<std::pair<Outcome, std::vector<std::string>>> items;
    items.reserve(groups.size());
    for (auto& kv : groups) {
        auto& gms = kv.second;
        std::sort(gms.begin(), gms.end()); // stable order inside group
        items.emplace_back(kv.first, gms);
    }
    std::sort(items.begin(), items.end(),
        [](const auto& L, const auto& R) {
            if (L.second.size() != R.second.size())
                return L.second.size() > R.second.size(); // DESC by group size
            if (L.first.winner != R.first.winner) return L.first.winner < R.first.winner;
            if (L.first.reason != R.first.reason) return L.first.reason < R.first.reason;
            if (L.first.rounds != R.first.rounds) return L.first.rounds < R.first.rounds;
            return L.first.finalState < R.first.finalState;
        });

    // Build output path
    auto ts = currentTimestamp();
    fs::path outPath = fs::path(config_.game_managers_folder)
                     / ("comparative_results_" + ts + ".txt");

    // Open file (or fallback)
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        std::string warnMsg = "Cannot create file " + outPath.string() + ", falling back to stdout";
        logWarn("FILEWRITER", "writeComparativeFile", warnMsg);
        ErrorLogger::instance().log(warnMsg);

        // Fallback to stdout
        std::cout << "game_map="   << config_.game_map   << "\n";
        std::cout << "algorithm1=" << stripSoExtension(config_.algorithm1) << "\n";
        std::cout << "algorithm2=" << stripSoExtension(config_.algorithm2) << "\n\n";

        for (size_t idx = 0; idx < items.size(); ++idx) {
            const auto& outcome = items[idx].first;
            const auto& gms     = items[idx].second;

            // GM list (comma-separated, no extra spaces to match existing style)
            for (size_t i = 0; i < gms.size(); ++i) {
                std::cout << gms[i] << (i + 1 < gms.size() ? "," : "");
            }
            std::cout << "\n";
            std::cout << outcomeMessage(outcome.winner, outcome.reason) << "\n";
            std::cout << outcome.rounds << "\n";
            std::cout << outcome.finalState; // finalState already contains newlines
            std::cout << "\n";               // exactly one blank line between groups
        }
        return false;
    }

    logInfo("FILEWRITER", "writeComparativeFile", "Writing to file: " + outPath.string());

    // Write header
    ofs << "game_map="   << config_.game_map   << "\n";
    ofs << "algorithm1=" << stripSoExtension(config_.algorithm1) << "\n";
    ofs << "algorithm2=" << stripSoExtension(config_.algorithm2) << "\n\n";

    // Write grouped results (sorted)
    for (size_t idx = 0; idx < items.size(); ++idx) {
        const auto& outcome = items[idx].first;
        const auto& gms     = items[idx].second;

        // GM list (comma-separated, no extra spaces to match existing style)
        for (size_t i = 0; i < gms.size(); ++i) {
            ofs << gms[i] << (i + 1 < gms.size() ? "," : "");
        }
        ofs << "\n";
        ofs << outcomeMessage(outcome.winner, outcome.reason) << "\n";
        ofs << outcome.rounds << "\n";
        ofs << outcome.finalState; // finalState already contains newlines
        ofs << "\n";               // exactly one blank line between groups
    }

    logInfo("FILEWRITER", "writeComparativeFile", "Comparative results file written successfully");
    return true;
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
        }
    }
    logInfo("SIMULATOR", "preloadMapsAndTrackValid", "Successfully preloaded " + std::to_string(mapViews.size()) + " valid map(s)");
    
    return mapViews;
}

// Competition results file writing
bool Simulator::writeCompetitionFile(const std::vector<CompetitionEntry>& results) const {
    logInfo("FILEWRITER", "writeCompetitionFile", "Writing competition results file");

    auto scores = calculateScores(results);
    auto sorted = sortScoresByDescending(scores);
    auto outPath = buildOutputPath();

    return writeToFile(outPath, sorted) || writeToStdout(sorted);
}

std::map<std::string, int> Simulator::calculateScores(const std::vector<CompetitionEntry>& results) const {
    std::map<std::string, int> scores;
    for (const auto& entry : results) {
        // Only initialize if not already in map
        if (scores.find(entry.a1) == scores.end()) {
            scores[entry.a1] = 0;
        }
        if (scores.find(entry.a2) == scores.end()) {
            scores[entry.a2] = 0;
        }
        
        int w = entry.res.winner;
        if (w == 1) {
            scores[entry.a1] += 3;
        } else if (w == 2) {
            scores[entry.a2] += 3;
        } else {
            scores[entry.a1] += 1;
            scores[entry.a2] += 1;
        }
    }
    
    logDebug("FILEWRITER", "writeCompetitionFile", 
        "Calculated scores for " + std::to_string(scores.size()) + " algorithms");
    return scores;
}

std::vector<std::pair<std::string, int>> Simulator::sortScoresByDescending(
    const std::map<std::string, int>& scores) const {
    std::vector<std::pair<std::string, int>> sorted(scores.begin(), scores.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& L, const auto& R) { return L.second > R.second; });
    return sorted;
}

fs::path Simulator::buildOutputPath() const {
    auto ts = currentTimestamp();
    return fs::path(config_.algorithms_folder) / ("competition_" + ts + ".txt");
}

bool Simulator::writeToFile(const fs::path& outPath, 
    const std::vector<std::pair<std::string, int>>& sorted) const {
    std::ofstream ofs(outPath);
    if (!ofs.is_open()) {
        std::string warnMsg = "Cannot create file " + outPath.string() + ", falling back to stdout";
        logWarn("FILEWRITER", "writeCompetitionFile", warnMsg);
        ErrorLogger::instance().log(warnMsg);
        return false;
    }

    logInfo("FILEWRITER", "writeCompetitionFile", "Writing to file: " + outPath.string());
    writeContent(ofs, sorted);
    logInfo("FILEWRITER", "writeCompetitionFile", "Competition results file written successfully");
    return true;
}

bool Simulator::writeToStdout(const std::vector<std::pair<std::string, int>>& sorted) const {
    writeContent(std::cout, sorted);
    return false;
}

void Simulator::writeContent(std::ostream& os, 
    const std::vector<std::pair<std::string, int>>& sorted) const {
    os << "game_maps_folder=" << config_.game_maps_folder << "\n";
    os << "game_manager=" << stripSoExtension(config_.game_manager) << "\n\n";
    for (const auto& p : sorted) {
        os << p.first << " " << p.second << "\n";
    }
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

    // std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" << component << "] [" << function << "] " << message << std::endl;

    // Also log warnings to ErrorLogger
    // LOG_ERROR("WARN: [" + component + "] [" + function + "] " + message);
    ErrorLogger::instance().log("WARN: [" + component + "] [" + function + "] " + message);

}

void Simulator::logError(const std::string& component, const std::string& function, const std::string& message) const {
    std::lock_guard<std::mutex> lock(debugMutex_);

    // std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" << component << "] [" << function << "] " << message << std::endl;

    // Also log errors to ErrorLogger
    // LOG_ERROR("ERROR: [" + component + "] [" + function + "] " + message);
    ErrorLogger::instance().log("ERROR: [" + component + "] [" + function + "] " + message);
}
