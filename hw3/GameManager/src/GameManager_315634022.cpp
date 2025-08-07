#include "GameManager_315634022.h"
#include <GameManagerRegistration.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <thread>
#include "FinalBoardView.h"

using namespace GameManager_315634022;

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

MyGameManager_315634022::MyGameManager_315634022(bool verbose)
  : verbose_(verbose), state_(nullptr)
{
    DEBUG_PRINT("GAMEMANAGER", "constructor", "GameManager_315634022 instance created", verbose_);
}

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
    // throw std::runtime_error("GameManager_315634022 is Fucking With you"); 
    DEBUG_PRINT("GAMEMANAGER", "run", 
        "Game parameters - Map: " + map_name + 
        ", Size: " + std::to_string(map_width) + "x" + std::to_string(map_height) + 
        ", MaxSteps: " + std::to_string(max_steps) + 
        ", NumShells: " + std::to_string(num_shells) + 
        ", Players: " + name1 + " vs " + name2, verbose_);
    
    prepareLog(map_name, name1, name2); 
    initializeGame(map_width, map_height, map, map_name, max_steps, num_shells,
                   player1, name1, player2, name2, 
                   player1_tank_algo_factory, player2_tank_algo_factory);
    gameLoop();
    GameResult result = finalize();
    
    INFO_PRINT("GAMEMANAGER", "run", "Game execution completed successfully");
    return result;
}

void MyGameManager_315634022::prepareLog(const std::string& map_name, const std::string& algo1_name, const std::string& algo2_name) {
    DEBUG_PRINT("LOGMANAGER", "prepareLog", "Preparing log file for verbose output", verbose_);
    
    if (!verbose_) {
        DEBUG_PRINT("LOGMANAGER", "prepareLog", "Verbose mode disabled, skipping log file creation", false);
        return;
    }
    
    // Extract just the filename without path and extension from map_name
    std::string clean_map_name = map_name;
    size_t last_slash = clean_map_name.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        clean_map_name = clean_map_name.substr(last_slash + 1);
    }
    size_t last_dot = clean_map_name.find_last_of(".");
    if (last_dot != std::string::npos) {
        clean_map_name = clean_map_name.substr(0, last_dot);
    }
    
    // Clean algorithm names (remove path and extension)
    auto cleanAlgoName = [](const std::string& name) {
        std::string clean = name;
        size_t last_slash = clean.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            clean = clean.substr(last_slash + 1);
        }
        size_t last_dot = clean.find_last_of(".");
        if (last_dot != std::string::npos) {
            clean = clean.substr(0, last_dot);
        }
        return clean;
    };
    
    std::string clean_algo1 = cleanAlgoName(algo1_name);
    std::string clean_algo2 = cleanAlgoName(algo2_name);
    
    DEBUG_PRINT("LOGMANAGER", "prepareLog", 
        "Cleaned names - Map: " + clean_map_name + ", Algo1: " + clean_algo1 + ", Algo2: " + clean_algo2, verbose_);
    
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << "GM_315634022_log_" 
       << clean_map_name << "_"
       << clean_algo1 << "_vs_" 
       << clean_algo2 << "_"
       << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S")
       << ".txt";
    
    std::string log_filename = ss.str();
    log_file_.open(log_filename);
    
    if (!log_file_.is_open()) {
        ERROR_PRINT("LOGMANAGER", "prepareLog", "Failed to open log file: " + log_filename);
    } else {
        INFO_PRINT("LOGMANAGER", "prepareLog", "Log file created successfully: " + log_filename);
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
    DEBUG_PRINT("GAMEMANAGER", "initializeGame", "Starting game initialization", verbose_);
    DEBUG_PRINT("GAMEMANAGER", "initializeGame", 
        "Board dimensions: " + std::to_string(width) + "x" + std::to_string(height), verbose_);
    
    // 1) Build and load the board
    INFO_PRINT("BOARDMANAGER", "initializeGame", "Initializing and loading game board from SatelliteView");
    board_ = Board(height, width);
    board_.loadFromSatelliteView(satellite_view);
    DEBUG_PRINT("BOARDMANAGER", "initializeGame", "Board loaded successfully from SatelliteView", verbose_);
    
    // 2) Initialize the GameState with the parsed Board using make_unique
    INFO_PRINT("GAMESTATE", "initializeGame", "Creating GameState instance");
    state_ = std::make_unique<GameState>(
        std::move(board_),
        map_name,
        max_steps,
        num_shells,
        p1, name1,
        p2, name2,
        f1, f2,
        verbose_
    );
    
    DEBUG_PRINT("GAMEMANAGER", "initializeGame", "Game initialization completed successfully", verbose_);
}

