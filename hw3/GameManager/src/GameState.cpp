// GameState.cpp
#include "GameState.h"
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>

using namespace GameManager_315634022;

// ——————————————————————————————————————————————————
// Professional Debug Logging System
// ——————————————————————————————————————————————————

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

GameState::GameState(
    Board&&                board,
    std::string            map_name,
    std::size_t            max_steps,
    std::size_t            num_shells,
    Player&                player1,
    const std::string&     name1,
    Player&                player2,
    const std::string&     name2,
    TankAlgorithmFactory   factory1,
    TankAlgorithmFactory   factory2,
    bool                   verbose
)
  : verbose_(verbose)
  , board_(std::move(board))
  , map_name_(std::move(map_name))
  , max_steps_(max_steps)
  , currentStep_(0)
  , num_shells_(num_shells)
  , gameOver_(false)
  , resultStr_()
  , p1_(player1)
  , name1_(name1)
  , p2_(player2)
  , name2_(name2)
  , algoFactory1_(std::move(factory1))
  , algoFactory2_(std::move(factory2))
{
    INFO_PRINT("GAMESTATE", "constructor", "Initializing GameState");
    DEBUG_PRINT("GAMESTATE", "constructor", 
        "Parameters - Map: " + map_name_ + 
        ", MaxSteps: " + std::to_string(max_steps_) + 
        ", NumShells: " + std::to_string(num_shells_), verbose_);

    // --- Initialize tanks from board ---
    rows_ = board_.getRows();
    cols_ = board_.getCols();
    nextTankIndex_[1] = nextTankIndex_[2] = 0;
    tankIdMap_.assign(3, std::vector<std::size_t>(rows_*cols_, SIZE_MAX));

    DEBUG_PRINT("GAMESTATE", "constructor", 
        "Board dimensions: " + std::to_string(cols_) + "x" + std::to_string(rows_), verbose_);

    INFO_PRINT("TANKMANAGER", "constructor", "Scanning board for tank positions");
    
    for (std::size_t r = 0; r < rows_; ++r) {
        for (std::size_t c = 0; c < cols_; ++c) {
            const auto& cell = board_.getCell(int(c), int(r));
            if (cell.content == CellContent::TANK1 || cell.content == CellContent::TANK2) {
                int pidx = (cell.content == CellContent::TANK1 ? 1 : 2);
                int tidx = nextTankIndex_[pidx]++;
                
                TankState ts{
                /*player_index*/   pidx,
                /*tank_index*/     tidx,
                /*x*/              int(c),
                /*y*/              int(r),
                /*direction*/      (pidx==1?6:2),
                /*alive*/          true,
                /*shells_left*/    num_shells_,
                /*shootCooldown*/  0,
                /*backwardDelay*/  0,
                /*lastActionBack*/ false
                };

                all_tanks_.push_back(ts);
                tankIdMap_[pidx][tidx] = all_tanks_.size()-1;
                
                DEBUG_PRINT("TANKMANAGER", "constructor", 
                    "Tank discovered - Player " + std::to_string(pidx) + 
                    ", Index " + std::to_string(tidx) + 
                    " at (" + std::to_string(c) + "," + std::to_string(r) + ")", verbose_);
            }
        }
    }

    INFO_PRINT("TANKMANAGER", "constructor", 
        "Tank scan complete - Found " + std::to_string(all_tanks_.size()) + " tanks total");

    // --- Create per‐tank algorithms ---
    all_tank_algorithms_.clear();
    DEBUG_PRINT("ALGOMANAGER", "constructor", "Starting algorithm creation for all tanks", verbose_);
    
    for (size_t i = 0; i < all_tanks_.size(); ++i) {
        auto& ts = all_tanks_[i];
        DEBUG_PRINT("ALGOMANAGER", "constructor", 
            "Creating algorithm for Tank " + std::to_string(i) + 
            " (Player " + std::to_string(ts.player_index) + 
            ", TankIndex " + std::to_string(ts.tank_index) + 
            " at (" + std::to_string(ts.x) + "," + std::to_string(ts.y) + "))", verbose_);
        
        std::unique_ptr<TankAlgorithm> algo;
        try {
            if (ts.player_index == 1) {
                DEBUG_PRINT("ALGOMANAGER", "constructor", "Calling factory1 for player 1", verbose_);
                algo = algoFactory1_(ts.player_index, ts.tank_index);
            } else {
                DEBUG_PRINT("ALGOMANAGER", "constructor", "Calling factory2 for player 2", verbose_);
                algo = algoFactory2_(ts.player_index, ts.tank_index);
            }
            
            if (algo) {
                DEBUG_PRINT("ALGOMANAGER", "constructor", 
                    "Algorithm created successfully for Tank " + std::to_string(i), verbose_);
                
                // Test basic algorithm functionality
                DEBUG_PRINT("ALGOMANAGER", "constructor", 
                    "Testing algorithm with getAction() call", verbose_);
                try {
                    ActionRequest test_action = algo->getAction();
                    DEBUG_PRINT("ALGOMANAGER", "constructor", 
                        "Algorithm test successful, returned action: " + std::to_string(static_cast<int>(test_action)), verbose_);
                } catch (const std::exception& e) {
                    ERROR_PRINT("ALGOMANAGER", "constructor", 
                        "Algorithm getAction() test failed with exception: " + std::string(e.what()));
                    throw;
                } catch (...) {
                    ERROR_PRINT("ALGOMANAGER", "constructor", 
                        "Algorithm getAction() test failed with unknown exception");
                    throw;
                }
                
                all_tank_algorithms_.push_back(std::move(algo));
            } else {
                ERROR_PRINT("ALGOMANAGER", "constructor", 
                    "Factory returned null algorithm for Tank " + std::to_string(i));
                throw std::runtime_error("Factory returned null algorithm");
            }
        } catch (const std::exception& e) {
            ERROR_PRINT("ALGOMANAGER", "constructor", 
                "Exception during algorithm creation for Tank " + std::to_string(i) + ": " + std::string(e.what()));
            throw;
        } catch (...) {
            ERROR_PRINT("ALGOMANAGER", "constructor", 
                "Unknown exception during algorithm creation for Tank " + std::to_string(i));
            throw;
        }
    }

    INFO_PRINT("ALGOMANAGER", "constructor", 
        "Algorithm creation complete - " + std::to_string(all_tank_algorithms_.size()) + " algorithms created");

    // Verify all algorithms are still valid
    DEBUG_PRINT("ALGOMANAGER", "constructor", "Verifying algorithm validity", verbose_);
    for (size_t i = 0; i < all_tank_algorithms_.size(); ++i) {
        if (all_tank_algorithms_[i].get() == nullptr) {
            ERROR_PRINT("ALGOMANAGER", "constructor", 
                "Algorithm " + std::to_string(i) + " became null after creation");
            throw std::runtime_error("Algorithm became null after creation");
        }
        DEBUG_PRINT("ALGOMANAGER", "constructor", 
            "Algorithm " + std::to_string(i) + " verified valid", verbose_);
    }

    // Initialize game state containers
    shells_.clear();
    toRemove_.clear();
    positionMap_.clear();
    
    INFO_PRINT("GAMESTATE", "constructor", "GameState initialization completed successfully");
}

