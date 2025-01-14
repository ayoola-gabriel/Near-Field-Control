#include <SPI.h>
#include <Preferences.h>

#include <nRF24L01.h>
#include <printf.h>
#include <RF24_config.h>
#include <RF24.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define BROADCAST_ID "CXL5GLBHZRPPSQ30"
// #define BROADCAST_ID_SETUP "CXL5GLBHZRPPSQ30SETUP"

char unit_id[9];
uint16_t onTime = 0;
uint8_t sensitivity = 0;
const char* keyName;

bool pairMode = false;

#define sensitivityAddress "senseAddress"
#define onTimeAddress "onTimeAddress"
#define unitIdAddress "unitIDAddress"
#define keyNameAddress "keyNameAddress"

#define CSN 8
#define CE 7
#define IRQ 9

#define LED 0
#define motionSense 3
#define controlOut 1

uint32_t tt = 0;
bool outputEnabled = false;
bool timedOut = false;
uint32_t outputTimeout = 0;
uint32_t noRadioTimeOut = 0;

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e" // UART service UUID
#define CHARACTERISTIC_UUID_KEY_NAME "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_SS "6e400004-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TO "6e400005-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_ADD_KEY "6e400006-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RESET "6e400007-b5a3-f393-e0a9-e50e24dcca9e"


//create an RF24 object
RF24 radio(CE, CSN);  // CE, CSN

Preferences preferences;

//address through which two modules communicate.
const byte address[6] = "CXL5G";

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

void blink(uint8_t pin, uint8_t times){
  for(int i=0;i<times;i++){
    digitalWrite(pin, HIGH);
    delay(20);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

/****Bluetooth Callbacks****/
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class KeyNameCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.print("Key name: ");
        keyName = rxValue.c_str();
        preferences.putString(keyNameAddress, keyName);
        blink(LED,1);
      }
    }
};

class SensitivityCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        sensitivity = atoi(rxValue.c_str());
        preferences.putUInt(sensitivityAddress, sensitivity);
        blink(LED,1);
      }
    }
};

class TimeOffCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        onTime = atoi(rxValue.c_str());
        preferences.putUInt(onTimeAddress, onTime);
        blink(LED,1);
      }
    }
};

class AddKeyCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        for(int i=0;i<rxValue.length();i++){
          unit_id[i] = rxValue[i];
          Serial.print(unit_id[i]);
        }
        Serial.println();
        blink(LED,1);
        pairMode = true;
        preferences.putString(unitIdAddress,unit_id);
        
      }
    }
};

class ClearAllDataCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        preferences.clear();
        Serial.println("Clearing all data");
        sensitivity = 0;
        onTime = 0;
        blink(LED,5);
        for(int i;i<sizeof(unit_id);i++){
          unit_id[i] = 0;
        }
      }
    }
};

/******* */

bool inpair = false;

void addNewDevices(){
  if(!inpair){
  blink(LED,2);
  inpair = true;
  Serial.print("Pairing Mode\nUnit Address: ");
  char tempAddrChar[6];
  byte tempAddr[6];
  for(int i=0;i<5;i++){
    tempAddr[i] = unit_id[i];
    Serial.print((char)tempAddr[i]);
   }
  Serial.println(); 
   
  radio.openReadingPipe(0,tempAddr);
  radio.openWritingPipe(tempAddr);
  }
  
    radio.startListening();
    if (radio.available()) {
    char text[32] = "";
    radio.read(&text, sizeof(text));
    Serial.print("UNIT ID received: ");
    Serial.println(text);
    // strcpy(unit_id,text);
    if(!strcmp(text,unit_id)){
      Serial.println("Broadcasting");
      const char id[] = BROADCAST_ID;
      delay(1000);
      radio.stopListening();
      bool s = radio.write(&id, sizeof(id));
      if(s){
      Serial.print(unit_id);
      Serial.println(" ADDED");
      inpair = false;
      pairMode = false;
    } else Serial.println("Broadcast fail");
    }    
  }
}

