
from datetime import datetime

class Logger:
    """A class for logging text and binary data.
    
    Files will be created/appended for individual days. The
    filenames will consist of a datestamp, followed by 
    a user specified string, and types of [.raw,.log]
    """

    def __init__(self, location:str, fileName:str)->None:
        """
        Parameters
        ----------
        location: str
            The directory where the log files will be saved.
        fileName: str
            The suffix of the log files name. 
        """

        datestamp = datetime.now().strftime('%Y-%m-%d')
        self.fname = f'{location}/{datestamp}-{fileName}'

        self.textFile = open(f'{self.fname}.log', 'a')
        self.rawFile = open(f'{self.fname}.raw', 'ab')

    def log(self, text:str)->None:
        """Write text to the .log file."""
        timeStamp = datetime.now()
        ts = timeStamp.isoformat(sep=' ', timespec='seconds')
        self.textFile.write(f'{ts} {text}\n')
        self.textFile.flush()

    def raw(self, rawData:bytearray)->None:
        """Write byes to the .raw file."""

        self.rawFile.write(rawData)
        self.rawFile.flush()

    def logFilePath(self)->str:
        return self.fname + '.log'
