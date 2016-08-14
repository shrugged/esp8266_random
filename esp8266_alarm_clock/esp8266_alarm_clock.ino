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
#include <rtttl.h>
#include "sketch_may23c.h"
#include "ntp_util.h"
#include <OneButton.h>
#include <TimeAlarms.h>

const char song_P[] PROGMEM = "PacMan:b=160:32b,32p,32b6,32p,32f#6,32p,32d#6,32p,32b6,32f#6,16p,16d#6,16p,32c6,32p,32c7,32p,32g6,32p,32e6,32p,32c7,32g6,16p,16e6,16p,32b,32p,32b6,32p,32f#6,32p,32d#6,32p,32b6,32f#6,16p,16d#6,16p,32d#6,32e6,32f6,32p,32f6,32f#6,32g6,32p,32g6,32g#6,32a6,32p,32b.6";

ProgmemPlayer player(SPEAKER_PIN);

OneButton pushbutton = OneButton(BUTTON_PIN, true);

State state_time_mode(NULL, &time_mode, NULL);
State state_date_mode(NULL, &date_mode, NULL);
State state_alarm_mode(NULL, &alarm_mode, NULL);
State state_alarm_edit_mode(NULL, &alarm_edit_mode, NULL);
State state_text_mode(NULL, &text_mode, NULL);
State state_timer_mode(NULL, &timer_mode, NULL);

State state_alarm_is_on(&alarm_on_enter, &alarm_on, &alarm_on_exit);
State state_alarm_off_mode(NULL, &alarm_off, NULL);

Fsm fsm_alarm(&state_alarm_is_on);
Fsm fsm_alarm_edit_mode(&state_alarm_edit_mode);
Fsm fsm_view_mode(&state_date_mode);

#define MAX_ALARMS 10
#define MAX_REFRESH_RATE 1000 //1 seconds in milliseconds

unsigned long last_refresh = 0;
unsigned int alarm_display;

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

  Alarm.alarmRepeat(7, 0, 0, shot_alarm); //test alarm
  alarm_enable_dot = true;

  fsm_view_mode.add_transition(&state_time_mode, &state_date_mode, VIEW_MODE, NULL);
  fsm_view_mode.add_transition(&state_date_mode, &state_alarm_mode, VIEW_MODE, NULL);
  fsm_view_mode.add_transition(&state_alarm_mode, &state_time_mode, VIEW_MODE, NULL);
  fsm_view_mode.add_transition(&state_text_mode, &state_text_mode, VIEW_MODE, NULL);

  fsm_alarm.add_transition(&state_alarm_is_on, &state_alarm_off_mode, ALARM, NULL);
  fsm_alarm.add_transition(&state_alarm_off_mode, &state_alarm_is_on, ALARM, NULL);

  fsm_alarm_edit_mode.add_transition(&state_alarm_mode, &state_alarm_edit_mode, ALARM_EDIT, NULL);
  fsm_alarm_edit_mode.add_transition(&state_alarm_edit_mode, &state_alarm_mode, ALARM_EDIT, NULL);
  player.setSong(song_P);

  pushbutton.attachDoubleClick(pushbutton_doubleclick);
  pushbutton.attachClick(pushbutton_click);
  pushbutton.attachLongPressStart(pushbutton_hold);
}

void time_mode_enter()
{
  if (Alarm.count_alarms() > 0)
    alarm_enable_dot = true;
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

  if (second() % 2 == 0)
  {
    lc.setLed(2, 2, 5, true );
    lc.setLed(2, 2, 3, true );
  }
  else
  {
    lc.setLed(2, 2, 5, false );
    lc.setLed(2, 2, 3, false );
  }
}

void time_mode_exit()
{
  alarm_enable_dot = false;
}

void alarm_mode_enter()
{
  alarm_display = 0; // is it neccessary?
}

void alarm_mode()
{
  AlarmId next_alarm = Alarm.read(alarm_display);

  if (  next_alarm != dtINVALID_TIME)
  {
    current_display[3] = hour(next_alarm) / 10;
    current_display[2] = hour(next_alarm) % 10;
    if (minute(next_alarm) < 10)
      current_display[1] = 0;
    else
      current_display[1] = minute(next_alarm) / 10;

    current_display[0] = minute(next_alarm) % 10;

    if (second() % 2 == 0)
    {
      lc.setLed(2, 2, 5, true );
      lc.setLed(2, 2, 3, true );
    }
    else
    {
      lc.setLed(2, 2, 5, false );
      lc.setLed(2, 2, 3, false );
    }

    if (Alarm.isEnabled(next_alarm))
      alarm_enable_dot = true;
    else
      alarm_enable_dot = false;
  }
  alarm_display++;
}

void alarm_mode_exit()
{
  alarm_enable_dot = false;
  alarm_display = 0;
}

void alarm_edit_mode()
{
  AlarmId next_alarm = Alarm.read(alarm_display);
  if (  next_alarm != dtINVALID_TIME)
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

void timer_mode()
{
  // in minutes since i only have 4 display
}

void set_alarm_mode()
{
}

void alarm_on_enter()
{

}

void alarm_on()
{
  player.pollSong();
  
  if (second() % 2 == 0)
  {
    current_display[0] = -1;
    current_display[1] = -1;
    current_display[2] = -1;
    current_display[3] = -1;
  }
  else //just show the time
  {
    time_mode();
  }
}

void alarm_on_exit()
{
  Alarm.free(Alarm.getTriggeredAlarmId());
  //player.interrupt();
}

void alarm_off()
{

}

void shot_alarm()
{
  // prevent dual alarms
  if (fsm_alarm.get_current_state() != &state_alarm_is_on)
  {
    fsm_alarm.trigger(ALARM);
  }
}

void pushbutton_click()
{
  //stop alarm
  if (fsm_alarm.get_current_state() == &state_alarm_is_on)
  {
    fsm_alarm.trigger(ALARM);
  }
  //go through alarm list
  else if (fsm_view_mode.get_current_state() == &state_alarm_mode && alarm_display < dtNBR_ALARMS)
  {
    alarm_display++;
  }
  //go through view list
  else
  {
    fsm_view_mode.trigger(VIEW_MODE);
  }
}

void pushbutton_hold()
{
  //alarm edit mode
  if (fsm_view_mode.get_current_state() == &state_alarm_mode)
  {
    fsm_alarm_edit_mode.trigger(ALARM_EDIT);
  }
  else if (fsm_alarm_edit_mode.get_current_state() == &state_alarm_edit_mode)
  {

  }
}

void pushbutton_doubleclick()
{

}

void update_display()
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
      else if( current_display[i] == -1)
      {
        lc.clearDisplay(i);
      }
    }

    if (alarm_enable_dot)
      lc.setLed(0, 7, 7, true );

    last_refresh = millis();
  }
}

void loop()
{
  mdns.update();
  ArduinoOTA.handle();
  client.loop();
  pushbutton.tick();

  if (timeStatus() != timeNotSet)
  {
    update_display();
  }

  fsm_alarm.update();
  Alarm.delay(0);
}
