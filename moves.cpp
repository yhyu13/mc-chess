#include <stdexcept>
#include <cassert>

#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <boost/optional.hpp>

#include "moves.hpp"
#include "direction.hpp"
#include "knight.hpp"

// routines for generating moves for White.  flip the bitboards vertically and swap ownership
// to make them applicable for Black.

// i fought the language, and the language won.
std::string Move::typename_from_type(Type type) {
  switch (type) {
  case Type::normal:           return "normal";
  case Type::double_push:      return "double_push";
  case Type::castle_kingside:  return "castle_kingside";
  case Type::castle_queenside: return "castle_queenside";
  case Type::capture:          return "capture";
  case Type::promotion_knight: return "promotion_knight";
  case Type::promotion_bishop: return "promotion_bishop";
  case Type::promotion_rook:   return "promotion_rook";
  case Type::promotion_queen:  return "promotion_queen";
  case Type::capturing_promotion_knight: return "capturing_promotion_knight";
  case Type::capturing_promotion_bishop: return "capturing_promotion_bishop";
  case Type::capturing_promotion_rook:   return "capturing_promotion_rook";
  case Type::capturing_promotion_queen:  return "capturing_promotion_queen";
  default: throw std::invalid_argument(str(boost::format("undefined move type: %|1$#x|") % (unsigned short)(type)));
  }
}

Move::Move(squares::Index from, squares::Index to, Type type = Type::normal) :
  move(((Word)type << offset_type) | (from << offset_from) | (to << offset_to))
{
  // TODO: change squares::Index to an unsigned type to catch some more bugs here
  assert(0 <= from && from < squares::partition.cardinality);
  assert(0 <= to   && to   < squares::partition.cardinality);
}

Move::Move(const Move& that) :
  move(that.move)
{
}

Move& Move::operator=(const Move& that) {
  this->move = that.move;
  return *this;
}

Move::Type Move::type() const { return static_cast<Move::Type>((move >> offset_type) & ((1 << nbits_type) - 1)); }
squares::Index Move::from() const { return (move >> offset_from) & ((1 << nbits_from) - 1); }
squares::Index Move::to  () const { return (move >> offset_to)   & ((1 << nbits_to)   - 1); }

bool Move::is_capture() const {
  switch (type()) {
  case Move::Type::capture:
  case Move::Type::capturing_promotion_knight:
  case Move::Type::capturing_promotion_bishop:
  case Move::Type::capturing_promotion_rook:
  case Move::Type::capturing_promotion_queen:
    return true;
  default:
    return false;
  }
}

bool Move::operator==(const Move& that) const { return this->move == that.move; }
bool Move::operator!=(const Move& that) const { return this->move != that.move; }
bool Move::operator< (const Move& that) const { return this->move <  that.move; }

// after https://chessprogramming.wikispaces.com/Hyperbola+Quintessence
Bitboard moves::slides(Bitboard occupancy, Bitboard piece, Bitboard mobility) {
  Bitboard forward, reverse;
  mobility &= ~piece;
  forward = occupancy & mobility;
  reverse  = __builtin_bswap64(forward);
  forward -= piece;
  reverse -= __builtin_bswap64(piece);
  forward ^= __builtin_bswap64(reverse);
  forward &= mobility;
  return forward;
}

Bitboard moves::rank_onto_a1h8(Bitboard b, Rank rank) {
  // put the bits for the relevant rank into LSB
  b = (b >> rank.index * directions::vertical) & 0xff;
  // map LSB onto a1h8 diagonal
  b = (b * 0x0101010101010101) & antidiagonals::a1h8;
  return b;
}

Bitboard moves::a1h8_onto_rank(Bitboard b, Rank rank) {
  assert((b & ~antidiagonals::a1h8) == 0);
  // map diagonal onto MSB
  b *= 0x0101010101010101;
  // down to LSB, dropping everything except MSB
  b /= 0x0100000000000000;
  // shift up to the desired rank
  b <<= rank.index * directions::vertical;
  return b;
}

