// Automatic Packet Rreporting System (APRS) 
// Compile at 24Mhz to minimize power consumption
#include <Adafruit_MPL3115A2.h>   //for the pressurse sensor
#include <Wire.h> 
#include <Snooze.h>               //For low power hibernate
#include <TinyGPS++.h>            //GPS parsing library
#include <aprs.h>                 //APRS encoding and transmission library
#include <WProgram.h>

/* These are the variables you may want to change before a flight */
#define FLIGHT_NAME "MCM1028"      //Flight name in APRS message

// Mode 0 (Flight) parameters
#define FLIGHT_PERIOD 15              // How many minutes to be in active flight mode before sleeping
#define FLIGHT_APRS_TX_PERIOD 3       // How often to send an APRS position in minutes in flight mode

// Mode 1 (Hibernate) parameter
#define HIBERNATE_PERIOD 3       // How many days to sleep before beginning transmission

// Mode 2 (Track) parameters
#define TRACK_APRS_TX_PERIOD 3   // How often to send APRS position in minutes in track mode
#define TRACK_GPS_PERIOD 1        // How often to look for a GPS position in hours in track mode
#define TRACK_PING_TX_PERIOD 10    // how often to send a 'beep' in seconds
#define PING_DURATION 1000          // How long the ping transmission is in ms

#define debug //uncomment this for console debug

// Load drivers for the snooze code and configure snooze block.  We only us the RTC alamr so that is all we need to load. 
SnoozeAlarm  alarm;
SnoozeBlock config_teensy36(alarm); 

/* Pin Configuration */
#define V_TX_SHUTDOWN 23        // Enable pin for 3.6 to 5V boost converter
#define V_GPS_SHUTDOWN 30       // Power switch on 3.3v line to GPS and Pressure Sensor
#define TX_ENABLE 2             // Pull this high to turn on the transmitterpush (Push To Talk function)

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

struct PathAddress addresses[] = {
  {(char *)D_CALLSIGN, D_CALLSIGN_ID},    // Destination callsign
  {(char *)S_CALLSIGN, S_CALLSIGN_ID},    // Source callsign
  {(char *)NULL, 0},                      // Digi1 (first digi in the chain)
  {(char *)NULL, 0}                       // Digi2 (second digi in the chain)
};

#define GPSSERIAL Serial1 //UBlox GPS is on Serial1
Adafruit_MPL3115A2 baro = Adafruit_MPL3115A2();
TinyGPSPlus gps;

//Table for DAC generated sine wave for transmittin 'pings'
int sin_Wave[] = {2045,2173,2301,2428,2553,2676,2797,2915,3030,3140,3247,3348,
              3444,3535,3620,3699,3771,3837,3895,3946,3989,4025,4053,4073,
              4085,4090,4085,4073,4053,4025,3989,3946,3895,3837,3771,3699,
              3620,3535,3444,3348,3247,3140,3030,2915,2797,2676,2553,2428,
              2301,2173,2044,1916,1788,1661,1536,1413,1292,1174,1059,949,842,
              741,645,554,469,390,318,252,194,143,100,64,36,16,4,0,4,16,36,
              64,100,143,194,252,318,390,469,554,645,741,842,949,1059,1174,
              1292,1413,1536,1661,1788,1916}; 

int flight_APRS_packets = 0;      // initialize Mode 0 (Flight) loop counter
int days_in_hibernate = 0;        // initialize Mode 1 (Hibernate) days counter

long pings_sent = 5;
int track_APRS_packets = 0;

int mode = 0;   // Mode variable
                // Mode 0 = Flight mode, send APRS often
                // Mode 1 = Sleep Mode, hibernate for X days
                // Mode 2 = Track Mode, pings with occasional APRS


void setup()
{
  Serial.begin(9600); // For debugging output over the USB port
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

  /* Blink 5 times on power up so we know it is working */
  for (int i = 0; i < 5 ; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }

  Serial.println("Begin");
  Serial.println(FLIGHT_NAME);
  delay(500);  

  
  /* Set up the APRS code */
  aprs_setup(50,        // number of preamble flags to send
       TX_ENABLE,       // Use PTT pin
       10,              // ms to wait after PTT to transmit
       0, 0             // No VOX ton
       );
  
  Serial.println("Entering_Flight_Mode");
  delay(500);  
  broadcastLocation( "TRACKER_ON_3" ); // XMT Tracker ON message to the radio
  delay(500);
}

