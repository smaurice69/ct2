#include "uci.h"

#include <iostream>
#include <sstream>

namespace ct2 {

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
                    if (ss >> word && word == "moves") {
                        // ignore moves for now
                    }
                } else if (word == "fen") {
                    std::string fen;
                    std::getline(ss, fen);
                    if (!fen.empty() && fen[0] == ' ') fen = fen.substr(1);
                    board.loadFEN(fen);
                }
            }
        } else if (token == "ucinewgame") {
            // nothing
        } else if (token.rfind("go", 0) == 0) {
            std::cout << "bestmove 0000" << std::endl;
        }
    }
}

} // namespace ct2
