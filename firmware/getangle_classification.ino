//to load this code change this setting: arduino uno/Tools/Partition Scheme:HUGE APPS (3 MB no OTA/1 MB SPIFFS)
//Insert mp3 file in a folder named 'mp3' in the root path of a microSD. they are sorted by the date and hour of file uploading.
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Arduino.h>
#include <arduinoFFT.h>
#include "model_alldata.h"  //change this line for test the other models
#include "Wire.h"
#include <MPU6050_light.h>
#include <BLEServer.h>
#include <deque>
#include <math.h>
#include "Bluetooth.h"
#include <Adafruit_NeoPixel.h>
#include "DFRobotDFPlayerMini.h"  //DFPlayermini_Fast can be an alternative
// CONSTANTS
#define INTERVAL 33
#define SAMPLES 16  // Number of samples processed
#define SAMPLING_FREQUENCY 30
#define CLASSIFICATION_BUFFER_SIZE 5  // Number of classification saved before to indicate wich class is the most rapresentative(Majority Voting) - DEFAULT VALUE
#define PIN_STRIP 13
#define NUMPIXELS 10
#define LED_LOG1 32 //RED
#define LED_LOG2 25 //YELLOW
#define potPin 34

//VARIABLES

//fft variables
float vReal[SAMPLES];  // Vettore per le componenti reali
float vImag[SAMPLES];  // Vettore per le componenti immaginarie
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);
float features[3 * (SAMPLES / 2)];  // magnitude array that will cuncatenate fft results of the 3 features

// selected features - best 3 features ranked by a RandomForestClassifier
float feature1[SAMPLES];
float feature2[SAMPLES];
float feature3[SAMPLES];

int bufferIndex = 0;  // current index of samples saved in the buffer - from 0 to SAMPLES

// variables that store classification results
int classificationBuffer[CLASSIFICATION_BUFFER_SIZE];  // Buffer per le classificazioni
int classificationIndex = 0;                           // Saved Classification Index - from 0 to CLASSIFICATION_BUFFER_SIZE
int cbs = 5; //variable classification buffer size - 5 is default
MPU6050 mpu(Wire);
using namespace std;

// Temporal variable that defines acquisition cycles, f_s = 30 Hz; t_s = 33 ms
long prevMillis = 0;
unsigned long timestep = 0;
uint8_t timestepByte = 0;
int durata = 0;
int prev_durata = 0;
// bluetooth variables
bool identification = false;
void function_identify(bool identify);
void parse_commands(char* commands);
void function_(char* commands);
int current_function = 0;
bool send_diagnostic = false;
int lastReadValue = -1;  // BT trasmission command value

//byte data that will be sent to the Godot App
uint8_t log_values[11]; 
float x = 0;
float y = 0;
float z = 0;
float Gx = 0;
float Gy = 0;
float Gz = 0;
float Ax = 0;
float Ay = 0;
float Az = 0;
int classification = 0;
int mostFrequentClass = -1;

//Output Response Inizialization
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN_STRIP, NEO_GRB + NEO_KHZ800); //Led strip initialize
DFRobotDFPlayerMini player;  // Mp3 player initialize
Eloquent::ML::Port::DecisionTree clf; //if using model_all.h (with frog) micromlgen lilbrary to generate c function

//Volume settings Initializatioon
int potValue = 0;       
int volume = 0;        
int volumevalues[] = {0, 5, 10, 15, 20};

//Light intensity
int lightvalues[] = {0,32,64,128,255};
int lightvalue = 2;

