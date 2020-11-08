#include <IRremote.h>
#include <IRremoteInt.h>
#include <EEPROM.h>

/*LDR settings*/
#define LDR_PIN A0
#define DEFAULT_LDR_ON_VAL 400        //The default LDR value to be considered as the value for darkness in the room & signal the light on event
#define DEFAULT_LDR_OFF_VAL 800       //The default LDR value to be considered as the value for brightness in the room & signal the light off event
#define DEFAULT_LDR_STATE_CODE 1      //The default state in which to put the system when there's darkness
int ldrValNow = 0;
int ldrValAddrOn = 100;               //Address of LDR values for turning lights on in EEPROM
int ldrValAddrOff = 101;              //Address of LDR values for turning lights off in EEPROM
int ldrOnStateCodeAddr = 102;         //Address of lighting mode i.e. State in which lights get turned on in EEPROM
int ldrValOn = DEFAULT_LDR_ON_VAL;    //Setting defaults using constants defined above
int ldrValOff = DEFAULT_LDR_OFF_VAL;  //Setting defaults using constants defined above

/*IR Remote settings*/
#define RECV_PIN 2
#define RECV_DEBOUNCE_TIME 250
IRrecv irrecv(RECV_PIN);
decode_results results;
unsigned long last = 0;
unsigned long currentMillis;

/*Relay Settings*/
int relayLen = 4;
char relayNames[4][10] = {"DIM", "3W", "STRIP", "TUBE"};    //Some names of relay connections for debugging only
int relayState[] = {0, 0, 0, 0};      //States of individual relays
int relayPins[] = {12, 11, 10, 9};    //Pins of arduino connected to relays

/*Codes for different relays with named constants, highlighting what kind of light is connected to the relay*/
#define RELAY_DIM 0
#define RELAY_3W 1
#define RELAY_STRIP 2
#define RELAY_TUBE 3

/*Remote event named constants*/
#define HIGH_STATE 1    //Up button press event from remote
#define LOW_STATE 2     //Down button press from remote

/*System States*/
#define OFF_STATE 0     //State when none of the lights are on
#define DIM_STATE 1     //State when colorful 0.5W LED bulbs gets turned on. This is the dimmest light
#define HALF_BACKLIT_STATE 2  //State when warm 3W LED bulb gets turned on. This is the slightly brighter than the dimmest light
#define FULL_BACKLIT_STATE 3  //State when warm 3W LED bulb & LED backlit strip gets turned on. This is the brighter than the 3W bulb light
#define FULL_LIGHT_STATE 4    //State when 9W LED tubelight gets turned on. This is the brighted light than the any other light

/*Other settings*/
#define PIEZO_PIN 8
#define LOOP_DELAY 500
int ldrOnStateCode = DEFAULT_LDR_STATE_CODE;

boolean settingsMode = false; //If the user presses the power button, the system goes in the settings mode for some time for saving any configurations. This mode gets automatically diabled.
unsigned long settingsWaitTimerVal = 0;
unsigned long SETTINGS_MODE_INPUT_PERMISSIBLE_DELAY = 3000; //The time in which the settings mode should remain active, giving user some timed window to perform the configuration

int sysState = OFF_STATE; //Initial state of the system
boolean isManualTriggered = false;  //Flag to tell the system that the user has performed some manual operation to change the lighting mode, disabling the auto turning ON/OFF of any light based on LDR values. This was required to prevent the lights from turning on when it shouldn't and vice versa.

//Helper function to play the beep sound when the system has restarted
void beepShort() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIEZO_PIN, HIGH);
    delay(100);
    digitalWrite(PIEZO_PIN, LOW);
    delay(100);
  }
}

//Helper function to play the beep sound 3 times when some invalid configurations are being stored like trying to store a lesser LDR value for turning the lights on than turning them off
void errorBeep() {
  beepShort();
  beepShort();
  beepShort();
}

//Function to turn the specific relay on
void turnOnRelay(int relayCode) {
  digitalWrite(relayPins[relayCode], LOW);
  relayState[relayCode] = 1;
  Serial.print("Turning ");
  Serial.print(relayNames[relayCode]);
  Serial.println(" on.");
}

//Function to turn the specific relay off
void turnOffRelay(int relayCode) {
  digitalWrite(relayPins[relayCode], HIGH);
  relayState[relayCode] = 0;
  Serial.print("Turning ");
  Serial.print(relayNames[relayCode]);
  Serial.println(" off.");
}

