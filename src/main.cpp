#include "board.h"
#include "uci.h"

int main() {
    ct2::init_magics();
    ct2::Board board;
    ct2::uci_loop(board);
    return 0;
}
