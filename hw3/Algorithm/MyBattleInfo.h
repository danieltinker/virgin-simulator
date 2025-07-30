// Algorithm/MyBattleInfo.h
#pragma once
#include "BattleInfo.h"
#include <vector>
#include <cstddef>

/// Extends BattleInfo with a full grid snapshot + self‐position + shell count.
struct MyBattleInfo : public BattleInfo {
    std::size_t rows, cols;
    std::vector<std::vector<char>> grid;
    std::size_t selfX = 0, selfY = 0;
    std::size_t shellsRemaining = 0;

    MyBattleInfo(std::size_t r, std::size_t c)
      : rows(r), cols(c),
        grid(r, std::vector<char>(c,' ')),
        selfX(0), selfY(0),
        shellsRemaining(0)
    {}
};