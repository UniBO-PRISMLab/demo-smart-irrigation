/*
 * Smart Irrigation and Plant Monitoring System
 * Arduino UNO - Two-vase irrigation signal
 *
 * Architecture:
 * - Keeps sensor reading, decision logic, and actuator control separate
 * - Uses millis()-based timing, no blocking delay() calls
 * - No pump control: each red LED reports irrigation need for one vase
 */

// ============================================================================
// PIN ASSIGNMENTS
// ============================================================================

#define NUM_VASES 2

// Soil moisture sensor pins (analog)
const int SOIL_MOISTURE_PINS[NUM_VASES] = {A0, A1};

// Per-vase red irrigation signal LEDs (digital)
const int IRRIGATION_SIGNAL_LED_PINS[NUM_VASES] = {2, 4};

// Light sensor (LDR) pin (analog)
const int LDR_PIN = A2;

// UV illumination LED control pin (digital)
const int LED_PANEL_PIN = 7;

const char* const VASE_NAMES[NUM_VASES] = {"BASILICO", "ROSMARINO"};

// ============================================================================
// SOIL MOISTURE CALIBRATION
// ============================================================================

// Raw ADC values for dry and wet soil, calibrated from measured samples.
const int SOIL_DRY = 830;
const int SOIL_WET = 415;

// A raw reading around 715 maps to roughly 25% moisture.
const int MOISTURE_THRESHOLD_PCT = 25;

// ============================================================================
// LIGHT SENSOR THRESHOLDS
// ============================================================================

const int LDR_THRESHOLD_DARK = 200;
const int LDR_THRESHOLD_MID = 500;
const int LDR_THRESHOLD_LIGHT = 800;

const unsigned long LDR_DEBUG_INTERVAL_MS = 60000;
const unsigned long LIGHT_TRACKING_DAY_MS = 24UL * 60UL * 60UL * 1000UL;
const unsigned long INSUFFICIENT_LIGHT_LIMIT_MS = 12UL * 60UL * 60UL * 1000UL;

// Local time assumptions for daytime detection.
const int SYSTEM_START_HOUR = 9;
const int DAYTIME_START_HOUR = 6;
const int DAYTIME_END_HOUR = 20;

// ============================================================================
// SOIL MOISTURE SAMPLING
// ============================================================================

const unsigned long MOISTURE_SAMPLE_INTERVAL_MS = 100;
const unsigned long MOISTURE_AVERAGE_INTERVAL_MS = 10000;
const int MOISTURE_SAMPLE_COUNT =
  (int)(MOISTURE_AVERAGE_INTERVAL_MS / MOISTURE_SAMPLE_INTERVAL_MS);

// ============================================================================
// STATE TRACKING - SOIL MOISTURE
// ============================================================================

unsigned long lastMoistureSampleTime[NUM_VASES];
int moistureSamples[NUM_VASES][MOISTURE_SAMPLE_COUNT];
int moistureSampleIndex[NUM_VASES];
int currentMoisturePct[NUM_VASES];
bool irrigationSignalLedIsOn[NUM_VASES];

// ============================================================================
// STATE TRACKING - LIGHTING
// ============================================================================

int currentLDRValue = 0;
bool ledPanelIsOn = false;
unsigned long totalDarkTimeToday = 0;
unsigned long totalMidLightTimeToday = 0;
unsigned long totalBrightTimeToday = 0;
unsigned long lastLDRDebugTime = 0;
unsigned long lastLDRSampleTime = 0;
unsigned long dayStartTime = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(9600);

  initializePins();
  initializeState();

  Serial.println(F("Sistema inizializzato."));
  Serial.println(F("[CONFIG] Richiesta irrigazione segnalata dai LED rossi."));
}

void initializePins() {
  for (int vase = 0; vase < NUM_VASES; vase++) {
    pinMode(IRRIGATION_SIGNAL_LED_PINS[vase], OUTPUT);
    digitalWrite(IRRIGATION_SIGNAL_LED_PINS[vase], LOW);
    pinMode(SOIL_MOISTURE_PINS[vase], INPUT);
  }

  pinMode(LED_PANEL_PIN, OUTPUT);
  digitalWrite(LED_PANEL_PIN, LOW);
  pinMode(LDR_PIN, INPUT);
}

void initializeState() {
  unsigned long currentTime = millis();
  dayStartTime = currentTime;
  lastLDRSampleTime = currentTime;

  for (int vase = 0; vase < NUM_VASES; vase++) {
    initializeVaseState(vase, currentTime);
  }
}

