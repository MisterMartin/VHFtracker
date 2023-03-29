// Automatic Packet Rreporting System (APRS) 
// *** Compile at 24Mhz to minimize power consumption

//*************************************************************************
//             Configuration
// Tracker model. One or the other must be defined.
#define TRACKER_REVA
//#define TRACKER_REVB

#define FLIGHT_NUM "1039"             // MUST be 4 numeric characters, .e.g. "1030"
#define PING_ID    39                 // Ping ID (0-255) sent for ping ID byte (should be last two digits of the flight number)

// PREFLIGHT mode parameters
#define PREFLIGHT_APRS_TX_PERIOD 10   // (Default 10) How often to send an APRS position (minutes)
#define MAX_PREFLIGHT_PACKETS    12   // (Default 12) PREFLIGHT mode runs for 
                                      // (PREFLIGHT_APRS_TX_PERIOD)*(MAX_PREFLIGHT_PACKETS) (minutes)
                                      // (PREFLIGHT_APRS_TX_PERIOD=10)*(MAX_PREFLIGHT_PACKETS=12)= 10*12 = 120 minutes Preflight mode time
// FLIGHT mode parameters
#define FLIGHT_TIME 120               // (Default 120) How many minutes to remain in Flight Mode

// HIBERNATE parameters --- Launch Date: 02 April 2023 (JD 92), Wake Up Date: 01 Nov 2023(JD 305) -----------
#define HIBERNATE_PERIOD        213   // Will Hibernate for 320-88 = 213 days

// TRACK mode parameters
#define TRACK_GPS_PERIOD         24   // (Default 24) How often to look for a GPS position (hours)
#define TRACK_APRS_TX_PERIOD     10   // (Default 10) How often to send APRS position message (minutes)
#define TRACK_PING_TX_PERIOD     15   // (Default 15) How often to send a Ping (seconds)
//*************************************************************************

// Note: for post processing reasons, make all APRS ASCII text message fields a fixed 8 characters long

#include <afsk.h>
#include <ax25.h>
#include <aprs.h>                 // APRS encoding and transmission library
#include <Wire.h> 
#include <Snooze.h>               // For low power hibernate
#include <TinyGPS++.h>            // GPS parsing library
#include <WProgram.h>

void ax25_send_flag();            // Forward definition, because it is not defined in ax25.h

// Load drivers for the snooze code and configure snooze block.  We only us the RTC alamr so that is all we need to load. 
SnoozeAlarm  alarm;
SnoozeBlock config_teensy36(alarm); 

TinyGPSPlus gps;

// Message headers for the APRS messages.
#define HEL_MSG    FLIGHT_NUM"_HEL"   // Power on APRS message says hello
#define PRE_MSG    FLIGHT_NUM"_PRE"   // Flight name in APRS preflight mode message
#define FLT_MSG    FLIGHT_NUM"_FLT"   // Flight name in APRS flight mode message
#define HIB_MSG    FLIGHT_NUM"_HIB"   // Flight name in APRS hibernate mode message
#define TRK_MSG    FLIGHT_NUM"_TRK"   // Flight name in APRS track mode message (sent every TRACK_APRS_TX_PERIOD)
#define TRK_GPS    FLIGHT_NUM"_GPS"   // Flight name in APRS track mode GPS message (sent every TRACK_GPS_PERIOD)

#define GPS_24bit_SF 93200     // approx (2^24)/180, used to scale decimal degrees to 24 bit integer

#define GPS_PWR_ON  1     // used in getGPS(long timeout, bool Power) function, leave GPS PWR on
#define GPS_PWR_OFF 0     // used in getGPS(long timeout, bool Power) function, Turn GPS PWR off when done

// mode definitions
#define PREFLIGHT 0       // Prefight mode, Accquire GPS location, send APRS often, no pings
#define FLIGHT    1       // Fight mode, Accquire GPS location, no APRS, no pings
#define HIBERNATE 2       // Hibernate mode, do nothing until wake up day
#define TRACK     3       // Track mode, Accquire GPS location, send APRS, send GPS and tone pings


// Pin usage revision dependencies
#ifdef TRACKER_REVA
#define GPSSERIAL Serial1       // UBlox GPS is on Serial1
/* Pin Configuration */
#define V_TX_SHUTDOWN 23        // Enable pin for 3.6 to 5V boost converter
#define V_GPS_SHUTDOWN 30       // Power switch on 3.3v line to GPS and Pressure Sensor
#define TX_ENABLE 2             // Pull this high to turn on the transmitter (Push To Talk function)
#endif

