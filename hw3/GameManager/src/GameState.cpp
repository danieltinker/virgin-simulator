// GameState.cpp
#include "GameState.h"
#include "MySatelliteView.h"   // for GetBattleInfo handling
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>

using namespace GameManager_315634022;
namespace { constexpr std::size_t ZERO_SHELLS_TIE_STREAK = 40; }

// ——————————————————————————————————————————————————
// Professional Debug Logging System
// ——————————————————————————————————————————————————
namespace {
const char* actionToString(ActionRequest a) {
    switch (a) {
        case ActionRequest::MoveForward:    return "MoveForward";
        case ActionRequest::MoveBackward:   return "MoveBackward";
        case ActionRequest::RotateLeft90:   return "RotateLeft90";
        case ActionRequest::RotateRight90:  return "RotateRight90";
        case ActionRequest::RotateLeft45:   return "RotateLeft45";
        case ActionRequest::RotateRight45:  return "RotateRight45";
        case ActionRequest::Shoot:          return "Shoot";
        case ActionRequest::GetBattleInfo:  return "GetBattleInfo";
        case ActionRequest::DoNothing:      return "DoNothing";
    }
    return "Unknown";
}
}

static std::mutex g_debug_mutex;

#define DEBUG_PRINT(component, function, message, debug_flag) \
    do { if (debug_flag) { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cout << "[T" << std::this_thread::get_id() << "] [DEBUG] [" << component \
              << "] [" << function << "] " << message << std::endl; }} while(0)

#define INFO_PRINT(component, function, message) \
    do { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cout << "[T" << std::this_thread::get_id() << "] [INFO] [" << component \
              << "] [" << function << "] " << message << std::endl; } while(0)

#define WARN_PRINT(component, function, message) \
    do { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" << component \
              << "] [" << function << "] " << message << std::endl; } while(0)

#define ERROR_PRINT(component, function, message) \
    do { std::lock_guard<std::mutex> lock(g_debug_mutex); \
    std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" << component \
              << "] [" << function << "] " << message << std::endl; } while(0)


// ——————————————————————————————————————————————————
// Ctor / Dtor and simple getters (original behavior)
// ——————————————————————————————————————————————————
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

    // Verify algorithms valid
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
std::size_t GameState::getCurrentTurn() const { return currentStep_; }
const Board& GameState::getBoard() const { return board_; }


// === extracted helpers implementations ===
void GameState::logTurnStart(std::size_t N) {
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn",
        "Starting turn " + std::to_string(currentStep_ + 1) +
        " with " + std::to_string(N) + " tanks", verbose_);
    INFO_PRINT("ACTIONGATHER", "advanceOneTurn", "Gathering action requests from all tanks");
}

void GameState::gatherActionRequests(std::vector<ActionRequest>& actions,
                                     std::vector<bool>& ignored) {
    const size_t N = all_tanks_.size();
    for (size_t k = 0; k < N; ++k) {
        auto& ts  = all_tanks_[k];
        if (!ts.alive) continue;
        auto& alg = *all_tank_algorithms_[k];
        ActionRequest req = alg.getAction();
        DEBUG_PRINT("ACTIONGATHER", "advanceOneTurn",
            "Tank " + std::to_string(k) + " requested: " + actionToString(req), verbose_);

        if (req == ActionRequest::GetBattleInfo) {
            INFO_PRINT("BATTLEINFO", "advanceOneTurn",
                "Tank " + std::to_string(k) + " requested battle info");

            // Build visibility snapshot (like original)
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
            if (ts.y >= 0 && ts.y < static_cast<int>(rows_) &&
                ts.x >= 0 && ts.x < static_cast<int>(cols_)) {
                grid[ts.y][ts.x] = '%';
            }

            DEBUG_PRINT("BATTLEINFO", "advanceOneTurn",
                "Creating MySatelliteView for Tank " + std::to_string(k) +
                " at (" + std::to_string(ts.x) + "," + std::to_string(ts.y) + ")", verbose_);

            MySatelliteView view(grid, rows_, cols_, ts.x, ts.y);

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
            if (ts.player_index == 1) p1_.updateTankWithBattleInfo(alg, view);
            else                       p2_.updateTankWithBattleInfo(alg, view);

            DEBUG_PRINT("BATTLEINFO", "advanceOneTurn",
                "Tank " + std::to_string(k) + " battle info update completed", verbose_);
            actions[k] = ActionRequest::GetBattleInfo;
            ignored[k] = false;
        } else {
            actions[k] = req;
        }
    }
}

