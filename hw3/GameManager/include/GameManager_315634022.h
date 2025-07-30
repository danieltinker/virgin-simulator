#pragma once

#include <AbstractGameManager.h>
#include <Board.h>
#include <GameState.h>
// #include <FinalBoardView.h>
#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace GameManager_315634022 {

class MyGameManager_315634022 : public AbstractGameManager {
public:
    explicit MyGameManager_315634022(bool verbose);
    ~MyGameManager_315634022() override = default;

    GameResult run(
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
    ) override;

private:
    // void prepareLog(const std::string& map_name);
    void prepareLog(const std::string& map_name, const std::string& algo1_name, const std::string& algo2_name);
    void initializeGame(
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
    );
    void gameLoop();
    GameResult finalize();

    bool                            verbose_;
    Board                           board_;
    std::unique_ptr<GameState>      state_;     // Fixed: use pointer to avoid reference member issues
    std::ofstream                   log_file_;
};

} // namespace GameManager_315634022