#include "FinalBoardView.h"
#include <SatelliteView.h>   // the abstract interface

using namespace GameManager_315634022;

//— an anonymous helper that adapts Board → SatelliteView
namespace {
    class BoardSatView : public ::SatelliteView {
    public:
        explicit BoardSatView(const Board& b) : board_(b) {}
        char getObjectAt(size_t x, size_t y) const override {
            const auto& cell = board_.getCell(x, y);
            switch (cell.content) {
                case CellContent::WALL:   return '#';
                case CellContent::MINE:   return '@';
                case CellContent::TANK1:  return '1';
                case CellContent::TANK2:  return '2';
                default:                  return ' ';
            }
        }
    private:
        Board board_;
    };
}

FinalBoardView::FinalBoardView(const Board& finalBoard)
  : board_(finalBoard)
{}

GameResult FinalBoardView::toResult() const {
    GameResult result;

    // TODO: fill in your winner/reason/rounds/remaining_tanks here...

    // **critical**: wrap your final Board in a SatelliteView so gameState != nullptr
    result.gameState = std::make_unique<BoardSatView>(board_);
    return result;
}