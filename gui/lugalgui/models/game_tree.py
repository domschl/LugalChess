"""Game Tree and Position Management Model."""

from typing import Any
import chess


UNICODE_PIECES: dict[str, str] = {
    "K": "♔", "Q": "♕", "R": "♖", "B": "♗", "N": "♘", "P": "♙",
    "k": "♚", "q": "♛", "r": "♜", "b": "♝", "n": "♞", "p": "♟",
}


class GameNode:
    """Represents a node in the chess game tree."""

    def __init__(self, move: chess.Move | None = None, parent: "GameNode | None" = None) -> None:
        self.move: chess.Move | None = move
        self.parent: GameNode | None = parent
        self.children: list[GameNode] = []
        self.comment: str = ""
        self.nags: set[int] = set()
        self.eval_score: int | None = None  # Centipawns or mate plies


class GameTree:
    """Manages the full game variation tree, current move index, and FEN position."""

    def __init__(self, fen: str = chess.STARTING_FEN) -> None:
        self.board: chess.Board = chess.Board(fen)
        self.root: GameNode = GameNode()
        self.current_node: GameNode = self.root
        self.move_history: list[chess.Move] = []

    def reset_to_start(self) -> None:
        """Reset game tree to the standard starting position."""
        self.board.reset()
        self.root = GameNode()
        self.current_node = self.root
        self.move_history.clear()

    def load_fen(self, fen: str) -> bool:
        """Load a custom FEN position."""
        try:
            self.board = chess.Board(fen)
            self.root = GameNode()
            self.current_node = self.root
            self.move_history.clear()
            return True
        except ValueError:
            return False

    def push_move(self, move: chess.Move) -> bool:
        """Apply a legal move to the current position and add to game tree."""
        if move in self.board.legal_moves:
            self.board.push(move)
            self.move_history.append(move)
            
            # Check if move already exists as a child node
            existing_child: GameNode | None = None
            for child in self.current_node.children:
                if child.move == move:
                    existing_child = child
                    break
                    
            if existing_child:
                self.current_node = existing_child
            else:
                new_node = GameNode(move=move, parent=self.current_node)
                self.current_node.children.append(new_node)
                self.current_node = new_node
            return True
        return False

    def push_uci_str(self, uci_str: str) -> bool:
        """Parse and apply a move string in UCI format (e.g. 'e2e4') or SAN format (e.g. 'c5', 'Nf3')."""
        try:
            move = chess.Move.from_uci(uci_str)
            return self.push_move(move)
        except ValueError:
            pass

        try:
            move = self.board.parse_san(uci_str)
            return self.push_move(move)
        except ValueError:
            return False

    def pop_move(self) -> bool:
        """Take back one half-move (undo)."""
        if self.board.move_stack:
            self.board.pop()
            self.move_history.pop()
            if self.current_node.parent:
                self.current_node = self.current_node.parent
            return True
        return False

    def get_legal_moves_from_square(self, square: chess.Square) -> list[chess.Move]:
        """Return all legal moves starting from a given square."""
        return [m for m in self.board.legal_moves if m.from_square == square]

    def is_game_over(self) -> bool:
        """Check if current board position is game over."""
        return self.board.is_game_over()

    def get_status_str(self) -> str:
        """Return game status description (Checkmate, Stalemate, Draw, Check, Normal)."""
        if self.board.is_checkmate():
            winner = "Black" if self.board.turn == chess.WHITE else "White"
            return f"Checkmate! {winner} wins."
        if self.board.is_stalemate():
            return "Stalemate - Draw"
        if self.board.is_insufficient_material():
            return "Draw by insufficient material"
        if self.board.is_fivefold_repetition() or self.board.is_seventyfive_moves():
            return "Draw by rule"
        if self.board.is_check():
            side = "White" if self.board.turn == chess.WHITE else "Black"
            return f"Check! ({side} to move)"
        side = "White" if self.board.turn == chess.WHITE else "Black"
        return f"{side} to move"

    def get_san_history(self) -> list[dict[str, Any]]:
        """Reconstruct full move list formatted with Unicode symbols and SAN notation."""
        temp_board = chess.Board(self.root.move.uci() if self.root.move else chess.STARTING_FEN)
        if self.root.move is None and self.move_history:
            temp_board = chess.Board()

        move_records: list[dict[str, Any]] = []
        for i, move in enumerate(self.move_history):
            san_str = temp_board.san(move)
            # Replace piece characters with rich Unicode symbols
            for p_char, u_char in UNICODE_PIECES.items():
                if p_char.isupper():
                    san_str = san_str.replace(p_char, u_char)
                    
            move_records.append({
                "index": i,
                "ply": i + 1,
                "move_num": (i // 2) + 1,
                "is_white": (i % 2 == 0),
                "move": move,
                "san": san_str,
                "fen": temp_board.fen()
            })
            temp_board.push(move)
        return move_records