#ifdef TRACKER_REVB
#define GPSSERIAL Serial5       // UBlox GPS is on Serial1
/* Pin Configuration */
#define V_TX_SHUTDOWN 15        // Enable pin for 3.6 to 5V boost converter
#define V_GPS_SHUTDOWN 36       // Power switch on 3.3v line to GPS and Pressure Sensor
#define TX_ENABLE 16             // Pull this high to turn on the transmitter (Push To Talk function)
#endif

/* APRS Configuration */
// Set your callsign and SSID here. Common values for the SSID are
#define S_CALLSIGN      "KE0PRP"
#define S_CALLSIGN_ID   11          // 11 is usually for balloons
                                    // Destination callsign: APRS (with SSID=0) is usually okay.
#define D_CALLSIGN      "APRS"
#define D_CALLSIGN_ID   0
                                    // Symbol Table: '/' is primary table '\' is secondary table
#define SYMBOL_TABLE '/' 
                                    // Primary Table Symbols: /O=balloon, /-=House, /v=Blue Van, />=Red Car
#define SYMBOL_CHAR 'O'

struct PathAddress addresses[] = 
  {
    {(char *)D_CALLSIGN, D_CALLSIGN_ID},    // Destination callsign
    {(char *)S_CALLSIGN, S_CALLSIGN_ID},    // Source callsign
    {(char *)NULL, 0},                      // Digi1 (first digi in the chain)
    {(char *)NULL, 0}                       // Digi2 (second digi in the chain)
  };

// quarter wave sine table for tone generation
unsigned int Sine[26] =         // 26 samples for 1/4 sine wave
{2047,2177,2306,2434,2561,2686,2808,2927,3042,3154,3261,3363,3460,
 3551,3636,3715,3787,3852,3909,3960,4002,4037,4063,4082,4092,4094};

int Preflight_APRS_packets = 0;    // Preflight mode loop counter
int Hibernate_day = 0;             // number of days in hibernation
int track_APRS_packets = 1;        // used in Track Mode, counts the number of APRS packets sent in track mode
long pings_sent = 1;               // used in Track Mode, counts the number of pings sent in track mode
byte ping_state = 0;               // used in Track Mode, controlls the type of ping (tone or encoded position data) sent in track mode

long StartTime = 0;                   // Used in Flight Mode
long MaxTime = FLIGHT_TIME*1000*60;   // Maximum time to stay in Flight Mode, MaxTime(msec), FLIGHT_TIME(minutes)  
   
// 8 byte array (ID, FIX, LAT2, LAT1, LAT0, LON2, LON1, LON0) for sending binary GPS ping
byte Ping_Array[8] = {PING_ID, 0, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA};       // dummy values for testing (LAT=60.004, LON=120.008

unsigned long int Fix_Time =0;     // value uesd to send gps.location.age() to the radio using the ALT value displayed on the TH-D74 radio
byte APRS_XMIT_count = 0;          // increments when ever a APRS message is sent

int mode = PREFLIGHT;              // mode must be initialized to PREFLIGHT for all balloon flights

#ifdef TRACKER_REVA
char* board_revision = "Configured for board REV A";
#endif
#ifdef TRACKER_REVB
char* board_revision = "Configured for board REV B";
#endif
#if (!defined(TRACKER_REVA) && !defined(TRACKER_REVB)) || (defined(TRACKER_REVA) && (defined(TRACKER_REVB)))
#error "One of TRACKER_REVA or TRACKER_REVB must be defined."
#endif

