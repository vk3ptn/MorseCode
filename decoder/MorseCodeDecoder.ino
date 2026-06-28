// Description: 240
//  Morse code decoder using ESP32 micro controller module (TENSTAR with OLED display)
//  Notes on configuration settings:
//  Tools|Board|esp32 select LilyGo T-Display (or ESP32 DEV)
//  Pin 15: Input - LOW open, HIGH close
//  Pin 13: Clear screen - LOW
//  Pin 2: Buzzer 
//  Set baud rate 115200 to view serial.print() output the Serial Monitor (for monitoring/debugging purpose) 
//
// Author: Phuong Nguyen (VK3PTN)
// Version:
//  1.0.0 Initial version for ESP32 TenStar T ST7735 1.14 LED Display Module - 10/06/2026
//  1.0.1 Fine tuning timing boundaries using the standard international morse ratios - 20/06/2026
//  1.0.2 Added adjustable transmission speed (SLOW|MEDIUM|FAST) 22/06/2026
//  1.0.3 Calibrated speed timing against standard morse code generators
//
// TODO: Review and clean up codes

#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <string>

// constants
const int DIGITAL_INPUT_PIN = 15; 
const int CLEAR_BUTTON_PIN = 13; 
const int SPEED_BUTTON_PIN = 21;
const int BUZZER_PIN = 2;           
const int BUZZER_FREQ = 120;  

// timing parameters - no longer const so they can be adjusted dynamically
// speed calibrated against general morse code generator
const int SLOW_SPEED = 70; // 10-12wpm
const int MEDIUM_SPEED = 50; //12-15wpm
const int FAST_SPEED = 30; //15-20wpm

// selectable speed options instead of fixed option 
const int SPEED_CYCLES[] = {SLOW_SPEED, MEDIUM_SPEED, FAST_SPEED};
int currentSpeedIndex = 0; // 0 = SLOW, 1 = MEDIUM, 2 = FAST

// initial default speed
int DOT_UNIT = MEDIUM_SPEED;

// intervals timing based on international standard 
int DASH_LIMIT   = DOT_UNIT * 3;  
int LETTER_LIMIT = DOT_UNIT * 3; 
int WORD_LIMIT   = DOT_UNIT * 7;  

// display config - header on top and text area below header
const int MIN_X_TEXT_AREA = 0;
const int MAX_X_TEXT_AREA = 240;
const int MIN_Y_TEXT_AREA = 22;
const int MAX_CHAR_PER_LINE = 13;

// cursor tracking for true typewriters flow
int currentX = MIN_X_TEXT_AREA;
int currentY = MIN_Y_TEXT_AREA;

// indicator position and size
const int INDICATOR_X = 224;
const int INDICATOR_Y = 8;
const int INDICATOR_SIZE = 8;

TFT_eSPI tft = TFT_eSPI();             

