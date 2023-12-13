/*
 * Project Semi-Autonomous Garden Watering System (prt-sagws)
 * 
 * Author: Disguised-Coffee
 * 
 * Description: 
 *      Didn't get enough time to integrate a database, so I adding 
 *      a DHT-11 would be ultimately useless.
 * 
 *      Simple program to water a single plant.
 * 
 *      ~ Version 1.3 ~ 
 *       - Implemented waterTheFlowers evokable function. 
 * 
 * Last updated: 12/8/23
 */

//LIBRARIES
//JSON Library
#include <JsonParserGeneratorRK.h>

//GLOBALS FOR MR WORLDWIDE
//Soil Sens 1
#define ADC_PIN_ONE A0     // Analog inputs
#define SOIL_SENS_PWR_ONE D2 // so that soil sensor doesn't ware out as quickly.

#define WATER_LEVEL_SENS A1 //POT

#define RELAY_PIN_ONE D3 //Pump 1

#define STATUS_LED D7 //Status Light

//Soil Sensor Constants:
//Note: Particle is different from Arduino, and these values vary,
//      as power draw does affect the soil sensors
#define DRY_SOIL 2033
#define WET_SOIL 1500

//Water Bucket Constants, calibrate as needed.
#define MIN_LVL 2800

#define BY_PASS_WATERLEVEL true //Bypasses the water level

#define MAX_TIME_WATERING 20  // Max seconds for watering session

//NOT SO GLOBAL VARIABLES
bool failSafe = false; //Determines whether to use the failsafe watering system (timed watering)

bool canSendData = true; //Prevents sop() from sending debug data

/*
 * prevents sop() from sending data.
*/
void stopDebugStream(){
  sop("Debug stream going dark...");
  canSendData = false;
}

// Does vice versa with sop()
void startDebugStream(){
  canSendData = true;
  sop("Debug stream going live!");
}

//Allow waterTheFlowers() to be evoked by a webhook
bool evokedWateringSession = false;

/**
 * Function to bypass watering parameters.
 * 
 * Use with caution.
*/
void evokeWatering(){
  evokedWateringSession = true;
}


//Timer setup
int counter = 0;

Timer timer(1000, updateSecondsFromTimer);

void updateSecondsFromTimer(){
    counter++;
}

void startTimer(){
  counter = 0;
  timer.start();
}

void stopTimer(){
  timer.stop();
}

// setup() runs once, when the device is first turned on.
void setup() {
  Particle.function("waterThePlants", evokeWatering); //binds function evokeWatering() to webhook
  
  Serial.begin(9600); //Remove this in final product, used for debugging
  sop("--------------\n\n~ Project SAGWS (baseline) ~ \n Running Version 1.1\n Program by Disguised_Coffee\n\n-------------- ");
  //Soil Sens Power
  pinMode(SOIL_SENS_PWR_ONE, OUTPUT);    // Set pin used to power sensor as output
  digitalWrite(SOIL_SENS_PWR_ONE, LOW);  //Ensure that this is off

  pinMode(STATUS_LED, OUTPUT);    // Set pin used to power status LED
  analogWrite(STATUS_LED, 255);

  //relay
  pinMode(RELAY_PIN_ONE,OUTPUT); 
  digitalWrite(RELAY_PIN_ONE, HIGH); //Leave this off for now (since our relay turns on at high voltages.).

  //turn off the light!
  delay(2000);
  digitalWrite(STATUS_LED, LOW);
  
  // finish initialization
  sop("Going to water the flowers!");
  waterTheFlowers();
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.
  // every 2 hours, or so, check for watering.
  if((checkTime(2) && (basicSoilSensTest() > DRY_SOIL)) || evokedWateringSession){
    evokedWateringSession = false;
    startDebugStream();
    waterTheFlowers();
    sop(String(counter));
  }
  delay(2000);
  sop("Waiting..."); //Ideally, a sleep function would be better.
  stopDebugStream(); //The first time it's fine... but please, limit your bandwidth!!!
}

/**
 * Main deal.
 * Water the Flowers.
*/
void waterTheFlowers(){
  if(testSensorEquipment()){
    // Soil Sens must be prepped before use!
    sop("Finished doing normal prep stuff... Warming up SoilSens... (20 seconds)");
    turnOnSoilSensPwr();
    delay(20000);
    sop("Preparing complete!");
    int initSM = readSoilHumidity();

    //Water the flowers!
    startTimer();
    startPump();
    int i = 0;
    stopDebugStream();
    while(isNotOverWatering() && isWaterLevelInThreshold() && counter < MAX_TIME_WATERING){
      if (i < 256){
        i = 0;
      }
      else{
        analogWrite(STATUS_LED, i);
        i++;
      }
      delay(50);
    }
    startDebugStream();
    turnOffSoilSensPwr();
    stopPump();
    stopTimer();
    createEventPayload(counter, initSM, readSoilHumidity(), readWaterLevel());

    sop("Finished Water The Flowers Cycle! \nWatered for " + String(counter) + " seconds!!");
  }else if (isWaterLevelInThreshold()) {
    sop("Check your soil sensor!!");
    safeWater();
    sop("Finished Water The Flowers Cycle! \nWatered for " + String(counter) + " seconds!!");
  }else{
    //FAILED TO WATER
    sop("Failed to water, not enough water!");
    failureLight(20);
  }
  setLastTimeSinceWater();
}

/**
 * check necessary conditions to continue watering the plants
*/
bool testSensorEquipment(){
  sop("Checking if watering is necessary and possible...");
  return !failSafe && isWaterLevelInThreshold() && soilSensorWorks();
}

