// GameState.h
#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <memory>

#include <Board.h>
#include <SatelliteView.h>
#include <Player.h>
#include <ActionRequest.h>
#include <GameResult.h>
#include "MySatelliteView.h"
#include "TankAlgorithm.h"

namespace GameManager_315634022 {

/// Maintains the board, tanks, shells, and orchestrates one‐turn advances.
class GameState {
public:
    // --- Injected constructor replaces initialize() + factory usage ---
    GameState(
        Board&& board,
        std::string map_name,
        std::size_t max_steps,
        std::size_t num_shells,
        Player& player1,
        const std::string& name1,
        Player& player2,
        const std::string& name2,
        TankAlgorithmFactory algoFactory1,
        TankAlgorithmFactory algoFactory2,
        bool verbose
    );
    ~GameState();

    // Query loop status
    bool        isGameOver()     const;
    std::string getResultString() const;

    std::size_t getMaxSteps() const {return max_steps_;}
    // Advance mechanics
    std::string advanceOneTurn();

    // Display state
    void printBoard() const;
    void dumpStep(std::size_t turn) const;
    std::size_t getCurrentTurn() const;
    /// Expose the current board for final‐state snapshotting
    const Board& getBoard() const;

private:
    // Sub‐step helpers (unchanged)
    void applyTankRotations(const std::vector<ActionRequest>& actions);
    void handleTankMineCollisions();
    void updateTankCooldowns();
    void confirmBackwardMoves(std::vector<bool>& ignored,
                              const std::vector<ActionRequest>& actions);
    void updateTankPositionsOnBoard(std::vector<bool>& ignored,
                                    std::vector<bool>& killedThisTurn,
                                    const std::vector<ActionRequest>& actions);
    void handleShooting(std::vector<bool>& ignored,
                        const std::vector<ActionRequest>& actions);
    void updateShellsWithOverrunCheck();
    void resolveShellCollisions();
    bool handleShellMidStepCollision(int x, int y);
    void cleanupDestroyedEntities();
    void checkGameEndConditions();
    void filterRemainingShells();

    std::unique_ptr<SatelliteView>
    createSatelliteViewFor(int queryX, int queryY) const;

    static const char* directionToArrow(int dir);

    // ---- Internal state ----
    bool                     verbose_;
    Board                    board_;
    std::string              map_name_;
    std::size_t              max_steps_, currentStep_;
    std::size_t              num_shells_;

    bool                     gameOver_;
    std::string              resultStr_;

    // Board dimensions & tank indexing
    std::size_t              rows_, cols_;
    int                      nextTankIndex_[3];

    // Tanks
    struct TankState {
        int           player_index;
        int           tank_index;
        int           x, y;
        int           direction;
        bool          alive;
        std::size_t   shells_left;
        int           shootCooldown;
        int           backwardDelayCounter;
        bool          lastActionBackwardExecuted;
    };
    std::vector<TankState>   all_tanks_;
    std::vector<std::vector<std::size_t>> tankIdMap_;

    // Injected players
    Player&                  p1_;
    std::string              name1_;
    Player&                  p2_;
    std::string              name2_;

    // Algorithm factories (one per player)
    TankAlgorithmFactory     algoFactory1_;
    TankAlgorithmFactory     algoFactory2_;
    std::vector<std::unique_ptr<TankAlgorithm>> all_tank_algorithms_;

    // Shells & mapping
    struct Shell { int x, y, dir; };
    std::vector<Shell>       shells_;
    std::set<std::size_t>    toRemove_;
    std::map<std::pair<int,int>, std::vector<std::size_t>> positionMap_;
};


} // namespace GameManager_315634022







