//*******************************libraries********************************
// ESP32--------------------------
#include <WiFi.h>
#include <HTTPClient.h>
#include <SimpleTimer.h>  //https://github.com/jfturcot/SimpleTimer
#include <HardwareSerial.h>
// OLED-----------------------------
#include <SPI.h>
#include <Wire.h>
#include "icons.h"
#include <Adafruit_GFX.h>          //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>      //https://github.com/adafruit/Adafruit_SSD1306
#include <Adafruit_Fingerprint.h>  //https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library
#include <ArduinoJson.h>
#include <string.h>
//************************************************************************
// Fingerprint scanner Pins
#define Finger_Rx 4  // Adjust as per your wiring
#define Finger_Tx 2  // Adjust as per your wiring
// Declaration for SSD1306 display connected using software I2C pins are(22 SCL, 21 SDA)
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)
//************************************************************************
WiFiClient client;
SimpleTimer timer;
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//************************************************************************
/* Set these to your desired credentials. */
const char *ssid = "ICONNET";
const char *password = "11223344";
const char *device_token = "UeXDtOessawgPbHfrd1X6n9xY6hWbytiYnCJXwEiTgfsNDgz6Do5JRNWZLok";
//************************************************************************
String getData, Link, finger_uuid;
String URL = "http://192.168.1.5:8001/api/v1/2024/";  // computer IP or the server domain
//************************************************************************
int FingerID = 0, t1, t2, finger_id;  // The Fingerprint ID from the scanner
bool device_Mode = false;             // Default Mode Enrollment
bool firstConnect = false;
unsigned long previousMillis = 0;
//************************************************************************
void setup() {
  Serial.begin(115200);
  delay(1000);
  //-----------initiate OLED display-------------
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  // you can delet these three lines if you don't want to get the Adfruit logo appear
  display.display();
  delay(2000);  // Pause for 2 seconds
  display.clearDisplay();
  //---------------------------------------------
  connectToWiFi();
  //---------------------------------------------
  // Set the data rate for the sensor serial port
  mySerial.begin(57600, SERIAL_8N1, Finger_Rx, Finger_Tx);
  Serial.println("\n\nAdafruit finger detect test");

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
    display.clearDisplay();
    display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
    display.display();
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    display.clearDisplay();
    display.drawBitmap(32, 0, FinPr_failed_bits, FinPr_failed_width, FinPr_failed_height, WHITE);
    display.display();
    while (1) {
      delay(1);
    }
  }
  //---------------------------------------------
  finger.getTemplateCount();
  Serial.print("Sensor contains ");
  Serial.print(finger.templateCount);
  Serial.println(" templates");
  Serial.println("Waiting for valid finger...");
  // Timers---------------------------------------
  timer.setInterval(25000L, CheckMode);
  t1 = timer.setInterval(10000L, addFingerID);  // Set an internal timer every 10sec to check if there a new fingerprint in the website to add it.
  t2 = timer.setInterval(15000L, Delete);       // Set an internal timer every 15sec to check wheater there an ID to delete in the website.
  //---------------------------------------------
  CheckMode();
}
//************************************************************************
void loop() {
  timer.run();  // Keep the timer in the loop function in order to update the time as soon as possible
  // check if there's a connection to Wi-Fi or not
  if (!WiFi.isConnected()) {
    if (millis() - previousMillis >= 10000) {
      previousMillis = millis();
      connectToWiFi();  // Retry to connect to Wi-Fi
    }
  }
  checkFingerprintID();
  delay(10);
}

//************Display the fingerprint ID state on the OLED*************
void DisplayFingerprintID() {
  //Fingerprint has been detected
  if (FingerID > 0) {
    display.clearDisplay();
    display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
    display.display();
    SendFingerprintID(FingerID);  // Send the Fingerprint ID to the website.
    delay(2000);
  }
  //---------------------------------------------
  //No finger detected
  else if (FingerID == 0) {
    display.clearDisplay();
    display.drawBitmap(32, 0, FinPr_start_bits, FinPr_start_width, FinPr_start_height, WHITE);
    display.display();
  }
  //---------------------------------------------
  //Didn't find a match
  else if (FingerID == -1) {
    display.clearDisplay();
    display.drawBitmap(34, 0, FinPr_invalid_bits, FinPr_invalid_width, FinPr_invalid_height, WHITE);
    display.display();
  }
  //---------------------------------------------
  //Didn't find the scanner or there an error
  else if (FingerID == -2) {
    display.clearDisplay();
    display.drawBitmap(32, 0, FinPr_failed_bits, FinPr_failed_width, FinPr_failed_height, WHITE);
    display.display();
  }
}
//********************Check Finger Print ID******************

