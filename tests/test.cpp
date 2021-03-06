#include <iostream>
#include <algorithm>
#include <chrono>

#define BOOST_TEST_MODULE test
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/format.hpp>
#include <boost/random.hpp>
#include <boost/optional/optional_io.hpp>

#include "../util.hpp"
#include "../direction.hpp"
#include "../state.hpp"
#include "../move_generation.hpp"
#include "../hash.hpp"
#include "../mcts.hpp"
#include "../mcts_agent.hpp"
#include "../notation.hpp"
#include "../targets.hpp"

// NOTE: evaluates arguments twice
#define BOOST_CHECK_BITBOARDS_EQUAL(a, b) \
  BOOST_CHECK_MESSAGE((a) == (b), boost::format("%|1$#x| != %|2$#x|") % (a) % (b));

BOOST_AUTO_TEST_CASE(randompoop) {
  hashes::generate_random_feature();
}

BOOST_AUTO_TEST_CASE(partitions) {
  using namespace squares::bitboards;
  BOOST_CHECK_EQUAL(bitboard::cardinality(0x1), 1);
  BOOST_CHECK_EQUAL(bitboard::cardinality(0x400000), 1);
  BOOST_CHECK_BITBOARDS_EQUAL(a1, 0x1);
  BOOST_CHECK_BITBOARDS_EQUAL(h8, 0x8000000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(e4, 0x0000000010000000);
  BOOST_CHECK_BITBOARDS_EQUAL(d4 | e4 | f4 | c5, 0x0000000438000000);
  BOOST_CHECK_BITBOARDS_EQUAL(files::bitboards::a,  a1 | a2 | a3 | a4 | a5 | a6 | a7 | a8);
  BOOST_CHECK_BITBOARDS_EQUAL(ranks::bitboards::_8, a8 | b8 | c8 | d8 | e8 | f8 | g8 | h8);
  BOOST_CHECK_BITBOARDS_EQUAL(giadonals::bitboards::a1h8, a1 | b2 | c3 | d4 | e5 | f6 | g7 | h8);
  BOOST_CHECK_BITBOARDS_EQUAL(diagonals::bitboards::a8h1, a8 | b7 | c6 | d5 | e4 | f3 | g2 | h1);
}

BOOST_AUTO_TEST_CASE(in_between) {
  using namespace squares::bitboards;
  BOOST_CHECK_BITBOARDS_EQUAL(squares::in_between(squares::a1, squares::g7), b2 | c3 | d4 | e5 | f6);
  BOOST_CHECK_BITBOARDS_EQUAL(squares::in_between(squares::a1, squares::h7), 0);
  BOOST_CHECK_BITBOARDS_EQUAL(squares::in_between(squares::c3, squares::f3), d3 | e3);
  BOOST_CHECK_BITBOARDS_EQUAL(squares::in_between(squares::b6, squares::f2), c5 | d4 | e3);
  BOOST_CHECK_BITBOARDS_EQUAL(squares::in_between(squares::b6, squares::f1), 0);
  BOOST_CHECK_BITBOARDS_EQUAL(squares::in_between(squares::b6, squares::b3), b5 | b4);
}

BOOST_AUTO_TEST_CASE(initial_moves) {
  State state;

  std::set<Move> expected_moves;
  using namespace directions;
  squares::for_each(ranks::bitboards::_2, [&expected_moves](squares::Index from) {
      expected_moves.emplace(from, from +   north, move_types::normal);
      expected_moves.emplace(from, from + 2*north, move_types::double_push);
    });
  squares::for_each(squares::bitboards::b1 | squares::bitboards::g1, [&expected_moves](squares::Index from) {
      expected_moves.emplace(from, from + 2*north + west, move_types::normal);
      expected_moves.emplace(from, from + 2*north + east, move_types::normal);
    });

  const std::vector<Move> moves = moves::moves(state);
  const std::set<Move> actual_moves(moves.begin(), moves.end());

  std::set<Move> falsenegatives, falsepositives;
  std::set_difference(expected_moves.begin(), expected_moves.end(),
                      actual_moves.begin(), actual_moves.end(),
                      std::inserter(falsenegatives, falsenegatives.begin()));
  std::set_difference(actual_moves.begin(), actual_moves.end(),
                      expected_moves.begin(), expected_moves.end(),
                      std::inserter(falsepositives, falsepositives.begin()));
  BOOST_CHECK_MESSAGE(falsenegatives.empty(), "legal moves not generated: " << falsenegatives);
  BOOST_CHECK_MESSAGE(falsepositives.empty(), "illegal moves generated: " << falsepositives);
}

BOOST_AUTO_TEST_CASE(rays_every_which_way) {
  squares::Index bishop_square = squares::f5, rook_square = squares::c3; 

  Bitboard diagonal = diagonals::bitboards::by_square(bishop_square);
  Bitboard giadonal = giadonals::bitboards::by_square(bishop_square);
  ranks::Index rank = ranks::by_square(rook_square);
  Bitboard file = files::bitboards::by_square(rook_square);

  Bitboard bishop_board = squares::bitboard(bishop_square);
  Bitboard rook_board = squares::bitboard(rook_square);

  Bitboard bda = targets::slides(bishop_board, bishop_board, diagonal & ~bishop_board);
  Bitboard bga = targets::slides(bishop_board, bishop_board, giadonal & ~bishop_board);
  Bitboard rra = targets::slides_rank(rook_board, rook_board, rank);
  Bitboard rfa = targets::slides(rook_board, rook_board, file & ~rook_board);

  BOOST_CHECK_BITBOARDS_EQUAL(bda, 0x0408100040800000);
  BOOST_CHECK_BITBOARDS_EQUAL(bga, 0x0080400010080402);
  BOOST_CHECK_BITBOARDS_EQUAL(rra, 0x0000000000fb0000);
  BOOST_CHECK_BITBOARDS_EQUAL(rfa, 0x0404040404000404);

  State state("8/8/8/5B2/8/2R5/8/8 w - - 0 1");
  BOOST_CHECK_BITBOARDS_EQUAL(targets::bishop_attacks(bishop_square, state.flat_occupancy),
                              0x0488500050880402);
  BOOST_CHECK_BITBOARDS_EQUAL(targets::rook_attacks(rook_square, state.flat_occupancy),
                              0x0404040404fb0404);
}

BOOST_AUTO_TEST_CASE(various_moves) {
  State state("r1b2rk1/pp1P1p1p/q1p2n2/2N2PpB/1NP2bP1/2R1B3/PP2Q2P/R3K3 w Q g6 0 1");

  {
    using namespace colors;
    using namespace pieces;
    using namespace castles;
    using namespace squares::bitboards;
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][pawn],   a2 | b2 | c4 | d7 | f5 | g4 | h2);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][knight], b4 | c5);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][bishop], e3 | h5);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][rook],   a1 | c3);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][queen],  e2);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][king],   e1);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][pawn],   a7 | b7 | c6 | f7 | g5 | h7);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][knight], f6);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][bishop], c8 | f4);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][rook],   a8 | f8);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][queen],  a6);
    BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][king],   g8);
    BOOST_CHECK_BITBOARDS_EQUAL(state.en_passant_square,    g6);
    BOOST_CHECK_BITBOARDS_EQUAL(state.their_attacks, 0xfeef5fdbf5518100);
    BOOST_CHECK(!state.castling_rights[white][kingside]);
    BOOST_CHECK(!state.castling_rights[black][kingside]);
    BOOST_CHECK( state.castling_rights[white][queenside]);
    BOOST_CHECK(!state.castling_rights[black][queenside]);
    BOOST_CHECK_EQUAL(state.us, white);
  }

  BOOST_CHECK_BITBOARDS_EQUAL(targets::rook_attacks(squares::c3, state.flat_occupancy),
                              0x00000000041b0404);

  BOOST_CHECK_BITBOARDS_EQUAL(targets::king_attacks(state.board[colors::white][pieces::king]),
                              0x0000000000003828);

  std::set<Move> expected_moves;

  {
    using namespace squares;
    // a1 rook
    for (squares::Index target: {b1, c1, d1})
      expected_moves.emplace(Move(a1, target, move_types::normal));
  
    // e1 king
    for (squares::Index target: {d1, f1, d2, f2})
      expected_moves.emplace(Move(e1, target, move_types::normal));
    expected_moves.emplace(Move(e1, c1, move_types::castle_queenside));
  
    // a2 pawn
    expected_moves.emplace(Move(a2, a3, move_types::normal));
    expected_moves.emplace(Move(a2, a4, move_types::double_push));
  
    // b2 pawn
    expected_moves.emplace(Move(b2, b3, move_types::normal));
  
    // e2 queen
    for (squares::Index target: {f1, f2, g2, f3, d3, d2, c2, d1})
      expected_moves.emplace(Move(e2, target, move_types::normal));
  
    // h2 pawn
    expected_moves.emplace(Move(h2, h3, move_types::normal));
    expected_moves.emplace(Move(h2, h4, move_types::double_push));
  
    // c3 rook
    for (squares::Index target: {c2, c1, d3, b3, a3})
      expected_moves.emplace(Move(c3, target, move_types::normal));
    
    // e3 bishop
    for (squares::Index target: {f2, g1, d4, d2, c1})
      expected_moves.emplace(Move(e3, target, move_types::normal));
    expected_moves.emplace(Move(e3, f4, move_types::capture));
  
    // b4 knight
    for (squares::Index target: {d5, d3, c2})
      expected_moves.emplace(Move(b4, target, move_types::normal));
    for (squares::Index target: {a6, c6})
      expected_moves.emplace(Move(b4, target, move_types::capture));
  
    // c4 pawn
  
    // g4 pawn
  
    // c5 knight
    for (squares::Index target: {e6, e4, d3, b3, a4})
      expected_moves.emplace(Move(c5, target, move_types::normal));
    for (squares::Index target: {a6, b7})
      expected_moves.emplace(Move(c5, target, move_types::capture));
  
    // f5 pawn
    expected_moves.emplace(Move(f5, g6, move_types::capture));
  
    // h5 bishop
    expected_moves.emplace(Move(h5, g6, move_types::normal));
    expected_moves.emplace(Move(h5, f7, move_types::capture));
  
    // d7 pawn
    for (MoveType type: {move_types::promotion_knight,
                         move_types::promotion_bishop,
                         move_types::promotion_rook,
                         move_types::promotion_queen}) {
      expected_moves.emplace(Move(d7, d8, type));
    }
    for (MoveType type: {move_types::capturing_promotion_knight,
                         move_types::capturing_promotion_bishop,
                         move_types::capturing_promotion_rook,
                         move_types::capturing_promotion_queen}) {
      expected_moves.emplace(Move(d7, c8, type));
    }
  }

  const std::vector<Move> moves = moves::moves(state);
  const std::set<Move> actual_moves(moves.begin(), moves.end());

  std::set<Move> falsenegatives, falsepositives;
  std::set_difference(expected_moves.begin(), expected_moves.end(),
                      actual_moves.begin(), actual_moves.end(),
                      std::inserter(falsenegatives, falsenegatives.begin()));
  std::set_difference(actual_moves.begin(), actual_moves.end(),
                      expected_moves.begin(), expected_moves.end(),
                      std::inserter(falsepositives, falsepositives.begin()));
  BOOST_CHECK_MESSAGE(falsenegatives.empty(), "legal moves not generated: " << falsenegatives);
  BOOST_CHECK_MESSAGE(falsepositives.empty(), "illegal moves generated: " << falsepositives);
}