// like slides, but for attacks by a single rook along a rank.  slides() doesn't work for
// that because the byteswap does not reverse the relevant bits; they are all in the same
// byte.
Bitboard moves::slides_rank(Bitboard occupancy, Bitboard piece, Rank rank) {
  occupancy = rank_onto_a1h8(occupancy, rank);
  piece     = rank_onto_a1h8(piece,     rank);
  Bitboard attacks = slides(occupancy, piece, antidiagonals::a1h8);
  return a1h8_onto_rank(attacks, rank);
}

using namespace directions;

Bitboard moves::pawn_attacks_w(Bitboard pawn) { return ((pawn & ~files::a) << vertical >> horizontal); }
Bitboard moves::pawn_attacks_e(Bitboard pawn) { return ((pawn & ~files::h) << vertical << horizontal); }

Bitboard moves::knight_attacks(Bitboard knight, short leftshift, short rightshift, Bitboard badtarget) {
  return (knight << leftshift >> rightshift) & ~badtarget;
}

Bitboard moves::bishop_attacks(Bitboard occupancy, squares::Index source) {
  return 
    slides(occupancy, squares::partition[source].bitboard,     diagonals::partition.parts_by_square_index[source]) |
    slides(occupancy, squares::partition[source].bitboard, antidiagonals::partition.parts_by_square_index[source]);
}

Bitboard moves::rook_attacks(Bitboard occupancy, squares::Index source) {
  return 
    slides     (occupancy, squares::partition[source].bitboard, files::partition.parts_by_square_index[source]) |
    slides_rank(occupancy, squares::partition[source].bitboard, ranks::partition.parts_by_square_index[source]);
}

Bitboard moves::queen_attacks(Bitboard occupancy, squares::Index source) {
  return bishop_attacks(occupancy, source) | rook_attacks(occupancy, source);
}

Bitboard moves::king_attacks(Bitboard king) {
  Bitboard leftright = ((king & ~files::a) >> horizontal) | ((king & ~files::h) << horizontal);
  Bitboard triple = leftright | king;
  return leftright | (triple << vertical) | (triple >> vertical);
}

// NOTE: includes attacks on own pieces
Bitboard moves::all_attacks(Bitboard occupancy, Board board) {
  std::array<Bitboard, pieces::cardinality> attackers = board[colors::white];

  Bitboard knight_attacks = 0;
  for (KnightAttackType ka: get_knight_attack_types())
    // TODO: dry up
    knight_attacks |= moves::knight_attacks(attackers[pieces::knight], ka.leftshift, ka.rightshift, ka.badtargets);

  // TODO: dry up
  Bitboard bishop_attacks = 0;
  bitboard::for_each_member(attackers[pieces::bishop], [&bishop_attacks, &occupancy](squares::Index source) {
      bishop_attacks |= moves::bishop_attacks(occupancy, source);
    });

  Bitboard rook_attacks = 0;
  bitboard::for_each_member(attackers[pieces::rook], [&rook_attacks, &occupancy](squares::Index source) {
      rook_attacks |= moves::rook_attacks(occupancy, source);
    });

  Bitboard queen_attacks = 0;
  bitboard::for_each_member(attackers[pieces::queen], [&queen_attacks, &occupancy](squares::Index source) {
      queen_attacks |= moves::queen_attacks(occupancy, source);
    });

  return
    pawn_attacks_w(attackers[pieces::pawn]) |
    pawn_attacks_e(attackers[pieces::pawn]) |
    king_attacks  (attackers[pieces::king]) |
    knight_attacks | bishop_attacks | rook_attacks | queen_attacks;
}

