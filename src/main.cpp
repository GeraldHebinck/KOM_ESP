#include <Arduino.h>
#include <MQTT.h>
#include <MQTTClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Si7021.h>

/*
  Constants
*/
const char mqtt_head[] = "FANCTRL/SENSOR/";
const char mqtt_ota[] = "FANCTRL/SENSOR/OTA";
const char mqtt_sleeptime[] = "FANCTRL/SENSOR/SLEEPTIME";
const char ssid[] = "Pi_KOM2018";
const char password[] = "raspikom2018";
const char mqtt_server[] = "192.168.42.1";

/*
  Variables
*/
bool sleep = true;
bool sendstatus = true;

unsigned int iSleeptime = 120;
unsigned int adcValue = 0;

float humidity=0;
float temperature=0;

unsigned long sum=0;
unsigned long now=0;
unsigned long lastMsg= 0;

char msg[20];
char cId[18] = "01:23:45:67:89:AB";
char cIp[16] = "000.000.000.000";

String sIp, sId, sSleeptime;



/*
  Set ADC Mode
*/
ADC_MODE(ADC_VCC); // Set ADC to Pin A0


/*
  Create Instances
*/
WiFiClient espClient;
MQTTClient mqttClient;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
Si7021 si7021;

/*
	Callback Function
*/
 void messageReceived(MQTTClient *client, char topic[], char payload[], int payload_length) {
  Serial.print("Message arrived in Topic [");
  Serial.print(topic);
  Serial.print("] ");
  if(payload){

    for (int i = 0; i < payload_length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();

    if(strcmp(topic, mqtt_ota)){
      if ((char)payload[0] == '1') {
        sleep = false;
      } else {
        sleep = true;
      }
    }
    else
      sSleeptime = payload;
      iSleeptime = atoi(payload);
  }
  else{
    Serial.println("No Payload");
  }

}

/*
  WiFi Init Function
*/
void setup_wifi(void) {
  delay(10);
  Serial.print("Try to connect to: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected");
  sIp = WiFi.localIP().toString();
  sIp.toCharArray(cIp,16);
  sId = WiFi.macAddress();
  sId.toCharArray(cId,18);
}

/*
  HTTP Init Function
*/
void setup_http(void) {
  MDNS.begin(cIp);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", cIp);
}

/*
  MQTT Reconnect Function
*/
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(cId)) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_ota);
      mqttClient.subscribe(mqtt_sleeptime);
     } else {
      Serial.print("failed, try again in 5 seconds");
      delay(5000);
    }
  }
}

/*
  Setup Function
*/
void setup() {
	pinMode(14, OUTPUT);
	Serial.begin(74880);
  setup_wifi();
  setup_http();
  mqttClient.begin(mqtt_server, espClient);
  mqttClient.onMessageAdvanced(messageReceived);
  reconnect();
  mqttClient.publish("FANCTRL/SENSOR/IP/" + sId, sIp, true, 1);
//	mqttClient.publish(mqtt_head + sId + "/STATUS","Starting I2C", true,1);
  si7021.begin();
//	mqttClient.publish(mqtt_head + sId + "/STATUS","I2C started", true,1);
  lastMsg=millis();
  digitalWrite(14,HIGH);
}

/*
  Loop Function
*/
void loop()
{
	if(WiFi.status() != WL_CONNECTED) {
		setup_wifi();
	}
	if (!mqttClient.connected()) {
		reconnect();
	}
  httpServer.handleClient();
	mqttClient.loop();

  now = millis();
  if ((now - lastMsg) > 10000) // once per 10 seconds
  {
    lastMsg=now;

    humidity=si7021.measureHumidity();
    temperature=si7021.getTemperatureFromPreviousHumidityMeasurement();
    adcValue = analogRead(A0);

    Serial.print("Voltage = ");
    Serial.print(adcValue);
    Serial.println(" mV");
    Serial.print("Humidity = ");
    Serial.print(humidity);
    Serial.println(" %rF");
    Serial.print("Temperature = ");
    Serial.println(temperature);
    Serial.println(" °C");

    mqttClient.publish(mqtt_head + sId + "/VOLT", String(adcValue), true,1);
    mqttClient.publish(mqtt_head + sId + "/HUM", String(humidity, 2), true,1);
    mqttClient.publish(mqtt_head + sId + "/TEMP", String(temperature, 2), true,1);

    if (sleep)
    {
      Serial.println("Deepsleep for " + sSleeptime + " seconds");
      mqttClient.publish(mqtt_head + sId + "/STATUS","SLEEP", true,1);
      mqttClient.disconnect();
      digitalWrite(14,LOW);
      ESP.deepSleep(iSleeptime*10e5);
    }
    else
    {
      if(sendstatus){
        mqttClient.publish(mqtt_head + sId + "/STATUS","ALIVE", true,1);
        sendstatus = 0;
      }

    }
  }
}
