#include "board.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>

namespace ct2 {

namespace {

inline int char_to_piece(char c) {
    switch (c) {
    case 'P': return WP;
    case 'N': return WN;
    case 'B': return WB;
    case 'R': return WR;
    case 'Q': return WQ;
    case 'K': return WK;
    case 'p': return BP;
    case 'n': return BN;
    case 'b': return BB;
    case 'r': return BR;
    case 'q': return BQ;
    case 'k': return BK;
    default: return -1;
    }
}

} // namespace

Board::Board() {
    bitboards.fill(0);
    occupancies.fill(0);
    side = WHITE;
}

void Board::update_occupancies() {
    occupancies[WHITE] = bitboards[WP] | bitboards[WN] | bitboards[WB] |
                         bitboards[WR] | bitboards[WQ] | bitboards[WK];
    occupancies[BLACK] = bitboards[BP] | bitboards[BN] | bitboards[BB] |
                         bitboards[BR] | bitboards[BQ] | bitboards[BK];
    occupancies[2] = occupancies[WHITE] | occupancies[BLACK];
}

bool Board::loadFEN(const std::string& fen) {
    bitboards.fill(0);
    occupancies.fill(0);
    side = WHITE;

    std::istringstream iss(fen);
    std::string boardPart, sidePart, castling, ep;
    if (!(iss >> boardPart >> sidePart >> castling >> ep))
        return false;

    int sq = 56; // start from A8
    for (char c : boardPart) {
        if (c == '/') {
            sq -= 16; // move to next rank
        } else if (c >= '1' && c <= '8') {
            sq += c - '0';
        } else {
            int p = char_to_piece(c);
            if (p < 0) return false;
            bitboards[p] |= 1ULL << sq;
            sq++;
        }
    }

    side = (sidePart == "w" ? WHITE : BLACK);

    occupancies[WHITE] = bitboards[WP] | bitboards[WN] | bitboards[WB] |
                         bitboards[WR] | bitboards[WQ] | bitboards[WK];
    occupancies[BLACK] = bitboards[BP] | bitboards[BN] | bitboards[BB] |
                         bitboards[BR] | bitboards[BQ] | bitboards[BK];
    occupancies[2] = occupancies[WHITE] | occupancies[BLACK];

    return true;
}

std::string Board::getFEN() const {
    // Only piece placement and side to move, for simplicity
    std::string s;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            int sq = rank * 8 + file;
            bool found = false;
            for (int p = 0; p < PIECE_NB; ++p) {
                if (bitboards[p] & (1ULL << sq)) {
                    if (empty) { s += char('0' + empty); empty = 0; }
                    const char* pieces = "PNBRQKpnbrqk";
                    s += pieces[p];
                    found = true;
                    break;
                }
            }
            if (!found) empty++;
        }
        if (empty) s += char('0' + empty);
        if (rank > 0) s += '/';
    }
    s += side == WHITE ? " w" : " b";
    s += " - - 0 1"; // stub
    return s;
}

// ================= Move generation =====================
static std::array<uint64_t, 64> knightAttacks;
static std::array<uint64_t, 64> kingAttacks;

static int pop_lsb(uint64_t& b) {
    int sq = __builtin_ctzll(b);
    b &= b - 1;
    return sq;
}