//Main function where the processing for changing the system state happens
void changeSystemState(int stateEventType, boolean isExactStateCode) {
  boolean isValidEvent = false;
  if (!isExactStateCode && stateEventType == HIGH_STATE && sysState < 4) {
    sysState++;
    isValidEvent = true;
  } else if (!isExactStateCode && stateEventType == LOW_STATE && sysState > 0) {
    sysState--;
    isValidEvent = true;
  }

  if (isExactStateCode) {
    if (stateEventType == OFF_STATE || DIM_STATE || stateEventType == HALF_BACKLIT_STATE || stateEventType == FULL_BACKLIT_STATE || stateEventType == FULL_LIGHT_STATE) {
      isValidEvent = true;
      sysState = stateEventType;
    }
  }

  if (isValidEvent) {
    initRelays();
    if (sysState == DIM_STATE) {
      turnOnRelay(RELAY_DIM);
    } else if (sysState == HALF_BACKLIT_STATE) {
      turnOnRelay(RELAY_3W);
    } else if (sysState == FULL_BACKLIT_STATE) {
      turnOnRelay(RELAY_3W);
      turnOnRelay(RELAY_STRIP);
    } else if (sysState == FULL_LIGHT_STATE) {
      turnOnRelay(RELAY_TUBE);
    }

    showStatus();
  }
}

//Helper function for debugging
void printRelayStatus(int relayCode) {
  if (relayState[relayCode]) {
    Serial.print("ON.");
  } else {
    Serial.print("OFF");
  }
}

//Helper function for debugging
void showStatus() {
  Serial.println("\n------------------");
  Serial.println("Relay States:");
  for (int i = 0; i < relayLen; i++) {
    Serial.print(i);
    Serial.print(" -> ");
    Serial.print(relayNames[i]);
    Serial.print("(");
    printRelayStatus(i);
    Serial.print(")");
    Serial.println();
  }

  Serial.println("\n------------------");
  Serial.print("LDR value NOW:");
  Serial.println(ldrValNow);

  Serial.print("LDR address(EEPROM) and value for ON:");
  Serial.print(ldrValAddrOn);
  Serial.print("\t");
  Serial.print(ldrValOn, DEC);
  Serial.println();

  Serial.print("LDR address(EEPROM) and value for OFF:");
  Serial.print(ldrValAddrOff);
  Serial.print("\t");
  Serial.print(ldrValOff, DEC);
  Serial.println();

  Serial.print("LDR state code address(EEPROM) and value:");
  Serial.print(ldrOnStateCodeAddr);
  Serial.print("\t");
  Serial.print(ldrOnStateCode, DEC);
  Serial.println();

  Serial.println("\n------------------");
  Serial.print("System State: ");
  Serial.println(sysState);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIEZO_PIN, OUTPUT);
  irrecv.enableIRIn();
  initSys(false);
}

//Function that helps performs the initial boot, soft and factory reset. Hard reset can happen only when the power supply is disconnected from the system.
void initSys(boolean isManual) {
  initRelays();
  sysState = 0;
  settingsWaitTimerVal = 0;
  settingsMode = false;
  if (isManual) {
    beepShort();
  }
  delay(1000);
  setupRomVals();
  showStatus();
  isManualTriggered = false;
}

//Function to store the current value being read from LDR in EEPROM as the value to turn the lights automatically on
void commitLdrNowAsOn() {
  Serial.println("\n------------------");
  Serial.print("Storing current LDR value code as ON code in ROM.");
  if (ldrValNow < ldrValOff) {
    byte ldrValCode = ldrValNow / 10;
    EEPROM.write(ldrValAddrOn, ldrValCode);
  } else {
    Serial.print("LDR on value cannot be greater than LDR off value.");
    errorBeep();
  }
}

//Function to store the current value being read from LDR in EEPROM as the value to turn the lights automatically off
void commitLdrNowAsOff() {
  Serial.println("\n------------------");
  Serial.print("Storing current LDR value code as OFF code in ROM.");
  if (ldrValNow > ldrValOn) {
    byte ldrValCode = ldrValNow / 10;
    EEPROM.write(ldrValAddrOff, ldrValCode);
  } else {
    Serial.print("LDR off value cannot be lesser than LDR on value.");
    errorBeep();
  }
}

//Function to store the specified state to put the system in when automatically turning the lights on
void commitLdrOnStateCode(int stateCode) {
  Serial.println("\n------------------");
  Serial.print("Storing LDR state code in ROM as:");
  Serial.println(stateCode);
  byte ldrOnStateCodeInRom = stateCode;
  EEPROM.write(ldrOnStateCodeAddr, ldrOnStateCodeInRom);
}

//Function to override the configurations stored in EEPROM to the initial setup i.e. factory reset
void resetRomVals() {
  Serial.println("\n------------------");
  Serial.print("Storing default LDR ON/OFF codes in ROM.");
  byte defaultLdrValOnCode = DEFAULT_LDR_ON_VAL / 10;
  EEPROM.write(ldrValAddrOn, defaultLdrValOnCode);
  byte defaultLdrValOffCode = DEFAULT_LDR_OFF_VAL / 10;
  EEPROM.write(ldrValAddrOff, defaultLdrValOffCode);
  byte defaultLdrOnStateCode = DEFAULT_LDR_STATE_CODE;
  EEPROM.write(ldrOnStateCodeAddr, defaultLdrOnStateCode);
}

