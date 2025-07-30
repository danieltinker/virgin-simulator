// // include/MySatelliteView.h
// #pragma once

// #include "SatelliteView.h"
// #include <vector>

// namespace GameManager_315634022 {

// /// Concrete SatelliteView holding a full grid snapshot plus '%' at the querying tank.
// class MySatelliteView : public SatelliteView {
// public:
//     /// @param board   current board snapshot (grid[y][x])
//     /// @param rows    number of rows in the grid
//     /// @param cols    number of columns
//     /// @param queryX  x-coordinate of querying tank
//     /// @param queryY  y-coordinate of querying tank
//     MySatelliteView(const std::vector<std::vector<char>>& board,
//                     std::size_t rows,
//                     std::size_t cols,
//                     std::size_t queryX,
//                     std::size_t queryY)
//       : rows_(rows), cols_(cols), grid_(board)
//     {
//         if (queryX < cols_ && queryY < rows_) {
//             grid_[queryY][queryX] = '%';
//         }
//     }

//     char getObjectAt(std::size_t x, std::size_t y) const override {
//         if (x >= cols_ || y >= rows_) {
//             return '&';
//         }
//         return grid_[y][x];
//     }

// private:
//     std::size_t rows_, cols_;
//     std::vector<std::vector<char>> grid_;
// };
//} namespace GameManager_315634022



//////////////////////////////////////////////////
// // MySatelliteView.h
// #pragma once
// #include <SatelliteView.h>
// #include <vector>

// namespace GameManager_315634022 {

// class MySatelliteView : public SatelliteView {
// private:
//     // Store the actual data that the base class pointers reference
//     std::vector<std::vector<char>> grid_data_;
//     std::vector<char*> row_pointers_;
    
//     std::size_t rows_;
//     std::size_t cols_;
//     int tank_x_;
//     int tank_y_;

// public:
//     MySatelliteView(const std::vector<std::vector<char>>& grid, 
//                     std::size_t rows, std::size_t cols, 
//                     int tank_x, int tank_y);
    
//     virtual ~MySatelliteView() = default;
    
//     // Implement the pure virtual function from SatelliteView
//     virtual char getObjectAt(size_t x, size_t y) const override {
//         if (y >= rows_ || x >= cols_) {
//             return ' '; // return space for out-of-bounds
//         }
//         return grid_data_[y][x];
//     }
    
//     // Override other methods that external algorithms might expect
//     std::size_t getRows() const { return rows_; }
//     std::size_t getCols() const { return cols_; }
//     int getTankX() const { return tank_x_; }
//     int getTankY() const { return tank_y_; }
//     char** getGrid() const { return const_cast<char**>(row_pointers_.data()); }
// };

// } // namespace GameManager_315634022





// Maybe the external algorithm expects a flattened vector representation
// Let's try providing multiple data representations


// MySatelliteView.h - Maximum compatibility approach
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
