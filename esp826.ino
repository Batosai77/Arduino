//*******************************libraries********************************
// NodeMCU--------------------------
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <ESP8266HTTPClient.h>
#include <SimpleTimer.h>  //https://github.com/jfturcot/SimpleTimer
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
#define Finger_Rx 14  // D5
#define Finger_Tx 12  // D6
// Declaration for SSD1306 display connected using software I2C pins are(22 SCL, 21 SDA)
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET 0      // Reset pin # (or -1 if sharing Arduino reset pin)
//************************************************************************
WiFiClient client;
SimpleTimer timer;
SoftwareSerial mySerial(Finger_Rx, Finger_Tx);
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
  finger.begin(57600);
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

    http.begin(client, Link);

    int httpCode = http.DELETE();
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
    //Serial.println("Deleted!");
    display.clearDisplay();
    display.setTextSize(2);       // Normal 2:2 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Deleted!\n"));
    display.display();
    SendDeleteFingerID();
    return p;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    //Serial.println("Communication error");
    display.clearDisplay();
    display.setTextSize(1);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Communication error!\n"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    //Serial.println("Could not delete in that location");
    display.clearDisplay();
    display.setTextSize(1);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Could not delete in that location!\n"));
    display.display();
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    //Serial.println("Error writing to flash");
    display.clearDisplay();
    display.setTextSize(1);       // Normal 1:1 pixel scale
    display.setTextColor(WHITE);  // Draw white text
    display.setCursor(0, 0);      // Start at top-left corner
    display.print(F("Error writing to flash!\n"));
    display.display();
    return p;
  } else {
    //Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
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

    Link = URL + "add-record" + "/" + "fingerprint";
    http.begin(client, Link);

    StaticJsonDocument<200> doc;
    doc["finger_id"] = finger_id;

    String body;
    serializeJson(doc, body);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("token", device_token);

    int httpCode = http.POST(body);
    Serial.println(httpCode);

    if (httpCode == 200) {
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(15, 0);     // Start at top-left corner
      display.print(F("Welcome"));
      display.setCursor(0, 20);
      // display.print(user_name);
      display.display();
    } else if (httpCode == 403) {
      String payload = http.getString();
      StaticJsonDocument<200> responseDoc;
      deserializeJson(responseDoc, payload);

      const char *code = responseDoc["code"];
      const char *expiredAt = responseDoc["expired_at"];

      display.clearDisplay();
      display.setTextSize(1);       // Smaller text size for more info
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.print(F("Code: "));
      display.print(code);
      display.setCursor(0, 10);
      display.print(F("Expired At: "));
      display.print(expiredAt);
      display.display();
    } else {
      display.clearDisplay();
      display.setTextSize(2);       // Normal 2:2 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(15, 0);     // Start at top-left corner
      display.print(F("Gak Boleh"));
      display.setCursor(0, 20);
      display.display();
    }
    delay(10);
    http.end();
  }
}
//********************Get Finger ID******************
int getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      // Serial.println("No finger detected");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Communication error");
      return -2;
    case FINGERPRINT_IMAGEFAIL:
      // Serial.println("Imaging error");
      return -2;
    default:
      // Serial.println("Unknown error");
      return -2;
  }
  // OK success!
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      // Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Serial.println("Communication error");
      return -2;
    case FINGERPRINT_FEATUREFAIL:
      // Serial.println("Could not find fingerprint features");
      return -2;
    case FINGERPRINT_INVALIDIMAGE:
      // Serial.println("Could not find fingerprint features");
      return -2;
    default:
      // Serial.println("Unknown error");
      return -2;
  }
  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    // Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Serial.println("Communication error");
    return -2;
  } else if (p == FINGERPRINT_NOTFOUND) {
    // Serial.println("Did not find a match");
    return -1;
  } else {
    // Serial.println("Unknown error");
    return -2;
  }

  // found a match!
  display.display();
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  return finger.fingerID;
}

//********************Confirm Adding******************

void confirmAdding() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    Link = URL + "attendance" + "/" + String(finger_uuid);
    http.begin(client, Link);

    StaticJsonDocument<200> doc;
    doc["type"] = "attendance";
    doc["_method"] = "PUT";

    String body;
    serializeJson(doc, body);

    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(body);
    Serial.println(httpCode);
    Serial.println(Link);
    if (httpCode == 200) {
      display.clearDisplay();
      display.setTextSize(1.5);     // Normal 1:1 pixel scale
      display.setTextColor(WHITE);  // Draw white text
      display.setCursor(0, 0);      // Start at top-left corner
      display.display();
      timer.disable(t1);
      delay(2000);
    } else {
      Serial.println("Error Confirm Adding!");
    }
    http.end();
  }
}
//********************Get Finger Enroll******************