void GameState::executePhases(std::vector<ActionRequest>& actions,
                              std::vector<bool>& ignored,
                              std::vector<bool>& killed) {
    // ORIGINAL ORDER — do not change
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn",
        "Executing game phases for turn " + std::to_string(currentStep_ + 1), verbose_);
        
    // Phase 1: Apply rotations and handle mine collisions
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn", "Phase 1: Applying rotations and handling mine collisions", verbose_);
    applyTankRotations(actions);
    handleTankMineCollisions();
    updateTankCooldowns();
    confirmBackwardMoves(ignored, actions);
    updateShellsWithOverrunCheck();
    resolveShellCollisions();
    handleShooting(ignored, actions);
    updateTankPositionsOnBoard(ignored, killed, actions);
    filterRemainingShells();
    cleanupDestroyedEntities();
    checkGameEndConditions();
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn",
        "Game phases executed successfully for turn " + std::to_string(currentStep_ + 1), verbose_);
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
        if (ts.shootCooldown > 0) { ignored[k] = true; continue; }
        // 2) out of ammo?
        if (ts.shells_left == 0)  { ignored[k] = true; continue; }
        // 3) fire!
        ts.shells_left--;
        ts.shootCooldown = 4;    // fixed 4‐turn cooldown
        spawn(ts);
    }
}

std::string GameState::buildTurnLogString(const std::vector<ActionRequest>& logActions,
                                          const std::vector<bool>& ignored,
                                          const std::vector<bool>& killed) const {
    (void)killed; // suppress unused parameter warning (no behavior change)
    const size_t N = all_tanks_.size();
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
            if (act == ActionRequest::DoNothing) oss << "killed";
            else oss << name << " (killed)";
        }
        if (k + 1 < N) oss << ", ";
    }
    return oss.str();
}

// std::string GameState::renderRow(std::size_t r) const {
//     std::ostringstream line;
//     for (size_t c = 0; c < cols_; ++c) {
//         bool shellHere = false;
//         for (const auto& sh : shells_) {
//             if (sh.x == int(c) && sh.y == int(r)) { shellHere = true; break; }
//         }
//         if (shellHere) { line << '*'; continue; }
//         const auto& cell = board_.getCell(int(c), int(r));
//         switch (cell.content) {
//             case CellContent::EMPTY: line << '_'; break;
//             case CellContent::WALL:  line << '#'; break;
//             case CellContent::MINE:  line << '@'; break;
//             case CellContent::TANK1:
//             case CellContent::TANK2: {
//                 line << tankArrowAt(r, c);
//                 break;
//             }
//             default: line << '?'; break;
//         }
//     }
//     return line.str();
// }

std::string GameState::renderRow(std::size_t r) const {
    std::ostringstream line;
    for (size_t c = 0; c < cols_; ++c) {
        bool shellHere = false;
        for (const auto& sh : shells_) {
            if (sh.x == int(c) && sh.y == int(r)) { shellHere = true; break; }
        }
        if (shellHere) { line << '*'; continue; }

        const auto& cell = board_.getCell(int(c), int(r));
        switch (cell.content) {
            case CellContent::EMPTY: line << ' '; break;     // <— space, not underscore
            case CellContent::WALL:  line << '#'; break;
            case CellContent::MINE:  line << '@'; break;
            case CellContent::TANK1:
            case CellContent::TANK2: {
                line << tankArrowAt(r, c);
                break;
            }
            default: line << ' '; break;
        }
    }
    return line.str();
}


std::string GameState::tankArrowAt(std::size_t r, std::size_t c) const {
    int pid = 0, dir = 0;
    for (auto const& ts : all_tanks_) {
        if (ts.alive && ts.x == int(c) && ts.y == int(r)) {
            pid = ts.player_index;
            dir = ts.direction;
            break;
        }
    }
    const char* arr = directionToArrow(dir);
    std::ostringstream out;
    out << (pid == 1 ? "\033[31m" : "\033[34m") << arr << "\033[0m";
    return out.str();
}

