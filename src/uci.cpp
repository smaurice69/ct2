#include "uci.h"
#include "bitops.h"
#include <algorithm>
#include <array>
#include <cctype>

#include <iostream>
#include <sstream>

namespace ct2 {

static int sq_from_str(const std::string &s) {
  return (s[1] - '1') * 8 + (s[0] - 'a');
}

static std::string sq_to_str(int sq) {
  std::string s(2, 'a');
  s[0] = 'a' + (sq % 8);
  s[1] = '1' + (sq / 8);
  return s;
}

static Board::Move parse_move(const std::string &m, const Board &b) {
  Board::Move mv{};
  mv.from = sq_from_str(m.substr(0, 2));
  mv.to = sq_from_str(m.substr(2, 2));
  mv.piece = PIECE_NB;
  for (int p = WP; p < PIECE_NB; ++p)
    if (b.pieceBB((Piece)p) & (1ULL << mv.from))
      mv.piece = (Piece)p;
  mv.capture = PIECE_NB;
  for (int p = WP; p < PIECE_NB; ++p)
    if (b.pieceBB((Piece)p) & (1ULL << mv.to))
      mv.capture = (Piece)p;
  mv.promotion = PIECE_NB;
  mv.is_ep = false;
  mv.is_castling = false;
  if (m.size() > 4) {
    char prom = m[4];
    switch (prom) {
    case 'q':
    case 'Q':
      mv.promotion = (mv.piece == WP ? WQ : BQ);
      break;
    case 'r':
    case 'R':
      mv.promotion = (mv.piece == WP ? WR : BR);
      break;
    case 'b':
    case 'B':
      mv.promotion = (mv.piece == WP ? WB : BB);
      break;
    case 'n':
    case 'N':
      mv.promotion = (mv.piece == WP ? WN : BN);
      break;
    }
  }
  // detect castling
  if ((mv.piece == WK || mv.piece == BK) && std::abs(mv.to - mv.from) == 2) {
    mv.is_castling = true;
  }
  // detect en-passant
  if (mv.piece == WP || mv.piece == BP) {
    if (mv.capture == PIECE_NB && m.size() == 4) {
      int dir = (mv.piece == WP) ? 8 : -8;
      if (b.ep_square_sq() == mv.to &&
          b.pieceBB((mv.piece == WP ? BP : WP)) & (1ULL << (mv.to - dir))) {
        mv.is_ep = true;
        mv.capture = (mv.piece == WP ? BP : WP);
      }
    }
  }
  return mv;
}

static std::string move_to_str(const Board::Move &m) {
  std::string s = sq_to_str(m.from) + sq_to_str(m.to);
  if (m.promotion != PIECE_NB) {
    const char *promStr = "pnbrqkPNBRQK"; // not all used
    char c = 'q';
    switch (m.promotion) {
    case WN:
    case BN:
      c = 'n';
      break;
    case WB:
    case BB:
      c = 'b';
      break;
    case WR:
    case BR:
      c = 'r';
      break;
    case WQ:
    case BQ:
      c = 'q';
      break;
    default:
      break;
    }
    s += c;
  }
  return s;
}

static int piece_square(Piece p, int sq) {
  int f = sq % 8;
  int r = sq / 8;
  if (p >= BP)
    r = 7 - r; // mirror for black pieces
  switch (p % 6) {
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

static int evaluate(const Board &b) {
  static const int valPiece[6] = {100, 320, 330, 500, 900, 0};
  int score = 0;
  for (int p = WP; p < PIECE_NB; ++p) {
    uint64_t bb = b.pieceBB((Piece)p);
    int color = (p < BP) ? 1 : -1;
    while (bb) {
      int sq = ctz64(bb);
      bb &= bb - 1;
      score += color * (valPiece[p % 6] + piece_square((Piece)p, sq));
    }
  }
  return (b.side_to_move() == WHITE ? score : -score);
}

static int negamax(Board &b, int depth, int alpha, int beta) {
  if (depth == 0)
    return evaluate(b);
  auto moves = b.generate_legal_moves();
  if (moves.empty())
    return -100000 + depth; // checkmate or stalemate
  int best = -1000000;
  for (const auto &mv : moves) {
    Board copy = b;
    copy.make_move(mv);
    int score = -negamax(copy, depth - 1, -beta, -alpha);
    if (score > best)
      best = score;
    if (best > alpha)
      alpha = best;
    if (alpha >= beta)
      break;
  }
  return best;
}

static Board::Move search_best(Board &b) {
  auto moves = b.generate_legal_moves();
  if (moves.empty())
    return Board::Move{0, 0, WP, PIECE_NB, PIECE_NB, false, false};
  Board::Move best = moves[0];
  int bestScore = -1000000;
  for (const auto &mv : moves) {
    Board copy = b;
    copy.make_move(mv);
    int sc = -negamax(copy, 3, -1000000, 1000000);
    if (sc > bestScore) {
      bestScore = sc;
      best = mv;
    }
  }
  return best;
}

void uci_loop(Board &board) {
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
          board.loadFEN(
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
        } else if (word == "fen") {
          std::string fen;
          std::getline(ss, fen);
          if (!fen.empty() && fen[0] == ' ')
            fen = fen.substr(1);
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