Bitboard moves::black_attacks(Bitboard occupancy, Board board) {
  // view the board from the perspective of black to generate black's attacks
  using namespace colors;
  Bitboard b_occupancy = bitboard::flip_vertically(occupancy);
  Board b_board;
  for (Piece piece: pieces::values) {
    b_board[white][piece] = bitboard::flip_vertically(board[black][piece]);
    b_board[black][piece] = bitboard::flip_vertically(board[white][piece]);
  }
  Bitboard b_attacks = all_attacks(b_occupancy, b_board);
  return bitboard::flip_vertically(b_attacks);
}

enum Relativity {
  absolute=0, relative = 1
};

// Generate moves from the set of targets.  `source' is the index of the source square, either
// relative to the target or absolute.  The moves are added to the `moves' vector.
void moves_from_targets(std::vector<Move>& moves, const Bitboard& targets, const int& source, const Relativity& relative, const Move::Type& type) {
  bitboard::for_each_member(targets, [&moves, &relative, &source, &type](const squares::Index& target) {
      moves.push_back(Move(relative*target + source, target, type));
    });
}

// Generate normal and capturing moves from the set of attacks.
void moves_from_attacks(std::vector<Move>& moves, Bitboard attacks,
                               Bitboard us, Bitboard them,
                               int source, Relativity relativity) {
  attacks &= ~us;
  moves_from_targets(moves, attacks & ~them, source, relativity, Move::Type::normal);
  moves_from_targets(moves, attacks &  them, source, relativity, Move::Type::capture);
}


void moves::pawn(std::vector<Move>& moves, Bitboard pawn, Bitboard us, Bitboard them, Bitboard en_passant_square) {
  Bitboard flat_occupancy = us | them;

  // single push
  Bitboard single_push_targets = (pawn << vertical) & ~flat_occupancy;
  moves_from_targets(moves, single_push_targets &~ ranks::_8, south, relative, Move::Type::normal);
  Bitboard promotion_targets = single_push_targets & ranks::_8;
  if (promotion_targets != 0) {
    for (Move::Type promotion: {Move::Type::promotion_knight, Move::Type::promotion_bishop,
                                Move::Type::promotion_rook,   Move::Type::promotion_queen}) {
      moves_from_targets(moves, promotion_targets, south, relative, promotion);
    }
  }

  // double push
  Bitboard double_push_targets = ((single_push_targets & ranks::_3) << vertical) & ~flat_occupancy;
  moves_from_targets(moves, double_push_targets, 2*south, relative, Move::Type::double_push);

  // captures
  // TODO: dry up
  Bitboard capture_targets = pawn_attacks_w(pawn) & (them | en_passant_square);
  moves_from_targets(moves, capture_targets &~ ranks::_8, south + east, relative, Move::Type::capture);
  promotion_targets = capture_targets & ranks::_8;
  if (promotion_targets != 0) {
    for (Move::Type promotion: {Move::Type::capturing_promotion_knight, Move::Type::capturing_promotion_bishop,
                                Move::Type::capturing_promotion_rook,   Move::Type::capturing_promotion_queen}) {
      moves_from_targets(moves, promotion_targets, south + east, relative, promotion);
    }
  }

  capture_targets = pawn_attacks_e(pawn) & (them | en_passant_square);
  moves_from_targets(moves, capture_targets &~ ranks::_8, south + west, relative, Move::Type::capture);
  promotion_targets = capture_targets & ranks::_8;
  if (promotion_targets != 0) {
    for (Move::Type promotion: {Move::Type::capturing_promotion_knight, Move::Type::capturing_promotion_bishop,
                                Move::Type::capturing_promotion_rook,   Move::Type::capturing_promotion_queen}) {
      moves_from_targets(moves, promotion_targets, south + west, relative, promotion);
    }
  }
}

void moves::knight(std::vector<Move>& moves, Bitboard knight, Bitboard us, Bitboard them, Bitboard en_passant_square) {
  for (KnightAttackType ka: get_knight_attack_types()) {
    moves_from_attacks(moves, knight_attacks(knight, ka.leftshift, ka.rightshift, ka.badtargets),
                       us, them, -ka.leftshift + ka.rightshift, relative);
  }
}