GameState::~GameState() = default;

bool GameState::isGameOver() const { return gameOver_; }
std::string GameState::getResultString() const { return resultStr_; }

static const char* actionToString(ActionRequest a) {
    switch (a) {
      case ActionRequest::MoveForward:    return "MoveForward";
      case ActionRequest::MoveBackward:   return "MoveBackward";
      case ActionRequest::RotateLeft90:   return "RotateLeft90";
      case ActionRequest::RotateRight90:  return "RotateRight90";
      case ActionRequest::RotateLeft45:   return "RotateLeft45";
      case ActionRequest::RotateRight45:  return "RotateRight45";
      case ActionRequest::Shoot:          return "Shoot";
      case ActionRequest::GetBattleInfo:  return "GetBattleInfo";
      default:                            return "DoNothing";
    }
}

std::string GameState::advanceOneTurn() {
    if (gameOver_) {
        DEBUG_PRINT("GAMELOOP", "advanceOneTurn", "Game already over, returning empty string", verbose_);
        return "";
    }

    const size_t N = all_tanks_.size();
    std::vector<ActionRequest> actions(N, ActionRequest::DoNothing);
    std::vector<bool> ignored(N, false), killed(N, false);

    DEBUG_PRINT("GAMELOOP", "advanceOneTurn", 
        "Starting turn " + std::to_string(currentStep_ + 1) + 
        " with " + std::to_string(N) + " tanks", verbose_);

    // 1) Gather raw requests
    INFO_PRINT("ACTIONGATHER", "advanceOneTurn", "Gathering action requests from all tanks");
    
    for (size_t k = 0; k < N; ++k) {
        auto& ts  = all_tanks_[k];
        auto& alg = *all_tank_algorithms_[k];
        if (!ts.alive) {
            DEBUG_PRINT("ACTIONGATHER", "advanceOneTurn", 
                "Tank " + std::to_string(k) + " is dead, skipping action request", verbose_);
            continue;
        }

        ActionRequest req = alg.getAction();
        DEBUG_PRINT("ACTIONGATHER", "advanceOneTurn", 
            "Tank " + std::to_string(k) + " requested: " + actionToString(req), verbose_);

        if (req == ActionRequest::GetBattleInfo) {
            INFO_PRINT("BATTLEINFO", "advanceOneTurn", 
                "Tank " + std::to_string(k) + " requested battle info");
            
            // Build a visibility snapshot
            std::vector<std::vector<char>> grid(rows_, std::vector<char>(cols_, ' '));
            for (size_t yy = 0; yy < rows_; ++yy) {
                for (size_t xx = 0; xx < cols_; ++xx) {
                    const auto& cell = board_.getCell(int(xx), int(yy));
                    grid[yy][xx] = (cell.content==CellContent::WALL ? '#' :
                        cell.content==CellContent::MINE ? '@' :
                        cell.content==CellContent::TANK1 ? '1' :
                        cell.content==CellContent::TANK2 ? '2' : ' ');
                }
            }
            
            // Mark the querying tank's position specially
            if (ts.y >= 0 && ts.y < static_cast<int>(rows_) && 
                ts.x >= 0 && ts.x < static_cast<int>(cols_)) {
                grid[ts.y][ts.x] = '%';
            }
            
            DEBUG_PRINT("BATTLEINFO", "advanceOneTurn", 
                "Creating MySatelliteView for Tank " + std::to_string(k) + 
                " at (" + std::to_string(ts.x) + "," + std::to_string(ts.y) + ")", verbose_);
            
            MySatelliteView view(grid, rows_, cols_, ts.x, ts.y);
            
            // Validate the view before using it
            char** test_grid = view.getGrid();
            if (test_grid == nullptr) {
                ERROR_PRINT("BATTLEINFO", "advanceOneTurn", 
                    "MySatelliteView returned null grid pointer for Tank " + std::to_string(k));
            } else if (test_grid[0] == nullptr) {
                ERROR_PRINT("BATTLEINFO", "advanceOneTurn", 
                    "MySatelliteView first row is null for Tank " + std::to_string(k));
            } else {
                DEBUG_PRINT("BATTLEINFO", "advanceOneTurn", 
                    "MySatelliteView validation successful for Tank " + std::to_string(k), verbose_);
            }
            
            DEBUG_PRINT("BATTLEINFO", "advanceOneTurn", 
                "Updating tank with battle info via player interface", verbose_);
            
            if (ts.player_index == 1) {
                p1_.updateTankWithBattleInfo(alg, view);
            } else {
                p2_.updateTankWithBattleInfo(alg, view);
            }
            
            DEBUG_PRINT("BATTLEINFO", "advanceOneTurn", 
                "Tank " + std::to_string(k) + " battle info update completed", verbose_);
            actions[k] = ActionRequest::GetBattleInfo;
        } else {
            actions[k] = req;
        }
    }

    // Snapshot the original requests for logging
    std::vector<ActionRequest> logActions = actions;
    
    INFO_PRINT("BACKWARDLOGIC", "advanceOneTurn", "Processing backward movement delay logic");

    // 2) Backward‐delay logic (2 turns idle, 3rd turn executes)
    for (size_t k = 0; k < N; ++k) {
        auto& ts   = all_tanks_[k];
        auto  orig = logActions[k];

        // (A) Mid‐delay from a previous MoveBackward?
        if (ts.backwardDelayCounter > 0) {
            DEBUG_PRINT("BACKWARDLOGIC", "advanceOneTurn", 
                "Tank " + std::to_string(k) + " has backward delay counter: " + 
                std::to_string(ts.backwardDelayCounter), verbose_);
            
            --ts.backwardDelayCounter;
            if (ts.backwardDelayCounter == 0) {
                // 3rd turn → actually move backward
                DEBUG_PRINT("BACKWARDLOGIC", "advanceOneTurn", 
                    "Tank " + std::to_string(k) + " executing delayed backward movement", verbose_);
                ts.lastActionBackwardExecuted = true;
                actions[k] = ActionRequest::MoveBackward;
                ignored[k] = true;
            } else {
                // still in delay → only forward/info allowed
                if (orig == ActionRequest::MoveForward) {
                    DEBUG_PRINT("BACKWARDLOGIC", "advanceOneTurn", 
                        "Tank " + std::to_string(k) + " forward movement cancels backward delay", verbose_);
                    // cancel the delay
                    ts.backwardDelayCounter       = 0;
                    ts.lastActionBackwardExecuted = false;
                    actions[k] = ActionRequest::DoNothing;
                    ignored[k] = false;
                } else if (orig == ActionRequest::GetBattleInfo) {
                    actions[k] = ActionRequest::GetBattleInfo;
                    ignored[k] = false;
                } else {
                    DEBUG_PRINT("BACKWARDLOGIC", "advanceOneTurn", 
                        "Tank " + std::to_string(k) + " action ignored due to backward delay", verbose_);
                    actions[k] = ActionRequest::DoNothing;
                    ignored[k] = true;
                }
            }
            continue;
        }

        // (B) No pending delay: new MoveBackward request?
        if (orig == ActionRequest::MoveBackward) {
            DEBUG_PRINT("BACKWARDLOGIC", "advanceOneTurn", 
                "Tank " + std::to_string(k) + " initiating backward movement delay", verbose_);
            // schedule exactly 2 idle turns then exec on the 3rd
            ts.backwardDelayCounter       = ts.lastActionBackwardExecuted ? 1 : 3;
            ts.lastActionBackwardExecuted = false;

            // do nothing this turn (exec will happen when counter→0)
            actions[k] = ActionRequest::DoNothing;
            ignored[k] = false;
            continue;
        }

        // (C) All other actions clear the "just did backward" flag
        ts.lastActionBackwardExecuted = false;
    }

    // 3) Process game mechanics
    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Applying tank rotations");
    applyTankRotations(actions);
    
    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Handling tank-mine collisions");
    handleTankMineCollisions();

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Updating tank cooldowns");
    updateTankCooldowns();

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Confirming backward moves");
    confirmBackwardMoves(ignored, actions);

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Updating shell positions");
    updateShellsWithOverrunCheck();
    resolveShellCollisions();

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Processing shooting actions");
    handleShooting(ignored, actions);

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Updating tank positions");
    updateTankPositionsOnBoard(ignored, killed, actions);

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Cleaning up destroyed entities");
    filterRemainingShells();
    cleanupDestroyedEntities();

    INFO_PRINT("GAMELOGIC", "advanceOneTurn", "Checking game end conditions");
    checkGameEndConditions();

    // Advance step & drop shoot cooldowns
    ++currentStep_;
    for (auto& ts : all_tanks_)
        if (ts.shootCooldown > 0) --ts.shootCooldown;

    DEBUG_PRINT("GAMELOOP", "advanceOneTurn", 
        "Turn " + std::to_string(currentStep_) + " completed", verbose_);

    // Console output of decisions and board state
    INFO_PRINT("TURNREPORT", "advanceOneTurn", "=== Turn Decisions Report ===");
    for (size_t k = 0; k < N; ++k) {
        const char* actName = actionToString(logActions[k]);
        bool wasIgnored = ignored[k] && logActions[k] != ActionRequest::GetBattleInfo;
        
        std::string status = wasIgnored ? " (ignored)" : " (accepted)";
        INFO_PRINT("TURNREPORT", "advanceOneTurn", 
            "Tank[" + std::to_string(k) + "]: " + std::string(actName) + status);
    }
    
    INFO_PRINT("TURNREPORT", "advanceOneTurn", "=== Current Board State ===");
    printBoard();

    // Generate logging string
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn", "Generating turn log string", verbose_);
    std::ostringstream oss;
    for (size_t k = 0; k < N; ++k) {
        const auto act = logActions[k];
        const char* name = actionToString(act);
        
        if (all_tanks_[k].alive) {
            oss << name;
            if (ignored[k] && act != ActionRequest::GetBattleInfo) {
                oss << " (ignored)";
            }
        } else {
            // tank died this turn
            if (act == ActionRequest::DoNothing) {
                oss << "killed";
            } else {
                oss << name << " (killed)";
            }
        }
        if (k + 1 < N) oss << ", ";
    }
    
    std::string result = oss.str();
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn", "Turn log: " + result, verbose_);
    return result;
}

