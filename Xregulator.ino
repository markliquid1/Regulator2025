
/**
 * AI_SUMMARY: Alternator Regulator Project - this file contains libraries, most hardcoded values, setup(), and loop().  Supporting unctions are in two additional files: 2_importantFunctions should alwasy be parsed, but 3_nonImportantFunctions is usually not necessary, containing standard and/or reliable and/or non-used functions.
 * AI_PURPOSE: Read data from various sensors (ADS1115, INA228, NMEA2K CAN Network, VictronVEDirect), control alternator field output, provide real-time user interface with persistent settings
 * AI_INPUTS: Sensor data, hardcoded values, and parameters from user interface
 * AI_OUTPUTS: user interface live display, control of GPIO
 * AI_DEPENDENCIES: 
 * AI_RISKS: heap usage is high.  Wifi payload approaching 1400 byte limit for packet transmission.  Variable naming is inconsisent, need to be careful not to assume consistent patterns.   Unit conversion can be very confusing and propogate to many places, have to trace dependencies in variables ALL THE WAY to every end point.
 * AI_OPTIMIZE: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat.  When impossible, always mimik my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instrucutions for me to edit if function is long.  Always specify which option you chose.
 */

// X Engineering Alternator Regulator
// Delete later, my Wifi Network Password for easy copying:  5FENYC8ABC

#include <OneWire.h>            // temp sensors
#include <DallasTemperature.h>  // temp sensors
#include <SPI.h>                // display
#include <U8g2lib.h>            // display
#include <Wire.h>               // unknown if needed, but probably
#include <ADS1115_lite.h>       // measuring 4 analog inputs
ADS1115_lite adc(ADS1115_DEFAULT_ADDRESS);
#include "VeDirectFrameHandler.h"  // for victron communication
#include "INA228.h"
INA228 INA(0x40);
//DONT MOVE THE NEXT 6 LINES AROUND, MUST STAY IN THIS ORDER
#include <Arduino.h>                  // maybe not needed, was in NMEA2K example I copied
#define ESP32_CAN_RX_PIN GPIO_NUM_16  //
#define ESP32_CAN_TX_PIN GPIO_NUM_17  //

#include <NMEA2000_CAN.h>  // Thank you Timo Lappalainen, great stuff!
#include <N2kMessages.h>
#include <N2kMessagesEnumToStr.h>  // questionably needed
#include <WiFi.h>
#include <AsyncTCP.h>                    // for wifi stuff, important, don't ever update, use mathieucarbou github repository
#include <LittleFS.h>                    // for wifi stuff
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

//WIFI STUFF
//these will be the custom network created by the user in AP mode
String esp32_ap_ssid = "ALTERNATOR_WIFI";  // Default SSID
const char *AP_SSID_FILE = "/apssid.txt";  // File to store custom SSID
// WiFi connection timeout when trying to avoid Access Point Mode (and connect to ship's wifi on reboot)
const unsigned long WIFI_TIMEOUT = 20000;  // 20 seconds
const char *AP_PASSWORD_FILE = "/appass.txt";
String esp32_ap_password = "alternator123";  // Default ESP32 AP password
const int FACTORY_RESET_PIN = 35;            // GPIO35 for factory reset
int permanentAPMode = 0;                     // 0 = try client mode, 1 = stay in AP mode
const char *WIFI_MODE_FILE = "/wifimode.txt";

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
// Session and health tracking (add with other globals)
unsigned long sessionStartTime = 0;
unsigned long CurrentWifiSessionDuration = 0;
unsigned long wifiSessionStartTime = 0;
unsigned long lastWifiSessionDuration = 0;  // Duration of last WiFi session (minutes, persistent)
unsigned long LastSessionDuration = 0;      // minutes, persistent
unsigned long CurrentSessionDuration = 0; //minutes
int LastSessionMaxLoopTime = 0;   // milliseconds, persistent
int MaxLoopTime = 0;              // not displayed on client, but available here
int lastSessionMinHeap = 999999;  // KB, persistent
int wifiReconnectsThisSession = 0;    // not persistent 
int wifiReconnectsTotal = 0;  // persistent
int EXTRA;
int LastResetReason;          // why the ESP32 restarted most recently
int ancientResetReason = 0;   // why the ESP32 restarted 2 sessions ago
int sessionMinHeap = 999999;  // Current session minimum
int totalPowerCycles = 0;     // Total number of power cycles (persistent)
// Warning throttling
unsigned long lastHeapWarningTime = 0;
unsigned long lastStackWarningTime = 0;
const unsigned long WARNING_THROTTLE_INTERVAL = 30000;  // 30 seconds

