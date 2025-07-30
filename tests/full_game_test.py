import os
import chess
import chess.engine

ENGINE_PATH = os.path.join(os.path.dirname(__file__), '..', 'build', 'ct2')
STOCKFISH = '/usr/games/stockfish'

MAX_PLIES = 20  # 10 moves each side

def main():
    board = chess.Board()
    with chess.engine.SimpleEngine.popen_uci(ENGINE_PATH) as eng, \
         chess.engine.SimpleEngine.popen_uci(STOCKFISH) as sf:
        for _ in range(MAX_PLIES):
            res = eng.play(board, chess.engine.Limit(depth=1))
            if not board.is_legal(res.move):
                raise AssertionError(f'Engine produced illegal move: {res.move}')
            board.push(res.move)

            res = sf.play(board, chess.engine.Limit(depth=1))
            if not board.is_legal(res.move):
                raise AssertionError(f'Stockfish produced illegal move: {res.move}')
            board.push(res.move)

if __name__ == '__main__':
    main()
