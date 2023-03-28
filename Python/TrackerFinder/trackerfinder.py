import sys
import signal
from argparse import ArgumentParser
from PyQt5.QtWidgets import QApplication
from PyQt5.QtCore import QTimer
from TrackerAX25 import TrackerAX25
from TrackerFinderMainWindow import TrackerFinderMainWindow
#
# Python modules:
#  PyQt5
#  pytq5-tools
# 
def sigint_handler(x, y):
    '''Terminate the trackerAx25 thread

    Necessary because it is blocked on a read, and
    so the event loop is not running.
    '''
    trackerAx25.terminate()
    sys.exit(0)

def closeApp()->None:
    sigint_handler(None, None)

trackerAX25 = None

if __name__ == "__main__":
    import sys

    def parseArgs() -> dict:
        parser = ArgumentParser()

        if sys.platform == 'win32':
            parser.add_argument("-d", "--device", help="APRS device",
                    action="store", default='COM4')
        else:
            parser.add_argument("-d", "--device", help="APRS device",
                    action="store", default='/dev/tty.TH-D74')

        return parser.parse_args()

    args = parseArgs()

    app = QApplication(sys.argv)

    # Create the main window
    mainWindow = TrackerFinderMainWindow()
    mainWindow.closeSignal.connect(closeApp)
    mainWindow.show()

    # Create and start the ax25 reader thread.
    trackerAx25 = TrackerAX25(device=args.device)
    trackerAx25.aprsSignal.connect(mainWindow.aprsMsg)
    trackerAx25.posPingSignal.connect(mainWindow.pingMsg)
    trackerAx25.start()

    # Catch SIG_INT
    signal.signal(signal.SIGINT, sigint_handler)

    # Running the timer insures that the event loop runs
    timer = QTimer()
    timer.timeout.connect(lambda: None)
    timer.start(100)

    app.exec()
