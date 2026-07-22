"""Interactive High-DPI Graphical Chessboard Widget."""

from typing import Any
import chess
from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QBrush, QColor, QFont, QFontMetrics, QMouseEvent, QPainter, QPainterPath, QPen
from PySide6.QtWidgets import QWidget

from lugalgui.models.game_tree import UNICODE_PIECES


class ChessBoardWidget(QWidget):
    """High-performance 2D interactive chessboard view."""

    # Qt Signals
    user_move_made = Signal(str)  # Emits move string in UCI format, e.g. 'e2e4' or 'e7e8q'

    # Theme colors
    LIGHT_SQUARE = QColor("#F0D9B5")
    DARK_SQUARE = QColor("#B58863")
    SELECTED_COLOR = QColor(255, 255, 0, 140)
    LAST_MOVE_COLOR = QColor(155, 199, 0, 140)
    HIGHLIGHT_DOT_COLOR = QColor(20, 85, 30, 120)
    CHECK_COLOR = QColor(230, 40, 40, 180)

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setMinimumSize(400, 400)
        self.board: chess.Board = chess.Board()
        self.flipped: bool = False  # False = White at bottom, True = Black at bottom
        self.selected_square: chess.Square | None = None
        self.legal_destinations: set[chess.Square] = set()
        self.last_move: chess.Move | None = None
        
        # Drag and drop tracking
        self.dragging: bool = False
        self.drag_square: chess.Square | None = None
        self.drag_pos: QPointF = QPointF(0, 0)
        
        self.setMouseTracking(True)

    def set_board(self, board: chess.Board, last_move: chess.Move | None = None) -> None:
        """Update current board position and force redraw."""
        self.board = board.copy()
        self.last_move = last_move
        self.selected_square = None
        self.legal_destinations.clear()
        self.update()

    def flip_board(self) -> None:
        """Toggle board perspective (White vs Black orientation)."""
        self.flipped = not self.flipped
        self.update()

    def _square_at_point(self, pos: QPointF) -> chess.Square | None:
        """Map screen pixel position to chess square index (0..63)."""
        board_size = min(self.width(), self.height())
        offset_x = (self.width() - board_size) / 2.0
        offset_y = (self.height() - board_size) / 2.0
        sq_size = board_size / 8.0

        rel_x = pos.x() - offset_x
        rel_y = pos.y() - offset_y

        if rel_x < 0 or rel_x >= board_size or rel_y < 0 or rel_y >= board_size:
            return None

        col = int(rel_x // sq_size)
        row = int(rel_y // sq_size)

        if self.flipped:
            file_idx = 7 - col
            rank_idx = row
        else:
            file_idx = col
            rank_idx = 7 - row

        return chess.square(file_idx, rank_idx)

    def _square_rect(self, sq: chess.Square) -> QRectF:
        """Calculate screen rectangle for a given square index."""
        board_size = min(self.width(), self.height())
        offset_x = (self.width() - board_size) / 2.0
        offset_y = (self.height() - board_size) / 2.0
        sq_size = board_size / 8.0

        file_idx = chess.square_file(sq)
        rank_idx = chess.square_rank(sq)

        if self.flipped:
            col = 7 - file_idx
            row = rank_idx
        else:
            col = file_idx
            row = 7 - rank_idx

        x = offset_x + col * sq_size
        y = offset_y + row * sq_size
        return QRectF(x, y, sq_size, sq_size)

    def mousePressEvent(self, event: QMouseEvent) -> None:
        """Handle mouse click for square selection or drag initialization."""
        if event.button() != Qt.MouseButton.LeftButton:
            return

        sq = self._square_at_point(event.position())
        if sq is None:
            self.selected_square = None
            self.legal_destinations.clear()
            self.update()
            return

        # If a square was already selected and user clicks a legal destination
        if self.selected_square is not None and sq in self.legal_destinations:
            self._attempt_move(self.selected_square, sq)
            self.selected_square = None
            self.legal_destinations.clear()
            self.update()
            return

        # Otherwise select clicked piece square
        piece = self.board.piece_at(sq)
        if piece and piece.color == self.board.turn:
            self.selected_square = sq
            self.drag_square = sq
            self.dragging = True
            self.drag_pos = event.position()
            self.legal_destinations = {
                m.to_square for m in self.board.legal_moves if m.from_square == sq
            }
        else:
            self.selected_square = None
            self.legal_destinations.clear()

        self.update()

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        """Track mouse drag position."""
        if self.dragging:
            self.drag_pos = event.position()
            self.update()

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        """Handle mouse release for piece drag drop."""
        if event.button() == Qt.MouseButton.LeftButton and self.dragging:
            self.dragging = False
            target_sq = self._square_at_point(event.position())
            if self.drag_square is not None and target_sq is not None and target_sq in self.legal_destinations:
                self._attempt_move(self.drag_square, target_sq)
                self.selected_square = None
                self.legal_destinations.clear()

            self.drag_square = None
            self.update()

    def _attempt_move(self, from_sq: chess.Square, to_sq: chess.Square) -> None:
        """Format UCI move string and emit user_move_made signal."""
        # Auto-promote to Queen if reaching last rank
        piece = self.board.piece_at(from_sq)
        is_pawn = piece and piece.piece_type == chess.PAWN
        target_rank = chess.square_rank(to_sq)
        
        promo_char = ""
        if is_pawn and (target_rank == 7 or target_rank == 0):
            promo_char = "q"

        move_uci = f"{chess.square_name(from_sq)}{chess.square_name(to_sq)}{promo_char}"
        self.user_move_made.emit(move_uci)

    def paintEvent(self, event: Any) -> None:
        """Render chessboard squares, coordinates, highlights, and Unicode pieces."""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)

        board_size = min(self.width(), self.height())
        offset_x = (self.width() - board_size) / 2.0
        offset_y = (self.height() - board_size) / 2.0
        sq_size = board_size / 8.0

        # 1. Draw 64 Squares
        for sq in range(64):
            rect = self._square_rect(sq)
            file_idx = chess.square_file(sq)
            rank_idx = chess.square_rank(sq)
            is_light = (file_idx + rank_idx) % 2 != 0

            brush = QBrush(self.LIGHT_SQUARE if is_light else self.DARK_SQUARE)
            painter.fillRect(rect, brush)

        # 2. Draw Highlights
        # Last move highlight
        if self.last_move:
            painter.fillRect(self._square_rect(self.last_move.from_square), QBrush(self.LAST_MOVE_COLOR))
            painter.fillRect(self._square_rect(self.last_move.to_square), QBrush(self.LAST_MOVE_COLOR))

        # Selected square highlight
        if self.selected_square is not None:
            painter.fillRect(self._square_rect(self.selected_square), QBrush(self.SELECTED_COLOR))

        # King in check highlight
        if self.board.is_check():
            king_sq = self.board.king(self.board.turn)
            if king_sq is not None:
                painter.fillRect(self._square_rect(king_sq), QBrush(self.CHECK_COLOR))

        # Destination dots
        for dst_sq in self.legal_destinations:
            d_rect = self._square_rect(dst_sq)
            center = d_rect.center()
            radius = sq_size * 0.18
            
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QBrush(self.HIGHLIGHT_DOT_COLOR))
            painter.drawEllipse(center, radius, radius)

        # 3. Render Pieces using Rich Unicode Characters
        font = QFont("Sans-Serif", int(sq_size * 0.75), QFont.Weight.Bold)
        painter.setFont(font)

        for sq in range(64):
            if self.dragging and sq == self.drag_square:
                continue  # Skip drawing dragged piece at its original square

            piece = self.board.piece_at(sq)
            if piece:
                rect = self._square_rect(sq)
                symbol = UNICODE_PIECES.get(piece.symbol(), piece.symbol())
                
                # Use contrast outline color
                color = QColor(255, 255, 255) if piece.color == chess.WHITE else QColor(10, 10, 10)
                painter.setPen(QPen(color))
                painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, symbol)

        # 4. Render Dragged Piece under mouse cursor
        if self.dragging and self.drag_square is not None:
            piece = self.board.piece_at(self.drag_square)
            if piece:
                symbol = UNICODE_PIECES.get(piece.symbol(), piece.symbol())
                drag_rect = QRectF(self.drag_pos.x() - sq_size / 2.0, self.drag_pos.y() - sq_size / 2.0, sq_size, sq_size)
                color = QColor(255, 255, 255) if piece.color == chess.WHITE else QColor(10, 10, 10)
                painter.setPen(QPen(color))
                painter.drawText(drag_rect, Qt.AlignmentFlag.AlignCenter, symbol)
