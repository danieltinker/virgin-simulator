
// GameState.cpp
#include "GameState.h"
#include <iostream>
#include <sstream>

using namespace GameManager_315634022;
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
    // --- Initialize tanks from board ---
    rows_ = board_.getRows();
    cols_ = board_.getCols();
    nextTankIndex_[1] = nextTankIndex_[2] = 0;
    tankIdMap_.assign(3, std::vector<std::size_t>(rows_*cols_, SIZE_MAX));

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
            }
        }
    }

    // // --- Create per‐tank algorithms ---
    // all_tank_algorithms_.clear();
    // for (auto& ts : all_tanks_) {
    //     if (ts.player_index == 1)
    //         all_tank_algorithms_.push_back(algoFactory1_(ts.player_index, ts.tank_index));
    //     else
    //         all_tank_algorithms_.push_back(algoFactory2_(ts.player_index, ts.tank_index));
    // }

    

// --- Create per‐tank algorithms ---
all_tank_algorithms_.clear();
std::cout << "=== ALGORITHM CREATION DEBUG ===" << std::endl;
std::cout << "Creating algorithms for " << all_tanks_.size() << " tanks" << std::endl;
std::cout << "Factory1 pointer: " << &algoFactory1_ << std::endl;
std::cout << "Factory2 pointer: " << &algoFactory2_ << std::endl;

for (size_t i = 0; i < all_tanks_.size(); ++i) {
    auto& ts = all_tanks_[i];
    std::cout << "\n--- Tank " << i << " ---" << std::endl;
    std::cout << "Player: " << ts.player_index << ", Tank index: " << ts.tank_index << std::endl;
    std::cout << "Tank position: (" << ts.x << ", " << ts.y << ")" << std::endl;
    std::cout << "Tank alive: " << (ts.alive ? "yes" : "no") << std::endl;
    
    std::unique_ptr<TankAlgorithm> algo;
    try {
        if (ts.player_index == 1) {
            std::cout << "Calling factory1 for player 1..." << std::endl;
            algo = algoFactory1_(ts.player_index, ts.tank_index);
            std::cout << "Factory1 returned: " << algo.get() << std::endl;
        } else {
            std::cout << "Calling factory2 for player 2..." << std::endl;
            algo = algoFactory2_(ts.player_index, ts.tank_index);
            std::cout << "Factory2 returned: " << algo.get() << std::endl;
        }
        
        if (algo) {
            std::cout << "Algorithm created successfully!" << std::endl;
            std::cout << "Algorithm pointer: " << algo.get() << std::endl;
            // std::cout << "Algorithm type info: " << typeid(*algo).name() << std::endl;
            
            // Test if we can call a basic method
            try {
                std::cout << "Testing algorithm with getAction()..." << std::endl;
                ActionRequest test_action = algo->getAction();
                std::cout << "Algorithm getAction() succeeded, returned: " << static_cast<int>(test_action) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "ERROR: Algorithm getAction() threw exception: " << e.what() << std::endl;
            } catch (...) {
                std::cout << "ERROR: Algorithm getAction() threw unknown exception" << std::endl;
            }
            
            all_tank_algorithms_.push_back(std::move(algo));
        } else {
            std::cout << "ERROR: Factory returned null algorithm!" << std::endl;
            throw std::runtime_error("Factory returned null algorithm");
        }
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception during algorithm creation: " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cout << "ERROR: Unknown exception during algorithm creation" << std::endl;
        throw;
    }
}

std::cout << "\n=== ALGORITHM CREATION COMPLETE ===" << std::endl;
std::cout << "Total algorithms created: " << all_tank_algorithms_.size() << std::endl;

// Verify all algorithms are still valid
for (size_t i = 0; i < all_tank_algorithms_.size(); ++i) {
    std::cout << "Algorithm " << i << " pointer: " << all_tank_algorithms_[i].get() << std::endl;
}
std::cout << "=== END ALGORITHM CREATION DEBUG ===" << std::endl;


    // size_t count1 = 0, count2 = 0;
    // for (auto& ts : all_tanks_) {
    //     if (ts.player_index == 1) ++count1;
    //     else if (ts.player_index == 2) ++count2;
    // }
    // std::cout << "[DEBUG] Spawned tanks — player1=" << count1 
    //         << ", player2=" << count2 << "\n";
    // for (auto& ts : all_tanks_) {
    //     std::cout << "[DEBUG]  Tank(" << ts.player_index << "," << ts.tank_index<< ") at (" << ts.x << "," << ts.y << ")\n";
    // }

    // Shells & end‐state
    shells_.clear();
    toRemove_.clear();
    positionMap_.clear();
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
      default:                                    return "DoNothing";
    }
}


