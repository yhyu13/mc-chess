#pragma once

#include "trivialboardpartition.hpp"

namespace squares {
  extern TrivialBoardPartition partition;
  typedef TrivialBoardPartition::Index Index;

#define _(i) partition[i]
  const TrivialBoardPartition::Part a1 = _( 0), b1 = _( 1), c1 = _( 2), d1 = _( 3), e1 = _( 4), f1 = _( 5), g1 = _( 6), h1 = _( 7),
                                    a2 = _( 8), b2 = _( 9), c2 = _(10), d2 = _(11), e2 = _(12), f2 = _(13), g2 = _(14), h2 = _(15),
                                    a3 = _(16), b3 = _(17), c3 = _(18), d3 = _(19), e3 = _(20), f3 = _(21), g3 = _(22), h3 = _(23),
                                    a4 = _(24), b4 = _(25), c4 = _(26), d4 = _(27), e4 = _(28), f4 = _(29), g4 = _(30), h4 = _(31),
                                    a5 = _(32), b5 = _(33), c5 = _(34), d5 = _(35), e5 = _(36), f5 = _(37), g5 = _(38), h5 = _(39),
                                    a6 = _(40), b6 = _(41), c6 = _(42), d6 = _(43), e6 = _(44), f6 = _(45), g6 = _(46), h6 = _(47),
                                    a7 = _(48), b7 = _(49), c7 = _(50), d7 = _(51), e7 = _(52), f7 = _(53), g7 = _(54), h7 = _(55),
                                    a8 = _(56), b8 = _(57), c8 = _(58), d8 = _(59), e8 = _(60), f8 = _(61), g8 = _(62), h8 = _(63);
#undef _

  Index index_from_bitboard(Bitboard b);
  TrivialBoardPartition::Part from_bitboard(Bitboard b);
}