std::size_t GameState::getCurrentTurn() const { return currentStep_; }

//------------------------------------------------------------------------------
void GameState::printBoard() const {
    // 1) Build a snapshot of the board with any moving shells overlaid
    auto gridCopy = board_.getGrid();
    for (auto const& sh : shells_)
    gridCopy[sh.y][sh.x].hasShellOverlay = true;

// 2) Mark each live tank on the grid
for (auto const& ts : all_tanks_) {
    if (!ts.alive) continue;
    gridCopy[ts.y][ts.x].content =
    ts.player_index == 1 ? CellContent::TANK1 : CellContent::TANK2;
}

// 3) Print row by row
for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            const auto& cell = gridCopy[r][c];

            // * Supercede everything: if there's a shell here, draw '*'
            if (cell.hasShellOverlay) {
                std::cout << '*';
                continue;
            }

            switch (cell.content) {
                case CellContent::WALL:
                    std::cout << '#';
                    break;

                case CellContent::EMPTY:
                    // empty ground
                    std::cout << '_';
                    break;

                case CellContent::MINE:
                    // if a shell landed on a mine, we’d already have shown '*'
                    std::cout << '@';
                    break;

                case CellContent::TANK1:
                case CellContent::TANK2: {
                    // find that tank’s direction arrow
                    int pid = (cell.content == CellContent::TANK1 ? 1 : 2);
                    int dir = 0;
                    for (auto const& ts : all_tanks_) {
                        if (ts.alive
                         && ts.player_index == pid
                         && ts.x == int(c)
                         && ts.y == int(r)) {
                            dir = ts.direction;
                            break;
                        }
                    }
                    const char* arr = directionToArrow(dir);
                    // color‐code tanks red/blue
                    std::cout << (pid == 1 ? "\033[31m" : "\033[34m")
                              << arr << "\033[0m";
                    break;
                }

                default:
                    // shouldn't happen
                    std::cout << '?';
                    break;
            }
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}


