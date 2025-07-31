#include "TankAlgorithmRegistration.h"
#include "TankAlgorithm_315634022.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <typeinfo>

using namespace Algorithm_315634022;
REGISTER_TANK_ALGORITHM(TankAlgorithm_315634022);

// Enterprise-level debug logging macros
#define DEBUG_LOG(level, msg) \
    do { \
        std::cout << "[T" << std::this_thread::get_id() << "] " \
                  << "[" << level << "] " \
                  << "[TANK_315634022] " \
                  << "[" << __FUNCTION__ << ":" << __LINE__ << "] " \
                  << msg << std::endl; \
        std::cout.flush(); \
    } while(0)

#define DEBUG_PTR(ptr_name, ptr) \
    DEBUG_LOG("DEBUG", #ptr_name " address: " << (void*)(ptr))

#define DEBUG_ENTER() DEBUG_LOG("ENTER", "Function entry")
#define DEBUG_EXIT() DEBUG_LOG("EXIT", "Function exit")

// ——— CTOR ——————————————————————————————————————————————————————————————
TankAlgorithm_315634022::TankAlgorithm_315634022(int playerIndex, int tankIndex)
  : lastInfo_{1,1},
    direction_(playerIndex == 1 ? 6 : 2),
    shellsLeft_(-1),
    needView_(true)
{
    DEBUG_ENTER();
    DEBUG_LOG("INFO", "Constructor - playerIndex: " << playerIndex 
              << ", tankIndex: " << tankIndex 
              << ", direction: " << direction_
              << ", shellsLeft: " << shellsLeft_
              << ", needView: " << needView_);
    
    // Validate member initialization
    DEBUG_LOG("DEBUG", "lastInfo_ dimensions: " << lastInfo_.rows << "x" << lastInfo_.cols);
    DEBUG_LOG("DEBUG", "lastInfo_ grid size: " << lastInfo_.grid.size());
    
    DEBUG_EXIT();
}

// ——— updateBattleInfo ————————————————————————————————————————————————
void TankAlgorithm_315634022::updateBattleInfo(BattleInfo &baseInfo) {
    DEBUG_ENTER();
    
    try {
        // Critical safety checks
        DEBUG_PTR("baseInfo", &baseInfo);
        
        DEBUG_LOG("DEBUG", "Received BattleInfo object");
        DEBUG_LOG("DEBUG", "baseInfo type: " << typeid(baseInfo).name());
        
        // DANGEROUS CAST - This is likely where the segfault occurs
        DEBUG_LOG("DEBUG", "Attempting dynamic_cast to MyBattleInfo...");
        
        // Use dynamic_cast instead of static_cast for safety
        MyBattleInfo* myInfo = dynamic_cast<MyBattleInfo*>(&baseInfo);
        if (myInfo == nullptr) {
            DEBUG_LOG("ERROR", "CRITICAL: dynamic_cast to MyBattleInfo* failed!");
            DEBUG_LOG("ERROR", "baseInfo is not actually a MyBattleInfo object");
            DEBUG_LOG("ERROR", "This explains the segmentation fault!");
            DEBUG_EXIT();
            return;
        }
        
        DEBUG_LOG("SUCCESS", "dynamic_cast to MyBattleInfo* succeeded");
        DEBUG_PTR("myInfo", myInfo);
        
        // Validate the MyBattleInfo object
        DEBUG_LOG("DEBUG", "Validating MyBattleInfo object...");
        DEBUG_LOG("DEBUG", "MyBattleInfo dimensions: " << myInfo->rows << "x" << myInfo->cols);
        DEBUG_LOG("DEBUG", "MyBattleInfo grid size: " << myInfo->grid.size());
        DEBUG_LOG("DEBUG", "MyBattleInfo selfX: " << myInfo->selfX << ", selfY: " << myInfo->selfY);
        DEBUG_LOG("DEBUG", "MyBattleInfo shellsRemaining: " << myInfo->shellsRemaining);
        
        // Validate grid structure
        if (myInfo->grid.empty()) {
            DEBUG_LOG("WARNING", "MyBattleInfo grid is empty");
        } else {
            DEBUG_LOG("DEBUG", "Grid first row size: " << myInfo->grid[0].size());
            
            // Log grid contents
            DEBUG_LOG("DEBUG", "Grid contents:");
            for (std::size_t y = 0; y < myInfo->rows && y < myInfo->grid.size(); ++y) {
                std::string row_str = "";
                for (std::size_t x = 0; x < myInfo->cols && x < myInfo->grid[y].size(); ++x) {
                    row_str += myInfo->grid[y][x];
                }
                DEBUG_LOG("DEBUG", "Row " << y << ": '" << row_str << "'");
            }
        }
        
        // Safe copy operation
        DEBUG_LOG("DEBUG", "Performing safe copy of MyBattleInfo...");
        DEBUG_LOG("DEBUG", "Before copy - lastInfo_ dimensions: " << lastInfo_.rows << "x" << lastInfo_.cols);
        
        lastInfo_ = *myInfo;  // This line could also cause issues if copy is broken
        
        DEBUG_LOG("DEBUG", "After copy - lastInfo_ dimensions: " << lastInfo_.rows << "x" << lastInfo_.cols);
        DEBUG_LOG("DEBUG", "After copy - lastInfo_ grid size: " << lastInfo_.grid.size());
        
        // Shell count management
        if (shellsLeft_ < 0) {
            DEBUG_LOG("DEBUG", "First battle info update - setting shellsLeft_");
            shellsLeft_ = int(lastInfo_.shellsRemaining);
            DEBUG_LOG("INFO", "shellsLeft_ initialized to: " << shellsLeft_);
        } else {
            DEBUG_LOG("DEBUG", "Subsequent update - shellsLeft_ remains: " << shellsLeft_);
        }
        
        needView_ = false;
        DEBUG_LOG("DEBUG", "needView_ set to false");
        
        DEBUG_LOG("SUCCESS", "updateBattleInfo completed successfully");
        
    } catch (const std::bad_cast& e) {
        DEBUG_LOG("ERROR", "Bad cast exception: " << e.what());
        throw;
    } catch (const std::exception& e) {
        DEBUG_LOG("ERROR", "Exception in updateBattleInfo: " << e.what());
        throw;
    } catch (...) {
        DEBUG_LOG("ERROR", "Unknown exception in updateBattleInfo");
        throw;
    }
    
    DEBUG_EXIT();
}

// ——— getAction —————————————————————————————————————————————————————————
ActionRequest TankAlgorithm_315634022::getAction() {
    DEBUG_ENTER();
    
    DEBUG_LOG("DEBUG", "Current state - direction_: " << direction_ 
              << ", shellsLeft_: " << shellsLeft_ 
              << ", needView_: " << needView_);
    
    // Debug the logic flow
    if (needView_) {
        DEBUG_LOG("DEBUG", "Need view is true - requesting battle info");
        needView_ = false;
        DEBUG_EXIT();
        return ActionRequest::GetBattleInfo;
    }
    
    DEBUG_LOG("DEBUG", "Returning GetBattleInfo (default action)");
    DEBUG_EXIT();
    return ActionRequest::GetBattleInfo;
}