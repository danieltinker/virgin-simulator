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

#define DEBUG_PRINT(component, function, message, debug_flag) \
    do { if (debug_flag) { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cout << "[T" << std::this_thread::get_id() << "] [DEBUG] [" << component \
              << "] [" << function << "] " << message << std::endl; }} while(0)

#define INFO_PRINT(component, function, message) \
    do { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cout << "[T" << std::this_thread::get_id() << "] [INFO] [" << component \
              << "] [" << function << "] " << message << std::endl; } while(0)

#define WARN_PRINT(component, function, message) \
    do { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" << component \
              << "] [" << function << "] " << message << std::endl; } while(0)

#define ERROR_PRINT(component, function, message) \
    do { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" << component \
              << "] [" << function << "] " << message << std::endl; } while(0)

namespace {
    struct BoardStats { size_t walls=0, mines=0, t1=0, t2=0, empty=0; };

    inline void tallyCell(BoardStats& s, GameManager_315634022::CellContent c) {
        switch (c) {
            case GameManager_315634022::CellContent::WALL:   ++s.walls; break;
            case GameManager_315634022::CellContent::MINE:   ++s.mines; break;
            case GameManager_315634022::CellContent::TANK1:  ++s.t1;    break;
            case GameManager_315634022::CellContent::TANK2:  ++s.t2;    break;
            case GameManager_315634022::CellContent::EMPTY:  ++s.empty; break;
        }
    }

    // FIX: Cell is in the namespace, not nested in Board
    inline char cellChar(const GameManager_315634022::Cell& cell) {
        using CC = GameManager_315634022::CellContent;
        if (cell.content == CC::WALL)   return '#';
        if (cell.content == CC::MINE)   return '@';
        if (cell.content == CC::TANK1)  return '1';
        if (cell.content == CC::TANK2)  return '2';
        return ' ';
    }

    std::string visualizeBoard(const GameManager_315634022::Board& B) {
        std::ostringstream oss;
        oss << "Board after loadFromSatelliteView (" << B.getRows() << "×" << B.getCols() << "):\n";
        for (std::size_t y = 0; y < B.getRows(); ++y) {
            for (std::size_t x = 0; x < B.getCols(); ++x) {
                oss << cellChar(B.getCell(int(x), int(y)));
            }
            if (y + 1 < B.getRows()) oss << "\n";
        }
        return oss.str();
    }

    void logBoardStats(const BoardStats& s) {
        DEBUG_PRINT("BOARD", "loadFromSatelliteView", 
            "Board statistics - Walls: " + std::to_string(s.walls) + 
            ", Mines: " + std::to_string(s.mines) + 
            ", Player1 tanks: " + std::to_string(s.t1) + 
            ", Player2 tanks: " + std::to_string(s.t2) + 
            ", Empty cells: " + std::to_string(s.empty), true);
    }
} // anonymous namespace

Board::Board(std::size_t rows, std::size_t cols)
  : rows_(rows), cols_(cols),
    grid_(rows, std::vector<Cell>(cols))
{
    DEBUG_PRINT("BOARD", "constructor", 
        "Board created with dimensions: " + std::to_string(rows) + "x" + std::to_string(cols), true);
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
        "Loading board from SatelliteView - dimensions: " + std::to_string(rows_) + "x" + std::to_string(cols_));

    BoardStats stats;
    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            char c = sv.getObjectAt(x, y);
            switch (c) {
                case '#':  setCell(x, y, CellContent::WALL);  tallyCell(stats, CellContent::WALL);  break;
                case '@':  setCell(x, y, CellContent::MINE);  tallyCell(stats, CellContent::MINE);  break;
                case '1':  setCell(x, y, CellContent::TANK1); tallyCell(stats, CellContent::TANK1);
                           DEBUG_PRINT("BOARD", "loadFromSatelliteView", 
                               "Player 1 tank found at (" + std::to_string(x) + "," + std::to_string(y) + ")", true);
                           break;
                case '2':  setCell(x, y, CellContent::TANK2); tallyCell(stats, CellContent::TANK2);
                           DEBUG_PRINT("BOARD", "loadFromSatelliteView", 
                               "Player 2 tank found at (" + std::to_string(x) + "," + std::to_string(y) + ")", true);
                           break;
                default:   setCell(x, y, CellContent::EMPTY); tallyCell(stats, CellContent::EMPTY); break;
            }
        }
    }

    logBoardStats(stats);
    DEBUG_PRINT("BOARD", "loadFromSatelliteView", "Board layout loaded successfully:", true);
    DEBUG_PRINT("BOARD", "loadFromSatelliteView", visualizeBoard(*this), true);
    INFO_PRINT("BOARD", "loadFromSatelliteView", "Board loading completed successfully");
}
