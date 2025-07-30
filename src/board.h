#ifndef CT2_BOARD_H
#define CT2_BOARD_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ct2 {

enum Piece {
    WP, WN, WB, WR, WQ, WK,
    BP, BN, BB, BR, BQ, BK,
    PIECE_NB
};

enum Color { WHITE, BLACK, COLOR_NB };

struct Magic {
    uint64_t mask;
    uint64_t magic;
    int shift;
    std::vector<uint64_t> attacks;
};

class Board {
public:
    Board();
    bool loadFEN(const std::string& fen);
    std::string getFEN() const;

    struct Move {
        int from;
        int to;
        Piece piece;
        Piece capture;
        Piece promotion;
        bool is_ep;
        bool is_castling;
    };

    std::vector<Move> generate_moves() const;
    std::vector<Move> generate_legal_moves() const;
    bool square_attacked(int sq, Color by) const;
    bool in_check(Color c) const;
    bool make_move(const Move& m);

    uint64_t pieceBB(Piece p) const { return bitboards[p]; }
    uint64_t occupancyBB(Color c) const { return occupancies[c]; }
    uint64_t occupancyBB() const { return occupancies[2]; }
    Color side_to_move() const { return side; }

private:
    std::array<uint64_t, PIECE_NB> bitboards{};
    std::array<uint64_t, 3> occupancies{}; // white, black, both
    Color side;
    uint8_t castling; // KQkq = 1|2|4|8
    int ep_square;    // -1 if none

    void update_occupancies();
};

// Magic bitboard related
void init_magics();
void init_tables();
uint64_t bishop_attacks(int sq, uint64_t occ);
uint64_t rook_attacks(int sq, uint64_t occ);

} // namespace ct2

#endif // CT2_BOARD_H
