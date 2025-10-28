/**
* AI_SUMMARY: Alternator Regulator Project - Hardware: ESP32-S3-WROOM-1U-N16R8 with PSRAM enabled- this file contains libraries, hardcoded values, setup(), and loop(). Supporting functions are in two additional files: 2_importantFunctions should always be parsed, but 3_nonImportantFunctions is usually not necessary, containing functions less relevant to this debugging session.
* AI_PURPOSE: Read data from various sensors (ADS1115, INA228, NMEA2K CAN Network, VictronVEDirect), control alternator field output, provide real-time user interface with persistent settings
* AI_INPUTS: Sensor data from hardware, hardcoded values, and settings from user interface
* AI_OUTPUTS: user interface display, control of GPIO
* AI_DEPENDENCIES:
* AI_RISKS: Variable naming is inconsistent, need to be careful not to assume consistent patterns. Unit conversion can be very confusing and propagate to many places, have to trace dependencies in variables ALL THE WAY to every end point.  Everything works, but the battery monitor units are challenging to follow thru the codebase.
* CRITICAL_INSTRUCTION_FOR_AI:: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat. When impossible, always mimick my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instructions for me to edit if function is long, to conserve tokens. Always specify which option you chose.
*/

// Delete later, my boat Wifi Network Password for easy copying:  5FENYC8ABC
//home:      HOME-2A8A-2.4
//pw:        exact7204health



// X Engineering Alternator Regulator
//     Copyright (C) 2025  Mark Nickerson

//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.

//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.

//  See <https://www.gnu.org/licenses/> for GNU General Public License

// Contact me at mark@xengineering.net

//By using this device, you agree to XEngineering’s Terms and Conditions:
// https://xengineering.net/terms-and-conditions/

#include <OneWire.h>            // temp sensors
#include <DallasTemperature.h>  // temp sensors
//#include <SPI.h>                // display- removed to save connectors space, code commented out
//#include <U8g2lib.h>            // display- removed to save connectors space, code commented out
#include <Wire.h>          // unknown if needed, but probably
#include <ADS1115_lite.h>  // measuring 4 analog inputs
ADS1115_lite adc(ADS1115_DEFAULT_ADDRESS);
#include "VeDirectFrameHandler.h"  // for victron communication
#include "INA228.h"
INA228 INA(0x40);
//DONT MOVE THE NEXT 6 LINES AROUND, MUST STAY IN THIS ORDER
#include <Arduino.h>                  // maybe not needed, was in NMEA2K example I copied
#define ESP32_CAN_RX_PIN GPIO_NUM_16  //
#define ESP32_CAN_TX_PIN GPIO_NUM_17  //
//#include <NMEA2000_CAN.h>             // Thank you Timo Lappalainen, great stuff  // this one doesn't work with S3
#include <NMEA2000_esp32.h>                                    // (This one is for ESP32-S3, thank you Svante Karlsson)
tNMEA2000_esp32 NMEA2000(ESP32_CAN_TX_PIN, ESP32_CAN_RX_PIN);  // necessary for ESP32-S3 library
#include <N2kMessages.h>
#include <N2kMessagesEnumToStr.h>  // questionably needed
#include <WiFi.h>
#include <AsyncTCP.h>                    // for wifi stuff, IMPORTANT, don't ever update, use mathieucarbou github repository
#include <LittleFS.h>                    //
fs::LittleFSFS webFS;                    // Separate filesystem for web files
bool usingFactoryWebFiles = false;       // Track which web partition is mounted
#include <ESPAsyncWebServer.h>           // for wifi stuff, important, don't ever update, use mathieucarbou github repository
#include <DNSServer.h>                   // For captive portal (Wifi Network Provisioning) functionality
#include <ESPmDNS.h>                     // helps with wifi provisioning (to save users trouble of looking up ESP32's IP address)
#include "esp_heap_caps.h"               // needed for tracking heap usage
#include "freertos/FreeRTOS.h"           // for stack usage
#include "freertos/task.h"               // for stack usage
#define configGENERATE_RUN_TIME_STATS 1  // for CPU use tracking
#include <mbedtls/md.h>                  // security
#include <vector>                        // Console message queue system
#include <String>                        // Console message queue system
#include "esp_task_wdt.h"                //Watch dog to prevent hung up code from wreaking havoc
#include "esp_log.h"                     // get rid of spam in serial monitor
#include <TinyGPSPlus.h>                 // used for NMEA0183, was working great but not currently implemented
#include <math.h>                        // For pow() function needed by Peukert calculation
#include "nvs_flash.h"                   //for persistent storage that's better than Flash
#include "nvs.h"                         //for persistent storage that's better than Flash
#include <HTTPClient.h>                  // for weather reports
#include <ArduinoJson.h>                 // for weather reports (and secure OTA?)
#include <WiFiClientSecure.h>            // for weather reports
#include <PID_v1.h>                      // PID library from http://brettbeauregard.com/blog/2011/04/improving-the-beginners-pid-introduction/    Thanks Brett!
#include <esp_ota_ops.h>                 // secure OTA
#include <mbedtls/pk.h>                  // secure OTA
#include <mbedtls/base64.h>              // secure OTA
#include "esp_system.h"                  // secure OTA
#include <Adafruit_Sensor.h>             // BMP390
#include <Adafruit_BMP3XX.h>             // BMP390
#include "esp_partition.h"               // for esp_partition_find_first
#include "esp_heap_caps.h"


int hardwarePresent = 0;  // generally keep this on (=1), having it off (=0) results in a message in Console, not much else, since we don't even initialize any hardware

// Parse JSON update response - this struct definition cannot move down in file, I don't know why, but leave it here.
struct UpdateInfo {
  bool hasUpdate;
  String version;
  String firmwareUrl;
  String signatureUrl;
  String changelog;
  size_t firmwareSize;
};

// OTA server configuration
const char *OTA_SERVER_URL = "https://ota.xengineering.net";
// IMPORTANT: Firmware Version number MUST follow semantic versioning (x.y.z format)
// - Only numeric digits and dots allowed (regex: ^\d+\.\d+\.\d+$)
// - Examples: "1.0.0", "2.1.3", "10.5.22" ✅
// - Invalid: "1.1.1Retry", "v2.0.0", "2.1.0-beta" ❌
// - Server uses PHP version_compare() for semantic comparison
// - New version MUST be numerically higher than previous for updates to trigger
// - Devices with higher versions will NOT receive "downgrades"
const char *FIRMWARE_VERSION = "0.0.2";

int firmwareVersionInt = 0;  // Will be calculated in setup(), used to display version in client
int deviceIdUpper = 0;       // Will be calculated in setup(), used to display device ID in client (esp32 mac address)
int deviceIdLower = 0;       // Will be calculated in setup(), used to display device ID in client (esp32 mac address)

int currentPartitionType = 0;  // 0=factory, 1=ota_0

// Streaming tar extraction state (no gzip decompression)
struct StreamingExtractor {
  // Tar parsing state
  bool inTarHeader;
  uint8_t tarHeader[512];
  size_t tarHeaderPos;
  String currentFileName;
  size_t currentFileSize;
  size_t currentFilePos;
  bool isCurrentFileFirmware;

  //Padding state for proper streaming
  bool inPadding;
  size_t paddingRemaining;

  // OTA state
  esp_ota_handle_t otaHandle;
  const esp_partition_t *otaPartition;
  bool otaStarted;

  // LittleFS state
  File currentWebFile;
  bool prodFSMounted;

  // Hash verification
  mbedtls_md_context_t hashCtx;
  bool hashStarted;
};

//BMP390
#define BMP3_ADDR 0x76
Adafruit_BMP3XX bmp;


//WIFI STUFF
//these will be the custom network created by the user in AP mode
String esp32_ap_ssid = "ALTERNATOR_WIFI";  // Default SSID
const char *AP_SSID_FILE = "/apssid.txt";  // File to store custom SSID
// WiFi connection timeout when trying to avoid Access Point Mode (and connect to ship's wifi on reboot)
const unsigned long WIFI_TIMEOUT = 20000;  // 20 seconds
const char *AP_PASSWORD_FILE = "/appass.txt";
String esp32_ap_password = "alternator123";  // Default ESP32 AP password
//WiFi Reconnection Management with Signal Strength Awareness
struct WiFiReconnection {
  unsigned long lastAttempt = 0;
  int attemptCount = 0;
  unsigned long currentInterval = 2000;      // Start at 2 seconds
  const unsigned long minInterval = 2000;    // 2 seconds minimum
  const unsigned long maxInterval = 300000;  // 5 minutes maximum
  const int maxAttempts = 20;                // Give up after 20 attempts
  bool giveUpMode = false;
  int lastSignalStrength = -999;       // Track signal strength when connected
  const int minSignalThreshold = -80;  // Don't retry aggressively if signal was weaker than -80 dBm
} wifiRecon;

//Security
char requiredPassword[32] = "admin";  // Max password length = 31 chars     Password for access to change settings from browser
char storedPasswordHash[65] = { 0 };

// ===== HEAP MONITORING =====
int rawFreeHeap = 0;      // in bytes
int FreeHeap = 0;         // in KB
int MinFreeHeap = 0;      // in KB
int FreeInternalRam = 0;  // in KB
int Heapfrag = 0;         // 0–100 %, integer only

