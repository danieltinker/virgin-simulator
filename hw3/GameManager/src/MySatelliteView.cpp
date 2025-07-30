// #include "MySatelliteView.h"








// // MySatelliteView.cpp
// #include "MySatelliteView.h"

// namespace GameManager_315634022 {

// MySatelliteView::MySatelliteView(const std::vector<std::vector<char>>& grid, 
//                                  std::size_t rows, std::size_t cols, 
//                                  int tank_x, int tank_y)
//     : SatelliteView(), // Use default constructor
//       rows_(rows), cols_(cols), tank_x_(tank_x), tank_y_(tank_y)
// {
//     // Deep copy the grid data
//     grid_data_.reserve(rows);
//     row_pointers_.reserve(rows);
    
//     for (std::size_t r = 0; r < rows; ++r) {
//         // Ensure each row has exactly cols_ characters
//         grid_data_.emplace_back(cols);
//         for (std::size_t c = 0; c < cols; ++c) {
//             if (c < grid[r].size()) {
//                 grid_data_[r][c] = grid[r][c];
//             } else {
//                 grid_data_[r][c] = ' '; // pad with spaces if needed
//             }
//         }
//         // Store pointer to the actual data
//         row_pointers_.push_back(grid_data_[r].data());
//     }
// }

// } // namespace GameManager_315634022













// MySatelliteView.cpp
#include "MySatelliteView.h"
#include <iostream>
namespace GameManager_315634022 {

MySatelliteView::MySatelliteView(const std::vector<std::vector<char>>& input_grid, 
                                 std::size_t input_rows, std::size_t input_cols, 
                                 int input_tank_x, int input_tank_y)
    : SatelliteView(), // Use default constructor
      rows(input_rows), cols(input_cols), tank_x(input_tank_x), tank_y(input_tank_y)
{
    // Initialize both 2D and 1D representations
    grid.reserve(rows);
    row_pointers_.reserve(rows);
    flat_grid.reserve(rows * cols);
    
    for (std::size_t r = 0; r < rows; ++r) {
        // 2D grid
        grid.emplace_back(cols);
        for (std::size_t c = 0; c < cols; ++c) {
            char cell_char = ' ';
            if (c < input_grid[r].size()) {
                cell_char = input_grid[r][c];
            }
            grid[r][c] = cell_char;
            flat_grid.push_back(cell_char); // Also add to flat grid
        }
        // Store pointer to the actual row data
        row_pointers_.push_back(grid[r].data());
    }
    
    // Debug output to verify our data
    std::cout << "MySatelliteView constructed:" << std::endl;
    std::cout << "  rows=" << rows << ", cols=" << cols << std::endl;
    std::cout << "  tank=(" << tank_x << "," << tank_y << ")" << std::endl;
    std::cout << "  grid.size()=" << grid.size() << std::endl;
    std::cout << "  flat_grid.size()=" << flat_grid.size() << std::endl;
    std::cout << "  row_pointers_.size()=" << row_pointers_.size() << std::endl;
}

} // namespace GameManager_315634022