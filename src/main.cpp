#include <Arduino.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <MFRC522.h>
#include <DFRobotDFPlayerMini.h>
#include <Bounce2.h>

//Global variables :/
String currentUID = "";
unsigned long idleMillis = 0;
const unsigned long idleDelay = 60000;

// MFRC522
#define SS_PIN 9
#define RST_PIN 8
MFRC522 mfrc522(SS_PIN, RST_PIN);
String ReadRFIDCard();

// DFPlayer
SoftwareSerial dfSerial(5, 6); // RX,TX
DFRobotDFPlayerMini mp3;

// Buttons
#define PLAY_BUTTON 2
#define STANDBY_BUTTON 3
Bounce playDebouncer = Bounce();
Bounce standbyDebouncer = Bounce();

// Standby Timing
unsigned long trackFinishedMillis = 0;
const unsigned long standbyDelay = 30000; // 30sec

// State Machine
enum BoxState
{
  PLAYING,
  PAUSED,
  STANDBY,
  IDLE,
  TRACK_FINISHED_WAIT
};
BoxState state = IDLE;

//Forward declarations
void handleButtons();
void handleRFID();
void handleTrackFinished();
void enterStandby();
void enterLowPowerSleep();
void wakeup();
String getUIDString();

// Helper Functions
String getUIDString()
{
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    if (mfrc522.uid.uidByte[i] < 0x10)
      uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  return uid;
}

uint8_t uidToFolder()
{ // UID to folder mapping
  uint32_t uidInt = 0;
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    uidInt = uidInt * 256 + mfrc522.uid.uidByte[i];
  }
  return (uidInt % 100) + 1; // folder 1-100
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  // Buttons
  pinMode(PLAY_BUTTON, INPUT_PULLUP);
  pinMode(STANDBY_BUTTON, INPUT_PULLUP);
  playDebouncer.attach(PLAY_BUTTON);
  playDebouncer.interval(25);
  standbyDebouncer.attach(STANDBY_BUTTON);
  standbyDebouncer.interval(25);

  // DFPlayer
  dfSerial.begin(9600);
  if (!mp3.begin(dfSerial))
  {
    Serial.println("DFPlayer failed init!");
    while (true)
      ;
  }
  mp3.volume(20);
}

void loop()
{
  playDebouncer.update();
  standbyDebouncer.update();
  handleButtons();
  handleRFID();
  handleTrackFinished();

   if (state == IDLE && (millis() - idleMillis >= idleDelay)) {
      enterStandby();
  }

  if (state != IDLE) {
      idleMillis = millis(); // reset timer when activity happens
  }
}

void handleButtons()
{
  if (playDebouncer.fell())
  {
    if (state == PLAYING)
    {
      mp3.pause();
      state = PAUSED;
    }
    else if (state == PAUSED)
    {
      mp3.start();
      state = PLAYING;
    }
  }
  if (standbyDebouncer.fell())
  {
    enterStandby();
  }
}

  void handleRFID()
{
  if (!mfrc522.PICC_IsNewCardPresent())
    return;
  if (!mfrc522.PICC_ReadCardSerial())
    return;

  String newUID = getUIDString();

  // If a different card is detected
  if (newUID != currentUID)
  {
    currentUID = newUID;

    uint8_t folder = uidToFolder();
    Serial.print("New card, folder: ");
    Serial.println(folder);

    mp3.stop();                 // stop current audio
    mp3.playFolder(folder, 1);  // auto-play new card
    state = PLAYING;
  }

  mfrc522.PICC_HaltA();
}

void handleTrackFinished()
{
  if (state == PLAYING && mp3.available())
  {
    if (mp3.readType() == DFPlayerPlayFinished)
    {
      trackFinishedMillis = millis();
      state = TRACK_FINISHED_WAIT;
    }
  }
  if (state == TRACK_FINISHED_WAIT)
  {
    if (millis() - trackFinishedMillis >= standbyDelay)
    {
      enterStandby();
    }
  }
}


void enterStandby()
{
  if (state == STANDBY)
    return; // Already in standby, ignore

  Serial.println("Entering standby...");
  trackFinishedMillis = millis(); //reset idle timer

  // Stop audiobook and prepare for sleep
  mp3.stop();
  state = STANDBY;

  // Play sleep sound (folder 2, track 1)
  mp3.playFolder(2, 1);
  unsigned long start = millis();
      while (true)
      {
          if (mp3.available())
          {
              if (mp3.readType() == DFPlayerPlayFinished)
                  break;
          }
          // timeout just in case
          if (millis() - start > 2000) // 5 sec max
              break;
      }
  enterLowPowerSleep();

  // After waking
  state = IDLE;  // ready for next action

  // Play wake sound (folder 1, track 1)
  mp3.playFolder(1, 1);
  delay(50); // let DFPlayer latch
}

void enterLowPowerSleep()
{
  // Wait until button released to avoid retrigger
  while (digitalRead(STANDBY_BUTTON) == LOW);
  delay(20);

  // Prepare MCU for sleep
  ADCSRA &= ~(1 << ADEN);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(digitalPinToInterrupt(STANDBY_BUTTON), wakeup, LOW);

  // Enter sleep
  sleep_cpu();

  // MCU wakes here
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(STANDBY_BUTTON));
  ADCSRA |= (1 << ADEN);

  //reset auto-timers
  trackFinishedMillis = millis();
  idleMillis = millis();

  // Wait until button released after waking
  while (digitalRead(STANDBY_BUTTON) == LOW);
  delay(20);

  // Update debouncers
  playDebouncer.update();
  standbyDebouncer.update();
}

void wakeup()
{
}

