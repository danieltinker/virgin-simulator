#pragma once

#include "TankAlgorithm.h"
#include "SatelliteView.h"

#include <memory>     
#include <functional> 
#include <cstddef>
/**
 * Every “Player” must implement this interface.  When GameState sees
 * a tank asking for GetBattleInfo, it will call updateTankWithBattleInfo()
 * and pass in a reference to the tank’s TankAlgorithm plus a BattleInfo.
 */
class Player {
public:
	virtual ~Player() {}
    virtual void updateTankWithBattleInfo
        (TankAlgorithm& tank, SatelliteView& satellite_view) = 0;
};

using PlayerFactory = 
std::function<std::unique_ptr<Player>
(int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells)>;
