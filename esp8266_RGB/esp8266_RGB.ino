#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

#define BLUE_PIN 12
#define RED_PIN 14
#define GREEN_PIN 13
 
const char* HOSTNAME = "ESP-RGB-SOFA";
#define PASSWORD "OMFG@@"

//#define DEBUGGING(...) Serial.println( __VA_ARGS__ )
//#define DEBUGGING_L(...) Serial.print( __VA_ARGS__ )

#ifndef DEBUGGING
#define DEBUGGING(...)
#endif
#ifndef DEBUGGING_L
#define DEBUGGING_L(...)
#endif

const char* mqtt_server = "192.168.10.219";
const char* mqtt_user = "shrugged";
const char* mqtt_pass = "fucker%%";

WiFiClient espClient;
PubSubClient client(espClient);

MDNSResponder mdns;

int brightness = 100;
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
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(HOSTNAME, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe("/home/lights/sofa/color");
      client.subscribe("/home/lights/sofa/SW1");
      return;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  if(strcmp(topic,"/home/lights/sofa/color")==0){
    String msg = toString(payload, length);
    Serial.println(msg);
    int c1 = msg.indexOf(';');
    int c2 = msg.indexOf(';',c1+1);
    
    //int r = map(msg.toInt(),0,100,0,PWM_VALUE);
    //r = constrain(r,0,PWM_VALUE);
    int r =  map(msg.toInt(), 0, 100, 0, 1023);
    
    //int g = map(msg.substring(c1+1,c2).toInt(),0,100,0,PWM_VALUE);
    //g = constrain(g, 0, PWM_VALUE);
     int g =  map(msg.substring(c1+1,c2).toInt(), 0, 100, 0, 1023);
     
    //int b = map(msg.substring(c2+1).toInt(),0,100,0,PWM_VALUE);
    //b = constrain(b,0,PWM_VALUE);
    int b =  map(msg.substring(c2+1).toInt(), 0, 100, 0, 1023);
    
    /*red = gamma_table[r];
    green = gamma_table[g];
    blue = gamma_table[b];
*/
    Serial.println(r);
    Serial.println(b);
    Serial.println(g);
    analogWrite(RED_PIN, r);
    analogWrite(GREEN_PIN, g);
    analogWrite(BLUE_PIN, b);
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

  // Turn off debug output
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
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  analogWrite(BLUE_PIN, 1023);
  analogWrite(RED_PIN, 1023);
  analogWrite(GREEN_PIN, 1023);
  
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
