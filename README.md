# VHFtracker

Based on Teensy support provided by PJRC.

## Arduino Configuration
- Select the Teensy 3.6 board.
- Set the CPU speed to 24MHz.
## Hardware Revisions

Different I/O pins were used for some functions. A #define in the Arduino
sketch is used to select the targeted board.

| Rev A          | Rev B           |REV A Pin|REV B Pin| Notes |
|----------------|-----------------|---------|---------|-------|
| 0_RX1_(GPS_TX) | 33_TX5_(GPS_RX) |     0*  |    33*  | Selected as Serial1 or Serial5 |
| 1_TX1_(GPS_RX) | 34_RX5_(GPS_TX) |     1*  |    34*  | Selected as Serial1 or Serial5 |
| 2_TX_ENABLE    | 16_TX_ENABLE    |     2*  |    16*  |       |
| A21_TX_AUDIO   | DAC0            |    21   |    21   |       |
| 18_SDA0_(PRESS)| (N/A)           |    18   |         |       |
| 19_SCL0_(PRESS)| (N/A)           |    19   |         |       |
| A6_3V7_VMON    |(VMON UNLABELED) |    20   |    20   |       |
| 23_5V_SHTDWN   | 15_5V_SHTDWN    |    23*  |    15*  |       |
| 30_GPS_PWR_EN  | 36_GPS_PWR_EN   |    30*  |    36*  |       |
| 31_GPS_EXTINT  |                 |    31   |         | How is this handled for Rev B? |
| 32_GPS_RST     |                 |    32   |         | How is this handled for Rev B? |

\* = signals that must be (re)defined based on the board revision.
## Arduino IDE

- The previous IDE was a pre-packaged by PRJC, as a modified version of Arduino IDE v1.x, 
  and ran as Teensyduino.
- PJRC released a board support package which now works with Arduino IDE v2.X. Install the
  Arduino IDE first, and then configure Teensy support, found on 
  the [PRJC download page](https://www.pjrc.com/teensy/td_download.html).
- For some reason, the board manager wouldn't read the PRJC board URL. I experimented with
  manually editing ~/.arduinoIDE/arduino-cli.yaml to add the URL, and restarting the 
  IDE multiple times. It finally read the URL and provided the Teensy in the boards list.
  Perhaps the issue was due to the slow Internet access from McMurdo.

## Tracker Code

- It turned out that the +5V control was not being used for the encoded ping. For Rev A,
  the ping was working because although the 5V shutdown was disabling the 5V regulator,
  power was still available to the op amp via a bypass diode. The switch that was added
  in Rev B prevented this, and the ping was not working. Once the code was corrected,
  the ping started working.

## Batteries

- In the 2019 deployment, a couple of the trackers failed because the commercial battery
  holders were breaking during landing impact. New holders were designed and fabricated on
  the 3d printer. Three designs were created and printed:
    - For Rev A: a single D cell holder for a battery with welded tabs. Hot glue holds the
      battery in at the open end.
    - For Rev B: a holder for 4 AA cells, with open ends for batteries with soldered/welded
      connections. Our batteries would not take solder, so these wer not used.
    - For Rev B: a holder for 4 AA cells, with no openings. Salvaged steel strips were used
      to make contacts. This design was flown.
