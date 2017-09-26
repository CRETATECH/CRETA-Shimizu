/**
  * This source is for Arduino Sensor Reader of Shimizu IoT System
  */

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

/* DS18B20 SETUP */
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

/* ATLAS SENSOR DEFINE */
#define CONDUCTITY_ADDR 100
#define PH_ADDR 99
#define ORP_ADDR 98
#define OXYGEN_ADDR 97

#define GENERAL_DELAY 300
#define CONDUCTIVITY_DELAY 2000
#define PH_DELAY 900
#define ORP_DELAY 900
#define OXYGEN_DELAY 600

int gDeviceAddr[4] = {CONDUCTITY_ADDR, PH_ADDR, ORP_ADDR, OXYGEN_ADDR};
int gDeviceDelay[4] = {CONDUCTIVITY_DELAY, PH_DELAY, ORP_DELAY, OXYGEN_DELAY};

/* GLOBAL VARIABLES */
String gSensorAll = "";
String gJsonRx = "";
uint32_t gSensorTime = 120000;
int gEepromFirst = 0;
int gEepromLast = 0;
uint32_t gPeriodicTime = 0;

/* JSON STRING RX */
struct {
  String T; /* Type */
  String F; /* Function */
  String D; /* Data */
} gFrameRx;

/* LOCAL FUNCTION PROTOTYPES */
void sendSensorString(void);
void readAtlasSensor(void);
void readDS18B20(void);
int parseJson(String json);
int serialCommand(void);

/* ARDUINO SETUP AND LOOP FUNCTION */
void setup() {
  Serial.begin(115200);
  Wire.begin();
  ds18b20.begin();
}

void loop() {
  char serialByte;
  uint16_t _timeOut;
  _timeOut = 0xFFFF;
  if(Serial.available()){
    serialByte = Serial.read();
    if(serialByte == '{'){
      gJsonRx += serialByte;
      while((serialByte != '}')&(_timeOut != 0)){
        if(Serial.available()){
          serialByte = Serial.read();
          gJsonRx += serialByte;
        } else{
          _timeOut--;
        }
      }
    }
    if(_timeOut == 0){
      Serial.println("{\"T\":\"RES\",\"F\":\"\",\"D\":\"ERR\"}");
    }
    else if(parseJson(gJsonRx) == 1){
      Serial.println("{\"T\":\"RES\",\"F\":\"\",\"D\":\"ERR\"}");
    } else{
      serialCommand();
    }
    gJsonRx = "";
  }
  
  if((millis() - gPeriodicTime) > gSensorTime){
    readAtlasSensor();
    readDS18B20();
    sendSensorString();
    eepromSave();
    
    // Reset all flags and global variables
    gPeriodicTime = millis();
    gSensorAll = "";
  }
}

/* LOCAL FUNCTIONS */
/**
 * @brief   To save sensor datas to EEPROM
 * @detail  There's 10 section (50 bytes each) for 10 dataset.
 *          Save gSensorAll as string.
 */
void eepromSave(void){
  char sensorstring_array[50];
  gSensorAll.toCharArray(sensorstring_array, 50);
  int i;
  // Clear that section
  for(i = 0; i < 50; i++){
    EEPROM.write((gEepromLast*50)+i, 0);
  }
  // Write to that section
  for(i = 0; i < 50; i++){
    EEPROM.write((gEepromLast*50)+i, sensorstring_array[i]);
  }
  gEepromLast++;
  if(gEepromLast == 10){
    gEepromLast = 0;
  }
  if(gEepromLast == gEepromFirst){
    gEepromFirst++;
  }
  if(gEepromFirst == 10){
    gEepromFirst = 0;
  }
}
/**
 * @brief   To Process serial command
 * @detail  Read gFrameRx to get T, F, D value 
 */
int serialCommand(void){
  if(gFrameRx.T != "CMD"){
    return 1;
  } else {
    if(gFrameRx.F == "TIME"){
      if(0 != gFrameRx.D.toInt()){
        Serial.println("{\"T\":\"RES\",\"F\":\"TIME\",\"D\":\"OK\"}");
        gSensorTime = 60000*gFrameRx.D.toInt();
      } else {
        Serial.println("{\"T\":\"RES\",\"F\":\"TIME\",\"D\":\"ERR\"}");
      }
    } else if(gFrameRx.F == "SENS"){
      int _num;
      // Uncomment if immediate reading is neccessary
      /*
      readAtlasSensor();
      readDS18B20();
      eepromSave();
      */
      if(gEepromLast >= gEepromFirst){
        _num = gEepromLast - gEepromFirst;
      } else{
        _num = gEepromFirst - gEepromLast + 9;
      }
      Serial.println("{\"T\":\"RES\",\"F\":\"SENS\",\"D\":\"" + String(_num, DEC) +"\"}");
      int i, j;
      char sensorstring_array[50];
      for(i = 0; i < _num; i++){
        // Read from EEPROM to array
        for(j = 0; j < 50; j++){
          sensorstring_array[j] = EEPROM.read((gEepromFirst*50)+j);
        }
        gSensorAll = String(sensorstring_array);
        sendSensorString();
        gEepromFirst++;
        if(gEepromFirst == 10){
          gEepromFirst = 0;
        }
      }
      gSensorAll = "";
    } else{
      Serial.println("{\"T\":\"RES\",\"F\":\"\",\"D\":\"ERR\"}");
    }
  }
  gFrameRx.T = "";
  gFrameRx.F = "";
  gFrameRx.D = "";
}
/**
 * @brief   To parse JSON string
 * @detail  JSON parsed value will be save to gFrameRx
 */