// ===== TASK STACK MONITORING =====
const int MAX_TASKS = 20;  // Adjust if you're running lots of tasks
TaskStatus_t taskArray[MAX_TASKS];
// Optional: reuse these across calls if needed
int numTasks = 0;
int tasksCaptured = 0;
int stackBytes = 0;
int core = 0;

//CPU
// ===== CPU LOAD TRACKING =====
unsigned long lastIdle0Time = 0;  // Previous IDLE0 task runtime counter
unsigned long lastIdle1Time = 0;  // Previous IDLE1 task runtime counter
unsigned long lastCheckTime = 0;  // Last time CPU load was measured
int cpuLoadCore0 = 0;             // CPU load percentage for Core 0
int cpuLoadCore1 = 0;             // CPU load percentage for Core 1

//More health monitoring
// Session and health tracking
unsigned long sessionStartTime = 0;
// Duration of last WiFi session (minutes, persistent)
unsigned long LastSessionDuration = 0;     // minutes, persistent
unsigned long CurrentSessionDuration = 0;  //minutes
int LastSessionMaxLoopTime = 0;            // milliseconds, persistent
int MaxLoopTime = 0;                       // not displayed on client, but available here
int lastSessionMinHeap = 999999;           // KB, persistent
int wifiReconnectsTotal = 0;               // persistent, the one i actually use
// Simple WiFi disconnect tracking
unsigned long lastPingTime = 0;
int wifiDisconnectCount = 0;
int LastResetReason;         // why the ESP32 restarted most recently
int ancientResetReason = 0;  // why the ESP32 restarted 2 sessions ago
int totalPowerCycles = 0;    // Total number of power cycles (persistent)
// Warning throttling
unsigned long lastHeapWarningTime = 0;
unsigned long lastStackWarningTime = 0;
const unsigned long WARNING_THROTTLE_INTERVAL = 30000;  // 30 seconds

// These are health variables for the TempTask (digital temperature measurement)
unsigned long lastTempTaskHeartbeat = 0;
bool tempTaskHealthy = true;
const unsigned long TEMP_TASK_TIMEOUT = 20000;  // 20 seconds

//Console
unsigned long lastConsoleMessageTime = 0;
const unsigned long CONSOLE_MESSAGE_INTERVAL = 5000;  //
// Fixed-size circular buffer:
struct ConsoleMessage {
  char message[128];  // Fixed size instead of dynamic String
  unsigned long timestamp;
};
#define CONSOLE_QUEUE_SIZE 10  // Reduced from potentially unlimited vector
ConsoleMessage consoleQueue[CONSOLE_QUEUE_SIZE];
volatile int consoleHead = 0;
volatile int consoleTail = 0;
volatile int consoleCount = 0;


// DNS Server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

enum DeviceMode {
  MODE_CONFIG,  // WiFi configuration (minimal - just DNS + config page)
  MODE_AP,      // Access Point with full functionality
  MODE_CLIENT   // Client mode connected to ship's WiFi
};
DeviceMode currentMode = MODE_CLIENT;  // gets overwritten immediately, exact mode here doesn't matter

//little fs monitor
bool littleFSMounted = false;

int INADisconnected = 0;
int WifiHeartBeat = 0;
int VeTime = 0;
int SendWifiTime = 0;
int AnalogReadTime = 0;        // this is the present
int AnalogReadTime2 = 0;       // this is the maximum ever
int AinputTrackerTime = 5000;  //ms- how long between resetting maximum loop time and analog read times in display
//Needed for ALERT pin on INA228 to shut off Field and delay 10 seconds before retrying if fault condition is gone
bool inaOvervoltageLatched = false;
unsigned long inaOvervoltageTime = 0;


// Weather Mode Global Variables (add with your other globals)
float UVToday = 0.0;
float UVTomorrow = 0.0;
float UVDay2 = 0.0;
int SolarWatts = 660;                      // nominal solar array power in watts
float pKwHrToday;                          // predicted solar output in kw*hr for the rest of today, as of the last weather forecast download
float pKwHrTomorrow;                       // predicted solar output in kw*hr for all of tomorrow
float pKwHr2days;                          // predicted solar output in kw*hr for 2 days from now
const float MJ_TO_KWH_CONVERSION = 0.278;  // MJ/m²/day to kWh/m²/day
const float STC_IRRADIANCE = 1000.0f;      // Standard Test Conditions W/m²
float performanceRatio = .75;              // Real-world performance losses (typically 0.75)


unsigned long weatherLastUpdate = 0;
int weatherDataValid = 0;  // 0=false, 1=true
String weatherLastError = "";
int weatherHttpResponseCode = 0;

// Weather Mode Settings (add to your settings section)
int weatherModeEnabled = 0;            // 0=disabled, 1=enabled
float UVThresholdHigh = 2.1;           // UV index above this = high solar expected (kWh)
int WeatherUpdateInterval = 21600000;  // Update every 6 hours (in milliseconds)
int WeatherTimeoutMs = 10000;          // HTTP timeout in milliseconds

// Weather Mode Status Variables
int currentWeatherMode = 0;           // 0=normal, 1=high solar, 2=low solar
unsigned long nextWeatherUpdate = 0;  // When next update is due

//Input Settings
int TargetAmps = 40;  //Normal alternator output, for best performance, set to something that just barely won't overheat
int TargetAmpL = 25;  //Alternator output in Lo mode
int uTargetAmps = 3;  // the one that gets used as the real target

float FloatVoltage = 13.4;                       // self-explanatory
float BulkVoltage = 13.9;                        // this could have been called Target Bulk Voltage to be more clear
float ChargingVoltageTarget = 0;                 // This becomes active target
float VoltageHardwareLimit = BulkVoltage + 0.1;  // could make this a setting later, but this should be decently safe
bool inBulkStage = true;
unsigned long bulkCompleteTime = 1000;  // milliseconds
unsigned long bulkCompleteTimer = 0;    // this is a timer, don't change

unsigned long FLOAT_DURATION = 12 * 3600;  // 12 hours in seconds
unsigned long floatStartTime = 0;

float FieldAdjustmentInterval = 50;  // The regulator field output is updated once every this many milliseconds
float TemperatureLimitF = 150;       // the offset appears to be +40 to +50 to get true max alternator external metal temp, depending on temp sensor installation, so 150 here will produce a metal temp ~200F
int ManualFieldToggle = 1;           // set to 1 to enable manual control of regulator field output, helpful for debugging
int SwitchControlOverride = 1;       // set to 1 for web interface switches to override physical switch panel
int ForceFloat = 0;                  // Set to 1 to target 0 amps at battery (not really "float" charging, but we end up call it it that, sorry)
int OnOff = 0;                       // 0 is charger off, 1 is charger On (corresponds to Alternator Enable in Basic Settings)
int Ignition = 0;                    // Digital Input      NEED THIS TO HAVE WIFI ON , FOR NOW
int IgnitionOverride = 1;            // to fake the ignition signal w/ software
int HiLow = 1;                       // 0 will be a low setting, 1 a high setting
int AmpSrc = 0;                      // 0=Alt Hall Effect, 1=Battery Shunt, 2=NMEA2K Batt, 3=NMEA2K Alt, 4=NMEA0183 Batt, 5=NMEA0183 Alt, 6=Victron Batt, 7=Other
int LimpHome = 0;                    // 1 will set to limp home mode, whatever that gets set up to be
int resolution = 12;                 // for OneWire temp sensor measurement
int VeData = 0;                      // Set to 1 if VE serial data exists
int NMEA0183Data = 0;                // Set to 1 if NMEA serial data exists doesn't do anything yet
//Field PWM stuff
float dutyStep = 0.8;   // Field Adj Step (V) larger value = faster response but more unstable
const int pwmPin = 14;  // field PWM pin # (was 32)
//const int pwmChannel = 0;      //0–7 available for high-speed PWM  (ESP32)
const int pwmResolution = 12;          // Error = +0.010%    PWM Resolution = ±0.024% (1/4096)
float SwitchingFrequency = 1200;       // Field switching frequency 15kHz - well above audible, well below problematic frequencies
const int MIN_SAFE_FREQUENCY = 50;     // Above most audible issues // set to 2000 later
const int MAX_SAFE_FREQUENCY = 25000;  // Below core loss and EMI concerns
float MaxDuty = 99.0;
float MinDuty = 33.0;      //33 works on my boat to make sure RPM is always present, may need to make funciton of RPM?? Later
int ManualDutyTarget = 4;  // example manual override value
int InvertAltAmps = 1;     // change sign of alternator amp reading
int InvertBattAmps = 0;    // change sign of battery amp reading
uint32_t Freq = 0;         // ESP32 switching Frequency in case we want to report it for debugging

