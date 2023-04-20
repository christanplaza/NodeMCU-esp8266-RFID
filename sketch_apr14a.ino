#include <EEPROM.h>
#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFiClientSecureBearSSL.h>



String serverName = "http://192.168.54.60:8000";

const char* ssid = "cosmo"; // Replace with WIFI SSID
const char* password = "asdf1234"; // Replace with WIFI Password

const String masterKey = "998138152"; // Replace with UID of masterKey
const String roomNumber = "RM001"; // Replace with room number

// const char* ssid = "ZTE_2.4G_LZ5S3v"; // Replace with WIFI SSID
// const char* password = "Apy5NAC4"; // Replace with WIFI Password

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

constexpr uint8_t RST_PIN = D3;     // Configurable, see typical pin layout above
constexpr uint8_t SS_PIN = D4;     // Configurable, see typical pin layout above
constexpr uint8_t PIC_PIN1 = D1;
constexpr uint8_t PIC_PIN2 = D2;
constexpr uint8_t RED_LED = D8;

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

String tag;
HTTPClient http;
WiFiClient client;


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  pinMode(RED_LED, OUTPUT);
  pinMode(PIC_PIN1, OUTPUT);
  pinMode(PIC_PIN2, OUTPUT);

  // Connect to Wifi
  Serial.println("Connecting to Wifi");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("There was an error with the network");
    digitalWrite(RED_LED, HIGH);
  } else {
    Serial.println("");
    Serial.print("Connected to Wifi network with IP Address: ");
    digitalWrite(RED_LED, LOW);
    Serial.println(WiFi.localIP());
  }
  Serial.println("You may Scan your KeyCard");
  
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  EEPROM.begin(512);

  
  getLatestData();
}

void loop() {
  if ( !rfid.PICC_IsNewCardPresent())
      return;
  if (rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      tag += rfid.uid.uidByte[i];
    }
    Serial.println(tag);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    
    if (tag == masterKey) {
      sendDoorSignal(true);
    } else {
      if (WiFi.status() == WL_CONNECTED) {
        String url = serverName + "/door/" + roomNumber + "/" + tag;
        http.begin(client, url);
        http.addHeader("Accept", "*/*");
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST("");
        String response = http.getString();

        StaticJsonDocument<200> responseDoc;
        deserializeJson(responseDoc, response);

        if (responseDoc.containsKey("error")) {
          String errorMessage = responseDoc["error"].as<String>();
          Serial.println("Error: " + errorMessage);
          sendDoorSignal(false);
        } else {
          Serial.println("Response: " + response);
          String message = responseDoc["message"].as<String>();
          if (message == "ok") {
            sendDoorSignal(true);
          }
        }
      } else {
        bool isRecorded = verifyRFIDinEEPROM(tag);
        if (isRecorded) {
          sendDoorSignal(true);
        }
        tag = "";
        return;
      }
    }
    tag = "";
  }
}

void sendDoorSignal(bool status) {
  if (status) {
    digitalWrite(PIC_PIN1, LOW);  // Send a 1 to the PIC18F4550
    digitalWrite(PIC_PIN2, HIGH);  // Send a 1 to the PIC18F4550
    delay(2000);
    digitalWrite(PIC_PIN1, LOW);  // Send a 1 to the PIC18F4550
    digitalWrite(PIC_PIN2, LOW);  // Send a 1 to the PIC18F4550
    delay(2000);
  } else {
    digitalWrite(PIC_PIN1, HIGH);  // Send a 1 to the PIC18F4550
    digitalWrite(PIC_PIN2, LOW);  // Send a 1 to the PIC18F4550
    delay(2000);
    digitalWrite(PIC_PIN1, LOW);  // Send a 1 to the PIC18F4550
    digitalWrite(PIC_PIN2, LOW);  // Send a 1 to the PIC18F4550
  }
}

std::vector<String> extractRFID(const String& jsonString, const String& roomNumber) {
  const size_t capacity = JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1) + 100;
  DynamicJsonDocument doc(capacity);

  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return std::vector<String>(); // Return an empty vector if deserialization fails
  }

  std::vector<String> rfids; // Create a vector to store the RFIDs
  JsonArray json_list = doc["json_list"];
  for (JsonObject obj : json_list) {
    if (obj["room_ID"].as<String>() == roomNumber) {
      rfids.push_back(obj["rfid"].as<String>()); // Add the matching RFID to the vector
    }
  }

  return rfids; // Return the vector of RFIDs
}

void saveRFIDtoEEPROM(const std::vector<String>& rfids) {
  int address = 0; // Choose an EEPROM address to start saving the RFID strings

  for (String rfid : rfids) {
    // Write the length of the RFID string to the EEPROM
    byte rfidLength = rfid.length();
    EEPROM.write(address, rfidLength);

    // Write the RFID string to the EEPROM
    for (int i = 0; i < rfidLength; i++) {
      EEPROM.write(address + 1 + i, rfid[i]);
    }

    // Increment the EEPROM address to save the next RFID string
    address += rfidLength + 1;
  }

  // Commit the changes to the EEPROM
  EEPROM.commit();
}

bool verifyRFIDinEEPROM(const String& rfid) {
  int address = 0; // Choose the same EEPROM address used in the saveRFIDtoEEPROM function

  while (address < EEPROM.length()) {
    // Read the length of the RFID string from the EEPROM
    byte rfidLength = EEPROM.read(address);

    // Read the RFID string from the EEPROM
    String storedRFID = "";
    for (int i = 0; i < rfidLength; i++) {
      storedRFID += char(EEPROM.read(address + 1 + i));
    }

    if (storedRFID == rfid) {
      return true; // Return true if the RFID matches
    }

    // Increment the EEPROM address to the next RFID string
    address += rfidLength + 1;
  }

  return false; // Return false if the RFID is not found
}

void getLatestData() {
  if (WiFi.status() == WL_CONNECTED) {
    String url = serverName + "/json_list";
    Serial.print("Sending GET request to: ");
    Serial.println(url);
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      std::vector<String> rfids = extractRFID(payload, roomNumber);

      for (String rfid : rfids) {
        Serial.println("RFID: " + rfid);
      }
      saveRFIDtoEEPROM(rfids);

    } else {
      Serial.print("HTTP GET failed, error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("Not connected to Wi-Fi.");
  }
}