std::vector<Board::Move> Board::generate_moves() const {
    std::vector<Move> moves;
    uint64_t own = occupancies[side];
    uint64_t opp = occupancies[side ^ 1];

    auto add_leaper = [&](Piece p, const std::array<uint64_t,64>& table) {
        uint64_t bb = bitboards[p];
        while (bb) {
            int from = pop_lsb(bb);
            uint64_t targets = table[from] & ~own;
            uint64_t t = targets;
            while (t) {
                int to = pop_lsb(t);
                Piece cap = PIECE_NB;
                if (opp & (1ULL<<to))
                    for (int pc=WP; pc<PIECE_NB; ++pc) if (bitboards[pc] & (1ULL<<to)) cap=(Piece)pc;
                moves.push_back({from,to,p,cap});
            }
        }
    };

    auto add_slider = [&](Piece p, bool bishopLike) {
        uint64_t bb = bitboards[p];
        while (bb) {
            int from = pop_lsb(bb);
            uint64_t targets = bishopLike ? bishop_attacks(from, occupancies[2])
                                         : rook_attacks(from, occupancies[2]);
            targets &= ~own;
            uint64_t t = targets;
            while (t) {
                int to = pop_lsb(t);
                Piece cap = PIECE_NB;
                if (opp & (1ULL<<to))
                    for (int pc=WP; pc<PIECE_NB; ++pc) if (bitboards[pc] & (1ULL<<to)) cap=(Piece)pc;
                moves.push_back({from,to,p,cap});
            }
        }
    };

    if (side == WHITE) {
        uint64_t pawns = bitboards[WP];
        uint64_t single = (pawns << 8) & ~occupancies[2];
        uint64_t t = single;
        while (t) {
            int to = pop_lsb(t);
            int from = to - 8;
            moves.push_back({from,to,WP,PIECE_NB});
        }
        uint64_t captL = (pawns << 7) & ~0x0101010101010101ULL & opp;
        t = captL;
        while (t) {
            int to = pop_lsb(t);
            int from = to - 7;
            Piece cap=PIECE_NB; for(int pc=WP;pc<PIECE_NB;++pc) if(bitboards[pc]&(1ULL<<to)) cap=(Piece)pc;
            moves.push_back({from,to,WP,cap});
        }
        uint64_t captR = (pawns << 9) & ~0x8080808080808080ULL & opp;
        t = captR;
        while (t) {
            int to = pop_lsb(t);
            int from = to - 9;
            Piece cap=PIECE_NB; for(int pc=WP;pc<PIECE_NB;++pc) if(bitboards[pc]&(1ULL<<to)) cap=(Piece)pc;
            moves.push_back({from,to,WP,cap});
        }
        add_leaper(WN, knightAttacks);
        add_slider(WB, true);
        add_slider(WR, false);
        add_slider(WQ, true);
        add_slider(WQ, false);
        add_leaper(WK, kingAttacks);
    } else {
        uint64_t pawns = bitboards[BP];
        uint64_t single = (pawns >> 8) & ~occupancies[2];
        uint64_t t = single;
        while (t) {
            int to = pop_lsb(t);
            int from = to + 8;
            moves.push_back({from,to,BP,PIECE_NB});
        }
        uint64_t captL = (pawns >> 7) & ~0x8080808080808080ULL & opp;
        t = captL;
        while (t) {
            int to = pop_lsb(t);
            int from = to + 7;
            Piece cap=PIECE_NB; for(int pc=WP;pc<PIECE_NB;++pc) if(bitboards[pc]&(1ULL<<to)) cap=(Piece)pc;
            moves.push_back({from,to,BP,cap});
        }
        uint64_t captR = (pawns >> 9) & ~0x0101010101010101ULL & opp;
        t = captR;
        while (t) {
            int to = pop_lsb(t);
            int from = to + 9;
            Piece cap=PIECE_NB; for(int pc=WP;pc<PIECE_NB;++pc) if(bitboards[pc]&(1ULL<<to)) cap=(Piece)pc;
            moves.push_back({from,to,BP,cap});
        }
        add_leaper(BN, knightAttacks);
        add_slider(BB, true);
        add_slider(BR, false);
        add_slider(BQ, true);
        add_slider(BQ, false);
        add_leaper(BK, kingAttacks);
    }

    return moves;
}

bool Board::make_move(const Move& m) {
    assert(m.piece >= 0 && m.piece < PIECE_NB);
    assert(m.from >= 0 && m.from < 64);
    assert(m.to >= 0 && m.to < 64);
    uint64_t fromBB = 1ULL << m.from;
    uint64_t toBB = 1ULL << m.to;
    bitboards[m.piece] &= ~fromBB;
    bitboards[m.piece] |= toBB;
    if (m.capture != PIECE_NB)
        bitboards[m.capture] &= ~toBB;
    update_occupancies();
    side = (side == WHITE ? BLACK : WHITE);
    return true;
}

// ================= Magic bitboards =====================

static std::array<Magic, 64> rookMagics;
static std::array<Magic, 64> bishopMagics;

static uint64_t random_uint64() {
    static uint64_t x = 88172645463325252ULL;
    x ^= x << 7;  x ^= x >> 9;
    return x;
}