//Variables to store measurements
float ShuntVoltage_mV;                // Battery shunt voltage from INA228
float Bcur;                           // battery shunt current from INA228
float targetCurrent;                  // This is used in the field adjustment loop, gets set to the desired source of current info (ie battery shunt, alt hall sensor, victron, etc.)
float IBV;                            // Ina 228 battery voltage
float IBVMax = 6;                     // used to track maximum battery voltage    TEMPORARY TROUBLESHOOTING
float dutyCycle;                      // Field outout %--- this is just what's transmitted over Wifi (case sensitive)
float FieldResistance = 2;            // Field resistance in Ohms usually between 2 and 6 Ω, changes 10-20% with temp
float vvout;                          // Calculated field volts (approximate)
float iiout;                          // Calculated field amps (approximate)
float AlternatorTemperatureF = NAN;   // alternator temperature
float MaxAlternatorTemperatureF = 0;  // maximum alternator temperature
// === Thermistor Stuff
float R_fixed = 10000.0;             // Series resistor in ohms
float Beta = 3950.0;                 // Thermistor Beta constant (e.g. 3950K)
float R0 = 10000.0;                  // Thermistor resistance at T0
float T0_C = 25.0;                   // Reference temp in Celsius
int TempSource = 0;                  // 0 for OneWire default, 1 for Thermistor
int temperatureThermistor = -99;     // thermistor reading
int MaxTemperatureThermistor = -99;  // maximum thermistor temperature (on alternator)
int TempToUse;                       // gets set to temperatureThermistor or AlternatorTemperatureF
TaskHandle_t tempTaskHandle = NULL;  // make a separate cpu task for temp reading because it's so slow
float VictronVoltage = 0;            // battery V reading from VeDirect
float VictronCurrent = 0;            // battery Current (careful, can also be solar current if hooked up to solar charge controller not BMV712)
float HeadingNMEA = 0;               // Just here to test NMEA functionality

double LatitudeNMEA = 43.6591;    // from own ship AIS
double LongitudeNMEA = -70.2568;  // from own ship AIS
int NMEA2KData = 0;               //
int NMEA2KVerbose = 0;            // print stuff to serial monitor or not
int SatelliteCountNMEA = 0;       // Number of satellites


// ADS1115
int16_t Raw = 0;
float Channel0V, Channel1V, Channel2V, Channel3V;
float BatteryV, MeasuredAmps, RPM;  //Readings from ADS1115
//float voltage;                      // This is the one that gets populated by dropdown menu selection (battery voltage source)
float MeasuredAmpsMax;  // used to track maximum alternator output
float RPMMax;           // used to track maximum RPM
int ADS1115Disconnected = 0;

// Battery SOC Monitoring Variables
int BatteryCapacity_Ah = 300;         // Battery capacity in Amp-hours
int SOC_percent = 7500;               // State of Charge percentage (0-100) but have to multiply by 100 for annoying reasons, but go with it
int ManualSOCPoint = 25;              // Used to set it manually
int CoulombCount_Ah_scaled = 7500;    // Current energy in battery (Ah × 100 for precision)
float PeukertRatedCurrent_A = 15.0f;  // Standard discharge rate for Peukert (C/20), will be calculated from capacity
bool FullChargeDetected = false;      // Flag for full charge detection
unsigned long FullChargeTimer = 600;  // Timer for full charge detection, 10 minutes
// Timing variables
unsigned long currentTime = 0;
unsigned long elapsedMillis = 0;
unsigned long lastSOCUpdateTime = 0;      // Last time SOC was updated
unsigned long lastEngineMonitorTime = 0;  // Last time engine metrics were updated
unsigned long lastDataSaveTime = 0;       // Last time data was saved to LittleFS
int SOCUpdateInterval = 2000;             // Update SOC every 2 seconds.   Don't make this smaller than 1 without study

//NVS Stuff
unsigned long lastNVSSaveTime = 0;
const unsigned long NVS_SAVE_INTERVAL = 120000;  // 2 minutes
/*
NVS lifetime analysis (ESP32-S3-WROOM-1U-N16R8, 16 MB flash)
Partition: nvs @ 0x9000, size = 0x5000 (20 KB = 5 pages × 4 KB)
Effective pages for wear leveling: ~4 (1 reserved for GC)
Entries/page ≈ 126 (32 B each) → total ~504 entries capacity before GC
Flash endurance: ≥100,000 erase cycles per page

Save cadence = 2 minutes → 720 saves/day
Case A (5 keys change/save): 3,600 entries/day → 3,600/504 ≈ 7.14 GC cycles/day
 → 7.14 erases/page/day → lifetime ≈ 100,000/7.14 ≈ 14,000 days ≈ 38 years
Case B (21 keys change/save): 15,120 entries/day → 30 GC cycles/day
 → 30 erases/page/day → lifetime ≈ 3,333 days ≈ 9 years

Conclusion: At 2-minute saves with change-detection, NVS wear is well within
multi-year (≈10–40 yr) service life even in worst case.
*/


// Previous values for change detection (simple globals)
uint32_t prev_ChargedEnergy = 0;
uint32_t prev_DischrgdEnergy = 0;
uint32_t prev_AltChrgdEnergy = 0;
int32_t prev_AltFuelUsed = 0;
int32_t prev_EngineRunTime = 0;
int32_t prev_EngineCycles = 0;
int32_t prev_AltOnTime = 0;
int32_t prev_SOC_percent = 0;
int32_t prev_CoulombCount = 0;
uint32_t prev_SessionDur = 0;
int32_t prev_MaxLoop = 0;
int32_t prev_MinHeap = 0;
int32_t prev_PowerCycles = 0;
float prev_InsulDamage = 0.0f;
float prev_GreaseDamage = 0.0f;
float prev_BrushDamage = 0.0f;
float prev_ShuntGain = 0.0f;
float prev_AltZero = 0.0f;
uint32_t prev_LastGainTime = 0;
uint32_t prev_LastZeroTime = 0;
float prev_LastZeroTemp = 0.0f;

// Accumulators for runtime tracking
unsigned long engineRunAccumulator = 0;     // Milliseconds accumulator for engine runtime
unsigned long alternatorOnAccumulator = 0;  // Milliseconds accumulator for alternator runtime

//ALternator Lifetime Prediction
// Thermal Stress Calculation - Settings
float WindingTempOffset = 50.0;  // User configurable winding temp offset (°F)
float PulleyRatio = 2.0;         // User configurable pulley ratio
int ManualLifePercentage = 100;  // Manual override for life remaining %

// Thermal Stress Calculation - Accumulated Damage
float CumulativeInsulationDamage = 0.0;  // 0.0 to 1.0 (1.0 = end of life)
float CumulativeGreaseDamage = 0.0;      // 0.0 to 1.0 (1.0 = end of life)
float CumulativeBrushDamage = 0.0;       // 0.0 to 1.0 (1.0 = end of life)

// Thermal Stress Calculation - Current Status
float InsulationLifePercent = 100.0;  // Current insulation life remaining %
float GreaseLifePercent = 100.0;      // Current grease life remaining %
float BrushLifePercent = 100.0;       // Current brush life remaining %
float PredictedLifeHours = 10000.0;   // Minimum predicted life remaining
int LifeIndicatorColor = 0;           // 0=green, 1=yellow, 2=red


// Timing
unsigned long lastThermalUpdateTime = 0;              // Last thermal calculation time
const unsigned long THERMAL_UPDATE_INTERVAL = 10000;  // 10 seconds
const int AnalogInputReadInterval = 900;              // self explanatory

// Physical Constants
const float EA_INSULATION = 1.0f;     // eV, activation energy
const float BOLTZMANN_K = 8.617e-5f;  // eV/K
const float T_REF_K = 373.15f;        // 100°C reference temperature in Kelvin
const float L_REF_INSUL = 100000.0f;  // Reference insulation life at 100°C (hours)
const float L_REF_GREASE = 40000.0f;  // Reference grease life at 158°F (hours)
const float L_REF_BRUSH = 5000.0f;    // Reference brush life at 6000 RPM and 150°F (hours)

//Cool dynamic factors
// SOC Correction Dynamic Learning
int AutoShuntGainCorrection = 0;           // 0=off, 1=on - enable/disable auto-correction
float DynamicShuntGainFactor = 1.0;        // Learned gain correction factor (starts at 1.0)
int ResetDynamicShuntGain = 0;             // Momentary reset button (0=normal, 1=reset)
unsigned long lastGainCorrectionTime = 0;  // Last time gain correction was applied

// SOC Correction Protection Constants
const float MAX_GAIN_ADJUSTMENT_PER_CYCLE = 0.05;            // Max 5% change per full charge detection
const float MIN_DYNAMIC_GAIN_FACTOR = 0.8;                   // Don't go below 80%
const float MAX_DYNAMIC_GAIN_FACTOR = 1.2;                   // Don't go above 120%
const float MAX_REASONABLE_ERROR = 0.2;                      // Don't correct if error > 20%
const unsigned long MIN_GAIN_CORRECTION_INTERVAL = 3600000;  // 1 hour minimum between corrections

// Alternator Current Auto-Zeroing
int AutoAltCurrentZero = 0;         // 0=off, 1=on - enable/disable auto-zeroing
float DynamicAltCurrentZero = 0.0;  // Learned zero offset (starts at 0.0)
int ResetDynamicAltZero = 0;        // Momentary reset button (0=normal, 1=reset)

// Auto-zeroing timing and state tracking
unsigned long lastAutoZeroTime = 0;                // Last time auto-zero was performed
float lastAutoZeroTemp = -999.0;                   // Temperature when last auto-zero was done
unsigned long autoZeroStartTime = 0;               // When current auto-zero cycle started (0 = not active)
const unsigned long AUTO_ZERO_DURATION = 10000;    // 10 seconds at zero field
const unsigned long AUTO_ZERO_INTERVAL = 3600000;  // 1 hour in milliseconds
const float AUTO_ZERO_TEMP_DELTA = 20.0;           // 20°F temperature change triggers auto-zero
// Auto-zero averaging variables
float autoZeroAccumulator = 0.0;
int autoZeroSampleCount = 0;

