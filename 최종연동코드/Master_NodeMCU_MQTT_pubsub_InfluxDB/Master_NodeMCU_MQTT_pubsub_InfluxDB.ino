#include <Wire.h> //통신과 관련된 라이브러리
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define Gled D5
#define Yled D6
#define Rled D7

#define DEVICE "ESP8266"
//자신의 wifi와 비번에 맞게 바꾸기
//#define WIFI_SSID "KT_GiGA_2G_0056"
//#define WIFI_PASSWORD "9bfa4df641"
#define WIFI_SSID "806호"
#define WIFI_PASSWORD "soom806*"

//influxDB 정보도 내껄로 바꾸기
#define INFLUXDB_URL "https://europe-west1-1.gcp.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "YR8i0gtr_8vTa9M4EkJHX7HvPtyVJ8dyRCxeg9KcygS9ojYsmMdgJbc9DXcvjx3z24NPoFW2esEslFb-_BlIxQ=="
#define INFLUXDB_ORG "c3312546fb56c4de"

#define INFLUXDB_BUCKET "IOTSensor"

#define TZ_INFO "PKT-5"

//와이파이 이름,비번 바꾸고
//cmd창에서 ipconfig를 통해 주소확인하고, 아래의 코드 동일하게 바꾸기
const char* ssid = "806호";
const char* password = "soom806*";
const char* mqtt_server = "192.168.0.62";

StaticJsonDocument<200> doc; //RAM 할당

WiFiClient espClient;
PubSubClient client(espClient);

ESP8266WiFiMulti wifiMulti;
InfluxDBClient dbclient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN,InfluxDbCloud2CACert);

Point sensor("Sensor"); //테이블 이름

long lastMsg = 0;
char msg[50];

float windspeed = -1;
float wave = -1;
float hpa = -1;

float predict1 = -1;
float predict2 = -1;
  
void setup() {
  Serial.begin(9600); /* begin serial for debug */
  pinMode(Gled,OUTPUT);
  pinMode(Yled,OUTPUT);
  pinMode(Rled,OUTPUT);
  Wire.begin(D1, D2); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */
  while (!Serial) continue;

  setup_MQTT();
  setup_InfluxDB();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
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
}

void setup_MQTT(){
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.subscribe("Model");    
}

void setup_InfluxDB(){
  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  // Add tags
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (dbclient.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(dbclient.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(dbclient.getLastErrorMessage());
  }
}

//subscribe 해온 메시지를 문자열로 만듬(byte -> 문자열)
void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

  subFromModel(payload);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("Model");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

////////////////////////////////////////////////////////////////
//대부분의 설정은 아래의 코드들을 건드리면 됩니다.
void loop() {
  windspeed = readWindspeed();
  wave = readWave();
  hpa = readHpa();

  Serial.print("SensorData: ");
  Serial.print("Windspeed: ");
  Serial.print(windspeed);
  Serial.print(", Wave: ");
  Serial.print(wave);
  Serial.print(", Hpa: ");
  Serial.println(hpa);

  makeJson(windspeed, wave, hpa);
  insertInfluxDB(windspeed, wave, hpa, predict1, predict2);

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();

  if(predict1 == 0){
    digitalWrite(Gled, HIGH);
    digitalWrite(Yled, LOW);
    digitalWrite(Rled, LOW);
  }
  else if(predict1 == 1){
    digitalWrite(Gled, LOW);
    digitalWrite(Yled, HIGH);
    digitalWrite(Rled, LOW);
  }
  else if(predict1 == 2){
    digitalWrite(Gled, LOW);
    digitalWrite(Yled, LOW);
    digitalWrite(Rled, HIGH);
  }

  delay(1000);
}

float readWindspeed(){
  Wire.beginTransmission(8); /* begin with device address 8 */
  Wire.write(0);
  Wire.endTransmission();    /* stop transmitting */

  Wire.requestFrom(8, 13); /* request & read data of size 13 from slave */

  String dString = "";
  while (Wire.available()) {
    char c = Wire.read();
    dString = dString + c;
  }

  float windspeed = dString.toFloat();
  
  return windspeed;
}

float readWave(){
  Wire.beginTransmission(8); /* begin with device address 8 */
  Wire.write(1);
  Wire.endTransmission();    /* stop transmitting */

  Wire.requestFrom(8, 13); /* request & read data of size 13 from slave */

  String dString = "";
  while (Wire.available()) {
    char c = Wire.read();
    dString = dString + c;
  }

  float wave = dString.toFloat();

  return wave;
}

float readHpa(){
  Wire.beginTransmission(8); /* begin with device address 8 */
  Wire.write(2);
  Wire.endTransmission();    /* stop transmitting */

  Wire.requestFrom(8, 13); /* request & read data of size 13 from slave */

  String dString = "";
  while (Wire.available()) {
    char c = Wire.read();
    dString = dString + c;
  }

  float hpa = dString.toFloat();
  
  return hpa;
}

void makeJson(float windspeed, float wave, float hpa){
  JsonObject root = doc.to<JsonObject>();
  root["Wind"] = windspeed;
  root["Wave"] = wave;   
  root["Hpa"] = hpa;
    
  Serial.print("JsonData: ");
  serializeJson(root, Serial);  //시리얼 모니터창에 출력
  Serial.println();

  serializeJson(root, msg);  // MQTT msg array에 담기

  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;
    Serial.print("MQTT-Publish: ");
    Serial.println(msg);
    client.publish("Sensor", msg);
  }
}

void subFromModel(byte* input){

  StaticJsonDocument <256> subDoc;
  deserializeJson(subDoc,input);
  
  predict1 = subDoc["Predict1"];
  predict2 = subDoc["Predict2"];
  Serial.print("predict1 = ");
  Serial.print(predict1);
  Serial.print(", predict2 = ");
  Serial.println(predict2);
}

void insertInfluxDB(float windspeed, float wave, float hpa, float predict1, float predict2){
  sensor.clearFields();  
  sensor.addField("Windspeed",windspeed);
  sensor.addField("Wave",wave);
  sensor.addField("HPA",hpa);
  sensor.addField("predict1",predict1);
  sensor.addField("predict2",predict2);

  Serial.print("Writing: ");
  Serial.println(dbclient.pointToLineProtocol(sensor));

  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!dbclient.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(dbclient.getLastErrorMessage());
  }
  
}
