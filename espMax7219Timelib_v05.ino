/*
  Buana Karya Instrument Presents
  NTP CLOCK WIFI ESP8266
  Programmed by: Aviezab, DeGray
  under Library WIFI MANAGER, ESP8266

  changelog
  v0.5 16 December 2016 13:58 Zeta DeGray
  -remove state machine since with timelib automatically count second
  -removing function for NTP queueing (not needed in TImelib)
  -add config callback function
  -move back blink function inside loop (it doesnt work outside loop)
  x-add led status 
  -repair RTC function
  -add config timeout and break after config
  -separate wifi autoconnect to a function
  -add WiFi.status for filtering connected/not since it always break after config
  -swap order of RTC and Wifi in setup (now its RTC 1st)
  -add reverse bool value function
  -move function to turn on led to nyalakan function (more stable)


  v0.4 08 November 2016 18:29 Zeta DeGray
  -menambah fungsi tick detik dengan library Time
  -update NTP via wifi digabung lewat library Time
  -memindahkan fungsi2 ke bawah untuk memudahkan baca
  -memisahkan fungsi parsing epoch ke jam menit detik

  v0.3 31 October 2016 14:27 Zeta DeGray
  -separate parsing NTP from main loop
  -merging code to drive 7Seg and RTC
  -change program flow to state diagram
  -put reset button code inside loop (for easier reset)


  v0.2 27 June 2016 08:27 Zeta DeGray
  -update WiFiManager library to version 0.12
  -change the way saving variable from wifi manager to EEPROM
  -add password to AP mode for security "buanakarya"


  v0.1 23 June 2016 08:37 Zeta DeGray
  -changing variable name korewak to EEPROM_BUFF
  -changing variable name korewa to NTP_ADDR
  -changing delay for waiting packet reply to 6s
  -changing delay for loop to 4s

*/
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

//Library for EEPROM
#include <EEPROM.h>

//Library for RTC
#include <Wire.h>
#include "RTClib.h"
RTC_DS1307 rtc;
uint16_t years;
uint8_t months;
uint8_t days;
uint8_t hours;
uint8_t minutes;
uint8_t seconds;
uint8_t dayofTheWeek;

//Library for Max7219 7seg driver (modified for ESP8266)
// pin GPIO 13 is connected to the DataIn
// pin GPIO 14 is connected to the CLK
// pin GPIO 12 is connected to LOAD
#include "LedControl.h"
LedControl lc = LedControl(13, 14, 12, 1);

/*-------- Library Jam ----------*/
#include <TimeLib.h>


//initialize Wifi Manager
WiFiManager wifim;


unsigned int localPort = 2390;      // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
//char* ntpServerName = ""; //"0.id.pool.ntp.org"; //"ns1.itb.ac.id";  //"ntp.itb.ac.id"; //"time.nist.gov";
char ntpServerName[20] = "id.pool.ntp.org";
const int timeZone = 7;     // Central European Time

char EEPROM_BUFF[20] = "";
int alamat = 0;
int i = 0;

String NTP_ADDR;
char ntp_server[40];

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;


//Variable for 7 Segment
int jam1;
int jam2;
int menit1;
int menit2;
int detik1;
int detik2;

int resetCount = 0;

// send an NTP request to the time server at the given address


time_t prevDisplay = 0;
unsigned long detikMillis = 0; //millis utk blink detik
const long interval = 30000;
bool ledDetik = LOW;
bool ledPesan = HIGH;
int ledCount = 0;
int no_NTP = 0;
int syncCount =0;
const int SYNC_TIME = 1800; // 1800s = 30minutes
//int resetCount =0;


time_t getNtpTime();
unsigned long sendNTPpacket(IPAddress& address);
void configModeCallback (WiFiManager *myWiFiManager);
void connectWifi();
void requestNTP();
void parseTime();
void nyalakan();
void setRTC();
void readRTC();
void resetWifi();
void digitalClockDisplay();
bool switchBool(bool val);

WiFiManagerParameter custom_ntp_server("ntpserver", "NTP server address", ntp_server, 40);


void setup()
{
  EEPROM.begin(512);
  Serial.begin(9600);
  Serial.println();

  /*
    The MAX72XX is in power-saving mode on startup,
    we have to do a wakeup call
  */
  lc.shutdown(0, false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0, 8);
  /* and clear the display */
  lc.clearDisplay(0);

  // if pin 2 is LOW, clear eeprom (reset all to default)
  pinMode(2, INPUT);

  //set config mode timeout for 5 minute
  wifim.setConfigPortalTimeout(300);

  //set break after config so it run on RTC 
  wifim.setBreakAfterConfig (true);

  //set callback when hit config mode (place "init text" there)
  wifim.setAPCallback(configModeCallback);

    //add parameter
  wifim.addParameter(&custom_ntp_server);
  
  //call function to connect wifi
  connectWifi();


  //Initialize RTC
  rtc.begin();
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    readRTC();
  }else{
    Serial.println("RTC is Running!");
    readRTC();
  }

  //  try to get NTP  
  setSyncProvider(getNtpTime);
  setSyncInterval(SYNC_TIME); //sync every 30 minute
}


