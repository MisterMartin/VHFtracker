import sys
from serial import Serial
from PyQt5.QtCore import QObject, QTimer, QIODevice, QThread, QFile, pyqtSignal
from datetime import datetime

class TrackerAX25(QThread):
    '''Read AX.25 messages from the VHFTracker, decode, and deliver as signals.

    This is started as a thread. It will perform a blocking read, waiting for
    each character of the AX.25 packet to come in. When the 0xc0 message
    terminator is received, the message is decoded.

    There are two types of messages: an APRS message, and our own
    compact, custom position ping message.
    '''
    
    # Emitted when a message has been received and decoded.
    # dict: { 
    # }
    aprsSignal = pyqtSignal(dict)
    posPingSignal = pyqtSignal(dict)
    # Emit a string to go in a text log file
    logSignal = pyqtSignal(str)
    # Emit a byte array of APRS or Ping data
    rawSignal = pyqtSignal(bytes)

    def __init__(self, device:str=None)->None:
        super().__init__()
        self.device = device
        self.pingCount = 0
        self.pingLastTime = None
        self.aprsLastTime = None

    def run(self)->None:
        self.file = Serial(self.device)
        print(f'Open {self.file} {self.file} ')
        msg = bytearray()
        while(1):
            c = self.file.read(1)
            msg += c
            if (len(msg) > 1 and msg[-1] == 0xc0):
                if (len(msg) == 11):
                    elapsedSecs, self.pingLastTime = self.elapsedSecs(self.pingLastTime)
                    self.pingCount += 1
                    # TRK position ping
                    posDict = {
                        'id': self.id(msg),
                        'age': self.age(msg),
                        'lat': self.lat(msg),
                        'lon': self.lon(msg),
                        'pingCount': self.pingCount,
                        'deltaTsecs': elapsedSecs
                    }
                    self.posPingSignal.emit(posDict)
                    self.pingLog(posDict)
                    self.rawSignal.emit(bytes(msg))
                    msg.clear()
                else:
                    # APRS message
                    print('APRS msg')
                    elapsedSecs, self.aprsLastTime = self.elapsedSecs(self.aprsLastTime)
                    msgDict = { 
                        'tag': self.tag(msg), 
                        'time': self.time(msg),
                        'lat': self.latitude(msg),
                        'lon': self.longitude(msg),
                        'failed': self.failed(msg),
                        'count': self.count(msg),
                        'alt': self.altitude(msg),
                        'deltaTsecs': elapsedSecs
                    }
                    self.aprsSignal.emit(msgDict)
                    self.aprsLog(msgDict)
                    self.rawSignal.emit(bytes(msg))
                    msg.clear()
    def readReady()->None:
        print('readReady')
        print(self.file.readAll())

    def elapsedSecs(self, timeOfLast: datetime)->int:
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
        
    def id(self, msg:bytearray)->int:
        return int(msg[2])

    def age(self, msg:bytearray)->int:
        return int(msg[3])

    def lat(self, msg:bytearray)->float:
        return self.degrees(msg[4:7])

    def lon(self, msg:bytearray)->float:
        return self.degrees(msg[7:10])

    def time(self, msg:bytearray)->str:
        return msg[33:40].decode()

    def latitude(self, msg:bytearray)->str:
        return msg[40:48].decode()

    def longitude(self, msg:bytearray)->str:
        return msg[49:58].decode()

    def failed(self, msg:bytearray)->str:
        return int(msg[59:62].decode())

    def count(self, msg:bytearray)->str:
        return int(msg[63:66].decode())

    def altitude(self, msg:bytearray)->str:
        # Follows /A=
        return str(msg[69:75].decode())

    def tag(self, msg:bytearray)->str:
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