//------------------------------------------------------------------------------
const char* GameState::directionToArrow(int dir) {
    static const char* arr[8] = {"↑","↗","→","↘","↓","↙","←","↖"};
    return arr[dir&7];
}

//------------------------------------------------------------------------------
void GameState::applyTankRotations(const std::vector<ActionRequest>& A) {
    for (size_t k=0; k<all_tanks_.size(); ++k) {
        if (!all_tanks_[k].alive) continue;
        int& d = all_tanks_[k].direction;
        switch(A[k]) {
        case ActionRequest::RotateLeft90:  d=(d+6)&7; break;
        case ActionRequest::RotateRight90: d=(d+2)&7; break;
        case ActionRequest::RotateLeft45:  d=(d+7)&7; break;
        case ActionRequest::RotateRight45: d=(d+1)&7; break;
        default: break;
        }
    }
}

void GameState::handleTankMineCollisions() {
    for (auto& ts: all_tanks_) {
        if (!ts.alive) continue;
        auto& cell = board_.getCell(ts.x, ts.y);
        if (cell.content==CellContent::MINE) {
            ts.alive = false;
            cell.content = CellContent::EMPTY;
        }
    }
}

void GameState::updateTankCooldowns() {
    // unused
}

//------------------------------------------------------------------------------
// 5) Confirm backward–move legality (now with wrapping)
void GameState::confirmBackwardMoves(std::vector<bool>& ignored,
                                     const std::vector<ActionRequest>& A)
{
    for (size_t k = 0; k < all_tanks_.size(); ++k) {
        if (!all_tanks_[k].alive || A[k] != ActionRequest::MoveBackward)
            continue;

        // compute backward direction
        int back = (all_tanks_[k].direction + 4) & 7;
        int dx = 0, dy = 0;
        switch (back) {
        case 0: dy = -1; break;
        case 1: dx = +1; dy = -1; break;
        case 2: dx = +1; break;
        case 3: dx = +1; dy = +1; break;
        case 4: dy = +1; break;
        case 5: dx = -1; dy = +1; break;
        case 6: dx = -1; break;
        case 7: dx = -1; dy = -1; break;
        }

        int nx = all_tanks_[k].x + dx;
        int ny = all_tanks_[k].y + dy;
        // wrap around
        board_.wrapCoords(nx, ny);
        // illegal if there's a wall after wrapping
        if (board_.getCell(nx, ny).content == CellContent::WALL) {
            ignored[k] = true;
        }
    }
}

