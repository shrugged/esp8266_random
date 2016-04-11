#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>

#define BLUE_PIN 12
#define RED_PIN 14
#define GREEN_PIN 13


#define HOSTNAME "ESP-HYGROMETER"
#define PASSWORD "OMFG@@"

//#define DEBUGGING(...) Serial.println( __VA_ARGS__ )
//#define DEBUGGING_L(...) Serial.print( __VA_ARGS__ )

#ifndef DEBUGGING
#define DEBUGGING(...)
#endif
#ifndef DEBUGGING_L
#define DEBUGGING_L(...)
#endif

MDNSResponder mdns;
WebSocketsServer webSocket = WebSocketsServer(7777);

#define SENSOR_PIN A0
int humidity = 0;

/*void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {  
  switch (type) {
    case WStype_DISCONNECTED:
    case WStype_CONNECTED:
    case WStype_TEXT:
    case WStype_BIN:
    case default:
      break;
    }
}*/

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
  Serial.begin(115200);

  WiFi.hostname(HOSTNAME);

  WiFiManager wifiManager;
  // put your setup code here, to run once:
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //wifiManager.resetSettings();

  // Turn off debug output
  wifiManager.setDebugOutput(false);


  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  if (!mdns.begin(HOSTNAME, WiFi.localIP())) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  webSocket.begin();
  //webSocket.onEvent(webSocketEvent);

  mdns.addService("ws", "tcp", 7777);
  Serial.println("mDNS responder started");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();

  pinMode(SENSOR_PIN, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  mdns.update();
  webSocket.loop();
  ArduinoOTA.handle();

 int sensorValue = analogRead(SENSOR_PIN);
 //sensorValue = constrain (sensorValue, 100,600);
 Serial.println(sensorValue);
 humidity = map (sensorValue, 0, 1024, 100, 0);
  Serial.print (humidity);
 Serial.println ("%");
 delay(1000); //collecting values between seconds
}