BOOST_AUTO_TEST_CASE(algebraic_moves) {
  using namespace colors;
  using namespace pieces;
  using namespace squares;

  State state;

  for (std::string word: words("e4 e5 Nf3 Nc6 Bc4 Bc5 b4 Bxb4 c3 Ba5 d4 exd4 0-0 d3 Qb3 Qf6")) {
    Move move = notation::algebraic::parse(word, state);
    state.make_move(move);
  }

  BOOST_CHECK_BITBOARDS_EQUAL(state.occupancy[white], 0x000000001426e167);
  BOOST_CHECK_BITBOARDS_EQUAL(state.occupancy[black], 0xd5ef240100080000);

  for (std::string word: words("e5 Qg6 Re1 Nge7 Ba3 b5 Qxb5 Rb8 Qa4 Bb6 Nbd2 Bb7 Ne4 Qf5 "
                               "Bxd3 Qh5 Nf6+ gxf6 exf6 Rg8 Rad1 Qxf3 Rxe7+ Nxe7 Qxd7+ "
                               "Kxd7 Bf5+ Ke8 Bd7+ Kf8")) {
    Move move = notation::algebraic::parse(word, state);
    state.make_move(move);
  }

  BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][pawn],   0x000020000004e100);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][knight], 0x0000000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][bishop], 0x0008000000010000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][rook],   0x0000000000000008);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][queen],  0x0000000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[white][king],   0x0000000000000040);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][pawn],   0x00a5000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][knight], 0x0010000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][bishop], 0x0002020000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][rook],   0x4200000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][queen],  0x0000000000200000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.board[black][king],   0x2000000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.en_passant_square,    0x0000000000000000);
  BOOST_CHECK_BITBOARDS_EQUAL(state.their_attacks,        0xfd777fed78fc7008);
  BOOST_CHECK_BITBOARDS_EQUAL(state.occupancy[white],     0x000820000005e148);
  BOOST_CHECK_BITBOARDS_EQUAL(state.occupancy[black],     0x62b7020000200000);
  BOOST_CHECK_EQUAL(state.us, white);

  std::set<Move> expected_moves;
