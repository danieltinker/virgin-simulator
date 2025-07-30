#include "GameManager_315634022.h"
#include <GameManagerRegistration.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include "FinalBoardView.h"

using namespace GameManager_315634022;

MyGameManager_315634022::MyGameManager_315634022(bool verbose)
  : verbose_(verbose), state_(nullptr)
{}

GameResult MyGameManager_315634022::run(
    size_t map_width,
    size_t map_height,
    const SatelliteView& map,
    string map_name,                    // Fixed: match AbstractGameManager signature
    size_t max_steps,
    size_t num_shells,
    Player& player1,                    // Fixed: parameter names
    string name1,                       // Fixed: parameter names
    Player& player2,                    // Fixed: parameter names
    string name2,                       // Fixed: parameter names
    TankAlgorithmFactory player1_tank_algo_factory,    // Fixed: parameter names
    TankAlgorithmFactory player2_tank_algo_factory     // Fixed: parameter names
) {
    std::cout << "GameManager_315634022 is now up and running..." <<std::endl;
    prepareLog(map_name);
    initializeGame(map_width, map_height, map, map_name, max_steps, num_shells,
                   player1, name1, player2, name2, 
                   player1_tank_algo_factory, player2_tank_algo_factory);
    gameLoop();
    return finalize();
}

void MyGameManager_315634022::prepareLog(const std::string& map_name) {
    if (!verbose_) return;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << "GM_315634022_log_" 
       << map_name << "_"
       << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S")
       << ".txt";
    log_file_.open(ss.str());
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
    // 1) Build and load the board
    std::cout << "GM_315634022: Getting Ready to Initilize & Load Game Board from SatView" << std::endl;
    board_ = Board(height,width);
    board_.loadFromSatelliteView(satellite_view);
    std::cout << "GM_315634022: Finished Initilize & Load Game Board from SatView \n Creating GameState..." << std::endl;
    
    // 2) Initialize the GameState with the parsed Board using make_unique
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
}

void MyGameManager_315634022::gameLoop() {
    std::size_t turn = 1;
    while (!state_->isGameOver()) {
        std::cout << "GM_315634022 Game Loop Turn = "<< turn<< std::endl;
        std::string actions = state_->advanceOneTurn();
        // state_->advanceOneTurn();
        if (verbose_) {
            // log_file_ << state_->getResultString() << "\n";
            log_file_ << actions << "\n";
        }
        turn++;
    }
     // 4) Final board + result
     std::string resultStr = state_->getResultString();
     std::cout << resultStr << "\n";
     if (verbose_) {
        log_file_ << resultStr << "\n";
        log_file_.close();
    }
}

GameResult MyGameManager_315634022::finalize() {
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

    // 5) build our final satellite view of the board
    FinalBoardView fbv{ B };
    GameResult gr = fbv.toResult();   // this now just gives us gr.gameState

    // 6) overwrite the rest of the fields
    gr.winner          = winner;
    gr.reason          = reason;
    gr.rounds          = rounds;
    gr.remaining_tanks = { a1, a2 };

    return gr;
 }

// Register the game manager
REGISTER_GAME_MANAGER(MyGameManager_315634022)