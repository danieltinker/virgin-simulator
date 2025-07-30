// Algorithm/Player_315634022.cpp
#include "Player_315634022.h"
#include "PlayerRegistration.h"
#include "ActionRequest.h"
#include "MyBattleInfo.h"

using namespace Algorithm_315634022;

REGISTER_PLAYER(Player_315634022);

Player_315634022::Player_315634022(int playerIndex,
                                   std::size_t rows,
                                   std::size_t cols,
                                   std::size_t /*max_steps*/,
                                   std::size_t num_shells)
  : playerIndex_(playerIndex),
    rows_(rows),
    cols_(cols),
    shells_(num_shells),
    first_(true)
{}

void Player_315634022::updateTankWithBattleInfo(TankAlgorithm &tank,
                                               SatelliteView &view) {
    // build a fresh BattleInfo
    MyBattleInfo info(rows_, cols_);
    if (first_) {
        info.shellsRemaining = shells_;
        first_ = false;
    }
    // snapshot the grid and locate ourselves
    for (std::size_t y = 0; y < rows_; ++y) {
        for (std::size_t x = 0; x < cols_; ++x) {
            char c = view.getObjectAt(x, y);
            info.grid[y][x] = c;
            if ((playerIndex_ == 1 && c=='1') ||
                (playerIndex_ == 2 && c=='2')) {
                info.selfX = x;
                info.selfY = y;
            }
        }
    }
    // hand off to the tank algorithm
    tank.updateBattleInfo(info);
}
