#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "bitboard.hpp"
#include "board.hpp"
#include "pieces.hpp"

class Move {
  typedef uint16_t Word;

  const Word move;
  static const size_t nbits_type = 4, nbits_from = 6, nbits_to = 6;
  static const size_t offset_type = 0, offset_from = offset_type + nbits_type, offset_to = offset_from + nbits_from;

public:
  // NOTE: at most 16!
  enum class Type {
    normal,
    double_push,
    castle_kingside,
    castle_queenside,
    promotion_knight,
    promotion_bishop,
    promotion_rook,
    promotion_queen,
  };

  static std::string typename_from_type(Type type);

  Move(squares::Index from, squares::Index to, Type type);
  Move(const Move& that);

  Type type() const;
  squares::Index from() const;
  squares::Index to  () const;

  bool operator==(const Move& that) const;
  bool operator!=(const Move& that) const;
  bool operator< (const Move& that) const; // used for std::set in tests

  friend std::ostream& operator<<(std::ostream& o, const Move& m);
};


namespace moves {
  Bitboard pawn_attacks_w(Bitboard pawn);
  Bitboard pawn_attacks_e(Bitboard pawn);
  Bitboard knight_attacks_nnw(Bitboard knight);
  Bitboard knight_attacks_ssw(Bitboard knight);
  Bitboard knight_attacks_nww(Bitboard knight);
  Bitboard knight_attacks_sww(Bitboard knight);
  Bitboard knight_attacks_nne(Bitboard knight);
  Bitboard knight_attacks_sse(Bitboard knight);
  Bitboard knight_attacks_nee(Bitboard knight);
  Bitboard knight_attacks_see(Bitboard knight);
  Bitboard bishop_attacks(Bitboard occupancy, squares::Index source);
  Bitboard rook_attacks(Bitboard occupancy, squares::Index source);
  Bitboard queen_attacks(Bitboard occupancy, squares::Index source);
  Bitboard king_attacks(Bitboard king);
  
  Bitboard all_attacks(Bitboard occupancy, std::array<Bitboard, pieces::cardinality> attackers);
  bool is_attacked(Bitboard targets, Bitboard occupancy, std::array<Bitboard, pieces::cardinality> attackers);
  
  void pawn(std::vector<Move>& moves, Bitboard pawn, Bitboard us, Bitboard them, Bitboard en_passant_square);
  void knight(std::vector<Move>& moves, Bitboard knight, Bitboard us, Bitboard them);
  void bishop(std::vector<Move>& moves, Bitboard bishop, Bitboard us, Bitboard them);
  void rook(std::vector<Move>& moves, Bitboard rook, Bitboard us, Bitboard them);
  void queen(std::vector<Move>& moves, Bitboard queen, Bitboard us, Bitboard them);
  void king(std::vector<Move>& moves, Bitboard king, Bitboard us, Bitboard them);
  void castle_kingside(std::vector<Move>& moves, Bitboard occupancy, std::array<Bitboard, pieces::cardinality> attackers);
  void castle_queenside(std::vector<Move>& moves, Bitboard occupancy, std::array<Bitboard, pieces::cardinality> attackers);
  void all_moves(std::vector<Move>& moves, Board board, Bitboard en_passant_square);
}