//Function to setup initial configurations
void setupRomVals() {
  byte ldrValOnCode = EEPROM.read(ldrValAddrOn);
  if (ldrValOnCode == 255) {
    Serial.println("\n------------------");
    Serial.print("LDR - ON code not stored. Storing default code...");
    byte defaultLdrValOnCode = DEFAULT_LDR_ON_VAL / 10;
    EEPROM.write(ldrValAddrOn, defaultLdrValOnCode);
  } else {
    ldrValOn = ldrValOnCode * 10;
  }

  byte ldrValOffCode = EEPROM.read(ldrValAddrOff);
  if (ldrValOffCode == 255) {
    Serial.println("\n------------------");
    Serial.print("LDR - OFF code not stored. Storing default code...");
    byte defaultLdrValOffCode = DEFAULT_LDR_OFF_VAL / 10;
    EEPROM.write(ldrValAddrOff, defaultLdrValOffCode);
  } else {
    ldrValOff = ldrValOffCode * 10;
  }

  byte ldrOnStateCodeInRom = EEPROM.read(ldrOnStateCodeAddr);
  if (ldrOnStateCodeInRom == 255) {
    Serial.println("\n------------------");
    Serial.print("LDR - state code not stored. Storing default code...");
    ldrOnStateCodeInRom = DEFAULT_LDR_STATE_CODE;
    EEPROM.write(ldrOnStateCodeAddr, ldrOnStateCodeInRom);
  } else {
    ldrOnStateCode = ldrOnStateCodeInRom;
  }
}

//Function to reset all the relays to OFF state
void initRelays() {
  for (int i = 0; i < relayLen; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
    relayState[i] = 0;
  }
}

//Function which keeps track of values read from LDR sensor
void monitorLdrEvents() {
  ldrValNow = analogRead(LDR_PIN);
  if (!isManualTriggered && sysState == OFF_STATE && ldrValNow < ldrValOn) {
    changeSystemState(ldrOnStateCode, true);
  } else if (!isManualTriggered && sysState == ldrOnStateCode && ldrValNow > ldrValOff) {
    changeSystemState(OFF_STATE, true);
  }
}

//Function to handle the input by the user and decide whether to put the system in settings mode
void handleRemoteInput() {
  isManualTriggered = true;
  if (settingsMode) {
    handleSettingsInput();
  } else {
    handleOperationInput();
  }
}

//Function to handle user's input when the system is in settings mode
void handleSettingsInput() {
  if (results.value == 0x8166817E)  {
    if (millis() - settingsWaitTimerVal < SETTINGS_MODE_INPUT_PERMISSIBLE_DELAY) {
      Serial.println("\nPerforming Hard Reset");
      resetRomVals();
      initSys(true);
    }
  } else if (results.value == 0x8166A15E)  {
    commitLdrNowAsOn();
  } else if (results.value == 0x816651AE)  {
    commitLdrNowAsOff();
  } else {
    int stateCode = OFF_STATE;
    if (results.value == 0x8166F906)  {
      stateCode = DIM_STATE;
    } else if (results.value == 0x816641BE)  {
      stateCode = HALF_BACKLIT_STATE;
    } else if (results.value == 0x8166D926)  {
      stateCode = FULL_BACKLIT_STATE;
    } else if (results.value == 0x8166C13E)  {
      stateCode = FULL_LIGHT_STATE;
    }
    if (stateCode != OFF_STATE) {
      commitLdrOnStateCode(stateCode);
    }
  }
}

//Function to handle user's input when the system is not in the settings mode
void handleOperationInput() {
  if (results.value == 0x8166817E)  {
    settingsMode = true;
    settingsWaitTimerVal = millis();
  } else if (results.value == 0x8166A15E)  {
    changeSystemState(HIGH_STATE, false);
    Serial.println("Firing State UP Event.");
  } else if (results.value == 0x816651AE)  {
    changeSystemState(LOW_STATE, false);
    Serial.println("Firing State DOWN Event.");
  } else if (results.value == 0x81669966) {
    isManualTriggered = false;
    showStatus();
  }
}

//Function to perform the automatic exit from the settings mode when the settings input permissible delay time has passed
void monitorSettingsMode() {
  if (settingsMode) {
    if (millis() - settingsWaitTimerVal > SETTINGS_MODE_INPUT_PERMISSIBLE_DELAY) {
      Serial.println("\nPerforming Soft Reset");
      initSys(true);
    }
  }
}

void loop() {
  delay(LOOP_DELAY);
  currentMillis = millis();
  monitorSettingsMode();
  monitorLdrEvents();
  if (irrecv.decode(&results)) {
    Serial.println("Received remote input, code:");
    Serial.println(results.value, HEX);
    if (currentMillis - last > RECV_DEBOUNCE_TIME) {
      handleRemoteInput();
    }
    last = currentMillis;
    irrecv.resume();
  }
}