void moves::bishop(std::vector<Move>& moves, Bitboard bishop, Bitboard us, Bitboard them, Bitboard en_passant_square) {
  Bitboard flat_occupancy = us | them;
  bitboard::for_each_member(bishop, [&moves, us, them, flat_occupancy](const squares::Index& source) {
      moves_from_attacks(moves, bishop_attacks(flat_occupancy, source), us, them, source, absolute);
    });
}

void moves::rook(std::vector<Move>& moves, Bitboard rook, Bitboard us, Bitboard them, Bitboard en_passant_square) {
  Bitboard flat_occupancy = us | them;
  bitboard::for_each_member(rook, [&moves, us, them, flat_occupancy](squares::Index source) {
      moves_from_attacks(moves, rook_attacks(flat_occupancy, source), us, them, source, absolute);
    });
}

void moves::queen(std::vector<Move>& moves, Bitboard queen, Bitboard us, Bitboard them, Bitboard en_passant_square) {
  Bitboard flat_occupancy = us | them;
  bitboard::for_each_member(queen, [&moves, us, them, flat_occupancy](squares::Index source) {
      moves_from_attacks(moves, queen_attacks(flat_occupancy, source), us, them, source, absolute);
    });
}

void moves::king(std::vector<Move>& moves, Bitboard king, Bitboard us, Bitboard them, Bitboard en_passant_square) {
  moves_from_attacks(moves, king_attacks(king), us, them, squares::index_from_bitboard(king), absolute);
}

// NOTE: can_castle_{king,queen}side convey that castling rights have not been lost,
// i.e., the king and the relevant rook have not moved
void moves::castle(std::vector<Move>& moves, Bitboard occupancy, Board board,
                   bool can_castle_kingside, bool can_castle_queenside) {
  using namespace squares;
  Bitboard attacks = black_attacks(occupancy, board);
  if (can_castle_kingside &&
      !(attacks & (e1 | f1 | g1)) &&
      !(occupancy & (f1 | g1)))
    moves.push_back(Move(e1.index, g1.index, Move::Type::castle_kingside));
  if (can_castle_queenside &&
      !(attacks & (e1 | d1 | c1 | b1)) &&
      !(occupancy & (d1 | c1 | b1)))
    moves.push_back(Move(e1.index, c1.index, Move::Type::castle_queenside));
}

typedef std::function<void(std::vector<Move>& moves, Bitboard piece, Bitboard us, Bitboard them, Bitboard en_passant_square)> MoveGenerator;

const auto move_generators_by_piece = []() {
  std::vector<MoveGenerator> move_generators = {
    [pieces::pawn]   = moves::pawn,
    [pieces::knight] = moves::knight,
    [pieces::bishop] = moves::bishop,
    [pieces::rook]   = moves::rook,
    [pieces::queen]  = moves::queen,
    [pieces::king]   = moves::king,
  };
  return move_generators;
}();

void moves::piece_moves(std::vector<Move>& moves, Piece piece, Board board, Occupancy occupancy, Bitboard en_passant_square) {
  Color us = colors::white, them = colors::black;
  move_generators_by_piece[piece](moves, board[us][piece], occupancy[us], occupancy[them], en_passant_square);
}

void moves::all_moves(std::vector<Move>& moves, Board board, Occupancy occupancy, Bitboard en_passant_square,
                      bool can_castle_kingside, bool can_castle_queenside) {
  Color us = colors::white, them = colors::black;
  for (Piece piece: pieces::values)
    move_generators_by_piece[piece](moves, board[us][piece], occupancy[us], occupancy[them], en_passant_square);
  castle(moves, occupancy[us] | occupancy[them], board,
         can_castle_kingside, can_castle_queenside);
}

std::ostream& operator<<(std::ostream& o, const Move& m) {
  o << "Move(" << squares::partition[m.from()].name <<
       "->" << squares::partition[m.to()].name <<
       "; " << Move::typename_from_type(m.type()) <<
       ")";
  return o;
}
