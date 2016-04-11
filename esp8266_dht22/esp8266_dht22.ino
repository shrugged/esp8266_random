#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <DHT.h>

#define HOSTNAME "ESP-DHT"
#define PASSWORD "OMFG@@"

#define DEBUGGING(...) Serial.println( __VA_ARGS__ )
#define DEBUGGING_L(...) Serial.print( __VA_ARGS__ )


#ifndef DEBUGGING
#define DEBUGGING(...)
#endif
#ifndef DEBUGGING_L
#define DEBUGGING_L(...)
#endif

#define DHT_PIN D4
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);
float temperature;
float humidity;

const char* mqtt_server = "192.168.10.219";
const char* mqtt_user = "shrugged";
const char* mqtt_pass = "fucker%%";

MDNSResponder mdns;

WiFiClient espClient;
PubSubClient client(espClient);

#define SLEEP_DELAY_IN_SECONDS  300

void configModeCallback (WiFiManager *myWiFiManager) {
  DEBUGGING("Entered config mode");
  DEBUGGING(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUGGING(myWiFiManager->getConfigPortalSSID());
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
  wifiManager.setDebugOutput(true);


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
    DEBUGGING("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }


  DEBUGGING("mDNS responder started");

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOSTNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)PASSWORD);

  ArduinoOTA.onStart([]() {
    
  });
  ArduinoOTA.onEnd([]() {
    
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUGGING("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUGGING("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUGGING("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUGGING("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUGGING("End Failed");
  });
  
  ArduinoOTA.begin();

  client.setServer(mqtt_server, 1883);
  
  // Setup DHT22
   dht.begin();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(HOSTNAME, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
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

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Hi there, I am back");
  mdns.update();
  ArduinoOTA.handle();

  if (!client.connected()) {
      reconnect();
  }
  client.loop();
  
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  delay(500);
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  char t2[50];
  char h2[50];
  dtostrf(t, 5, 2, t2);
  dtostrf(h, 5, 2, h2);
 
  client.publish("/sensors/outside/temp", t2 );  
  client.publish("/sensors/outside/humidity", h2 );

  Serial.println("Going to sleep");
  ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
  delay(500);
}