void checkFingerprintID() {
  FingerID = getFingerprintID();
  DisplayFingerprintID();
}

void addFingerID() {
  getFingerprintEnroll();
}

void Delete() {
  deleteFingerID(finger_id);
}

//********************Send Delete Finger ID******************
void SendDeleteFingerID() {
  if (WiFi.isConnected()) {
    HTTPClient http;
    Link = URL + "attendance" + "/" + finger_uuid;

    http.begin(Link);

    int httpCode = http.sendRequest("DELETE");
    Serial.println(httpCode);
    if (httpCode == 200) {
      http.end();
      timer.disable(t2);
      Serial.println("Deleted Success!");
      delay(1000);
    }
    http.end();
  }
}

//********************Delete Finger ID******************
uint8_t deleteFingerID(int finger_id) {
  uint8_t p = -1;

  p = finger.deleteModel(finger_id);

  if (p == FINGERPRINT_OK) {
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Deleted!\n"));
    display.display();
    SendDeleteFingerID();
    return p;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    display.clearDisplay();
    display.setTextSize(1);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Communication error!\n"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    display.clearDisplay();
    display.setTextSize(1);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Could not delete in that location!\n"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    display.clearDisplay();
    display.setTextSize(1);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Error writing to flash!\n"));
    display.display();
    return p;
  } else {
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Unknown error:\n"));
    display.display();
    return p;
  }
}

//********************Send Finger ID******************
void SendFingerprintID(int finger_id) {
  Serial.println("Sending the Fingerprint ID");
  if (WiFi.isConnected()) {
    HTTPClient http;
    Link = URL + "attendance" + "/";

    http.begin(Link);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Token " + String(device_token));
    String postData = "{\"finger_id\": " + String(finger_id) + "}";
    int httpCode = http.POST(postData);
    Serial.println(httpCode);
    if (httpCode == 201) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, http.getString());
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }
      finger_uuid = doc["uuid"].as<String>();
      Serial.println(finger_uuid);
      http.end();
      delay(1000);
    } else {
      http.end();
    }
  }
}

//********************Enroll New Finger Print******************

