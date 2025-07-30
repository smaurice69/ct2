import os
import chess
import chess.engine

ENGINE_PATH = os.path.join(os.path.dirname(__file__), '..', 'build', 'ct2')

POSITION = 'rnbqkb1r/1p1ppppp/8/p1p1P3/6n1/2N2N1P/PPPP1PP1/R1BQKB1R b KQkq - 0 5'

def main():
    board = chess.Board(POSITION)
    with chess.engine.SimpleEngine.popen_uci(ENGINE_PATH) as eng:
        res = eng.play(board, chess.engine.Limit(depth=1))
        if not board.is_legal(res.move):
            raise AssertionError(f'Engine produced illegal move: {res.move}')
        board.push(res.move)

if __name__ == '__main__':
    main()
