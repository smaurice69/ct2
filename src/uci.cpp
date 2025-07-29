#include "uci.h"
#include "bitops.h"
#include <algorithm>
#include <cctype>

#include <iostream>
#include <sstream>

namespace ct2 {

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

static int evaluate(const Board& b) {
    static const int val[PIECE_NB] = {100,320,330,500,900,0,-100,-320,-330,-500,-900,0};
    int score = 0;
    for(int p=WP; p<PIECE_NB; ++p) {
        score += popcount64(b.pieceBB((Piece)p)) * val[p];
    }
    return (b.side_to_move()==WHITE?1:-1)*score;
}

static Board::Move search_best(Board& b) {
    auto moves = b.generate_moves();
    if(moves.empty()) return Board::Move{0,0,WP,PIECE_NB,PIECE_NB,false,false};
    Board::Move best = moves[0];
    int bestScore = -1000000;
    for(const auto& mv : moves) {
        Board copy = b;
        copy.make_move(mv);
        int sc = evaluate(copy);
        if(sc > bestScore) { bestScore = sc; best = mv; }
    }
    return best;
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
                    board.loadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
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
            auto best = search_best(board);
            std::cout << "bestmove " << move_to_str(best) << std::endl;
        }
    }
}

} // namespace ct2
