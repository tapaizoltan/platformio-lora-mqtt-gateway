#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include "wifi_config.h"
#include "mqtt_config.h"

#define firmwareVersion "firmware version 1.0.0b"
int heartbeatTiming = 37; // heartbeat küldés idő másodpercben

char macToId[12] = {0};
uint8_t mac[6];
String gatewayId;
unsigned long lastMillis = 0;
unsigned long heartbeatCycleCounter = 0;
String totalUptime;

// NTP updater defifinitions start
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;
String timeStamp;
String ntpDate;
String ntpTime;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//LoRa modul láb definíciók
#define SCK 18
#define MISO 19
#define MOSI 23
#define SS 5
#define RST 14
#define DIO0 2

//LoRa frekvencia definíció
#define BAND 433E6 //866E6, 915E6

// LED Pin
const int ledPin = 4;

void initwifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());
  
  WiFi.macAddress(mac);
  sprintf(macToId, "%02x%02x%02x%", mac[3], mac[4], mac[5]);
  gatewayId = String(macToId);
}

void initlora()
{
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND))
  {
    Serial.println("Initialising LoRa module...");
    while (1);
    
    //LoRa paraméterek
      LoRa.setTxPower(14); //2-17
      //LoRa.setTxPower(17, PA_OUTPUT_PA_BOOST_PIN); //Supported values are between 2 and 17 for PA_OUTPUT_PA_BOOST_PIN
      LoRa.setSpreadingFactor(7); //6-12
      LoRa.setSignalBandwidth(125E3); //7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3, 62.5E3, 125E3, and 250E3.
      LoRa.setCodingRate4(5); //5-8
      LoRa.setPreambleLength(8); //Supported values are between 6 and 65535.
      LoRa.setSyncWord(0x12);
      LoRa.enableCrc();
      //LoRa.disableCrc();
      //LoRa.enableInvertIQ();
      //LoRa.disableInvertIQ();
  }
  Serial.println("LoRa initialisation success!");
  delay(1000);
  //LoRa.onReceive(onReceive);
 }

void initoled()
{
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
}

void callback(String topic, byte* payload, unsigned int length) {
  String control_subscribe_topic_call = topic_prologue;
  control_subscribe_topic_call += gatewayId;
  control_subscribe_topic_call += "/SYS";
  control_subscribe_topic_call += "/config";

  String mqtt_to_lora_subscribe_topic_call = topic_prologue;
  mqtt_to_lora_subscribe_topic_call += gatewayId;
  mqtt_to_lora_subscribe_topic_call += "/MQTTtoLORA";
  mqtt_to_lora_subscribe_topic_call += "/config";

  if(topic == control_subscribe_topic_call){
    Serial.print("Message arrived on SYS topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String payloadTemp;
    
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      payloadTemp += (char)payload[i];
    }

    /*
     * Ide jön a parancsok kiküldése a gateway-nek.
     * JSON format:
     * config{"command":""}
     * pl.:{"command":"restart"}
     * 
     * A gateway-nek kiküldhető command-ok:
     * restart -> gateway újraindítása
     */

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payloadTemp);
    String command = doc["command"];
    
    if(command == "restart")
    {
      Serial.println("Gateway restart!");
      delay(1500);
      ESP.restart();
    }
  
    else if(command == "parancs_02")
    {
      
    }
    
    else if(command == "parancs_03")
    {
      
    }
  
    else if(command == "parancs_03")
    {
      
    }
  }

  
  if(topic == mqtt_to_lora_subscribe_topic_call)
  {
    Serial.print("Message arrived on MQTTtoLORA topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String payloadTemp;
    
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      payloadTemp += (char)payload[i];
    }
    Serial.println();
    Serial.println(payloadTemp);
     
    /*
     * Ide jön az adatok kiküldése MQTT-ről LoRa irányába.
     * JSON format:
     * config{"id":"", "command":"", "package"="", "name":"", "description":"", "price":"", "currency":"", "sale":"", "sleep":""}
     * pl.:{"id":"001200121234", "command":"new", "package"="102356", "name":"HP Probook", "description":"i3, 8GB, 256SSD", "price":"146.000", "currency":"Ft", "sale":"10"}
     * 
     * A node-nak kiküldhető command-ok:
     * status -> ekkor a node visszaküldi a státuszát.
     * new -> ilyenkor új értékeket vár a node (name, description, price, vat, currency, sale)
     * sleep -> command:sleep, valueu:értéke ekkor csak a deepsleep változik meg a node-on
     * restart -> node újraindítása
     * show -> a node kiírja a conalkódját nagyba 10 másodpercig
     * ota -> ekkor a node bekapcsolja a wifiját és várja az ota update-et
     * 
     * Kulcsok:
     * id -> annak a node-nak az id-ja amelyiknak szól a csomag.
     * command -> parancsok amiket a node hajtson végre.
     * package -> ez egy csomagazonosító.
     * name -> a pricetag-en megjelenítendő termék neve.
     * description -> a pricetag-en megjelenítendő termék leírása.
     * price -> a pricetag-en megjelenítendő termék netto ára.
     * vat -> a pricetag-en megjelenítendő termék árának ÁFA értéke.
     * currency -> a pricetag-en megjelenítendő termék árának pénzneme.
     * sale -> a pricetag-en megjelenítendő termék árából elengedett kedvezmény százalék.
     * sleep -> ezzel másodpercben megadható, hogy a price tag mennyi ideig aludjon és ne ellenőrizze le a topikokat.
     */
     
    LoRa.beginPacket();
    LoRa.print(payloadTemp);
    LoRa.endPacket();
  }
  
}

