// MySatelliteView.h
#pragma once
#include <SatelliteView.h>
#include <vector>

namespace GameManager_315634022 {

class MySatelliteView : public SatelliteView {
public:
    // Multiple data representations to match what external algorithms might expect
    std::vector<std::vector<char>> grid;  // 2D grid
    std::vector<char> flat_grid;          // 1D flattened grid
    std::size_t rows;
    std::size_t cols;
    int tank_x;
    int tank_y;

private:
    std::vector<char*> row_pointers_;

public:
    MySatelliteView(const std::vector<std::vector<char>>& input_grid, 
                    std::size_t input_rows, std::size_t input_cols, 
                    int input_tank_x, int input_tank_y);
    
    virtual ~MySatelliteView() = default;
    
    // Implement the pure virtual function from SatelliteView
    virtual char getObjectAt(size_t x, size_t y) const override {
        if (y >= rows || x >= cols) {
            return ' ';
        }
        return grid[y][x];
    }
    
    // Provide accessors
    std::size_t getRows() const { return rows; }
    std::size_t getCols() const { return cols; }
    int getTankX() const { return tank_x; }
    int getTankY() const { return tank_y; }
    char** getGrid() const { return const_cast<char**>(row_pointers_.data()); }
    
    // Additional accessors for potential compatibility
    const std::vector<char>& getFlatGrid() const { return flat_grid; }
    char* getFlatGridPtr() { return flat_grid.data(); }
};

} // namespace GameManager_315634022
