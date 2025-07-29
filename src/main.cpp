#include "board.h"
#include "uci.h"

int main() {
    ct2::init_tables();
    ct2::Board board;
    ct2::uci_loop(board);
    return 0;
}
