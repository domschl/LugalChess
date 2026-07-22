"""Main PySide6 LugalChess GUI Application Window."""

import os
import sys
import chess
from PySide6.QtCore import Qt, Slot
from PySide6.QtGui import QAction, QIcon, QKeySequence
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QDockWidget,
    QFileDialog,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QSplitter,
    QStatusBar,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

from lugalgui.controllers.rp2350_controller import RP2350Controller
from lugalgui.controllers.uci_controller import UCIController
from lugalgui.models.engine_registry import EngineRegistry
from lugalgui.models.game_tree import GameTree
from lugalgui.views.aux_board_widget import AuxBoardWidget
from lugalgui.views.board_widget import ChessBoardWidget
from lugalgui.views.eval_bar_widget import EvalBarWidget
from lugalgui.views.eval_graph_widget import EvalGraphWidget
from lugalgui.views.notation_widget import NotationWidget


class MainWindow(QMainWindow):
    """Main Application Desktop Window for LugalChess GUI."""

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("LugalChess GUI 1.0 - Multi-Engine Analysis Workspace")
        self.resize(1180, 800)

        # Core Models, Engine Registry, and Controllers
        self.game_tree: GameTree = GameTree()
        self.engine_registry: EngineRegistry = EngineRegistry(self)
        self.uci_controller: UCIController = UCIController()
        self.rp2350_controller: RP2350Controller = RP2350Controller()
        
        self.search_level: int = 2
        self.level_times_ms: list[int] = [1000, 2000, 5000, 10000, 15000, 30000, 60000, -1]
        self.custom_time_ms: int | None = None
        self.custom_depth: int | None = None

        self.eval_history: list[tuple[int, float]] = []
        self.aux_boards: list[QDockWidget] = []

        # Default local engine executable
        project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        default_engine = os.path.join(project_root, "build", "engine", "lugalchess")
        if os.path.exists(default_engine):
            self.uci_controller.engine_path = default_engine

        # Initialize User Interface Widgets
        self._init_ui()
        self._connect_signals()

        # Auto-start UCI engine if available
        if self.uci_controller.engine_path:
            self.uci_controller.start_engine()

        # Start RP2350 USB CDC serial autodetect
        self.rp2350_controller.start_autodetect()

    def _init_ui(self) -> None:
        """Create and layout application UI components."""
        # 1. Main Central Area: Vertical Eval Bar + Graphical Chess Board
        central_container = QWidget(self)
        central_layout = QHBoxLayout(central_container)
        central_layout.setContentsMargins(6, 6, 6, 6)

        self.eval_bar_widget = EvalBarWidget(self)
        self.board_widget = ChessBoardWidget(self)

        central_layout.addWidget(self.eval_bar_widget)
        central_layout.addWidget(self.board_widget, stretch=1)
        self.setCentralWidget(central_container)

        # 2. Dock Panels: Move History & Engine Evaluation
        self.notation_widget = NotationWidget(self)
        
        self.dock_notation = QDockWidget("Move History & Notation", self)
        self.dock_notation.setWidget(self.notation_widget)
        self.dock_notation.setMinimumWidth(280)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.dock_notation)

        # 3. Engine Log & Evaluation Panel
        self.eval_label = QLabel("Score: +0.00 | Depth: 0", self)
        self.eval_label.setStyleSheet("font-size: 14px; font-weight: bold; padding: 4px;")
        
        self.pv_label = QLabel("PV: -", self)
        self.pv_label.setWordWrap(True)
        self.pv_label.setFixedHeight(44)  # Reserve steady 2-line height to prevent layout wobble
        self.pv_label.setStyleSheet("color: #0066CC; padding: 4px;")

        self.engine_log_edit = QPlainTextEdit(self)
        self.engine_log_edit.setReadOnly(True)

        # Engine Target Selector (Populated from EngineRegistry)
        self.engine_target_combo = QComboBox(self)
        self._populate_engine_target_combo()

        self.multipv_combo = QComboBox(self)
        self.multipv_combo.addItem("Multi-PV: 1 Line")
        self.multipv_combo.addItem("Multi-PV: 2 Lines")
        self.multipv_combo.addItem("Multi-PV: 3 Lines")
        self.multipv_combo.addItem("Multi-PV: 5 Lines")
        self.multipv_combo.setToolTip("Configure number of variation lines searched by engine")

        combo_layout = QHBoxLayout()
        combo_layout.addWidget(self.engine_target_combo, stretch=2)
        combo_layout.addWidget(self.multipv_combo, stretch=1)

        eval_container = QWidget(self)
        eval_layout = QVBoxLayout(eval_container)
        eval_layout.setContentsMargins(4, 4, 4, 4)
        eval_layout.addLayout(combo_layout)
        eval_layout.addWidget(self.eval_label)
        eval_layout.addWidget(self.pv_label)
        eval_layout.addWidget(self.engine_log_edit)

        self.dock_eval = QDockWidget("Engine Analysis & Console", self)
        self.dock_eval.setWidget(eval_container)
        self.dock_eval.setMinimumWidth(340)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.dock_eval)

        # 4. Advantage Timeline Chart Dock Panel
        self.eval_graph_widget = EvalGraphWidget(self)
        self.dock_graph = QDockWidget("Evaluation History & Advantage Timeline", self)
        self.dock_graph.setWidget(self.eval_graph_widget)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self.dock_graph)

        # 4. Status Bar & Indicators
        self.status_bar = QStatusBar(self)
        self.setStatusBar(self.status_bar)
        
        self.status_game_label = QLabel("Ready", self)
        self.status_hardware_label = QLabel("RP2350: Disconnected", self)
        self.status_hardware_label.setStyleSheet("color: gray; padding-right: 10px;")
        
        self.status_bar.addWidget(self.status_game_label, 1)
        self.status_bar.addPermanentWidget(self.status_hardware_label)

        # 5. Menu Bar & Actions
        self._create_menus_and_toolbars()

    def _create_menus_and_toolbars(self) -> None:
        """Build main window menus, toolbars, and keyboard shortcuts."""
        menubar = self.menuBar()

        # File Menu
        file_menu = menubar.addMenu("&File")
        
        act_new = QAction("&New Game", self)
        act_new.setShortcut(QKeySequence.StandardKey.New)
        act_new.triggered.connect(self.on_new_game)
        file_menu.addAction(act_new)

        act_fen = QAction("Load &FEN Position...", self)
        act_fen.triggered.connect(self.on_load_fen)
        file_menu.addAction(act_fen)

        file_menu.addSeparator()
        act_quit = QAction("&Quit", self)
        act_quit.setShortcut(QKeySequence.StandardKey.Quit)
        act_quit.triggered.connect(self.close)
        file_menu.addAction(act_quit)

        # Game Menu
        game_menu = menubar.addMenu("&Game")

        act_undo = QAction("&Undo Move", self)
        act_undo.setShortcut(QKeySequence("Ctrl+Z"))
        act_undo.triggered.connect(self.on_undo_move)
        game_menu.addAction(act_undo)

        act_flip = QAction("&Flip Board Orientation", self)
        act_flip.setShortcut(QKeySequence("Ctrl+F"))
        act_flip.triggered.connect(self.board_widget.flip_board)
        game_menu.addAction(act_flip)

        # Engine Menu
        engine_menu = menubar.addMenu("&Engine")

        act_go = QAction("&Engine Play Move (Go)", self)
        act_go.setShortcut(QKeySequence("Space"))
        act_go.triggered.connect(self.on_engine_go)
        engine_menu.addAction(act_go)

        act_stop = QAction("&Stop Thinking", self)
        act_stop.triggered.connect(self.on_engine_stop)
        engine_menu.addAction(act_stop)

        engine_menu.addSeparator()
        act_select_engine = QAction("Select &UCI Engine Executable...", self)
        act_select_engine.triggered.connect(self.on_select_engine_binary)
        engine_menu.addAction(act_select_engine)

        # Level Menu (Levels 1 to 8 + Custom)
        level_menu = menubar.addMenu("&Level")
        time_labels = ["Level 1 (1s)", "Level 2 (2s)", "Level 3 (5s)", "Level 4 (10s)", "Level 5 (15s)", "Level 6 (30s)", "Level 7 (60s)", "Level 8 (Infinite)"]
        for i, lbl in enumerate(time_labels, start=1):
            act_lvl = QAction(lbl, self)
            act_lvl.triggered.connect(lambda _, lvl=i: self.on_select_level(lvl))
            level_menu.addAction(act_lvl)

        level_menu.addSeparator()
        act_custom_level = QAction("Custom Level / Time Control...", self)
        act_custom_level.triggered.connect(self.on_custom_level_dialog)
        level_menu.addAction(act_custom_level)

        # View Menu
        view_menu = menubar.addMenu("&View")
        act_add_aux = QAction("&Add Auxiliary Analysis Board", self)
        act_add_aux.triggered.connect(self.on_add_aux_board)
        view_menu.addAction(act_add_aux)
        view_menu.addSeparator()

        view_menu.addAction(self.dock_notation.toggleViewAction())
        view_menu.addAction(self.dock_eval.toggleViewAction())
        view_menu.addAction(self.dock_graph.toggleViewAction())

        # Main Toolbar
        toolbar = QToolBar("Main Toolbar", self)
        self.addToolBar(toolbar)
        toolbar.addAction(act_new)
        toolbar.addAction(act_undo)
        toolbar.addAction(act_flip)
        toolbar.addAction(act_go)

    def _connect_signals(self) -> None:
        """Connect Signals and Slots between Controllers and Views."""
        # Board user move signal
        self.board_widget.user_move_made.connect(self.on_user_move)

        # Move notation history click signal
        self.notation_widget.move_selected.connect(self.on_notation_move_clicked)

        # Engine Target Dropdown & Registry Signals
        self.engine_target_combo.currentIndexChanged.connect(self.on_engine_target_changed)
        self.engine_registry.registry_updated.connect(self._populate_engine_target_combo)

        # Multi-PV Combo & Timeline Graph Click Signals
        self.multipv_combo.currentIndexChanged.connect(self.on_multipv_changed)
        self.eval_graph_widget.move_clicked.connect(self.on_notation_move_clicked)

        # UCI Controller Signals
        self.uci_controller.engine_started.connect(self.on_engine_started)
        self.uci_controller.search_progress.connect(self.on_search_progress)
        self.uci_controller.best_move_found.connect(self.on_best_move_found)
        self.uci_controller.log_received.connect(self.on_engine_log_line)

        # RP2350 Hardware Signals
        self.rp2350_controller.device_connected.connect(self.on_rp2350_connected)
        self.rp2350_controller.device_disconnected.connect(self.on_rp2350_disconnected)
        self.rp2350_controller.move_received.connect(self.on_rp2350_move)
        self.rp2350_controller.search_progress.connect(self.on_search_progress)
        self.rp2350_controller.line_received.connect(self.on_engine_log_line)
        self.rp2350_controller.new_game_received.connect(self.on_remote_new_game)

    @Slot()
    def on_new_game(self) -> None:
        """Reset game to starting position."""
        self.game_tree.reset_to_start()
        self.eval_history = []
        self.eval_graph_widget.set_eval_history(self.eval_history)
        self.eval_bar_widget.set_eval(0)
        self._update_board_and_notation()
        self.status_game_label.setText("New game started.")
        self.eval_label.setText("Score: +0.00 | Depth: 0")
        self.pv_label.setText("PV: -")
        
        # Notify active engine target
        if self._is_rp2350_selected():
            self.rp2350_controller.send_command("ucinewgame")
            self.rp2350_controller.send_command("position startpos")
        else:
            self.uci_controller.send_command("ucinewgame")

    @Slot()
    def on_remote_new_game(self) -> None:
        """Called when RP2350 hardware triggers new game."""
        self.game_tree.reset_to_start()
        self._update_board_and_notation()
        self.status_game_label.setText("New game started on RP2350 hardware.")
        self.eval_label.setText("Score: +0.00 | Depth: 0")
        self.pv_label.setText("PV: -")

    @Slot()
    def on_load_fen(self) -> None:
        """Prompt user for a FEN string."""
        fen, ok = QInputDialog.getText(self, "Load FEN Position", "Enter valid FEN string:")
        if ok and fen:
            if self.game_tree.load_fen(fen.strip()):
                self._update_board_and_notation()
                self.status_game_label.setText("FEN position loaded.")
            else:
                QMessageBox.warning(self, "Error", "Invalid FEN position string!")

    def _is_rp2350_selected(self) -> bool:
        """Return True if RP2350 USB Hardware Engine is selected target."""
        return self.engine_target_combo.currentData() == "RP2350_USB_CDC"

    @Slot(str)
    def on_user_move(self, uci_move: str) -> None:
        """Handle move played on the graphical chessboard."""
        if self.game_tree.push_uci_str(uci_move):
            self._update_board_and_notation()
            self.status_game_label.setText(self.game_tree.get_status_str())
            
            # Send updated position to active engine target
            moves_list = [m.uci() for m in self.game_tree.move_history]
            if self._is_rp2350_selected():
                self.rp2350_controller.set_position(moves=moves_list)
            else:
                self.uci_controller.set_position(moves=moves_list)

    @Slot()
    def on_undo_move(self) -> None:
        """Undo last half-move."""
        if self.game_tree.pop_move():
            self._update_board_and_notation()
            self.status_game_label.setText(self.game_tree.get_status_str())

    @Slot(int)
    def on_select_level(self, level: int) -> None:
        """Update active engine search level."""
        self.search_level = level
        self.custom_time_ms = None
        self.custom_depth = None
        t_ms = self.level_times_ms[level - 1]
        desc = f"{t_ms // 1000}s per move" if t_ms > 0 else "Infinite"
        self.status_game_label.setText(f"Engine search level set to Level {level} ({desc}).")

    @Slot()
    def on_custom_level_dialog(self) -> None:
        """Prompt user for custom search time limit or fixed ply depth."""
        val_str, ok = QInputDialog.getText(
            self,
            "Custom Level / Time Control",
            "Enter custom time limit (e.g. '3.5s', '12s', '90s') OR fixed depth (e.g. '8d', '12d'):"
        )
        if ok and val_str:
            val_clean = val_str.strip().lower()
            if val_clean.endswith("d") or val_clean.endswith("p"):
                try:
                    d = int(val_clean.rstrip("dp"))
                    if 1 <= d <= 64:
                        self.custom_depth = d
                        self.custom_time_ms = None
                        self.status_game_label.setText(f"Custom search level set to Fixed Depth {d} plies.")
                    else:
                        QMessageBox.warning(self, "Invalid Depth", "Depth must be between 1 and 64 plies.")
                except ValueError:
                    QMessageBox.warning(self, "Invalid Depth", "Invalid depth string.")
            else:
                try:
                    sec = float(val_clean.rstrip("s"))
                    if sec > 0:
                        ms = int(sec * 1000)
                        self.custom_time_ms = ms
                        self.custom_depth = None
                        self.status_game_label.setText(f"Custom search level set to {sec:.1f}s per move.")
                    else:
                        QMessageBox.warning(self, "Invalid Time", "Time limit must be positive.")
                except ValueError:
                    QMessageBox.warning(self, "Invalid Time", "Please specify e.g. '5s' or '10d'.")

    @Slot()
    def on_engine_go(self) -> None:
        """Force engine to calculate and make a move."""
        if self.game_tree.is_game_over():
            return
            
        moves_list = [m.uci() for m in self.game_tree.move_history]
        
        # Determine time limit or depth limit
        t_ms = self.level_times_ms[self.search_level - 1]
        if self.custom_time_ms is not None:
            t_ms = self.custom_time_ms

        is_rp2350 = self._is_rp2350_selected()
        controller = self.rp2350_controller if is_rp2350 else self.uci_controller

        if is_rp2350 and not self.rp2350_controller.serial_inst:
            QMessageBox.warning(self, "RP2350 Disconnected", "RP2350 hardware is not connected over USB serial!")
            return

        controller.set_position(moves=moves_list)
        
        target_name = "RP2350" if is_rp2350 else "Local Engine"
        if self.custom_depth is not None:
            self.status_game_label.setText(f"{target_name} is thinking (Depth {self.custom_depth})...")
            controller.start_search_depth(self.custom_depth)
        else:
            desc = f"{t_ms / 1000.0:.1f}s" if t_ms > 0 else "Infinite"
            self.status_game_label.setText(f"{target_name} is thinking ({desc})...")
            controller.start_search_time(t_ms)

    @Slot()
    def on_engine_stop(self) -> None:
        """Stop active engine search."""
        if self._is_rp2350_selected():
            self.rp2350_controller.stop_search()
        else:
            self.uci_controller.stop_search()

    @Slot(str)
    def on_engine_started(self, engine_name: str) -> None:
        """Called when UCI engine process responds with uciok."""
        self.status_game_label.setText(f"Engine connected: {engine_name}")

    @Slot(int)
    def on_engine_target_changed(self, index: int) -> None:
        """Handle engine target dropdown selection change."""
        if not self._is_rp2350_selected():
            eng_path = self.engine_target_combo.currentData()
            if eng_path and isinstance(eng_path, str) and eng_path != self.uci_controller.engine_path:
                self.uci_controller.engine_path = eng_path
                self.uci_controller.start_engine()

    def _populate_engine_target_combo(self) -> None:
        """Populate main engine target combo box from EngineRegistry."""
        self.engine_target_combo.clear()
        for eng in self.engine_registry.engines:
            if eng.is_hardware:
                self.engine_target_combo.addItem(f"Target: {eng.name}", eng.path)
            else:
                self.engine_target_combo.addItem(f"Target: {eng.name}", eng.path)

    @Slot(dict)
    def on_search_progress(self, info: dict) -> None:
        """Update live analysis panel with search depth, centipawn/mate score, PV line, and eval widgets."""
        depth = info.get("depth", 0)
        ply = len(self.game_tree.move_history)
        multipv_idx = info.get("multipv", 1)
        pv_moves = info.get("pv", [])
        
        # Score parsing
        if "score_mate" in info:
            m = info["score_mate"]
            score_str = f"Mate in {m}"
            self.eval_bar_widget.set_eval(None, is_mate=True, mate_in=m)
        elif "score_cp" in info:
            cp = info["score_cp"]
            # Format from White's perspective
            w_cp = cp if self.game_tree.board.turn == chess.WHITE else -cp
            score_str = f"{'+' if w_cp >= 0 else ''}{w_cp / 100.0:.2f}"
            self.eval_bar_widget.set_eval(w_cp)
            
            # Record in timeline history graph
            self._record_eval(ply, float(w_cp))
        else:
            score_str = "+0.00"

        self.eval_label.setText(f"Score: {score_str} | Depth: {depth}")

        if pv_moves:
            pv_str = " ".join(pv_moves)
            if multipv_idx > 1:
                self.pv_label.setText(f"PV #{multipv_idx} ({score_str}): {pv_str}")
            else:
                self.pv_label.setText(f"PV: {pv_str}")

        # Broadcast live search variation to all open auxiliary analysis boards
        for dock in self.aux_boards:
            widget = dock.widget()
            if isinstance(widget, AuxBoardWidget):
                widget.update_live_pv(multipv_idx, pv_moves, score_str, depth)

    def _record_eval(self, ply: int, score_cp: float) -> None:
        """Record evaluation score for current ply in timeline graph dataset."""
        # Replace existing entry for ply if already present, or append
        filtered = [entry for entry in self.eval_history if entry[0] != ply]
        filtered.append((ply, score_cp))
        filtered.sort(key=lambda x: x[0])
        self.eval_history = filtered
        self.eval_graph_widget.set_eval_history(self.eval_history)

    @Slot(int)
    def on_multipv_changed(self, index: int) -> None:
        """Update Multi-PV count on engine."""
        counts = [1, 2, 3, 5]
        count = counts[index] if 0 <= index < len(counts) else 1
        self.uci_controller.set_multipv(count)
        self.status_game_label.setText(f"Engine Multi-PV search set to {count} variation lines.")

    @Slot(int)
    def on_aux_request_multipv(self, count: int) -> None:
        """Ensure main engine Multi-PV setting is at least equal to requested count."""
        index_map = {1: 0, 2: 1, 3: 2, 5: 3}
        if count in index_map:
            target_idx = index_map[count]
            if self.multipv_combo.currentIndex() < target_idx:
                self.multipv_combo.setCurrentIndex(target_idx)

    @Slot()
    def on_add_aux_board(self) -> None:
        """Create and display a new auxiliary analysis board dock widget."""
        count = len(self.aux_boards) + 1
        aux_widget = AuxBoardWidget(f"Auxiliary Board #{count}", self.engine_registry, self)
        aux_widget.set_main_board_fen(self.game_tree.board.fen())

        dock_aux = QDockWidget(f"Auxiliary Analysis Board #{count}", self)
        dock_aux.setWidget(aux_widget)
        dock_aux.setMinimumWidth(300)

        aux_widget.sync_requested.connect(self.on_sync_aux_to_main)
        aux_widget.request_multipv.connect(self.on_aux_request_multipv)

        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, dock_aux)
        self.aux_boards.append(dock_aux)

    @Slot(str)
    def on_sync_aux_to_main(self, fen: str) -> None:
        """Sync position from auxiliary board to main board."""
        if self.game_tree.load_fen(fen):
            self.board_widget.set_board(self.game_tree.board)
            self.notation_widget.update_history(self.game_tree.get_san_history())
            self.status_game_label.setText("Synced position from Auxiliary Board to Main Board.")

    @Slot(str, str)
    def on_best_move_found(self, best_move: str, ponder_move: str) -> None:
        """Called when engine outputs bestmove."""
        if best_move and not self.game_tree.is_game_over():
            if self.game_tree.push_uci_str(best_move):
                self._update_board_and_notation()
                self.status_game_label.setText(f"Engine played {best_move}. {self.game_tree.get_status_str()}")

    @Slot(str)
    def on_engine_log_line(self, line: str) -> None:
        """Append raw engine log line to console output edit box."""
        self.engine_log_edit.appendPlainText(line)

    @Slot()
    def on_select_engine_binary(self) -> None:
        """Open file dialog to select external UCI engine binary."""
        file_path, _ = QFileDialog.getOpenFileName(self, "Select UCI Engine Executable", "", "Executables (*)")
        if file_path:
            name = os.path.basename(file_path)
            self.engine_registry.add_custom_engine(name, file_path)
            self.uci_controller.engine_path = file_path
            self.uci_controller.start_engine()

    @Slot(int)
    def on_notation_move_clicked(self, ply: int) -> None:
        """Navigate to specific ply index in game history."""
        while len(self.game_tree.move_history) > ply:
            self.game_tree.pop_move()
        self._update_board_and_notation()

    @Slot(str)
    def on_rp2350_connected(self, port_name: str) -> None:
        """Update UI status indicator when RP2350 USB CDC serial port connects."""
        self.status_hardware_label.setText(f"RP2350: Connected ({port_name})")
        self.status_hardware_label.setStyleSheet("color: #00AA00; font-weight: bold; padding-right: 10px;")

    @Slot()
    def on_rp2350_disconnected(self) -> None:
        """Update UI status indicator when RP2350 disconnects."""
        self.status_hardware_label.setText("RP2350: Disconnected")
        self.status_hardware_label.setStyleSheet("color: gray; padding-right: 10px;")

    @Slot(str)
    def on_rp2350_move(self, uci_move: str) -> None:
        """Handle move received from connected RP2350 serial stream."""
        if self.game_tree.push_uci_str(uci_move):
            self._update_board_and_notation()
            self.status_game_label.setText(f"RP2350 move: {uci_move}")

    def _update_board_and_notation(self) -> None:
        """Helper to sync view states with game model."""
        last_move = self.game_tree.move_history[-1] if self.game_tree.move_history else None
        self.board_widget.set_board(self.game_tree.board, last_move=last_move)
        self.notation_widget.update_history(self.game_tree.get_san_history())
        self._notify_aux_boards_position_changed()

    def _notify_aux_boards_position_changed(self) -> None:
        """Broadcast current main board FEN position to all open auxiliary boards."""
        fen = self.game_tree.board.fen()
        for dock in self.aux_boards:
            widget = dock.widget()
            if isinstance(widget, AuxBoardWidget):
                widget.set_main_board_fen(fen)


def main() -> None:
    """Application entry point."""
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
