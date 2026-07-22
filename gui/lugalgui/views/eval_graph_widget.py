"""Advantage Timeline Evaluation Chart Widget for LugalChess GUI."""

import math
from PySide6.QtCore import Signal, QPointF, QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QFont, QLinearGradient, QMouseEvent, QPainter, QPaintEvent, QPainterPath, QPen
from PySide6.QtWidgets import QWidget


class EvalGraphWidget(QWidget):
    """Plot chart rendering game evaluation history across move plies."""

    move_clicked = Signal(int)  # Emitted when user clicks on a ply point in the graph

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setMinimumHeight(120)
        self._eval_history: list[tuple[int, float]] = []  # list of (ply_index, score_cp)
        self._hovered_index: int | None = None

        self.setMouseTracking(True)

    def set_eval_history(self, history: list[tuple[int, float]]) -> None:
        """Update evaluation history dataset [(ply_0, score_0), (ply_1, score_1), ...]."""
        self._eval_history = history
        self.update()

    def paintEvent(self, event: QPaintEvent) -> None:
        """Render advantage timeline chart."""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w = self.width()
        h = self.height()

        # Background
        bg_color = QColor(24, 26, 30)
        painter.fillRect(0, 0, w, h, bg_color)

        if not self._eval_history:
            font = QFont("Arial", 9)
            painter.setFont(font)
            painter.setPen(QColor(140, 140, 140))
            painter.drawText(QRectF(0, 0, w, h), Qt.AlignmentFlag.AlignCenter, "No evaluation history recorded yet.")
            return

        # Margins
        margin_left = 35
        margin_right = 15
        margin_top = 15
        margin_bottom = 25

        plot_w = w - margin_left - margin_right
        plot_h = h - margin_top - margin_bottom

        zero_y = margin_top + plot_h / 2.0

        # Draw grid lines and score labels (+3, +1, 0, -1, -3)
        grid_pen = QPen(QColor(45, 50, 60), 1, Qt.PenStyle.DashLine)
        painter.setPen(grid_pen)
        painter.drawLine(margin_left, int(zero_y), w - margin_right, int(zero_y))

        # Y-axis bounds: cap evaluation plot between -5.0 and +5.0 pawns
        y_max = 5.0
        y_min = -5.0

        def cp_to_y(cp: float) -> float:
            clamped = max(y_min, min(y_max, cp / 100.0))
            ratio = (clamped - y_min) / (y_max - y_min)
            return margin_top + plot_h * (1.0 - ratio)

        # Draw Y-axis tick labels
        font = QFont("Arial", 8)
        painter.setFont(font)
        painter.setPen(QColor(150, 150, 150))
        painter.drawText(5, int(cp_to_y(300)) + 4, "+3.0")
        painter.drawText(5, int(zero_y) + 4, "0.0")
        painter.drawText(5, int(cp_to_y(-300)) + 4, "-3.0")

        # Map plies to points
        num_points = len(self._eval_history)
        points: list[QPointF] = []
        for i, (ply, score) in enumerate(self._eval_history):
            x = margin_left + (i / max(1, num_points - 1)) * plot_w if num_points > 1 else margin_left + plot_w / 2.0
            y = cp_to_y(score)
            points.append(QPointF(x, y))

        # Build white advantage area path (above zero line)
        if len(points) >= 2:
            path_line = QPainterPath()
            path_line.moveTo(points[0])
            for p in points[1:]:
                path_line.lineTo(p)

            painter.setPen(QPen(QColor(0, 180, 216), 2))
            painter.drawPath(path_line)

            # Draw data points
            for i, p in enumerate(points):
                if self._hovered_index == i:
                    painter.setBrush(QColor(255, 255, 255))
                    painter.drawEllipse(p, 5, 5)
                else:
                    painter.setBrush(QColor(0, 180, 216))
                    painter.drawEllipse(p, 3, 3)

        # Draw hovered tooltip
        if self._hovered_index is not None and 0 <= self._hovered_index < len(self._eval_history):
            ply, score = self._eval_history[self._hovered_index]
            p = points[self._hovered_index]
            sign = "+" if score > 0 else ""
            move_num = (ply // 2) + 1
            side_str = "W" if (ply % 2 == 0) else "B"
            tip_text = f"Move {move_num}.{side_str} | Score: {sign}{score / 100.0:.2f}"

            painter.setBrush(QColor(30, 30, 30, 230))
            painter.setPen(QColor(0, 180, 216))
            tip_rect = QRectF(p.x() - 60, max(5, p.y() - 30), 120, 22)
            painter.drawRoundedRect(tip_rect, 4, 4)
            painter.setPen(QColor(255, 255, 255))
            painter.drawText(tip_rect, Qt.AlignmentFlag.AlignCenter, tip_text)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        """Track hovered ply point."""
        if not self._eval_history:
            return

        w = self.width()
        margin_left = 35
        margin_right = 15
        plot_w = w - margin_left - margin_right

        pos_x = event.position().x()
        if margin_left <= pos_x <= w - margin_right:
            num_points = len(self._eval_history)
            idx = int(round(((pos_x - margin_left) / max(1, plot_w)) * (num_points - 1)))
            idx = max(0, min(num_points - 1, idx))
            if self._hovered_index != idx:
                self._hovered_index = idx
                self.update()
        else:
            if self._hovered_index is not None:
                self._hovered_index = None
                self.update()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        """Jump to clicked ply position in game tree."""
        if self._hovered_index is not None and 0 <= self._hovered_index < len(self._eval_history):
            ply, _ = self._eval_history[self._hovered_index]
            self.move_clicked.emit(ply)
