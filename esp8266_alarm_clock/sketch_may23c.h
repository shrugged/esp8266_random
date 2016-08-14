#define NUM_DISPLAY 4

const int SPEAKER_PIN = D2;
const int BUTTON_PIN = D1;

IPAddress timeServerIP;
const char* ntpServerName = "ca.pool.ntp.org";

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


// data, clock, cs, numdevices
LedControl lc = LedControl(D7, D5, D4, NUM_DISPLAY);

int prev_display[NUM_DISPLAY] = { -1, -1, -1, -1 };
int current_display[NUM_DISPLAY] = { -1, -1, -1, -1 };

time_t next_alarm = 0;
time_t prevDisplay = 0; // when the digital clock was displayed

boolean alarm_enable_dot;

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

enum states
{
  VIEW_MODE,
  ALARM,
  ALARM_EDIT
};

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


void clearDisplay()
{
  for (int i = 0; i < NUM_DISPLAY; i++)
  {
    prev_display[i] = -1;
    lc.clearDisplay(i);
  }
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