//Console
unsigned long lastConsoleMessageTime = 0;
const unsigned long CONSOLE_MESSAGE_INTERVAL = 2000;  // 60 seconds
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

// WiFi modes
enum WiFiMode {
  AWIFI_MODE_CLIENT,
  AWIFI_MODE_AP
};


WiFiMode currentWiFiMode = AWIFI_MODE_CLIENT;

// Add this new enum after the existing WiFiMode enum
enum OperationalMode {
  CONFIG_AP_MODE,       // First boot configuration
  OPERATIONAL_AP_MODE,  // Permanent AP with full functionality
  CLIENT_MODE           // Connected to ship's WiFi
};

// Add this function after your forward declarations section
OperationalMode getCurrentMode() {
  if (currentWiFiMode == AWIFI_MODE_CLIENT) {
    return CLIENT_MODE;
  } else if (permanentAPMode == 1) {
    return OPERATIONAL_AP_MODE;
  } else {
    return CONFIG_AP_MODE;
  }
}

//little fs monitor
bool littleFSMounted = false;

//delete this later ?
int INADisconnected = 0;
int WifiHeartBeat = 0;
int previousMillisBLINK = 0;  // used for heartbeat blinking LED
int intervalBLINK = 1000;     // used for heartbeat blinking LED interval
bool ledState = false;        // used for heartbeat blinking LED state
int VeTime = 0;
int SendWifiTime = 0;
int AnalogReadTime = 0;        // this is the present
int AnalogReadTime2 = 0;       // this is the maximum ever
int AinputTrackerTime = 5000;  //ms- how long between resetting maximum loop time and analog read times in display


int FlipFlopper = 0;   // delete
int FlipFlopper2 = 0;  // delete

//Input Settings
int TargetAmps = 40;  //Normal alternator output, for best performance, set to something that just barely won't overheat
int TargetAmpL = 25;  //Alternator output in Lo mode
int uTargetAmps = 3;  // the one that gets used as the real target

float TargetFloatVoltage = 13.4;                       // self-explanatory
float FullChargeVoltage = 13.9;                        // this could have been called Target Bulk Voltage to be more clear
float ChargingVoltageTarget = 0;                       // This becomes active target
float VoltageHardwareLimit = FullChargeVoltage + 0.1;  // could make this a setting later, but this should be decently safe
bool inBulkStage = true;
unsigned long bulkCompleteTime = 1000;  // milliseconds
unsigned long bulkCompleteTimer = 0;    // this is a timer, don't change

unsigned long FLOAT_DURATION = 12 * 3600;  // 12 hours in seconds
unsigned long floatStartTime = 0;

float dutyStep = 0.8;  // duty step to adjust field target by, each time the loop runs.  Larger numbers = faster response, less stability
float MaxDuty = 99.0;
float MinDuty = 1.0;       //18
int ManualDutyTarget = 4;  // example manual override value


