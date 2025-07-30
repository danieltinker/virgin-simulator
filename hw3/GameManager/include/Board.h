// include/Board.h
#pragma once

#include <vector>
#include <cstddef>
#include <SatelliteView.h>

namespace GameManager_315634022{


/// Contents of a single board cell.
enum class CellContent {
    EMPTY,
    WALL,
    MINE,
    TANK1,
    TANK2
};

/// A cell tracks its content, wall-hit count, and any shell overlay.
struct Cell {
    CellContent content = CellContent::EMPTY;
    int         wallHits = 0;
    bool        hasShellOverlay = false;
};

/// A toroidal grid of Cells supporting walls, mines, and tanks.
class Board {
public:
    Board() = default;
    Board(std::size_t rows, std::size_t cols);

    std::size_t getRows()   const { return rows_; }
    std::size_t getCols()   const { return cols_; }
    int         getWidth()  const { return int(cols_); }
    int         getHeight() const { return int(rows_); }

    const std::vector<std::vector<Cell>>& getGrid() const { return grid_; }

    Cell&       getCell(int x, int y)       { return grid_[y][x]; }
    const Cell& getCell(int x, int y) const { return grid_[y][x]; }

    /// Sets content at (x,y), resetting wallHits if it becomes a wall.
    void setCell(int x, int y, CellContent c);

    /// Wraps x,y into valid range [0..width) × [0..height).
    void wrapCoords(int& x, int& y) const;

    /// Clears all shell overlays.
    void clearShellMarks();

    /// No-op: tanks are tracked in CellContent, not via flags.
    void clearTankMarks() {}

     /// Fill this board from the simulator’s map snapshot
    void loadFromSatelliteView(const SatelliteView& view);

private:
    // bool verbose_;
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<std::vector<Cell>> grid_;
};
}