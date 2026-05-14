/*
 * Smart Irrigation and Plant Monitoring System
 * Arduino UNO - Two-vase moisture indicator
 *
 * Architecture:
 * - Keeps sensor reading, decision logic, and actuator control separate
 * - Uses millis()-based timing, no blocking delay() calls
 * - No pump control: each vase LED reports low soil moisture
 */

// ============================================================================
// PIN ASSIGNMENTS
// ============================================================================

#define NUM_VASES 2

// Soil moisture sensor pins (analog)
const int SOIL_MOISTURE_PINS[NUM_VASES] = {A0, A1};

// Per-vase moisture alert LEDs (digital)
const int VASE_LED_PINS[NUM_VASES] = {2, 4};

// Light sensor (LDR) pin (analog)
const int LDR_PIN = A2;

// LED panel control pin (digital)
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
bool vaseLedIsOn[NUM_VASES];

// ============================================================================
// STATE TRACKING - LIGHTING
// ============================================================================

enum LightLevel {
  LIGHT_LEVEL_DARK,
  LIGHT_LEVEL_MID,
  LIGHT_LEVEL_GOOD,
  LIGHT_LEVEL_BRIGHT
};

int currentLDRValue = 0;
LightLevel currentLightLevel = LIGHT_LEVEL_DARK;
bool ledPanelIsOn = false;
unsigned long totalDarkTimeToday = 0;
unsigned long totalMidLightTimeToday = 0;
unsigned long totalGoodLightTimeToday = 0;
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
  Serial.println(F("[CONFIG] Bassa umidita' segnalata dai LED dei vasi."));
}

void initializePins() {
  for (int vase = 0; vase < NUM_VASES; vase++) {
    pinMode(VASE_LED_PINS[vase], OUTPUT);
    digitalWrite(VASE_LED_PINS[vase], LOW);
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
  vaseLedIsOn[vase] = false;

  for (int sample = 0; sample < MOISTURE_SAMPLE_COUNT; sample++) {
    moistureSamples[vase][sample] = 0;
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long currentTime = millis();

  updateVaseMoistureIndicators(currentTime);
  updateLDRReading(currentTime);
  evaluateLightingNeed(currentTime);
  syncLedPanelPin();
}

void updateVaseMoistureIndicators(unsigned long currentTime) {
  for (int vase = 0; vase < NUM_VASES; vase++) {
    bool hasNewMoistureReading = updateSoilMoisture(vase, currentTime);
    if (!hasNewMoistureReading) {
      continue;
    }

    evaluateMoistureLedNeed(vase);
    syncVaseLedPin(vase);
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
  currentLightLevel = getLightLevel(currentLDRValue);

  trackLightDuration(currentLightLevel, elapsedMs);
  printLDRStatusIfDue(currentTime);
}

LightLevel getLightLevel(int ldrValue) {
  if (ldrValue < LDR_THRESHOLD_DARK) {
    return LIGHT_LEVEL_DARK;
  }
  if (ldrValue < LDR_THRESHOLD_MID) {
    return LIGHT_LEVEL_MID;
  }
  if (ldrValue < LDR_THRESHOLD_LIGHT) {
    return LIGHT_LEVEL_GOOD;
  }
  return LIGHT_LEVEL_BRIGHT;
}

void trackLightDuration(LightLevel lightLevel, unsigned long elapsedMs) {
  if (lightLevel == LIGHT_LEVEL_DARK) {
    totalDarkTimeToday += elapsedMs;
    return;
  }
  if (lightLevel == LIGHT_LEVEL_MID) {
    totalMidLightTimeToday += elapsedMs;
    return;
  }
  if (lightLevel == LIGHT_LEVEL_GOOD) {
    totalGoodLightTimeToday += elapsedMs;
    return;
  }
  totalBrightTimeToday += elapsedMs;
}

void resetLightTrackingDay(unsigned long currentTime) {
  totalDarkTimeToday = 0;
  totalMidLightTimeToday = 0;
  totalGoodLightTimeToday = 0;
  totalBrightTimeToday = 0;
  dayStartTime = currentTime;
}

// ============================================================================
// DECISION LOGIC - MOISTURE LEDS
// ============================================================================

void evaluateMoistureLedNeed(int vase) {
  bool shouldTurnOn = isBelowMoistureThreshold(currentMoisturePct[vase]);
  if (vaseLedIsOn[vase] == shouldTurnOn) {
    return;
  }

  vaseLedIsOn[vase] = shouldTurnOn;
  printVaseLedState(vase, shouldTurnOn);
}

bool isBelowMoistureThreshold(int moisturePct) {
  return moisturePct < MOISTURE_THRESHOLD_PCT;
}

// ============================================================================
// DECISION LOGIC - LIGHTING
// ============================================================================

void evaluateLightingNeed(unsigned long currentTime) {
  unsigned long insufficientLightTime = totalDarkTimeToday + totalMidLightTimeToday;
  if (insufficientLightTime > INSUFFICIENT_LIGHT_LIMIT_MS) {
    setLedPanelState(true);
    return;
  }

  bool darkDuringDay = isDaytime(currentTime) &&
                       currentLightLevel == LIGHT_LEVEL_DARK;
  setLedPanelState(darkDuringDay);
}

void setLedPanelState(bool shouldTurnOn) {
  if (ledPanelIsOn == shouldTurnOn) {
    return;
  }

  ledPanelIsOn = shouldTurnOn;
  printLedPanelState(shouldTurnOn);
}

// ============================================================================
// ACTUATOR CONTROL - LEDS
// ============================================================================

void syncVaseLedPin(int vase) {
  digitalWrite(VASE_LED_PINS[vase], vaseLedIsOn[vase] ? HIGH : LOW);
}

void syncLedPanelPin() {
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

const char* getLDRCategory(LightLevel lightLevel) {
  if (lightLevel == LIGHT_LEVEL_DARK) {
    return "BUIO";
  }
  if (lightLevel == LIGHT_LEVEL_MID) {
    return "LUCE-MEDIA";
  }
  if (lightLevel == LIGHT_LEVEL_GOOD) {
    return "LUCE";
  }
  return "LUCE-FORTE";
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

void printVaseLedState(int vase, bool isOn) {
  Serial.print(F("[LED_VASO] "));
  Serial.print(getVaseName(vase));
  Serial.print(F(" | "));
  Serial.print(isOn ? F("ACCESO") : F("SPENTO"));
  Serial.print(F(" | Umidita': "));
  Serial.print(currentMoisturePct[vase]);
  Serial.println(F("%"));
}

void printLedPanelState(bool isOn) {
  Serial.print(F("[PANNELLO_LUCE] "));
  Serial.println(isOn ? F("ACCESO") : F("SPENTO"));
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
  Serial.print(getLDRCategory(currentLightLevel));
  Serial.print(F(" | Giorno - Buio: "));
  Serial.print(totalDarkTimeToday / 1000);
  Serial.print(F("s | Luce media: "));
  Serial.print(totalMidLightTimeToday / 1000);
  Serial.print(F("s | Luce: "));
  Serial.print(totalGoodLightTimeToday / 1000);
  Serial.print(F("s | Luce forte: "));
  Serial.print(totalBrightTimeToday / 1000);
  Serial.println(F("s"));
}
