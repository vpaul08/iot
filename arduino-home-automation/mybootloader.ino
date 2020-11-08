#include <Wire.h>
#include <RTClib.h>
#include <IRremote.h>
#include <IRremoteInt.h>

/*IR Remote settings*/
int RECV_PIN = 3;
IRrecv irrecv(RECV_PIN);
decode_results results;
unsigned long last = millis();

#define PIEZO_PIN 2
/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

/* Constants for drive dependence */
#define DURATION_DRIVEN 0
#define TIME_DRIVEN 1

/* Constants for specifying inactive of active time for the relay */
#define TIME_ACTIVE 0
#define TIME_INACTIVE 1

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)

/* Program variables */
RTC_DS3231 rtc;
DateTime now;
char daysOfTheWeek[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
int relayCode = 0;                //Variable for storing received data
int defaultDurationCode = 96;     //96*5 min = 8hrs

boolean statMode = false;
boolean monitoring = true;
int relayLen = 6;
char relayNames[6][15] = {"GEYSER", "PORCH", "GATE", "MOTOR", "BACK LIGHTS", "NIGHT LIGHTS"};
int relayState[] = {0, 0, 0, 0, 0, 0};
int relayManualState[] = {0, 0, 0, 0, 0, 0};
int relayActiveOnPowerRestore[] = {0, 1, 1, 0, 1, 1};
unsigned long relayPreviousMillis[] = {0, 0, 0, 0, 0, 0};
int relayPins[] = {4, 5, 6, 7, 8, 9};
int relayDrivePrecedences[] = {DURATION_DRIVEN, TIME_DRIVEN, TIME_DRIVEN, DURATION_DRIVEN, TIME_DRIVEN, DURATION_DRIVEN};

int relayDurations[] = {30, 240, 210, 15, 210, 540};
int relayActiveTimeHours[] = {6, 18, 19, 10, 19, 21};
int relayActiveTimeMinutes[] = {0, 45, 0, 30, 0, 0};
int relayInactiveTimeHours[] = {6, 22, 22, 10, 22, 6};
int relayInactiveTimeMinutes[] = {30, 45, 30, 45, 30, 0};

/*Test Data
  int relayDurations[] = {5,0,0,2};
  int relayActiveTimeHours[] = {14,13,13,14};
  int relayActiveTimeMinutes[] = {25,55,55,25};
  int relayInactiveTimeHours[] = {14,13,13,14};
  int relayInactiveTimeMinutes[] = {30,56,56,27};
  //*/

void showTentativeOffStatus(int relayCode) {
  Serial.print("To be turned off ");
  if (relayDrivePrecedences[relayCode] == DURATION_DRIVEN) {
    Serial.print("in ");
    Serial.print(relayDurations[relayCode]);
    Serial.print(" minutes");
  } else {
    int inactiveHr = relayInactiveTimeHours[relayCode];
    int inactiveMin = relayInactiveTimeMinutes[relayCode];
    Serial.print("at ");
    printDigits(inactiveHr, false);
    printDigits(inactiveMin, true);
  }
  Serial.println(" or manually.");
}

void turnOnRelay(int relayCode) {
  digitalWrite(relayPins[relayCode], LOW);
  relayPreviousMillis[relayCode] = millis();
  relayState[relayCode] = 1;
  Serial.print("Turning ");
  Serial.print(relayNames[relayCode]);
  Serial.println(" on.");
  showTentativeOffStatus(relayCode);
}

void turnOffRelay(int relayCode) {
  digitalWrite(relayPins[relayCode], HIGH);
  relayPreviousMillis[relayCode] = 0;
  relayState[relayCode] = 0;
  Serial.print("Turning ");
  Serial.print(relayNames[relayCode]);
  Serial.println(" off.");
}

void toggleRelayState(int relayCode, boolean isManual) {
  if (!isValidSerialChoice(relayCode)) {
    return;
  }
  if (isManual) {
    relayManualState[relayCode] = 1;
    Serial.print("\nManually ");
  }

  if (relayState[relayCode] == 0) {
    turnOnRelay(relayCode);
  } else {
    turnOffRelay(relayCode);
  }
}

void monitorRelay(int relayCode, unsigned long currentMillis) {
  if (relayManualState[relayCode]) { //Check whether relay was fiddled by the user
    if (relayState[relayCode]) { //If turned on, then turn off after duration for duration dependent and at inactive time otherwise
      if (relayDrivePrecedences[relayCode] == DURATION_DRIVEN) {
        if (currentMillis - relayPreviousMillis[relayCode] > relayDurations[relayCode] * 60 * 1000UL) {
          relayManualState[relayCode] = 0;
          turnOffRelay(relayCode);
        }
      } else {
        if ((compareActiveTimeWithNow(relayCode) == 1)) {
          relayManualState[relayCode] = 0;
          turnOffRelay(relayCode);
        }
      }
    } else { //If turned off, then reset manual flag if before or after active time so that it can be turned on at active time
      if (compareActiveTimeWithNow(relayCode) == -1 || compareActiveTimeWithNow(relayCode) == 1) {
        relayManualState[relayCode] = 0;
      }
    }
  } else if (relayState[relayCode]) {
    if (relayDrivePrecedences[relayCode] == DURATION_DRIVEN) {
      if (currentMillis - relayPreviousMillis[relayCode] > relayDurations[relayCode] * 60 * 1000UL) {
        relayManualState[relayCode] = 0;
        turnOffRelay(relayCode);
      }
    } else {
      if (compareActiveTimeWithNow(relayCode) == 1) {
        relayManualState[relayCode] = 0;
        turnOffRelay(relayCode);
      }
    }
  } else {
    if (compareActiveTimeWithNow(relayCode) == 0) {
      relayManualState[relayCode] = 0;
      turnOnRelay(relayCode);
    }
  }
}

void monitorRelays(unsigned long currentMillis) {
  if(monitoring) {
    for (int relayCode = 0; relayCode < relayLen; relayCode++) {
      monitorRelay(relayCode, currentMillis);
    }
  }
}

boolean monitorRelaysOnInit() {
  printDivider();
  Serial.println("Power restore monitor working...");
  boolean restored = false;
  for (int i = 0; i < relayLen; i++) {
    if (relayActiveOnPowerRestore[i]) {
      Serial.print("\nChecking ");
      Serial.println(relayNames[i]);
      if ((compareActiveTimeWithNow(i) == 0)) {
        restored = true;
        turnOnRelay(i);
      }
    }
  }
  if (restored) {
    Serial.println("\nRelays turned on after power restore.");
  }
  Serial.println("\nPower restore complete.");
  return restored;
}

int compareActiveTimeWithNow(int relayCode) {
  int activeHr = relayActiveTimeHours[relayCode];
  int activeMin = relayActiveTimeMinutes[relayCode];
  int inactiveHr = relayInactiveTimeHours[relayCode];
  int inactiveMin = relayInactiveTimeMinutes[relayCode];
  int nowHr = now.hour();
  int nowMin = now.minute();

  int activeTimeInMin = activeHr * 60 + activeMin;
  int inactiveTimeInMin = inactiveHr * 60 + inactiveMin;
  int nowInMin = nowHr * 60 + nowMin;

  if (nowInMin < activeTimeInMin) {
    //Serial.println("Activation time in future");
    return -1;
  } else if ((nowInMin >= activeTimeInMin && nowInMin < inactiveTimeInMin)) {
    //Serial.println("Activation time at present");
    return 0;
  } else if ((nowInMin == inactiveTimeInMin)) {
    //Serial.println("Activation time over by 1 min");
    return 1;
  } else {
    //Serial.println("Activation time over");
    return 2;
  }
}

/* Other Helper functions */
void printRelayStatus(int relayCode) {
  if (relayManualState[relayCode]) {
    Serial.print("Manually ");
  }
  if (relayState[relayCode] == 0) {
    Serial.print("OFF.");
  } else {
    Serial.print("ON since ");
    unsigned long onSince = millis() - relayPreviousMillis[relayCode];
    time(onSince);
  }
}
void printDivider() {
  Serial.println("\n------------------");
}
void showStatus(int relayCode) {
  printDivider();
  showTime();
  Serial.print("Status for: ");
  Serial.println(relayNames[relayCode]);
  Serial.print("\nCurrent Status: ");
  printRelayStatus(relayCode);
  Serial.println();
  Serial.print("\nRelay Arduino Pin: ");
  Serial.println(relayPins[relayCode]);

  Serial.print("\nRelay drive depencence:");
  if (relayDrivePrecedences[relayCode] == DURATION_DRIVEN) {
    Serial.println("DURATION");
    Serial.print("\nRelay Duration Status: ");
    if (relayDurations[relayCode] > 0) {
      Serial.print("Duration Enabled for ");
      Serial.print(relayDurations[relayCode]);
      Serial.println(" minute(s).");
    } else {
      Serial.println("No Duration set.");
    }
  } else {
    Serial.println("TIME");
  }
  Serial.print("\nActive time set at ");
  Serial.println(relayTimeToStr(relayCode, TIME_ACTIVE));
  Serial.print("\nInactive time set at ");
  Serial.println(relayTimeToStr(relayCode, TIME_INACTIVE));
}


void showMenu(boolean isStatMode, boolean isInitMenu) {
  printDivider();
  showTime();
  if (isInitMenu) {
    Serial.println("Active mode.");
    Serial.println("\nEnter the option from the list below.");
    showRelayOptions();
    Serial.println("9 -> Settings Mode");
  } else if (isStatMode) {
    Serial.println("Entered settings mode.");
    Serial.println("\nEnter the option from the list below.");
    showRelayOptions();
    Serial.println("9 -> Exit settings.");
  } else {
    Serial.println("Leaving settings mode.");
  }
  printDivider();
}

void showRelayOptions() {
  for (int i = 0; i < relayLen; i++) {
    Serial.print(i);
    Serial.print(" -> ");
    Serial.print(relayNames[i]);
    Serial.print("(");
    printRelayStatus(i);
    Serial.print(")");
    Serial.println();
  }
}

void time(long val) {
  val = val / 1000;
  int days = elapsedDays(val);
  int hours = numberOfHours(val);
  int minutes = numberOfMinutes(val);
  int seconds = numberOfSeconds(val);

  // digital clock display of current time
  // Serial.print(days,DEC);
  printDigits(hours, false);
  printDigits(minutes, true);
  printDigits(seconds, true);
}

void printDigits(byte digits, boolean printSeparator) {
  if (printSeparator)
    Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits, DEC);
}

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  } else {
    Serial.println("Found RTC");
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  now = rtc.now();
}

