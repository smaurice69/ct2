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

// ================= Magic bitboards =====================

static std::array<Magic, 64> rookMagics;
static std::array<Magic, 64> bishopMagics;

static uint64_t random_uint64() {
    static uint64_t x = 88172645463325252ULL;
    x ^= x << 7;  x ^= x >> 9;
    return x;
}

static int popcount(uint64_t b) {
    return __builtin_popcountll(b);
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

} // namespace ct2