#define MV(from, to, type) expected_moves.emplace(from, to, type);
  MV(c3, c4, move_types::normal);
  MV(g2, g3, move_types::normal);
  MV(g2, f3, move_types::capture); // leaves king in check
  MV(g2, g4, move_types::double_push);
  MV(h2, h3, move_types::normal);
  MV(h2, h4, move_types::double_push);
  for (squares::Index target: {a4, b5, c6, e8, c8, e6, f5, g4, h3})
    MV(d7, target, move_types::normal);
  for (squares::Index target: {c1, b2, b4, c5, d6})
    MV(a3, target, move_types::normal);
  MV(a3, e7, move_types::capture);
  for (squares::Index target: {a1, b1, c1, e1, f1, d2, d3, d4, d5, d6})
    MV(d1, target, move_types::normal);
  MV(f6, e7, move_types::capture);
  MV(g1, f1, move_types::normal);
  MV(g1, h1, move_types::normal);
#undef MV

  std::vector<Move> moves = moves::moves(state);
  std::set<Move> actual_moves(moves.begin(), moves.end());

  std::set<Move> falsenegatives, falsepositives;
  std::set_difference(expected_moves.begin(), expected_moves.end(),
                      actual_moves.begin(), actual_moves.end(),
                      std::inserter(falsenegatives, falsenegatives.begin()));
  std::set_difference(actual_moves.begin(), actual_moves.end(),
                      expected_moves.begin(), expected_moves.end(),
                      std::inserter(falsepositives, falsepositives.begin()));
  BOOST_CHECK_MESSAGE(falsenegatives.empty(), "legal moves not generated: " << falsenegatives);
  BOOST_CHECK_MESSAGE(falsepositives.empty(), "illegal moves generated: " << falsepositives);
}

