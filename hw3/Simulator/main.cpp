#include <iostream>
#include "ArgParser.h"
#include "Simulator.h"

int main(int argc, char* argv[]) {
    // Parse command line arguments
    Config cfg;
    if (!parseArguments(argc, argv, cfg)) {
        return 1;
    }
    
    try {
        // Create and run the simulator
        Simulator simulator(cfg);
        int result = simulator.run();
        
        // Optional: Print some statistics
        std::cout << "\nSimulation Statistics:\n";
        std::cout << "Total games played: " << simulator.getTotalGamesPlayed() << "\n";
        std::cout << "Algorithms loaded: " << simulator.getSuccessfullyLoadedAlgorithms() << "\n";
        std::cout << "GameManagers loaded: " << simulator.getSuccessfullyLoadedGameManagers() << "\n";
        
        return result;
        
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}