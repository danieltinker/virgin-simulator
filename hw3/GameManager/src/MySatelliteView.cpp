// MySatelliteView.cpp
#include "MySatelliteView.h"
#include <iostream>
#include <mutex>
#include <thread>

namespace GameManager_315634022 {

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

MySatelliteView::MySatelliteView(const std::vector<std::vector<char>>& input_grid, 
                                 std::size_t input_rows, std::size_t input_cols, 
                                 int input_tank_x, int input_tank_y)
    : SatelliteView(), // Use default constructor
      rows(input_rows), cols(input_cols), tank_x(input_tank_x), tank_y(input_tank_y)
{
    DEBUG_PRINT("SATELLITEVIEW", "constructor", 
        "Creating MySatelliteView - dimensions: " + std::to_string(cols) + "x" + std::to_string(rows) + 
        ", tank position: (" + std::to_string(tank_x) + "," + std::to_string(tank_y) + ")", true);

    // Initialize both 2D and 1D representations
    grid.reserve(rows);
    row_pointers_.reserve(rows);
    flat_grid.reserve(rows * cols);
    
    DEBUG_PRINT("SATELLITEVIEW", "constructor", "Initializing grid data structures", true);
    
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
    
    // Validation and verification
    bool validation_passed = true;
    if (grid.size() != rows) {
        ERROR_PRINT("SATELLITEVIEW", "constructor", 
            "Grid size mismatch - expected " + std::to_string(rows) + ", got " + std::to_string(grid.size()));
        validation_passed = false;
    }
    
    if (flat_grid.size() != rows * cols) {
        ERROR_PRINT("SATELLITEVIEW", "constructor", 
            "Flat grid size mismatch - expected " + std::to_string(rows * cols) + ", got " + std::to_string(flat_grid.size()));
        validation_passed = false;
    }
    
    if (row_pointers_.size() != rows) {
        ERROR_PRINT("SATELLITEVIEW", "constructor", 
            "Row pointers size mismatch - expected " + std::to_string(rows) + ", got " + std::to_string(row_pointers_.size()));
        validation_passed = false;
    }
    
    // Verify row pointers are valid
    for (std::size_t i = 0; i < row_pointers_.size(); ++i) {
        if (row_pointers_[i] == nullptr) {
            ERROR_PRINT("SATELLITEVIEW", "constructor", 
                "Row pointer " + std::to_string(i) + " is null");
            validation_passed = false;
        }
    }
    
    if (validation_passed) {
        DEBUG_PRINT("SATELLITEVIEW", "constructor", 
            std::string("MySatelliteView construction completed successfully - ") +
            "grid.size()=" + std::to_string(grid.size()) + 
            ", flat_grid.size()=" + std::to_string(flat_grid.size()) + 
            ", row_pointers.size()=" + std::to_string(row_pointers_.size()), true);
    } else {
        ERROR_PRINT("SATELLITEVIEW", "constructor", "MySatelliteView construction failed validation");
    }
}

} // namespace GameManager_315634022