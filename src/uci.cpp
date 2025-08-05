#include "uci.h"
#include "bitops.h"
#include <algorithm>
#include <cctype>
#include <array>
#include <vector>
#include <unordered_map>
#include <random>

#include "opening_book.h"

#include <iostream>
#include <sstream>

namespace ct2 {

static const int VAL_PIECE[6] = {100,320,330,500,900,0};

struct TTEntry {
    int depth;
    int score;
};

static std::unordered_map<std::string, TTEntry> TT;
static uint64_t nodes = 0;

static const int MAX_DEPTH = 6;

static std::unordered_map<std::string, std::vector<std::string>> openingBook = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {"e2e4", "d2d4", "c2c4", "g1f3"}}
};

  static std::mt19937 rng(2024);

static int quiescence(Board& b, int alpha, int beta);

static int sq_from_str(const std::string& s) {
    return (s[1]-'1')*8 + (s[0]-'a');
}

static std::string sq_to_str(int sq) {
    std::string s(2,'a');
    s[0] = 'a' + (sq % 8);
    s[1] = '1' + (sq / 8);
    return s;
}

static Board::Move parse_move(const std::string& m, const Board& b) {
    Board::Move mv{};
    mv.from = sq_from_str(m.substr(0,2));
    mv.to = sq_from_str(m.substr(2,2));
    mv.piece = PIECE_NB;
    for(int p=WP;p<PIECE_NB;++p) if(b.pieceBB((Piece)p) & (1ULL<<mv.from)) mv.piece=(Piece)p;
    mv.capture = PIECE_NB;
    for(int p=WP;p<PIECE_NB;++p) if(b.pieceBB((Piece)p) & (1ULL<<mv.to)) mv.capture=(Piece)p;
    mv.promotion = PIECE_NB;
    mv.is_ep = false;
    mv.is_castling = false;

    // Detect castling moves so the rook gets moved correctly
    if(mv.piece == WK && mv.from == 4 && (mv.to == 6 || mv.to == 2))
        mv.is_castling = true;
    else if(mv.piece == BK && mv.from == 60 && (mv.to == 62 || mv.to == 58))
        mv.is_castling = true;

    // Detect en passant captures
    if(mv.piece == WP && mv.to == b.ep_square_sq() && (mv.to - mv.from == 7 || mv.to - mv.from == 9)) {
        mv.is_ep = true;
        mv.capture = BP;
    }
    if(mv.piece == BP && mv.to == b.ep_square_sq() && (mv.from - mv.to == 7 || mv.from - mv.to == 9)) {
        mv.is_ep = true;
        mv.capture = WP;
    }
    if (m.size() > 4) {
        char prom = m[4];
        switch(prom) {
            case 'q': case 'Q': mv.promotion = (mv.piece==WP?WQ:BQ); break;
            case 'r': case 'R': mv.promotion = (mv.piece==WP?WR:BR); break;
            case 'b': case 'B': mv.promotion = (mv.piece==WP?WB:BB); break;
            case 'n': case 'N': mv.promotion = (mv.piece==WP?WN:BN); break;
        }
    }
    return mv;
}

static std::string move_to_str(const Board::Move& m) {
    std::string s = sq_to_str(m.from) + sq_to_str(m.to);
    if (m.promotion != PIECE_NB) {
        const char* promStr = "pnbrqkPNBRQK"; // not all used
        char c='q';
        switch(m.promotion) {
            case WN: case BN: c='n'; break;
            case WB: case BB: c='b'; break;
            case WR: case BR: c='r'; break;
            case WQ: case BQ: c='q'; break;
            default: break;
        }
        s += c;
    }
    return s;
}

static bool get_book_move(const Board& b, Board::Move& out) {
    auto it = openingBook.find(b.getFEN());
    if (it == openingBook.end()) return false;
    const auto& moves = it->second;
    if (moves.empty()) return false;
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    out = parse_move(moves[dist(rng)], b);
    return true;
}

static int piece_square(Piece p, int sq) {
    int f = sq % 8;
    int r = sq / 8;
    if (p >= BP) r = 7 - r; // mirror for black pieces
    switch(p % 6) {
        case WP: // pawn
            return r * 10 + (3 - std::abs(3 - f)) * 2;
        case WN: // knight
            return 30 - (std::abs(3 - f) + std::abs(3 - r)) * 4;
        case WB: // bishop
            return 30 - (std::max(std::abs(3 - f), std::abs(3 - r)) * 3);
        case WR: // rook
            return r * 4;
        case WQ: // queen
            return 10 - (std::abs(3 - f) + std::abs(3 - r));
        default: // king
            return -(std::abs(3 - f) + std::abs(3 - r));
    }
}

static int evaluate(const Board& b) {
    int score = 0;
    for(int p = WP; p < PIECE_NB; ++p) {
        uint64_t bb = b.pieceBB((Piece)p);
        int color = (p < BP) ? 1 : -1;
        while(bb) {
            int sq = ctz64(bb);
            bb &= bb - 1;
            score += color * (VAL_PIECE[p % 6] + piece_square((Piece)p, sq));
        }
    }
    return (b.side_to_move() == WHITE ? score : -score);
}

