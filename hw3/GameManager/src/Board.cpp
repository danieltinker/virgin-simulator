// src/Board.cpp
#include "Board.h"
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>

using namespace GameManager_315634022;

// ——————————————————————————————————————————————————————
// Professional Debug Logging System
// ——————————————————————————————————————————————————————

static std::mutex g_debug_mutex;

// Thread-safe debug print with component identification
#define DEBUG_PRINT(component, function, message, debug_flag) \
    do { \
        if (debug_flag) { \
            std::lock_guard<std::mutex> lock(g_debug_mutex); \
            std::cout << "[T" << std::this_thread::get_id() << "] [" << component << "] [" << function << "] " << message << std::endl; \
        } \
    } while(0)

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

Board::Board(std::size_t rows, std::size_t cols)
  : rows_(rows), cols_(cols),
    grid_(rows, std::vector<Cell>(cols))
{
    DEBUG_PRINT("BOARD", "constructor", 
        "Board created with dimensions: " + std::to_string(cols) + "x" + std::to_string(rows), true);
}

void Board::setCell(int x, int y, CellContent c) {
    Cell& cell = grid_[y][x];
    cell.content         = c;
    cell.wallHits        = (c == CellContent::WALL ? 0 : 0);
    cell.hasShellOverlay = false;
}

void Board::wrapCoords(int& x, int& y) const {
    int w = int(cols_), h = int(rows_);
    x = (x % w + w) % w;
    y = (y % h + h) % h;
}

void Board::clearShellMarks() {
    for (auto& row : grid_)
        for (auto& cell : row)
            cell.hasShellOverlay = false;
}

void Board::loadFromSatelliteView(const SatelliteView& sv) {
    INFO_PRINT("BOARD", "loadFromSatelliteView", 
        "Loading board from SatelliteView - dimensions: " + std::to_string(cols_) + "x" + std::to_string(rows_));
    
    // Cell type counters for statistics
    size_t walls = 0, mines = 0, tank1s = 0, tank2s = 0, empty = 0;
    
    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            char c = sv.getObjectAt(x, y);
            switch (c) {
                case '#':  
                    setCell(x, y, CellContent::WALL);  
                    walls++;
                    break;
                case '@':  
                    setCell(x, y, CellContent::MINE);  
                    mines++;
                    break;
                case '1':  
                    setCell(x, y, CellContent::TANK1); 
                    tank1s++;
                    DEBUG_PRINT("BOARD", "loadFromSatelliteView", 
                        "Player 1 tank found at (" + std::to_string(x) + "," + std::to_string(y) + ")", true);
                    break;
                case '2':  
                    setCell(x, y, CellContent::TANK2); 
                    tank2s++;
                    DEBUG_PRINT("BOARD", "loadFromSatelliteView", 
                        "Player 2 tank found at (" + std::to_string(x) + "," + std::to_string(y) + ")", true);
                    break;
                default:   
                    setCell(x, y, CellContent::EMPTY); 
                    empty++;
                    break;
            }
        }
    }

    // Log board statistics
    DEBUG_PRINT("BOARD", "loadFromSatelliteView", 
        "Board statistics - Walls: " + std::to_string(walls) + 
        ", Mines: " + std::to_string(mines) + 
        ", Player1 tanks: " + std::to_string(tank1s) + 
        ", Player2 tanks: " + std::to_string(tank2s) + 
        ", Empty cells: " + std::to_string(empty), true);

    // Generate and log board visualization
    DEBUG_PRINT("BOARD", "loadFromSatelliteView", "Board layout loaded successfully:", true);
    
    std::ostringstream boardVis;
    boardVis << "Board after loadFromSatelliteView (" << rows_ << "×" << cols_ << "):\n";
    
    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            const auto& cell = getCell(x, y);
            char d = '_';
            if (cell.content == CellContent::WALL)   d = '#';
            else if (cell.content == CellContent::MINE)   d = '@';
            else if (cell.content == CellContent::TANK1)  d = '1';
            else if (cell.content == CellContent::TANK2)  d = '2';
            boardVis << d;
        }
        if (y + 1 < rows_) boardVis << "\n";
    }
    
    // Log the complete board visualization
    DEBUG_PRINT("BOARD", "loadFromSatelliteView", boardVis.str(), true);
    
    INFO_PRINT("BOARD", "loadFromSatelliteView", "Board loading completed successfully");
}