//Momentary Buttons and alarm logic
int FactorySettings = 0;  // Reset Button
// Add these alarm variables with your other globals
bool alarmLatch = false;    // Current latched alarm state
int AlarmLatchEnabled = 0;  // Whether latching is enabled (0/1 for consistency)
int AlarmTest = 0;          // Momentary alarm test (1 = test active)
int ResetAlarmLatch = 0;    // Momentary reset command
unsigned long alarmTestStartTime = 0;
const unsigned long ALARM_TEST_DURATION = 2000;  // 2 seconds test duration
int Alarm_Status;                                // for alarm mirror light on Client

//More Settings
// SOC Parameters
int CurrentThreshold = 1;             // Ignore currents below this (A × 100)
int PeukertExponent_scaled = 105;     // Peukert exponent × 100 (112 = 1.12)
int ChargeEfficiency_scaled = 99;     // Charging efficiency % (0-100)
int ChargedVoltage_Scaled = 1450;     // Voltage threshold for "charged" (V × 100) (a Battery Monitor setup parameter, nothing to do with alternator)
int TailCurrent = 2;                  // A percentage of battery capacity in amp hours
int ShuntResistanceMicroOhm = 100;    // Shunt resistance in microohms
int ChargedDetectionTime = 1000;      // Time at charged state to consider 100% (seconds)
int IgnoreTemperature = 0;            // If no temp sensor, set to 1
int bmsLogic = 0;                     // if BMS is asked to turn the alternator on and off
int bmsLogicLevelOff = 0;             // set to 0 if the BMS gives a low signal (<3V?) when no charging is desired
bool chargingEnabled;                 // defined from other variables
bool bmsSignalActive;                 // Read from digital input pin 36
int AlarmActivate = 0;                // set to 1 to enable alarm conditions
int TempAlarm = 190;                  // above this value, sound alarm
int VoltageAlarmHigh = 15;            // above this value, sound alarm
int VoltageAlarmLow = 11;             // below this value, sound alarm
int CurrentAlarmHigh = 100;           // above this value, sound alarm
int MaximumAllowedBatteryAmps = 100;  // safety for battery, optional
int FourWay = 0;                      // 0 voltage data source = INA228 , 1 voltage source = ADS1115, 2 voltage source = NMEA2k, 3 voltage source = Victron VeDirect
int RPMScalingFactor = 2000;          // self explanatory, adjust until it matches your trusted tachometer
float AlternatorCOffset = 0;          // tare for alt current
float BatteryCOffset = 0;             // tare or batt current
int timeToFullChargeMin = -999;       // self explained
int timeToFullDischargeMin = -999;    // self explained


int ResetTemp;              // reset the maximum alternator temperature tracker
int ResetVoltage;           // reset the maximum battery voltage measured
int ResetCurrent;           // reset the maximmum alternator output current
int ResetEngineRunTime;     // reset engine run time tracker
int ResetAlternatorOnTime;  //reset AlternatorOnTime
int ResetEnergy;            // ???
int ResetDischargedEnergy;  // total discharged from battery
int ResetFuelUsed;          // fuel used by alternator
int ResetAlternatorChargedEnergy;
int ResetEngineCycles;
int ResetRPMMax;
int ResetThermTemp = 0;  // Max thermistor temp reset

unsigned long fieldCollapseTime = 0;
const unsigned long FIELD_COLLAPSE_DELAY = 10000;  // 10 seconds
int fieldActiveStatus = 0;                         // direct read of ESP32 hardare to control the field active light in Banner


int Voltage_scaled = 0;            // Battery voltage scaled (V × 100)
int BatteryCurrent_scaled = 0;     // A × 100
int AlternatorCurrent_scaled = 0;  // Alternator current scaled (A × 100)
int BatteryPower_scaled = 0;       // Battery power (W × 100)
int EnergyDelta_scaled = 0;        // Energy change (Wh × 100)
int AlternatorFuelUsed = 0;        // Total fuel used by alternator (mL) - INTEGER (note: mL not L)
bool alternatorIsOn = false;       // Current alternator state
// Energy Tracking Variables
unsigned long ChargedEnergy = 0;            // Total charged energy from battery (Wh)
unsigned long DischargedEnergy = 0;         // Total discharged energy from battery (Wh)
unsigned long AlternatorChargedEnergy = 0;  // Total energy from alternator (Wh)
int FuelEfficiency_scaled = 250;            // Engine efficiency: Wh per mL of fuel (× 100)
// Engine & Alternator Runtime Tracking
int EngineRunTime = 0;          // Time engine has been spinning (minutes)
int EngineCycles = 0;           // Average RPM * Minutes of run time
int AlternatorOnTime = 0;       // Time alternator has been producing current (minutes)
bool engineWasRunning = false;  // Engine state in previous check
bool alternatorWasOn = false;   // Alternator state in previous check


// variables used to show how long each loop takes
uint64_t starttime;
uint64_t endtime;
int LoopTime;             // must not use unsigned long becasue cant run String() on an unsigned long and that's done by the wifi code
int WifiStrength;         // must not use unsigned long becasue cant run String() on an unsigned long and that's done by the wifi code
int MaximumLoopTime;      // must not use unsigned long becasue cant run String() on an unsigned long and that's done by the wifi code
int prev_millis7888 = 0;  // used to reset the meximum loop time

// Global variable to track ESP32 restart time
unsigned long lastRestartTime = 0;

const unsigned long RESTART_INTERVAL = 7200000;  // 2 hour in milliseconds = 7200000
//const unsigned long RESTART_INTERVAL = 300000;  // 5 minutes temporary development interval fix later


int BatteryVoltageSource = 0;  // select  "0">INA228    value="1">ADS1115     value="2">VictronVeDirect     value="3">NMEA0183     value="4">NMEA2K
int BatteryCurrentSource = 0;  // 0=INA228, 1=NMEA2K Batt, 2=NMEA0183 Batt, 3=Victron Batt
int timeAxisModeChanging = 0;  // toggle the time axis on and off in Plots.  Off = less janky but less info

int RPMThreshold = -20000;  //below this, there will be no field output in auto mode (Update this if we have RPM at low speeds and no field, otherwise, depend on Ignition) Check this later

int maxPoints;                //number of points plotted per plot (X axis length)
int Ymin1 = -10, Ymax1 = 90;  // Current plot
int Ymin2 = 10, Ymax2 = 20;   // Voltage plot
int Ymin3 = 0, Ymax3 = 4000;  // RPM plot
int Ymin4 = 50, Ymax4 = 250;  // Temperature plot


//Advanced temperature control variables:
//=============================================================================
//                        LEARNING MODE GLOBAL VARIABLES
//=============================================================================

// ===== LEARNING MODE CONTROL PARAMETERS =====
// Primary Controls
int LearningMode = 0;             // 0=disabled, 1=enabled
int LearningPaused = 0;           // Temporarily pause learning
int LearningUpwardEnabled = 1;    // Allow upward table adjustments
int LearningDownwardEnabled = 1;  // Allow downward table adjustments

//unknown
bool learningTableUpdated = false;

// Learning Parameters
int AlternatorNominalAmps = 100;               // Alternator rating for penalty calculation
float LearningUpStep = 1;                      // Table increase amount (A)
float LearningDownStep = 2;                    // Table decrease amount (A)
float AmbientTempCorrectionFactor = -0.5;      // A per °C correction
float AmbientTempBaseline = 20.0;              // Baseline temp (°C)
unsigned long MinLearningInterval = 30000;     // Min time between updates (ms)
unsigned long SafeOperationThreshold = 30000;  // Time for upward learning (ms)

// RPM Transition Filtering (add with other learning variables)
unsigned long lastSignificantRPMChange = 0;  // Timestamp of last major RPM change
int lastStableRPM = 0;                       // RPM before transition
int LearningSettlingPeriod = 30000;          // Milliseconds to wait after RPM change (default 30s)
int LearningRPMChangeThreshold = 500;        // RPM change that triggers settling period
int LearningTempHysteresis = 10;

// PID Tuning
float PidKp = 0.5;                  // Proportional gain
float PidKi = 0.0;                  // Integral gain
float PidKd = 0.0;                  // Derivative gain
unsigned long PidSampleTime = 100;  // PID update interval (ms)

// Table Bounds & Safety
float MaxTableValue = 120.0;               // Maximum table entry (A)
float MinTableValue = 0.0;                 // Minimum table entry (A)
float MaxPenaltyPercent = 15.0;            // Max penalty as % of nominal
unsigned long MaxPenaltyDuration = 60000;  // Max penalty time (ms)

// Advanced Learning
float NeighborLearningFactor = 0.25;                // Neighbor reduction factor
int LearningRPMSpacing = 500;                       // RPM spacing for table
unsigned long LearningMemoryDuration = 2592000000;  // How long to remember events (30 days in ms)

// Safety Overrides
int IgnoreLearningDuringPenalty = 1;  // Block learning during penalty
int EnableNeighborLearning = 1;       // Update adjacent RPM points
int EnableAmbientCorrection = 0;      // Apply temperature correction
int LearningFailsafeMode = 0;         // Extra conservative behavior

// Diagnostics & Debugging
int ShowLearningDebugMessages = 1;  // Verbose console output
int LogAllLearningEvents = 0;       // Log every learning decision
int EXTRA;                          //
int LearningDryRunMode = 0;         // Calculate but don't apply changes

