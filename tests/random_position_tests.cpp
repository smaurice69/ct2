#include "board.h"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>

using namespace ct2;

TEST(MoveGeneration, StartPositionMoves) {
    init_tables();
    Board b;
    ASSERT_TRUE(b.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"));
    auto moves = b.generate_moves();
    EXPECT_EQ(moves.size(), 20u);
}

TEST(MoveGeneration, RandomPositions) {
    init_tables();
    std::ifstream in("tests/random_positions.txt");
    ASSERT_TRUE(in.is_open());
    std::string boardPart, side, castling, ep;
    int halfmove, fullmove;
    int count = 0;
    while (in >> boardPart >> side >> castling >> ep >> halfmove >> fullmove) {
        std::stringstream fen;
        fen << boardPart << ' ' << side << ' ' << castling << ' ' << ep << ' '
            << halfmove << ' ' << fullmove;
        Board b;
        ASSERT_TRUE(b.loadFEN(fen.str())) << fen.str();
        auto moves = b.generate_moves();
        for(const auto& mv : moves) {
            // piece must exist on from square
            EXPECT_TRUE(b.pieceBB(mv.piece) & (1ULL<<mv.from)) << fen.str();
            // destination must be empty or contain capture target
            bool dest_occ = b.occupancyBB() & (1ULL<<mv.to);
            if(dest_occ)
                EXPECT_NE(mv.capture, PIECE_NB) << fen.str();
            if(mv.piece==WR||mv.piece==BR||mv.piece==WB||mv.piece==BB||mv.piece==WQ||mv.piece==BQ) {
                int fr=mv.from/8, ff=mv.from%8, tr=mv.to/8, tf=mv.to%8;
                int dr=(tr>fr)-(tr<fr);
                int df=(tf>ff)-(tf<ff);
                int r=fr+dr, f=ff+df;
                while(r!=tr || f!=tf) {
                    EXPECT_FALSE(b.occupancyBB() & (1ULL<<(r*8+f))) << fen.str();
                    r+=dr; f+=df;
                }
            }
        }
        ++count;
    }
    EXPECT_GE(count, 200);
}

