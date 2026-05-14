/*
 * LDR Light Sensor Reader with Category
 * 
 * Simple example: reads LDR sensor and categorizes light level (DARK / MID-LIGHT / LIGHT).
 * Prints every 1 second.
 * 
 * Uses the same thresholds as the main smart_irrigigation project.
 * Pin: A2 (LDR)
 */

// LDR pin
const int LDR_PIN = A2;

// Light thresholds (same as main project)
const int LDR_THRESHOLD_DARK = 200;
const int LDR_THRESHOLD_MID = 500;
const int LDR_THRESHOLD_LIGHT = 800;

// Timing
const unsigned long PRINT_INTERVAL_MS = 1000;
unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Lettore sensore luce LDR");
  Serial.println("Lettura ogni secondo...\n");
  Serial.println("Soglie:");
  Serial.print("  BUIO: < ");
  Serial.println(LDR_THRESHOLD_DARK);
  Serial.print("  LUCE-MEDIA: < ");
  Serial.println(LDR_THRESHOLD_MID);
  Serial.print("  LUCE: >= ");
  Serial.println(LDR_THRESHOLD_MID);
  Serial.println();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Read every 1 second
  if (currentTime - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = currentTime;
    
    int ldr_raw = analogRead(LDR_PIN);
    const char* category = getLDRCategory(ldr_raw);
    
    Serial.print("Valore LDR: ");
    Serial.print(ldr_raw);
    Serial.print(" | Categoria: ");
    Serial.println(category);
  }
}

const char* getLDRCategory(int ldrValue) {
  if (ldrValue < LDR_THRESHOLD_DARK) {
    return "BUIO";
  } else if (ldrValue < LDR_THRESHOLD_MID) {
    return "LUCE-MEDIA";
  } else {
    return "LUCE";
  }
}
