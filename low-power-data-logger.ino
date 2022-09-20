#include "secrets.h"
#include <OneWire.h>
#include <DallasTemperature.h>
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature DBS18B20(&oneWire);

#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
//#define DEVICE "ESP8266"
#define DEVICE "ESP12F-B"

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>



// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
//#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
//#define TZ_INFO "America/Sao_Paulo-3"
#define TZ_INFO "EST+3"


// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);


// Data point
Point sensor("wifi_status");

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Start up the temp sensor
  DBS18B20.begin();

 
  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  
  Serial.println();
  Serial.print("Connecting to wifi");
  int connectionAtempt = 0;
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
    connectionAtempt++;
    if (connectionAtempt>30) {
      Serial.print("Failed to connect to wifi. Deep Sleeping again...");
      ESP.deepSleep(120e6);
      delay(5000);
    }
  }
  Serial.println();
  Serial.println("Connected do wifi!");
  Serial.println(WiFi.localIP());

  // Add tags
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  Serial.println("Starting time Sync...");
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("1st: InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }


// Clear fields for reusing the point. Tags will remain untouched
  sensor.clearFields();

  int adcRead = 0; // value read from the battery
  
  adcRead = analogRead(A0);
  Serial.print("ADC: ");
  Serial.println(adcRead);
  float batteryVoltage = (float)adcRead;
  batteryVoltage = batteryVoltage * ((13+100)/13) /1023.0 *0.98;  

  Serial.print("Requesting temperatures...");
  DBS18B20.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  // After we got the temperatures, we can print them here.
  float tempC = DBS18B20.getTempCByIndex(0);
  // Check if reading was successful
  if(tempC != DEVICE_DISCONNECTED_C) 
  {
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(tempC);
    sensor.addField("temperature", tempC);
  } 
  else { Serial.println("Error: Could not read temperature data");}

  // Store measured value into point
  // Report RSSI of currently connected network
  sensor.addField("rssi", WiFi.RSSI());
  sensor.addField("battery", batteryVoltage);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  // If no Wifi signal, try to reconnect it
  //if (WiFi.status() != WL_CONNECTED) {
  //  Serial.println("Wifi connection lost");
  //}

  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  //Deep Sleep. GPIO16 tem que estar fisicamente conectado ao RST
  Serial.println("Preparing to deep-sleep... Wait 2 minutes.");
  ESP.deepSleep(120e6);
  delay(2000);

}

void loop() {
  
}
