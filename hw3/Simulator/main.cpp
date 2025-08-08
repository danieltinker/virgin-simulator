// Simulator/main.cpp
#include <iostream>
#include "ArgParser.h"
#include "Simulator.h"

#include "ErrorLogger.h"
#include <exception>
#include <cstdlib>
using namespace UserCommon_315634022;

static void terminateHandler() {
    try {
        std::exception_ptr eptr = std::current_exception();
        if (eptr) {
            std::rethrow_exception(eptr);
        } else {
            ErrorLogger::instance().log("Terminated due to unknown exception.");
        }
    } catch (const std::exception& ex) {
        ErrorLogger::instance().log(std::string("Unhandled exception: ") + ex.what());
    } catch (...) {
        ErrorLogger::instance().log("Unhandled non-standard exception.");
    }
    std::abort();
}

int main(int argc, char* argv[]) {
    ErrorLogger::instance().init();
    std::set_terminate(terminateHandler);

    Config cfg;
    if (!parseArguments(argc, argv, cfg)) {
        ErrorLogger::instance().log("Failed to parse command line arguments");
        return 1;
    }

    try {
        Simulator simulator(cfg);
        int result = simulator.run();

        std::cout << "\nSimulation Statistics:\n";
        std::cout << "Total games played: " << simulator.getTotalGamesPlayed() << "\n";
        std::cout << "Algorithms loaded: " << simulator.getSuccessfullyLoadedAlgorithms() << "\n";
        std::cout << "GameManagers loaded: " << simulator.getSuccessfullyLoadedGameManagers() << "\n";
        
        return result;
    } catch (const std::exception& ex) {
        ErrorLogger::instance().log(std::string("Fatal error: ") + ex.what());
        return 1;
    } catch (...) {
        ErrorLogger::instance().log("Unknown fatal error occurred");
        return 1;
    }
}