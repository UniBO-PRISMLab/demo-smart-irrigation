/*
 * Soil Moisture Sensor Reader
 * 
 * Simple example: reads soil moisture sensor analog values and prints them every 1 second.
 * 
 * Pin: A0
 */

// Soil moisture sensor pins
const int SOIL_MOISTURE_PIN= A0;

// Timing
const unsigned long PRINT_INTERVAL_MS = 1000;
unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Soil Moisture Sensor Reader");
  Serial.println("Reading every 1 second...\n");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Print every 1 second
  if (currentTime - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = currentTime;
    
    int raw = analogRead(SOIL_MOISTURE_PIN);
    
    Serial.print("MOSITURE (A0): ");
    Serial.println(raw);
  }
}
