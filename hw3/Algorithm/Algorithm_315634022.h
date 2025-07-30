#pragma once

#include <TankAlgorithm.h>
#include <Player.h>

namespace Algorithm_315634022 {

class TankAlgorithm_315634022 : public TankAlgorithm {
public:
    TankAlgorithm_315634022(int player_idx, int tank_idx);
    ActionRequest getAction() override;
    void updateBattleInfo(BattleInfo& info) override;
};

class Player_315634022 : public Player {
public:
    Player_315634022(int player_idx, size_t x, size_t y, size_t max_steps, size_t num_shells);
    void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& view) override;
};

} // namespace Algorithm_315634022