//------------------------------------------------------------------------------
// (6) updateTankPositionsOnBoard: move forward/backward, no wrap,
//     plus handle head‐on tank collisions and multi‐tank collisions.
//------------------------------------------------------------------------------
void GameState::updateTankPositionsOnBoard(std::vector<bool>& ignored,
                                           std::vector<bool>& killedThisTurn,
                                           const std::vector<ActionRequest>& actions)
{
     board_.clearTankMarks();

    const size_t N = all_tanks_.size();
    std::vector<std::pair<int,int>> oldPos(N), newPos(N);

    // 1) compute oldPos & newPos (with wrapping)
    for (size_t k = 0; k < N; ++k) {
        oldPos[k] = { all_tanks_[k].x, all_tanks_[k].y };

        if (!all_tanks_[k].alive
         || ignored[k]
         || (actions[k] != ActionRequest::MoveForward
          && actions[k] != ActionRequest::MoveBackward))
        {
            newPos[k] = oldPos[k];
            continue;
        }

        // figure out dx,dy
        int dir = all_tanks_[k].direction;
        if (actions[k] == ActionRequest::MoveBackward)
            dir = (dir + 4) & 7;

        int dx = 0, dy = 0;
        switch (dir) {
        case 0: dy = -1; break;
        case 1: dx = +1; dy = -1; break;
        case 2: dx = +1; break;
        case 3: dx = +1; dy = +1; break;
        case 4: dy = +1; break;
        case 5: dx = -1; dy = +1; break;
        case 6: dx = -1; break;
        case 7: dx = -1; dy = -1; break;
        }

        int nx = all_tanks_[k].x + dx;
        int ny = all_tanks_[k].y + dy;
        // wrap around the board edges
        board_.wrapCoords(nx, ny);

        // if after wrapping there's a wall, treat as ignored
        if (board_.getCell(nx, ny).content == CellContent::WALL) {
            newPos[k] = oldPos[k];
            ignored[k] = true;
        } else {
            newPos[k] = { nx, ny };
        }
    }

    // 2a) Head-on swaps: two tanks exchanging places → both die
    for (std::size_t i = 0; i < N; ++i) {
      for (std::size_t j = i+1; j < N; ++j) {
        if (!all_tanks_[i].alive || !all_tanks_[j].alive) continue;
        if (killedThisTurn[i] || killedThisTurn[j])        continue;
        if (newPos[i] == oldPos[j] && newPos[j] == oldPos[i]) {
          killedThisTurn[i] = killedThisTurn[j] = true;
          all_tanks_[i].alive = all_tanks_[j].alive = false;
          // clear both old positions
          board_.setCell(oldPos[i].first, oldPos[i].second, CellContent::EMPTY);
          board_.setCell(oldPos[j].first, oldPos[j].second, CellContent::EMPTY);
        }
      }
    }

    // 2b) Moving-into-stationary: a mover steps onto someone who stayed put → both die
    for (std::size_t k = 0; k < N; ++k) {
      if (!all_tanks_[k].alive                   ) continue;  // dead already
      if (killedThisTurn[k]                      ) continue;  // marked in 2a
      if (newPos[k] == oldPos[k]) continue;                 // didn’t move
      for (std::size_t j = 0; j < N; ++j) {
        if (j == k)                                             continue;
        if (!all_tanks_[j].alive                              ) continue;  // dead
        if (killedThisTurn[j]                                 ) continue;  // marked
        if (newPos[j] != oldPos[j]) continue;                   // j must be stationary
        if (newPos[k] == oldPos[j]) {
          // k moved into j’s square
          killedThisTurn[k] = killedThisTurn[j] = true;
          all_tanks_[k].alive = all_tanks_[j].alive = false;
          board_.setCell(oldPos[k].first, oldPos[k].second, CellContent::EMPTY);
          board_.setCell(oldPos[j].first, oldPos[j].second, CellContent::EMPTY);
        }
      }
    }

    // 2c) Multi-tank collisions at same destination: any cell with ≥2 movers → all die
    std::map<std::pair<int,int>, std::vector<std::size_t>> destMap;
    for (std::size_t k = 0; k < N; ++k) {
      if (!all_tanks_[k].alive 
       || killedThisTurn[k] 
       || newPos[k] == oldPos[k]) continue;
      destMap[newPos[k]].push_back(k);
    }
    for (auto const& [pos, vec] : destMap) {
      if (vec.size() > 1) {
        for (auto k : vec) {
          if (!all_tanks_[k].alive || killedThisTurn[k]) continue;
          killedThisTurn[k]   = true;
          all_tanks_[k].alive = false;
          // clear their old position
          board_.setCell(oldPos[k].first, oldPos[k].second, CellContent::EMPTY);
        }
      }
    }


    // 3) Now apply every non‐colliding move
    for (std::size_t k = 0; k < N; ++k) {
        if (!all_tanks_[k].alive)
            continue;

        auto [ox, oy] = oldPos[k];
        auto [nx, ny] = newPos[k];

        // stayed in place?
        if (nx == ox && ny == oy) {
            board_.setCell(ox, oy,
                all_tanks_[k].player_index == 1
                  ? CellContent::TANK1
                  : CellContent::TANK2
            );
            continue;
        }

        // illegal: wall
        if (board_.getCell(nx, ny).content == CellContent::WALL) {
            ignored[k] = true;
            board_.setCell(ox, oy,
                all_tanks_[k].player_index == 1
                  ? CellContent::TANK1
                  : CellContent::TANK2
            );
            continue;
        }

        // --- mutual shell‐tank destruction ---
        {
            bool collidedWithShell = false;
            for (size_t s = 0; s < shells_.size(); ++s) {
                if (shells_[s].x == nx && shells_[s].y == ny) {
                    // kill tank
                    all_tanks_[k].alive = false;
                    killedThisTurn[k]   = true;
                    // clear its old cell
                    board_.setCell(ox, oy, CellContent::EMPTY);
                    board_.setCell(nx, ny, CellContent::EMPTY);
                    // remove that shell
                    shells_.erase(shells_.begin() + s);
                    collidedWithShell = true;
                    break;
                }
            }
            if (collidedWithShell)
                continue;  // tank is dead, skip the rest
        }

        // mine → both die
        if (board_.getCell(nx, ny).content == CellContent::MINE) {
            killedThisTurn[k]   = true;
            all_tanks_[k].alive = false;
            board_.setCell(ox, oy, CellContent::EMPTY);
            board_.setCell(nx, ny, CellContent::EMPTY);
            continue;
        }

        // normal move
        board_.setCell(ox, oy, CellContent::EMPTY);
        all_tanks_[k].x = nx;
        all_tanks_[k].y = ny;
        board_.setCell(nx, ny,
            all_tanks_[k].player_index == 1
              ? CellContent::TANK1
              : CellContent::TANK2
        );
    }
}

