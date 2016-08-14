#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <WebSocketsServer.h>   //https://github.com/Links2004/arduinoWebSockets/tree/async
#include <Hash.h>

#define BLUE_PIN 5
#define RED_PIN 6
#define GREEN_PIN 7
 
const char* HOSTNAME = "ESP-RGB-SOFA";
#define PASSWORD "OMFG@@"

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

const char* mqtt_server = "192.168.10.89";
const char* mqtt_user = "shrugged";
const char* mqtt_pass = "fucker%%";

WiFiClient espClient;
PubSubClient client(espClient);

MDNSResponder mdns;

WebSocketsServer webSocket = WebSocketsServer(81);

// 0 to 1024
int brightness = 1024;
// status on by default
int stats = 1;
int red = 0;
int blue = 0;
int green = 0;

String toString(byte* payload, unsigned int length) {
  int i = 0;
  char buff[length + 1];
  for (i = 0; i < length; i++) {
    buff[i] = payload[i];
  }
  buff[i] = '\0';
  String msg = String(buff);
  return msg;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUGGING_L("Entered config mode");
  DEBUGGING_L(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUGGING_L(myWiFiManager->getConfigPortalSSID());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUGGING("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(HOSTNAME, mqtt_user, mqtt_pass)) {
      DEBUGGING_L("connected");
      client.subscribe("home/sofa/rgb1/brightness");
      client.subscribe("home/sofa/rgb1/switch");
      client.subscribe("home/sofa/rgb1/colors");
      return;
    } else {
      DEBUGGING("failed, rc=");
      DEBUGGING(client.state());
      DEBUGGING_L(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  
  if(strcmp(topic,"home/sofa/rgb1/colors")==0){
    String msg = toString(payload, length);
    client.publish("home/sofa/rgb1/colors_status", (const char*)payload);
    int c1 = msg.indexOf(',');
    int c2 = msg.indexOf(',',c1+1);
    
    int r =  map(msg.toInt(), 0, 255, 0, 1024);
    
     int g =  map(msg.substring(c1+1,c2).toInt(), 0, 255, 0, 1024);
     
    int b =  map(msg.substring(c2+1).toInt(), 0, 255, 0, 1024);
    
    red = r;
    green = g;
    blue = b;
  } else if (strcmp(topic,"home/sofa/rgb1/brightness")==0) {
    String msg = toString(payload, length);
    client.publish("home/sofa/rgb1/brightness_status", (const char*)payload);
    int b = msg.toInt();
    brightness = map(b, 0, 255, 0, 1024);
  } else if (strcmp(topic,"home/sofa/rgb1/switch")==0) {
    String msg = toString(payload, length);
    int s = msg.toInt();
    if(s == 1 ) {
      stats = 1;
      client.publish("home/sofa/rgb1/status", "1");
    } else if (s == 0 ) {
      stats = 0;
      client.publish("home/sofa/rgb1/status", "0");
    }
  }

  if(stats == 1) {
    analogWrite(RED_PIN, map(red, 0, 1024, 0, brightness));
    analogWrite(GREEN_PIN, map(green, 0, 1024, 0, brightness));
    analogWrite(BLUE_PIN, map(blue, 0, 1024, 0, brightness));
    delay(10);
  } else if (stats == 0) {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  analogWriteFreq(200);
  
  WiFi.hostname(HOSTNAME);

  WiFiManager wifiManager;
  // put your setup code here, to run once:
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //wifiManager.resetSettings();
  
  #ifdef DEBUGGING
    wifiManager.setDebugOutput(true);
  #endif

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
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  analogWrite(BLUE_PIN, 1023);
  analogWrite(RED_PIN, 1023);
  analogWrite(GREEN_PIN, 1023);
  client.publish("home/sofa/rgb1/brightness_status", "1023");
  
  delay(100);
}

void loop() {
  // put your main code here, to run repeatedly:
  mdns.update();
  ArduinoOTA.handle();
  
  if (!client.connected()) {
      reconnect();
  }
  client.loop();
  delay(10);
}
