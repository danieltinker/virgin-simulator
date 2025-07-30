#include "AlgorithmRegistrar.h"
#include "TankAlgorithmRegistration.h"
TankAlgorithmRegistration::TankAlgorithmRegistration(TankAlgorithmFactory factory) {
    auto& regsitrar = AlgorithmRegistrar::get();
    regsitrar.addTankAlgorithmFactoryToLastEntry(std::move(factory));
}