BOOST_AUTO_TEST_CASE(move_randomly) {
  State state;
  boost::mt19937 generator;
  for (int i = 0; i < 100; i++) {
    boost::optional<Move> move = moves::random_move(state, generator);
    if (!move)
      break;
    state.make_move(*move);
    state.require_consistent();
  }
}

BOOST_AUTO_TEST_CASE(regression2) {
  State state("rnbqk1nr/1ppp2pp/5p2/p3p3/1b1PP3/8/PPPQNPPP/RNB1KB1R w KQkq a6 0 0");
  for (std::string word: words("d4e5 b7b6 c2c4 a8a7 e2g3 f6e5 d2c3 b4f8 e1d2 d8e7 d2c2 c7c6 a2a3 e7e6 f2f3 f8e7 c1d2 c8a6 b2b4 e6h6 h2h3 a6c8 c2b3 h6e3 c3d3 e7f6 b4a5 f6e7 b3a4 e3f2 g3f5 f2c5 d3e2 c5f2 f5h6 f2g2 h3h4 e7h4 d2e1 a7a8 a5a6 b8a6 c4c5 b6b5 e2b5 a6b8 a4b3 a8a3 b3c4 h4g5 f1d3 g5h6 d3c2 d7d5")) {
    Move move = notation::coordinate::parse(word, state);
    state.make_move(move);
  }

  // this used to leave king in check; it's an en passant capture but not
  // processed as such because State::make_move updated the en_passant_square
  // before handling the capture.
  state.make_move(notation::coordinate::parse("c5d6", state));
}

