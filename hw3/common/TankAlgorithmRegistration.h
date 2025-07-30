#pragma once

#include <functional>
#include <memory>
#include <string>
#include <iostream>

#include "TankAlgorithm.h"
// #include "TankAlgorithmRegistrar.h"
struct TankAlgorithmRegistration {
  TankAlgorithmRegistration(TankAlgorithmFactory);
};

#define REGISTER_TANK_ALGORITHM(class_name)                       \
    static TankAlgorithmRegistration register_me_##class_name(   \
        [](int player_index, int tank_index) {                   \
            return std::make_unique<class_name>(                 \
                player_index, tank_index                         \
            );                                                    \
        })
        