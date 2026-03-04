

#include "Bluetooth.h"



BLEService *generalService;
BLECharacteristic *commandCharacteristic;
BLECharacteristic *stringStreamCharacteristic;
BLECharacteristic *logCharacteristic;
BLECharacteristic *diagnosticCharacteristic;

void setup_BLE_ESP32()
{
  BLEDevice::init("ECHO"); 
  BLEDevice::setMTU(512);
  BLEServer *bleServer = BLEDevice::createServer(); //create server
  bleServer->setCallbacks(new ServerCallbacksHandler()); // set callbacks for connections and disconnections

  CharacteristicCallback *charCallback = new CharacteristicCallback();
  generalService = bleServer->createService(GENERAL_SERVICE_UUID);
 
  // debug characteristic 
  diagnosticCharacteristic = generalService->createCharacteristic(DIAGNOSTIC_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY );
  diagnosticCharacteristic->setCallbacks(charCallback);

  // command characteristic
  commandCharacteristic = generalService->createCharacteristic(COMMAND_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY ); //read??
  commandCharacteristic->setCallbacks(charCallback);

  // command characteristic
  stringStreamCharacteristic = generalService->createCharacteristic(STRING_STREAM_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY );
  stringStreamCharacteristic->setCallbacks(charCallback);

  // touch status chara
  logCharacteristic = generalService->createCharacteristic(LOG_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  logCharacteristic->setCallbacks(charCallback);
 
  //pCharacteristic->setValue(&currentButtonState, 4);//set current value of the characteristic
  generalService->start();

  // Add the service to our "advertisement" and start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(GENERAL_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined!");
}


