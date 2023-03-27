import sys
from argparse import ArgumentParser
from PyQt5.QtWidgets import QApplication
from TrackerAX25 import TrackerAX25
from TrackerFinderMainWindow import TrackerFinderMainWindow
#
# Python modules:
#  PyQt5
#  pytq5-tools
# 
def newMsg(msg: dict)->None:
    print(msg)

if __name__ == "__main__":
    import sys

    def parseArgs() -> dict:
        parser = ArgumentParser()
        parser.add_argument("-d", "--device", help="APRS device",
                    action="store", default='/dev/tty.TH-D74')

        return parser.parse_args()

    args = parseArgs()

    app = QApplication(sys.argv)

    # Create the main window
    mainWindow = TrackerFinderMainWindow()
    mainWindow.show()

    # Create and start the ax25 reader thread.
    trackerAx25 = TrackerAX25(device=args.device)
    trackerAx25.aprsSignal.connect(mainWindow.aprsMsg)
    trackerAx25.posPingSignal.connect(mainWindow.pingMsg)
    trackerAx25.start()

    app.exec()
