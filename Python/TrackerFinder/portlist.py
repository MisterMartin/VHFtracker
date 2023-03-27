import sys
from PyQt5.QtSerialPort import QSerialPortInfo, QSerialPort


if sys.platform == 'win32':
    ports = [p.portName() for p in QSerialPortInfo.availablePorts()]
else:
    ports = [p.systemLocation() for p in QSerialPortInfo.availablePorts()]

print(ports)