void showTime() {
  now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek() - 1]);
  Serial.print(") ");
  printDigits(now.hour(), false);
  printDigits(now.minute(), true);
  printDigits(now.second(), true);
  Serial.println();
}

String relayTimeToStr(int relayCode, int time_type) {
  int hr, mn;
  if (time_type == TIME_ACTIVE) {
    hr = relayActiveTimeHours[relayCode];
    mn = relayActiveTimeMinutes[relayCode];
  } else if (time_type == TIME_INACTIVE) {
    hr = relayInactiveTimeHours[relayCode];
    mn = relayInactiveTimeMinutes[relayCode];
  }

  String postmod = "am";
  String separator = ":";
  if (hr > 12) {
    postmod = "pm";
    hr = hr % 12;
  }

  if (mn < 10) {
    separator = separator + "0";
  }
  return String(hr) + separator + String(mn) + postmod;
}

boolean isValidSerialChoice(int relayCode) {
  if (relayCode >= relayLen || relayCode < 0) {
    Serial.println("Enter a valid choice:");
    return false;
  }
  return true;
}

class PiezoHelper {
  private:
    int pin;
    int delayInMillis = 100;
    int repeatShort = 2;
    int repeatLong = 4;

  public:
    PiezoHelper(int pPin) {
      pin = pPin;
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }

