//  Description:
//    Using ESP32 Dev Module to transmit Morse code signal
//    Input: Morse key OPEN|HIGH CLOSE|LOW
//    Oputput: RF signal 7.272MHz, Audio tone 600Hz, Led indicator
//  Author: Phuong Nguyen (VK3PTN)
//  Version:
//    1.0.0 Initial verion                                                        18/06/2026
//    1.0.1 Applied hardware interrupted Routines (ISR) to reduce latency time    20/06/2026

#include <Arduino.h>

// pin Mappings - all clustered on the same physical side of the ESP32
const int KEY_PIN    = 14;  // morse key input HIGH:OFF LOW:ON
const int RF_TX_PIN  = 25;  // square wave output
const int AUDIO_PIN  = 26;  // audio tone for the buzzer

// signal indicator
#ifdef LED_BUILTIN
  const int LED_PIN = LED_BUILTIN; 
#else
  const int LED_PIN = 2; 
#endif

// RF configuration (10 MHz)
// Configuration (7.272 MHz for 40-Meter Band)
const uint32_t RF_FREQ = 7272727;  // 80 MHz / 11 = 7.272727 MHz
const uint8_t LEDC_RES = 1;         // 1-bit resolution
const uint32_t DUTY_VALUE = 1;      // 50% duty cycle

// audio tone configuration
const uint32_t AUDIO_FREQ = 600;    // 600Hz audio tone
const uint8_t AUDIO_RES   = 8;      // 8-bit resolution
const uint32_t AUDIO_DUTY  = 128;    // 50% duty cycle (256 / 2)

// Use 'volatile' for fast hardware execution inside interrupts
volatile bool keyIsPressed = false;
volatile bool stateChanged = false;

// HARDWARE INTERRUPT SERVICE ROUTINE (ISR)
void IRAM_ATTR handleKeyInterrupt() {
  bool currentState = !(gpio_get_level((gpio_num_t)KEY_PIN));
  
  if (currentState != keyIsPressed) {
    keyIsPressed = currentState;
    stateChanged = true;
  }
}

void setup() {
  // initial pin settings
  pinMode(KEY_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);         
  digitalWrite(LED_PIN, LOW);
  pinMode(RF_TX_PIN, OUTPUT);
  digitalWrite(RF_TX_PIN, LOW);
  pinMode(AUDIO_PIN, OUTPUT);
  digitalWrite(AUDIO_PIN, LOW);

  // attach ultra-low latency interrupt
  attachInterrupt(digitalPinToInterrupt(KEY_PIN), handleKeyInterrupt, CHANGE);
}

void loop() {
  if (stateChanged) {
    stateChanged = false; 

    if (keyIsPressed) {
      // --- INSTANT KEY DOWN ---
      ledcAttach(RF_TX_PIN, RF_FREQ, LEDC_RES); 
      ledcWrite(RF_TX_PIN, DUTY_VALUE);         
      
      ledcAttach(AUDIO_PIN, AUDIO_FREQ, AUDIO_RES);
      ledcWrite(AUDIO_PIN, AUDIO_DUTY);
      
      digitalWrite(LED_PIN, HIGH);              
    } 
    else {
      // --- INSTANT KEY UP ---
      ledcDetach(RF_TX_PIN);                    
      pinMode(RF_TX_PIN, OUTPUT);               
      digitalWrite(RF_TX_PIN, LOW);             
      
      ledcDetach(AUDIO_PIN);
      pinMode(AUDIO_PIN, OUTPUT);
      digitalWrite(AUDIO_PIN, LOW);
      
      digitalWrite(LED_PIN, LOW);               
    }
  }
}