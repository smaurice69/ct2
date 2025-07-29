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

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
