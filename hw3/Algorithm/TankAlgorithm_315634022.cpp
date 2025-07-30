// Algorithm/TankAlgorithm_315634022.cpp
#include "TankAlgorithmRegistration.h"
#include "TankAlgorithm_315634022.h"
using namespace Algorithm_315634022;
REGISTER_TANK_ALGORITHM(TankAlgorithm_315634022);
// ——— CTOR ——————————————————————————————————————————————————————————————
TankAlgorithm_315634022::TankAlgorithm_315634022(int playerIndex, int /*tankIndex*/)
  : lastInfo_{1,1},
    direction_(playerIndex == 1 ? 6 : 2),
    shellsLeft_(-1),
    needView_(true)
{}

// ——— updateBattleInfo ————————————————————————————————————————————————
void TankAlgorithm_315634022::updateBattleInfo(BattleInfo &baseInfo) {
    // We know baseInfo is actually MyBattleInfo
    std:: cout <<"my direction in life is" << direction_ << std::endl;
    lastInfo_ = static_cast<MyBattleInfo&>(baseInfo);
    if (shellsLeft_ < 0) {
        shellsLeft_ = int(lastInfo_.shellsRemaining);
    }
    needView_ = false;
}

// ——— getAction —————————————————————————————————————————————————————————
ActionRequest TankAlgorithm_315634022::getAction() {
    // 1) first thing first: ask for view once
    if (needView_) {
        needView_ = false;
        return ActionRequest::GetBattleInfo;
    }
    // 2) if got view just shoot!
    return ActionRequest::Shoot;
}