uint8_t getFingerprintEnroll() {
  uint8_t p = -1;
  int finger_id = 0;
  //--------------------------------------------
  if (WiFi.isConnected()) {
    HTTPClient http;
    Link = URL + "fingerid" + "/";

    http.begin(Link);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Token " + String(device_token));
    int httpCode = http.GET();
    Serial.println(httpCode);
    if (httpCode == 200) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, http.getString());
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return p;
      }
      finger_id = doc["finger_id"].as<int>();
      Serial.println(finger_id);
      http.end();
      delay(1000);
    } else {
      http.end();
    }
  }
  //--------------------------------------------
  if (finger_id == 0) {
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("No Data"));
    display.display();
    return p;
  }
  //--------------------------------------------
  display.clearDisplay();
  display.setTextSize(1);       // Normal 1:1 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Waiting for valid finger to enroll as #"));
  display.print(finger_id);
  display.display();
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("No finger"));
        display.display();
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Com error"));
        display.display();
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Img error"));
        display.display();
        break;
      default:
        Serial.println("Unknown error");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Unk error"));
        display.display();
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Img convrt"));
      display.display();
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Img messy"));
      display.display();
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Com error"));
      display.display();
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Feature fail"));
      display.display();
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Invalid img"));
      display.display();
      return p;
    default:
      Serial.println("Unknown error");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Unk error"));
      display.display();
      return p;
  }

  Serial.println("Remove finger");
  display.clearDisplay();
  display.setTextSize(2);       // Normal 2:2 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Remove\n finger"));
  display.display();
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(finger_id);
  p = -1;
  Serial.println("Place same finger again");
  display.clearDisplay();
  display.setTextSize(2);       // Normal 2:2 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Place same\n finger"));
  display.display();
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Image taken"));
        display.display();
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("No finger"));
        display.display();
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Com error"));
        display.display();
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Img error"));
        display.display();
        break;
      default:
        Serial.println("Unknown error");
        display.clearDisplay();
        display.setTextSize(2);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("Unk error"));
        display.display();
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Img convrt"));
      display.display();
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Img messy"));
      display.display();
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Com error"));
      display.display();
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Feature fail"));
      display.display();
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Invalid img"));
      display.display();
      return p;
    default:
      Serial.println("Unknown error");
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Unk error"));
      display.display();
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(finger_id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Match\n success"));
    display.display();
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Com error"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Not match"));
    display.display();
    return p;
  } else {
    Serial.println("Unknown error");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Unk error"));
    display.display();
    return p;
  }

  Serial.print("ID ");
  Serial.println(finger_id);
  p = finger.storeModel(finger_id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Success\n Stored"));
    display.display();
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Com error"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Bad loc"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Flash err"));
    display.display();
    return p;
  } else {
    Serial.println("Unknown error");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Unk error"));
    display.display();
    return p;
  }

  //----------------------------------------
  if (WiFi.isConnected()) {
    HTTPClient http;
    Link = URL + "fingerprint" + "/";

    http.begin(Link);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Token " + String(device_token));
    String postData = "{\"finger_id\": " + String(finger_id) + "}";
    int httpCode = http.POST(postData);
    Serial.println(httpCode);
    if (httpCode == 201) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, http.getString());
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return p;
      }
      finger_uuid = doc["uuid"].as<String>();
      Serial.println(finger_uuid);
      http.end();
      delay(1000);
    } else {
      http.end();
    }
  }

  return finger_id;
}

//*******************************************************
void setup() {
  pinMode(Red, OUTPUT);
  pinMode(Green, OUTPUT);
  digitalWrite(Red, HIGH);
  digitalWrite(Green, LOW);
  delay(1000);
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) {
      delay(1);
    }
  }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(1000);
  display.clearDisplay();

  display.setTextSize(1);       // Normal 1:1 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Wait for net"));
  display.display();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.setTextSize(2);       // Normal 2:2 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Connected"));
  display.display();
  delay(1000);
}

//**********************************************
uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available())
      ;
    num = Serial.parseInt();
  }
  return num;
}
//**********************************************
void loop() {
  int x = 0;

  // display fingerprint sensor data
  Serial.println("Send any character to enroll a finger print");
  display.clearDisplay();
  display.setTextSize(1);       // Normal 1:1 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Send char"));
  display.display();
  while (!Serial.available())
    ;
  while (Serial.available()) {
    Serial.read();
  }
  x = getFingerprintEnroll();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(2);       // Normal 2:2 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Success"));
  display.display();
  delay(1000);
}

//********************connect to the WiFi******************
void connectToWiFi() {
  WiFi.mode(WIFI_OFF);  // Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  display.clearDisplay();
  display.setTextSize(1);       // Normal 1:1 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Connecting to \n"));
  display.setCursor(0, 50);
  display.setTextSize(2);
  display.print(ssid);
  display.drawBitmap(73, 10, Wifi_start_bits, Wifi_start_width, Wifi_start_height, WHITE);
  display.display();

  uint32_t periodToConnect = 30000L;
  for (uint32_t StartToConnect = millis(); (millis() - StartToConnect) < periodToConnect;) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    } else {
      break;
    }
  }

  if (WiFi.isConnected()) {
    Serial.println("");
    Serial.println("Connected");

    display.clearDisplay();
    display.setTextSize(2);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(8, 0);      // Start at top-left corner
    display.print(F("Connected \n"));
    display.drawBitmap(33, 15, Wifi_connected_bits, Wifi_connected_width, Wifi_connected_height, WHITE);
    display.display();

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  // IP address assigned to your ESP
  } else {
    Serial.println("");
    Serial.println("Not Connected");
    WiFi.mode(WIFI_OFF);
    delay(1000);
  }
  delay(1000);
}
