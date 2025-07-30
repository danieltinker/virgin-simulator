#pragma once

#include <Board.h>         // or wherever your Board lives
#include <GameResult.h>    // the definition of GameResult
#include <string>
#include <vector>

namespace GameManager_315634022{
/// Transforms a finished Board into a GameResult
class FinalBoardView {
public:
    /// Take ownership (or const‑copy) of the final board state
    explicit FinalBoardView(const Board& finalBoard);

    /// Convert to whatever your runner expects
    GameResult toResult() const;

private:
    Board   board_;
};
}