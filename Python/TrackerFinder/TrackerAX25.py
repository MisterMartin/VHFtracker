"""Process AC25 messages from a LASP VHF Tracker

The tracker AX25 messages are received on a serial device.

PyQt5 provides threading support. Pyserial is used for serial
device interfacing.

pip install pyqt5 pyqt5-tools pyserial
"""

import sys
from serial import Serial
from PyQt5.QtCore import QObject, QTimer, QIODevice, QThread, QFile, pyqtSignal
from datetime import datetime

class TrackerAX25(QThread):
    '''Read AX.25 messages from the VHFTracker, decode, and deliver as signals.

    This is run as a thread in order to allow the Qt event loop to continue 
    during a blocking read. It will perform the read, waiting for
    each character of the AX.25 packet to come in. When the 0xc0 message
    terminator is received, the message is decoded.

    There are two types of messages: an APRS message, and our own
    compact, custom position ping message.

    Signals are emitted when messages have been decoded. These signals
    either contain dictionaries with the message data, or
    items that are meant to be logged.
    '''
    
    # Emitted with itemized data from an APRS message
    aprsSignal = pyqtSignal(dict)
    # Emitted with itemized data from an encoded ping
    encodedPingSignal = pyqtSignal(dict)
    # Emit a text form of either APRS or Ping data
    logSignal = pyqtSignal(str)
    # Emit a byte array of APRS or Ping data
    rawSignal = pyqtSignal(bytes)

    def __init__(self, device:str=None)->None:
        """
        Parameters
        ----------
        device: str
            The message data source
        """

        super().__init__()
        self.device = device
        self.pingCount = 0
        self.pingLastTime = None
        self.aprsLastTime = None

    def run(self)->None:
        """When start() is called on an instance of this class, this function is called.
        """

        # Open the data source.
        self.file = Serial(self.device)

        # The incoming message is built in msg.
        msg = bytearray()
        while(1):
            c = self.file.read(1)
            msg += c
            if (len(msg) > 1 and msg[-1] == 0xc0):
                if (len(msg) == 11):
                    # A ping message has been received.
                    elapsedSecs, self.pingLastTime = self.elapsedSecs(self.pingLastTime)
                    self.pingCount += 1
                    # TRK position ping
                    posDict = {
                        'id': self.pingId(msg),
                        'age': self.pingAge(msg),
                        'lat': self.pingLat(msg),
                        'lon': self.pingLon(msg),
                        'pingCount': self.pingCount,
                        'deltaTsecs': elapsedSecs
                    }
                    self.encodedPingSignal.emit(posDict)
                    self.pingLog(posDict)
                    self.rawSignal.emit(bytes(msg))
                    msg.clear()
                else:
                    # An APRS message has been received.
                    elapsedSecs, self.aprsLastTime = self.elapsedSecs(self.aprsLastTime)
                    msgDict = { 
                        'tag': self.aprsTag(msg), 
                        'time': self.pingTime(msg),
                        'lat': self.aprsLat(msg),
                        'lon': self.aprsLon(msg),
                        'failed': self.aprsFailed(msg),
                        'count': self.aprsCount(msg),
                        'alt': self.aprsAlt(msg),
                        'deltaTsecs': elapsedSecs
                    }
                    self.aprsSignal.emit(msgDict)
                    self.aprsLog(msgDict)
                    self.rawSignal.emit(bytes(msg))
                    msg.clear()

    def elapsedSecs(self, timeOfLast: datetime)->int:
        """Return the (number of seconds since timeOfLast, current time)"""

        currentTime = datetime.now()
        if not timeOfLast:
            elapsed = currentTime - currentTime
        else:
            elapsed = currentTime - timeOfLast
        timeOfLast = currentTime
        return [elapsed.seconds, currentTime]
            
    def degrees(self, b:bytearray)->float:
        bigInt = b[0]*65535 + b[1]*255 + b[0]
        degrees = bigInt / 93200.0
        return degrees
        
    def pingId(self, msg:bytearray)->int:
        return int(msg[2])

    def pingAge(self, msg:bytearray)->int:
        return int(msg[3])

    def pingLat(self, msg:bytearray)->float:
        return self.degrees(msg[4:7])

    def pingLon(self, msg:bytearray)->float:
        return self.degrees(msg[7:10])

    def pingTime(self, msg:bytearray)->str:
        return msg[33:40].decode()

    def aprsLat(self, msg:bytearray)->str:
        return msg[40:48].decode()

    def aprsLon(self, msg:bytearray)->str:
        return msg[49:58].decode()

    def aprsFailed(self, msg:bytearray)->str:
        return int(msg[59:62].decode())

    def aprsCount(self, msg:bytearray)->str:
        return int(msg[63:66].decode())

    def aprsAlt(self, msg:bytearray)->str:
        # Follows /A=
        return str(msg[69:75].decode())

    def aprsTag(self, msg:bytearray)->str:
        return msg[75:83].decode()

    def aprsLog(self, msgDict:dict)->None:
        '''Emit a signal for text logging of a decoded APRS msg'''
        logMsg = f'''\
APRS \
{msgDict['tag']} \
{msgDict['lat']} \
{msgDict['lon']} \
{msgDict['failed']} \
{msgDict['count']} \
{msgDict['alt']}\
'''
        self.logSignal.emit(logMsg)

    def pingLog(self, msgDict:dict)->None:
        '''Emit a signal for text logging of a decoded ping msg'''
        logMsg = f'''\
PING \
{msgDict['id']} \
{msgDict['age']} \
{msgDict['lat']: .4f} \
{msgDict['lon']: .4f}\
'''
        self.logSignal.emit(logMsg)