// Basic Getters
int readSoilHumidity(){
    return analogRead(ADC_PIN_ONE);
}

int readWaterLevel(){
  return analogRead(WATER_LEVEL_SENS);
}

//Relay Signal Pin
void startPump(){ 
    sop("Watering...");
    digitalWrite(RELAY_PIN_ONE, LOW);
}

void stopPump(){ 
    sop("Stopping water");
    digitalWrite(RELAY_PIN_ONE, HIGH);
}

//Soil Sensor power
void turnOnSoilSensPwr(){
    digitalWrite(SOIL_SENS_PWR_ONE, HIGH);
}

void turnOffSoilSensPwr(){
    digitalWrite(SOIL_SENS_PWR_ONE, LOW);
}

// Boolean checkers
bool isNotOverWatering(){
  sop("Checking Water Humidity...");
  int lvl = readSoilHumidity();
  sop("Water Humidity lvl at " + String(lvl));
  return ((lvl > WET_SOIL));
}
//Checks if water level is within the acceptable threshold.
bool isWaterLevelInThreshold(){
  if(!BY_PASS_WATERLEVEL){
    sop("Checking water threshold...");
    int lvl = readWaterLevel();
    sop("Water Threshold : " + String(lvl) + "\n\n");
    if (lvl > MIN_LVL){
      sop("Threshold is Good!");
      return true;
    }else{
      sop("Need more water please!");
      return false;
    }
  }
  else{
    sop("! ~ Bypassing water level ~ !");
    return true;
  }
}

bool soilSensorWorks(){
  sop("Testing if soil sensor works...");
  int avg = 0;
  for(int i = 0; i < 3; i++){
    avg += readSoilHumidity();
    delay(500);
  }
  avg /= 3;
  if (avg == 0 || avg == 4095) {
    sop("Darn, sensor is broken");
      failSafe = true;
    return false;
  } else{
    sop("Sensor Test works! Gave average value of : ");
    sop(String(avg));
    return true;
  }
}

//Less power intesive test.
int basicSoilSensTest()
{
  digitalWrite(SOIL_SENS_PWR_ONE, HIGH);       // Turn sensor power on
  delay(10000);                      // Allow circuit time to settle
  int val_ADC = analogRead(ADC_PIN_ONE); // Read analog value from sensor
  delay(100);                        // Small delay probably not needed (going to leave this for now)
  digitalWrite(SOIL_SENS_PWR_ONE, LOW);        // Turn sensor power off
  return val_ADC;                    // Return analog moisture value
}

//Watering lights
void blinkWateringLight(){
  for(int i = 0; i < 256; i++){
    analogWrite(STATUS_LED, i);
    delay(50);
  }
  for(int i = 255; i > 0; i--){
    analogWrite(STATUS_LED, i);
    delay(50);
  }
}

/**
 * LED pattern when watering has failed,
 *
 * Does 1 blink per second.
*/
void failureLight(int maxTime){
  sop("Doing failure light...");
  startTimer();
  while(counter < maxTime){
    if((counter % 2) == 0){
    }else{
      digitalWrite(STATUS_LED,LOW);
    }
    digitalWrite(STATUS_LED,HIGH);
  }
  stopTimer();
}

/**
 * failSafe Mode
 * Uses a timer to water plants instead.
*/
void safeWater(){
  sop("Safe watering...");
  startTimer();
  startPump();
  int i = 0;
  while(isWaterLevelInThreshold() && counter < MAX_TIME_WATERING){
    if (i < 256){
      i = 0;
    }
    else{
      analogWrite(STATUS_LED, i);
      i++;
    }
    delay(50);
  }
  stopPump();
  stopTimer();
  createEventPayload(counter, -1, -1 , readWaterLevel());
}

// Time checker.
int currentTime;

void setLastTimeSinceWater()
{
    currentTime = Time.now();
}

/**
 * Checks time if time is greater than specified hours
*/
bool checkTime(int hours){
    return Time.hour(Time.now() - currentTime) >= hours;
}

/**
 * System.out.println()
 * (An AP CS A Java Reference.)
 * Desc: outputs text to Serial monitor if available and/or the Particle cloud if enabled.
*/
void sop(String str){
  if(Serial){
    Serial.println(str);
  }
  if(canSendData){
    JsonWriterStatic<256> jw;
    {
        JsonWriterAutoObject obj(&jw);
        jw.insertKeyValue("debug", str);
    }
    Particle.publish("debugMsg", jw.getBuffer());
  }
}


/**
 * Creates payload to send to a database, or used as helpful debug info.
 * 
 * Originally from Particle's Air Sensor tutorial
 * 
 * @param pumpRunTime Runtime of the pump
 * @param initSM initial Soil Humidity
 * @param finl final Soil Humidity
 * @param waterLvl waterlevel of the bucket
*/
void createEventPayload(int pumpRunTime, int initSM, int finlSM, int waterLvl){//, int envTemp, int envHumd){
  JsonWriterStatic<256> jw;
  {
    JsonWriterAutoObject obj(&jw);

    //Pump 1
    jw.insertKeyValue("pump-on-time", pumpRunTime);
    if(initSM != -1){
      jw.insertKeyValue("init-soil-hum", initSM);
      jw.insertKeyValue("final-soil-hum", finlSM);
    }
    
    jw.insertKeyValue("curr-water-l", waterLvl);
    
    // DHT-11 values (To be implemented...)
    // if(envTemp != -1){
    //   jw.insertKeyValue("env-temp", envTemp);
    //   jw.insertKeyValue("env-humid", envHumd);
    // }
  }
  // Publish info to DB
  Particle.publish("sendWateringInformation", jw.getBuffer());
}