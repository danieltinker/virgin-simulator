#pragma once

#include "ActionRequest.h"
#include <cstddef>

namespace GameManager_315634022 {

/*
  A Tank holds its player index, (x,y) position, facing direction (0..7),
  number of shells left, a cooldown, and an alive flag.  It can apply one
  of the ActionRequest commands to update its own state.
*/
class Tank {
public:
    // Constructor: (playerIndex, initial x, initial y, numShells)
    Tank(int playerIndex,
         std::size_t x,
         std::size_t y,
         std::size_t numShells);

    ~Tank();

    // Accessors:
    int getPlayerIndex() const;
    std::size_t getX() const;
    std::size_t getY() const;
    bool isAlive() const;
    std::size_t getShellsRemaining() const;
    int getCooldown() const;
    int getDirectionIndex() const;

    // Called each tick to apply the chosen action to this tank:
    void applyAction(ActionRequest action);

    // Called by GameManager/GameState once per tick to decrement cooldown:
    void tickCooldown();

    // Kill the tank immediately:
    void destroy();
    
    // For applyAction, move deltas for each direction index:
    static constexpr int DX[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
    static constexpr int DY[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };
private:
    int playerIndex_;
    std::size_t x_, y_;
    std::size_t shellsRemaining_;
    int cooldown_;
    bool alive_;
    int directionIndex_;  // 0=Up, 1=Up-Right, 2=Right, 3=Down-Right, 4=Down, 5=Down-Left, 6=Left, 7=Up-Left

};

} // namespace arena
