#include "GameManagerRegistrar.h"
#include "GameManagerRegistration.h"

GameManagerRegistration::GameManagerRegistration(GameManagerFactory factory) {
    auto& registrar = GameManagerRegistrar::get();
    registrar.addGameManagerFactoryToLastEntry(std::move(factory));
}