std::vector<std::pair<int,int>> GameState::computeShellDeltas() const {
    std::vector<std::pair<int,int>> delta(shells_.size());
    for (size_t i = 0; i < shells_.size(); ++i) {
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
    return delta;
}

void GameState::processShellHalfStep(const std::vector<std::pair<int,int>>& delta,
                                     const std::vector<std::pair<int,int>>& oldPos,
                                     int /*step*/) {
    for (size_t i = 0; i < shells_.size(); ++i) {
        if (toRemove_.count(i)) continue;
        int nx = shells_[i].x + delta[i].first;
        int ny = shells_[i].y + delta[i].second;
        board_.wrapCoords(nx, ny);

        for (size_t j = 0; j < shells_.size(); ++j) {
            if (i == j || toRemove_.count(j)) continue;
            auto [oxj, oyj] = oldPos[j];
            int nxj = oxj + delta[j].first;
            int nyj = oyj + delta[j].second;
            board_.wrapCoords(nxj, nyj);
            if (nx == oxj && ny == oyj && nxj == oldPos[i].first && nyj == oldPos[i].second) {
                toRemove_.insert(i);
                toRemove_.insert(j);
            }
        }
        if (toRemove_.count(i)) continue;
        shells_[i].x = nx;
        shells_[i].y = ny;
        if (handleShellMidStepCollision(nx, ny)) {
            toRemove_.insert(i);
            continue;
        }
        positionMap_[{nx, ny}].push_back(i);
    }
}

// give nice arrows for 8 directions
const char* GameState::directionToArrow(int dir) {
    static const char* arr[8] = {"↑","↗","→","↘","↓","↙","←","↖"};
    return arr[dir&7];
}


std::string GameState::advanceOneTurn() {
    if (gameOver_) {
        DEBUG_PRINT("GAMELOOP", "advanceOneTurn", "Game already over, returning empty string", verbose_);
        return "";
    }

    const size_t N = all_tanks_.size();
    std::vector<ActionRequest> actions(N, ActionRequest::DoNothing);
    std::vector<bool> ignored(N, false), killed(N, false);

    logTurnStart(N);
    gatherActionRequests(actions, ignored);

    // Snapshot original actions for logging
    std::vector<ActionRequest> logActions = actions;

    // Execute the per-turn phases (original order includes checkGameEndConditions)
    executePhases(actions, ignored, killed);

    // 🔧 Restore: advance step counter and drop shoot cooldowns AFTER phases
    ++currentStep_;
    for (auto& ts : all_tanks_) {
        if (ts.shootCooldown > 0) --ts.shootCooldown;
    }

    INFO_PRINT("TURNREPORT", "advanceOneTurn", "=== Current Board State ===");
    printBoard();

    // Build turn result string
    std::string result = buildTurnLogString(logActions, ignored, killed);
    DEBUG_PRINT("GAMELOOP", "advanceOneTurn", "Turn log: " + result, verbose_);
    return result;
}



void GameState::updateShellsWithOverrunCheck() {
    toRemove_.clear();
    positionMap_.clear();
    board_.clearShellMarks();

    const size_t S = shells_.size();
    std::vector<std::pair<int,int>> oldPos(S);
    for (size_t i = 0; i < S; ++i) oldPos[i] = { shells_[i].x, shells_[i].y };

    const auto delta = computeShellDeltas();
    for (int step = 0; step < 2; ++step) processShellHalfStep(delta, oldPos, step);
}

void GameState::printBoard() const {
    // overlay shells & tanks dynamically in renderRow()
    for (size_t r = 0; r < rows_; ++r) {
        std::cout << renderRow(r) << "\n";
    }
    std::cout << std::endl;
}


// ——————————————————————————————————————————————————
// Original mechanics broken out as separate methods
// ——————————————————————————————————————————————————
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
    // unused in original
}

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

// move + collisions (original logic)
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
          board_.setCell(oldPos[i].first, oldPos[i].second, CellContent::EMPTY);
          board_.setCell(oldPos[j].first, oldPos[j].second, CellContent::EMPTY);
        }
      }
    }

    // 2b) Moving-into-stationary
    for (std::size_t k = 0; k < N; ++k) {
      if (!all_tanks_[k].alive) continue;
      if (killedThisTurn[k])    continue;
      if (newPos[k] == oldPos[k]) continue;
      for (std::size_t j = 0; j < N; ++j) {
        if (j == k) continue;
        if (!all_tanks_[j].alive) continue;
        if (killedThisTurn[j])    continue;
        if (newPos[j] != oldPos[j]) continue;
        if (newPos[k] == oldPos[j]) {
          killedThisTurn[k] = killedThisTurn[j] = true;
          all_tanks_[k].alive = all_tanks_[j].alive = false;
          board_.setCell(oldPos[k].first, oldPos[k].second, CellContent::EMPTY);
          board_.setCell(oldPos[j].first, oldPos[j].second, CellContent::EMPTY);
        }
      }
    }

    // 2c) Multi-tank collisions at same destination
    std::map<std::pair<int,int>, std::vector<std::size_t>> destMap;
    for (std::size_t k = 0; k < N; ++k) {
      if (!all_tanks_[k].alive || killedThisTurn[k] || newPos[k] == oldPos[k]) continue;
      destMap[newPos[k]].push_back(k);
    }
    for (auto const& [pos, vec] : destMap) {
      if (vec.size() > 1) {
        for (auto k : vec) {
          if (!all_tanks_[k].alive || killedThisTurn[k]) continue;
          killedThisTurn[k]   = true;
          all_tanks_[k].alive = false;
          board_.setCell(oldPos[k].first, oldPos[k].second, CellContent::EMPTY);
        }
      }
    }

    // 3) Apply non-colliding moves
    for (std::size_t k = 0; k < N; ++k) {
        if (!all_tanks_[k].alive) continue;

        auto [ox, oy] = oldPos[k];
        auto [nx, ny] = newPos[k];

        // stayed in place?
        if (nx == ox && ny == oy) {
            board_.setCell(ox, oy,
                all_tanks_[k].player_index == 1 ? CellContent::TANK1 : CellContent::TANK2);
            continue;
        }

        // illegal: wall
        if (board_.getCell(nx, ny).content == CellContent::WALL) {
            ignored[k] = true;
            board_.setCell(ox, oy,
                all_tanks_[k].player_index == 1 ? CellContent::TANK1 : CellContent::TANK2);
            continue;
        }

        // mutual shell‐tank destruction
        {
            bool collidedWithShell = false;
            for (size_t s = 0; s < shells_.size(); ++s) {
                if (shells_[s].x == nx && shells_[s].y == ny) {
                    all_tanks_[k].alive = false;
                    killedThisTurn[k]   = true;
                    board_.setCell(ox, oy, CellContent::EMPTY);
                    board_.setCell(nx, ny, CellContent::EMPTY);
                    shells_.erase(shells_.begin() + s);
                    collidedWithShell = true;
                    break;
                }
            }
            if (collidedWithShell) continue;
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
            all_tanks_[k].player_index == 1 ? CellContent::TANK1 : CellContent::TANK2);
    }
}

