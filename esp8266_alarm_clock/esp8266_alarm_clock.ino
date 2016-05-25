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

State state_time_mode(&time_mode_enter, &time_mode, NULL);
State state_date_mode(&date_mode_enter, &time_mode, NULL);
State state_alarm_mode(&alarm_mode_enter, &time_mode, NULL);
State state_text_mode(&text_mode_enter, &time_mode, NULL);

State state_alarm_on_mode(&alarm_on_enter, &alarm, NULL);
State state_alarm_off_mode(&alarm_off_enter, &alarm, NULL);

Fsm fsm_alarm(&state_alarm_on_mode);
Fsm fsm_view_mode(&state_date_mode);

#define MAX_ALARMS 10
#define MAX_REFRESH_RATE 60 // in milliseconds

unsigned long last_refresh = 0;

time_t alarms[MAX_ALARMS];

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

  fsm_view_mode.add_transition(&state_time_mode, &state_date_mode, VIEW_MODE, NULL);
  fsm_view_mode.add_transition(&state_date_mode, &state_alarm_mode, VIEW_MODE, NULL);
  fsm_view_mode.add_transition(&state_alarm_mode, &state_time_mode, VIEW_MODE, NULL);
  fsm_view_mode.add_transition(&state_text_mode, &state_text_mode, VIEW_MODE, NULL);

  fsm_alarm.add_transition(&state_alarm_on_mode, &state_alarm_off_mode, ALARM, NULL);
  fsm_alarm.add_transition(&state_alarm_off_mode, &state_alarm_on_mode, ALARM, NULL);

}

void time_mode_enter()
{

}

void text_mode_enter()
{
}

void date_mode_enter()
{

}

void alarm_mode_enter()
{

}

void alarm_on_enter()
{

}

void alarm_off_enter()
{

}

void time_mode()
{
  current_display[3] = hour() / 10;
  current_display[2] = hour() % 10;
  if (minute() < 10)
    current_display[1] = 0;
  else
    current_display[1] = minute() / 10;

  current_display[0] = minute() % 10;

  lc.setLed(2, 2, 5, true );
  lc.setLed(2, 2, 3, true );
}

void alarm_mode()
{
  current_display[3] = hour(next_alarm) / 10;
  current_display[2] = hour(next_alarm) % 10;
  if (minute(next_alarm) < 10)
    current_display[1] = 0;
  else
    current_display[1] = minute(next_alarm) / 10;

  current_display[0] = minute(next_alarm) % 10;

  lc.setLed(2, 2, 5, true );
  lc.setLed(2, 2, 3, true );
}

void date_mode()
{
  if (month() < 10)
    current_display[3] = 0;
  else
    current_display[3] = month() / 10;

  current_display[2] = month() % 10;
  if (day() < 10)
    current_display[1] = 0;
  else
    current_display[1] = day() / 10;

  current_display[0] = day() % 10;
}

void text_mode()
{
}

void alarm()
{

}

void loop()
{
  mdns.update();
  ArduinoOTA.handle();
  client.loop();

  if (pushbutton.update()) {
    if (pushbutton.risingEdge()) {
      if (fsm_alarm.get_current_state() == &state_alarm_on_mode)
      {
        fsm_alarm.trigger(ALARM);
        next_alarm += 86400 ;
      }
      else
      {
        fsm_view_mode.trigger(VIEW_MODE);
      }
    }
  }

  if (timeStatus() != timeNotSet)
  {
    if (millis() - last_refresh > MAX_REFRESH_RATE)
    {
      fsm_view_mode.update();

      //anti-flicker display
      for ( int i = 0; i < NUM_DISPLAY; i++)
      {
        if ( current_display[i] != prev_display[i])
        {
          lc.clearDisplay(i);

          drawNum(current_display[i], i );
          prev_display[i] = current_display[i];
        }
      }

      if (alarm_dot)
        lc.setLed(0, 7, 7, true );
    }
    if (now() >= next_alarm && fsm_alarm.get_current_state() != &state_alarm_on_mode)
      fsm_alarm.trigger(ALARM);
  }

  fsm_alarm.update();
}
