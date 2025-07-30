//----------------------------------
// Simulator/PlayerRegistration.cpp
//----------------------------------
#include "AlgorithmRegistrar.h"        // your host’s registrar API
#include "PlayerRegistration.h"  // the header that declares PlayerRegistration

PlayerRegistration::PlayerRegistration(PlayerFactory factory) {
    auto& regsitrar = AlgorithmRegistrar::get();
    regsitrar.addPlayerFactoryToLastEntry(std::move(factory));
}