void GameState::resolveShellCollisions() {
    // if two or more shells occupy the same cell, they all die
    for (auto const& entry : positionMap_) {
        const auto& idxs = entry.second;
        if (idxs.size() > 1) {
            for (auto idx : idxs) toRemove_.insert(idx);
        }
    }
}

void GameState::filterRemainingShells() {
    std::vector<Shell> remaining;
    remaining.reserve(shells_.size());
    for (std::size_t i = 0; i < shells_.size(); ++i) {
        if (toRemove_.count(i) == 0) {
            auto& cell = board_.getCell(shells_[i].x, shells_[i].y);
            cell.hasShellOverlay = true;
            remaining.push_back(shells_[i]);
        }
    }
    shells_.swap(remaining);
}

bool GameState::handleShellMidStepCollision(int x, int y) {
    Cell& cell = board_.getCell(x, y);

    // wall?
    if (cell.content == CellContent::WALL) {
        cell.wallHits++;
        if (cell.wallHits >= 2) cell.content = CellContent::EMPTY;
        return true;
    }

    // tank?
    if (cell.content == CellContent::TANK1 || cell.content == CellContent::TANK2) {
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

    // mine or empty → pass
    return false;
}

void GameState::cleanupDestroyedEntities() {
    // nothing in original
}

void GameState::checkGameEndConditions() {
    int a1=0,a2=0;
    for (auto const& ts: all_tanks_) {
        if (ts.alive) (ts.player_index==1?++a1:++a2);
    }

    // Win / both-dead checks first (original precedence)
    if (a1==0 && a2==0) {
        gameOver_=true; resultStr_="Tie, both players have zero tanks";
        return;
    }
    if (a1==0) {
        gameOver_=true; resultStr_="Player 2 won with "+std::to_string(a2)+" tanks still alive";
        return;
    }
    if (a2==0) {
        gameOver_=true; resultStr_="Player 1 won with "+std::to_string(a1)+" tanks still alive";
        return;
    }

    // NEW: track consecutive turns where BOTH players have zero shells total
    std::size_t p1Shells=0, p2Shells=0;
    for (auto const& ts: all_tanks_) {
        if (!ts.alive) continue;
        if (ts.player_index==1) p1Shells += ts.shells_left;
        else                    p2Shells += ts.shells_left;
    }
    if (p1Shells==0 && p2Shells==0) ++zeroShellsStreak_;
    else                             zeroShellsStreak_ = 0;

    if (zeroShellsStreak_ >= ZERO_SHELLS_TIE_STREAK) {
        gameOver_ = true;
        // EXACT wording required
        resultStr_ = "Tie, both players have zero shells for " +
                     std::to_string(ZERO_SHELLS_TIE_STREAK) + " steps";
        return;
    }

    // Max-steps tie (same wording you requested)
    if (currentStep_ + 1 >= max_steps_) {
        gameOver_ = true;
        resultStr_ = "Tie, reached max steps = " + std::to_string(max_steps_) +
                     ", player 1 has " + std::to_string(a1) +
                     " tanks, player 2 has " + std::to_string(a2) + " tanks";
        return;
    }
}