    void beepShort() {
      for (int i = 0; i < repeatShort; i++) {
        digitalWrite(pin, HIGH);
        delay(delayInMillis);
        digitalWrite(pin, LOW);
        delay(delayInMillis);
      }
    }

    void beepLong() {
      for (int i = 0; i < repeatLong; i++) {
        digitalWrite(pin, HIGH);
        delay(delayInMillis);
        digitalWrite(pin, LOW);
        delay(delayInMillis);
      }
    }
};

void setup() {
  Serial.begin(9600);   //Sets the baud for serial data transmission
  pinMode(LED_BUILTIN, OUTPUT);  //Sets digital pin 13 as output pin
  irrecv.enableIRIn();
  reboot();
}

void reboot() {
  monitoring = true;
  PiezoHelper piezo(PIEZO_PIN);
  initRTC();
  initRelays();
  piezo.beepShort();
  delay(1000);
  if (monitorRelaysOnInit()) {
    piezo.beepLong();
  }
  showMenu(false, true);
}

void initRelays() {
  for (int i = 0; i < relayLen; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
    relayState[i] = 0;
    relayManualState[i] = 0;
    relayPreviousMillis[i] = 0;
  }
}


void loop() {
  unsigned long currentMillis = millis();
  now = rtc.now();
  delay(100);
  monitorRelays(currentMillis);
  if (Serial.available() > 0) {    // Send data only when you receive data:
    relayCode = Serial.parseInt();        //Read the incoming data & store into data
    if (relayCode == 9) {
      statMode = !statMode;
      showMenu(statMode, false);
      return;
    }
    if (!isValidSerialChoice(relayCode)) {
      return;
    }
    if (statMode) {
      showStatus(relayCode);
    } else {
      toggleRelayState(relayCode, true);
    }
  } else if (irrecv.decode(&results)) {
    Serial.println("Received remote input, code:");
    Serial.println(results.value, HEX);
    if (currentMillis - last > 250) {
      if (results.value == 0x8166817E)  {
        Serial.println("Re-booting system...");
        reboot();
        Serial.println("System rebooted.");
      } else if (results.value == 0x8166A15E)  {
        monitoring = true;
        Serial.println("Monitoring Enabled.");
      } else if (results.value == 0x816651AE)  {
        monitoring = false;
        Serial.println("Monitoring Disabled.");
      } else {
        if (results.value == 0x8166F906)  {
          relayCode = 0;
        } else if (results.value == 0x816641BE)  {
          relayCode = 1;
        } else if (results.value == 0x8166D926)  {
          relayCode = 2;
        } else if (results.value == 0x8166C13E)  {
          relayCode = 3;
        } else {
          relayCode = -1;
        }
        if (isValidSerialChoice(relayCode)) {
          toggleRelayState(relayCode, true);
        }

      }
    }
    last = millis();
    irrecv.resume(); // Receive the next value
  }

}
