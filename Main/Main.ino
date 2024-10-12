#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "students.h" // contains students data


String api = "https://script.google.com/macros/s/AKfycbzQEVj9kmcjjy3vA9AGwSkLpsLiK8d5aYDu_KtTEl_tJleuSWbW1IPOkfLOfW4yFvM/exec";

const char* ssid = "ERROR 404";
const char* password = "11223344";

// I2C LCD display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RTC
RTC_DS1307 rtc;

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

String UID;

// Task control var
volatile bool displayTaskRunning = false;
TaskHandle_t displayTaskHandle = NULL;

byte rowPins[ROWS] = { 13, 12, 14, 27 };
byte colPins[COLS] = { 26, 25, 33, 32 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// RFID setup
#define SS_PIN 5
#define RST_PIN 17
MFRC522 rfid(SS_PIN, RST_PIN);

#define buzzerPin 4

String replaceSpace(String str) {
  String tmp = "";
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] != ' ')
      tmp += str[i];
    else
      tmp += "%20";
  }
  return tmp;
}

void Https_Request(String name, String urk) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String Path = api + "?name=" + name + "&regNo=" + urk;

    http.begin(Path.c_str());

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  pinMode(2, OUTPUT);
  digitalWrite(2, 1);

  pinMode(buzzerPin, OUTPUT);
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running! Setting the time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Set RTC to compile time
  }

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();

 
  // Start the display task on core 1
  startDisplayTask();
}

void loop() {
  // TO READ AND CHECK RFID WITH THE DATA
  UID = readRFID();
  //Serial.println(UID);//1ef3a23
  if (UID != "") {
    for (int i = 0; i < 3; i++) {
      if (students[i].rfidID == UID) {
        Entry(students[i].name, students[i].reg);
        break;
      }
    }
  }

  // TO RECIVE AND CHECK THE FINGER ID WITH THE DATA
  if (Serial.available() > 0) {
    String receivedString = Serial.readStringUntil('\n');
    int id = receivedString.toInt();
    for (int i = 0; i < 3; i++) {
      if (students[i].fingerprintID == id) {
        Entry(students[i].name, students[i].reg);
        break;
      }
    }
  }

  // TO ADD MORE STUDENT (WILL BE ADDED IN THE FUTURE)
  char key = keypad.getKey();
  if (key == 'A') {
    Serial.println("ENR 12");
  }

}

// TASK TO DISPLAY TIME ON LCD IN CORE 1
void displayTimeTask(void* pvParameters) {
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  while (displayTaskRunning) {
    DateTime now = rtc.now();

    // Clear the LCD and set the cursor to the start of the first row
    lcd.clear();
    lcd.setCursor(0, 0);
    
    // Display time in AM/PM format
    int hour = now.hour();
    String period = "AM";
    
    if (hour >= 12) {
      period = "PM";
      if (hour > 12) hour -= 12; // Convert to 12-hour format
    } else if (hour == 0) {
      hour = 12; // Midnight case
    }
    
    // Print time
    lcd.print("   ");
    lcd.print(hour);
    lcd.print(":");
    if (now.minute() < 10) lcd.print("0"); // Leading zero for minutes
    lcd.print(now.minute());
    lcd.print(":");
    if (now.second() < 10) lcd.print("0"); // Leading zero for seconds
    lcd.print(now.second());
    lcd.print(" ");
    lcd.print(period); // Append AM/PM

    // Move to the second row
    lcd.setCursor(0, 1);
    
    // Display the date and day
    lcd.print(" ");
    lcd.print(now.day(), DEC); // Day of the month
    lcd.print("/");
    lcd.print(now.month(), DEC); // Month
    lcd.print("/");
    lcd.print(now.year(), DEC); // Year
    lcd.print(" ");
    lcd.print(days[now.dayOfTheWeek()]);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}


// Functions to start & stop the Display time task.
void startDisplayTask() {
  if (!displayTaskRunning) {
    displayTaskRunning = true;
    xTaskCreatePinnedToCore(displayTimeTask, "Display Time Task", 2048, NULL, 1, &displayTaskHandle, 1); // Create a task in core 1
  }
}

void stopDisplayTask() {
  displayTaskRunning = false;
  if (displayTaskHandle != NULL) {
    vTaskDelete(displayTaskHandle); // Delete the task
    displayTaskHandle = NULL;
  }
}

// THIS FUNC HANDLES THE ENTRY DISP AND HTTP GET
void Entry(String n, String r){
        String msg = n + "\n" + r;
        Serial.println(msg);
        stopDisplayTask();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(n);
        lcd.setCursor(0, 1);
        lcd.print(r);
        beep(100);
        Https_Request(replaceSpace(n), r);
        delay(1000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ENTRY REGISTERED");
        beep(1000);
        startDisplayTask();
}

// READ RFID AND RETURNS THE ID
String readRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return "";
  }

  String id = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    id += String(rfid.uid.uidByte[i], HEX);
  }
  rfid.PICC_HaltA();
  return id;
}

// FUNCTION TO BEEP THE BUZZER (TIME)
void beep(int d) {
  digitalWrite(buzzerPin, HIGH);
  delay(d); 
  digitalWrite(buzzerPin, LOW); 
}