// Data Management
int AutoSaveLearningTable = 1;                     // Auto-save to NVS
unsigned long LearningTableSaveInterval = 300000;  // How often to save (5 min in ms)

// Momentary Actions (Reset to 0 after execution)
int ResetLearningTable = 0;    // Reset to factory defaults
int ClearOverheatHistory = 0;  // Clear all overheat counters

// ===== LEARNING TABLE DATA =====
#define RPM_TABLE_SIZE 10
float rpmCurrentTable[RPM_TABLE_SIZE] = { 0, 40, 50, 50, 50, 50, 50, 50, 50, 50 };  //
int rpmTableRPMPoints[RPM_TABLE_SIZE] = { 100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600 };
// Reset for conservative factory defaults for current limits
float defaultCurrentValues[RPM_TABLE_SIZE] = { 0, 50, 60, 60, 60, 60, 60, 60, 60, 60 };
// Reset for factory defaults for RPM breakpoints
int defaultRPMValues[RPM_TABLE_SIZE] = { 100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600 };

unsigned long lastOverheatTime[RPM_TABLE_SIZE] = { 0 };          // Timestamp of last overheat per RPM
int overheatCount[RPM_TABLE_SIZE] = { 0 };                       // Total overheat events per RPM
unsigned long cumulativeNoOverheatTime[RPM_TABLE_SIZE] = { 0 };  // Safe operation time per RPM

// ===== LEARNING STATE VARIABLES =====
unsigned long overheatingPenaltyTimer = 0;   // Remaining penalty time (s)
float overheatingPenaltyAmps = 0.0;          // Current penalty amount (A)
int currentRPMTableIndex = -1;               // Which table entry we're in (-1 = invalid)
unsigned long learningSessionStartTime = 0;  // Session start time for current RPM
unsigned long lastTableUpdateTime = 0;       // Last learning update timestamp

// ===== PID CONTROLLER VARIABLES =====
double pidInput = 0.0;                                                             // Current measured amps
double pidOutput = 0.0;                                                            // PID duty cycle output
double pidSetpoint = 0.0;                                                          // Current target amps
PID currentPID(&pidInput, &pidOutput, &pidSetpoint, PidKp, PidKi, PidKd, DIRECT);  // PID object
bool pidInitialized = false;                                                       // PID initialization status

// ===== CALCULATED VALUES FOR DISPLAY =====
float learningTargetFromRPM = -1.0;  // Table lookup result before corrections
float ambientTempCorrection = 0.0;   // Calculated temp correction (A)
float finalLearningTarget = 0.0;     // After all corrections applied
float ambientTemp = 20.0;            // Current ambient temperature (°C)
float baroPressure = 1000;           // BMP380


// ===== LEARNING DIAGNOSTICS =====
unsigned long totalLearningEvents = 0;    // Total learning updates performed
unsigned long totalOverheats = 0;         // Sum of all overheatCount[]
unsigned long totalSafeHours = 0;         // Sum of all cumulativeNoOverheatTime[]
float averageTableValue = 0.0;            // Mean of rpmCurrentTable[]
unsigned long timeSinceLastOverheat = 0;  // Global time since any overheat

//=============================================================================
//                    END LEARNING MODE GLOBAL VARIABLES
//=============================================================================


// Universal data freshness tracking
// Complete DataIndex enum for all variables displayed in Live Data
// Streamlined DataIndex enum - only tracks real-time sensor data that might go stale if a sensor is disconnected
// Excludes peak/cumulative values that should persist even when source fails
enum DataIndex {
  // NMEA/GPS Data (real-time navigation)
  IDX_HEADING_NMEA = 0,  // 0
  IDX_LATITUDE_NMEA,     // 1
  IDX_LONGITUDE_NMEA,    // 2
  IDX_SATELLITE_COUNT,   // 3
  // Victron VE.Direct Data (real-time readings)
  IDX_VICTRON_VOLTAGE,  // 4
  IDX_VICTRON_CURRENT,  // 5
  // Temperature Sensors (real-time only)
  IDX_ALTERNATOR_TEMP,  // 6
  IDX_THERMISTOR_TEMP,  // 7
  // Engine/RPM Data (real-time only)
  IDX_RPM,  // 8
  // Alternator Current/Power (real-time readings)
  IDX_MEASURED_AMPS,  // 9
  // Battery Voltage Sources (real-time readings)
  IDX_BATTERY_V,  // 10 - ADS1115 battery voltage
  IDX_IBV,        // 11 - INA228 battery voltage
  // Battery Current (real-time reading)
  IDX_BCUR,  // 12 - Battery current from INA228
  // ADS1115 Channels (real-time analog readings)
  IDX_CHANNEL3V,  // 13 - ADS Ch3 Voltage
  // Real-time calculated values (these become meaningless if inputs are stale)
  IDX_DUTY_CYCLE,   // 14 - Field duty cycle percentage
  IDX_FIELD_VOLTS,  // 15 - vvout (calculated field voltage)
  IDX_FIELD_AMPS,   // 16 - iiout (calculated field current)
  // Keep this last - gives us array bounds checking
  MAX_DATA_INDICES = 17  // Now matches the 17 timestamps being sent
};
unsigned long dataTimestamps[MAX_DATA_INDICES];  // Uses the enum size automatically

const unsigned long DATA_TIMEOUT = 10000;  // 10 seconds default timeout
// Universal macros for clean syntax
#define MARK_FRESH(index) dataTimestamps[index] = millis()
#define IS_STALE(index) (millis() - dataTimestamps[index] > DATA_TIMEOUT)
#define SET_IF_STALE(index, variable, staleValue) \
  if (IS_STALE(index)) { variable = staleValue; }


// ISRG Root X1 Certificate (Let's Encrypt Root CA)
// Valid until: June 4, 2035 (expires in 2035, stable for 10+ years)
// This root certificate doesn't change like intermediate certificates (R10->R12 rotation)
// Using root instead of intermediate prevents certificate validation failures during Let's Encrypt updates
const char *server_root_ca =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
  "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
  "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
  "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
  "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
  "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
  "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
  "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
  "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
  "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
  "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
  "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
  "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
  "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
  "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
  "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
  "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
  "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
  "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
  "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
  "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
  "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
  "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
  "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
  "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
  "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
  "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
  "-----END CERTIFICATE-----\n";


// RSA Public Key -
const char *OTA_PUBLIC_KEY =
  "-----BEGIN PUBLIC KEY-----\n"
  "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAp2sRgMjD4wazKHo6Rk3g\n"
  "7APj7xsYGimOUKhWGWvd8wxFOEL+bHatAeKyCZXBjrZI9rdiUGtyHDdXS246YvV6\n"
  "dr56hJFT05n9iMHfx5T99NOyBl8Nm18dijqLGDSgVD9rC8v4aMrx5lSMrgzfz+bh\n"
  "HaDny6BFBHvaCQZW2KxXdnZElgnRidsoSR+5twdwdv9biHEwm4Miss1QQjmVZPVg\n"
  "SPIj4j9pf6wf8+kr57nEtjQORu+lLVj3OkM+A94N4k78+tR3gc3UMr1GvrqXm9D6\n"
  "VBH4OZDEgnx+QzraU9O60c6Jp/YXJkeLjRGFS7AaTd5+8sEFHoyx4BhvvxyUC7tX\n"
  "SpIZqziIlRjdJFE+8PjJ76u2BD1JmGeboUU+sxLJFiMpNeBskJG0kVwkFobkUtpN\n"
  "7O57CaGuzA8Eih+x3jTO85CiqgsK7rRckFyvMin1AgIvvxN9CUY+jTYvfcRdUO5Y\n"
  "7xQ8bFCvNUnpmqlWRveq3cxNZpNjOf99NinLJ6ewuaBqYXTtYbNCeAO+l0IiEl74\n"
  "c0HALRefCJO9mHmSoHsYJ7RRTLKhNO7jvoo7B+qCb2JDUKhwaLEetIDF6nw0dH22\n"
  "hONJuBOYktuwhrrsupJyBWYHHFmnFhqBcXCuTiPDhpcW05/3oAwApS7SpN8bN7dp\n"
  "SXQcqfsev1ke5IRosYpCv6cCAwEAAQ==\n"
  "-----END PUBLIC KEY-----\n";


// pre-setup stuff
// onewire    Data wire is connetec to the Arduino digital pin 13
#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;

// //Display     HAD TO REMOVE FOR CONNECTOR SPACE
// // OLED pin mapping from RJ45 → ESP32
// #define OLED_CS 5      // RJ45 Pin 15
// #define OLED_DC 19     // RJ45 Pin 14
// #define OLED_RESET 27  // RJ45 Pin 13
// // SSD1306 OLED using 4-wire SPI, full framebuffer
// U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RESET);
// bool displayAvailable = false;  // Global flag to track if display is working

//VictronEnergy
VeDirectFrameHandler myve;

// WIFI STUFF
// WiFi wake button functionality (GPIO5)
int WiFiWakeButton = 0;                           // Current button state (1=pressed)
unsigned long wifiWakeButtonPressTime = 0;        // Timestamp when button pressed
const unsigned long WIFI_WAKE_DURATION = 300000;  // 5 minutes
bool wifiWakeActive = false;                      // Tracking wake mode state
unsigned long wifiWakeTimeout = 0;                // When WiFi should turn off (0 = inactive)