float FieldAdjustmentInterval = 500;      // The regulator field output is updated once every this many milliseconds
float MinimumFieldVoltage = 1;            // A min value here ensures that engine speed can be measured even with no alternator output commanded.  (This is only enforced when Ignition input is high)
float AlternatorTemperatureLimitF = 150;  // the offset appears to be +40 to +50 to get true max alternator external metal temp, depending on temp sensor installation, so 150 here will produce a metal temp ~200F
int ManualFieldToggle = 1;                // set to 1 to enable manual control of regulator field output, helpful for debugging
//float ManualVoltageTarget = 5;            // voltage target corresponding to the toggle above
int SwitchControlOverride = 1;  // set to 1 for web interface switches to override physical switch panel
int ForceFloat = 0;             // Set to 1 to target 0 amps at battery (not really "float" charging, but we end up call it it that, sorry)
int OnOff = 0;                  // 0 is charger off, 1 is charger On (corresponds to Alternator Enable in Basic Settings)
int Ignition = 1;               // Digital Input      NEED THIS TO HAVE WIFI ON , FOR NOW
int IgnitionOverride = 1;       // to fake the ignition signal w/ software
int HiLow = 1;                  // 0 will be a low setting, 1 a high setting
int AmpSrc = 0;                 // 0=Alt Hall Effect, 1=Battery Shunt, 2=NMEA2K Batt, 3=NMEA2K Alt, 4=NMEA0183 Batt, 5=NMEA0183 Alt, 6=Victron Batt, 7=Other
int LimpHome = 0;               // 1 will set to limp home mode, whatever that gets set up to be
int resolution = 12;            // for OneWire temp sensor measurement
int VeData = 0;                 // Set to 1 if VE serial data exists
int NMEA0183Data = 0;           // Set to 1 if NMEA serial data exists doesn't do anything yet
int NMEA2KData = 1;             // doesn't do anything yet
int NMEA2KVerbose = 0;          // print stuff to serial monitor or not
//Field PWM stuff
float interval = 0.8;   // larger value = faster response but more unstable
float vout = 1;         // needs deleting
const int pwmPin = 32;  // field PWM
//const int pwmChannel = 0;      //0–7 available for high-speed PWM  (ESP32)
const int pwmResolution = 16;  // 16-bit resolution (0–65535)
float fffr = 500;              // this is the switching frequency in Hz, uses Float for wifi reasons
int InvertAltAmps = 1;         // change sign of alternator amp reading
int InvertBattAmps = 0;        // change sign of battery amp reading
uint32_t Freq = 0;             // ESP32 switching Frequency in case we want to report it for debugging

//Variables to store measurements
float ShuntVoltage_mV;                  // Battery shunt voltage from INA228
float Bcur;                             // battery shunt current from INA228
float targetCurrent;                    // This is used in the field adjustment loop, gets set to the desired source of current info (ie battery shunt, alt hall sensor, victron, etc.)
float IBV;                              // Ina 228 battery voltage
float IBVMax = NAN;                     // used to track maximum battery voltage
float dutyCycle;                        // Field outout %--- this is just what's transmitted over Wifi (case sensitive)
float FieldResistance = 2;              // Field resistance in Ohms usually between 2 and 6 Ω, changes 10-20% with temp
float vvout;                            // Calculated field volts (approximate)
float iiout;                            // Calculated field amps (approximate)
float AlternatorTemperatureF = NAN;     // alternator temperature
float MaxAlternatorTemperatureF = NAN;  // maximum alternator temperature
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
double LatitudeNMEA = 0.0;           // GPS Latitude
double LongitudeNMEA = 0.0;          // GPS Longitude
int SatelliteCountNMEA = 0;          // Number of satellites

// //Badass State machine
// const unsigned long ADSConversionDelay = 155;  // 125 ms for 8 SPS but... Recommendation:Raise ADSConversionDelay to 150 or 160 ms — this gives ~20–30% margin without hurting performance much.     Can change this back later
// enum I2CReadStage {
//   I2C_IDLE,
//   I2C_CHECK_INA_ALERT,
//   I2C_READ_INA_BUS,
//   I2C_READ_INA_SHUNT,
//   I2C_PROCESS_INA,
//   I2C_TRIGGER_ADS,
//   I2C_WAIT_ADS,
//   I2C_READ_ADS,
//   I2C_PROCESS_ADS
// };

// I2CReadStage i2cStage = I2C_IDLE;
// uint8_t adsChannel = 0;
// unsigned long i2cStageTimer = 0;
// int16_t Raw = 0;
// unsigned long lastINARead = 0;
// unsigned long start33 = 0;