//************************************************************************************************
//************************************************************************************************
void setup()
{
  Serial.begin(9600); // For debugging output over the USB port
  Serial.println(__FILE__);
  Serial.print("Build time: ");
  Serial.print(__TIMESTAMP__);
  Serial.print(" ");
  Serial.println(board_revision);

  /* GPS Setup */
   GPSSERIAL.begin(9600); //GPS receiver setup

   pinMode(LED_BUILTIN, OUTPUT);
   pinMode(V_TX_SHUTDOWN, OUTPUT);
   pinMode(V_GPS_SHUTDOWN, OUTPUT);
   pinMode(TX_ENABLE,OUTPUT);

   /* Start with everything off */
   digitalWrite(V_TX_SHUTDOWN, LOW);
   digitalWrite(V_GPS_SHUTDOWN, LOW);
   digitalWrite(TX_ENABLE, LOW);
   analogWriteResolution(12);
   analogReference(INTERNAL);

// Blink 5 times on power up so we know it is working */
  for (int i = 0; i < 5 ; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
  
// Set up the APRS code
  aprs_setup(
       10,              // Send 10 APRS preamble flags (this is a WAG, but if needed can be longer to help TH-D74 sync), must be at least 2
       TX_ENABLE,       // Use PTT pin
       5,               // Wait 5 mesc after TX_ENABLE goes high to start sending data transmission
       0, 0             // No VOX ton
       );
  
  broadcastLocation(HEL_MSG);   // Send hello APRS message to the TH-D74 Tracker radio
}

//************************************************************************************************
//************************************************************************************************
void loop()
{
   //---------------------------------------------------------------------------------------------  
   // PREFLIGHT Mode: Only sends APRS positions, no pings. Attempts to update GPS position for every APRS packet sent
   //
   if (mode == PREFLIGHT) 
   {
      if (Preflight_APRS_packets < MAX_PREFLIGHT_PACKETS)     // Preflight mode will run for (MAX_PREFLIGHT_PACKETS)*(PREFLIGHT_APRS_TX_PERIOD) (in seconds)
        {        
         alarm.setRtcTimer(0, PREFLIGHT_APRS_TX_PERIOD,0);    // hour, min, sec
         Snooze.hibernate( config_teensy36 );
         getGPS(120,GPS_PWR_OFF);            // try for 2 minutes to get a GPS Position, turn GPS PWR off when done
         broadcastLocation(PRE_MSG);         // Send the location by APRS
         Preflight_APRS_packets++;        
        }
      else
        {
         broadcastLocation(FLT_MSG);      // Send "FLT_MODE" APRS message to the TH-D74 Tracker radio
         Ping_Array[1] = 0;               // Reset the counter that tracks the number of invalid GPS fixes
         APRS_XMIT_count = 0;             // Reset APRS message sent counter when changing modes
         mode = FLIGHT;                   // Go to FLIGHT mode when Preflight mode is completed
        }   
   }   

   //---------------------------------------------------------------------------------------------
   // FLIGHT MODE: Acquires and stores GPS postions only, No APRS messages or pings sent
   //              Stays in flight mode for FLIGHT_TIME (units: minutes)
   //
     if (mode == FLIGHT)
      {
        StartTime = millis();
            
        while (millis()-StartTime < MaxTime)    // continously update the GPS position while in FLIGHT mode
         {
          getGPS(120,GPS_PWR_ON);               // try for 2 minutes to get a GPS Position, Leave GPS PWR ON
         }                                                   
      
        digitalWrite(V_GPS_SHUTDOWN, LOW);      // Always turn off GPS Power when leaving FLIGHT mode
        broadcastLocation(HIB_MSG);             // Send "HIB_MODE" APRS message to the TH-D74 Tracker radio
        Ping_Array[1] = 0;                      // Reset the counter that tracks the number of invalid GPS fixes
        APRS_XMIT_count = 0;                    // Reset APRS message sent counter when changing modes
        mode = HIBERNATE;                       // Go to HIBERNATE mode when flight mode is completed
      } 

  //---------------------------------------------------------------------------------------------
  // HIBERNATE MODE: Wakes up once per day to increment the days in hibernate counter
  //                 Gets a GPS postion when the hibernate period is complete
  //
  //    Hibernation function: alarm.setRtcTimer(Hours, minutes, seconds)
  //
  if (mode == HIBERNATE) 
   {
      if (Hibernate_day < HIBERNATE_PERIOD)      // stay in Hibernate until hibernate period is complete
        {
         alarm.setRtcTimer(24, 0, 0);            // Set Timer to 24 hours, 0 minutes, 0 seconds to go to sleep for a day
         Snooze.hibernate( config_teensy36 );
         Hibernate_day ++;                       // number of days in hibernate   
        }
       else
        {
         getGPS(300,GPS_PWR_OFF);         // try for 5 minutes to get a GPS Position                                                             
         broadcastLocation(TRK_MSG);      // Send "TRK_MODE" APRS message to the TH-D74 Tracker radio
         APRS_XMIT_count = 0;             // Reset APRS message sent counter when changing modes
         mode = TRACK;                    // Go to TRACK mode when hibernate period completed    
        }  
   }

   //---------------------------------------------------------------------------------------------
   // TRACK MODE: Updates GPS position and sends APRS message every TRACK_GPS_PERIOD (hours)
   //             Sends APRS message every TRACK_APRS_TX_PERIOD (minutes)
   //             Sends pings every TRACK_PING_TX_PERIOD (seconds)
   //
   if (mode == TRACK) 
   {
     if((pings_sent*TRACK_PING_TX_PERIOD) >= TRACK_APRS_TX_PERIOD * 60)      // if it is time, send an APRS message   
       {
          if(track_APRS_packets * TRACK_APRS_TX_PERIOD >= TRACK_GPS_PERIOD * 60)     // If it is time, update the GPS position 
            {
              getGPS(180,GPS_PWR_OFF);      // try for 3 minutes to get a GPS Position
              broadcastLocation(TRK_GPS);   // Send the new GPS location by APRS and indicate a GPS cycle
              track_APRS_packets = 1;       // Reset track_APRS_packets counter after the GPS position has been updated
            }
          else
            {
              broadcastLocation(TRK_MSG);  // Send the existing GPS location by APRS and indicate a TRK cycle
              track_APRS_packets++;        // increment the counter that keeps track of the number of APRS messages sent               
            }   
          pings_sent = 1;                  // Reset ping counter after every APRS meesage
          ping_state = 0;                  // first ping after any APRS message is a tone
       }
      else                          // Send a Ping if no APRS message was sent
       {
         pings_sent++;              // increment the counter for number of pings sent
         
         switch (ping_state)        // send three tone and one encoded GPS pings for every TRACK_PING_TX_PERIOD
          {                            
            case 0:
              Output_Ping_tone(11,53);  // 704 Hz tone for 75 msec
              ping_state++;             // goto next state
              break;
            case 1:
              Output_Ping_tone(11,53);  // 704 Hz tone for 75 msec
              ping_state++;             // goto next state
              break; 
            case 2:
              Output_Ping_tone(11,53);  // 704 Hz tone for 75 msec
              ping_state++;             // goto next state
              break;        
            case 3:
              EncodedPing();            // Send encoded GPS position track ping     
              ping_state = 0;           // goto state 0
              break;                               
            default: 
              ping_state = 0;       // goto state 0 (should never get here)
              break;
          }       
       }        
     alarm.setRtcTimer(0, 0, TRACK_PING_TX_PERIOD);       // hour, min, sec go back to sleep until the next ping
     Snooze.hibernate( config_teensy36 );
   }
}

//************************************************************************************************
//************************************************************************************************
// Function to broadcast your location
// This function sends an APRS position report, including the 'comment' string. 
// First it configure the DAC to 12 but mode, 1.2V reference.
// Second, it switches on the required hardware (radio module) and waits for the 
// radio DC-DC converter to stabilize.   It the keys up the radio (PTT), waits for that to 
// stabilize and sends the AFSK encoded position using the digitcal to analog
// converter to generate to tones. 
// Finaly it disables the DAC to save power and switches off the boost converter for the radio
//
void broadcastLocation(const char *comment)
{
  int nAddresses; 
  // Enable the DAC that is used for generating the tone for the APRS radio
  analogWriteResolution(12);          // set DAC resolution to 12 bits
  analogReference(INTERNAL);          // use 1.2V internal reference (0V <= DAC Vout < 1.2V)
  analogWrite(A21, 0);                // Initialize DAC output to 0V
  
  digitalWrite(V_TX_SHUTDOWN, HIGH);  // Turn on 5V DC-DC converter for Radiometrix
   
  if (gps.altitude.meters() > 1524)   // If above 5000 feet (1524 meters) switch to a single hop path - only relevant in CONUS
    {
      // APRS recomendations for > 5000 feet is:
      // Path: WIDE2-1 is acceptable, but no path is preferred.
      nAddresses = 3;
      addresses[2].callsign = "WIDE2";
      addresses[2].ssid = 1;
    } 
  else 
    {
      // Below 1500 meters use a much more generous path (assuming a mobile station)
      // Path is "WIDE1-1,WIDE2-2"
      nAddresses = 4;
      addresses[2].callsign = "WIDE1";
      addresses[2].ssid = 1;
      addresses[3].callsign = "WIDE2";
      addresses[3].ssid = 2;
    }

  Fix_Time = gps.location.age();           // time (LSB=msec) since last GPS position
  if (Fix_Time > 99999) Fix_Time = 99999;  // Limit to 99,999 because its the largest value the TH-D74 radio can display
  APRS_XMIT_count++;                       // increment APRS message counter every time a message is sent (0-255, rolls over)

  aprs_send(addresses, nAddresses
      ,gps.date.day()           // does not seem to work
      ,gps.time.hour()
      ,gps.time.minute()
      ,gps.location.lat()       // degrees
      ,gps.location.lng()       // degrees
//    ,gps.altitude.meters()    // meters      
      ,Fix_Time                 // (TH-D74 Radio: ALT),new hijacked param: age of the GPS fix (msec), integer
//    ,gps.course.deg()         // in degrees (0-359)
      ,Ping_Array[1]            // (TH-D74 Radio: CSE),new hijacked param: Counts number of failed GPS positions, 0<= Ping_Array[1] <= 255
//    ,gps.speed.mph()          // in mph
      ,APRS_XMIT_count          // (TH-D74 Radio: SPD),new hijacked param: APRS transmission counter (0-255, rolls over) 
      ,SYMBOL_TABLE
      ,SYMBOL_CHAR
      ,comment);
          
  DAC0_C0 = ~DAC_C0_DACEN;            // DAC_C0_LPEN - disable DAC to save power
  digitalWrite(V_TX_SHUTDOWN, LOW);   // Turn off 5V DC-DC converter for Radiometrix 
}

//************************************************************************************************
//************************************************************************************************
//  This function turns on the GPS and looks for a position solution either until it has a solution,
//  or it reaches timeout seconds.
//  
//  If a position is found, it updates the global GPS position structure and returns true
//  If a position was not found, GPS position structure unchanged and returns false
//
//  timeout in seconds
//  millis() : returns a unsigned 32 bit integer, LSB = 1 msec, 0xFFFFFFFF is about 50 days
//           : initialized to 0x00000000 at power up
//
bool getGPS(long timeout, bool Power)
{ 
    long startTime = millis(); 
    digitalWrite(V_GPS_SHUTDOWN, HIGH);     // turn on GPS Power
    GPSSERIAL.begin(9600);                  // GPS receiver serial setup
    delay(500);
    
    while((gps.location.age() > 2000) && ((millis() - startTime) < timeout*1000l))  // wait for a GPS position that is less than 2s old or timeout
      {
       if (GPSSERIAL.available()) gps.encode(GPSSERIAL.read());                     // this sends incoming NEMA data to a parser, need to do this regularly
      }

    GPSSERIAL.end();
    
    if (Power == GPS_PWR_OFF) digitalWrite(V_GPS_SHUTDOWN, LOW);    // turn off GPS Power

    if (gps.location.age() < 3000)          
      {
       Update_Ping_Array();                 // update Ping Array with new valid GPS data 
       if (mode == TRACK) Ping_Array[1]=0;  // when in TRACK mode only, reset the missed fix counter everytime a valid GPS fix is found
       return true;      
      }
    else
      {
       if (Ping_Array[1] != 255) Ping_Array[1]++;    // increment counter that keeps track of missed GPS fixes (0-255), hold at 255
       return false;
      }
}

//************************************************************************************************
//************************************************************************************************
// This functions calculates scaled Lat\Lon that is used for sending position pings:
//
void Update_Ping_Array(void)
{ 
 float Angle_f;
 unsigned long int Angle_scaled;
  
// 0 deg <= Lat < 90 deg, decimal degreees, 24 bit unsigned word
 Angle_f = gps.location.lat();
 Angle_scaled = (unsigned long int) abs(Angle_f*GPS_24bit_SF);
 Angle_scaled = Angle_scaled & 0x00FFFFFF;       // only a 24 bit word, ensure upper bits are all zeros
 Ping_Array[2] = (byte) (Angle_scaled >> 16);    // Latitude MS byte 2 
 Ping_Array[3] = (byte) (Angle_scaled >> 8);     // Latitude byte 1
 Ping_Array[4] = (byte)  Angle_scaled;           // Latitude LS byte 0
    
// 0 deg <= Lon < 180 deg, deciamal degrees, 24 bit unsigned word                   
 Angle_f = gps.location.lng();
 Angle_scaled = (unsigned long int) abs(Angle_f*GPS_24bit_SF);
 Angle_scaled = Angle_scaled & 0x00FFFFFF;       // only a 24 bit word, ensure upper bits are all zeros
 Ping_Array[5] = (byte) (Angle_scaled >> 16);    // Longitude MS byte 2 
 Ping_Array[6] = (byte) (Angle_scaled >> 8);     // Longitude byte 1
 Ping_Array[7] = (byte)  Angle_scaled;           // Longitude LS byte 0      
}

//************************************************************************************************
//************************************************************************************************
//  This function sends the values in Ping_Array[8] to the TH-D74 radio
//  Transmission Time = 103 msec
//
void EncodedPing(void)
{
 uint8_t buf[128];
   
  // Use the AX.25 library to build the string to send 
  ax25_initBuffer(buf, 128);    // initialize a buffer to hold the built up string

  // send 3 '0x7E' preamble flags (this is a WAG, but if needed can be longer to help the TH-D74 sync), must be at least 2 flags
  ax25_send_flag();             // add an '0x7E' preamble to the start of the buffer
  ax25_send_flag();             // add an '0x7E' preamble to the start of the buffer
  ax25_send_flag();             // add an '0x7E' preamble to the start of the buffer
  
  for (int i=0; i<8; i++) ax25_send_byte(Ping_Array[i]);    // XMIT Ping Array by sending one byte at a time
  
  ax25_send_footer();           // add the CRC to the end

  // Use the AFSK library to send the string
  afsk_set_buffer(buf,ax25_getPacketSize());  // send the AX.25 buffer to the AFSK que
  afsk_start();                               // send it
  while(afsk_busy());                         // wait for the send to complete
}

//************************************************************************************************
//************************************************************************************************
//    Generates sine wave for Ping Tone using 1/4 sine wave values
//      - Tone starts and ends with DAC voltage = 0V
//
//        Frequency Hz |   406    515     617     704     819     980
//        -------------|-----------------------------------------------
//        Tone_Period  |    21     16      13      11       9       7
//                     |
//   Tone_PW  (75 msec)|    30     39      46      53      61      74
//   Tone_PW (100 msec)|    41     52      62      70      82      98
//
//   Tone_PW = (int) [Length of Tone (sec)]*[Frequency (Hz)], rounded up
//
void Output_Ping_tone(int Tone_Period, byte Tone_PW)
{
  int i;
   
  pinMode(A21,OUTPUT);                  // Make sure the DAC is set as output
  analogWriteResolution(12);            // Set for 12 bit DAC (0 to 4095)
  analogReference(INTERNAL);            // internal 1.1V reference for ADC output
  analogWrite(A21, 0);                  // Initialize DAC output to 0.0V  
  digitalWrite(V_TX_SHUTDOWN, HIGH);    // Turn on 5V DC-DC converter for Radiometrix
  digitalWrite(TX_ENABLE, HIGH);        // Key the radio up
  delay(5);                             // Wait 5 msec for the HX1 transmitter to stabilize (see HX1 data sheet)

  for (int n=0; n<Tone_PW; n++)
   {      
//----------- Fourth Quadrant (270-360 degrees)
  for (i=25; i>-1; i--) 
      {
       analogWrite(A21, (Sine[25] - Sine[i]));
       delayMicroseconds(Tone_Period);
      }
            
//------- First Quadrant (0-90 degrees)
  for (i=1; i<26; i++) 
      {
       analogWrite(A21, Sine[i]);
       delayMicroseconds(Tone_Period);
      }

//----------- Second Quadrant (90-180 degrees)
  for (i=24; i>-1; i--) 
      { 
       analogWrite(A21, Sine[i]);
       delayMicroseconds(Tone_Period);
      }      
      
//----------- Third Quadrant (180-270 degrees)
  for (i=1; i<26; i++) 
      {
       analogWrite(A21, (Sine[25] - Sine[i]));
       delayMicroseconds(Tone_Period);
      }
  } 
           
  DAC0_C0 = ~DAC_C0_DACEN;              // DAC_C0_LPEN - disable DAC to save power
  digitalWrite(TX_ENABLE, LOW);         // unkey the radio
  digitalWrite(V_TX_SHUTDOWN, LOW);     // turn off the DC-DC boost
}