void initializeVaseState(int vase, unsigned long currentTime) {
  lastMoistureSampleTime[vase] = currentTime;
  moistureSampleIndex[vase] = 0;
  currentMoisturePct[vase] = 0;
  irrigationSignalLedIsOn[vase] = false;

  for (int sample = 0; sample < MOISTURE_SAMPLE_COUNT; sample++) {
    moistureSamples[vase][sample] = 0;
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long currentTime = millis();

  updateIrrigationSignalLeds(currentTime);
  updateLDRReading(currentTime);
  evaluateLightingNeed(currentTime);
  updateLEDState();
}

void updateIrrigationSignalLeds(unsigned long currentTime) {
  for (int vase = 0; vase < NUM_VASES; vase++) {
    bool hasNewMoistureReading = updateSoilMoisture(vase, currentTime);
    if (!hasNewMoistureReading) {
      continue;
    }

    evaluateIrrigationSignalNeed(vase);
    syncIrrigationSignalLedPin(vase);
  }
}

// ============================================================================
// SENSOR READING - SOIL MOISTURE
// ============================================================================

bool updateSoilMoisture(int vase, unsigned long currentTime) {
  if (currentTime - lastMoistureSampleTime[vase] < MOISTURE_SAMPLE_INTERVAL_MS) {
    return false;
  }

  lastMoistureSampleTime[vase] = currentTime;
  storeMoistureSample(vase);
  return isMoistureAverageReady(vase);
}

void storeMoistureSample(int vase) {
  int rawValue = analogRead(SOIL_MOISTURE_PINS[vase]);
  moistureSamples[vase][moistureSampleIndex[vase]] = rawValue;
  moistureSampleIndex[vase] =
    (moistureSampleIndex[vase] + 1) % MOISTURE_SAMPLE_COUNT;
}

bool isMoistureAverageReady(int vase) {
  if (moistureSampleIndex[vase] != 0) {
    return false;
  }

  currentMoisturePct[vase] = calculateAverageMoisture(vase);
  return true;
}

int calculateAverageMoisture(int vase) {
  int averageRaw = calculateTrimmedRawAverage(vase);
  int averagePct = rawToPercentage(averageRaw);

  printMoistureReading(vase, averageRaw, averagePct);
  return averagePct;
}

int calculateTrimmedRawAverage(int vase) {
  long sampleTotal = 0;
  int lowestSample = 1023;
  int highestSample = 0;

  for (int sample = 0; sample < MOISTURE_SAMPLE_COUNT; sample++) {
    int value = moistureSamples[vase][sample];
    sampleTotal += value;
    lowestSample = min(lowestSample, value);
    highestSample = max(highestSample, value);
  }

  return removeOutliersFromAverage(sampleTotal, lowestSample, highestSample);
}

int removeOutliersFromAverage(long sampleTotal, int lowestSample, int highestSample) {
  long trimmedTotal = sampleTotal - lowestSample - highestSample;
  int trimmedCount = MOISTURE_SAMPLE_COUNT - 2;
  return (int)(trimmedTotal / trimmedCount);
}

int rawToPercentage(int rawValue) {
  rawValue = constrain(rawValue, SOIL_WET, SOIL_DRY);

  // Use long math on UNO because int is 16-bit.
  long range = (long)SOIL_DRY - SOIL_WET;
  long offset = (long)rawValue - SOIL_WET;
  int percent = 100 - (int)((offset * 100L) / range);

  return constrain(percent, 0, 100);
}

// ============================================================================
// SENSOR READING - LIGHT
// ============================================================================

void updateLDRReading(unsigned long currentTime) {
  if (currentTime - dayStartTime >= LIGHT_TRACKING_DAY_MS) {
    resetLightTrackingDay(currentTime);
  }

  unsigned long elapsedMs = currentTime - lastLDRSampleTime;
  lastLDRSampleTime = currentTime;
  currentLDRValue = analogRead(LDR_PIN);

  trackLightDuration(currentLDRValue, elapsedMs);
  printLDRStatusIfDue(currentTime);
}

void trackLightDuration(int ldrValue, unsigned long elapsedMs) {
  if (ldrValue < LDR_THRESHOLD_DARK) {
    totalDarkTimeToday += elapsedMs;
    return;
  }
  if (ldrValue < LDR_THRESHOLD_MID) {
    totalMidLightTimeToday += elapsedMs;
    return;
  }

  totalBrightTimeToday += elapsedMs;
}

void resetLightTrackingDay(unsigned long currentTime) {
  totalDarkTimeToday = 0;
  totalMidLightTimeToday = 0;
  totalBrightTimeToday = 0;
  dayStartTime = currentTime;
}

// ============================================================================
// DECISION LOGIC - IRRIGATION SIGNAL LEDS
// ============================================================================

void evaluateIrrigationSignalNeed(int vase) {
  bool shouldTurnOn = isIrrigationNeeded(vase);
  if (irrigationSignalLedIsOn[vase] == shouldTurnOn) {
    return;
  }

  irrigationSignalLedIsOn[vase] = shouldTurnOn;
  printIrrigationSignalLedState(vase, shouldTurnOn);
}

bool isIrrigationNeeded(int vase) {
  return currentMoisturePct[vase] < MOISTURE_THRESHOLD_PCT;
}

// ============================================================================
// DECISION LOGIC - LIGHTING
// ============================================================================

void evaluateLightingNeed(unsigned long currentTime) {
  unsigned long insufficientLightTime = totalDarkTimeToday + totalMidLightTimeToday;
  if (insufficientLightTime > INSUFFICIENT_LIGHT_LIMIT_MS) {
    turnLEDOn();
    return;
  }

  if (isDaytime(currentTime) && currentLDRValue < LDR_THRESHOLD_DARK) {
    turnLEDOn();
    return;
  }

  turnLEDOff();
}

// ============================================================================
// ACTUATOR CONTROL - LEDS
// ============================================================================

void syncIrrigationSignalLedPin(int vase) {
  digitalWrite(
    IRRIGATION_SIGNAL_LED_PINS[vase],
    irrigationSignalLedIsOn[vase] ? HIGH : LOW
  );
}

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

void updateLEDState() {
  digitalWrite(LED_PANEL_PIN, ledPanelIsOn ? HIGH : LOW);
}

// ============================================================================
// TIME HELPERS
// ============================================================================

int getCurrentHour(unsigned long currentTime) {
  unsigned long elapsedHours = currentTime / (60UL * 60UL * 1000UL);
  return (SYSTEM_START_HOUR + elapsedHours) % 24;
}

bool isDaytime(unsigned long currentTime) {
  int hour = getCurrentHour(currentTime);
  return hour >= DAYTIME_START_HOUR && hour < DAYTIME_END_HOUR;
}

// ============================================================================
// DEBUG & MONITORING
// ============================================================================

const char* getVaseName(int vase) {
  return VASE_NAMES[vase];
}

const char* getLDRCategory(int ldrValue) {
  if (ldrValue < LDR_THRESHOLD_DARK) {
    return "BUIO";
  }
  if (ldrValue < LDR_THRESHOLD_MID) {
    return "LUCE-MEDIA";
  }
  return "LUCE";
}

void printMoistureReading(int vase, int rawValue, int percentValue) {
  Serial.print(F("[UMIDITA] "));
  Serial.print(getVaseName(vase));
  Serial.print(F(" | Soglia: "));
  Serial.print(MOISTURE_THRESHOLD_PCT);
  Serial.print(F("% | Grezzo: "));
  Serial.print(rawValue);
  Serial.print(F(" | Convertito: "));
  Serial.print(percentValue);
  Serial.println(F("%"));
}

void printIrrigationSignalLedState(int vase, bool isOn) {
  Serial.print(F("[LED_IRRIGAZIONE] "));
  Serial.print(getVaseName(vase));
  Serial.print(F(" | "));
  Serial.print(isOn ? F("ACCESO") : F("SPENTO"));
  Serial.print(F(" | Richiesta irrigazione: "));
  Serial.print(isOn ? F("SI") : F("NO"));
  Serial.print(F(" | Umidita': "));
  Serial.print(currentMoisturePct[vase]);
  Serial.println(F("%"));
}

void printLDRStatusIfDue(unsigned long currentTime) {
  if (currentTime - lastLDRDebugTime < LDR_DEBUG_INTERVAL_MS) {
    return;
  }

  printLDRStatus();
  lastLDRDebugTime = currentTime;
}

void printLDRStatus() {
  Serial.print(F("[LDR] Valore: "));
  Serial.print(currentLDRValue);
  Serial.print(F(" | Categoria: "));
  Serial.print(getLDRCategory(currentLDRValue));
  Serial.print(F(" | Giorno - Buio: "));
  Serial.print(totalDarkTimeToday / 1000);
  Serial.print(F("s | Luce media: "));
  Serial.print(totalMidLightTimeToday / 1000);
  Serial.print(F("s | Luce: "));
  Serial.print(totalBrightTimeToday / 1000);
  Serial.println(F("s"));
}
