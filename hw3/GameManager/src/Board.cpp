// src/Board.cpp
#include "Board.h"
#include <iostream>
using namespace GameManager_315634022;
Board::Board(std::size_t rows, std::size_t cols)
  : rows_(rows), cols_(cols),
    grid_(rows, std::vector<Cell>(cols))
{}

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
// Board.cpp

void Board::loadFromSatelliteView(const SatelliteView& sv) {
    // Use our own rows_ and cols_, not legacy width_/height_!
    
    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            char c = sv.getObjectAt(x, y);
            switch (c) {
                case '#':  setCell(x, y, CellContent::WALL);  break;
                case '@':  setCell(x, y, CellContent::MINE);  break;
                case '1':  setCell(x, y, CellContent::TANK1); break;
                case '2':  setCell(x, y, CellContent::TANK2); break;
                default:   setCell(x, y, CellContent::EMPTY); break;
            }
        }
    }

    // DEBUG: dump entire board
    // if (verbose_) {

    std::cout << "[DEBUG] Board after loadFromSatelliteView (" 
              << rows_ << "×" << cols_ << "):\n";
    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            const auto& cell = getCell(x, y);
            char d = '_';
            if (cell.content == CellContent::WALL)   d = '#';
            else if (cell.content == CellContent::MINE)   d = '@';
            else if (cell.content == CellContent::TANK1)  d = '1';
            else if (cell.content == CellContent::TANK2)  d = '2';
            std::cout << d;
        }
        std::cout << "\n";
    }
    // }
}

