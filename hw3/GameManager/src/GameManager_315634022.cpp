#include "GameManager_315634022.h"
#include <GameManagerRegistration.h>
#include <SatelliteView.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <fstream>
#include <iostream>
#include <vector>

using namespace GameManager_315634022;

// ——————————————————————————————————————————————————————
// Thread-safe console logging
// ——————————————————————————————————————————————————————
static std::mutex g_debug_mutex;

#define DEBUG_PRINT(component, function, message, debug_flag) \
    do { \
        if (debug_flag) { \
            std::lock_guard<std::mutex> lock(g_debug_mutex); \
            std::cout << "[T" << std::this_thread::get_id() << "] [DEBUG] [" \
                      << component << "] [" << function << "] " << message << std::endl; \
        } \
    } while(0)

#define INFO_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cout << "[T" << std::this_thread::get_id() << "] [INFO] [" \
                  << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

#define WARN_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cerr << "[T" << std::this_thread::get_id() << "] [WARN] [" \
                  << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

#define ERROR_PRINT(component, function, message) \
    do { \
        std::lock_guard<std::mutex> lock(g_debug_mutex); \
        std::cerr << "[T" << std::this_thread::get_id() << "] [ERROR] [" \
                  << component << "] [" << function << "] " << message << std::endl; \
    } while(0)

namespace {

// ---------- small utils ----------
std::string baseName(const std::string& path) {
    std::string name = path;
    size_t last_slash = name.find_last_of("/\\");
    if (last_slash != std::string::npos) name = name.substr(last_slash + 1);
    size_t last_dot = name.find_last_of('.');
    if (last_dot != std::string::npos) name = name.substr(0, last_dot);
    return name;
}

std::string nowStamp() {
    auto t = std::time(nullptr);
    std::ostringstream ts;
    ts << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ts.str();
}

std::string buildLogFilename(const std::string& map,
                             const std::string& algo1,
                             const std::string& algo2) {
    std::ostringstream fn;
    fn << "log_" << baseName(map) << "_" << baseName(algo1)
       << "_vs_" << baseName(algo2) << "_" << nowStamp() << ".txt";
    return fn.str();
}

// ---------- board -> char ----------
inline char renderCell(const Cell& c) {
    if (c.hasShellOverlay) return '*';  // overlay wins
    switch (c.content) {
        case CellContent::WALL:  return '#';
        case CellContent::MINE:  return '@';
        case CellContent::TANK1: return '1';
        case CellContent::TANK2: return '2';
        default:                  return ' ';
    }
}

// Owns a snapshot of the final board; no dangling references.
class OwningSatelliteView : public ::SatelliteView {
public:
    explicit OwningSatelliteView(const Board& B)
        : rows_(B.getRows()), cols_(B.getCols()), grid_(rows_ * cols_, ' ')
    {
        for (size_t y = 0; y < rows_; ++y) {
            for (size_t x = 0; x < cols_; ++x) {
                const auto& cell = B.getCell(static_cast<int>(x), static_cast<int>(y));
                grid_[y * cols_ + x] = renderCell(cell);
            }
        }
    }

    char getObjectAt(size_t x, size_t y) const override {
        if (x >= cols_ || y >= rows_) return '&';
        return grid_[y * cols_ + x];
    }

private:
    size_t rows_, cols_;
    std::vector<char> grid_;
};

// ---------- debug helpers ----------
inline const char* reasonToString(GameResult::Reason r) {
    switch (r) {
        case GameResult::ALL_TANKS_DEAD: return "ALL_TANKS_DEAD";
        case GameResult::ZERO_SHELLS:    return "ZERO_SHELLS";
        case GameResult::MAX_STEPS:      return "MAX_STEPS";
        default:                         return "UNKNOWN";
    }
}

inline std::string summarizeGameResult(const GameResult& gr, const Board& B) {
    auto p1 = (gr.remaining_tanks.size() > 0 ? gr.remaining_tanks[0] : 0);
    auto p2 = (gr.remaining_tanks.size() > 1 ? gr.remaining_tanks[1] : 0);

    const auto rows = B.getRows();
    const auto cols = B.getCols();
    char corner = ' ';
    if (rows > 0 && cols > 0) {
        corner = renderCell(B.getCell(0, 0));
    }

    std::ostringstream os;
    os << "GameResult { "
       << "winner=" << gr.winner
       << ", reason=" << reasonToString(gr.reason)
       << ", rounds=" << gr.rounds
       << ", remaining_tanks={p1:" << p1 << ", p2:" << p2 << "}"
       << ", board=" << cols << "x" << rows
       << ", corner00='" << corner << "'"
       << ", gameState=" << (gr.gameState ? "present" : "null")
       << " }";
    return os.str();
}

} // namespace

// ——————————————————————————————————————————————————————
// MyGameManager_315634022
// ——————————————————————————————————————————————————————
MyGameManager_315634022::MyGameManager_315634022(bool verbose)
  : verbose_(verbose), state_(nullptr)
{}