// standard morse codes and texts mapping
const std::unordered_map<std::string, char> morseMap = {
    {".", 'E'}, {"-", 'T'}, {"..", 'I'}, {".-", 'A'}, {"-.", 'N'}, {"--", 'M'}, {"...", 'S'}, 
    {"..-", 'U'}, {".-.", 'R'}, {".--", 'W'}, {"-..", 'D'}, {"-.-", 'K'}, {"--.", 'G'},
    {"---", 'O'}, {"....", 'H'}, {"...-", 'V'}, {"..-.", 'F'}, {".-..", 'L'}, {".--.", 'P'}, 
    {".---", 'J'}, {"-...", 'B'}, {"-..-", 'X'}, {"-.-.", 'C'}, {"-.--", 'Y'}, {"--..", 'Z'}, {"--.-", 'Q'},
    {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'}, {".....", '5'},
    {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'}, {"-----", '0'}, 
    {"-..-.", '/'}, {".-.-.-", '.'}, {"--..--", ','},  {"..--..", '?'}
};

String currentSequence = "";
unsigned long stateChangeTime = 0;
bool lastToneState = false;
bool letterPending = false;
bool lastSpeedButtonState = HIGH; // keeping track of the speed adjust pin

char decodeMorse(const String& sequence) {
    auto it = morseMap.find(sequence.c_str());
    if (it != morseMap.end()) {
        return it->second; 
    }
    return '?'; 
}

// redraw only the header text area to update the selected options
void updateHeader() {
    // clear header line background up to the active indicator dot
    tft.fillRect(0, 0, INDICATOR_X - 10, 20, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("CW SPEED: ");
    
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // display speed in yellow text

    if (DOT_UNIT == SLOW_SPEED) {
        tft.print("SLOW");
    } else if (DOT_UNIT == MEDIUM_SPEED) {
        tft.print("MEDIUM");
    } else if (DOT_UNIT == FAST_SPEED) {
        tft.print("FAST");
    } else {
        // to cover all possible senarios but should never happen
        tft.print("UNKNOWN");
    }
}

// clear text display area
void clearScreenRoutine() {
    tft.fillRect(MIN_X_TEXT_AREA, MIN_Y_TEXT_AREA, 240, 115, TFT_BLACK);
    currentX = MIN_X_TEXT_AREA;
    currentY = MIN_Y_TEXT_AREA;
    // debug
    Serial.println("\n--- Screen Cleared ---");
}

// display texts 
void printToScreen(char newChar) {
    tft.setTextSize(3);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    
    const int CHAR_WIDTH = 18;
    const int CHAR_HEIGHT = 26;

    if (currentX + CHAR_WIDTH > 235) {
        currentX = MIN_X_TEXT_AREA;
        currentY += CHAR_HEIGHT;
    }

    if (currentY + CHAR_HEIGHT > 135) {
        clearScreenRoutine();
    }

    tft.setCursor(currentX, currentY);

    if (newChar == ' ') {
        tft.fillRect(currentX, currentY, CHAR_WIDTH, CHAR_HEIGHT, TFT_BLACK);
        currentX += CHAR_WIDTH; 
    } else {
        tft.fillRect(currentX, currentY, CHAR_WIDTH, CHAR_HEIGHT, TFT_BLACK);
        tft.print(newChar);
        currentX += CHAR_WIDTH; 
    }
}

// signal indicator - ON/OFF
void displayIndicator(int x, int y, int size, uint32_t color) {
    tft.fillCircle(x, y, size, color);  
}

void startBuzzerTone() {
    tone(BUZZER_PIN, BUZZER_FREQ);
}

void stopBuzzerTone() {
    noTone(BUZZER_PIN); 
}

void setup() {
    Serial.begin(115200); // serial baudrate - for debuging/monitoring at this stage
    // set control pins on HIGH by default   
    pinMode(DIGITAL_INPUT_PIN, INPUT_PULLUP);  
    pinMode(CLEAR_BUTTON_PIN, INPUT_PULLUP);  
    pinMode(SPEED_BUTTON_PIN, INPUT_PULLUP);

    // initialise display
    tft.init();
    tft.setRotation(1); // landscape mode 
    tft.fillScreen(TFT_BLACK);

    // initialise the header display default speed
    currentSpeedIndex = 1; // inital speed set to MEDIUM
    DOT_UNIT = SPEED_CYCLES[currentSpeedIndex];
    updateHeader();

    stateChangeTime = millis();
}

void loop() {
    // ---- SPEED CONTROL HANDLING ----
    bool currentSpeedButtonState = digitalRead(SPEED_BUTTON_PIN);
    
    // check for falling edge transition (button pushed down to LOW)
    if (currentSpeedButtonState == LOW && lastSpeedButtonState == HIGH) {
        delay(50); // Simple debounce delay
        
    // cycling index counter
    currentSpeedIndex = (currentSpeedIndex + 1) % 3;
    
    // assign the new speed in the cycle
    DOT_UNIT = SPEED_CYCLES[currentSpeedIndex];

        // recalculate intervals base on the speed setting
        DASH_LIMIT   = DOT_UNIT * 3;
        LETTER_LIMIT = DOT_UNIT * 3; // or 4
        WORD_LIMIT   = DOT_UNIT * 7;

        updateHeader();

        // debug        
        Serial.print("Speed updated. Mode Index: ");
        Serial.print(currentSpeedIndex);
        Serial.print(" | New DOT_UNIT: ");
        Serial.println(DOT_UNIT);
    }
    lastSpeedButtonState = currentSpeedButtonState;

    // ---- CLEAR SCREEN BUTTON HANDLING ----
    if (digitalRead(CLEAR_BUTTON_PIN) == LOW) {
        clearScreenRoutine();
        delay(200);  
    }

    // ---- MORSE SIGNAL ENGINE DECODER ----
    bool toneDetected = (digitalRead(DIGITAL_INPUT_PIN) == LOW);
    unsigned long currentTime = millis();
    unsigned long duration = currentTime - stateChangeTime;

    if (toneDetected) {
        displayIndicator(INDICATOR_X, INDICATOR_Y, INDICATOR_SIZE, TFT_RED);
        startBuzzerTone(); 
    } else {
        displayIndicator(INDICATOR_X, INDICATOR_Y, INDICATOR_SIZE, TFT_DARKGREY);
        stopBuzzerTone();
    }

    if (toneDetected != lastToneState) {
        if (lastToneState) {
            // debug
            Serial.print("[SIGNAL LOW] Pulse Duration: ");
            Serial.print(duration);
            Serial.print(" ms -> Interpreted as: ");
            
            if (duration >= DASH_LIMIT) {
                currentSequence += "-";
                Serial.println("DASH (-)");
            } else if (duration > (DOT_UNIT / 2)) { // adjusted noise margin check to scale with speed
                currentSequence += ".";
                Serial.println("DOT (.)");
            } else {
                Serial.println("NOISE GLITCH (Ignored)");
            }
            
            Serial.print("  -> Current Buffer Content: "); Serial.println(currentSequence);
            letterPending = true;
        } else {
            Serial.print("[SIGNAL HIGH] Gap Duration: ");
            Serial.print(duration);
            Serial.print(" ms -> ");
            
            if (duration >= WORD_LIMIT) {
                Serial.println("WORD GAP MET");
            } else if (duration >= LETTER_LIMIT) {
                Serial.println("LETTER GAP MET");
            } else {
                Serial.println("ELEMENT GAP");
            }
        }
        lastToneState = toneDetected;
        stateChangeTime = currentTime;
    } else {
        if (!toneDetected && letterPending) {
            if (duration >= WORD_LIMIT) {
                if (currentSequence.length() > 0) {
                    char c = decodeMorse(currentSequence);
                    if (c != '?') {
                        Serial.print(" >> DECODED WORD ELEMENT: "); Serial.println(c);
                        printToScreen(c);
                    }
                }
                Serial.print(" ");
                printToScreen(' '); 
                
                currentSequence = "";
                letterPending = false;
            } 
            else if (duration >= LETTER_LIMIT && currentSequence.length() > 0) {
                char c = decodeMorse(currentSequence);
                if (c != '?') {
                    Serial.print(" >> DECODED LETTER ELEMENT: "); Serial.println(c);
                    printToScreen(c);
                } else {
                    Serial.print(" >> UNKNOWN SEQUENCE DETECTED: "); Serial.println(currentSequence);
                }
                currentSequence = "";
                letterPending = false; 
            }
        }
    }
}