BOOST_AUTO_TEST_CASE(regression3) {
  State state("rnbqkbnr/1ppppppp/p7/8/8/P2P4/1PP1PPPP/RNBQKBNR b KQkq - 0 0");
  State previous_state;
  for (std::string word: words("a6a5 c1g5 h7h5 g5c1 h5h4 b2b3 f7f5 c1d2 g8f6 c2c3 b7b5 g2g4 d7d6 a3a4 a8a7 a4b5 c8d7 g1f3 d7c6 b1a3 a7a6 g4g5 d6d5 a3c2 c6b7 d3d4 b8d7 e2e4 f6g4 h2h3 g4f2 c3c4 c7c6 c2b4 e7e6 b4d3 f8b4 a1a4 d5e4 b5a6 e4f3 d4d5 c6d5 a4a1 d7f8 a6b7 f2h1")) {
    previous_state = state;
    Move move = notation::coordinate::parse(word, state);
    state.make_move(move);
    state.require_consistent();
    // the final move, f2h1, is a rook capture that should cost white the
    // right to castle kingside.
  }
}

BOOST_AUTO_TEST_CASE(king_capture) {
  using namespace squares;

  State state("8/5B2/8/Q1pk4/8/8/PPP5/6K1 b - - 0 0");

  std::set<Move> expected_moves;
#define MV(from, to, type) expected_moves.emplace(from, to, type);
  MV(d5, e5, move_types::normal);
  MV(d5, d6, move_types::normal);
  MV(d5, c6, move_types::normal);
  MV(d5, c4, move_types::normal); // leaves king in check
  MV(d5, d4, move_types::normal);
  MV(d5, e4, move_types::normal);
#undef MV

  std::vector<Move> moves = moves::moves(state);
  std::set<Move> actual_moves(moves.begin(), moves.end());

  std::set<Move> falsenegatives, falsepositives;
  std::set_difference(expected_moves.begin(), expected_moves.end(),
                      actual_moves.begin(), actual_moves.end(),
                      std::inserter(falsenegatives, falsenegatives.begin()));
  std::set_difference(actual_moves.begin(), actual_moves.end(),
                      expected_moves.begin(), expected_moves.end(),
                      std::inserter(falsepositives, falsepositives.begin()));
  BOOST_REQUIRE_MESSAGE(falsenegatives.empty(), "legal moves not generated: " << falsenegatives);
  BOOST_REQUIRE_MESSAGE(falsepositives.empty(), "illegal moves generated: " << falsepositives);

  state.make_move(Move(d5, c4, move_types::normal)); // leaves king in check

  BOOST_REQUIRE(state.their_king_attacked());

  moves = moves::moves(state);
  BOOST_REQUIRE_MESSAGE(std::all_of(std::begin(moves), std::end(moves), [&](Move const& move) {
        return squares::bitboard(move.target()) & state.board[state.them][pieces::king];
      }),
    "king capture not forced, state: " << state << " has non-king-capture move in " << moves);

  state.make_move(moves[0]);

  moves = moves::moves(state);
  BOOST_CHECK_MESSAGE(moves.empty(), "after king capture, state: " << state << " still has moves: " << moves);

  BOOST_CHECK_EQUAL(state.winner(), colors::white);
}