//------------------------------------------------------------------------------
std::string GameState::advanceOneTurn() {
    if (gameOver_) return "";

    const size_t N = all_tanks_.size();
    std::vector<ActionRequest> actions(N, ActionRequest::DoNothing);
    std::vector<bool> ignored(N,false), killed(N,false);

    // if (verbose_) {
    // std::cout << "[DEBUG] advanceOneTurn called — step=" << currentStep_ + 1
        //   << ", board=" << cols_ << "x" << rows_
        //   << ", num_shells=" << num_shells_ << "\n";
    // }      
     // 1) Gather raw requests
    for (size_t k = 0; k < N; ++k) {
        auto& ts  = all_tanks_[k];
        auto& alg = *all_tank_algorithms_[k];
        if (!ts.alive) continue;

        ActionRequest req = alg.getAction();
if (req == ActionRequest::GetBattleInfo) {
    std::cout <<"GameState Tank requested GetBattleInfo" << std::endl;
    // build a visibility snapshot
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
    // mark the querying tank's position specially
    if (ts.y >= 0 && ts.y < static_cast<int>(rows_) && 
        ts.x >= 0 && ts.x < static_cast<int>(cols_)) {
        grid[ts.y][ts.x] = '%';
    }
    
    std::cout <<"GameState Constructing SatelliteView " << std::endl;
    std::cout <<"Grid size: " << rows_ << "x" << cols_ << std::endl;
    std::cout <<"Tank position: (" << ts.x << ", " << ts.y << ")" << std::endl;
    
    // Print the grid we're about to pass
    std::cout << "Grid contents:" << std::endl;
    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            std::cout << grid[r][c];
        }
        std::cout << std::endl;
    }
    
    MySatelliteView view(grid, rows_, cols_, ts.x, ts.y);
    
    // Test our view before passing it
    std::cout << "Testing MySatelliteView:" << std::endl;
    std::cout << "Rows: " << view.getRows() << ", Cols: " << view.getCols() << std::endl;
    std::cout << "Tank: (" << view.getTankX() << ", " << view.getTankY() << ")" << std::endl;
    
    char** test_grid = view.getGrid();
    if (test_grid != nullptr) {
        std::cout << "Grid pointer is valid" << std::endl;
        // Test first row
        if (test_grid[0] != nullptr) {
            std::cout << "First row pointer is valid, first char: '" << test_grid[0][0] << "'" << std::endl;
        } else {
            std::cout << "ERROR: First row pointer is null!" << std::endl;
        }
    } else {
        std::cout << "ERROR: Grid pointer is null!" << std::endl;
    }
    
    std::cout << "About to call updateTankWithBattleInfo..." << std::endl;
    
    if (ts.player_index == 1) {
        p1_.updateTankWithBattleInfo(alg, view);
    } else {
        p2_.updateTankWithBattleInfo(alg, view);
    }
    std::cout <<"GameState Tank Was Updated with Battle Info" << std::endl;
    actions[k] = ActionRequest::GetBattleInfo;
}
        else {
            actions[k] = req;
        }
    }
// ─── Just after “Gather raw requests” and before any rotations ─────────────────
// 1) Snapshot the original requests for logging
std::vector<ActionRequest> logActions = actions;

