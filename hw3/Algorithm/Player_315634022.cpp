#include "Player_315634022.h"
#include "PlayerRegistration.h"
#include "ActionRequest.h"
#include "MyBattleInfo.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace Algorithm_315634022;

REGISTER_PLAYER(Player_315634022);

// Enterprise-level debug logging macros
#define DEBUG_LOG(level, msg) \
    do { \
        std::cout << "[T" << std::this_thread::get_id() << "] " \
                  << "[" << level << "] " \
                  << "[PLAYER_315634022] " \
                  << "[" << __FUNCTION__ << ":" << __LINE__ << "] " \
                  << msg << std::endl; \
        std::cout.flush(); \
    } while(0)

#define DEBUG_PTR(ptr_name, ptr) \
    DEBUG_LOG("DEBUG", #ptr_name " address: " << (void*)(ptr))

#define DEBUG_ENTER() DEBUG_LOG("ENTER", "Function entry")
#define DEBUG_EXIT() DEBUG_LOG("EXIT", "Function exit")

Player_315634022::Player_315634022(int playerIndex,
                                   std::size_t rows,
                                   std::size_t cols,
                                   std::size_t max_steps,
                                   std::size_t num_shells)
  : playerIndex_(playerIndex),
    rows_(rows),
    cols_(cols),
    shells_(num_shells),
    first_(true)
{
    DEBUG_ENTER();
    DEBUG_LOG("INFO", "Constructor parameters - playerIndex: " << playerIndex 
              << ", rows: " << rows << ", cols: " << cols 
              << ", max_steps: " << max_steps << ", num_shells: " << num_shells);
    DEBUG_EXIT();
}

void Player_315634022::updateTankWithBattleInfo(TankAlgorithm &tank,
                                               SatelliteView &view) {
    DEBUG_ENTER();
    
    // Critical safety checks with detailed logging
    DEBUG_PTR("tank", &tank);
    DEBUG_PTR("view", &view);
    
    try {
        DEBUG_LOG("INFO", "Starting battle info update - playerIndex: " << playerIndex_ 
                  << ", dimensions: " << rows_ << "x" << cols_ << ", first_call: " << first_);
        
        // Validate SatelliteView before using it
        DEBUG_LOG("DEBUG", "Testing SatelliteView access...");
        
        // Test a safe coordinate first
        if (rows_ > 0 && cols_ > 0) {
            DEBUG_LOG("DEBUG", "Attempting to read position (0,0) from SatelliteView");
            char test_char = view.getObjectAt(0, 0);
            DEBUG_LOG("DEBUG", "Successfully read from SatelliteView at (0,0): '" << test_char << "'");
        } else {
            DEBUG_LOG("ERROR", "Invalid board dimensions - rows: " << rows_ << ", cols: " << cols_);
            DEBUG_EXIT();
            return;
        }
        
        // Create MyBattleInfo with validation
        DEBUG_LOG("DEBUG", "Creating MyBattleInfo with dimensions " << rows_ << "x" << cols_);
        MyBattleInfo info(rows_, cols_);
        
        // Validate the created info object
        DEBUG_LOG("DEBUG", "MyBattleInfo created successfully - grid size: " 
                  << info.grid.size() << " rows");
        if (!info.grid.empty()) {
            DEBUG_LOG("DEBUG", "First row size: " << info.grid[0].size() << " cols");
        }
        
        // Shell count logic
        if (first_) {
            DEBUG_LOG("DEBUG", "First call - setting shells remaining to: " << shells_);
            info.shellsRemaining = shells_;
            first_ = false;
        } else {
            DEBUG_LOG("DEBUG", "Subsequent call - shells remaining unchanged");
        }
        
        // Grid scanning with detailed position logging
        DEBUG_LOG("DEBUG", "Starting grid scan...");
        bool selfFound = false;
        std::size_t selfX = 0, selfY = 0;
        
        for (std::size_t y = 0; y < rows_; ++y) {
            // DEBUG_LOG("TRACE", "Scanning row " << y << "/" << rows_);
            
            for (std::size_t x = 0; x < cols_; ++x) {
                // Bounds checking with detailed logging
                if (y >= info.grid.size()) {
                    DEBUG_LOG("ERROR", "Row index " << y << " exceeds grid size " << info.grid.size());
                    DEBUG_EXIT();
                    return;
                }
                if (x >= info.grid[y].size()) {
                    DEBUG_LOG("ERROR", "Col index " << x << " exceeds row size " << info.grid[y].size());
                    DEBUG_EXIT();
                    return;
                }
                
                // Safe SatelliteView access
                // DEBUG_LOG("TRACE", "Reading position (" << x << "," << y << ")");
                char c = view.getObjectAt(x, y);
                // DEBUG_LOG("TRACE", "Position (" << x << "," << y << "): '" << c << "'");
                
                info.grid[y][x] = c;
                
                // Self-location detection
                if ((playerIndex_ == 1 && c == '1') || (playerIndex_ == 2 && c == '2')) {
                    DEBUG_LOG("INFO", "Found self at position (" << x << "," << y << ") with character '" << c << "'");
                    selfX = x;
                    selfY = y;
                    selfFound = true;
                }
            }
        }
        
        // Validate self-location
        if (selfFound) {
            info.selfX = selfX;
            info.selfY = selfY;
            // DEBUG_LOG("INFO", "Self position set to (" << info.selfX << "," << info.selfY << ")");
        } else {
            DEBUG_LOG("WARNING", "Self position not found on grid for player " << playerIndex_);
        }
        
        // Final grid state logging
        DEBUG_LOG("DEBUG", "Grid scan complete. Final grid state:");
        for (std::size_t y = 0; y < rows_; ++y) {
            std::string row_str = "";
            for (std::size_t x = 0; x < cols_; ++x) {
                row_str += info.grid[y][x];
            }
            // DEBUG_LOG("DEBUG", "Row " << y << ": '" << row_str << "'");
        }
        
        // Critical: Tank algorithm update with safety checks
        DEBUG_LOG("DEBUG", "Preparing to call tank.updateBattleInfo()");
        DEBUG_PTR("tank_before_call", &tank);
        DEBUG_PTR("info_address", &info);
        
        // Test if tank object is valid by calling a safe method first
        DEBUG_LOG("DEBUG", "Testing tank object validity...");
        
        // THIS IS THE CRITICAL LINE - where the segfault likely occurs
        DEBUG_LOG("DEBUG", "Calling tank.updateBattleInfo() - THIS IS WHERE CRASH LIKELY OCCURS");
        tank.updateBattleInfo(info);
        DEBUG_LOG("SUCCESS", "tank.updateBattleInfo() completed successfully");
        
    } catch (const std::exception& e) {
        DEBUG_LOG("ERROR", "Exception caught: " << e.what());
        throw;
    } catch (...) {
        DEBUG_LOG("ERROR", "Unknown exception caught");
        throw;
    }
    
    DEBUG_EXIT();
}