// // ADS1115
// float Channel0V, Channel1V, Channel2V, Channel3V;
// float BatteryV, MeasuredAmps, RPM;  //Readings from ADS1115
// //float voltage;                      // This is the one that gets populated by dropdown menu selection (battery voltage source)
// float MeasuredAmpsMax;  // used to track maximum alternator output
// float RPMMax;           // used to track maximum RPM
// int ADS1115Disconnected = 0;

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
int DataSaveInterval = 60000;             // Save data every 1 minutes, good for 20 year durability of flash memory MIGHT NEED LOAD LEVELING TO ACHIEVE THIS LATER
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
int GPIO33_Status;                               // for alarm mirror light on Client

//More Settings
// SOC Parameters
int CurrentThreshold = 1;             // Ignore currents below this (A × 100)
int PeukertExponent_scaled = 105;     // Peukert exponent × 100 (112 = 1.12)
int ChargeEfficiency_scaled = 99;     // Charging efficiency % (0-100)
int ChargedVoltage_Scaled = 1450;     // Voltage threshold for "charged" (V × 100)
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


//Pointless Flags delete later
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
int AlternatorPower_scaled = 0;    // Alternator power (W × 100)
int AltEnergyDelta_scaled = 0;     // Alternator energy change (Wh × 100)
int joulesOut = 0;
int fuelEnergyUsed_J = 0;
int AlternatorFuelUsed = 0;   // Total fuel used by alternator (mL) - INTEGER (note: mL not L)
bool alternatorIsOn = false;  // Current alternator state
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

//"Blink without delay" style timer variables used to control how often differnet parts of code execute
static unsigned long prev_millis4;   //  Not Needed?
static unsigned long prev_millis66;  //used to delay the updating of the display
static unsigned long prev_millis22;  // used to delay field adjustment
static unsigned long prev_millis3;   // used to delay sampling of ADS1115 to every 2 seconds for example. //change for state machine
//static unsigned long prev_millis2;    // used to delay sampling of temp sensor to every 2 seconds for example
static unsigned long prev_millis33;    // used to delay sampling of Serial Data (ve direct)
static unsigned long prev_millis743;   // used to read NMEA2K Network Every X seconds
static unsigned long prev_millis5;     // used to initiate wifi data exchange
static unsigned long lastINARead = 0;  // don't read the INA228 needlessly often

// Global variable to track ESP32 restart time
unsigned long lastRestartTime = 0;
const unsigned long RESTART_INTERVAL = 3600000;  // 1 hour in milliseconds = 3600000 so 900000 this is 15 mins

int BatteryVoltageSource = 0;  // select  "0">INA228    value="1">ADS1115     value="2">VictronVeDirect     value="3">NMEA0183     value="4">NMEA2K
int AmpControlByRPM = 0;       // this is the toggle
int BatteryCurrentSource = 0;  // 0=INA228, 1=NMEA2K Batt, 2=NMEA0183 Batt, 3=Victron Batt
int timeAxisModeChanging = 0;  // toggle the time axis on and off in Plots.  Off = less janky but less info
// In lieu of a table for RPM based charging....
int RPM1 = 0;
int RPM2 = 500;
int RPM3 = 800;
int RPM4 = 1000;
int Amps1 = 0;
int Amps2 = 20;
int Amps3 = 30;
int Amps4 = 40;
int RPM5 = 1200;
int RPM6 = 1500;
int RPM7 = 4000;
int Amps5 = 40;
int Amps6 = 50;
int Amps7 = 30;
int RPMThreshold = -20000;  //below this, there will be no field output in auto mode (Update this if we have RPM at low speeds and no field, otherwise, depend on Ignition)

int maxPoints;                //number of points plotted per plot (X axis length)
int Ymin1 = 3, Ymax1 = 5;     // Current plot
int Ymin2 = 10, Ymax2 = 15;   // Voltage plot
int Ymin3 = 0, Ymax3 = 4000;  // RPM plot
int Ymin4 = 0, Ymax4 = 250;   // Temperature plot

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

// pre-setup stuff
// onewire    Data wire is connetec to the Arduino digital pin 13
#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;

//Display
// OLED pin mapping from RJ45 → ESP32
#define OLED_CS 5      // RJ45 Pin 15
#define OLED_DC 19     // RJ45 Pin 14
#define OLED_RESET 27  // RJ45 Pin 13
// SSD1306 OLED using 4-wire SPI, full framebuffer
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, OLED_CS, OLED_DC, OLED_RESET);
bool displayAvailable = false;  // Global flag to track if display is working