void reconnect() {
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");

    String clientId = "nedal-gateway-";
    clientId += String(WiFi.macAddress());
    

    if (client.connect(clientId.c_str(),mqtt_username,mqtt_password))
    {
      Serial.println("connected");
      
      String control_subscribe_topic_recon = topic_prologue;
      control_subscribe_topic_recon += gatewayId;
      control_subscribe_topic_recon += "/SYS";
      control_subscribe_topic_recon += "/config";
      client.subscribe(control_subscribe_topic_recon.c_str());

      String mqtt_to_lora_subscribe_topic_recon = topic_prologue;
      mqtt_to_lora_subscribe_topic_recon += gatewayId;
      mqtt_to_lora_subscribe_topic_recon += "/MQTTtoLORA";
      mqtt_to_lora_subscribe_topic_recon += "/config";
      client.subscribe(mqtt_to_lora_subscribe_topic_recon.c_str());
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttStartupStatus()
{
  if (!client.connected())
  {
    reconnect();
  }
 
  String version_status_publish_topic = topic_prologue;
  version_status_publish_topic += gatewayId;
  version_status_publish_topic += "/SYS/version";
  client.publish(version_status_publish_topic.c_str(),firmwareVersion);
}

void mqttHeartbeat()
{
  heartbeatCycleCounter++;
  
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  ntpDate = formattedDate.substring(0, splitT);
  ntpTime = formattedDate.substring(splitT+1, formattedDate.length()-1);
  String timeStamp = ntpDate;
  timeStamp += " ";
  timeStamp += ntpTime;

  String last_status_publish_topic = topic_prologue;
  last_status_publish_topic += gatewayId;
  last_status_publish_topic += "/SYS/last";
  client.publish(last_status_publish_topic.c_str(),timeStamp.c_str());

  unsigned long totalUptimeHour = (heartbeatTiming*heartbeatCycleCounter)/3600;
  unsigned long totalUptimeMinutes = ((heartbeatTiming*heartbeatCycleCounter)/60)%60;
  unsigned long totalUptimeSeconds = (heartbeatTiming*heartbeatCycleCounter)%60;
  
  String totalUptime = String(totalUptimeHour);
  totalUptime += "h";
  totalUptime += String(totalUptimeMinutes);
  totalUptime += "m";
  totalUptime += String(totalUptimeSeconds);
  totalUptime += "s";
  
  String uptime_status_publish_topic = topic_prologue;
  uptime_status_publish_topic += gatewayId;
  uptime_status_publish_topic += "/SYS/uptime";
  client.publish(uptime_status_publish_topic.c_str(),totalUptime.c_str());

  Serial.println("Transmitted heartbeat to MQTT!");
}

void setup()
{
  Serial.begin(115200);
  initlora();
  initwifi();
  initoled();
  
  display.clearDisplay();
  display.display();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("NED@L");
  display.println("GATEWAY");
  delay(2000);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.print("MAC: ");
  display.println(gatewayId);
  display.display();

  delay(2000);
  display.clearDisplay();
  
  timeClient.begin();
  timeClient.setTimeOffset(7200); //ez most GMT+2
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(65535);
  client.setCallback(callback);

  mqttStartupStatus();
 
}


void loop()
{
  if (millis() - lastMillis > (heartbeatTiming*1000))
  {
    lastMillis = millis();
    mqttHeartbeat();
  }
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet from LoRa: ");

    // NTP idő
    formattedDate = timeClient.getFormattedDate();
    int splitT = formattedDate.indexOf("T");
    ntpDate = formattedDate.substring(0, splitT);
    ntpTime = formattedDate.substring(splitT+1, formattedDate.length()-1);
    String timeStamp = ntpDate;
    timeStamp += " ";
    timeStamp += ntpTime;

    // LoRa csomagok olvasása
    while (LoRa.available()) {
      //Serial.print((char)LoRa.read());

      String LoRaData = LoRa.readString();
      String expandedLoRaData;
      /*
       * JSON format:
       * config{"id":"", "type":"", "fw":"", "name":"", "battery":"", "packetSize":"", "message":"", "rssi":"", "snr":"", "timestamp":""}
       */

      /*
       * Itt szétszedjük a LoRa irányából érkező JSON csomagot elemeire
       * és egyenként elküldjük az MQTT brókernek, majd a végén egyben is 
       * elküldjük egy 'config' topikba.
       */
       
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, LoRaData);
      const char* id = doc["id"];
      const char* type = doc["type"];
      const char* fw = doc["fw"];
      const char* nodename = doc["name"];
      const int battery = doc["battery"];
      const char* message = doc["message"];

      // Serial debug
      Serial.println(LoRaData);

      /*
       * Itt felülcsapjuk a kapott LoRaDatát rssi, snr és timestamp értékekkel
       */
      JsonDocument expandedDoc;
      expandedDoc["timestamp"] = timeStamp;
      expandedDoc["id"] = id;
      expandedDoc["type"] = type;
      expandedDoc["fw"] = fw;
      expandedDoc["name"] = nodename;
      expandedDoc["message"] = message;
      expandedDoc["battery"] = battery;
      expandedDoc["rssi"] = String(LoRa.packetRssi());
      expandedDoc["snr"] = String(LoRa.packetSnr());
      serializeJson(expandedDoc, expandedLoRaData);

      /*
       * Itt kiíratjuk a felülcsapott csomag paramétereit az OLED-re.
       */
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.println("RECEIVED PACKET");
      display.println("FROM LoRa!");
      display.println(""); 
      if (nodename != "")
      {
        display.println(nodename);
      }
      else
      {
        display.println(id);
      }
      display.print("Message: ");
      display.println(message);
      display.println(timeStamp.c_str());
      display.print("RSSI: ");
      display.print(String(LoRa.packetRssi()));
      display.println(" dBm");
      display.print("SNR: ");
      display.print(String(LoRa.packetSnr()));
      display.println(" dB");
      display.display();

      /*
      String timestamp_publish_topic = topic_prologue;
      timestamp_publish_topic += gatewayId;
      timestamp_publish_topic += "/LORAtoMQTT/";
      timestamp_publish_topic += type;
      timestamp_publish_topic += "/";
      timestamp_publish_topic += id;
      timestamp_publish_topic += timestamp_publish_topic_epilogue;
      client.publish(timestamp_publish_topic.c_str(),timeStamp.c_str());

      String rssi_publish_topic = topic_prologue;
      rssi_publish_topic += gatewayId;
      rssi_publish_topic += "/LORAtoMQTT/";
      rssi_publish_topic += type;
      rssi_publish_topic += "/";
      rssi_publish_topic += id;
      rssi_publish_topic += rssi_publish_topic_epilogue;
      client.publish(rssi_publish_topic.c_str(),String(LoRa.packetRssi()).c_str());

      String snr_publish_topic = topic_prologue;
      snr_publish_topic += gatewayId;
      snr_publish_topic += "/LORAtoMQTT/";
      snr_publish_topic += type;
      snr_publish_topic += "/";
      snr_publish_topic += id;
      snr_publish_topic += snr_publish_topic_epilogue;
      client.publish(snr_publish_topic.c_str(),String(LoRa.packetSnr()).c_str());
    
      String battery_publish_topic = topic_prologue;
      battery_publish_topic += gatewayId;
      battery_publish_topic += "/LORAtoMQTT/";
      battery_publish_topic += type;
      battery_publish_topic += "/";
      battery_publish_topic += id;
      battery_publish_topic += battery_publish_topic_epilogue;
      client.publish(battery_publish_topic.c_str(),String(battery).c_str());
      */

      String config_publish_topic = topic_prologue;
      config_publish_topic += gatewayId;
      config_publish_topic += "/LORAtoMQTT/";
      config_publish_topic += type;
      config_publish_topic += "/";
      config_publish_topic += id;
      config_publish_topic += "/config";

      client.publish(config_publish_topic.c_str(),expandedLoRaData.c_str());
      // Serial debug
      Serial.print("Transmitted packet to MQTT: ");
      Serial.println(expandedLoRaData);
    }
  }
}
