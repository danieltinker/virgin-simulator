#pragma once

#include <TankAlgorithm.h>
#include "MyBattleInfo.h"
#include "ActionRequest.h"
#include <queue>
#include <limits>

namespace Algorithm_315634022 {

class TankAlgorithm_315634022 : public TankAlgorithm {
public:
    TankAlgorithm_315634022(int, int);
    ActionRequest getAction() override;
    void updateBattleInfo(BattleInfo&) override;


private:
    MyBattleInfo lastInfo_;
    int          direction_;
    int          shellsLeft_;
    bool         needView_;

};

}
