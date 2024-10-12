#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

// Define pins for the fingerprint sensor
#define RX_PIN 2 // Fingerprint sensor TX
#define TX_PIN 3 // Fingerprint sensor RX

SoftwareSerial mySerial(RX_PIN, TX_PIN); // Create software serial for the fingerprint sensor
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

String cmd;

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600);
  
  Serial.println("Fingerprint sensor initializing...");
  finger.begin(57600);
  delay(5);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor ready!");
  } else {
    Serial.println("Fingerprint sensor not found. Check wiring.");
    while (1);
  }
}

void loop() {
  // Check if there's data from Serial
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("ENR")) {
      // Extract the template ID from the command
      int templateID = command.substring(4).toInt();
      enrollFingerprint(templateID);
    }
  }

  // Continuously read fingerprints
  int fingerID = getFingerprintID();
  if (fingerID >= 0) {
    Serial.println(fingerID);
  }

  delay(5); // Delay to prevent flooding the serial output
}

int getFingerprintID() {
  // Get the fingerprint ID
  int result = finger.getImage();
  if (result != FINGERPRINT_OK) return -1;

  result = finger.image2Tz();
  if (result != FINGERPRINT_OK) return -1;

  result = finger.fingerFastSearch();
  if (result != FINGERPRINT_OK) return -1;

  return finger.fingerID; // Return the detected finger ID
}

void enrollFingerprint(int templateID) {
  if (templateID < 0 || templateID > 127) { // Assuming ID range is 0-127
    Serial.println("Invalid template ID. Must be between 0 and 127.");
    return;
  }

  Serial.println("Place your finger on the sensor...");
  
  // Wait for finger to be read
  while (finger.getImage() != FINGERPRINT_OK);
  
  // Convert the image to template
  finger.image2Tz(1);
  
  Serial.println("Remove your finger...");
  delay(2000); // Wait for the finger to be removed

  Serial.println("Place your finger again...");
  
  // Wait for finger to be read again
  while (finger.getImage() != FINGERPRINT_OK);
  
  // Convert the image to template
  finger.image2Tz(2);
  
  // Create the fingerprint with the specified template ID
  int result = finger.createModel();
  if (result == FINGERPRINT_OK) {
    result = finger.storeModel(templateID);
    if (result == FINGERPRINT_OK) {
      Serial.print("Fingerprint enrolled with ID: ");
      Serial.println(templateID);
    } else {
      Serial.print("Error storing fingerprint: ");
      Serial.println(result);
    }
  } else {
    Serial.print("Error creating fingerprint model: ");
    Serial.println(result);
  }
}

