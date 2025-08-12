// #include "FinalBoardView.h"
// #include <SatelliteView.h>   // the abstract interface

// using namespace GameManager_315634022;

// //— an anonymous helper that adapts Board → SatelliteView
// namespace {
//     class BoardSatView : public ::SatelliteView {
//     public:
//         explicit BoardSatView(const Board& b) : board_(b) {}
//         char getObjectAt(size_t x, size_t y) const override {
//             const auto& cell = board_.getCell(x, y);
//             switch (cell.content) {
//                 case CellContent::WALL:   return '#';
//                 case CellContent::MINE:   return '@';
//                 case CellContent::TANK1:  return '1';
//                 case CellContent::TANK2:  return '2';
//                 default:                  return ' ';
//             }
//         }
//     private:
//         Board board_;
//     };
// }

// FinalBoardView::FinalBoardView(const Board& finalBoard)
//   : board_(finalBoard)
// {}

// GameResult FinalBoardView::toResult() const {
//     GameResult result;

//     // TODO: fill in your winner/reason/rounds/remaining_tanks here...

//     // **critical**: wrap your final Board in a SatelliteView so gameState != nullptr
//     result.gameState = std::make_unique<BoardSatView>(board_);
// std::fprintf(stderr, "[FinalBoardView::toResult] producing GameResult with gameState ptr=%p\n",
//                  static_cast<void*>(result.gameState.get()));

//     return result;
// }

// hw3/GameManager/src/FinalBoardView.cpp











// #include "FinalBoardView.h"
// #include <SatelliteView.h>   // the abstract interface
// #include <cstdio>            // std::fprintf

// using namespace GameManager_315634022;

// // — Adapt Board → SatelliteView so GameResult::gameState can own a snapshot
// namespace {
// class BoardSatView : public ::SatelliteView {
// public:
//     explicit BoardSatView(const Board& b) : board_(b) {}
//     char getObjectAt(size_t x, size_t y) const override {
//         const auto& cell = board_.getCell(int(x), int(y));
//         switch (cell.content) {
//             case CellContent::WALL:   return '#';
//             case CellContent::MINE:   return '@';
//             case CellContent::TANK1:  return '1';
//             case CellContent::TANK2:  return '2';
//             case CellContent::EMPTY:  return '_';
//         }
//         return '_';
//     }
//     size_t getWidth()  const  { return board_.getCols(); }
//     size_t getHeight() const  { return board_.getRows(); }
// private:
//     const Board& board_;
// };
// } // namespace

// FinalBoardView::FinalBoardView(const Board& finalBoard)
//   : board_(finalBoard)
// {}

// GameResult FinalBoardView::toResult() const {
//     GameResult result;

//     // Count remaining tanks directly from the final board
//     size_t p1 = 0, p2 = 0;
//     const size_t rows = board_.getRows();
//     const size_t cols = board_.getCols();
//     for (size_t y = 0; y < rows; ++y) {
//         for (size_t x = 0; x < cols; ++x) {
//             const auto& cell = board_.getCell(int(x), int(y));
//             if (cell.content == CellContent::TANK1) ++p1;
//             else if (cell.content == CellContent::TANK2) ++p2;
//         }
//     }
//     result.remaining_tanks = { p1, p2 };

//     // Winner can be deduced ONLY for the "all tanks dead" cases
//     if (p1 == 0 && p2 == 0) {
//         result.winner = 0;
//         result.reason = GameResult::ALL_TANKS_DEAD;
//     } else if (p1 == 0 && p2 > 0) {
//         result.winner = 2;
//         result.reason = GameResult::ALL_TANKS_DEAD;
//     } else if (p2 == 0 && p1 > 0) {
//         result.winner = 1;
//         result.reason = GameResult::ALL_TANKS_DEAD;
//     } else {
//         // Still tanks on both sides: the actual reason/rounds depend on GameState.
//         // Provide safe defaults that GameManager::finalize() will overwrite.
//         result.winner = 0; // tie by default here
//         result.reason = GameResult::MAX_STEPS; // placeholder
//     }

//     // Rounds are not inferable from the Board alone; GameManager will set this.
//     result.rounds = 0;

//     // Wrap the final Board in a SatelliteView so gameState != nullptr
//     result.gameState = std::make_unique<BoardSatView>(board_);
//     std::fprintf(stderr,
//         "[FinalBoardView::toResult] producing GameResult with gameState ptr=%p\n",
//         static_cast<void*>(result.gameState.get()));

//     return result;
// }




// hw3/GameManager/src/FinalBoardView.cpp
#include "FinalBoardView.h"
#include <SatelliteView.h>
#include <cstdio>

using namespace GameManager_315634022;

namespace {
// Adapt Board → SatelliteView so GameResult::gameState can own a snapshot
class BoardSatView : public ::SatelliteView {
public:
    explicit BoardSatView(const Board& b) : board_(b) {}

    // Overlay shells first (like renderRow), then fall back to cell content.
    char getObjectAt(size_t x, size_t y) const override {
        const size_t w = board_.getCols();
        const size_t h = board_.getRows();
        if (x >= w || y >= h) return '&';                   // ← out-of-bounds guard
        

        const auto& cell = board_.getCell(int(x), int(y));
        if (cell.hasShellOverlay) return '*';               // overlay wins
        switch (cell.content) {
            case CellContent::WALL:   return '#';
            case CellContent::MINE:   return '@';
            case CellContent::TANK1:  return '1';
            case CellContent::TANK2:  return '2';
            case CellContent::EMPTY:  return ' ';           // <— space, not underscore
        }
        return ' ';
    }

    size_t width()  const { return board_.getCols(); }
    size_t height() const { return board_.getRows(); }

private:
    const Board& board_;
};
} // namespace

FinalBoardView::FinalBoardView(const Board& finalBoard)
  : board_(finalBoard)
{}

GameResult FinalBoardView::toResult() const {
    GameResult result;

    // Count remaining tanks directly from the final board
    size_t p1 = 0, p2 = 0;
    const size_t rows = board_.getRows();
    const size_t cols = board_.getCols();
    for (size_t y = 0; y < rows; ++y) {
        for (size_t x = 0; x < cols; ++x) {
            const auto& c = board_.getCell(int(x), int(y)).content;
            if (c == CellContent::TANK1) ++p1;
            else if (c == CellContent::TANK2) ++p2;
        }
    }
    result.remaining_tanks = { p1, p2 };

    // Winner/reason defaults — GameManager::finalize() will set true reason/rounds
    if (p1 == 0 && p2 == 0)      { result.winner = 0; result.reason = GameResult::ALL_TANKS_DEAD; }
    else if (p1 == 0)            { result.winner = 2; result.reason = GameResult::ALL_TANKS_DEAD; }
    else if (p2 == 0)            { result.winner = 1; result.reason = GameResult::ALL_TANKS_DEAD; }
    else                         { result.winner = 0; result.reason = GameResult::MAX_STEPS; }

    result.rounds = 0; // filled accurately in finalize()

    // Wrap final board → SatelliteView (overlays '*' via BoardSatView)
    result.gameState = std::make_unique<BoardSatView>(board_);
    std::fprintf(stderr,
        "[FinalBoardView::toResult] producing GameResult with gameState ptr=%p\n",
        static_cast<void*>(result.gameState.get()));

    return result;
}
