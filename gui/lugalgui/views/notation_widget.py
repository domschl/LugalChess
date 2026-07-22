"""Move Tree & PGN Notation History Panel Widget."""

from typing import Any
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFont
from PySide6.QtWidgets import QHeaderView, QTableWidget, QTableWidgetItem, QWidget


class NotationWidget(QTableWidget):
    """Rich Unicode chess notation table panel."""

    move_selected = Signal(int)  # Emits ply index when clicked

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(0, 3, parent)
        self.setHorizontalHeaderLabels(["#", "White", "Black"])
        self.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        
        # Configure column resize behavior
        header = self.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)

        # Set font with rich Unicode support
        font = QFont("Sans-Serif", 11)
        self.setFont(font)
        self.cellClicked.connect(self._on_cell_clicked)

    def update_history(self, move_records: list[dict[str, Any]]) -> None:
        """Populate table with move records."""
        self.setRowCount(0)
        
        row = 0
        for i in range(0, len(move_records), 2):
            w_rec = move_records[i]
            b_rec = move_records[i + 1] if i + 1 < len(move_records) else None

            self.insertRow(row)

            # Move number
            item_num = QTableWidgetItem(f"{w_rec['move_num']}.")
            item_num.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            self.setItem(row, 0, item_num)

            # White move
            item_w = QTableWidgetItem(w_rec["san"])
            item_w.setData(Qt.ItemDataRole.UserRole, w_rec["ply"])
            self.setItem(row, 1, item_w)

            # Black move
            if b_rec:
                item_b = QTableWidgetItem(b_rec["san"])
                item_b.setData(Qt.ItemDataRole.UserRole, b_rec["ply"])
                self.setItem(row, 2, item_b)

            row += 1

        self.scrollToBottom()

    def _on_cell_clicked(self, row: int, col: int) -> None:
        """Emit move_selected signal when a move cell is clicked."""
        if col == 0:
            return
            
        item = self.item(row, col)
        if item:
            ply = item.data(Qt.ItemDataRole.UserRole)
            if ply is not None:
                self.move_selected.emit(int(ply))
