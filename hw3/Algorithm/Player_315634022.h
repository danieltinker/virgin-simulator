// #pragma once

// #include <Player.h>
// namespace Algorithm_315634022 {

// class Player_315634022 : public Player {
// public:
//     Player_315634022(int player_idx, size_t x, size_t y, size_t max_steps, size_t num_shells);
//     void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& view) override;
// };
// }

// Algorithm/Player_315634022.h
#pragma once
#include "Player.h"
#include <cstddef>
#include <string>

namespace Algorithm_315634022 {

class Player_315634022 : public Player {
public:
    Player_315634022(int playerIndex,
                     std::size_t rows,
                     std::size_t cols,
                     std::size_t max_steps,
                     std::size_t num_shells);

    void updateTankWithBattleInfo(TankAlgorithm &tank,
                                  SatelliteView &view) override;

private:
    int    playerIndex_;
    std::size_t rows_, cols_;
    std::size_t shells_;
    bool   first_;
};

} // namespace Algorithm_315634022