void setup() {
  
  //fill the putput classification array with all -1 values, this value will be updated within the loop phase
  for (int i = 0; i < CLASSIFICATION_BUFFER_SIZE; i++) {
    classificationBuffer[i] = -1;  
  }
  
  Serial.begin(115200); //Serial comunication for debug

  // DF mini Player Inizialization (RX=16<-->TXdfmini & TX=17<-->RXdfmini)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  
  Serial.println("Connecting to DFplayer"); 
  while (!player.begin(Serial2)) {  //if dfplayer is connected the rest of the code can start to run
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" DFplayer connected!");

  //Setting start Volume
  volume = 10;
  player.volume(volume);  

  //MPU9250 Inizialization
  Wire.begin();
  uint8_t status = mpu.begin();
  Serial.print(F("MPU9250 status: "));
  Serial.println(status);
  while (status != 0) {}  // if MPU is connected the rest of the code can run
  Serial.println(F("Calculating offsets, do not move MPU6050"));
  delay(1000);
  // mpu.upsideDownMounting = true; // uncomment this line if the MPU6050 is mounted upside-down
  mpu.calcOffsets();  // gyro and accelerometer
  Serial.println("Calibration Done!\n");

  //Initialization LED for comunication Android->Arduino the light up if the relative button in the app is pressed, 
  pinMode(LED_LOG1, OUTPUT);
  pinMode(LED_LOG2, OUTPUT);
  digitalWrite(LED_LOG1, LOW);  
  digitalWrite(LED_LOG2, LOW);       
  
  //last setup for BLE and LED strip
  setup_BLE_ESP32();

  //Strip led ON signal and setup
  strip.begin();
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0));  // Accendi con un colore viola medio
  }
  strip.show();  // accensione
  delay(1500);
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));  // Spegni i LED
  }
  strip.show();   // Mostra i LED spenti
}