int parseJson(String json){
  DynamicJsonBuffer _buffer;
  JsonObject& _root = _buffer.parseObject(json);
  if(!_root.success()){
    return 1;
  }
  String _T = _root["T"];
  gFrameRx.T = _T;
  String _F = _root["F"];
  gFrameRx.F = _F;
  String _D = _root["D"];
  gFrameRx.D = _D;
  return 0;
}
/**
 * @brief   To send data frame
 * @detail  Data is from gSensorAll.
 */
void sendSensorString(void){
  String EC;
  String TDS;
  String SAL;
  String GRA;
  String PH;
  String ORP;
  String OXY;
  String TEM;
  String JSON;
  char sensorstring_array[50];
  /* Send Json String */
  gSensorAll.toCharArray(sensorstring_array, 50);
  OXY = strtok(sensorstring_array, ",");
  ORP = strtok(NULL, ",");
  PH = strtok(NULL, ",");
  EC = strtok(NULL, ",");
  TDS = strtok(NULL, ",");
  SAL = strtok(NULL, ",");
  GRA = strtok(NULL, ",");
  TEM = strtok(NULL, ",");
  JSON = "{\"T\":\"DATA\",\"F\":\"SENS\",";
  JSON += "\"OXY\":\"";
  JSON += OXY + "\",";
  JSON += "\"ORP\":\"";
  JSON += ORP + "\",";
  JSON += "\"PH\":\"";
  JSON += PH + "\",";
  JSON += "\"EC\":\"";
  JSON += EC + "\",";
  JSON += "\"TDS\":\"";
  JSON += TDS + "\",";
  JSON += "\"SAL\":\"";
  JSON += SAL + "\",";
  JSON += "\"GRA\":\"";
  JSON += GRA + "\",";
  JSON += "\"TEM\":\"";
  JSON += TEM + "\"}";
  Serial.println(JSON);
}
/**
 * @brief   To read 4 Atlas' sensor
 * @detail  Data sensor will be save as string to gSensorAll, each value
 *          seperate by ','.
 */
void readAtlasSensor(void){
  int i = 0;
  int j = 0;
  int _device;
  int _delay;
  uint8_t _code;
  char _sensor_data[50];
  char _char;
  /* Read 4 Atlas Sensor */
  for(j = 4; j > 0; j--){
    _device = gDeviceAddr[j-1];
    _delay = gDeviceDelay[j-1];
    Wire.beginTransmission(_device);
    // Wire.write(gComputerData);
    Wire.write('r');
    Wire.endTransmission();
    delay(_delay);
    Wire.requestFrom(_device, 20, 1);
    _code = Wire.read();
    /*
    switch (_code) {                   //switch case based on what the response code is.
      case 1:                         //decimal 1.
        Serial.println("gCode = Success");    //means the command was successful.
        break;                        //exits the switch case.
  
      case 2:                         //decimal 2.
        Serial.println("gCode = Failed");     //means the command has failed.
        break;                        //exits the switch case.
  
      case 254:                      //decimal 254.
        Serial.println("gCode = Pending");   //means the command has not yet been finished calculating.
        break;                       //exits the switch case.
  
      case 255:                      //decimal 255.
        Serial.println("gCode = No Data");   //means there is no further data to send.
        break;                       //exits the switch case.
    }
    */
    if(_code == 1){
      i = 0;
      while(Wire.available()){
        _char = Wire.read();
        _sensor_data[i] = _char;
        i++;
        if(_char == 0){
          i = 0;
          Wire.endTransmission();
          break;
        }
      }
      String str(_sensor_data);
      gSensorAll = gSensorAll + str;
    } else{
      if(j != 1){
        gSensorAll = gSensorAll + "xxx";
      } else{
        gSensorAll = gSensorAll + "xxx,xxx,xxx,xxx";
      }
    }
    if(j != 1)
      gSensorAll = gSensorAll + ',';
    /* Clear all buffer */
    memset(_sensor_data, 0x00, sizeof(_sensor_data));
  }
}
/**
 * @brief   To read DS18B20
 * @detail  Add temperature value to the end of gSensorAll
 */
void readDS18B20(void){
  float _temperature;
  /* Read DS18B20 */
  ds18b20.requestTemperatures();
  _temperature = ds18b20.getTempCByIndex(0);
  String str(_temperature);
  if(str == "-127.00"){
    str = "xxx";
  }
  if(str == "127.00"){
    str = "xxx";
  }
  gSensorAll += ',' + str;
}