AsyncWebServer server(80);                  // Create AsyncWebServer object on port 80
AsyncEventSource events("/events");         // Create an Event Source on /events
unsigned long webgaugesinterval = 50;       // delay in ms between sensor updates on webpage
int plotTimeWindow = 120;                   // Plot time window in seconds
unsigned long healthystuffinterval = 5000;  // check hardware health parameters only every 5 seconds, not that they consume much

// WiFi provisioning settings
const char *WIFI_SSID_FILE = "/ssid.txt";
const char *WIFI_PASS_FILE = "/pass.txt";

typedef struct {
  unsigned long PGN;
  void (*Handler)(const tN2kMsg &N2kMsg);
} tNMEA2000Handler;

void SystemTime(const tN2kMsg &N2kMsg);
void Rudder(const tN2kMsg &N2kMsg);
void Speed(const tN2kMsg &N2kMsg);
void WaterDepth(const tN2kMsg &N2kMsg);
void DCStatus(const tN2kMsg &N2kMsg);
void BatteryConfigurationStatus(const tN2kMsg &N2kMsg);
void COGSOG(const tN2kMsg &N2kMsg);
void GNSS(const tN2kMsg &N2kMsg);
void Attitude(const tN2kMsg &N2kMsg);
void Heading(const tN2kMsg &N2kMsg);
void GNSSSatsInView(const tN2kMsg &N2kMsg);

//PGN Handler Table
tNMEA2000Handler NMEA2000Handlers[] = {
  { 126992L, &SystemTime },
  { 127245L, &Rudder },
  { 127250L, &Heading },
  { 127257L, &Attitude },
  { 127506L, &DCStatus },
  { 127513L, &BatteryConfigurationStatus },
  { 128259L, &Speed },
  { 128267L, &WaterDepth },
  { 129026L, &COGSOG },
  { 129029L, &GNSS },
  { 129540L, &GNSSSatsInView },
  { 0, 0 }
};

// Stream *OutputStream; // old, i think it worked at one point..
Stream *OutputStream = &Serial;  // safe, correct


//ADS1115 more pre-setup crap
enum ADS1115_State {
  ADS_IDLE,
  ADS_WAITING_FOR_CONVERSION
};

ADS1115_State adsState = ADS_IDLE;
uint8_t adsCurrentChannel = 0;
unsigned long adsStartTime = 0;
const unsigned long ADSConversionDelay = 20;  // see notes in initializeHardware()

// Forward declarations for WiFi functions
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void setupWiFi();
bool connectToWiFi(const char *ssid, const char *password, unsigned long timeout);
void setupAccessPoint();
void setupWiFiConfigServer();
void dnsHandleRequest();
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);

// HTML for the WiFi configuration page with traditional concatenation
const char WIFI_CONFIG_HTML[] PROGMEM =
  "<!DOCTYPE html>"
  "<html><head>"
  "<title>WiFi Configuration</title>"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<style>"
  "body{font-family:Arial;padding:20px;background:#f5f5f5}"
  ".card{background:white;padding:20px;border-radius:8px;max-width:400px;margin:0 auto}"
  "h1{color:#333;margin-bottom:20px;text-align:center}"
  "input,select{width:100%;padding:8px;margin:5px 0;border:1px solid #ddd;border-radius:4px}"
  "button{background:#00a19a;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;width:100%}"
  "button:hover{background:#008c86}"
  ".info-box{background:#e8f4f8;border:1px solid #bee5eb;color:#0c5460;padding:12px;border-radius:4px;margin:10px 0;font-size:14px}"
  "</style>"
  "</head><body>"

  "<div class=\"card\">"
  "<h1>WiFi Configuration</h1>"

  "<div class=\"info-box\">"
  "<strong>Preferred Option:</strong><br>"
  "Enter your ship's network WiFi credentials below. The regulator will henceforth run in Client Mode and the user interface will be accessible via your local wifi.  Navigate to alternator.local in any browser, just like you'd go to google.com."
  "</div>"

  "<form action=\"/wifi\" method=\"POST\">"
  "<label>Ship's WiFi Name (SSID):</label>"
  "<input type=\"text\" name=\"ssid\" placeholder=\"Required for client mode\">"
  "<label>Ship's WiFi Password:</label>"
  "<input type=\"password\" name=\"password\" placeholder=\"Required for client mode\">"

  "<div class=\"info-box\">"
  "<strong>Non-Preferred Option:</strong><br>"
  "As backup, or for ships without existing WiFi networks, you may use the regualtor as a Hotspot (aka Access Point). The regulator controller will broadcast a new WiFi network which you can connect to from any device.  The same interface and functionality will exist at alternator.local, but due to lack of internet connection, you won't be able to use Solar mode, get software updates, see Community features, etc.  This mode is less supported.  To enter this mode on a reboot, you must connect pin 12 in RJ3 (the rightmost ethernet connector, blue wire) to Ground.  I recommend to just leave it connected forever if you prefer this mode."
  "</div>"

  "<label>New Alt. Reg. Hotspot Name (SSID):</label>"
  "<input type=\"text\" name=\"hotspot_ssid\" placeholder=\"Leave blank for default: ALTERNATOR_WIFI\">"
  "<label>New Alt. Reg. Hotspot Password:</label>"
  "<input type=\"password\" name=\"ap_password\" placeholder=\"Leave blank for default: alternator123\">"

  "<div class=\"info-box\">"
  "***To boot into Hotspot mode, the Hotspot Wire must be connected to Ground during a restart.***"
  "</div>"

  "<div class=\"info-box\">"
  "To return to this Wifi Configuration page at any time, connect the Wifi Configuration Wire to Ground during a restart."
  "</div>"

  "<button type=\"submit\">Save Configuration</button>"

  "<div class=\"info-box\">"
  "After saving, this page may become unresponsive or disappear. In any case, wait 20 seconds, then reconnect to your chosen network to access the full alternator interface."
  "</div>"
  "</form>"
  "</div>"
  "</body></html>";