//VictronEnergy
VeDirectFrameHandler myve;

// WIFI STUFF
AsyncWebServer server(80);                  // Create AsyncWebServer object on port 80
AsyncEventSource events("/events");         // Create an Event Source on /events
unsigned long webgaugesinterval = 50;       // delay in ms between sensor updates on webpage
int plotTimeWindow = 10;                    // Plot time window in seconds
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

Stream *OutputStream;

//ADS1115 more pre-setup crap
enum ADS1115_State {
  ADS_IDLE,
  ADS_WAITING_FOR_CONVERSION
};

ADS1115_State adsState = ADS_IDLE;
uint8_t adsCurrentChannel = 0;
unsigned long adsStartTime = 0;
const unsigned long ADSConversionDelay = 155;  // 125 ms for 8 SPS but... Recommendation:Raise ADSConversionDelay to 150 or 160 ms — this gives ~20–30% margin without hurting performance much.     Can change this back later


// Forward declarations for WiFi functions
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void setupWiFi();
bool connectToWiFi(const char *ssid, const char *password, unsigned long timeout);
void setupAccessPoint();
void setupWiFiConfigServer();
void dnsHandleRequest();
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);

// HTML for the WiFi configuration page
// Replace your existing WIFI_CONFIG_HTML with this final working version:

