import sys
from PyQt5.QtWidgets import QApplication, QLabel, QMainWindow, QWidget, QVBoxLayout, QSizePolicy
from PyQt5.QtCore import Qt, QTimer, QTime


class DigitalClock(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.time_label = QLabel(self)
        self.time_label.setAlignment(Qt.AlignCenter)
        self.time_label.setStyleSheet(f"font-size: 16px; font-weight: bold;")

        layout = QVBoxLayout()
        layout.addWidget(self.time_label)
        self.setLayout(layout)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_time)
        self.timer.start(1000)

    def update_time(self):
        current_time = QTime.currentTime()
        display_text = current_time.toString("hh:mm:ss")
        self.time_label.setText(display_text)

    #def resizeEvent(self, event):
    #    self.time_label.setGeometry(0, 0, self.width(), self.height())

if __name__ == "__main__":
    app = QApplication(sys.argv)
    clock = DigitalClock()
    clock.show()
    sys.exit(app.exec_())