unsigned int popcount64(unsigned long long x) {
    unsigned int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

static int popcount(uint64_t b) {
    return popcount64(b);
}

static uint64_t knight_mask(int sq) {
    int r = sq / 8, f = sq % 8;
    const int offsets[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    uint64_t mask = 0;
    for(auto& o : offsets){
        int r1 = r + o[0];
        int f1 = f + o[1];
        if(r1>=0 && r1<8 && f1>=0 && f1<8) mask |= 1ULL << (r1*8+f1);
    }
    return mask;
}

static uint64_t king_mask(int sq) {
    int r = sq / 8, f = sq % 8;
    const int offsets[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    uint64_t mask = 0;
    for(auto& o : offsets){
        int r1 = r + o[0];
        int f1 = f + o[1];
        if(r1>=0 && r1<8 && f1>=0 && f1<8) mask |= 1ULL << (r1*8+f1);
    }
    return mask;
}

static uint64_t rook_mask(int sq) {
    int r = sq / 8, f = sq % 8;
    uint64_t mask = 0;
    for (int r1 = r + 1; r1 <= 6; ++r1) mask |= 1ULL << (r1 * 8 + f);
    for (int r1 = r - 1; r1 >= 1; --r1) mask |= 1ULL << (r1 * 8 + f);
    for (int f1 = f + 1; f1 <= 6; ++f1) mask |= 1ULL << (r * 8 + f1);
    for (int f1 = f - 1; f1 >= 1; --f1) mask |= 1ULL << (r * 8 + f1);
    return mask;
}

static uint64_t bishop_mask(int sq) {
    int r = sq / 8, f = sq % 8;
    uint64_t mask = 0;
    for (int r1=r+1,f1=f+1; r1<=6 && f1<=6; ++r1,++f1) mask |= 1ULL << (r1*8+f1);
    for (int r1=r+1,f1=f-1; r1<=6 && f1>=1; ++r1,--f1) mask |= 1ULL << (r1*8+f1);
    for (int r1=r-1,f1=f+1; r1>=1 && f1<=6; --r1,++f1) mask |= 1ULL << (r1*8+f1);
    for (int r1=r-1,f1=f-1; r1>=1 && f1>=1; --r1,--f1) mask |= 1ULL << (r1*8+f1);
    return mask;
}

static uint64_t sliding_attack(bool bishop, int sq, uint64_t occ) {
    uint64_t attacks = 0;
    int r = sq / 8, f = sq % 8;

    static const int rookDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    static const int bishopDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};

    const int (*dirs)[2] = bishop ? bishopDirs : rookDirs;
    int count = 4;
    for (int i=0;i<count;++i) {
        int dr = dirs[i][0];
        int df = dirs[i][1];
        int r1=r+dr, f1=f+df;
        while (r1>=0 && r1<8 && f1>=0 && f1<8) {
            int sq1 = r1*8+f1;
            attacks |= 1ULL<<sq1;
            if (occ & (1ULL<<sq1)) break;
            r1+=dr; f1+=df;
        }
    }
    return attacks;
}

static void init_magic_array(bool bishop, std::array<Magic,64>& magics) {
    for (int sq=0; sq<64; ++sq) {
        Magic m{};
        m.mask = bishop ? bishop_mask(sq) : rook_mask(sq);
        int bits = popcount(m.mask);
        m.shift = 64 - bits;
        size_t size = 1ULL << bits;
        m.attacks.assign(size, 0);

        std::vector<uint64_t> occupancies(size);
        std::vector<uint64_t> references(size);

        uint64_t b=0;
        for (size_t i=0;i<size;++i) {
            occupancies[i]=b;
            references[i]=sliding_attack(bishop,sq,b);
            b=(b-m.mask)&m.mask;
        }

        bool found=false;
        for(int k=0;k<100000 && !found;++k){
            uint64_t magic=random_uint64()&random_uint64()&random_uint64();

            if(popcount((magic*m.mask)>>56)<6) continue;
            std::vector<uint64_t> used(size, 0xFFFFFFFFFFFFFFFFULL);
            bool fail=false;
            for(size_t i=0;i<size && !fail;++i){
                uint64_t idx=(occupancies[i]*magic)>>m.shift;
                if(used[idx]==0xFFFFFFFFFFFFFFFFULL) used[idx]=references[i];
                else if(used[idx]!=references[i]) fail=true;
            }
            if(!fail){
                m.magic=magic;
                for(size_t i=0;i<size;++i){
                    uint64_t idx=(occupancies[i]*magic)>>m.shift;
                    m.attacks[idx]=references[i];
                }
                found=true;
            }
        }
        if(!found){
            // fallback: simple attacks without magic
            for(size_t i=0;i<size;++i){
                m.attacks[i]=references[i];
            }
        }
        magics[sq]=m;
    }
}

void init_magics() {
    init_magic_array(false, rookMagics);
    init_magic_array(true, bishopMagics);
}

uint64_t bishop_attacks(int sq, uint64_t occ) {
    const Magic& m = bishopMagics[sq];
    uint64_t occMask = occ & m.mask;
    uint64_t idx = (occMask * m.magic) >> m.shift;
    if(idx < m.attacks.size()) return m.attacks[idx];
    return 0ULL;
}

uint64_t rook_attacks(int sq, uint64_t occ) {
    const Magic& m = rookMagics[sq];
    uint64_t occMask = occ & m.mask;
    uint64_t idx = (occMask * m.magic) >> m.shift;
    if(idx < m.attacks.size()) return m.attacks[idx];
    return 0ULL;
}

static void init_leaper_attacks() {
    for(int sq=0; sq<64; ++sq) {
        knightAttacks[sq] = knight_mask(sq);
        kingAttacks[sq] = king_mask(sq);
    }
}

void init_tables() {
    init_leaper_attacks();
    init_magics();
}

} // namespace ct2
