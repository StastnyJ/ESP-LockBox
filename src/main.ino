#include "./pinout.h"

#include "SSD1306Wire.h"
#include <MFRC522.h>
#include <SPI.h>

#define D if (1)
#define BUTTON_DEBOUNCE_TIME 50
#define EXPECTED_CODE 4926
#define ACCEPTED_CARDS 2
#define REQUIRED_ONES_IN_ROW 5

// const int ipaddress[4] = {103, 97, 67, 25};

static byte lockIcon[] = {
    0xC0,
    0x00,
    0x70,
    0x03,
    0x10,
    0x06,
    0x08,
    0x04,
    0x08,
    0x04,
    0x58,
    0x04,
    0xFC,
    0x0F,
    0x04,
    0x08,
    0xC4,
    0x08,
    0xC4,
    0x09,
    0xC4,
    0x08,
    0x04,
    0x08,
    0x2C,
    0x0C,
    0xF8,
    0x0F,
};

static byte unlockIcon[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x78,
    0x00,
    0x48,
    0x00,
    0x4C,
    0x00,
    0xE0,
    0x07,
    0xF0,
    0x07,
    0x70,
    0x06,
    0xE0,
    0x0F,
    0xE0,
    0x07,
    0x20,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00,
};

byte detectedCard[] = {0, 0, 0, 0};
byte acceptedTags[] = {129, 122, 182, 72,
                       99, 22, 192, 27};

byte buttonsPins[] = {PLUS_BUTTON, NEXT_BUTTON, RESET_BUTTON, TOGGLE_DISPLAY_BUTTON};
byte lastButtonStates[] = {LOW, LOW, LOW, LOW};
unsigned long int lastButtonChangeTimes[] = {0, 0, 0, 0};
byte buttonPressedThisIteration[] = {0, 0, 0, 0};
byte buttonOnesInRow[] = {0, 0, 0, 0};

int displayOn = 1;

int pin = 0;
int currentDigit = 3;
int unlocked = LOW;

MFRC522::MIFARE_Key key;
MFRC522 rfid = MFRC522(SS_PIN, RST_PIN);

SSD1306Wire display(0x3c, I2C_SDA, I2C_SCL);

char str[5];
int offsets[] = {78, 66, 54, 42};

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(buttonsPins[i], INPUT);
  }

  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, LOW);

  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_24);

  D Serial.println(F("Init complete"));
}

void loop() {
  digitalWrite(LOCK_PIN, unlocked);
  detectButtonsPressed();
  readRFID();
  if (buttonPressedThisIteration[0]) plusPressed();
  if (buttonPressedThisIteration[1]) nextPressed();
  if (buttonPressedThisIteration[2]) resetPressed();
  if (buttonPressedThisIteration[3]) toggleDisplayPressed();
  if (pin == EXPECTED_CODE && isRFIDValid())
    unlock();
  redrawDisplay();
}

int pow10(int pow) {
  int res = 1;
  for (int i = 0; i < pow; i++)
    res *= 10;
  return res;
}

void plusPressed() {
  int val = (pin / pow10(currentDigit)) % 10;
  int difference = val == 9 ? -9 * pow10(currentDigit) : pow10(currentDigit);
  pin += difference;
  D Serial.printf("PIN: %04d (selected digit: %d)\n", pin, currentDigit);
}

void nextPressed() {
  currentDigit = (currentDigit + 3) % 4;
}

void resetPressed() {
  unlocked = 0;
  pin = 0;
  currentDigit = 3;
}

void toggleDisplayPressed() {
  displayOn = (displayOn + 1) % 2;
  if (!displayOn) {
    display.clear();
    display.display();
  }
}

void unlock() {
  unlocked = 1;
}

void detectButtonsPressed() {
  for (int i = 0; i < 4; i++) {
    buttonPressedThisIteration[i] = 0;
    int val = digitalRead(buttonsPins[i]);
    if (millis() + BUTTON_DEBOUNCE_TIME > lastButtonChangeTimes[i]) {
      if (val == HIGH && lastButtonStates[i] == 0) {
        buttonOnesInRow[i]++;
        if (buttonOnesInRow[i] >= REQUIRED_ONES_IN_ROW) {
          buttonOnesInRow[i] = 0;
          lastButtonStates[i] = 1;
          lastButtonChangeTimes[i] = millis();
          buttonPressedThisIteration[i] = 1;
        }
      }
      if (val == LOW) {
        buttonOnesInRow[i] = 0;
        if (lastButtonStates[i] == 1) {
          lastButtonStates[i] = 0;
          lastButtonChangeTimes[i] = millis();
        }
      }
    }
  }
}

int isRFIDValid() {
  if (detectedCard[0] == 0)
    return 0;
  for (int card = 0; card < ACCEPTED_CARDS; card++) {
    int ok = 1;
    for (int i = 0; i < 4; i++) {
      if (detectedCard[i] != acceptedTags[card * 4 + i]) {
        ok = 0;
        break;
      }
    }
    if (ok)
      return 1;
  }
  return 0;
}

void readRFID(void) {
  for (byte i = 0; i < 4; i++) {
    detectedCard[i] = 0;
  }
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;
  for (byte i = 0; i < 4; i++) {
    detectedCard[i] = rfid.uid.uidByte[i];
  }

  D Serial.print(F("RFID In dec: "));
  D printDec(rfid.uid.uidByte, rfid.uid.size);
  D Serial.println();

  rfid.PCD_Init();
}

void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

void redrawDisplay() {
  if (!displayOn)
    return;
  display.clear();
  sprintf(str, "%04d", pin);
  display.drawXbm(0, 0, 14, 14, unlocked ? unlockIcon : lockIcon);
  display.drawString(37, 20, str);
  display.drawString(offsets[currentDigit], 32, ".");
  display.display();
}