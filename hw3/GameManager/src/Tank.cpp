#include "Tank.h"

using namespace GameManager_315634022;

// Constructor
Tank::Tank(int playerIndex,
           std::size_t x,
           std::size_t y,
           std::size_t numShells)
    : playerIndex_(playerIndex),
      x_(x),
      y_(y),
      shellsRemaining_(numShells),
      cooldown_(0),
      alive_(true),
      directionIndex_((playerIndex == 1) ? 6 : 2) // Player1 faces left (6), Player2 faces right (2)
{}

int  Tank::getPlayerIndex()      const { return playerIndex_; }
std::size_t Tank::getX()         const { return x_; }
std::size_t Tank::getY()         const { return y_; }
std::size_t Tank::getShellsRemaining() const { return shellsRemaining_; }
bool Tank::isAlive()             const { return alive_; }
int  Tank::getCooldown()         const { return cooldown_; }
int  Tank::getDirectionIndex()   const { return directionIndex_; }

void Tank::applyAction(ActionRequest action) {
    if (!alive_) return;

    switch (action) {
        case ActionRequest::MoveForward:
            applyMoveForward();
            break;

        case ActionRequest::MoveBackward:
            applyMoveBackward();
            break;

        case ActionRequest::RotateLeft90:
            rotateLeft90();
            break;

        case ActionRequest::RotateRight90:
            rotateRight90();
            break;

        case ActionRequest::RotateLeft45:
            rotateLeft45();
            break;

        case ActionRequest::RotateRight45:
            rotateRight45();
            break;

        case ActionRequest::Shoot:
            handleShoot();
            break;

        case ActionRequest::GetBattleInfo:
        case ActionRequest::DoNothing:
            // No change to position or direction here
            break;
    }
}

// === extracted helpers (private) ===
void Tank::applyMoveForward() {
    int newX = static_cast<int>(x_) + DX[directionIndex_];
    int newY = static_cast<int>(y_) + DY[directionIndex_];
    if (newX >= 0 && newY >= 0) {
        x_ = static_cast<std::size_t>(newX);
        y_ = static_cast<std::size_t>(newY);
    }
}

void Tank::applyMoveBackward() {
    int backDir = (directionIndex_ + 4) & 7;
    int newX = static_cast<int>(x_) + DX[backDir];
    int newY = static_cast<int>(y_) + DY[backDir];
    if (newX >= 0 && newY >= 0) {
        x_ = static_cast<std::size_t>(newX);
        y_ = static_cast<std::size_t>(newY);
    }
}

void Tank::rotateLeft90()  { directionIndex_ = (directionIndex_ + 6) & 7; }
void Tank::rotateRight90() { directionIndex_ = (directionIndex_ + 2) & 7; }
void Tank::rotateLeft45()  { directionIndex_ = (directionIndex_ + 7) & 7; }
void Tank::rotateRight45() { directionIndex_ = (directionIndex_ + 1) & 7; }

void Tank::handleShoot() {
    if (cooldown_ == 0 && shellsRemaining_ > 0) {
        shellsRemaining_--;
        cooldown_ = 3; // example fixed 3‐tick cooldown
    }
}

void Tank::tickCooldown() {
    if (cooldown_ > 0) {
        cooldown_--;
    }
}

void Tank::destroy() {
    alive_ = false;
}
