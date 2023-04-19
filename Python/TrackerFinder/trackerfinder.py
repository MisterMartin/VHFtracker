import sys
import os
import signal
from argparse import ArgumentParser, ArgumentDefaultsHelpFormatter
from PyQt5.QtWidgets import QApplication
from PyQt5.QtCore import QTimer
from TrackerAX25 import TrackerAX25
from TrackerFinderMainWindow import TrackerFinderMainWindow
from Logger import Logger

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
        description = '''
        Capture AX25 messages sent by a LASP VHF tracker. The two Tracker message
        types, APRS and position encoded pings, are decoded. Raw and decoded messages
        are logged to .raw and .log files.
        '''
        epilog = '''
        The messages are typically delivered via the KISS APRS link over Bluetooth,
        from a handheld radio. The operating system serial device name is likely to be
        different than the default values provided here. Use system tools to determine the
        serial port device name when connected to the Bluetooth device.
        '''
        parser = ArgumentParser(description=description, epilog=epilog,
        formatter_class=ArgumentDefaultsHelpFormatter)

        if sys.platform == 'win32':
            parser.add_argument("-d", "--device", help="APRS device",
                    action="store", default='COM4')
        else:
            parser.add_argument("-d", "--device", help="APRS device",
                    action="store", default='/dev/tty.TH-D74')
        parser.add_argument('-l', '--location', help="Location of the log files", action="store", 
            default=os.path.expanduser("~/"))

        args = parser.parse_args()
        args.location = os.path.abspath(args.location)

        return args

    # Get the argumewnts
    args = parseArgs()

    app = QApplication(sys.argv)

    # Create the main window
    mainWindow = TrackerFinderMainWindow()
    mainWindow.closeSignal.connect(closeApp)
    mainWindow.setStatus(f'Device: {args.device}, Log dir:{args.location}')
    mainWindow.show()

    # Create and start the ax25 reader thread.
    trackerAx25 = TrackerAX25(device=args.device)
    trackerAx25.aprsSignal.connect(mainWindow.aprsMsg)
    trackerAx25.encodedPingSignal.connect(mainWindow.pingMsg)
    trackerAx25.logSignal.connect(mainWindow.addToLogWidget)
    trackerAx25.start()

    # Create the logger. Will create/append a .raw and .log file
    logger = Logger(location=args.location, fileName="TrackerFinder")
    trackerAx25.logSignal.connect(logger.log)
    trackerAx25.rawSignal.connect(logger.raw)
    logger.log('--- TrackerFinder Started ---')

    mainWindow.addToLogWidget('--- TrackerFinder Started ---')
    # Catch SIG_INT
    signal.signal(signal.SIGINT, sigint_handler)

    # Running the timer insures that the event loop runs
    timer = QTimer()
    timer.timeout.connect(lambda: None)
    timer.start(100)

    app.exec()