void setup() {
  Serial.end();  // Force complete serial restart (might not need this delete later)
  delay(100);
  Serial.begin(115200);          //note, the below settings only apply to Serial, not Serial1 or Serial2
  Serial.setTxTimeoutMs(500);    // avoid long stalls on any possible big serial monitor bursts
  Serial.setTxBufferSize(4096);  // 4 KB TX in internal RAM
  Serial.setRxBufferSize(2048);  // optional, we don't currently Rx anyway...
  delay(2000);

  // Force multiple attempts (might not need this delete later)
  // for (int i = 0; i < 5; i++) {
  //   Serial.println("=== BOOT ATTEMPT " + String(i) + " ===");
  //   Serial.println();
  //   Serial.flush();  // waits for all outgoing serial data to finish transmitting.  ie if you just did Serial.print("Hello"); followed by Serial.flush();, the function blocks until all bytes in the UART transmit buffer have been physically sent over the wire.
  //   delay(500);
  // }
  Serial.println("\n\n=== SYSTEM STARTUP ===");
  Serial.println();

  Serial.flush();  // Ensure it's sent before continuing
  int major = 0, minor = 0, patch = 0;

  // Parse FIRMWARE_VERSION directly (e.g. "0.1.65")
  const char *version = FIRMWARE_VERSION;
  int len = strlen(version);

  // Find first dot
  int firstDot = -1;
  for (int i = 0; i < len; i++) {
    if (version[i] == '.') {
      firstDot = i;
      break;
    }
  }

  // Find second dot
  int secondDot = -1;
  for (int i = firstDot + 1; i < len; i++) {
    if (version[i] == '.') {
      secondDot = i;
      break;
    }
  }

  // Parse each part
  if (firstDot > 0 && secondDot > firstDot) {
    // Parse major (0 to firstDot)
    for (int i = 0; i < firstDot; i++) {
      major = major * 10 + (version[i] - '0');
    }

    // Parse minor (firstDot+1 to secondDot)
    for (int i = firstDot + 1; i < secondDot; i++) {
      minor = minor * 10 + (version[i] - '0');
    }

    // Parse patch (secondDot+1 to end)
    for (int i = secondDot + 1; i < len; i++) {
      patch = patch * 10 + (version[i] - '0');
    }
  }

  firmwareVersionInt = major * 10000 + minor * 100 + patch;
  Serial.print("Firmware Version Int: ");
  Serial.println(firmwareVersionInt);
  Serial.println();



  uint64_t chipid = ESP.getEfuseMac();
  deviceIdUpper = (uint32_t)(chipid >> 32);
  deviceIdLower = (uint32_t)(chipid & 0xFFFFFFFF);

  printPartitionInfo();  // DELETE LATER, just used to prove the Custom scheme works
  initializeNVS();
  RestoreLastSessionValues();  // just used for ESP32 stats from last session

  //REMOVE LATER DEBUG
  // Serial.printf("IDLE stack  : %d B\n", (int)CONFIG_FREERTOS_IDLE_TASK_STACKSIZE);
  // Serial.printf("IPC stack   : %d B\n", (int)CONFIG_ESP_IPC_TASK_STACK_SIZE);
  // Serial.printf("Timer stack : %d B\n", (int)CONFIG_ESP_TIMER_TASK_STACK_SIZE);
  // Serial.printf("Event stack : %d B\n", (int)CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE);

  if (!ensureLittleFS()) {
    Serial.println("CRITICAL: Cannot continue without filesystem");
    Serial.println();

    while (true)
      ;  // halt
  }
  delay(500);
  checkWebFilesExist();
  sessionStartTime = millis();
  captureResetReason();            // immediately capture the reason for last ESP32 shutdown and store in LittleFS and variable that won't be overwritten until next boot
  ensurePreferredBootPartition();  // Ensure we boot from preferred partition
  loadNVSData();                   // Load persistent variables from NVS- everything from last session is restored
  initNVSCache();                  // Sync change-detection cache with loaded NVS values to prevent false writes
  //Reset some parameters to zero since we are re-starting on a re-boot
  CurrentSessionDuration = 0;
  MaxLoopTime = 0;
  totalPowerCycles++;  // Increment power cycle counter
  saveNVSData();       // Save immediately to persist the adjustments done above in setup so far
  setCpuFrequencyMhz(240);

  pinMode(4, OUTPUT);     // This pin is used to provide a high signal to Field Enable pin
  digitalWrite(4, LOW);   // Start with field off
  pinMode(5, INPUT);      // WiFi wake button
  pinMode(2, OUTPUT);     // This pin is used to provide a heartbeat (pin 2 of ESP32 is the LED)
  pinMode(1, INPUT);      // Ignition
  pinMode(21, OUTPUT);    // Alarm/Buzzer output (was 33)
  digitalWrite(21, LOW);  // Start with alarm off
  pinMode(42, INPUT);     // bmsLogic
  // PWM setup (needed for basic operation)
  //ledcAttach(pwmPin, SwitchingFrequency, pwmResolution);
  InitSystemSettings();       // load all settings from LittleFS.  If no files exist, create them.
  initWeatherModeSettings();  // Add weather mode settings--- otherwise similar to line above (InitSystemSettings)
  loadPasswordHash();
  // Check if we should wake WiFi for a pending OTA update
  nvs_handle_t wake_handle;
  if (nvs_open("update_req", NVS_READONLY, &wake_handle) == ESP_OK) {
    uint8_t wakeFlag = 0;
    if (nvs_get_u8(wake_handle, "wake_flag", &wakeFlag) == ESP_OK && wakeFlag == 1) {
      wifiWakeTimeout = millis() + WIFI_WAKE_DURATION;
      queueConsoleMessage("UPDATE: WiFi wake enabled for pending update (5 min)");

      // Clear the wake flag immediately
      nvs_close(wake_handle);
      nvs_handle_t clear_wake;
      if (nvs_open("update_req", NVS_READWRITE, &clear_wake) == ESP_OK) {
        nvs_erase_key(clear_wake, "wake_flag");
        nvs_commit(clear_wake);
        nvs_close(clear_wake);
      }
    } else {
      nvs_close(wake_handle);
    }
  }
  setupWiFi();  // NOW setup WiFi with all settings properly loaded
  esp_log_level_set("esp32-hal-i2c-ng", ESP_LOG_WARN);
  queueConsoleMessage("System starting up...");

  if (hardwarePresent == 1) {
    initializeHardware();  // Initialize hardware systems
  };

  // Enable watchdog - only after setup is complete
  // What Happens During Watchdog Reset:
  //Watchdog triggers (after 15 seconds of hang)
  //ESP32 immediately reboots (hardware reset)
  //ALL GPIO pins reset to 0 (including pin 4 field enable)
  //Field control pin 4 goes LOW → Field turns OFF
  //Alternator output stops → Batteries safe
  //ESP32 restarts and runs setup()
  //Normal operation resumes

  Serial.println();

  Serial.println("Enabling watchdog protection...");
  // Try to add main task to existing watchdog first
  esp_err_t add_result = esp_task_wdt_add(NULL);
  if (add_result == ESP_OK) {
    Serial.println("Watchdog already active - main task added");
    events.send("Watchdog already active - main task added", "console", millis());
  } else if (add_result == ESP_ERR_INVALID_STATE) {
    // Watchdog exists but we're already registered
    Serial.println("Watchdog already active - main task already registered");
    events.send("Watchdog already active - main task already registered", "console", millis());
  } else {
    // No watchdog exists, try to create one
    Serial.printf("No existing watchdog found (%s), creating new one\n", esp_err_to_name(add_result));

    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 15000,
      .idle_core_mask = 0,
      .trigger_panic = true
    };
    esp_err_t init_result = esp_task_wdt_init(&wdt_config);

    if (init_result == ESP_OK) {
      esp_task_wdt_add(NULL);  // Add main task
      Serial.println("Watchdog initialized successfully");
      queueConsoleMessage("Watchdog created and main task added");
    } else {
      Serial.printf("Watchdog init failed: %s\n", esp_err_to_name(init_result));
      queueConsoleMessage("Watchdog setup failed - continuing without watchdog protection");
    }
    Serial.println();
  }

  // OBSOLETE- this will get updated to check for existence of FORCED UPDATES in future work, but right now, it's irrelevant
  // // Only check for OTA updates in client mode, otherwise might hang and trigger watchdog
  // if (currentMode == MODE_CLIENT) {
  //   checkForOTAUpdate();
  // }

  // Force initial sensor readings before main loop starts
  delay(100);  // Brief settling time
  if (hardwarePresent == 1) {
    ReadAnalogInputs();
    delay(100);          // Give it a moment to process
    ReadAnalogInputs();  // Second reading to be sure
  }

  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  if (running_partition == factory_partition) {
    currentPartitionType = 0;  // Factory
    Serial.println("=== We are in factory partition! ===");

  } else {
    currentPartitionType = 1;  // OTA_0 (production)
    Serial.println("=== We are in OTA partition! ===");
  }

  Serial.println("=== SETUP COMPLETE ===");
}