uint8_t getFingerprintEnroll() {
  int p = -1;
  display.clearDisplay();
  display.drawBitmap(34, 0, FinPr_scan_bits, FinPr_scan_width, FinPr_scan_height, WHITE);
  display.display();
  while (p != FINGERPRINT_OK) {

    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        // Serial.println("Image taken");
        display.clearDisplay();
        display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
        display.display();
      case FINGERPRINT_NOFINGER:
        // Serial.println(".");
        display.setTextSize(1);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("scanning"));
        display.display();
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        display.clearDisplay();
        display.drawBitmap(34, 0, FinPr_invalid_bits, FinPr_invalid_width, FinPr_invalid_height, WHITE);
        display.display();
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      display.clearDisplay();
      display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
      display.display();
      break;
    case FINGERPRINT_IMAGEMESS:
      display.clearDisplay();
      display.drawBitmap(34, 0, FinPr_invalid_bits, FinPr_invalid_width, FinPr_invalid_height, WHITE);
      display.display();
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  display.clearDisplay();
  display.setTextSize(2);       // Normal 2:2 pixel scale
  display.setTextColor(WHITE);  // Draw white text
  display.setCursor(0, 0);      // Start at top-left corner
  display.print(F("Remove"));
  display.setCursor(0, 20);
  display.print(F("finger"));
  display.display();
  // Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(finger_id);
  p = -1;
  display.clearDisplay();
  display.drawBitmap(34, 0, FinPr_scan_bits, FinPr_scan_width, FinPr_scan_height, WHITE);
  display.display();
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        // Serial.println("Image taken");
        display.clearDisplay();
        display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
        display.display();
        break;
      case FINGERPRINT_NOFINGER:
        // Serial.println(".");
        display.setTextSize(1);       // Normal 2:2 pixel scale
        display.setTextColor(WHITE);  // Draw white text
        display.setCursor(0, 0);      // Start at top-left corner
        display.print(F("scanning"));
        display.display();
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      // Serial.println("Image converted");
      display.clearDisplay();
      display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
      display.display();
      break;
    case FINGERPRINT_IMAGEMESS:
      // Serial.println("Image too messy");
      display.clearDisplay();
      display.drawBitmap(34, 0, FinPr_invalid_bits, FinPr_invalid_width, FinPr_invalid_height, WHITE);
      display.display();
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(finger_id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
    display.clearDisplay();
    display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
    display.display();
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    display.clearDisplay();
    display.drawBitmap(34, 0, FinPr_invalid_bits, FinPr_invalid_width, FinPr_invalid_height, WHITE);
    display.display();
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(finger_id);
  p = finger.storeModel(finger_id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    display.clearDisplay();
    display.drawBitmap(34, 0, FinPr_valid_bits, FinPr_valid_width, FinPr_valid_height, WHITE);
    display.display();
    confirmAdding();
    return finger_id;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }
}


//********************Check Mode******************

void CheckMode() {
  Serial.println("Check Mode");
  if (WiFi.isConnected()) {
    HTTPClient http;  // Declare object of class HTTPClient
    // GET
    Link = URL + "get-mode" + "/attendance";
    http.begin(client, Link);   // initiate HTTP request,
    int httpCode = http.GET();  // Send the request

    if (httpCode == 200) {
      String payload = http.getString();  // Get the response payload
      Serial.println(payload);
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.println("deserializeJson() Failed");
        // Serial.println(error);
        http.end();
        return;
      }

      const char *mode = doc["mode"];
      finger_uuid = String((const char *)doc["finger_uuid"]);  // Ensure correct assignment
      finger_id = doc["finger_id"];                            // Ensure correct assignment
      if (strcmp(mode, "register") == 0) {
        timer.enable(t1);
        timer.disable(t2);
        Serial.println(finger_uuid);
        Serial.println("Deivce Mode: Enrollment");
        // getFingerprintEnroll();
      } else if (strcmp(mode, "delete") == 0) {
        timer.disable(t1);
        timer.enable(t2);
        Serial.println("Deivce Mode: Delete Finger ID");
      } else {
        timer.disable(t1);
        timer.disable(t2);
        Serial.println("Deivce Mode: Attandance");
      }
    } else {
      Serial.println("Error Http Request");
      http.end();
    }
    http.end();  // Close connection
  }
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