void loop()
{

  unsigned long nowMillis = millis();

  //subroutine that run once a second without interrupt
  if (nowMillis - detikMillis >= 1000)
  {
    detikMillis = nowMillis;
    // led blink only works inside loop
    //      ledBlink();
    ledDetik = switchBool(ledDetik);
    
    if (no_NTP == 1) { //if No NTP then run in RtC Mode
      ledCount++;
      if (ledCount >= 15) { //every 15s switch indictor lamp
        ledPesan = switchBool(ledPesan);
        ledCount = 0;
      }
    } else {
      ledPesan = HIGH;
    }
    
    if (digitalRead(2) == LOW) {
      resetCount++;
      if (resetCount >= 3) {
        resetWifi();
        delay(1000);
        Serial.println("Reset!!!");
        ESP.reset();
      }
    } else {
      resetCount = 0;
    }
  }

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }

  if (syncCount >=12 && no_NTP == 1){
    //if sync twice and no NTP
    //reset from the start
    resetWifi();
    delay(1000);
    Serial.println("Reset because no NTP!!!");
    ESP.reset();
    
  }
}


void connectWifi(){
  
  // make an AP with password
  wifim.autoConnect("Buana Karya NTPC", "buanakarya");

  NTP_ADDR = custom_ntp_server.getValue();

  for (i = 0; i < 19; i++)
  {
    EEPROM_BUFF[i] = EEPROM.read(i);
  }
  Serial.print("Read: ");
  Serial.print(EEPROM_BUFF);


  if (EEPROM_BUFF[0] != 0)
  {
    for (i = 0; i < 19; i++)
    {
      ntpServerName[i] = EEPROM_BUFF[i];
    }
  }
  else //write ntp servername to eeprom
  {
    NTP_ADDR.toCharArray(EEPROM_BUFF, 20);
    for (i = 0; i < 19; ++i)
    {
      EEPROM.write(i, EEPROM_BUFF[i]);
      Serial.print(" Wrote: ");
      Serial.print(EEPROM_BUFF[i]);
      Serial.print(" ");
    }
    EEPROM.commit();
    for (i = 0; i < 19; i++)
    {
      ntpServerName[i] = EEPROM_BUFF[i];
    }
  }

// If connected, print connected and start UDP server
  if (WiFi.status() == WL_CONNECTED){
    Serial.print("WiFi connected\n");
    Serial.print("IP address: ");
    Serial.print(WiFi.localIP());
  
    Serial.print(" Starting UDP");
    udp.begin(localPort);
    Serial.print(" Local port: ");
    Serial.println(udp.localPort());
  }else{
    //else run from RTC
    no_NTP = 1;
  }


}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");

  //show init text
  lc.setRow(0, 0, B00000100); //i
  lc.setRow(0, 1, 0x15); //n
  lc.setRow(0, 2, B00000100); //i
  lc.setRow(0, 3, B00001111); //t

}



// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}



void setRTC() {
  rtc.adjust(DateTime(years, months, days, hours, minutes, seconds));
}

void readRTC() {
  DateTime rtcnow = rtc.now();

  int hours = rtcnow.hour();
  int minutes = rtcnow.minute();
  int seconds = rtcnow.second();

  //sync RTC value to TimeLib
  setTime(hours, minutes, seconds, days, months, years);

  parseTime();
}

time_t getNtpTime()
{

  IPAddress ntpServerIP; // NTP server's ip address

  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      no_NTP = 0;
      syncCount = 0;
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }

  //no NTP response change to RTC
  Serial.println("No NTP Response :-( set from RTC");
  readRTC();
  no_NTP = 1;
  syncCount++; 
  return 0; // return 0 if unable to get the time
}

void digitalClockDisplay()
{
  hours = hour();
  minutes = minute();
  seconds = second();
  parseTime();

  nyalakan();
}


void parseTime() {
  if (hours >= 24) {
    hours = 0 ;
  }
  if (hours < 10)
  {
    Serial.print('0');
    jam1 = 0;
  } else {
    jam1 = (hours / 10);
  }
  Serial.print(hours); // print the hour (86400 equals secs per day)
  jam2 = (hours % 10);

  Serial.print(':');

  if ( minutes < 10 ) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
    menit1 = 0;
  } else {
    menit1 = (minutes / 10);
  }
  Serial.print(minutes); // print the minute (3600 equals secs per minute)
  menit2 = (minutes % 10);

  Serial.print(':');
  if ( (seconds) < 10 ) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.println(seconds); // print the second

}

void nyalakan() {
  lc.setDigit(0, 3, menit2, false);
  lc.setDigit(0, 2, menit1, false);
  lc.setDigit(0, 1, jam2, false);
  lc.setDigit(0, 0, jam1, false);

  lc.setLed(0, 0, 0, ledPesan); //turn on Dig1's DP for ON effect
  lc.setLed(0, 1, 0, ledDetik); //turn on Dig2's DP for second effect

}


void resetWifi()
{
  for (i = 0; i < 19; ++i)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM Cleared");
  delay(1000);
  wifim.resetSettings();

  //tampilkan rest di layar
  lc.setRow(0, 0, B01000110); //r
  lc.setRow(0, 1, B01011011); //S
  lc.setRow(0, 2, B00001111); //t
  lc.setRow(0, 3, B00000001); //-
  delay(3000);
}

bool switchBool(bool val){
  bool switched = val;
  if (switched == LOW) {
      switched = HIGH;
    }
    else {
      switched = LOW;
    }
  return switched;
}

