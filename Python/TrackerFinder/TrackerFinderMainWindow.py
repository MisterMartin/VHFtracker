"""User interface for monitoring a LASP VHF Tracker

The tracker AX25 messages are received on a serial device.

PyQt5 provides user interface and threading support.

pip install pyqt5 pyqt5-tools pyserial

A GUI definition is specified in TrackerFinderMainWindow.ui.

The .ui file is converted to the super class as follows:
    pyuic5 -x -o Ui_TrackerFinderMainWindow.py TrackerFinderMainWindow.ui

"""

import sys
from datetime import datetime
from PyQt5.QtWidgets import QApplication, QMainWindow, QMessageBox, QDesktopWidget, QHBoxLayout, QSizePolicy
from PyQt5.QtCore import QTimer, QIODevice, pyqtSignal
from Ui_TrackerFinderMainWindow import Ui_TrackerFinderMainWindow
from DigitalClock import DigitalClock
class TrackerFinderMainWindow(QMainWindow, Ui_TrackerFinderMainWindow):
    """The main window for displaying Tracker messages.

    """

    # Emitted when the window is closing
    closeSignal = pyqtSignal()

    def __init__(self)->None:
        super().__init__()
        self.setupUi(self)
        self.setWindowTitle('TrackerFinder')
        clock = DigitalClock(self.clockFrame)
        layout = QHBoxLayout()
        self.clockFrame.setLayout(layout)
        layout.addWidget(clock)


    def aprsMsg(self, msg:dict)->None:
        """Display APRS items in the GUI
        
        Parameters
        ----------
        msg: dict
            Contains the decoded APRS fields. The refeerenced
            dictionary items are REQUIRED.
        """

        self.aprsTag.setText(msg['tag'])
        self.aprsLat.setText(msg['lat'])
        self.aprsLon.setText(msg['lon'])
        self.aprsCount.setText(f"{msg['count']: d}")
        self.aprsFailed.setText(f"{msg['failed']: d}")
        self.aprsTime.setText(msg['time'])
        self.aprsAlt.setText(msg['alt'])
        self.aprsDeltaT.setText(f"{msg['deltaTsecs']: d}")

    def pingMsg(self, msg:dict)->None:
        """Display PING items in the GUI
        
        Parameters
        ----------
        msg: dict
            Contains the decoded PING fields. The refeerenced
            dictionary items are REQUIRED.
        """

        self.pingAge.setText(f"{msg['age']:d}")
        self.pingId.setText(f"{msg['id']:d}")
        self.pingLat.setText(f"{msg['lat']: .6f}")
        self.pingLon.setText(f"{msg['lon']: .6f}")
        self.pingCount.setText(f"{msg['pingCount']: d}")
        self.pingDeltaT.setText(f"{msg['deltaTsecs']: d}")

    def setStatus(self, text:str)->None:
        """Set the main window status bar"""

        self.statusBar().showMessage(text)

    def closeEvent(self, a0):
        self.closeSignal.emit()

    def addToLogWidget(self, text:str)->None:
        """Add the text to the scrolling log widget"""

        timeStamp = datetime.now()
        ts = timeStamp.isoformat(sep=' ', timespec='seconds')
        self.log.append(f'{ts} {text}')
