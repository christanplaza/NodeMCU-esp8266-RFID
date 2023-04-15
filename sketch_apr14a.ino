/* Read RFID Tag with RC522 RFID Reader
 *  Made by miliohm.com
 */
 
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

const char* ssid = "Impyerno Free Wifi"; // Replace with WIFI SSID
const char* password = "asdf1234";


const char* serverName = "https://endorm-server.onrender.com/";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

constexpr uint8_t RST_PIN = D3;     // Configurable, see typical pin layout above
constexpr uint8_t SS_PIN = D4;     // Configurable, see typical pin layout above

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

String tag;

void setup() {
  Serial.begin(9600);
  // Connect to Wifi
  Serial.println("Connecting to Wifi");
  while(Wifi.status() != W_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to Wifi network with IP Address: ")
  Serial.print(Wifi.localIP());
  
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
}

void loop() {
  if (Wifi.status() == WL_CONNECTED) {
    if ( ! rfid.PICC_IsNewCardPresent())
        return;
    if (rfid.PICC_ReadCardSerial()) {
      for (byte i = 0; i < 4; i++) {
        tag += rfid.uid.uidByte[i];
      }
      Serial.println(tag);
      // tag = "";
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  } else {
    // Wifi is not connected
    return;
  }
}