"""Colour-coded log view widget."""

from PySide6.QtCore import Qt, Slot
from PySide6.QtGui import QColor, QTextCharFormat, QFont
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPlainTextEdit,
    QPushButton, QComboBox, QLabel,
)

LOG_COLORS_DARK = {
    0: QColor("#FF0000"),  # Emergency
    1: QColor("#FF0000"),  # Alert
    2: QColor("#FF0000"),  # Critical
    3: QColor("#FF4444"),  # Error
    4: QColor("#FF00FF"),  # Warning
    5: QColor("#55AAFF"),  # Notice
    6: QColor("#CCCCCC"),  # Info/Normal
    7: QColor("#44FF44"),  # Debug
}

LOG_COLORS_LIGHT = {
    0: QColor("red"),
    1: QColor("red"),
    2: QColor("red"),
    3: QColor("darkRed"),
    4: QColor("magenta"),
    5: QColor("blue"),
    6: QColor("black"),
    7: QColor("darkGreen"),
}

LOG_LEVEL_NAMES = [
    "Emergency", "Alert", "Critical", "Error",
    "Warning", "Notice", "Normal", "Debug",
]


class LogWidget(QWidget):
    """A log pane with colour-coded output and level filtering."""

    def __init__(self, title="Log", dark=True, parent=None):
        super().__init__(parent)
        self._title = title
        self._min_level = 7
        self._paused = False
        self._buffer = []
        self._max_lines = 5000
        self._dark = dark
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)

        toolbar = QHBoxLayout()
        toolbar.addWidget(QLabel("Level:"))
        self._level_combo = QComboBox()
        self._level_combo.addItems(LOG_LEVEL_NAMES)
        self._level_combo.setCurrentIndex(self._min_level)
        self._level_combo.currentIndexChanged.connect(self._set_level)
        toolbar.addWidget(self._level_combo)

        self._pause_btn = QPushButton("Pause")
        self._pause_btn.setCheckable(True)
        self._pause_btn.toggled.connect(self._toggle_pause)
        toolbar.addWidget(self._pause_btn)

        self._clear_btn = QPushButton("Clear")
        self._clear_btn.clicked.connect(self._clear)
        toolbar.addWidget(self._clear_btn)

        toolbar.addStretch()
        layout.addLayout(toolbar)

        self._text = QPlainTextEdit()
        self._text.setReadOnly(True)
        self._text.setMaximumBlockCount(self._max_lines)
        font = QFont("Monospace", 9)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self._text.setFont(font)
        self._apply_theme()
        layout.addWidget(self._text)

    def _apply_theme(self):
        if self._dark:
            self._text.setStyleSheet(
                "QPlainTextEdit { background-color: #1e1e1e; color: #cccccc; }"
            )
        else:
            self._text.setStyleSheet("")

    def set_dark(self, dark):
        self._dark = dark
        self._apply_theme()

    @Slot(int)
    def _set_level(self, level):
        self._min_level = level

    @Slot(bool)
    def _toggle_pause(self, paused):
        self._paused = paused
        self._pause_btn.setText("Resume" if paused else "Pause")
        if not paused:
            for level, timestamp, text in self._buffer:
                self._append_line(level, timestamp, text)
            self._buffer.clear()

    @Slot()
    def _clear(self):
        self._text.clear()

    def append_log(self, level, timestamp, text):
        if level > self._min_level:
            return
        if self._paused:
            self._buffer.append((level, timestamp, text))
            return
        self._append_line(level, timestamp, text)

    def _append_line(self, level, timestamp, text):
        fmt = QTextCharFormat()
        colors = LOG_COLORS_DARK if self._dark else LOG_COLORS_LIGHT
        fmt.setForeground(colors.get(level, QColor("#CCCCCC") if self._dark else QColor("black")))
        if level <= 3:
            fmt.setFontWeight(QFont.Weight.Bold)

        cursor = self._text.textCursor()
        cursor.movePosition(cursor.MoveOperation.End)
        if timestamp:
            prefix_fmt = QTextCharFormat()
            prefix_fmt.setForeground(QColor("#888888"))
            cursor.insertText(f"{timestamp} ", prefix_fmt)
        cursor.insertText(f"{text}\n", fmt)
        self._text.setTextCursor(cursor)
        self._text.ensureCursorVisible()
