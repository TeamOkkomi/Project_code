#include <Wire.h>
#include <FreqPeriodCounter.h>

const byte counterPin = 3; 
const byte counterInterrupt = 1; // = pin 3
FreqPeriodCounter counter(counterPin, micros, 0);

int pos = 0;    // variable to store the servo position 
unsigned long windspeed; 
float wind_s = -1;

int request_type;

float wave = -1;

float hpa = -1;

void setup() {
  Wire.begin(8);                /* join i2c bus with address 8 */
  Wire.onReceive(receiveEvent); /* register receive event */
  Wire.onRequest(requestEvent); /* register request event */
  Serial.begin(9600);           /* start serial for debug */
  attachInterrupt(counterInterrupt, counterISR, CHANGE);
}

void loop() {

  //풍속센서 측정
  wind_s = getWindspeed();

  //수위센서 측정
  wave = getWave();

  //가변저항 hpa값 측정
  hpa = getHpa();
  
  delay(1000);
}

// function that executes whenever data is received from master
void receiveEvent(int howMany) {
  while (0 < Wire.available()) {
    request_type = Wire.read();      /* receive byte as a character */
    Serial.print("Request type: ");
    Serial.println(request_type);
  }
}

// function that executes whenever data is requested from master
void requestEvent() {
  // 풍속 요청
  if(request_type == 0){
    char buff[7];
    dtostrf(wind_s,7,2,buff); //실수형을 char 배열 형태로 변환

    Serial.print("SendData:");
    Serial.println(buff);
    
    Wire.write(buff); //마스터로 거리값 전송
  }

  // 파고 요청
  else if(request_type == 1){
    char buff[7];
    dtostrf(wave,7,2,buff); //실수형을 char 배열 형태로 변환

    Serial.print("SendData:");
    Serial.println(buff);
    
    Wire.write(buff); //마스터로 거리값 전송
  }

  // 기압 요청
  else if(request_type == 2){
    char buff[7];
    dtostrf(hpa,7,2,buff); //실수형을 char 배열 형태로 변환

    Serial.print("SendData:");
    Serial.println(buff);
    
    Wire.write(buff); //마스터로 거리값 전송
  }
  Serial.println();
}

float getWindspeed(){
  
  if(counter.ready()) { 
    windspeed = counter.hertz();
    if(windspeed > 200) windspeed = 0;

    wind_s = (0.062*windspeed+0.578) * 4.0; //아래의 수위센서처럼 측정값을 조절하기 위해 따로 *3을 해주었습니다. 이 곱셈값은 대강 넣어둔거라 재설정 할 필요가 있습니다
    Serial.print("windspeed: ");
    Serial.println(wind_s); 
  }
  
  return wind_s;
}

float getWave() {
  //측정 범위를 저희가 사용하는 파고 데이터의 형태와 크기에 맞게끔 재조정 해야합니다
  int level = analogRead(A0);  // 수위센서의 신호를 측정합니다. 0~1023의 값인데 직접 물에 넣어보면 1023까지는 측정되지 않습니다.
  int changelevel = map(level,0,1024,0,100); // 제가 측정했을때는 0~690까지 나왔고, 이 코드를 통해 그 범위를 0~25까지로 설정했습니다.
  wave = changelevel/10.0; // 재설정한 범위를 10으로 나누어서 0.1 ~ 2.5 정도로 측정되게 만들었습니다.
  Serial.print("Wave: ");
  Serial.println(wave);
  
  return wave;
}

float getHpa(){ //HPA값을 가변저항을 이용해서 얻을 수 있도록 했습니다.
  float r = analogRead(A1);  
  float hpa = map(r,0,1024,980,1039); //가변저항의 값이 980~1020까지 나오도록 했습니다. 필요에 따라 조정하시면 됩니다.
  Serial.print("HPA: ");  
  Serial.println(hpa);  

  return hpa;
}

void counterISR()
{ counter.poll();
}
