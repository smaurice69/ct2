#include "board.h"
#include "bitops.h"
#include <gtest/gtest.h>

using namespace ct2;

TEST(BoardTest, LoadFEN) {
    Board b;
    ASSERT_TRUE(b.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"));
    EXPECT_EQ(b.occupancyBB(WHITE), 0xFFFFULL);
    EXPECT_TRUE(b.occupancyBB() != 0);
}

TEST(MagicTest, RookAttacks) {
    init_magics();
    uint64_t occ = 0;
    uint64_t attacks = rook_attacks(27, occ); // d4
    EXPECT_EQ(popcount64(attacks), 14);
}

static int sq_from_str(const std::string& s) {
    return (s[1]-'1')*8 + (s[0]-'a');
}

static Board::Move parse_simple(const std::string& m, const Board& b) {
    Board::Move mv{};
    mv.from = sq_from_str(m.substr(0,2));
    mv.to = sq_from_str(m.substr(2,2));
    mv.piece = PIECE_NB;
    for(int p=WP;p<PIECE_NB;++p)
        if(b.pieceBB((Piece)p) & (1ULL<<mv.from)) mv.piece=(Piece)p;
    mv.capture = PIECE_NB;
    for(int p=WP;p<PIECE_NB;++p)
        if(b.pieceBB((Piece)p) & (1ULL<<mv.to)) mv.capture=(Piece)p;
    mv.promotion = PIECE_NB;
    mv.is_ep = false;
    mv.is_castling = false;
    return mv;
}

TEST(MoveGeneration, RegressionNoA5H3) {
    init_tables();
    Board b;
    ASSERT_TRUE(b.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"));
    std::vector<std::string> moves = {"e2e4","c7c5","b1c3","a7a5","g1f3","g8f6","e4e5","f6g4","h2h3"};
    for(const auto& m : moves) {
        auto mv = parse_simple(m, b);
        b.make_move(mv);
    }
    auto legal = b.generate_legal_moves();
    int from = sq_from_str("a5");
    int to = sq_from_str("h3");
    for(const auto& mv : legal) {
        ASSERT_FALSE(mv.from == from && mv.to == to);
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