BOOST_AUTO_TEST_CASE(unmake_move) {
  State state;
  boost::mt19937 generator;
  for (int i = 0; i < 100; i++) {
    boost::optional<Move> move = moves::random_move(state, generator);
    if (!move)
      break;
    State state2(state);
    Undo undo = state2.make_move(*move);
    state2.unmake_move(undo);
    BOOST_CHECK_EQUAL(state, state2);
    state.make_move(*move);
  }
}

BOOST_AUTO_TEST_CASE(mcts_agent) {
  State state;
  MCTSAgent agent(2);
  agent.set_state(state);
  auto decision = agent.start_decision(5);
  Move move = decision.get();
  agent.advance_state(move);
  decision = agent.start_decision(5);
  decision.get();
}

// better than the other.
BOOST_AUTO_TEST_CASE(mcts_agent_certain_win) {
  // observed in testing; black has two moves, Kxh4 and g5.  after g5,
  // Qxg5 mates but is estimated to have ~2/3 winrate.  are there that
  // many draws due to 50-move rule when simulating with pseudolegal moves,
  // or is something else going on?
  State state("rn4nr/p4N1p/6p1/1p1Q3k/1Pp4P/8/PP1PPP1P/RNB1KBR1 b Q - 0 0");
  MCTSAgent agent(2);
  agent.set_state(state);
  auto decision = agent.start_decision(5);
  decision.get();
  agent.advance_state(Move(squares::g6, squares::g5, move_types::normal));
  decision = agent.start_decision(5);
  decision.get();
}

BOOST_AUTO_TEST_CASE(serialize_mcts_agent) {
  MCTSAgent agent(2);
  agent.set_state(State());
  {
    auto decision = agent.start_decision(1);
    Move move = decision.get();
    agent.advance_state(move);
  }
  agent.save_yourself("/tmp/serialized_mcts_agent");
  agent.load_yourself("/tmp/serialized_mcts_agent");
  {
    auto decision = agent.start_decision(1);
    Move move = decision.get();
    agent.advance_state(move);
  }
}

BOOST_AUTO_TEST_CASE(mcts_endgame_graphviz) {
  return;
  State state("r1bk3r/p2p1pNp/n2B1n2/1p1NP2P/6P1/3P4/P1P1K3/q5b1 w - - 0 23");
  boost::mt19937 generator;
  mcts::Graph graph;
  for (int i = 0; i < 1e4; i++)
    graph.sample(state, generator);
  std::cout << "mcts results for state: " << std::endl;
  std::cout << state << std::endl;
  std::cout << "candidate moves: " << std::endl;
  graph.print_statistics(std::cout, state);
  std::cout << "principal variation: " << std::endl;
  graph.print_principal_variation(std::cout, state);
  //std::cout << "digraph G {" << std::endl;
  //graph.graphviz(std::cout, state);
  //std::cout << "}" << std::endl;
}
