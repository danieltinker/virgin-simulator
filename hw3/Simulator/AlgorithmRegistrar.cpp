#include "AlgorithmRegistrar.h"

AlgorithmRegistrar AlgorithmRegistrar::registrar;

AlgorithmRegistrar& AlgorithmRegistrar::get() {
    return registrar;
}