/* Main loop has 3 blocks of code that are exectued dependign on the 
 *  mode that the tracker is in.  
 */

void loop()
{
 /* Flight Mode
  * Only sends APRS positions, no pings. Updates GPS position for every APRS packet
  * Average Current consumption is 25mA
  */
 if (mode == 0) 
 {
    getGPS(120);                      // try for 2 minutes to get a GPS Position
    broadcastLocation( FLIGHT_NAME ); //Send the location by APRS

    if (flight_APRS_packets > int(float(FLIGHT_PERIOD)/float(FLIGHT_APRS_TX_PERIOD)))
    {
      mode = 1;
      broadcastLocation( "MODE=1=SLEEP" ); // XMT mode changing to MODE=1 (Hebernate)
    }
    flight_APRS_packets++;
      
    alarm.setRtcTimer(0, FLIGHT_APRS_TX_PERIOD,0);    // hour, min, sec
    Snooze.hibernate( config_teensy36 );
 }   
 
 /* Hibernate Mode 
  * Wakes up once per day to update the counter of how many days have elapsed
  * Otherwise doesn't do anything. 
  * Avergae Current 0.5 mA
  */
 if (mode == 1) 
 {
    days_in_hibernate++;
    if (days_in_hibernate > HIBERNATE_PERIOD)
    {
      mode = 2;                               // Go to Track Mode when hibernated period completed
      getGPS(300);                            //try for 5 minutes to get a GPS Position  
      broadcastLocation( "MODE=2=TRACK" );    // Send MODE=2=Track to the radio  
    }
//    alarm.setRtcTimer(24, 0, 0);            // Set Timer to 24 hours, 0 minutes, 0 seconds to go to sleep for a day
    alarm.setRtcTimer(0, 3, 0);               // make the day only one hour long for this test
    Snooze.hibernate( config_teensy36 );    
 }

 /* Track Mode
  * Sends APRS positions every TRACK_APRS_TX_PERIOD minutes
  * Sends pings every TRACK_PING_TX_PERIOD seconds, each ping is PING_DURATION ms long
  * Updates the GPS position every TRACK_GPS_PERIOD hours
  */
 if (mode == 2) 
 {
   ping(PING_DURATION);                                                     //send a ping

   if(pings_sent * TRACK_PING_TX_PERIOD > TRACK_APRS_TX_PERIOD * 60)        // if it is time to send an APRS, send it using the existing GPS position
   {
      broadcastLocation( FLIGHT_NAME );                                     //Send the location by APRS
      track_APRS_packets ++;
      pings_sent = 0;                                                        //reset ping counter
   }

  if(track_APRS_packets * TRACK_APRS_TX_PERIOD > TRACK_GPS_PERIOD * 60)     //if it is time to update GPS, update the position
  {
     getGPS(180);  //try for 3 minutes to get a GPS Position
  }

   alarm.setRtcTimer(0, 0, TRACK_PING_TX_PERIOD);                           // hour, min, sec go back to sleep until the next ping
   Snooze.hibernate( config_teensy36 ); 
 }
 delay(500);
}

// Function to broadcast your location
/* This function sends an APRS position report, including the 'comment' string. 
 * First it configure the DAC to 12 but mode, 1.2V reference.
 * Second, it switches on the required hardware (radio module) and waits for the 
 * radio DC-DC converter to stabilize.   It the keys up the radio (PTT), waits for that to 
 * stabilize and sends the AFSK encoded position using the digitcal to analog
 * converter to generate to tones. 
 * Finaly it disables the DAC to save power and switches off the boost converter for the radio
 */
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

#ifdef debug
  // For debugging print out the path
  Serial.print("APRS(");
  Serial.print(nAddresses);
  Serial.print("): ");
  for (int i=0; i < nAddresses; i++) 
    {
      Serial.print(addresses[i].callsign);
      Serial.print('-');
      Serial.print(addresses[i].ssid);
      if (i < nAddresses-1) Serial.print(',');
    }
  Serial.print(' ');
  Serial.print(SYMBOL_TABLE);
  Serial.print(SYMBOL_CHAR);
  Serial.println();
