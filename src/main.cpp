/*
AWS IoT Core demo
Based on 05_Mqtt_Device program, change mqtt broker to AWS IoT Core
Libraries:
- WiFi Manager
- PubSubClient
*/
#include <Arduino.h>
#include <PubSubClient.h>
#include <Ticker.h>
#if defined(ESP32)  
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
  #include <NTPClient.h>
  #include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#endif

#include <WiFiClientSecure.h>

#include <Wire.h>
#include "TickTwo.h"
#include "device.h"
#include "wifi_id.h" // define WIFI_SSID and WIFI_PASSWORD 
#include "aws_id.h"  // define

#if defined(ESP32)
  #define LED_COUNT 2
  #define ESP_getChipId() ESP.getEfuseMac()
  const uint8_t arLed[LED_COUNT] = {LED_RED, LED_GREEN};
#elif defined(ESP8266) 
  #define ESP_getChipId() ESP.getChipId()
  #define LED_COUNT 3
  const uint8_t arLed[LED_COUNT] = {LED_RED, LED_YELLOW, LED_GREEN};
  WiFiUDP udp;
  NTPClient ntp(udp);
#endif

// #define MQTT_BROKER_EMQX  "broker.emqx.io"
#define MQTT_TOPIC_PUBLISH   "esp_test/data"
#define MQTT_TOPIC_SUBSCRIBE "esp_test/cmd"  

// WiFiClient wifiClient;
WiFiClientSecure wifiClient;
PubSubClient  mqtt(wifiClient);

void initDevices();
void WifiManagerConnect(bool fReset);
void WifiConnect();
void setupSsl();
boolean mqttConnect(const char* szBroker, int nPort, const char* szClientId);
void onPublishMessage();

char g_szDeviceId[31];
TickTwo timerPublish(onPublishMessage, 3000);

void setup() {
  Serial.begin(115200);
  sprintf(g_szDeviceId, "esp_binus_iot-%08X",(uint32_t)ESP_getChipId());
  delay(100);
  initDevices();
#if defined(ESP8266)  
  WifiManagerConnect(false);
#else
  WifiConnect();
#endif  
  setupSsl();
  mqttConnect(AWS_IOT_ENDPOINT, 8883, g_szDeviceId);
  // mqttConnect(MQTT_BROKER_EMQX, 1883, g_szDeviceId);

  Serial.printf("Free Memory: %d\n", ESP.getFreeHeap());
  timerPublish.start();
}

void loop() {
  mqtt.loop();
  timerPublish.update();
}

void setupSsl()
{
#if defined(ESP8266)  
  ntp.begin();
  Serial.println("Connecting to time server...");
  while (!ntp.update())
  {
    ntp.forceUpdate();
    delay(100);
  }
  wifiClient.setX509Time(ntp.getEpochTime());

  BearSSL::X509List *cert = new BearSSL::X509List(AWS_CERT_CRT);
  BearSSL::PrivateKey *privateKey = new BearSSL::PrivateKey(AWS_CERT_PRIVATE);
  wifiClient.setClientRSACert(cert, privateKey);

  BearSSL::X509List *rootCA1 = new BearSSL::X509List(AWS_CERT_CA);
  wifiClient.setTrustAnchors(rootCA1);
  randomSeed(analogRead(0));
#elif defined(ESP32)  
  wifiClient.setCACert(AWS_CERT_CA);
  wifiClient.setCertificate(AWS_CERT_CRT);
  wifiClient.setPrivateKey(AWS_CERT_PRIVATE);
#endif
}

void initDevices()
{
  pinMode(LED_BUILTIN, OUTPUT);
  for (uint8_t i=0; i<LED_COUNT; i++)
    pinMode(arLed[i], OUTPUT);
  pinMode(PIN_SW, INPUT);
}

//Message arrived [esp32_test/cmd
void onMqttReceive(char* topic, byte* payload, unsigned int len) {
  Serial.printf("Message arrived [%s]: ", topic);
  Serial.write(payload, len);
  Serial.println();
}

int nMsgCount = 0;
void onPublishMessage()
{
  char szMsg[50];
  digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);
  sprintf(szMsg, "Hello from %s - %d", g_szDeviceId, nMsgCount++);
  mqtt.publish(MQTT_TOPIC_PUBLISH, szMsg);
  digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
}

boolean mqttConnect(const char* szBroker, int nPort, const char* szClientId) {
  mqtt.setServer(szBroker, nPort);
  mqtt.setCallback(onMqttReceive);
  Serial.printf("Connecting to %s clientId: %s...", szBroker, szClientId);

  if (!mqtt.connect(szClientId)) {
    Serial.print(" fail, rc=");
    Serial.print(mqtt.state());
    return false;
  }
  Serial.println(" success");
  mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
  Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
  onPublishMessage();
  return mqtt.connected();
}

#if defined(ESP8266)  
void WifiManagerConnect(bool fReset)
{
  WiFiManager wifiManager;
  if (fReset)
    wifiManager.resetSettings();

  if (!wifiManager.autoConnect("BinusIoT_AP", "binus123")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
}
#endif

void WifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }  
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}