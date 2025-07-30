// Simulator/GameManagerRegistrar.cpp
#include "GameManagerRegistrar.h"

GameManagerRegistrar GameManagerRegistrar::registrar;

GameManagerRegistrar& GameManagerRegistrar::get() {
    return registrar;
}
