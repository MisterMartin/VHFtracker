# VHFtracker

Based on Teensy support provided by PJRC.
## Rev A versus Rev B

Different I/O pins were used for some functions.

| Rev A          | Rev B           |REV A Pin|REV B Pin| Notes |
|----------------|-----------------|---------|---------|-------|
| 0_RX1_(GPS_TX) | 33_TX5_(GPS_RX) |     0   |    33   | Selected as Serial1 or Serial5 |
| 1_TX1_(GPS_RX) | 34_RX5_(GPS_TX) |     1   |    34   | Selected as Serial1 or Serial5 |
| 2_TX_ENABLE    | 16_TX_ENABLE    |     2   |    16   |       |
| A21_TX_AUDIO   | DAC0            |    21   |    21   |       |
| 18_SDA0_(PRESS)| (N/A)           |    18   |         |       |
| 19_SCL0_(PRESS)| (N/A)           |    19   |         |       |
| A6_3V7_VMON    |(VMON UNLABELED) |    20   |     20  |       |
| 23_5V_SHTDWN   | 15_5V_SHTDWN    |    23   |     15  |       |
| 30_GPS_PWR_EN  | 36_GPS_PWR_EN   |    30   |    36   |       |
| 31_GPS_EXTINT  |                 |    31   |         |       |
| 32_GPS_RST     |                 |    32   |         |       |
| A21_TX_AUDIO   |                 |     ?   |  (same) |       |

## Arduino IDE

- The previous IDE was a pre-packaged by PRJC, as a modified version of Arduino IDE v1.x, 
  and ran as Teensyduino.
- PJRC released a board support package which now works with Arduino IDE v2.X. Install the
  Arduino IDE first, and then add Teensy support, found on 
  the [PRJC download page](https://www.pjrc.com/teensy/td_download.html).
- For some reason, the board manager wouldn't read the PRJC board URL. I experimented with
  manually editing ~/.arduinoIDE/arduino-cli.yaml to add the URL, and restarting the 
  IDE multiple times. It finally ready the URL and provided the Teensy in the boards list.
  Perhaps the issue was due to the slow Internet access from McMurdo.