void GameState::handleShooting(std::vector<bool>& ignored,
                               const std::vector<ActionRequest>& A)
{
    auto spawn = [&](TankState& ts){
        int dx=0,dy=0;
        switch(ts.direction) {
        case 0: dy=-1; break; case 1: dx=1;dy=-1; break;
        case 2: dx=1; break;  case 3: dx=1;dy=1; break;
        case 4: dy=1; break;  case 5: dx=-1;dy=1; break;
        case 6: dx=-1; break; case 7: dx=-1;dy=-1; break;
        }
        int sx=(ts.x+dx+board_.getWidth())%board_.getWidth();
        int sy=(ts.y+dy+board_.getHeight())%board_.getHeight();
        if (!handleShellMidStepCollision(sx,sy))
            shells_.push_back({sx,sy,ts.direction});
    };

    for (size_t k = 0; k < all_tanks_.size(); ++k) {
        auto& ts = all_tanks_[k];
        if (!ts.alive || A[k] != ActionRequest::Shoot) continue;

        // 1) still cooling down?
        if (ts.shootCooldown > 0) {
            ignored[k] = true;
            continue;
        }
        // 2) out of ammo?
        if (ts.shells_left == 0) {
            ignored[k] = true;
            continue;
        }
        // 3) fire!
        ts.shells_left--;
        ts.shootCooldown = 4;    // set 4‐turn cooldown
        spawn(ts);
    }
}
//------------------------------------------------------------------------------
// (8) updateShellsWithOverrunCheck:
//     move each shell in two unit-steps, checking for tank/wall at each step
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// (6.5) updateShellsWithOverrunCheck:
//     move each shell in two unit-steps, checking for wall/tank collisions
//     and also if two shells cross through each other.
void GameState::updateShellsWithOverrunCheck() {
    toRemove_.clear();
    positionMap_.clear();
    board_.clearShellMarks();

    const size_t S = shells_.size();
    // 1) snapshot old positions and deltas
    std::vector<std::pair<int,int>> oldPos(S);
    std::vector<std::pair<int,int>> delta(S);
    for (size_t i = 0; i < S; ++i) {
        oldPos[i] = { shells_[i].x, shells_[i].y };
        int dx = 0, dy = 0;
        switch (shells_[i].dir) {
          case 0:  dy = -1; break;
          case 1:  dx = +1; dy = -1; break;
          case 2:  dx = +1; break;
          case 3:  dx = +1; dy = +1; break;
          case 4:  dy = +1; break;
          case 5:  dx = -1; dy = +1; break;
          case 6:  dx = -1; break;
          case 7:  dx = -1; dy = -1; break;
        }
        delta[i] = {dx, dy};
    }

    // 2) perform two sub-steps simultaneously
    for (int step = 0; step < 2; ++step) {
        for (size_t i = 0; i < shells_.size(); ++i) {
            if (toRemove_.count(i)) continue;  // already dying

            // compute this shell's next position
            int nx = shells_[i].x + delta[i].first;
            int ny = shells_[i].y + delta[i].second;
            board_.wrapCoords(nx, ny);

            // 2a) crossing-paths check
            for (size_t j = 0; j < shells_.size(); ++j) {
                if (i == j || toRemove_.count(j)) continue;
                // j's old and would-be new pos
                auto [oxj, oyj] = oldPos[j];
                int nxj = oxj + delta[j].first;
                int nyj = oyj + delta[j].second;
                board_.wrapCoords(nxj, nyj);
                // if i’s new == j’s old AND j’s new == i’s old → cross
                if (nx == oxj && ny == oyj
                 && nxj == oldPos[i].first
                 && nyj == oldPos[i].second)
                {
                    toRemove_.insert(i);
                    toRemove_.insert(j);
                    break;
                }
            }
            if (toRemove_.count(i)) continue;

            // advance the shell
            shells_[i].x = nx;
            shells_[i].y = ny;

            // 2b) tank/wall mid-step collision
            if (handleShellMidStepCollision(nx, ny)) {
                toRemove_.insert(i);
                continue;
            }

            // 2c) record for same-cell collisions
            positionMap_[{nx, ny}].push_back(i);
        }
    }
}