// 2) Backward‐delay logic (2 turns idle, 3rd turn executes)
for (size_t k = 0; k < N; ++k) {
    auto& ts   = all_tanks_[k];
    auto  orig = logActions[k];

    // (A) Mid‐delay from a previous MoveBackward?
    if (ts.backwardDelayCounter > 0) {
        --ts.backwardDelayCounter;
        if (ts.backwardDelayCounter == 0) {
            // 3rd turn → actually move backward
            ts.lastActionBackwardExecuted = true;
            actions[k] = ActionRequest::MoveBackward;
            ignored[k] = true;
        } else {
            // still in delay → only forward/info allowed
            if (orig == ActionRequest::MoveForward) {
                // cancel the delay
                ts.backwardDelayCounter       = 0;
                ts.lastActionBackwardExecuted = false;
                actions[k] = ActionRequest::DoNothing;
                ignored[k] = false;
            }
            else if (orig == ActionRequest::GetBattleInfo) {
                actions[k] = ActionRequest::GetBattleInfo;
                ignored[k] = false;
            }
            else {
                actions[k] = ActionRequest::DoNothing;
                ignored[k] = true;
            }
        }
        continue;
    }

    // (B) No pending delay: new MoveBackward request?
    if (orig == ActionRequest::MoveBackward) {
        // schedule exactly 2 idle turns then exec on the 3rd
        ts.backwardDelayCounter       = ts.lastActionBackwardExecuted ? 1 : 3;
        ts.lastActionBackwardExecuted = false;

        // do nothing this turn (exec will happen when counter→0)
        actions[k] = ActionRequest::DoNothing;
        ignored[k] = false;
        continue;
    }

    // (C) All other actions clear the “just did backward” flag
    ts.lastActionBackwardExecuted = false;
    // actions[k] remains orig; ignored[k] stays false
}


    // 2) Rotations
    applyTankRotations(actions);
    

    // 3) Mines
    handleTankMineCollisions();

    // 4) Cooldowns (unused)
    updateTankCooldowns();

    // 5) Backward legality check
    confirmBackwardMoves(ignored, actions);

    // 6) Shell movement & collisions
    updateShellsWithOverrunCheck();
    resolveShellCollisions();

    // 7) Shooting
    handleShooting(ignored, actions);

    // 8) Tank movement, collisions
    updateTankPositionsOnBoard(ignored, killed, actions);

    // 9) Cleanup shells & entities
    filterRemainingShells();
    cleanupDestroyedEntities();

    // 10) End‐of‐game
    checkGameEndConditions();

    // 11) Advance step & drop shoot cooldowns
    ++currentStep_;
    for (auto& ts : all_tanks_)
        if (ts.shootCooldown > 0) --ts.shootCooldown;


    // ─── Console print of each tank's decision and status ────────────────────────
    std::cout << "=== Decisions ===\n"<<std::endl;
    for (size_t k = 0; k < N; ++k) {
        const char* actName = actionToString(logActions[k]);
        bool wasIgnored     = ignored[k]
        && logActions[k] != ActionRequest::GetBattleInfo;
        std::cout << "  Tank[" << k << "]: "
        << actName
        << (wasIgnored ? " (ignored)" : " (accepted)")
        << "\n";
    }
    std::cout << std::endl;
    std::cout << "=== Board State: ===\n" << std::endl;
    printBoard();
    // ─── Logging: use the ORIGINAL requests ────────────────────────────────────
    std::cout <<"Logging File of GM STATE "<<std::endl;

    std::ostringstream oss;
    for (size_t k = 0; k < N; ++k) {
        const auto act = logActions[k];
        const char* name = "DoNothing";
        switch (act) {
            case ActionRequest::MoveForward:    name = "MoveForward";   break;
            case ActionRequest::MoveBackward:   name = "MoveBackward";  break;
            case ActionRequest::RotateLeft90:   name = "RotateLeft90";  break;
            case ActionRequest::RotateRight90:  name = "RotateRight90"; break;
            case ActionRequest::RotateLeft45:   name = "RotateLeft45";  break;
            case ActionRequest::RotateRight45:  name = "RotateRight45"; break;
            case ActionRequest::Shoot:          name = "Shoot";         break;
            case ActionRequest::GetBattleInfo:  name = "GetBattleInfo"; break;
            default:                                                    break;
        }
        
        if (all_tanks_[k].alive) {
            oss << name;
            if (ignored[k] && act != ActionRequest::GetBattleInfo)
            {
                oss << " (ignored)";
            }
        } else {
            // tank died this turn
            if (act == ActionRequest::DoNothing)
            oss << "killed";
        else
        oss << name << " (killed)";
}
if (k + 1 < N) oss << ", ";
}
    return oss.str();
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
