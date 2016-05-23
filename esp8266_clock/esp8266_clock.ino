/*
   Time_NTP.pde
   Example showing time sync to NTP time source

   This sketch uses the ESP8266WiFi library
*/

#include <TimeLib.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Hash.h>
#include <LedControl.h>

IPAddress timeServerIP;
const char* ntpServerName = "us.pool.ntp.org";

//const int timeZone = 1;     // Central European Time
const int timeZone = -4;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

unsigned int localPort = 2390;  // local port to listen for UDP packets

const char* HOSTNAME = "ESP-CLOCK";
#define PASSWORD "OMFG@@"

WiFiUDP Udp;
WiFiClient espClient;
PubSubClient client(espClient);
MDNSResponder mdns;

#define NUM_DISPLAY 4

// data, clock, cs, numdevices
//LedControl lc = LedControl(D7,D5,D6,4);
LedControl lc = LedControl(D7, D5, D4, NUM_DISPLAY);

int prev_display[NUM_DISPLAY] = {-1, -1, -1,-1 };

const int num[10][8] = {
  {0x00, 0x78, 0xcc, 0xec, 0xfc, 0xdc, 0xcc, 0x78}, // zero
  {0x00, 0xfc, 0x30, 0x30, 0x30, 0x30, 0xf0, 0x30}, // one
  {0x00, 0xfc, 0xcc, 0x60, 0x38, 0x0c, 0xcc, 0x78}, // two
  {0x00, 0x78, 0xcc, 0x0c, 0x38, 0x0c, 0xcc, 0x78}, // three
  {0x00, 0x0c, 0x0c, 0xfe, 0xcc, 0x6c, 0x3c, 0x1c}, // four
  {0x00, 0x78, 0xcc, 0x0c, 0x0c, 0xf8, 0xc0, 0xfc}, // five
  {0x00, 0x78, 0xcc, 0xcc, 0xf8, 0xc0, 0x60, 0x38}, // six
  {0x00, 0x60, 0x60, 0x30, 0x18, 0x0c, 0xcc, 0xfc}, // seven
  {0x00, 0x78, 0xcc, 0xcc, 0x78, 0xcc, 0xcc, 0x78}, // eight
  {0x00, 0x70, 0x18, 0x0c, 0x7c, 0xcc, 0xcc, 0x78} // nine
};

#define DEBUGGING(...) Serial.println( __VA_ARGS__ )
#define DEBUGGING_L(...) Serial.print( __VA_ARGS__ )
#define DEBUGGING_F(...) Serial.printf( __VA_ARGS__ )

#ifndef DEBUGGING
#define DEBUGGING(...)
#endif
#ifndef DEBUGGING_L
#define DEBUGGING_L(...)
#endif
#ifndef DEBUGGING_F
#define DEBUGGING_F(...)
#endif

void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUGGING_L("Entered config mode");
  DEBUGGING_L(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUGGING_L(myWiFiManager->getConfigPortalSSID());
}

void setup()
{
  Serial.begin(115200);

  WiFi.hostname(HOSTNAME);
  WiFiManager wifiManager;
  // put your setup code here, to run once:
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //wifiManager.resetSettings();

  wifiManager.setDebugOutput(true);

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    DEBUGGING("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  if (!mdns.begin(HOSTNAME, WiFi.localIP())) {
    DEBUGGING("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("mDNS responder started");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)PASSWORD);

  ArduinoOTA.onStart([]() {
    DEBUGGING_L("Start");
  });
  ArduinoOTA.onEnd([]() {
    DEBUGGING_L("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUGGING_F("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUGGING_F("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUGGING_L("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUGGING_L("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUGGING_L("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUGGING_L("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUGGING_L("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("Starting UDP");
  Udp.begin(localPort);

  for (int i = 0; i < 4; i++)
  {
    lc.shutdown(i, false); // come out of powersaving
    lc.setIntensity(i, 8); // set brightness 0-15
    lc.clearDisplay(i);   // clear display
  }

  for (int i = 0; i < NUM_DISPLAY; i++)
  {
    drawNum(1, NUM_DISPLAY);
  }

  // get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  setSyncProvider(getNtpTime);
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  mdns.update();
  ArduinoOTA.handle();
  client.loop();

  if (timeStatus() != timeNotSet) {
    if (now() > prevDisplay) { //update the display only if time has changed
      prevDisplay = now() + 59;
      //digitalClockDisplay();
      displayHour();
    }
  }
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}



void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}


void drawNum(int number, int display)
{
  lc.setColumn(display, 0, num[number][0]);
  lc.setColumn(display, 1, num[number][1]);
  lc.setColumn(display, 2, num[number][2]);
  lc.setColumn(display, 3, num[number][3]);
  lc.setColumn(display, 4, num[number][4]);
  lc.setColumn(display, 5, num[number][5]);
  lc.setColumn(display, 6, num[number][6]);
  lc.setColumn(display, 7, num[number][7]);
}

void displayHour()
{
  int display[NUM_DISPLAY];
  display[3] = hour() / 10;
  display[2] = hour() % 10;
  if (minute() < 10)
  {
    display[1] = 0;
  }
  else
  {
    display[1] = minute() / 10;
  }

  display[0] = minute() % 10;

  update_antiflicker(display);
}

void displayDate()
{
  int display[NUM_DISPLAY];
  if(month() < 10)
  {
    display[3] = 0;
  }
  else
  {
    display[3] = month() / 10;
  }
  display[2] = month() % 10;
  if (day() < 10)
  {
    display[1] = 0;
  }
  else
  {
    display[1] = day() / 10;
  }

  display[0] = day() % 10;

  update_antiflicker(display);
}

void update_antiflicker(int *display)
{
   for ( int i = 0; i < NUM_DISPLAY; i++)
  {
    if ( display[i] != prev_display[i])
    {
      lc.clearDisplay(i);

      drawNum(display[i],i );
      prev_display[i] = display[i];
    }
  }
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
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
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
