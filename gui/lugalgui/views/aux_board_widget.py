"""Auxiliary Board Analysis View Widget with Live Position Following and Task Assignments for LugalChess GUI."""

import chess
from PySide6.QtCore import Signal, Slot
from PySide6.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from lugalgui.controllers.uci_controller import UCIController
from lugalgui.controllers.xboard_adapter import XBoardAdapter
from lugalgui.models.engine_registry import EngineRegistry
from lugalgui.views.board_widget import ChessBoardWidget


class AuxBoardWidget(QWidget):
    """Auxiliary analysis board supporting task assignment (Live PV follower or Dedicated UCI engine)."""

    sync_requested = Signal(str)  # Emitted with FEN when user clicks 'Push to Main'
    request_multipv = Signal(int) # Emitted to request main engine Multi-PV adjustment

    TASK_MANUAL = 0
    TASK_LIVE_PV_1 = 1
    TASK_LIVE_PV_2 = 2
    TASK_LIVE_PV_3 = 3
    TASK_DEDICATED_ENGINE = 4

    def __init__(
        self,
        title: str = "Auxiliary Analysis Board",
        engine_registry: EngineRegistry | None = None,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle(title)
        self.engine_registry = engine_registry

        self.board = chess.Board()
        self.main_fen: str = chess.STARTING_FEN
        self.dedicated_controller: UCIController | None = None

        # UI Components
        self.board_widget = ChessBoardWidget(self)
        self.board_widget.set_board(self.board)

        self.task_combo = QComboBox(self)
        self.task_combo.addItem("Task: Manual Exploration")
        self.task_combo.addItem("Task: Live PV #1 Follower")
        self.task_combo.addItem("Task: Live PV #2 Follower")
        self.task_combo.addItem("Task: Live PV #3 Follower")

        # Add registered engines for dedicated background analysis
        if self.engine_registry:
            for eng in self.engine_registry.engines:
                if not eng.is_hardware:
                    self.task_combo.addItem(f"Dedicated Engine: {eng.name}", eng.path)

        self.status_label = QLabel("Task: Manual Exploration | Position: Start", self)
        self.status_label.setStyleSheet("color: #0066CC; font-weight: bold; font-size: 11px; padding: 2px;")

        self.pv_edit = QPlainTextEdit(self)
        self.pv_edit.setReadOnly(True)
        self.pv_edit.setFixedHeight(55)
        self.pv_edit.setStyleSheet("color: #0055AA; font-size: 11px; font-family: monospace; border: 1px solid #E0E0E0; background-color: #FAFAFA;")
        self.pv_edit.setPlainText("PV: -")

        self.btn_pull = QPushButton("Pull Main", self)
        self.btn_push = QPushButton("Push Main", self)
        self.btn_analyze = QPushButton("Analyze", self)
        self.btn_flip = QPushButton("Flip", self)

        self.btn_pull.setToolTip("Pull current position from Main Board into this auxiliary board")
        self.btn_push.setToolTip("Push this auxiliary board's position onto Main Board")
        self.btn_analyze.setToolTip("Run dedicated engine analysis on current position")

        self.btn_pull.clicked.connect(self.on_pull_main_clicked)
        self.btn_push.clicked.connect(self.on_push_main_clicked)
        self.btn_analyze.clicked.connect(self.on_start_dedicated_analysis)
        self.btn_flip.clicked.connect(self.on_flip_clicked)
        self.task_combo.currentIndexChanged.connect(self.on_task_changed)
        self.board_widget.user_move_made.connect(self.on_user_move)

        btn_layout = QHBoxLayout()
        btn_layout.addWidget(self.btn_pull)
        btn_layout.addWidget(self.btn_push)
        btn_layout.addWidget(self.btn_analyze)
        btn_layout.addWidget(self.btn_flip)

        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(4, 4, 4, 4)
        main_layout.addWidget(self.task_combo)
        main_layout.addWidget(self.status_label)
        main_layout.addWidget(self.pv_edit)
        main_layout.addWidget(self.board_widget, stretch=1)
        main_layout.addLayout(btn_layout)

    def set_position_fen(self, fen: str) -> None:
        """Set board position directly from FEN string."""
        try:
            self.board = chess.Board(fen)
            self.board_widget.set_board(self.board)
            self.status_label.setText(f"Position Loaded | Turn: {'White' if self.board.turn == chess.WHITE else 'Black'}")
            if self.task_combo.currentIndex() >= self.TASK_DEDICATED_ENGINE:
                self.on_start_dedicated_analysis()
        except ValueError:
            pass

    def set_main_board_fen(self, fen: str) -> None:
        """Called whenever the main board position changes."""
        self.main_fen = fen
        current_task = self.task_combo.currentIndex()

        if current_task in (self.TASK_LIVE_PV_1, self.TASK_LIVE_PV_2, self.TASK_LIVE_PV_3):
            # Update board to match main board position
            try:
                self.board = chess.Board(fen)
                self.board_widget.set_board(self.board)
                self.status_label.setText(f"Following Main Board | Turn: {'White' if self.board.turn == chess.WHITE else 'Black'}")
            except ValueError:
                pass
        elif current_task >= self.TASK_DEDICATED_ENGINE:
            # Auto-run dedicated engine analysis on new position
            try:
                self.board = chess.Board(fen)
                self.board_widget.set_board(self.board)
                self.on_start_dedicated_analysis()
            except ValueError:
                pass

    def update_live_pv(self, multipv_idx: int, pv_moves: list[str], score_str: str, depth: int) -> None:
        """Called when main engine emits live search progress."""
        current_task = self.task_combo.currentIndex()
        target_pv = 1
        if current_task == self.TASK_LIVE_PV_2:
            target_pv = 2
        elif current_task == self.TASK_LIVE_PV_3:
            target_pv = 3

        if current_task in (self.TASK_LIVE_PV_1, self.TASK_LIVE_PV_2, self.TASK_LIVE_PV_3):
            if multipv_idx == target_pv and pv_moves:
                # Play out PV line starting from current main board FEN
                try:
                    pv_board = chess.Board(self.main_fen)
                    for m_str in pv_moves:
                        try:
                            m = chess.Move.from_uci(m_str)
                            if m in pv_board.legal_moves:
                                pv_board.push(m)
                            else:
                                break
                        except ValueError:
                            break
                    self.board_widget.set_board(pv_board)
                    self.status_label.setText(f"Live PV #{multipv_idx} ({score_str}) | Depth: {depth}")
                    self.pv_edit.setPlainText(f"PV #{multipv_idx} ({score_str}): {' '.join(pv_moves)}")
                except ValueError:
                    pass

    @Slot(int)
    def on_task_changed(self, index: int) -> None:
        """Handle task mode change."""
        if index in (self.TASK_LIVE_PV_1, self.TASK_LIVE_PV_2, self.TASK_LIVE_PV_3):
            self._stop_dedicated_controller()
            self.set_main_board_fen(self.main_fen)
            if index == self.TASK_LIVE_PV_2:
                self.request_multipv.emit(2)
            elif index == self.TASK_LIVE_PV_3:
                self.request_multipv.emit(3)
        elif index >= self.TASK_DEDICATED_ENGINE:
            eng_path = self.task_combo.currentData()
            if eng_path:
                self._init_dedicated_controller(eng_path)
                self.set_main_board_fen(self.main_fen)
        else:
            self._stop_dedicated_controller()
            self.status_label.setText("Task: Manual Exploration")
            self.pv_edit.setPlainText("PV: -")

    def _init_dedicated_controller(self, eng_path: str) -> None:
        """Initialize dedicated background UCI engine instance (supports XBoard via XBoardAdapter)."""
        self._stop_dedicated_controller()
        eng_info = self.engine_registry.get_engine_by_path(eng_path) if self.engine_registry else None
        
        if eng_info and eng_info.is_xboard:
            self.dedicated_controller = XBoardAdapter(eng_path, args=eng_info.args, parent=self)
        else:
            uci_ctrl = UCIController(self)
            uci_ctrl.engine_path = eng_path
            if eng_info:
                uci_ctrl.engine_args = eng_info.args
            self.dedicated_controller = uci_ctrl

        self.dedicated_controller.search_progress.connect(self.on_dedicated_progress)
        self.dedicated_controller.start_engine()

    def _stop_dedicated_controller(self) -> None:
        """Stop and cleanup dedicated engine controller."""
        if self.dedicated_controller:
            self.dedicated_controller.stop_engine()
            self.dedicated_controller = None

    @Slot()
    def on_start_dedicated_analysis(self) -> None:
        """Trigger dedicated engine search on current board position."""
        if self.dedicated_controller and self.dedicated_controller.is_running:
            current_fen = self.board.fen()
            self.dedicated_controller.set_position(fen=current_fen)
            self.dedicated_controller.start_search_time(3000)

    @Slot(dict)
    def on_dedicated_progress(self, info: dict) -> None:
        """Update status label, PV edit, and board from dedicated background engine progress."""
        depth = info.get("depth", 0)
        pv_moves = info.get("pv", [])

        if "score_cp" in info:
            cp = info["score_cp"]
            w_cp = cp if self.board.turn == chess.WHITE else -cp
            score_str = f"{'+' if w_cp >= 0 else ''}{w_cp / 100.0:.2f}"
        elif "score_mate" in info:
            score_str = f"M{info['score_mate']}"
        else:
            score_str = "+0.00"

        if pv_moves:
            self.pv_edit.setPlainText(f"PV ({score_str}): {' '.join(pv_moves)}")
            try:
                pv_board = self.board.copy()
                for m_str in pv_moves:
                    m = chess.Move.from_uci(m_str)
                    if m in pv_board.legal_moves:
                        pv_board.push(m)
                    else:
                        break
                self.board_widget.set_board(pv_board)
            except ValueError:
                pass

        eng_name = "Dedicated Engine"
        if self.dedicated_controller and self.dedicated_controller.engine_name:
            eng_name = self.dedicated_controller.engine_name
        self.status_label.setText(f"{eng_name} | Score: {score_str} | Depth: {depth}")

    @Slot(str)
    def on_user_move(self, uci_move: str) -> None:
        """Play move on auxiliary board manually."""
        try:
            move = chess.Move.from_uci(uci_move)
            if move in self.board.legal_moves:
                self.board.push(move)
                self.board_widget.set_board(self.board)
                self.status_label.setText(f"Manual Move: {uci_move}")
                if self.task_combo.currentIndex() >= self.TASK_DEDICATED_ENGINE:
                    self.on_start_dedicated_analysis()
        except ValueError:
            pass

    @Slot()
    def on_pull_main_clicked(self) -> None:
        """Pull position from Main Board into this auxiliary board."""
        try:
            self.board = chess.Board(self.main_fen)
            self.board_widget.set_board(self.board)
            self.status_label.setText("Pulled position from Main Board.")
            if self.task_combo.currentIndex() >= self.TASK_DEDICATED_ENGINE:
                self.on_start_dedicated_analysis()
        except ValueError:
            pass

    @Slot()
    def on_push_main_clicked(self) -> None:
        """Emit request to push auxiliary position onto Main Board."""
        self.sync_requested.emit(self.board.fen())

    @Slot()
    def on_flip_clicked(self) -> None:
        """Toggle board perspective orientation."""
        self.board_widget.flip_board()

    def closeEvent(self, event) -> None:
        """Ensure dedicated engine process is terminated when widget closes."""
        self._stop_dedicated_controller()
        super().closeEvent(event)
