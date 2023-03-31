
from datetime import datetime

class Logger:
    def __init__(self, fileName:str)->None:
        datestamp = datetime.now().strftime('%Y-%m-%d')
        fname = f'{datestamp}-{fileName}'
        self.textFile = open(f'{fname}.log', 'a')
        self.rawFile = open(f'{fname}.raw', 'ab')

    def log(self, text:str)->None:
        timeStamp = datetime.now()
        ts = timeStamp.isoformat(sep=' ', timespec='seconds')
        self.textFile.write(f'{ts} {text}\n')
        self.textFile.flush()

    def raw(self, rawData:bytearray)->None:
        self.rawFile.write(rawData)
        self.rawFile.flush()
