//  Description:
//    Using ESP32 Dev Module to transmit Morse code signal
//    Input: Morse code signal with audio tone
//    Oputput: RF signal 7.272MHz, Audio tone 600Hz, Led indicator
//  Author: Phuong Nguyen (VK3PTN)
//  Version:
//    1.0.0 Initial verion                                                        26/06/2026
//    1.0.1 Reduce the noise level threshold to detect weak audio signal

#include <Arduino.h>

// Pin Mappings
const int ANALOG_KEY_PIN = 34;  // Read 600Hz audio from circuit here
const int RF_TX_PIN      = 25;  // Square wave output
const int AUDIO_PIN      = 26;  // Audio tone for the buzzer

#ifdef LED_BUILTIN
  const int LED_PIN = LED_BUILTIN; 
#else
  const int LED_PIN = 2; 
#endif

// RF configuration (7.272 MHz)
const uint32_t RF_FREQ   = 7272727;  
const uint8_t LEDC_RES   = 1;         // 1-bit resolution (0 or 1)
const uint32_t RF_ON     = 1;         // 50% duty cycle for 1-bit
const uint32_t RF_OFF    = 0;         // Off

// Audio tone configuration (600Hz)
const uint32_t AUDIO_FREQ = 600;    
const uint8_t AUDIO_RES   = 8;        // 8-bit resolution (0 to 255)
const uint32_t AUDIO_ON   = 128;      // 50% duty cycle for 8-bit
const uint32_t AUDIO_OFF  = 0;        // Off

// Auto-calibration variables
int noiseFloor = 4095;
int peakSignal = 0;
int voltageThreshold = 500; // Safe initial fallback
bool keyIsPressed = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);         
  digitalWrite(LED_PIN, LOW);

  // Initialize PWM outputs ONCE during setup to prevent timer lockups
  ledcAttach(RF_TX_PIN, RF_FREQ, LEDC_RES);
  ledcWrite(RF_TX_PIN, RF_OFF);

  ledcAttach(AUDIO_PIN, AUDIO_FREQ, AUDIO_RES);
  ledcWrite(AUDIO_PIN, AUDIO_OFF);

  Serial.println("--- Morse Audio Decoder Initialized ---");
  Serial.println("Play a continuous string of Morse code to auto-calibrate thresholds.");
}

void loop() {
  int currentAnalogValue = analogRead(ANALOG_KEY_PIN);

  // --- AUTO CALIBRATION LOGIC ---
  // Track the lowest and highest values ever seen to adapt to your laptop volume
  if (currentAnalogValue < noiseFloor && currentAnalogValue > 10) {
    noiseFloor = currentAnalogValue;
  }
  if (currentAnalogValue > peakSignal) {
    peakSignal = currentAnalogValue;
  }
  
  // Dynamically calculate the middle threshold point
  //if (peakSignal > noiseFloor + 150) { 
  //  voltageThreshold = noiseFloor + ((peakSignal - noiseFloor) / 2);
  //}

  if (peakSignal > noiseFloor + 30) { 
    // Sets the threshold to just 25% above the noise floor instead of 50%
    voltageThreshold = noiseFloor + ((peakSignal - noiseFloor) / 4); 
  }

  // Determine signal state
  bool toneDetected = (currentAnalogValue > voltageThreshold);

  // Debug Print Window (Open your Arduino Serial Plotter to see this visually)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 100) {
    lastPrint = millis();
    Serial.print("Signal:"); Serial.print(currentAnalogValue);
    Serial.print(",");
    Serial.print("Threshold:"); Serial.println(voltageThreshold);
  }

  // --- TRIGGER OUTPUTS ---
  if (toneDetected != keyIsPressed) {
    keyIsPressed = toneDetected; 

    if (keyIsPressed) {
      // KEY DOWN: Activate by turning duty cycles UP
      ledcWrite(RF_TX_PIN, RF_ON);         
      ledcWrite(AUDIO_PIN, AUDIO_ON);
      digitalWrite(LED_PIN, HIGH);              
    } 
    else {
      // KEY UP: Deactivate by setting duty cycles to 0
      ledcWrite(RF_TX_PIN, RF_OFF);               
      ledcWrite(AUDIO_PIN, AUDIO_OFF);
      digitalWrite(LED_PIN, LOW);               
    }
  }
}