void loop() {

  //Setting the loop duration at frequency = 1/INTERVAL, this loop governate all the arduino loop function (they start and end together)
  long curMillis = millis();
  if (curMillis - prevMillis >= INTERVAL) {
    long int start = millis();
    
    // Volume Changing - ONLY if the potentiometer change its Value, the Volume of the speaker change its Value (This is important! if player.volume is called every loop, the loop duration gets bigger!!)
    potValue = analogRead(potPin);
    volume = mapfloat(potValue, 0, 4095, 0, 30);
    static int lastVolume = -1;
    if (volume != lastVolume) {
      player.volume(volume);
      lastVolume = volume;
      Serial.print("Volume updated at: ");
      Serial.println(volume);
    }

    //Sensor Readings
    mpu.update();
    x = float(mpu.getAngleX());
    y = float(mpu.getAngleY());
    z = float(mpu.getAngleZ());
    Gx = float(mpu.getGyroX());  //+- 500 deg/s
    Gy = float(mpu.getGyroY());
    Gz = float(mpu.getGyroZ());
    Ax = float(mpu.getAccX());  //+-2g
    Ay = float(mpu.getAccY());
    Az = float(mpu.getAccZ() - 1);  // -1 compensa l'effetto della gravità

    // filling samples buffer of the 3 selected features
    feature1[bufferIndex] = x;
    feature2[bufferIndex] = y;
    feature3[bufferIndex] = Gz;
    bufferIndex++;

    // Checking if the buffer is full, hen it is, the processing of the data start
    if (bufferIndex >= SAMPLES) {
      bufferIndex = 0;

      // FFT function Computation
      auto calculateFFT = [](const float input[], float magnitudes[], int offset) {
        for (int i = 0; i < SAMPLES; i++) {
          vReal[i] = input[i];
          vImag[i] = 0.0f;  // the Immaginary part is 0
        }
        FFT.compute(FFTDirection::Forward);
        FFT.complexToMagnitude();
        for (int i = 0; i < SAMPLES / 2; i++) {
          magnitudes[offset + i] = vReal[i];
        }
      };

      // FFT computation of the 3 features and concatenation in the 'features' variable.
      calculateFFT(feature1, features, 0);
      calculateFFT(feature2, features, SAMPLES / 2);
      calculateFFT(feature3, features, SAMPLES);
      
      /*for (int i = 0; i < 24; i++) {
      Serial.print("features[");
      Serial.print(i);
      Serial.print("]: ");
      Serial.println(features[i]);
    }
    */
      //Classify and saving the result in a buffer for Majority Voting
      classification = clf.predict(features);  
      classificationBuffer[classificationIndex] = classification;
      classificationIndex++;

      // When the buffer of the classification is full, the most rapresentative class is saved in the 'mostFrequentClass' variable
      if (classificationIndex >= cbs) {  //before there was CLASSIFICATION_BUFFER_SIZE
        int classCounts[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < cbs; i++) { //before there was CLASSIFICATION_BUFFER_SIZE
          classCounts[classificationBuffer[i]]++;
        }
        int maxCount = 0;
        for (int i = 0; i < 4; i++) {
          if (classCounts[i] > maxCount) {
            maxCount = classCounts[i];
            mostFrequentClass = i;  //majority result
          }
        }

        // Conversion from a numerical label (0,1,2,3) to textual label for Serial Printing
        String classificationString;
        switch (mostFrequentClass) {
          case 0:
            classificationString = "AIRPLANE";
            for (int i = 0; i < strip.numPixels(); i++) {
              strip.setPixelColor(i, strip.Color(lightvalue, 0, 0));  // RED
              strip.show();                                    
            }
            player.play(1);  
            break;
          case 1:
            classificationString = "CAR";
            for (int i = 0; i < strip.numPixels(); i++) {
              strip.setPixelColor(i, strip.Color(lightvalue, 0, lightvalue));  // VIOLET
              strip.show();                                  
            }
            player.play(2);  
            break;
          case 2:
            classificationString = "FROG";
            for (int i = 0; i < strip.numPixels(); i++) {
              strip.setPixelColor(i, strip.Color(0, lightvalue, 0));  // GREEN
              strip.show();                                    
            }
            player.play(3);  
            break;
          case 3:
            classificationString = "IGNORING";
            for (int i = 0; i < strip.numPixels(); i++) {
              strip.setPixelColor(i, strip.Color(0, 0, lightvalue));  // BLUE
              strip.show();                                    
            }
            player.play(4);  
            break;
          default:
            classificationString = "UNKNOWN";  //useful for debug
            break;
        }

        //DEBUG PRINTS
        Serial.print("Activity: ");
        Serial.println(classificationString);

        classificationIndex = 0;
      }
    }
    

    // BT Transmission to Arduino
    String value = stringStreamCharacteristic->getValue();  // Simulate a reading
    if (!value.isEmpty()) {
      int readValue = atoi(value.c_str());
      if (readValue != lastReadValue) {  ///COMMAND: consecutive equal commands will not work!!!, this helps to have different commands

        if (readValue == 10){////COMMAND: Start log button will turn on the led strip (orange)
          Serial.println("Start Log");
          for (int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.Color(255, 165, 0));  // Colore arancione
            strip.show();                                       
          }
          //reset all when start log is pressed
          bufferIndex=0;
          classificationIndex = 0; 
          classification = 9;
          mostFrequentClass = 9; 

        } else if (readValue == 20) {//COMMAND: Stop log button will turn on the led strip (orange)
          Serial.println("Stop Log");
          for (int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.Color(255, 165, 0));  // Colore arancione
            strip.show();                                       
          }
        }  else if (readValue == 11) {///COMMAND: classification buffer size regulation (from 11 to 15)
          cbs=1;
        }  else if (readValue == 12) {
          cbs=2;
        }  else if (readValue == 13) {
          cbs=3;
        }  else if (readValue == 14) {
          cbs=4;
        }  else if (readValue == 15) {
          cbs=5;
        } else if (readValue == 21) {////COMMAND: volume intensity (from 21 to 25)
          player.volume(volumevalues[0]);
        }  else if (readValue == 22) {
          player.volume(volumevalues[1]);
        }  else if (readValue == 23) {
          player.volume(volumevalues[2]);
        }  else if (readValue == 24) {
          player.volume(volumevalues[3]);
        }  else if (readValue == 25) {
          player.volume(volumevalues[4]);
        }  else if (readValue == 31) {////COMMAND: light intensity (from 31 to 35)
          lightvalue = lightvalues[0];
        }  else if (readValue == 32) {
          lightvalue = lightvalues[1];
        }  else if (readValue == 33) {
          lightvalue = lightvalues[2];
        }  else if (readValue == 34) {
          lightvalue = lightvalues[3];
        }  else if (readValue == 35) {
          lightvalue = lightvalues[4];
        }  

        lastReadValue = readValue;  
      }
    }

    // BT Transmission to Android
    log_values[0] = static_cast<uint8_t>(mapfloat(Ax, -2, 2, 0, 255));
    log_values[1] = static_cast<uint8_t>(mapfloat(Ay, -2, 2, 0, 255));
    log_values[2] = static_cast<uint8_t>(mapfloat(Az, -2, 2, 0, 255)); 
    log_values[3] = static_cast<uint8_t>(mapAngleToByte(x));
    log_values[4] = static_cast<uint8_t>(mapAngleToByte(y));
    log_values[5] = static_cast<uint8_t>(mapAngleToByte(z));
    log_values[6] = static_cast<uint8_t>(mapfloat(Gx, -500, 500, 0, 255));
    log_values[7] = static_cast<uint8_t>(mapfloat(Gy, -500, 500, 0, 255));
    log_values[8] = static_cast<uint8_t>(mapfloat(Gz, -500, 500, 0, 255));
    log_values[9] = static_cast<uint8_t>(classification); //single classification result
    log_values[10] = static_cast<uint8_t>(mostFrequentClass); //majority voting result
    logCharacteristic->setValue(log_values, 11);
    logCharacteristic->notify();
    
    
    

    long int ende = millis();
    durata = ende - start;
    prev_durata = durata;
    Serial.print("Durata ciclo: "); //debug print for ms passed for cicle
    Serial.println(durata);
    prevMillis = curMillis;
    timestep++;

    timestepByte++;
  }
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float euler_to_byte(float angle_in_degrees) {
  // Normalizza l'angolo tra 0 e 360 gradi
  float normalized_angle = angle_in_degrees;
  while (normalized_angle >= 360.0) {
    normalized_angle -= 360.0;
  }
  while (normalized_angle < 0.0) {
    normalized_angle += 360.0;
  }

  // Mappa [0, 360] a [0, 255]
  float byte_value = (normalized_angle / 360.0) * 255.0;

  return byte_value;
}