const char WIFI_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<title>WiFi Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:Arial;padding:20px;background:#f5f5f5}
.card{background:white;padding:20px;border-radius:8px;max-width:400px;margin:0 auto}
h1{color:#333;margin-bottom:20px;text-align:center}
input,select{width:100%;padding:8px;margin:5px 0;border:1px solid #ddd;border-radius:4px}
button{background:#00a19a;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;width:100%}
button:hover{background:#008c86}
.radio-group{margin:10px 0}
.radio-group input{width:auto;margin-right:5px}
.info-box{background:#e8f4f8;border:1px solid #bee5eb;color:#0c5460;padding:12px;border-radius:4px;margin:10px 0;font-size:14px}
</style>
<script>
function toggleHotspotOptions() {
  const mode = document.querySelector('input[name="mode"]:checked').value;
  const clientFields = document.getElementById('client-options');
  if (mode === 'client') {
    clientFields.style.display = 'block';
  } else {
    clientFields.style.display = 'none';
  }
}
</script>
</head><body>
<div class="card">
<h1>WiFi Setup</h1>
<form action="/wifi" method="POST">

<div class="radio-group">
<label><strong>Connection Mode:</strong></label><br>
<input type="radio" name="mode" value="client" checked onchange="toggleHotspotOptions()"> Connect to ship's existing WiFi Network<br>
<input type="radio" name="mode" value="ap" onchange="toggleHotspotOptions()"> Alternator Controller will create a new and independent hotspot
</div>

<label>New Hotspot Name (SSID):</label>
<input type="text" id="hotspot_ssid" name="hotspot_ssid" placeholder="No entry = keep default: ALTERNATOR_WIFI">

<label>New Hotspot Password:</label>
<input type="password" name="ap_password" placeholder="No entry = keep default: alternator123">

<div id="client-options">
<label>Ship's WiFi Name (SSID):</label>
<input type="text" id="ssid" name="ssid" placeholder="Leave blank if using hotspot mode">

<label>Ship's WiFi Password:</label>
<input type="password" name="password" placeholder="Leave blank if using hotspot mode">
</div>

<div class="info-box">
On saving, the controller will disconnect from ALTERNATOR_WIFI, causing this page to hang up.  Cancel / close after 10 seconds.<br>
Reconnect to your selected network to access the full user interface.
</div>

<button type="submit">Save Configuration</button>
</form>
</div>
</body></html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  initializeNVS();
  RestoreLastSessionValues();  // just used for ESP32 stats from last session
  if (!ensureLittleFS()) {
    Serial.println("CRITICAL: Cannot continue without filesystem");
    while (true)
      ;  // halt
  }
  sessionStartTime = millis();
  captureResetReason();  // immediately capture the reason for last ESP32 shutdown and store in LittleFS and variable that won't be overwritten until next boot
  loadNVSData();         // Load persistent variables from NVS- everything from last session is restored
  //Reset some parameters to zero since we are re-starting on a re-boot
  CurrentSessionDuration = 0;
  MaxLoopTime = 0;
  sessionMinHeap = 99999;
  totalPowerCycles++;    // Increment power cycle counter
  saveNVSData();  // Save immediately to persist the adjustments done above in setup so far
  setCpuFrequencyMhz(240);
  pinMode(4, OUTPUT);     // This pin is used to provide a high signal to Field Enable pin
  digitalWrite(4, LOW);   // Start with field off
  pinMode(2, OUTPUT);     // This pin is used to provide a heartbeat (pin 2 of ESP32 is the LED)
  pinMode(39, INPUT);     // Ignition
  pinMode(33, OUTPUT);    // Alarm/Buzzer output
  digitalWrite(33, LOW);  // Start with alarm off
  // PWM setup (needed for basic operation)
  ledcAttach(pwmPin, fffr, pwmResolution);
  InitSystemSettings();  // load all settings from LittleFS.  If no files exist, create them.
  loadPasswordHash();
  setupWiFi();  // NOW setup WiFi with all settings properly loaded
  esp_log_level_set("esp32-hal-i2c-ng", ESP_LOG_WARN);
  queueConsoleMessage("System starting up...");
  initializeHardware();  // Initialize hardware systems
  Serial.println("=== SETUP COMPLETE ===");
  // Enable watchdog - only after setup is complete
  Serial.println("Enabling watchdog protection...");
  // What Happens During Watchdog Reset:
  //Watchdog triggers (after 15 seconds of hang)
  //ESP32 immediately reboots (hardware reset)
  //ALL GPIO pins reset to 0 (including pin 4 field enable)
  //Field control pin 4 goes LOW → Field turns OFF
  //Alternator output stops → Batteries safe
  //ESP32 restarts and runs setup()
  //Normal operation resumes
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,   // 15 seconds in milliseconds
    .idle_core_mask = 0,   // Don't monitor idle cores
    .trigger_panic = true  // Reboot on timeout
  };
  esp_task_wdt_init(&wdt_config);  // Initialize with config
  esp_task_wdt_add(NULL);          // Add main loop task to monitoring
  queueConsoleMessage("Watchdog enabled: 15 second timeout for safety");
}

void loop() {
  esp_task_wdt_reset();              // Feed the watchdog to prevent timeout
  starttime = esp_timer_get_time();  // Record start time for Loop
  currentTime = millis();

  Ignition = 1;  // Bypass power management for AP mode testing FIX THIS LATER

  // SOC and runtime update every 2 seconds
  if (currentTime - lastSOCUpdateTime >= SOCUpdateInterval) {
    CurrentSessionDuration = (millis() - sessionStartTime) / 1000/60; //minutes
    elapsedMillis = currentTime - lastSOCUpdateTime;
    lastSOCUpdateTime = currentTime;
    UpdateEngineRuntime(elapsedMillis);
    UpdateBatterySOC(elapsedMillis);
    handleSocGainReset();  // do the dynamic updates
    handleAltZeroReset();  // do the dynamic udpates
  }
  // Periodic Data Save Logic - save NVS data every 10 seconds
  if (currentTime - lastDataSaveTime >= 10000) {  // 10 seconds - NVS can handle frequent writes
    lastDataSaveTime = currentTime;
    saveNVSData();  // Only save current operational data, not session stats
  }
  // New three-way mode handling
  OperationalMode mode = getCurrentMode();
  switch (mode) {
    case CONFIG_AP_MODE:
      // Minimal functionality - just DNS and configuration
      dnsHandleRequest();
      break;
    case OPERATIONAL_AP_MODE:
      // Full functionality - same as client mode
      dnsHandleRequest();  // Still need DNS for AP mode
      // Fall through to full functionality...
    case CLIENT_MODE:
      // Full sensor/data functionality
      ReadAnalogInputs();
      if (VeData == 1) {
        ReadVEData();  //read Data from Victron
      }
      if (NMEA2KData == 1) {
        if (millis() - prev_millis743 > 2000) {  // Only parse every 2 seconds
          NMEA2000.ParseMessages();
          prev_millis743 = millis();
        }
      }
      GPIO33_Status = digitalRead(33);  // Store the reading, this could be obviously simplified later, but whatever
      CheckAlarms();
      calculateThermalStress();  // alternator lifetime modeling
      UpdateDisplay();
      checkAutoZeroTriggers();  //Auto-zero processing (must be before AdjustField)
      processAutoZero();        //Auto-zero processing (must be before AdjustField)
      AdjustField();            // This may need to get moved if it takes any power, but have to be careful we don't get stuck with Field On!
      // Ignition = !digitalRead(39);  // see if ignition is on    (fix this later)
      if (IgnitionOverride == 1) {
        Ignition = 1;
      }
      logDashboardValues();         // just nice to have some history in the Console
      updateSystemHealthMetrics();  // Add after existing health monitoring calls
      // Power management and WiFi logic moved to after the switch statement
      SomeHealthyStuff();  // do what used to be done in SendWifiData
      SendWifiData();
      // Client-specific connection monitoring
      if (mode == CLIENT_MODE) {
        checkWiFiConnection();
      }
      break;
  }
  // Power management section (the ignition logic) goes here after the switch
  if (Ignition == 0) {
    setCpuFrequencyMhz(10);
    WiFi.mode(WIFI_OFF);
    Serial.println("wifi has been turned off");
  } else {
    // Only do client-mode specific WiFi management in client mode
    if (mode == CLIENT_MODE && WiFi.status() != WL_CONNECTED) {
      setCpuFrequencyMhz(240);
      setupWiFi();
      Serial.println("Wifi is reinitialized");
      queueConsoleMessage("Wifi is reinitialized");
    }
    Freq = getCpuFrequencyMhz();  // unused at this time
  }
  //Blink LED on and off every X seconds - works in both power modes for status indication
  // optimize for power consumpiton later
  //AP Mode: Fast triple blink pattern
  //WiFi Disconnected: Medium blink with 100ms pulse
  //WiFi Connected: Normal toggle

  //FIX THIS LATER
  // if (millis() - previousMillisBLINK >= intervalBLINK) {
  //   // Use different blink patterns to indicate WiFi status
  //   if (currentWiFiMode == AWIFI_MODE_AP) {
  //     // Fast blink in AP mode (toggle twice)
  //     digitalWrite(2, HIGH);
  //     delay(50);
  //     digitalWrite(2, LOW);
  //     delay(50);
  //     digitalWrite(2, HIGH);
  //     delay(50);
  //     digitalWrite(2, (ledState = !ledState));
  //   } else if (WiFi.status() != WL_CONNECTED) {
  //     // Medium blink when WiFi is disconnected
  //     digitalWrite(2, HIGH);
  //     delay(100);
  //     digitalWrite(2, (ledState = !ledState));
  //   } else {
  //     // Normal blink when connected
  //     digitalWrite(2, (ledState = !ledState));
  //   }
  //   previousMillisBLINK = millis();
  // }


  if (millis() - prev_millis7888 > AinputTrackerTime) {  // every 5 seconds reset the maximum loop time
    MaximumLoopTime = 0;
    prev_millis7888 = millis();
  }
  endtime = esp_timer_get_time();  //Record end of Loop
  LoopTime = (endtime - starttime);
  if (LoopTime > 5000000) {  // 5 seconds in microseconds
    Serial.print("Loop time was too long: ");
    Serial.println(LoopTime);
    queueConsoleMessage("WARNING: Loop took " + String(LoopTime / 1000) + "ms - potential watchdog risk");
  }
//This one resets every 5 seconds (AinputTrackerTime) and is displayed on client
  if (LoopTime > MaximumLoopTime) {
    MaximumLoopTime = LoopTime;
  }
//This one is persistent, not displayed in current session for Client, but visible on next boot
    if (LoopTime > MaxLoopTime) {
    MaxLoopTime = LoopTime;            
  }

  
  checkAndRestart();  // Handle scheduled maintenance restarts
}



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
