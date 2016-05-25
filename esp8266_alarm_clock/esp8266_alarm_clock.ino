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
#include <Bounce2.h>
#include <Fsm.h>
#include "sketch_may23c.h"
#include "ntp_util.h"
#include "sound.h"

State state_time_mode(&time_mode_enter, NULL);
State state_date_mode(&date_mode_enter, NULL);
State state_alarm_mode(&alarm_mode_enter, NULL);

State state_alarm(&alarm_enter, &alarm_exit);

Fsm fsm(&state_date_mode);

void (*update_current)();

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

  ArduinoOTA.begin();
  Serial.println("Starting UDP");
  Udp.begin(localPort);

  for (int i = 0; i < NUM_DISPLAY; i++)
  {
    lc.shutdown(i, false); // come out of powersaving
    lc.setIntensity(i, 15); // set brightness 0-15
    lc.clearDisplay(i);   // clear display
    drawNum(1, i);
  }

  // get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  setSyncProvider(getNtpTime);

  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  next_alarm = 1464087600 +  timeZone * SECS_PER_HOUR;
  alarm_dot = true;

  fsm.add_transition(&state_time_mode, &state_date_mode, VIEW_MODE, NULL);
  fsm.add_transition(&state_date_mode, &state_alarm_mode, VIEW_MODE, NULL);
  fsm.add_transition(&state_alarm_mode, &state_time_mode, VIEW_MODE, NULL);

  fsm.add_transition(&state_time_mode, &state_alarm, ALARM_ON, NULL);
  fsm.add_transition(&state_date_mode, &state_alarm, ALARM_ON, NULL);
  fsm.add_transition(&state_alarm_mode, &state_alarm, ALARM_ON, NULL);

  fsm.add_transition(&state_alarm, &state_time_mode, ALARM_OFF, NULL);
  fsm.add_transition(&state_alarm, &state_date_mode, ALARM_OFF, NULL);
  fsm.add_transition(&state_alarm, &state_alarm_mode, ALARM_OFF, NULL);

  update_current = &time_mode;
}


void time_mode_enter()
{
  update_current = &time_mode;
}

void date_mode_enter()
{
  update_current = &date_mode;
}

void alarm_mode_enter()
{
  update_current = alarm_mode;
}

void alarm_enter()
{
  update_current = &alarm;
}

void alarm_exit()
{
  alarm_dot = false;
}

void time_mode()
{
  int display[NUM_DISPLAY];
  display[3] = hour() / 10;
  display[2] = hour() % 10;
  if (minute() < 10)
    display[1] = 0;
  else
    display[1] = minute() / 10;

  display[0] = minute() % 10;
  update_display(display);

  lc.setLed(2, 2, 5, time_dot );
  lc.setLed(2, 2, 3, time_dot );

  time_dot = !time_dot;
}

void alarm_mode()
{
  int display[NUM_DISPLAY];
  display[3] = hour(next_alarm) / 10;
  display[2] = hour(next_alarm) % 10;
  if (minute(next_alarm) < 10)
    display[1] = 0;
  else
    display[1] = minute(next_alarm) / 10;

  display[0] = minute(next_alarm) % 10;
  update_display(display);

  lc.setLed(2, 7, 4, true );
  lc.setLed(2, 7, 2, true );
}

void date_mode()
{
  int display[NUM_DISPLAY];
  if (month() < 10)
    display[3] = 0;
  else
    display[3] = month() / 10;

  display[2] = month() % 10;
  if (day() < 10)
    display[1] = 0;
  else
    display[1] = day() / 10;

  display[0] = day() % 10;
  update_display(display);
}

void alarm()
{
  if (alarm_flicker)
  {
    int display[NUM_DISPLAY];
    display[3] = hour(next_alarm) / 10;
    display[2] = hour(next_alarm) % 10;
    if (minute() < 10)
      display[1] = 0;
    else
      display[1] = minute(next_alarm) / 10;

    display[0] = minute(next_alarm) % 10;
    update_display(display);
  }
  else
    clearDisplay();

  alarm_flicker = !alarm_flicker;
  //playNote(SPEAKER_PIN, frequency, noteDuration);
}

void loop()
{
  mdns.update();
  ArduinoOTA.handle();
  client.loop();

  if (pushbutton.update()) {
    if (pushbutton.risingEdge()) {
      if (fsm.get_current_state() == &state_alarm)
      {
        fsm.trigger(ALARM_OFF);
        next_alarm += 86400 ;
      }
      else
      {
        fsm.trigger(VIEW_MODE);
      }
    }
  }

  if (timeStatus() != timeNotSet)
  {
    if (now() > prevDisplay)
    {
      //update_display();
      (*update_current)();
    }

    if (now() >= next_alarm)
      fsm.trigger(ALARM_ON);
  }
}
