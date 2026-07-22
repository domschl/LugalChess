"""Vertical Evaluation Bar Widget for LugalChess GUI."""

import math
from PySide6.QtCore import Property, QPropertyAnimation, QEasingCurve, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPaintEvent
from PySide6.QtWidgets import QWidget


class EvalBarWidget(QWidget):
    """Vertical visual evaluation bar displaying position advantage."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setFixedWidth(26)
        self.setMinimumHeight(200)

        self._white_ratio: float = 0.5  # 0.0 (Black 100%) to 1.0 (White 100%)
        self._target_white_ratio: float = 0.5
        self._score_text: str = "+0.00"
        self._is_mate: bool = False

        # Smooth animation transition
        self._anim = QPropertyAnimation(self, b"animated_white_ratio", self)
        self._anim.setDuration(250)
        self._anim.setEasingCurve(QEasingCurve.Type.OutCubic)

    def get_animated_white_ratio(self) -> float:
        return self._white_ratio

    def set_animated_white_ratio(self, val: float) -> None:
        self._white_ratio = val
        self.update()

    animated_white_ratio = Property(float, get_animated_white_ratio, set_animated_white_ratio)

    def set_eval(self, score_cp: int | None, is_mate: bool = False, mate_in: int = 0) -> None:
        """Update evaluation score in centipawns or mate-in plies."""
        self._is_mate = is_mate

        if is_mate:
            if mate_in > 0:
                target_ratio = 1.0
                self._score_text = f"M{mate_in}"
            else:
                target_ratio = 0.0
                self._score_text = f"-M{abs(mate_in)}"
        elif score_cp is not None:
            # Sigmoidal winning probability conversion: P(W) = 1 / (1 + 10^(-cp/400))
            prob = 1.0 / (1.0 + math.pow(10.0, -score_cp / 400.0))
            target_ratio = max(0.02, min(0.98, prob))
            sign = "+" if score_cp > 0 else ""
            self._score_text = f"{sign}{score_cp / 100.0:.2f}"
        else:
            target_ratio = 0.5
            self._score_text = "+0.00"

        # Trigger smooth transition
        self._anim.stop()
        self._anim.setStartValue(self._white_ratio)
        self._anim.setEndValue(target_ratio)
        self._anim.start()

    def paintEvent(self, event: QPaintEvent) -> None:
        """Draw vertical evaluation bar with smooth gradient and score text."""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w = self.width()
        h = self.height()

        # Overall rounded container border
        rect = QRectF(2, 2, w - 4, h - 4)

        # Draw Black background (top)
        black_color = QColor(36, 36, 36)
        painter.setBrush(black_color)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawRoundedRect(rect, 4, 4)

        # Draw White fill (bottom)
        white_height = (h - 4) * self._white_ratio
        white_y = (h - 2) - white_height

        white_rect = QRectF(2, white_y, w - 4, white_height)
        white_color = QColor(240, 240, 240)
        painter.setBrush(white_color)
        painter.drawRoundedRect(white_rect, 2, 2)

        # Draw center dividing line at equal (0.00) level
        painter.setPen(QColor(128, 128, 128, 120))
        mid_y = h / 2.0
        painter.drawLine(2, int(mid_y), w - 2, int(mid_y))

        # Draw Score text pill near top or bottom depending on ratio
        font = QFont("Arial", 8, QFont.Weight.Bold)
        painter.setFont(font)

        text_color = QColor(0, 0, 0) if self._white_ratio > 0.5 else QColor(255, 255, 255)
        text_y = int(h - 12) if self._white_ratio > 0.5 else 16

        painter.setPen(text_color)
        painter.drawText(QRectF(0, text_y - 10, w, 16), Qt.AlignmentFlag.AlignCenter, self._score_text)
