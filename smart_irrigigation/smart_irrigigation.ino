/*
 * Smart Irrigation and Plant Monitoring System
 * Arduino UNO - Single Vase Management
 * 
 * Architecture:
 * - Separates sensor reading from decision logic from actuator control
 * - Uses millis()-based timing, no blocking delay() calls
 * - Fail-safe: pumps default to OFF
 * - Single responsibility per function
 */

// ============================================================================
// PIN ASSIGNMENTS
// ============================================================================

// Vase configuration
#define NUM_VASES 2

// Soil moisture sensor pins (analog)
const int SOIL_MOISTURE_PINS[NUM_VASES] = {A0, A1};

// Water pump relay pins (digital)
const int PUMP_RELAY_PINS[NUM_VASES] = {2, 4};

// Light sensor (LDR) pin (analog)
const int LDR_PIN = A2;

// LED panel control pin (digital)
const int LED_PANEL_PIN = 3;

// Vase identifiers
enum VaseId { BASILICO = 0, ROSMARINO = 1 };

// ============================================================================
// SOIL MOISTURE CALIBRATION
// ============================================================================

// Raw ADC values for dry and wet soil (calibrated from the measured dataset)
const int SOIL_DRY = 830;     // Measured dry reading (approx. max)
const int SOIL_WET = 415;     // Measured wet reading (approx. min)

// Moisture percentage threshold below which irrigation is considered
// A raw reading around 715 corresponds to roughly 72-78% dry in the measured dataset,
// which translates to about 25% moisture in the firmware's wet->dry mapping.
const int MOISTURE_THRESHOLD_PCT = 25;

// ============================================================================
// VASE CONFIGURATION
// ============================================================================

// Per-vase min/max irrigation intervals (milliseconds)
const unsigned long VASE_MIN_IRRIGATION_INTERVAL_MS[NUM_VASES] = {
  48UL * 60UL * 60UL * 1000UL,   // BASILICO: 48 hours
  72UL * 60UL * 60UL * 1000UL    // ROSMARINO: 72 hours
};

const unsigned long VASE_MAX_IRRIGATION_INTERVAL_MS[NUM_VASES] = {
  96UL * 60UL * 60UL * 1000UL,   // BASILICO: 96 hours
  240UL * 60UL * 60UL * 1000UL   // ROSMARINO: 240 hours
};

// ============================================================================
// IRRIGATION TIMING
// ============================================================================

// Duration pump stays on per irrigation event (milliseconds)
const unsigned long PUMP_DURATION_MS = 1000;  // 1 second

// ============================================================================
// LIGHT SENSOR THRESHOLDS
// ============================================================================

const int LDR_THRESHOLD_DARK = 200;      // Dark condition
const int LDR_THRESHOLD_MID = 500;       // Mid-level light
const int LDR_THRESHOLD_LIGHT = 800;     // Bright condition
const unsigned long LDR_DEBUG_INTERVAL_MS = 60000;  // Print LDR status every 60s

// Local time assumptions for daytime detection
const int SYSTEM_START_HOUR = 9;          // Local hour when system was powered on
const int DAYTIME_START_HOUR = 6;         // European summer approximate day begins
const int DAYTIME_END_HOUR = 20;          // European summer approximate day ends

// ============================================================================
// SOIL MOISTURE SAMPLING
// ============================================================================

const unsigned long MOISTURE_SAMPLE_INTERVAL_MS = 100;  // Sample every 100ms
const unsigned long MOISTURE_AVERAGE_INTERVAL_MS = 10000; // Average every 10s
const int MOISTURE_SAMPLE_COUNT = 100;   // Number of samples per average
const unsigned long LOOP_CYCLE_TIME_MS = 1000;  // Approximate loop cycle time for state tracking

// ============================================================================
// STATE TRACKING - SOIL MOISTURE
// ============================================================================

unsigned long lastSampleTime = 0;
int moistureSamples[NUM_VASES][MOISTURE_SAMPLE_COUNT];
int sampleIndex[NUM_VASES];
int currentMoisturePct[NUM_VASES];

// ============================================================================
// STATE TRACKING - IRRIGATION
// ============================================================================

unsigned long lastIrrigationTime[NUM_VASES];
unsigned long pumpActivateTime[NUM_VASES];
bool pumpIsActive[NUM_VASES];

// ============================================================================
// STATE TRACKING - LIGHTING
// ============================================================================

int currentLDRValue = 0;
bool ledPanelIsOn = false;
unsigned long totalDarkTimeToday = 0;      // Cumulative dark time (ms)
unsigned long totalMidLightTimeToday = 0;  // Cumulative mid-light time (ms)
unsigned long totalBrightTimeToday = 0;    // Cumulative bright time (ms)
unsigned long lastLDRDebugTime = 0;
unsigned long dayStartTime = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(9600);
  
  initializePins();
  initializeState();
  
  Serial.println("System initialized.");
}

