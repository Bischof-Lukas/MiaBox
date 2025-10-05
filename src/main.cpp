#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 53
#define RST_PIN 49

MFRC522 rfid(SS_PIN, RST_PIN);
String ReadRFIDCard();

// Map of known UIDs to track numbers, will be a file later


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID reader ready");
}

void loop() {
  // put your main code here, to run repeatedly:
 ReadRFIDCard();
}

String ReadRFIDCard(){
  String uid = "";
  if(!rfid.PICC_IsNewCardPresent()) return "";
  if(!rfid.PICC_ReadCardSerial()) return "";

  Serial.print("Card UID: ");
    for(byte i = 0; i < rfid.uid.size; i++){
      if(i==0) Serial.print(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      else Serial.print(rfid.uid.uidByte[i] < 0x10 ? ":0" : ":");
      Serial.print(rfid.uid.uidByte[i], HEX);
      String 
    }
    Serial.println();
  rfid.PICC_HaltA();
  return uid;
}