void loop() {
  esp_task_wdt_reset();
  Ignition = !digitalRead(1);  // ! is for optocoupler
  if (IgnitionOverride == 1) {
    Ignition = 1;
  }

  static DeviceMode lastMode = MODE_CONFIG;  //DEBUG REMOVE LATER
  if (currentMode != lastMode) {
    Serial.printf("MODE CHANGED (0=CONFIG, 1= AP, 2 = CLIENT): %d -> %d\n", lastMode, currentMode);
    lastMode = currentMode;
  }  //END DEBUG

  WiFiWakeButton = !digitalRead(5);  // Read WiFi wake button state. Active LOW (button pulls to ground)

  // Button press extends/starts timeout
  if (WiFiWakeButton == 1) {
    wifiWakeTimeout = millis() + WIFI_WAKE_DURATION;  // Reset to 5min from now
  }

  // === OTA UPDATE RETRY LOGIC  ===
  static unsigned long lastUpdateCheck = 0;
  static bool updateCheckComplete = false;
  static int updateAttempts = 0;
  const int MAX_UPDATE_ATTEMPTS = 12;

  if (!updateCheckComplete && currentMode != MODE_CONFIG) {
    if (millis() - lastUpdateCheck > 5000 && updateAttempts < MAX_UPDATE_ATTEMPTS) {
      lastUpdateCheck = millis();
      updateAttempts++;
      checkForPendingManualUpdate();
      updateCheckComplete = true;
    } else if (updateAttempts > 0 && updateAttempts < MAX_UPDATE_ATTEMPTS) {
      // Only log "waiting" if we've started checking but haven't found an update
      queueConsoleMessage("UPDATE: Waiting for WiFi (" + String(updateAttempts) + "/12)");
    }

    if (updateAttempts >= MAX_UPDATE_ATTEMPTS) {
      updateCheckComplete = true;
      queueConsoleMessage("UPDATE: WiFi timeout after 60s - check connection and request update again");
    }
  }
  // === END OTA UPDATE RETRY ===

  // ========== COMMON CODE: Runs regardless of hardwarePresent flag ==========

  //temp debug print (runs whether hardware present or not):
  static unsigned long lastVersionPrint = 0;
  if (millis() - lastVersionPrint > 60000) {
    Serial.println("DEBUG: Firmware info");
    Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition == NULL) {
      Serial.println("ERROR: running_partition is NULL");
    } else {
      Serial.printf("Partition: %s\n", running_partition->label);
    }
    Serial.printf("Device ID: %s\n", getDeviceId().c_str());
    lastVersionPrint = millis();
  }

  esp_task_wdt_reset();              // Feed the watchdog to prevent timeout
  starttime = esp_timer_get_time();  // Record start time for Loop
  currentTime = millis();

  // SOC and runtime update every 2 seconds (runs regardless of hardwarePresent)
  if (currentTime - lastSOCUpdateTime >= SOCUpdateInterval) {
    CurrentSessionDuration = (millis() - sessionStartTime) / 1000 / 60;  //minutes
    elapsedMillis = currentTime - lastSOCUpdateTime;
    lastSOCUpdateTime = currentTime;
    UpdateEngineRuntime(elapsedMillis);
    UpdateBatterySOC(elapsedMillis);

    handleSocGainReset();  // do the dynamic updates
    handleAltZeroReset();  // do the dynamic udpates
  }

  saveNVSData();  // Only save current operational data, not session stats

  // ========== POWER MANAGEMENT: Handle ignition state and WiFi wake mode ==========
  // This runs BEFORE the mode switch to ensure WiFi is in correct state before attempting transmission
  // Power management affects AP and CLIENT modes, but NOT CONFIG mode (CONFIG mode exits early below)

  // Ignition state tracking variables
  static bool lastIgnitionState = -1;          // Track previous ignition state (-1 = uninitialized)
  static bool lastWifiWakeActive = false;      // Track previous WiFi wake mode state
  static bool wakeExpiryWarningShown = false;  // Track if we've warned about wake mode expiring

  if (Ignition == 0) {
    // ===== IGNITION OFF =====
    bool wifiWakeActive = (millis() < wifiWakeTimeout);

    // Detect state change for WiFi wake mode
    if (wifiWakeActive != lastWifiWakeActive) {
      if (wifiWakeActive) {
        Serial.println("WiFi Wake Mode ACTIVATED (GPIO5 triggered)");
      } else {
        Serial.println("WiFi Wake Mode DEACTIVATED - Entering low power mode");
        wakeExpiryWarningShown = false;  // Reset warning flag for next wake cycle
      }
      lastWifiWakeActive = wifiWakeActive;
    }

    if (wifiWakeActive) {
      // Wake mode active - full power for user monitoring
      setCpuFrequencyMhz(240);
      if (WiFi.getMode() == WIFI_OFF) {  // WiFi was turned off, need to reconnect
        setupWiFi();                     // Reconnects in current mode (AP or CLIENT)
      }
      // Warn user before timeout expires
      if ((wifiWakeTimeout - millis()) < 20000 && !wakeExpiryWarningShown) {
        queueConsoleMessage("WiFi wake mode expiring in 20 seconds - press button to extend");
        wakeExpiryWarningShown = true;
      }
    } else {
      // Low power mode - save battery
      setCpuFrequencyMhz(10);
      WiFi.mode(WIFI_OFF);
    }

    // Detect ignition turning OFF
    if (lastIgnitionState == 1) {
      Serial.println("Ignition OFF");
      lastIgnitionState = 0;
    }

  } else {
    // ===== IGNITION ON - Normal operation =====

    // Detect ignition state change to ON
    if (lastIgnitionState != 1) {
      Serial.println("Ignition ON - Normal operation mode");
      lastIgnitionState = 1;
    }

    wifiWakeTimeout = 0;             // Clear wake mode
    lastWifiWakeActive = false;      // Reset wake mode tracking
    wakeExpiryWarningShown = false;  // Reset warning flag
    setCpuFrequencyMhz(240);         // Ensure full speed

    // Reconnect WiFi if needed (works for both AP and CLIENT modes)
    if (WiFi.getMode() == WIFI_OFF) {
      setupWiFi();  // Reconnects in current mode
    }
  }
  // ========== END POWER MANAGEMENT ==========

  // ========== MODE HANDLING: Switch statement runs for all modes ==========
  switch (currentMode) {
    case MODE_CONFIG:
      Serial.println("Switch case for MODE_CONFIG entered");
      // Configuration mode - WiFi setup ONLY, no alternator operation
      // This is intentional for safety: user must configure WiFi before alternator use
      // EMERGENCY ACCESS: Hold GPIO46 LOW during boot to enter AP mode (full functionality without credentials)
      dnsHandleRequest();  // Process captive portal DNS requests
      Serial.println("dnsHandled, about to return");
      return;  // Exit loop() entirely - prevents alternator operation without proper configuration

    case MODE_AP:
      // Full functionality - same as client mode, plus DNS for captive portal
      Serial.println("Switch case for MODE_AP entered");
      dnsHandleRequest();  // Need DNS for captive portal in AP mode
      Serial.println("dnsHandled, about to fall through");
      // Fall through to full functionality (no break statement - intentional)

    case MODE_CLIENT:
      Serial.println("Switch case for MODE_CLIENT entered");
      // Full sensor/data functionality (also runs for MODE_AP due to fall-through above)

      // ========== HARDWARE-DEPENDENT SECTION ==========
      // Read sensors: Real hardware or fake data depending on hardwarePresent flag
      if (hardwarePresent == 1) {
        ReadAnalogInputs();  // Real sensor readings
      } else {
        ReadAnalogInputs_Fake();  // Fake sensor readings for development
        // Notify user every 5 seconds that we're in fake mode
        static unsigned long lastFakeWarning = 0;
        if (millis() - lastFakeWarning > 5000) {
          Serial.println("hardwarePresent=0: Using fake sensor data");
          events.send("hardwarePresent flag is 0 - using fake sensor data", "console", millis());
          lastFakeWarning = millis();
        }
      }
      // ========== END HARDWARE-DEPENDENT SECTION ==========

      // All remaining code runs regardless of hardwarePresent flag
      // This allows full development/testing even with broken hardware

      // Handle learning table reset- must be done outside of any particular mode, so might as well do it here.  Shouldn't this later be a "hasparam" operation rather than waste space in this loop?
      if (ResetLearningTable == 1) {
        resetLearningTableToDefaults();
        ResetLearningTable = 0;
      }

      if (hardwarePresent == 1) {
        checkTempTaskHealth();  // digital temperature measurement monitor (only with real hardware)
      }

      if (VeData == 1 && hardwarePresent == 1) {
        ReadVEData();  //read Data from Victron (only with real hardware)
      }
      if (NMEA2KData == 1 && hardwarePresent == 1) {
        NMEA2000.ParseMessages();  // CAN bus (only with real hardware)
      }

      if (hardwarePresent == 1) {
        Alarm_Status = digitalRead(21);  // Read alarm input (only with real hardware)
      }
      CheckAlarms();             // Process alarms (runs with fake or real data)
      calculateThermalStress();  // alternator lifetime modeling (runs with fake or real data)
      //UpdateDisplay();
      checkAutoZeroTriggers();  //Auto-zero processing (must be before AdjustField)
      processAutoZero();        //Auto-zero processing (must be before AdjustField)

      if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED) {  // this probably should be a request hasparam someday
        updateWeatherMode();
      }

      // Field control - only runs with real hardware
      // This may need to get moved if it takes any power, but have to be careful we don't get stuck with Field On!
      if (hardwarePresent == 1) {
        AdjustFieldLearnMode();
      }

      logDashboardValues();  //  nice to have some history in the Console
      SomeHealthyStuff();
      updateSystemHealthMetrics();
      SendWifiData();  // Safely sends data (has internal guard to check if WiFi is actually on)

      // Client-specific connection monitoring
      if (currentMode == MODE_CLIENT) {
        checkWiFiConnection();
        // Check for WiFi disconnects (simple ping method)
        static unsigned long lastDisconnectCheck = 0;
        static bool alreadyCountedDisconnect = false;  // NEW: Prevent spam counting
        if (millis() - lastDisconnectCheck > 10000) {  // Check every 10 seconds
          lastDisconnectCheck = millis();
          if (millis() - lastPingTime > 25000 && lastPingTime > 0) {
            // WiFi is disconnected
            if (!alreadyCountedDisconnect) {  // Only count once per disconnect
              wifiDisconnectCount++;
              wifiReconnectsTotal++;
              Serial.println("Client hearbeat ping missed: could be WiFi/router trouble. Count: " + String(wifiDisconnectCount));
              queueConsoleMessage("WiFi disconnect #" + String(wifiDisconnectCount));
              alreadyCountedDisconnect = true;  // Mark as counted
            }
          } else if (lastPingTime > 0 && (millis() - lastPingTime < 25000)) {
            // WiFi is connected (recent ping received)
            alreadyCountedDisconnect = false;  // Reset flag for next disconnect
          }
        }
      }
      break;  // Exit switch statement, continue with rest of loop()
  }

  // ========== LOOP TIMING METRICS ==========
  if (millis() - prev_millis7888 > AinputTrackerTime) {  // every 5 seconds reset the maximum loop time
    MaximumLoopTime = 0;
    prev_millis7888 = millis();
  }
  endtime = esp_timer_get_time();  //Record end of Loop
  LoopTime = (endtime - starttime);
  if (LoopTime > 5000000) {  // 5 seconds in microseconds
    Serial.println("WARNING: Loop took " + String(LoopTime / 1000) + "ms - potential watchdog risk");
    events.send("WARNING: Loop took " + String(LoopTime / 1000) + "ms - potential watchdog risk", "console", millis());
  }
  //This one resets every 5 seconds (AinputTrackerTime) and is displayed on client
  if (LoopTime > MaximumLoopTime) {
    MaximumLoopTime = LoopTime;
  }
  //This one is persistent, not displayed in current session for Client, but visible on next boot
  if (LoopTime > MaxLoopTime) {
    MaxLoopTime = LoopTime;
  }

  checkAndRestart();     // scheduled maintenance restart
  esp_task_wdt_reset();  // Always reset watchdog at end of loop
  delay(1);              // Not sure this is needed, whatever
}

//This has to stay here for unknown reasons
template<typename T> void PrintLabelValWithConversionCheckUnDef(const char *label, T val, double (*ConvFunc)(double val) = 0, bool AddLf = false, int8_t Desim = -1) {
  OutputStream->print(label);
  if (!N2kIsNA(val)) {
    if (Desim < 0) {
      if (ConvFunc) {
        OutputStream->print(ConvFunc(val));
      } else {
        OutputStream->print(val);
      }
    } else {
      if (ConvFunc) {
        OutputStream->print(ConvFunc(val), Desim);
      } else {
        OutputStream->print(val, Desim);
      }
    }
  } else OutputStream->print("not available");
  if (AddLf) OutputStream->println();
}
//*****************************************************************************