float mapAngleToByte(float angle_degrees) {
  // Normalizza l'angolo tra [0, 360)
  while (angle_degrees >= 360.0) {
    angle_degrees -= 360.0;
  }
  while (angle_degrees < 0.0) {
    angle_degrees += 360.0;
  }

  // Mappa [0, 360) a [0, 255] byte range centrato attorno a 128
  uint8_t byte_value = (uint8_t)((angle_degrees / 360.0) * 255 + 128) % 256;

  return byte_value;
}

void write_float(uint8_t* buff, const float value) {
  const uint8_t* addr = reinterpret_cast<const uint8_t*>(&value);
  for (int i = 0; i != 4; i++)
    *buff++ = *addr++;
}

void parse_commands(char* commands) {
  Serial.print("[DEBUG][Lokahi.ino] parse_commands()\n\t");
  Serial.print("...parsing command, whole content is: ");
  Serial.print(commands);  // NOT SURE IT IS CORRECT THE PRINT OF A CHAR POINTER, CHECK!
  Serial.print("\n\t");

  // Splitting command in tokens separated by comma
  char* token = strtok(commands, ",");

  while (token != NULL) {
    String command = String(token);

    if (command.startsWith("Lokahi_test")) {
      Serial.print("Command: Lokahi test");
      Serial.println(command);
    } else if (command.startsWith("Lokahi_test_2")) {
      Serial.print("Command: Lokahi test_2");
      Serial.println(command);
    } else if (command.startsWith("Id")) {  // THIS IS the identification on device during connect phase! Id0 means stop identity
      Serial.print("Token is: ");
      Serial.println(command);

      command.remove(0, 2);
      int id = atoi(command.c_str());

      identification = id > 0;

      function_identify(identification);

      // This means that connection has been confirmed and we reset to initial function
      if (id == 0) {
        // In LOKAHI still we have no reset() neither InitFunction()
        // reset();
        // InitFunction5();
      } else {
        Serial.print("[WARNING!] command not recognized!\n\n");
      }
    }  // ...and so on for other commands

    // Last row of while loop
    token = strtok(NULL, ",");
  }  // Close while loop, and function parse_commands() ends
}

void function_identify(bool enable) {
  Serial.print("[DEBUG][Lokahi.ino] function_identify()\n\t");
  Serial.print("enable is: ");
  Serial.println(enable);
  Serial.println();

  if (enable) {
    // Add functionality here
  } else {
    // Add functionality here
  }
}

void Init_Function0() {
  current_function = 0;
}