static int move_order_score(const Board::Move& mv) {
    int score = 0;
    if (mv.capture != PIECE_NB)
        score += 10 * VAL_PIECE[mv.capture % 6] - VAL_PIECE[mv.piece % 6];
    if (mv.promotion != PIECE_NB)
        score += VAL_PIECE[mv.promotion % 6];
    return score;
}

static bool is_quiet(const Board::Move& mv) {
    return mv.capture == PIECE_NB && mv.promotion == PIECE_NB;
}

struct SearchResult {
    Board::Move best;
    int score;
};

static int negamax(Board& b, int depth, int alpha, int beta) {
    nodes++;
    if (depth == 0) {
        return quiescence(b, alpha, beta);
    }

    std::string key = b.getFEN();
    auto ttIt = TT.find(key);
    if (ttIt != TT.end() && ttIt->second.depth >= depth)
        return ttIt->second.score;

    auto moves = b.generate_legal_moves();
    if (moves.empty()) return -100000 + depth; // checkmate or stalemate
    std::sort(moves.begin(), moves.end(), [](const Board::Move& a, const Board::Move& b) {
        return move_order_score(a) > move_order_score(b);
    });
    int eval = 0;
    if (depth == 1) eval = evaluate(b);
    int best = -1000000;
    for (const auto& mv : moves) {
        if (depth == 1 && is_quiet(mv) && eval + 200 <= alpha) continue; // futility pruning
        Board copy = b;
        copy.make_move(mv);
        int score = -negamax(copy, depth - 1, -beta, -alpha);
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    TT[key] = {depth, best};
    return best;
}

static int quiescence(Board& b, int alpha, int beta) {
    nodes++;
    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;
    auto moves = b.generate_legal_moves();
    std::sort(moves.begin(), moves.end(), [](const Board::Move& a, const Board::Move& b) {
        return move_order_score(a) > move_order_score(b);
    });
    for (const auto& mv : moves) {
        if (mv.capture == PIECE_NB && mv.promotion == PIECE_NB) continue;
        Board copy = b;
        copy.make_move(mv);
        int score = -quiescence(copy, -beta, -alpha);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

static SearchResult search_best(Board& b) {
    Board::Move bookMove;
    if (get_book_move(b, bookMove)) {
        Board copy = b;
        copy.make_move(bookMove);
        int sc = evaluate(copy);
        return {bookMove, sc};
    }
    auto moves = b.generate_legal_moves();

    if (moves.empty()) {
        int sc = b.in_check(b.side_to_move()) ? -100000 : 0;
        return {Board::Move{0,0,WP,PIECE_NB,PIECE_NB,false,false}, sc};
    }

    std::sort(moves.begin(), moves.end(), [](const Board::Move& a, const Board::Move& b) {
        return move_order_score(a) > move_order_score(b);
    });
    Board::Move best = moves[0];
    int bestScore = -1000000;
    for (int depth = 1; depth <= MAX_DEPTH; ++depth) {
        Board::Move localBest = moves[0];
        int localBestScore = -1000000;
        for (const auto& mv : moves) {
            Board copy = b;
            copy.make_move(mv);
            int sc = -negamax(copy, depth - 1, -1000000, 1000000);
            if (sc > localBestScore) {
                localBestScore = sc;
                localBest = mv;
            }
        }
        best = localBest;
        bestScore = localBestScore;
    }
    return {best, bestScore};
}

void uci_loop(Board& board) {
    std::string token;
    std::cout << "id name ct2" << std::endl;
    std::cout << "id author codex" << std::endl;
    std::cout << "uciok" << std::endl;

    while (std::getline(std::cin, token)) {
        if (token == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (token == "quit") {
            break;
        } else if (token.rfind("position", 0) == 0) {
            std::istringstream ss(token);
            std::string word;
            ss >> word; // position
            if (ss >> word) {
                if (word == "startpos") {
                    board.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                } else if (word == "fen") {
                    std::string fen;
                    std::getline(ss, fen);
                    if (!fen.empty() && fen[0] == ' ') fen = fen.substr(1);
                    board.loadFEN(fen);
                }
                if (ss >> word && word == "moves") {
                    while (ss >> word) {
                        auto mv = parse_move(word, board);
                        board.make_move(mv);
                    }
                }
            }
        } else if (token == "ucinewgame") {
            // nothing
        } else if (token.rfind("go", 0) == 0) {
            nodes = 0;
            auto result = search_best(board);
            std::cout << "info score cp " << result.score

                      << " depth " << MAX_DEPTH << " nodes " << nodes
                      << " pv " << move_to_str(result.best) << std::endl;
            std::cout << "bestmove " << move_to_str(result.best) << std::endl;
        }
    }
}

} // namespace ct2