void initializePins() {
  // Relay outputs default to LOW (pump OFF - fail safe)
  for (int i = 0; i < NUM_VASES; i++) {
    pinMode(PUMP_RELAY_PINS[i], OUTPUT);
    digitalWrite(PUMP_RELAY_PINS[i], LOW);
  }
  
  // LED panel output defaults to LOW (LED OFF)
  pinMode(LED_PANEL_PIN, OUTPUT);
  digitalWrite(LED_PANEL_PIN, LOW);
  
  // Sensor inputs
  for (int i = 0; i < NUM_VASES; i++) {
    pinMode(SOIL_MOISTURE_PINS[i], INPUT);
  }
  pinMode(LDR_PIN, INPUT);
}

void initializeState() {
  lastSampleTime = millis();
  dayStartTime = millis();
  
  // Initialize per-vase state
  for (int i = 0; i < NUM_VASES; i++) {
    lastIrrigationTime[i] = millis();
    sampleIndex[i] = 0;
    currentMoisturePct[i] = 0;
    pumpIsActive[i] = false;
    
    // Clear sample buffers
    for (int j = 0; j < MOISTURE_SAMPLE_COUNT; j++) {
      moistureSamples[i][j] = 0;
    }
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long currentTime = millis();
  
  // Non-blocking sensor reads and decision logic for each vase
  for (int vase = 0; vase < NUM_VASES; vase++) {
    updateSoilMoisture(vase, currentTime);
    evaluateIrrigationNeed(vase, currentTime);
    updatePumpState(vase, currentTime);
  }
  
  // Global sensor reads and decisions
  updateLDRReading(currentTime);
  evaluateLightingNeed(currentTime);
  updateLEDState(currentTime);
}

// ============================================================================
// SENSOR READING - SOIL MOISTURE
// ============================================================================

void updateSoilMoisture(int vase, unsigned long currentTime) {
  if (currentTime - lastSampleTime < MOISTURE_SAMPLE_INTERVAL_MS) {
    return;  // Not time to sample yet
  }
  
  lastSampleTime = currentTime;
  
  int rawValue = analogRead(SOIL_MOISTURE_PINS[vase]);
  moistureSamples[vase][sampleIndex[vase]] = rawValue;
  sampleIndex[vase] = (sampleIndex[vase] + 1) % MOISTURE_SAMPLE_COUNT;
  
  // When buffer is full, calculate average
  if (sampleIndex[vase] == 0) {
    currentMoisturePct[vase] = calculateAverageMoisture(vase);
  }
}

int calculateAverageMoisture(int vase) {
  // Sum all samples for this vase
  long sum = 0;
  for (int i = 0; i < MOISTURE_SAMPLE_COUNT; i++) {
    sum += moistureSamples[vase][i];
  }
  
  int avgRaw = sum / MOISTURE_SAMPLE_COUNT;
  int avgPct = rawToPercentage(avgRaw);
  
  // Log consolidated moisture reading
  printMoistureReading(vase, avgRaw, avgPct);
  
  return avgPct;
}

int rawToPercentage(int rawValue) {
  // Clamp to range [SOIL_WET, SOIL_DRY]
  if (rawValue > SOIL_DRY) rawValue = SOIL_DRY;
  if (rawValue < SOIL_WET) rawValue = SOIL_WET;
  
  // Linear mapping: DRY -> 0%, WET -> 100%
  int percent = 100 - ((rawValue - SOIL_WET) * 100) / (SOIL_DRY - SOIL_WET);
  return percent;
}

// ============================================================================
// SENSOR READING - LIGHT
// ============================================================================

void updateLDRReading(unsigned long currentTime) {
  currentLDRValue = analogRead(LDR_PIN);
  
  // Categorize and track cumulative time for each light condition
  if (currentLDRValue < LDR_THRESHOLD_DARK) {
    totalDarkTimeToday += LOOP_CYCLE_TIME_MS;
  } else if (currentLDRValue < LDR_THRESHOLD_MID) {
    totalMidLightTimeToday += LOOP_CYCLE_TIME_MS;
  } else {
    totalBrightTimeToday += LOOP_CYCLE_TIME_MS;
  }
  
  // Print LDR status at configured interval
  if (currentTime - lastLDRDebugTime >= LDR_DEBUG_INTERVAL_MS) {
    printLDRStatus(currentTime);
    lastLDRDebugTime = currentTime;
  }
}

// ============================================================================
// DECISION LOGIC - IRRIGATION
// ============================================================================

void evaluateIrrigationNeed(int vase, unsigned long currentTime) {
  // Check max interval: if too long since last irrigation, force watering
  if (currentTime - lastIrrigationTime[vase] >= VASE_MAX_IRRIGATION_INTERVAL_MS[vase]) {
    triggerIrrigation(vase, currentTime);
    return;
  }
  
  // Check moisture threshold with min interval guard
  if (currentMoisturePct[vase] < MOISTURE_THRESHOLD_PCT) {
    if (currentTime - lastIrrigationTime[vase] >= VASE_MIN_IRRIGATION_INTERVAL_MS[vase]) {
      triggerIrrigation(vase, currentTime);
    }
  }
}

void triggerIrrigation(int vase, unsigned long currentTime) {
  lastIrrigationTime[vase] = currentTime;
  pumpActivateTime[vase] = currentTime;
  pumpIsActive[vase] = true;
}

// ============================================================================
// DECISION LOGIC - LIGHTING
// ============================================================================

void evaluateLightingNeed(unsigned long currentTime) {
  // If plant got insufficient light during the day, supplement
  // Count dark + mid-light as insufficient
  unsigned long insufficientLightTime = totalDarkTimeToday + totalMidLightTimeToday;
  if (insufficientLightTime > 12UL * 60UL * 60UL * 1000UL) {  // > 12 hours insufficient light
    turnLEDOn();
    return;
  }
  
  // If currently very dark during daytime hours, supplement
  if (isDaytime(currentTime) && currentLDRValue < LDR_THRESHOLD_DARK) {
    turnLEDOn();
    return;
  }
  
  turnLEDOff();
}

// ============================================================================
// ACTUATOR CONTROL - PUMP
// ============================================================================

void updatePumpState(int vase, unsigned long currentTime) {
  if (!pumpIsActive[vase]) {
    // Ensure pump is off (safe default)
    digitalWrite(PUMP_RELAY_PINS[vase], LOW);
    return;
  }
  
  // Activate pump when triggered
  digitalWrite(PUMP_RELAY_PINS[vase], HIGH);
  
  // Check if pump duration has elapsed
  if (currentTime - pumpActivateTime[vase] >= PUMP_DURATION_MS) {
    deactivatePump(vase);
  }
}

void deactivatePump(int vase) {
  digitalWrite(PUMP_RELAY_PINS[vase], LOW);
  pumpIsActive[vase] = false;
}

// ============================================================================
// ACTUATOR CONTROL - LED
// ============================================================================

void turnLEDOn() {
  if (!ledPanelIsOn) {
    digitalWrite(LED_PANEL_PIN, HIGH);
    ledPanelIsOn = true;
  }
}

void turnLEDOff() {
  if (ledPanelIsOn) {
    digitalWrite(LED_PANEL_PIN, LOW);
    ledPanelIsOn = false;
  }
}

void updateLEDState(unsigned long currentTime) {
  // Ensure the LED pin always matches the current desired LED state.
  digitalWrite(LED_PANEL_PIN, ledPanelIsOn ? HIGH : LOW);
}

// ============================================================================
// DEBUG & MONITORING
// ============================================================================

const char* getLDRCategory(int ldrValue) {
  if (ldrValue < LDR_THRESHOLD_DARK) return "DARK";
  if (ldrValue < LDR_THRESHOLD_MID) return "MID-LIGHT";
  return "LIGHT";
}

int getCurrentHour(unsigned long currentTime) {
  unsigned long elapsedHours = currentTime / (60UL * 60UL * 1000UL);
  return (SYSTEM_START_HOUR + elapsedHours) % 24;
}

bool isDaytime(unsigned long currentTime) {
  int hour = getCurrentHour(currentTime);
  return hour >= DAYTIME_START_HOUR && hour < DAYTIME_END_HOUR;
}

void printMoistureReading(int vase, int rawValue, int percentValue) {
  const char* vaseNames[NUM_VASES] = {"BASILICO", "ROSMARINO"};
  Serial.print("[MOISTURE] ");
  Serial.print(vaseNames[vase]);
  Serial.print(" | Threshold: ");
  Serial.print(MOISTURE_THRESHOLD_PCT);
  Serial.print("% | Raw: ");
  Serial.print(rawValue);
  Serial.print(" | Converted: ");
  Serial.print(percentValue);
  Serial.println("%");
}

void printLDRStatus(unsigned long currentTime) {
  Serial.print("[LDR] Value: ");
  Serial.print(currentLDRValue);
  Serial.print(" | Category: ");
  Serial.print(getLDRCategory(currentLDRValue));
  Serial.print(" | Daily - Dark: ");
  Serial.print(totalDarkTimeToday / 1000);
  Serial.print("s | Mid: ");
  Serial.print(totalMidLightTimeToday / 1000);
  Serial.print("s | Bright: ");
  Serial.print(totalBrightTimeToday / 1000);
  Serial.println("s");
}