void setup()
{
  // while(!Serial);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Device begin");
  pinMode(LED, OUTPUT);
  pinMode(motionSense,INPUT);
  pinMode(controlOut,OUTPUT_OPEN_DRAIN);
  digitalWrite(LED,LOW);
  digitalWrite(controlOut,HIGH);

  if (!radio.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }
  
  radio.setPALevel(RF24_PA_HIGH);
  
  // radio.openWritingPipe(address);
  radio.openReadingPipe(0,address);
  // attachInterrupt(digitalPinToInterrupt(button), button_ISR, FALLING);

  preferences.begin("user-data", false);

  // BLE functions
  // Create the BLE Device
  BLEDevice::init("Near Field Control");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pKeyNameCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_KEY_NAME,
											BLECharacteristic::PROPERTY_WRITE
										);

  BLECharacteristic * pSensitivityCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_SS, //sensitivity characteristics
											BLECharacteristic::PROPERTY_WRITE
										);

  BLECharacteristic * pTimeOffCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_TO, //time off characteristics
											BLECharacteristic::PROPERTY_WRITE
										);
              
  BLECharacteristic * pAddKeyCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_ADD_KEY,
											BLECharacteristic::PROPERTY_WRITE
										);

  BLECharacteristic * pClearAllCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RESET,
											BLECharacteristic::PROPERTY_WRITE
										);

  pKeyNameCharacteristic->setCallbacks(new KeyNameCallbacks());
  pSensitivityCharacteristic->setCallbacks(new SensitivityCallback());
  pTimeOffCharacteristic->setCallbacks(new TimeOffCallback());
  pAddKeyCharacteristic->setCallbacks(new AddKeyCallback());
  pClearAllCharacteristic->setCallbacks(new ClearAllDataCallback());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  sensitivity = preferences.getUInt(sensitivityAddress,0);
  onTime = preferences.getUInt(onTimeAddress,0);
  String k = preferences.getString(unitIdAddress,String());
  k.toCharArray(unit_id,9);

  Serial.printf("Sensitivity: %d, On Time: %d, UNIT ID: %s\n", sensitivity, onTime, unit_id);


}

// bool s = false;
void loop()
{
  if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
        pairMode = false;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
        oldDeviceConnected = deviceConnected;
       }
    
    if(pairMode==true){
      addNewDevices();
    }
    else {
     radio.openReadingPipe(0,address);
    radio.startListening();

    if (radio.available()) {
    char text[32] = "";
    radio.read(&text, sizeof(text));
    // Serial.println(text);
    // s = false;
    String t = String(text);
    String broadcast = t.substring(0,16);
    String key = t.substring(16);
    
   
    if(broadcast.equals(BROADCAST_ID)&&key.equals(unit_id)) {
      noRadioTimeOut = millis();
      int s = checkReceivedStrength();
      Serial.printf("RSSI: %d\n",s);

      switch(sensitivity){
        case 0: if(motionDetected()) {
                  onControl();
                }
                break;
        case 1: if(s>97){
                  onControl();
                } 
                break;
        case 2: onControl();
                break;
      } 
      
    } 
  // _delay(1000);
  } else offControl(true);
  
  if(outputEnabled){
  offControl();
  }
  
}
refreshControl();
}

bool motionDetected(){
  return digitalRead(motionSense);
}

void onControl(){
  if(!outputEnabled){
    tt = millis();
    outputEnabled = true;
    digitalWrite(controlOut, LOW);
    digitalWrite(LED, HIGH);
    Serial.println("Output turned on");
  }
}

void offControl(){
  if(onTime==0) {
    return;
  }
  uint32_t t = onTime * 1000;
  if(!timedOut) {
    outputTimeout = millis();
  }
    if((millis() - tt)>= t){
    timedOut = true;
    // tt = millis();
    
    digitalWrite(controlOut, HIGH);
    digitalWrite(LED, LOW);

  }
}

void offControl(bool s){
  if(s){
  if(millis()-noRadioTimeOut >= 15000){
  noRadioTimeOut = millis();
  digitalWrite(LED,LOW);
  digitalWrite(controlOut, HIGH);
  outputEnabled = false;
      timedOut = false;
  }
  }
}

void refreshControl(){
    if(outputEnabled && timedOut){
     if((millis()-outputTimeout) >= 30000){
      outputTimeout = millis();
      Serial.println("Output Refreshed");
      outputEnabled = false;
      timedOut = false;
    } 
  }
}


//set output delay
void _delay(uint32_t ms){
  uint32_t t = millis();
  while(millis()-t < ms);
}

int checkStrength(){
  // radio.openWritingPipe(address);
  radio.stopListening();

    radio.setRetries(0,1);
  int counter = 0;
  char buffer[32]= "***###";

    for(int i=0; i<100; i++)
{
   int status = radio.write(buffer,32); // send 32 bytes of data. It does not matter what it is
   if(status)
       counter++;

   delay(1); // try again in 1 millisecond
}
radio.setRetries(5,15);

return counter;
}

// radio.startListening();

uint8_t checkReceivedStrength(){
uint8_t counter = 0;
        // delay(1);
        for(int i=0;i<200;i++){
          if(radio.available()){
            char text[32] = "";
            radio.read(&text, sizeof(text));
            if(!strcmp(text,"***###")){
              counter++;
            }
          } 
          delay(1);
        }
        return counter;
}        
        