#endif  

  // Send the packet
  aprs_send(addresses, nAddresses
      ,gps.date.day(), gps.time.hour(), gps.time.minute()
      ,gps.location.lat(), gps.location.lng() // degrees
      ,gps.altitude.meters() // meters
      ,gps.course.deg()
      ,gps.speed.mph()
      ,SYMBOL_TABLE
      ,SYMBOL_CHAR
      ,comment);
      
  DAC0_C0 = ~DAC_C0_DACEN;            // DAC_C0_LPEN - disable DAC to save power
  digitalWrite(V_TX_SHUTDOWN, LOW);   // Turn off 5V DC-DC converter for Radiometrix 
}

void ping(int period)
{
  /* This function sends a 'ping' for a period of 'period' ms. 
   *  It first configures the DAC for 12 bit operation and 1.2V reference
   *  It then powers on the boost converter for the radio and waits a few ms
   *  for it to stabilize
   *  It then keys the radio, waits a ms for it to stabilize, then sends a tone
   *  of duration 'period'.
   *  Finally it switches everything back off again.
   */

   
  /* Enable the DAC */
  analogWriteResolution(12);
  analogReference(INTERNAL);
  
#ifdef DEBUG
  digitalWrite(LED_BUILTIN, HIGH); //turn on the LED
#endif
  
  digitalWrite(V_TX_SHUTDOWN, HIGH); //Turn on 5V DC-DC converter for Radiometrix
  delay(3); //wait to stabilize
  digitalWrite(TX_ENABLE, HIGH); // key the radio up
  delay(1);

  toneBurst(1000,period);  //Send the ping
 
  DAC0_C0 = ~DAC_C0_DACEN; //DAC_C0_LPEN - disable DAC to save power
  digitalWrite(TX_ENABLE, LOW); //unkey the radio
  digitalWrite(V_TX_SHUTDOWN, LOW); //turn off the DC-DC boost
  digitalWrite(LED_BUILTIN, LOW);
  pings_sent++;
}

void toneBurst(int frequency, float period)
{
  /* This function generates a sine wave tone burst with a user defined 
   *  frequency and period and outputs it on DAC0. 
   *  The sine wave is stored in a 100 element look up table. 
   */

  float points_per_wave = 100;
  int delay_per_sample = (int) (1.0 / ((float)frequency * points_per_wave) * 1e6); //delay between updates in uS
  int samples_per_period =  int((period/10000.0)*frequency*points_per_wave); //how many samples needed to reach the period
  pinMode(A21,OUTPUT); // Make sure the DAC is set as output
  
  for (int i = 0; i< samples_per_period; i++) //generates a 20 point 1kHz Sine wave
      {
              analogWrite(A21, sin_Wave[i%100]);
              delayMicroseconds(delay_per_sample-3);
      }
  analogWrite(A21, 0);
}

bool getGPS(long timeout)
{
    /* This function turns on the GPS and looks for a position
     *  solution either until it has a solution, or it reaches
     *  timeout seconds.  If a position is found it updates the global
     *  GPS position structure and returns true.  If no position was found 
     *  it returns false and leaves the GPS position structure unchanged
     */
    
    long startTime = millis(); 
    digitalWrite(V_GPS_SHUTDOWN, HIGH); // turn on GPS Power
    GPSSERIAL.begin(9600); //GPS receiver serial setup
    delay(500);
    
    while((gps.location.age() > 2000) && ((millis() - startTime) < timeout*1000l))  //wait for a GPS position that is less than 2s old or timeout
    {
      if (GPSSERIAL.available()) {
         gps.encode(GPSSERIAL.read()); //this sends incoming NEMA data to a parser, need to do this regularly
       }
     }
#ifdef debug  
    Serial.printf("Solution Age: %f, Location: %f, %f altitude %f\n\r",
      gps.location.age(), gps.location.lat(), gps.location.lng(), gps.altitude.meters());
#endif      

    GPSSERIAL.end();
    digitalWrite(V_GPS_SHUTDOWN, LOW); // turn off GPS Power

    if (gps.location.age() < 3000)
      return true;
    else
      return false; 
}