void GameState::resolveShellCollisions() {
    // if two or more shells occupy the same cell, they all die
    for (auto const& entry : positionMap_) {
        const auto& idxs = entry.second;
        if (idxs.size() > 1) {
            for (auto idx : idxs) {
                toRemove_.insert(idx);
            }
        }
    }
}

void GameState::filterRemainingShells() {
    std::vector<Shell> remaining;
    remaining.reserve(shells_.size());
    for (std::size_t i = 0; i < shells_.size(); ++i) {
        // survivors are those not in toRemove_
        if (toRemove_.count(i) == 0) {
            // mark overlay
            auto& cell = board_.getCell(shells_[i].x, shells_[i].y);
            cell.hasShellOverlay = true;
            remaining.push_back(shells_[i]);
        }
    }
    shells_.swap(remaining);
}

//------------------------------------------------------------------------------
// (I) handleShellMidStepCollision: wall/tank logic at (x,y)
//------------------------------------------------------------------------------
bool GameState::handleShellMidStepCollision(int x, int y) {
    Cell& cell = board_.getCell(x, y);

    // 1) Wall?
    if (cell.content == CellContent::WALL) {
        cell.wallHits++;
        if (cell.wallHits >= 2) {
            cell.content = CellContent::EMPTY;
        }
        return true;
    }

    // 2) Tank?
    if (cell.content == CellContent::TANK1 || cell.content == CellContent::TANK2) {
        // find and kill the matching TankState
        int pid = (cell.content == CellContent::TANK1 ? 1 : 2);
        for (auto& ts : all_tanks_) {
            if (ts.alive && ts.player_index == pid && ts.x == x && ts.y == y) {
                ts.alive = false;
                break;
            }
        }
        cell.content = CellContent::EMPTY;
        return true;
    }

    // 3) Mine or empty: shells pass through mines, are only removed on tanks/walls
    return false;
}

void GameState::cleanupDestroyedEntities() {
    // nothing
}

void GameState::checkGameEndConditions() {
    int a1=0,a2=0;
    for (auto const& ts: all_tanks_) {
        if (ts.alive) (ts.player_index==1?++a1:++a2);
    }
    if (a1==0 && a2==0) {
        gameOver_=true; resultStr_="Tie, both players have zero tanks";
    }
    else if (a1==0) {
        gameOver_=true; resultStr_="Player 2 won with "+std::to_string(a2)+" tanks still alive";
    }
    else if (a2==0) {
        gameOver_=true; resultStr_="Player 1 won with "+std::to_string(a1)+" tanks still alive";
    }
    else if (currentStep_ + 1 >= max_steps_) {
    gameOver_ = true;
    resultStr_ = "Tie, reached max steps=" + std::to_string(max_steps_)+
                   ", player1 has "+std::to_string(a1)+
                   ", player2 has "+std::to_string(a2);
    }
}

//----------------------------------------------------------------------------  
const Board& GameState::getBoard() const {
    return board_;
}