GameResult MyGameManager_315634022::run(
    size_t map_width,
    size_t map_height,
    const SatelliteView& map,
    string map_name,
    size_t max_steps,
    size_t num_shells,
    Player& player1,
    string name1,
    Player& player2,
    string name2,
    TankAlgorithmFactory player1_tank_algo_factory,
    TankAlgorithmFactory player2_tank_algo_factory
) {
    INFO_PRINT("GAMEMANAGER", "run", "GameManager_315634022 starting game execution");

    prepareLog(map_name, name1, name2);

    initializeGame(map_width, map_height, map, map_name,
                   max_steps, num_shells,
                   player1, name1, player2, name2,
                   player1_tank_algo_factory, player2_tank_algo_factory);


    // --- Early termination guard (before any player interaction) ---
    {
        const Board& B = state_->getBoard();
        std::size_t p1 = 0, p2 = 0;
        for (std::size_t y = 0; y < B.getRows(); ++y) {
            for (std::size_t x = 0; x < B.getCols(); ++x) {
                const auto c = B.getCell(static_cast<int>(x), static_cast<int>(y)).content;
                if (c == CellContent::TANK1)      ++p1;
                else if (c == CellContent::TANK2) ++p2;
            }
        }
        if (p1 == 0 || p2 == 0) {
            INFO_PRINT("GAMEEND", "run",
                "Early termination BEFORE gameLoop: p1=" + std::to_string(p1) +
                ", p2=" + std::to_string(p2) +
                " -> reason=ALL_TANKS_DEAD, rounds=0");
            // Reuse existing finalize(): winner/reason/rounds derived consistently.
            return finalize();
        }
    }


    gameLoop();

    return finalize();
}

void MyGameManager_315634022::prepareLog(const std::string& map_name,
                                         const std::string& algo1_name,
                                         const std::string& algo2_name) {
    if (!verbose_) return;
    const std::string log_filename = buildLogFilename(map_name, algo1_name, algo2_name);
    log_file_.open(log_filename);
    if (!log_file_.is_open()) {
        ERROR_PRINT("LOGMANAGER", "prepareLog", "Failed to open log file: " + log_filename);
    }
}

void MyGameManager_315634022::initializeGame(
    size_t width,
    size_t height,
    const SatelliteView& satellite_view,
    const std::string& map_name,
    size_t max_steps,
    size_t num_shells,
    Player& p1,
    const std::string& name1,
    Player& p2,
    const std::string& name2,
    TankAlgorithmFactory f1,
    TankAlgorithmFactory f2
) {
    INFO_PRINT("GAMEMANAGER", "initializeGame", "Initializing game with provided parameters");

    // Board(rows, cols) — pass (height, width)
    board_ = Board(height, width);
    board_.loadFromSatelliteView(satellite_view);

    state_ = std::make_unique<GameState>(
        std::move(board_), map_name, max_steps, num_shells,
        p1, name1, p2, name2, f1, f2, verbose_
    );

    INFO_PRINT("GAMEMANAGER", "initializeGame", "GameState created successfully");
}

void MyGameManager_315634022::gameLoop() {
    INFO_PRINT("GAMEMANAGER", "gameLoop", "Entering game loop");
    while (!state_->isGameOver()) {
        std::string decisions = state_->advanceOneTurn();   // increments internally
        if (verbose_ && log_file_.is_open()) {
            log_file_ << decisions << "\n";
            log_file_.flush();
        }
    }
    INFO_PRINT("GAMEMANAGER", "gameLoop", "Game loop completed");
}

GameResult MyGameManager_315634022::finalize() {
    DEBUG_PRINT("GAMEMANAGER", "finalize", "Finalizing game result", verbose_);

    GameResult gr{};

    const Board& B = state_->getBoard();

    // Snapshot so result.gameState stays valid after we return
    gr.gameState = std::make_unique<OwningSatelliteView>(B);

    // Recount remaining tanks from the final board (source of truth)
    size_t p1 = 0, p2 = 0;
    for (size_t y = 0; y < B.getRows(); ++y) {
        for (size_t x = 0; x < B.getCols(); ++x) {
            const auto c = B.getCell(static_cast<int>(x), static_cast<int>(y)).content;
            if (c == CellContent::TANK1) ++p1;
            else if (c == CellContent::TANK2) ++p2;
        }
    }
    gr.remaining_tanks = { p1, p2 };

    // Winner & reason
    if (p1 == 0 && p2 == 0) {
        gr.winner = 0;
        gr.reason = GameResult::ALL_TANKS_DEAD;
    } else if (p1 == 0) {
        gr.winner = 2;
        gr.reason = GameResult::ALL_TANKS_DEAD;
    } else if (p2 == 0) {
        gr.winner = 1;
        gr.reason = GameResult::ALL_TANKS_DEAD;
    } else {
        gr.winner = 0;
        const std::string finalLine = state_->getResultString();
        gr.reason = (finalLine.find("zero shells") != std::string::npos)
                        ? GameResult::ZERO_SHELLS
                        : GameResult::MAX_STEPS;
    }

    // Total rounds taken (turns are already incremented at end of each turn)
    gr.rounds = state_->getCurrentTurn();

    // Write the exact final line to the plain log (no headers/footers)
        // Write the exact final line to the plain log (no headers/footers).
    // On early termination (no P1/P2 at start) resultStr_ may be empty because
    // checkGameEndConditions() never ran; synthesize the canonical line here.
    if (verbose_ && log_file_.is_open()) {
        std::string finalLine = state_->getResultString();
        if (finalLine.empty()) {
            if (p1 == 0 && p2 == 0) {
                finalLine = "Tie, both players have zero tanks";
            } else if (p1 == 0) {
                finalLine = "Player 2 won with " + std::to_string(p2) + " tanks still alive";
            } else if (p2 == 0) {
                finalLine = "Player 1 won with " + std::to_string(p1) + " tanks still alive";
            }
        }
        if (!finalLine.empty()) {
           log_file_ << finalLine << "\n";
            log_file_.flush();
        }
        log_file_.close();
    }
    // Detailed one-liner summary of the result for debugging
    DEBUG_PRINT("GAMEMANAGER", "finalize", summarizeGameResult(gr, B), verbose_);

    return gr;
}

// Keep registration
REGISTER_GAME_MANAGER(MyGameManager_315634022)