void MyGameManager_315634022::gameLoop() {
    INFO_PRINT("GAMELOOP", "gameLoop", "Starting main game loop");
    
    std::size_t turn = 1;
    while (!state_->isGameOver()) {
        DEBUG_PRINT("GAMELOOP", "gameLoop", "Executing turn " + std::to_string(turn), verbose_);
        
        std::string actions = state_->advanceOneTurn();
        
        if (verbose_) {
            DEBUG_PRINT("GAMELOOP", "gameLoop", "Writing turn actions to log file", verbose_);
            log_file_ << actions << std::endl;
        }
        
        turn++;
    }
    
    // Final board + result
    std::string resultStr = state_->getResultString();
    INFO_PRINT("GAMELOOP", "gameLoop", "Game completed: " + resultStr);
    
    if (verbose_) {
        DEBUG_PRINT("LOGMANAGER", "gameLoop", "Writing final results to log file and closing", verbose_);
        log_file_ << resultStr << "\n";
        log_file_.close();
    }
    
    DEBUG_PRINT("GAMELOOP", "gameLoop", "Game loop completed after " + std::to_string(turn - 1) + " turns", verbose_);
}

GameResult MyGameManager_315634022::finalize() {
    DEBUG_PRINT("GAMEMANAGER", "finalize", "Starting game result finalization", verbose_);
    
    // --- build a fully‑populated GameResult ---
    const Board& B = state_->getBoard();

    // 1) count remaining tanks
    size_t a1 = 0, a2 = 0;
    for (size_t y = 0; y < B.getRows(); ++y) {
        for (size_t x = 0; x < B.getCols(); ++x) {
            auto c = B.getCell(int(x), int(y)).content;
            if (c == CellContent::TANK1) ++a1;
            else if (c == CellContent::TANK2) ++a2;
        }
    }

    DEBUG_PRINT("GAMEMANAGER", "finalize", 
        "Tank count - Player1: " + std::to_string(a1) + ", Player2: " + std::to_string(a2), verbose_);

    // 2) decide winner (0=tie, 1=Player1, 2=Player2)
    int winner = 0;
    if      (a1 == 0 && a2 == 0) winner = 0;
    else if (a1 == 0)            winner = 2;
    else if (a2 == 0)            winner = 1;
    else                          winner = 0;  // tie by steps

    // 3) pick the correct Reason enum
    GameResult::Reason reason;
    if (a1 == 0 || a2 == 0)        reason = GameResult::ALL_TANKS_DEAD;
    else if (state_->getCurrentTurn() >= state_->getMaxSteps()) 
                                   reason = GameResult::MAX_STEPS;
    else                            reason = GameResult::ZERO_SHELLS;

    // 4) how many rounds actually played
    size_t rounds = state_->getCurrentTurn();

    DEBUG_PRINT("GAMEMANAGER", "finalize", 
        "Game outcome - Winner: " + std::to_string(winner) + 
        ", Reason: " + std::to_string(static_cast<int>(reason)) + 
        ", Rounds: " + std::to_string(rounds), verbose_);

    // 5) build our final satellite view of the board
    DEBUG_PRINT("GAMEMANAGER", "finalize", "Building final board view", verbose_);
    FinalBoardView fbv{ B };
    GameResult gr = fbv.toResult();   // this now just gives us gr.gameState

    // 6) overwrite the rest of the fields
    gr.winner          = winner;
    gr.reason          = reason;
    gr.rounds          = rounds;
    gr.remaining_tanks = { a1, a2 };

    INFO_PRINT("GAMEMANAGER", "finalize", "Game result finalization completed successfully");
    return gr;
}

// Register the game manager
REGISTER_GAME_MANAGER(MyGameManager_315634022)