/**
 * AI_SUMMARY: These functions probably are less useful for AI to parse for the problem at hand. There is another file called 2_ImportantFunctions that can be shared if necessary.
 * AI_PURPOSE: 
 * AI_INPUTS: 
 * AI_OUTPUTS: 
 * AI_DEPENDENCIES: 
 * AI_RISKS: 
 * CRITICAL_INSTRUCTION_FOR_AI:: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat. When impossible, always mimick my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instructions for me to edit if function is long, to conserve tokens. Always specify which option you chose.
 */


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



//*****************************************************************************



void Rudder(const tN2kMsg& N2kMsg) {
  unsigned char Instance;
  tN2kRudderDirectionOrder RudderDirectionOrder;
  double RudderPosition;
  double AngleOrder;

  if (ParseN2kRudder(N2kMsg, RudderPosition, Instance, RudderDirectionOrder, AngleOrder)) {
    //Uncomment below to get serial montior back
    // PrintLabelValWithConversionCheckUnDef("Rudder: ", Instance, 0, true);
    // PrintLabelValWithConversionCheckUnDef("  position (deg): ", RudderPosition, &RadToDeg, true);
    // OutputStream->print("  direction order: ");
    // PrintN2kEnumType(RudderDirectionOrder, OutputStream);
    // PrintLabelValWithConversionCheckUnDef("  angle order (deg): ", AngleOrder, &RadToDeg, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void Heading(const tN2kMsg& N2kMsg) {
  static unsigned long lastHeadingUpdate = 0;
  if (millis() - lastHeadingUpdate < 2000) return;  // Throttle to once every 2 seconds
  lastHeadingUpdate = millis();
  unsigned char SID;
  tN2kHeadingReference HeadingReference;
  double Heading;
  double Deviation;
  double Variation;

  if (ParseN2kHeading(N2kMsg, SID, Heading, Deviation, Variation, HeadingReference)) {
    // Parsing succeeded - update variable and mark fresh
    HeadingNMEA = Heading * 180.0 / PI;  // Convert radians to degrees
    MARK_FRESH(IDX_HEADING_NMEA);        // Only called on successful parse

    // Uncomment below to get serial monitor output back
    // OutputStream->println("Heading:");
    // PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
    // OutputStream->print("  reference: ");
    // PrintN2kEnumType(HeadingReference, OutputStream);
    // PrintLabelValWithConversionCheckUnDef("  Heading (deg): ", Heading, &RadToDeg, true);
    // PrintLabelValWithConversionCheckUnDef("  Deviation (deg): ", Deviation, &RadToDeg, true);
    // PrintLabelValWithConversionCheckUnDef("  Variation (deg): ", Variation, &RadToDeg, true);
  } else {
    // Parsing failed - do NOT call MARK_FRESH, data will go stale automatically
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void COGSOG(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  tN2kHeadingReference HeadingReference;
  double COG;
  double SOG;

  if (ParseN2kCOGSOGRapid(N2kMsg, SID, HeadingReference, COG, SOG)) {
    OutputStream->println("COG/SOG:");
    PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
    OutputStream->print("  reference: ");
    PrintN2kEnumType(HeadingReference, OutputStream);
    PrintLabelValWithConversionCheckUnDef("  COG (deg): ", COG, &RadToDeg, true);
    PrintLabelValWithConversionCheckUnDef("  SOG (m/s): ", SOG, 0, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************



void GNSS(const tN2kMsg& N2kMsg) {
  //Serial.println("=== GNSS function called ===");
  //Serial.printf("PGN: %lu\n", N2kMsg.PGN);
  static unsigned long lastGNSSUpdate = 0;
  if (millis() - lastGNSSUpdate < 2000) return;  // Throttle to once every 2 seconds
  lastGNSSUpdate = millis();

  unsigned char SID;
  uint16_t DaysSince1970;
  double SecondsSinceMidnight;
  double Latitude;
  double Longitude;
  double Altitude;
  tN2kGNSStype GNSStype;
  tN2kGNSSmethod GNSSmethod;
  unsigned char nSatellites;
  double HDOP;
  double PDOP;
  double GeoidalSeparation;
  unsigned char nReferenceStations;
  tN2kGNSStype ReferenceStationType;
  uint16_t ReferenceStationID;
  double AgeOfCorrection;

  if (ParseN2kGNSS(N2kMsg, SID, DaysSince1970, SecondsSinceMidnight,
                   Latitude, Longitude, Altitude,
                   GNSStype, GNSSmethod,
                   nSatellites, HDOP, PDOP, GeoidalSeparation,
                   nReferenceStations, ReferenceStationType, ReferenceStationID,
                   AgeOfCorrection)) {

    //Serial.println("GNSS parsing SUCCESS!");
    //Serial.printf("Raw values - Lat: %f, Lon: %f, Sats: %d\n", Latitude, Longitude, nSatellites);

    // Check if we have valid GPS data (not NaN and reasonable values)
    if (!isnan(Latitude) && !isnan(Longitude) && Latitude != 0.0 && Longitude != 0.0 && abs(Latitude) <= 90.0 && abs(Longitude) <= 180.0 && nSatellites > 0) {

      //Store values globally for web interface
      LatitudeNMEA = Latitude;
      LongitudeNMEA = Longitude;
      SatelliteCountNMEA = nSatellites;

      // Mark all GPS data as fresh - only called on valid data
      MARK_FRESH(IDX_LATITUDE_NMEA);
      MARK_FRESH(IDX_LONGITUDE_NMEA);
      MARK_FRESH(IDX_SATELLITE_COUNT);

      //Serial.printf("Valid GNSS data stored - LatNMEA: %f, LonNMEA: %f, SatNMEA: %d\n", LatitudeNMEA, LongitudeNMEA, SatelliteCountNMEA);
    } else {
      //Serial.println("GNSS data invalid - values are NaN, zero, or out of range");
      // Invalid data - do NOT call MARK_FRESH, let data go stale
    }
  } else {
    //Serial.println("GNSS parsing FAILED!");
    // Parse failed - do NOT call MARK_FRESH, let data go stale
  }
}
//*****************************************************************************
void GNSSSatsInView(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  tN2kRangeResidualMode Mode;
  uint8_t NumberOfSVs;
  tSatelliteInfo SatelliteInfo;

  if (ParseN2kPGNSatellitesInView(N2kMsg, SID, Mode, NumberOfSVs)) {
    OutputStream->println("Satellites in view: ");
    OutputStream->print("  mode: ");
    OutputStream->println(Mode);
    OutputStream->print("  number of satellites: ");
    OutputStream->println(NumberOfSVs);
    for (uint8_t i = 0; i < NumberOfSVs && ParseN2kPGNSatellitesInView(N2kMsg, i, SatelliteInfo); i++) {
      OutputStream->print("  Satellite PRN: ");
      OutputStream->println(SatelliteInfo.PRN);
      PrintLabelValWithConversionCheckUnDef("    elevation: ", SatelliteInfo.Elevation, &RadToDeg, true, 1);
      PrintLabelValWithConversionCheckUnDef("    azimuth:   ", SatelliteInfo.Azimuth, &RadToDeg, true, 1);
      PrintLabelValWithConversionCheckUnDef("    SNR:       ", SatelliteInfo.SNR, 0, true, 1);
      PrintLabelValWithConversionCheckUnDef("    residuals: ", SatelliteInfo.RangeResiduals, 0, true, 1);
      OutputStream->print("    status: ");
      OutputStream->println(SatelliteInfo.UsageStatus);
    }
  }
}
//*****************************************************************************
void BatteryConfigurationStatus(const tN2kMsg& N2kMsg) {
  unsigned char BatInstance;
  tN2kBatType BatType;
  tN2kBatEqSupport SupportsEqual;
  tN2kBatNomVolt BatNominalVoltage;
  tN2kBatChem BatChemistry;
  double BatCapacity;
  int8_t BatTemperatureCoefficient;
  double PeukertExponent;
  int8_t ChargeEfficiencyFactor;

  if (ParseN2kBatConf(N2kMsg, BatInstance, BatType, SupportsEqual, BatNominalVoltage, BatChemistry, BatCapacity, BatTemperatureCoefficient, PeukertExponent, ChargeEfficiencyFactor)) {
    PrintLabelValWithConversionCheckUnDef("Battery instance: ", BatInstance, 0, true);
    OutputStream->print("  - type: ");
    PrintN2kEnumType(BatType, OutputStream);
    OutputStream->print("  - support equal.: ");
    PrintN2kEnumType(SupportsEqual, OutputStream);
    OutputStream->print("  - nominal voltage: ");
    PrintN2kEnumType(BatNominalVoltage, OutputStream);
    OutputStream->print("  - chemistry: ");
    PrintN2kEnumType(BatChemistry, OutputStream);
    PrintLabelValWithConversionCheckUnDef("  - capacity (Ah): ", BatCapacity, &CoulombToAh, true);
    PrintLabelValWithConversionCheckUnDef("  - temperature coefficient (%): ", BatTemperatureCoefficient, 0, true);
    PrintLabelValWithConversionCheckUnDef("  - peukert exponent: ", PeukertExponent, 0, true);
    PrintLabelValWithConversionCheckUnDef("  - charge efficiency factor (%): ", ChargeEfficiencyFactor, 0, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void DCStatus(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  unsigned char DCInstance;
  tN2kDCType DCType;
  unsigned char StateOfCharge;
  unsigned char StateOfHealth;
  double TimeRemaining;
  double RippleVoltage;
  double Capacity;

  if (ParseN2kDCStatus(N2kMsg, SID, DCInstance, DCType, StateOfCharge, StateOfHealth, TimeRemaining, RippleVoltage, Capacity)) {
    OutputStream->print("DC instance: ");
    OutputStream->println(DCInstance);
    OutputStream->print("  - type: ");
    PrintN2kEnumType(DCType, OutputStream);
    OutputStream->print("  - state of charge (%): ");
    OutputStream->println(StateOfCharge);
    OutputStream->print("  - state of health (%): ");
    OutputStream->println(StateOfHealth);
    OutputStream->print("  - time remaining (h): ");
    OutputStream->println(TimeRemaining / 60);
    OutputStream->print("  - ripple voltage: ");
    OutputStream->println(RippleVoltage);
    OutputStream->print("  - capacity: ");
    OutputStream->println(Capacity);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}
//*****************************************************************************
void Speed(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  double SOW;
  double SOG;
  tN2kSpeedWaterReferenceType SWRT;

  if (ParseN2kBoatSpeed(N2kMsg, SID, SOW, SOG, SWRT)) {
    OutputStream->print("Boat speed:");
    PrintLabelValWithConversionCheckUnDef(" SOW:", N2kIsNA(SOW) ? SOW : msToKnots(SOW));
    PrintLabelValWithConversionCheckUnDef(", SOG:", N2kIsNA(SOG) ? SOG : msToKnots(SOG));
    OutputStream->print(", ");
    PrintN2kEnumType(SWRT, OutputStream, true);
  }
}
//*****************************************************************************
void WaterDepth(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  double DepthBelowTransducer;
  double Offset;

  if (ParseN2kWaterDepth(N2kMsg, SID, DepthBelowTransducer, Offset)) {
    if (N2kIsNA(Offset) || Offset == 0) {
      PrintLabelValWithConversionCheckUnDef("Depth below transducer", DepthBelowTransducer);
      if (N2kIsNA(Offset)) {
        OutputStream->println(", offset not available");
      } else {
        OutputStream->println(", offset=0");
      }
    } else {
      if (Offset > 0) {
        OutputStream->print("Water depth:");
      } else {
        OutputStream->print("Depth below keel:");
      }
      if (!N2kIsNA(DepthBelowTransducer)) {
        OutputStream->println(DepthBelowTransducer + Offset);
      } else {
        OutputStream->println(" not available");
      }
    }
  }
}
//*****************************************************************************
void Attitude(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  double Yaw;
  double Pitch;
  double Roll;

  if (ParseN2kAttitude(N2kMsg, SID, Yaw, Pitch, Roll)) {
    OutputStream->println("Attitude:");
    PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
    PrintLabelValWithConversionCheckUnDef("  Yaw (deg): ", Yaw, &RadToDeg, true);
    PrintLabelValWithConversionCheckUnDef("  Pitch (deg): ", Pitch, &RadToDeg, true);
    PrintLabelValWithConversionCheckUnDef("  Roll (deg): ", Roll, &RadToDeg, true);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}

//*****************************************************************************
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg& N2kMsg) {
  int iHandler;
  // Find handler
  if (NMEA2KVerbose == 1) {
    OutputStream->print("In Main Handler: ");
    OutputStream->println(N2kMsg.PGN);
  }
  for (iHandler = 0; NMEA2000Handlers[iHandler].PGN != 0 && !(N2kMsg.PGN == NMEA2000Handlers[iHandler].PGN); iHandler++)
    ;

  if (NMEA2000Handlers[iHandler].PGN != 0) {
    NMEA2000Handlers[iHandler].Handler(N2kMsg);
  }
}

void ReadVEData() {
  if (VeData != 1) {
    return;
  }
  static unsigned long lastVEDataRead = 0;
  const unsigned long VE_DATA_INTERVAL = 2000;  // 2 seconds

  unsigned long currentTime = millis();

  // Check if it's time to read VE data
  if (currentTime - lastVEDataRead <= VE_DATA_INTERVAL) {
    return;
  }

  int start1 = micros();      // Start timing VeData
  bool dataReceived = false;  // Track if we got any valid data

  while (Serial1.available()) {
    myve.rxData(Serial1.read());
    for (int i = 0; i < myve.veEnd; i++) {
      if (strcmp(myve.veName[i], "V") == 0) {
        float newVoltage = (atof(myve.veValue[i]) / 1000);
        if (newVoltage > 0 && newVoltage < 100) {  // Sanity check
          VictronVoltage = newVoltage;
          MARK_FRESH(IDX_VICTRON_VOLTAGE);  // Only mark fresh on valid data
          dataReceived = true;
        }
      }
      if (strcmp(myve.veName[i], "I") == 0) {
        float newCurrent = (atof(myve.veValue[i]) / 1000);
        if (newCurrent > -1000 && newCurrent < 1000) {  // Sanity check
          VictronCurrent = newCurrent;
          MARK_FRESH(IDX_VICTRON_CURRENT);  // Only mark fresh on valid data
          dataReceived = true;
        }
      }
    }
    yield();  // Allow other processes to run
  }

  int end1 = micros();     // End timing
  VeTime = end1 - start1;  // Store elapsed time
  lastVEDataRead = currentTime;
}
void printBasicTaskStackInfo() {
  numTasks = uxTaskGetNumberOfTasks();
  if (numTasks > MAX_TASKS) numTasks = MAX_TASKS;  // Prevent overflow

  tasksCaptured = uxTaskGetSystemState(taskArray, numTasks, NULL);

  // Serial.println(F("\n===== TASK STACK REMAINING (BYTES) ====="));
  // Serial.println(F("Task Name        | Core | Stack Remaining | Alert"));

  char coreIdBuffer[8];  // Buffer for core ID display

  for (int i = 0; i < tasksCaptured; i++) {
    const char* taskName = taskArray[i].pcTaskName;
    stackBytes = taskArray[i].usStackHighWaterMark * sizeof(StackType_t);
    core = taskArray[i].xCoreID;

    // Format core ID
    if (core < 0 || core > 16) {
      snprintf(coreIdBuffer, sizeof(coreIdBuffer), "N/A");
    } else {
      snprintf(coreIdBuffer, sizeof(coreIdBuffer), "%d", core);
    }

    const char* alert = "";
    if (strcmp(taskName, "IDLE0") == 0 || strcmp(taskName, "IDLE1") == 0 || strcmp(taskName, "ipc0") == 0 || strcmp(taskName, "ipc1") == 0) {
      int totalStack = 1024;  // Known from sdkconfig output
      int percentRemaining = (stackBytes * 100) / totalStack;
      if (stackBytes < 100) {
        alert = "CRITICAL <10% of 1024B";  // True emergency
      } else if (stackBytes < 200) {
        alert = "LOW <20% of 1024B";  // Monitor but normal
      } else {
        alert = "";  // Normal operation
      }
    } else {
      if (stackBytes < 256) {
        alert = "LOW STACK";
      } else if (stackBytes < 512) {
        alert = "WARN";
      }
    }

    // Serial.printf("%-16s |  %-3s  |     %5d B     | %s\n",
    //               taskName,
    //               coreIdBuffer,
    //               stackBytes,
    //               alert);
  }

  // Serial.println(F("==========================================\n"));
}
void printHeapStats() {
  rawFreeHeap = esp_get_free_heap_size();                                 // in bytes
  FreeHeap = rawFreeHeap / 1024;                                          // in KB
  MinFreeHeap = esp_get_minimum_free_heap_size() / 1024;                  // in KB
  FreeInternalRam = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;  // in KB

  if (rawFreeHeap == 0) {
    Heapfrag = 100;
  } else {
    Heapfrag = 100 - ((heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) * 100) / rawFreeHeap);
  }

  // Serial.println(F("========== HEAP STATS =========="));
  // Serial.printf("Free Heap:               %5u KB\n", FreeHeap);
  // Serial.printf("Minimum Ever Free Heap:  %5u KB\n", MinFreeHeap);
  // Serial.printf("Free Internal RAM:       %5u KB\n", FreeInternalRam);
  // Serial.printf("Heap Fragmentation:      %5u %%\n", Heapfrag);
  // Serial.println(F("================================"));
}
void updateCpuLoad() {
  const int MAX_TASKS = 20;
  TaskStatus_t taskSnapshot[MAX_TASKS];
  UBaseType_t taskCount = uxTaskGetSystemState(taskSnapshot, MAX_TASKS, NULL);
  unsigned long idle0Time = 0;
  unsigned long idle1Time = 0;
  unsigned long now = millis();
  for (int i = 0; i < taskCount; i++) {
    if (strcmp(taskSnapshot[i].pcTaskName, "IDLE0") == 0) {
      idle0Time = taskSnapshot[i].ulRunTimeCounter;
    } else if (strcmp(taskSnapshot[i].pcTaskName, "IDLE1") == 0) {
      idle1Time = taskSnapshot[i].ulRunTimeCounter;
    }
  }
  if (lastCheckTime == 0) {
    lastIdle0Time = idle0Time;
    lastIdle1Time = idle1Time;
    lastCheckTime = now;
    return;
  }
  unsigned long deltaIdle0 = idle0Time - lastIdle0Time;
  unsigned long deltaIdle1 = idle1Time - lastIdle1Time;
  unsigned long timeDiff = now - lastCheckTime;
  if (timeDiff == 0) return;
  // Fixed calculation - using your existing variables
  cpuLoadCore0 = 100 - ((deltaIdle0 * 100) / (timeDiff * 100));
  cpuLoadCore1 = 100 - ((deltaIdle1 * 100) / (timeDiff * 100));
  // Ensure values stay in valid range
  if (cpuLoadCore0 < 0) cpuLoadCore0 = 0;
  if (cpuLoadCore0 > 100) cpuLoadCore0 = 100;
  if (cpuLoadCore1 < 0) cpuLoadCore1 = 0;
  if (cpuLoadCore1 > 100) cpuLoadCore1 = 100;
  lastIdle0Time = idle0Time;
  lastIdle1Time = idle1Time;
  lastCheckTime = now;
  // Print CPU load directly
  // Serial.printf("CPU Load: Core 0 = %3d%%, Core 1 = %3d%%\n", cpuLoadCore0, cpuLoadCore1);
}
void testTaskStats() {
  char statsBuffer[1024];  // Enough for 15–20 tasks

  vTaskGetRunTimeStats(statsBuffer);

  // Serial.println(F("========== TASK CPU USAGE =========="));
  // Serial.println(statsBuffer);
  // Serial.println(F("====================================\n"));
}
void SomeHealthyStuff() {
  static unsigned long prev_millis544 = 0;
  unsigned long now544 = millis();
  if (now544 - prev_millis544 > healthystuffinterval) {
    printHeapStats();           //   Should be ~25–65 µs with no serial prints
    printBasicTaskStackInfo();  //Should be ~70–170 µs µs for 10 tasks (conservative estimate with no serial prints)
    // updateCpuLoad();            //~200–250 for 10 tasks    // This function allocates 800 bytes on the stack, should be rewritten to malloc if it's ever needed again
    testTaskStats();  //unknown
    prev_millis544 = now544;
  }
}
void updateSystemHealthMetrics() {
  unsigned long now = millis();

  // Critical heap warnings (throttled)
  if (FreeHeap < 20 && (now - lastHeapWarningTime > WARNING_THROTTLE_INTERVAL)) {
    queueConsoleMessage("CRITICAL: Heap dangerously low (" + String(FreeHeap) + "KB) - system may crash");
    lastHeapWarningTime = now;
  } else if (FreeHeap < 50 && (now - lastHeapWarningTime > WARNING_THROTTLE_INTERVAL)) {
    queueConsoleMessage("WARNING: Low heap memory (" + String(FreeHeap) + "KB) - monitor closely");
    lastHeapWarningTime = now;
  }

  // Stack warnings for all tasks (throttled)
  if (now - lastStackWarningTime > WARNING_THROTTLE_INTERVAL) {
    bool stackWarningIssued = false;
    for (int i = 0; i < tasksCaptured && i < MAX_TASKS; i++) {
      int stackBytes = taskArray[i].usStackHighWaterMark * sizeof(StackType_t);
      const char* taskName = taskArray[i].pcTaskName;

      // Updated thresholds based on expert analysis
      if (stackBytes < 128) {  // Changed from 256
        queueConsoleMessage("CRITICAL: " + String(taskName) + " stack very low (" + String(stackBytes) + "B)");
        stackWarningIssued = true;
      } else if (stackBytes < 200 && (  // Only warn for system tasks below 200B
                   strcmp(taskName, "IDLE0") == 0 || strcmp(taskName, "IDLE1") == 0 || strcmp(taskName, "ipc0") == 0 || strcmp(taskName, "ipc1") == 0)) {
        queueConsoleMessage("INFO: " + String(taskName) + " stack lean (" + String(stackBytes) + "B) - normal operation");
        stackWarningIssued = true;
      }
    }
    if (stackWarningIssued) {
      lastStackWarningTime = now;
    }
  }
}
void checkAndRestart() {
  //Restart the ESP32 every hour just for maintenance
  unsigned long currentMillis = millis();
  // Check if millis() has rolled over (happens after ~49.7 days)
  if (currentMillis < lastRestartTime) {
    lastRestartTime = 0;  // Reset on overflow
  }
  // Check if it's time to restart
  if (currentMillis - lastRestartTime >= RESTART_INTERVAL) {
    events.send("Performing scheduled restart for system maintenance", "console");
    events.send("Device restarting for maintenance. Will reconnect shortly.", "status");

    delay(2500);
    ESP.restart();
  }
}
void captureResetReason() {
  // Load the last session's reset reason (reason for closing down 2 sessions ago)
  ancientResetReason = readFile(LittleFS, "/LastResetReason.txt").toInt();
  // Read raw hardware reset reason as integer (this is why it closed last session)
  int rawReason = (int)esp_reset_reason();
  // Convert raw ESP32 code to compact application-specific code
  switch (rawReason) {
    case ESP_RST_SW: LastResetReason = 1; break;
    case ESP_RST_DEEPSLEEP: LastResetReason = 2; break;
    case ESP_RST_EXT: LastResetReason = 3; break;
    case ESP_RST_TASK_WDT: LastResetReason = 4; break;
    case ESP_RST_PANIC: LastResetReason = 5; break;
    case ESP_RST_BROWNOUT: LastResetReason = 6; break;
    default: LastResetReason = 8; break;
  }
  // Save most recent reset reason to file for restoration next session
  writeFile(LittleFS, "/LastResetReason.txt", String(LastResetReason).c_str());
}


void UpdateBatterySOC(unsigned long elapsedMillis) {
  // Convert elapsed milliseconds to seconds for calculations
  float elapsedSeconds = elapsedMillis / 1000.0f;

  // =================================================================
  //     BATTERY STATE MONITORING -
  // =================================================================
  // This section tracks battery state of charge from ALL sources:

  // Get current battery voltage and current readings
  float currentBatteryVoltage = getBatteryVoltage();
  Voltage_scaled = currentBatteryVoltage * 100;  // V × 100 for precision

  // Get battery current for SoC tracking (uses dedicated battery source)
  float batteryCurrentForSoC = getBatteryCurrent();
  BatteryCurrent_scaled = batteryCurrentForSoC * 100;  // A × 100 for precision

  // Calculate battery power (positive = charging, negative = discharging)
  BatteryPower_scaled = (Voltage_scaled * BatteryCurrent_scaled) / 100;  // W × 100

  // Energy calculations - convert power to energy over time
  float batteryPower_W = BatteryPower_scaled / 100.0f;
  float energyDelta_Wh = (batteryPower_W * elapsedSeconds) / 3600.0f;  // Power × time = energy

  // Energy accumulation with floating point precision
  static float chargedEnergyAccumulator = 0.0f;
  static float dischargedEnergyAccumulator = 0.0f;

  if (BatteryCurrent_scaled > 0) {
    // Charging - energy going INTO battery
    chargedEnergyAccumulator += energyDelta_Wh;
    if (chargedEnergyAccumulator >= 1.0f) {
      ChargedEnergy += (int)chargedEnergyAccumulator;
      chargedEnergyAccumulator -= (int)chargedEnergyAccumulator;
    }
  } else if (BatteryCurrent_scaled < 0) {
    // Discharging - energy coming OUT of battery
    dischargedEnergyAccumulator += abs(energyDelta_Wh);
    if (dischargedEnergyAccumulator >= 1.0f) {
      DischargedEnergy += (int)dischargedEnergyAccumulator;
      dischargedEnergyAccumulator -= (int)dischargedEnergyAccumulator;
    }
  }

  // =================================================================
  //     COULOMB COUNTING - BATTERY STATE OF CHARGE TRACKING
  // =================================================================
  // Track actual amp-hours into/out of battery with efficiency corrections

  static float coulombAccumulator_Ah = 0.0f;
  float batteryCurrent_A = BatteryCurrent_scaled / 100.0f;
  float deltaAh = (batteryCurrent_A * elapsedSeconds) / 3600.0f;  // A × hours = Ah

  if (BatteryCurrent_scaled >= 0) {
    // Charging - apply charge efficiency (not all energy makes it in)
    float batteryDeltaAh = deltaAh * (ChargeEfficiency_scaled / 100.0f);
    coulombAccumulator_Ah += batteryDeltaAh;
  } else {
    // Discharging - apply Peukert compensation for high discharge rates
    float dischargeCurrent_A = abs(batteryCurrent_A);
    float peukertThreshold = BatteryCapacity_Ah / 100.0f;  // C/100 threshold

    if (dischargeCurrent_A > peukertThreshold) {
      // High discharge rate - apply Peukert equation
      float peukertExponent = PeukertExponent_scaled / 100.0f;
      dischargeCurrent_A = constrain(dischargeCurrent_A, 0, BatteryCapacity_Ah);  // Max 1C
      float currentRatio = PeukertRatedCurrent_A / dischargeCurrent_A;
      float peukertFactor = pow(currentRatio, peukertExponent - 1.0f);
      peukertFactor = constrain(peukertFactor, 0.5f, 2.0f);  // Sanity limits
      float batteryDeltaAh = deltaAh * peukertFactor;
      coulombAccumulator_Ah += batteryDeltaAh;
    } else {
      // Low discharge rate - no Peukert compensation needed
      coulombAccumulator_Ah += deltaAh;
    }
  }

  // Update SoC when we've accumulated enough change (0.01 Ah threshold)
  if (abs(coulombAccumulator_Ah) >= 0.01f) {
    int deltaAh_scaled = (int)(coulombAccumulator_Ah * 100.0f);
    CoulombCount_Ah_scaled += deltaAh_scaled;
    coulombAccumulator_Ah -= (deltaAh_scaled / 100.0f);  // Keep remainder
  }

  // Calculate State of Charge percentage with bounds checking
  CoulombCount_Ah_scaled = constrain(CoulombCount_Ah_scaled, 0, BatteryCapacity_Ah * 100);
  float SoC_float = (float)CoulombCount_Ah_scaled / (BatteryCapacity_Ah * 100.0f) * 100.0f;
  SOC_percent = (int)(SoC_float * 100);  // Store as percentage × 100 for 2 decimals

  // =================================================================
  //     FULL CHARGE DETECTION - WORKS FROM ANY CHARGING SOURCE
  // =================================================================
  // When battery voltage is high AND current is low (tail current),
  // we know it's fully charged regardless of what's charging it

  if ((abs(BatteryCurrent_scaled) <= (TailCurrent * BatteryCapacity_Ah / 100)) && (Voltage_scaled >= ChargedVoltage_Scaled)) {
    // Conditions met - start/continue timer
    FullChargeTimer += elapsedSeconds;

    if (FullChargeTimer >= ChargedDetectionTime) {
      // Timer expired - battery is definitely full
      SOC_percent = 10000;  // 100.00%
      CoulombCount_Ah_scaled = BatteryCapacity_Ah * 100;
      FullChargeDetected = true;
      coulombAccumulator_Ah = 0.0f;
      queueConsoleMessage("BATTERY: Full charge detected - SoC reset to 100%");

      // Apply shunt gain correction ONLY if using INA228 battery shunt
      if (AutoShuntGainCorrection == 1 && AmpSrc == 1) {
        applySocGainCorrection();
      }
    }
  } else {
    // Conditions not met - reset timer
    FullChargeTimer = 0;
    FullChargeDetected = false;
  }

  // =================================================================
  //     ALTERNATOR-SPECIFIC TRACKING - ONLY RUNS WHEN ALT IS ON
  // =================================================================
  // These metrics are ONLY about the alternator's contribution

  // Determine if alternator is actually producing current
  alternatorIsOn = (AlternatorCurrent_scaled > CurrentThreshold * 100);

  if (alternatorIsOn) {
    // Track alternator runtime (seconds → AlternatorOnTime)
    alternatorOnAccumulator += elapsedMillis;
    if (alternatorOnAccumulator >= 1000) {  // Every 1 second
      int secondsRun = alternatorOnAccumulator / 1000;
      AlternatorOnTime += secondsRun;
      alternatorOnAccumulator %= 1000;  // Keep remainder milliseconds
    }

    alternatorWasOn = alternatorIsOn;  // State tracking

    // Calculate alternator energy output
    static float alternatorEnergyAccumulator = 0.0f;
    float alternatorPower_W = (currentBatteryVoltage * MeasuredAmps);
    float altEnergyDelta_Wh = (alternatorPower_W * elapsedSeconds) / 3600.0f;

    if (altEnergyDelta_Wh > 0) {
      alternatorEnergyAccumulator += altEnergyDelta_Wh;
      if (alternatorEnergyAccumulator >= 1.0f) {
        AlternatorChargedEnergy += (int)alternatorEnergyAccumulator;
        alternatorEnergyAccumulator -= (int)alternatorEnergyAccumulator;
      }

      // =================================================================
      //     FUEL CONSUMPTION CALCULATION - ALTERNATOR SPECIFIC
      // =================================================================
      // Calculate diesel fuel used to produce this electrical energy
      // Chain: Diesel → Engine (30% eff) → Alternator (50% eff) → Electricity

      float energyJoules = altEnergyDelta_Wh * 3600.0f;  // Convert Wh to Joules
      const float engineEfficiency = 0.30f;              // Engine: fuel → mechanical (30%)
      const float alternatorEfficiency = 0.50f;          // Alt: mechanical → electrical (50%)

      // Total system efficiency = 0.30 × 0.50 = 0.15 (15%)
      float fuelEnergyUsed_J = energyJoules / (engineEfficiency * alternatorEfficiency);

      const float dieselEnergy_J_per_mL = 36000.0f;  // Energy content of diesel
      float fuelUsed_mL = fuelEnergyUsed_J / dieselEnergy_J_per_mL;

      // Accumulate fuel (prevents losing fractional mL)
      static float fuelAccumulator = 0.0f;
      fuelAccumulator += fuelUsed_mL;
      if (fuelAccumulator >= 1.0f) {
        AlternatorFuelUsed += (int)fuelAccumulator;
        fuelAccumulator -= (int)fuelAccumulator;
      }
    }
  }
}
void UpdateEngineRuntime(unsigned long elapsedMillis) {
  // Check if engine is running (RPM > 100)
  bool engineIsRunning = (RPM > 100 && RPM < 6000);

  if (engineIsRunning) {
    // Accumulate running time in milliseconds
    engineRunAccumulator += elapsedMillis;

    // Update total engine run time every second
    if (engineRunAccumulator >= 1000) {  // 1 second in milliseconds
      int secondsRun = engineRunAccumulator / 1000;
      EngineRunTime += secondsRun;

      // Update engine cycles (RPM * seconds / 60)
      EngineCycles += (RPM * secondsRun) / 60;

      // Keep the remainder milliseconds
      engineRunAccumulator %= 1000;
    }
  }

  // Update engine state
  engineWasRunning = engineIsRunning;
}
float getBatteryCurrent() {
  // Function specifically for battery monitoring (SoC, energy tracking)
  // Only uses battery current sources, never alternator sources

  float batteryCurrent = 0;
  static unsigned long lastWarningTime = 0;
  const unsigned long WARNING_INTERVAL = 10000;

  switch (BatteryCurrentSource) {
    case 0:  // INA228 Battery Shunt (default)
      if (INADisconnected == 0 && !isnan(Bcur)) {
        batteryCurrent = Bcur;  // Already has BatteryCOffset and dynamic gain applied
      } else {
        if (millis() - lastWarningTime > WARNING_INTERVAL) {
          queueConsoleMessage("C0- WARNING: INA228 unavailable for battery monitoring");
          lastWarningTime = millis();
        }
        batteryCurrent = 0;  // Safe default for SoC tracking
      }
      break;

    case 1:  // NMEA2K Battery
      queueConsoleMessage("C1-NMEA2K Battery current not implemented, using INA228");
      batteryCurrent = Bcur;
      if (1 == 1) {  // update this later
        batteryCurrent = -batteryCurrent;
      }
      break;

    case 2:  // NMEA0183 Battery
      if (millis() - lastWarningTime > WARNING_INTERVAL) {

        queueConsoleMessage("C2-NMEA0183 Battery current not implemented, using INA228");
        lastWarningTime = millis();
      }
      batteryCurrent = Bcur;
      if (1 == 1) {  // update this later
        batteryCurrent = -batteryCurrent;
      }
      break;

    case 3:  // Victron Battery
      if (abs(VictronCurrent) > 0.1) {
        batteryCurrent = VictronCurrent;
        // No inversion for external sources - they handle their own polarity
      } else {
        if (millis() - lastWarningTime > WARNING_INTERVAL) {
          queueConsoleMessage("Victron current not available, using INA228");
          lastWarningTime = millis();
        }
        batteryCurrent = Bcur;
        if (1 == 1) {  // update this later
          batteryCurrent = -batteryCurrent;
        }
      }
      break;

    default:
      queueConsoleMessage("Invalid BatteryCurrentSource, using INA228");
      batteryCurrent = Bcur;
      if (1 == 1) {  // update this later
        batteryCurrent = -batteryCurrent;
      }
      break;
  }

  return batteryCurrent;
}
float getBatteryVoltage() {
  static unsigned long lastWarningTime = 0;
  const unsigned long WARNING_INTERVAL = 10000;  // 10 seconds between warnings
  float selectedVoltage = 0;
  switch (BatteryVoltageSource) {
    case 0:  // INA228
             // if (INADisconnected == 0 && !isnan(IBV) && IBV > 8.0 && IBV < 70.0 && millis() > 5000) {  // the 5000 was causing timing issues on startup (millis was not >5000 yet so it always defaulted to ADS1115)
      if (INADisconnected == 0 && !isnan(IBV) && IBV > 8.0 && IBV < 70.0) {
        selectedVoltage = IBV;
      } else {
        if (millis() - lastWarningTime > WARNING_INTERVAL) {
          queueConsoleMessage("WARNING: INA228 unavailable (disconnected:" + String(INADisconnected) + " value:" + String(IBV, 2) + "), falling back to ADS1115");
          lastWarningTime = millis();
        }
        selectedVoltage = BatteryV;
      }
      break;

    case 1:  // ADS1115
      if (ADS1115Disconnected == 0 && !isnan(BatteryV) && BatteryV > 8.0 && BatteryV < 70.0) {
        selectedVoltage = BatteryV;
      } else {
        if (millis() - lastWarningTime > WARNING_INTERVAL) {
          queueConsoleMessage("WARNING: ADS1115 unavailable (disconnected:" + String(ADS1115Disconnected) + " value:" + String(BatteryV, 2) + "), falling back to INA228");
          lastWarningTime = millis();
        }
        selectedVoltage = IBV;
      }
      break;

    case 2:  // VictronVeDirect
      if (VictronVoltage > 8.0 && VictronVoltage < 70.0) {
        selectedVoltage = VictronVoltage;
      } else {
        if (millis() - lastWarningTime > WARNING_INTERVAL) {
          queueConsoleMessage("WARNING: Invalid Victron voltage (" + String(VictronVoltage, 2) + "V), falling back to INA228");
          lastWarningTime = millis();
        }
        selectedVoltage = IBV;
      }
      break;

    case 3:  // NMEA0183
      if (millis() - lastWarningTime > WARNING_INTERVAL) {
        queueConsoleMessage("NMEA0183 voltage source not implemented, using INA228");
        lastWarningTime = millis();
      }
      selectedVoltage = IBV;
      break;

    case 4:  // NMEA2K
      if (millis() - lastWarningTime > WARNING_INTERVAL) {
        queueConsoleMessage("NMEA2K voltage source not implemented, using INA228");
        lastWarningTime = millis();
      }
      selectedVoltage = IBV;
      break;

    default:
      if (millis() - lastWarningTime > WARNING_INTERVAL) {
        queueConsoleMessage("Invalid battery voltage source (" + String(BatteryVoltageSource) + "), using INA228");
        lastWarningTime = millis();
      }
      selectedVoltage = IBV;
      break;
  }

  // Final validation
  if (selectedVoltage < 8.0 || selectedVoltage > 70.0 || isnan(selectedVoltage)) {
    if (millis() - lastWarningTime > WARNING_INTERVAL) {
      queueConsoleMessage("CRITICAL: No valid battery voltage found! Using 999 default.");
      lastWarningTime = millis();
    }
    selectedVoltage = 999;  // This needs updating later, but this will effectively shut off the field
  }

  return selectedVoltage;
}
float getTargetAmps() {
  float targetValue = 0;
  static unsigned long lastTargetAmpsWarning = 0;

  switch (AmpSrc) {
    case 0:  // Alt Hall Effect Sensor
      targetValue = MeasuredAmps;
      break;

    case 1:  // Battery Shunt
      targetValue = Bcur;
      break;

    case 2:  // NMEA2K Batt
      if (millis() - lastTargetAmpsWarning > 10000) {
        queueConsoleMessage("C2a NMEA2K Battery current not implemented, using Battery Shunt");
        lastTargetAmpsWarning = millis();
      }
      targetValue = Bcur;
      break;

    case 3:  // NMEA2K Alt
      if (millis() - lastTargetAmpsWarning > 10000) {
        queueConsoleMessage("C3a NMEA2K Alternator current not implemented, using Alt Hall Sensor");
        lastTargetAmpsWarning = millis();
      }
      targetValue = MeasuredAmps;
      break;

    case 4:  // NMEA0183 Batt
      if (millis() - lastTargetAmpsWarning > 10000) {
        queueConsoleMessage("C4a NMEA0183 Battery current not implemented, using Battery Shunt");
        lastTargetAmpsWarning = millis();
      }
      targetValue = Bcur;
      break;

    case 5:  // NMEA0183 Alt
      if (millis() - lastTargetAmpsWarning > 10000) {
        queueConsoleMessage("C5a NMEA0183 Alternator current not implemented, using Alt Hall Sensor");
        lastTargetAmpsWarning = millis();
      }
      targetValue = MeasuredAmps;
      break;

    case 6:                             // Victron Batt
      if (abs(VictronCurrent) > 0.1) {  // Valid reading
        targetValue = VictronCurrent;
      } else {
        if (millis() - lastTargetAmpsWarning > 10000) {
          queueConsoleMessage("C6a Victron current not available, using Battery Shunt");
          lastTargetAmpsWarning = millis();
        }
        targetValue = Bcur;
      }
      break;

    case 7:  // Other
      targetValue = MeasuredAmps;
      break;

    default:
      if (millis() - lastTargetAmpsWarning > 10000) {
        queueConsoleMessage("Invalid AmpSrc (" + String(AmpSrc) + "), using Alt Hall Sensor");
        lastTargetAmpsWarning = millis();
      }
      targetValue = MeasuredAmps;
      break;
  }

  return targetValue;
}
int thermistorTempC(float V_thermistor) {
  float Vcc = 5.0;
  float R_thermistor = R_fixed * (V_thermistor / (Vcc - V_thermistor));
  float T0_K = T0_C + 273.15;
  float tempK = 1.0 / ((1.0 / T0_K) + (1.0 / Beta) * log(R_thermistor / R0));
  return (int)(tempK - 273.15);  // Cast to int for whole degrees
}
void CheckAlarms() {
  static unsigned long lastRunTime = 0;
  if (millis() - lastRunTime < 250) return;  // Only run every 250ms
  lastRunTime = millis();

  static bool previousAlarmState = false;
  bool currentAlarmCondition = false;
  bool outputAlarmState = false;
  String alarmReason = "";

  // Handle alarm test - this can trigger regardless of AlarmActivate
  if (AlarmTest == 1) {
    if (alarmTestStartTime == 0) {
      alarmTestStartTime = millis();
      queueConsoleMessage("ALARM TEST: Testing buzzer for 2 seconds");
    }

    if (millis() - alarmTestStartTime < ALARM_TEST_DURATION) {
      currentAlarmCondition = true;  // Treat test as a real alarm condition
      alarmReason = "Alarm test active";
    } else {
      // Test completed, reset
      AlarmTest = 0;
      alarmTestStartTime = 0;
      queueConsoleMessage("ALARM TEST: Complete");
    }
  }

  // Normal alarm checking - ONLY when AlarmActivate is enabled
  if (AlarmActivate == 1) {
    // Set TempToUse based on source (ADD THIS)
    if (TempSource == 0) {
      TempToUse = AlternatorTemperatureF;
    } else if (TempSource == 1) {
      TempToUse = temperatureThermistor;
    }
    // Temperature alarm
    if (TempAlarm > 0 && TempToUse > TempAlarm) {
      currentAlarmCondition = true;
      alarmReason = "High alternator temperature: " + String(TempToUse) + "°F (limit: " + String(TempAlarm) + "°F)";
    }

    // Voltage alarms
    float currentVoltage = getBatteryVoltage();
    if (VoltageAlarmHigh > 0 && currentVoltage > VoltageAlarmHigh) {
      currentAlarmCondition = true;
      alarmReason = "High battery voltage: " + String(currentVoltage, 2) + "V (limit: " + String(VoltageAlarmHigh) + "V)";
    }

    if (VoltageAlarmLow > 0 && currentVoltage < VoltageAlarmLow && currentVoltage > 8.0) {
      currentAlarmCondition = true;
      alarmReason = "Low battery voltage: " + String(currentVoltage, 2) + "V (limit: " + String(VoltageAlarmLow) + "V)";
    }

    // Current alarm (alternator)
    if (CurrentAlarmHigh > 0 && MeasuredAmps > CurrentAlarmHigh) {
      currentAlarmCondition = true;
      alarmReason = "High alternator current: " + String(MeasuredAmps, 1) + "A (limit: " + String(CurrentAlarmHigh) + "A)";
    }
    // Current alarm (battery)
    if (MaximumAllowedBatteryAmps > 0 && (Bcur) > MaximumAllowedBatteryAmps) {
      currentAlarmCondition = true;
      alarmReason = "High battery current: " + String(abs(Bcur), 1) + "A (limit: " + String(MaximumAllowedBatteryAmps) + "A)";
    }
  }


  // INA228 hardware overvoltage latch
  uint16_t alertStatus = readINA228AlertRegister(INA.getAddress());
  if (!inaOvervoltageLatched && (alertStatus & 0x0080)) {
    inaOvervoltageLatched = true;
    inaOvervoltageTime = millis();
    currentAlarmCondition = true;
    alarmReason = "INA228 hardware overvoltage detected!! (Battery exceeded Bulk Voltage setting + 0.1V)";
  }

  // After 10 seconds, try to clear INA228 alert latch
  if (inaOvervoltageLatched && millis() - inaOvervoltageTime >= 10000) {
    clearINA228AlertLatch(INA.getAddress());
    alertStatus = readINA228AlertRegister(INA.getAddress());
    if (!(alertStatus & 0x0080)) {
      inaOvervoltageLatched = false;
      queueConsoleMessage("INA228 ALERT: Overvoltage condition cleared");
    } else {
      currentAlarmCondition = true;  // Still active
      alarmReason = "INA228 hardware overvoltage still present";
    }
  }

  // Handle manual latch reset
  if (ResetAlarmLatch == 1) {
    alarmLatch = false;
    ResetAlarmLatch = 0;
    queueConsoleMessage("ALARM LATCH: Manually reset");
  }

  // Handle latching logic (applies to ALL alarm conditions including test)
  if (AlarmLatchEnabled == 1) {
    // Latch mode: Once alarm trips, stays on until manually cleared
    if (currentAlarmCondition) {
      alarmLatch = true;  // Set latch
    }
    outputAlarmState = alarmLatch;  // Output follows latch state
  } else {
    // Non-latch mode: Output follows current conditions
    outputAlarmState = currentAlarmCondition;
  }

  // Final output control - respects AlarmActivate EXCEPT for test
  bool finalOutput = false;
  if (AlarmTest == 1) {
    // Test always works regardless of AlarmActivate
    finalOutput = outputAlarmState;
  } else if (AlarmActivate == 1) {
    // Normal alarms only work when AlarmActivate is on
    finalOutput = outputAlarmState;
  }
  // else: AlarmActivate is off and not testing = no output

  digitalWrite(21, finalOutput ? HIGH : LOW);  // get rid of this bs
  // digitalWrite(21, finalOutput);     // go back to this later
  // Console messaging
  if (currentAlarmCondition != previousAlarmState) {
    if (currentAlarmCondition) {
      queueConsoleMessage("ALARM ACTIVATED: " + alarmReason);
    } else if (AlarmLatchEnabled == 0) {
      queueConsoleMessage("ALARM CLEARED");
    }
    previousAlarmState = currentAlarmCondition;
  }
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 5000) {  // Every 5 seconds
    lastDebugTime = millis();
    if (AlarmActivate == 1) {
      // queueConsoleMessage("ALARM DEBUG: GPIO33=" + String(digitalRead(33)) + ", AlarmActivate=" + String(AlarmActivate) + ", TempAlarm=" + String(TempAlarm) + ", CurrentTemp=" + String(TempSource == 0 ? AlternatorTemperatureF : temperatureThermistor));
    }
  }
}
void logDashboardValues() {
  static unsigned long lastDashboardLog = 0;
  if (millis() - lastDashboardLog >= 10000) {  // Every 10 seconds
    lastDashboardLog = millis();
    String dashboardLog = "DASHBOARD: ";
    dashboardLog += "IBV=" + String(IBV, 2) + "V ";
    dashboardLog += "SoC=" + String(SOC_percent / 100) + "% ";
    dashboardLog += "AltI=" + String(MeasuredAmps, 1) + "A ";
    dashboardLog += "BattI=" + String(Bcur, 1) + "A ";
    dashboardLog += "AltT=" + String((int)AlternatorTemperatureF) + "°F ";
    dashboardLog += "RPM=" + String((int)RPM);
    queueConsoleMessage(dashboardLog);
  }
}
void updateChargingStage() {
  float currentVoltage = getBatteryVoltage();

  if (inBulkStage) {
    // Currently in bulk charging
    ChargingVoltageTarget = BulkVoltage;

    if (currentVoltage >= ChargingVoltageTarget) {
      if (bulkCompleteTimer == 0) {
        bulkCompleteTimer = millis();                                // Start timer
      } else if (millis() - bulkCompleteTimer > bulkCompleteTime) {  // 1 second above bulk voltage
        inBulkStage = false;
        floatStartTime = millis();
        queueConsoleMessage("CHARGING: Bulk stage complete, switching to float");
      }
    } else {
      bulkCompleteTimer = 0;  // Reset timer if voltage drops
    }
  } else {
    // Currently in float charging
    ChargingVoltageTarget = FloatVoltage;

    // Return to bulk after time expires OR if voltage drops significantly
    if ((millis() - floatStartTime > FLOAT_DURATION * 1000) || (currentVoltage < FloatVoltage - 0.5)) {
      inBulkStage = true;
      bulkCompleteTimer = 0;
      floatStartTime = millis();
      queueConsoleMessage("CHARGING: Returning to bulk stage");
    }
  }
}
void calculateChargeTimes() {
  static unsigned long lastCalcTime = 0;
  unsigned long now = millis();
  if (now - lastCalcTime < AnalogInputReadInterval) return;  // Only run every 2 seconds
  lastCalcTime = now;

  // Get the current amperage based on AmpSrc setting
  float currentAmps = getTargetAmps();

  if (currentAmps > 0.01) {  // charging
    // Calculate remaining capacity needed to reach 100%
    float currentSoC = SOC_percent / 100.0;  // Convert from scaled format
    float remainingCapacity = BatteryCapacity_Ah * (100.0 - currentSoC) / 100.0;
    timeToFullChargeMin = (int)(remainingCapacity / currentAmps * 60.0);
    timeToFullDischargeMin = -999;           // Not applicable while charging
  } else if (currentAmps < -0.01) {          // discharging
    float currentSoC = SOC_percent / 100.0;  // Convert from scaled format
    float availableCapacity = BatteryCapacity_Ah * currentSoC / 100.0;
    timeToFullDischargeMin = (int)(availableCapacity / (-currentAmps) * 60.0);
    timeToFullChargeMin = -999;  // Not applicable while discharging
  } else {
    // No significant current flow
    timeToFullChargeMin = -999;
    timeToFullDischargeMin = -999;
  }
}
void applySocGainCorrection() {
  // Only apply if feature is enabled AND using INA228 battery shunt
  if (AutoShuntGainCorrection == 0 || AmpSrc != 1) {
    return;
  }

  // Check minimum time interval
  unsigned long now = millis();
  if (now - lastGainCorrectionTime < MIN_GAIN_CORRECTION_INTERVAL) {
    queueConsoleMessage("SOC Gain: Correction blocked, too soon since last adjustment");
    return;
  }

  // Calculate actual vs expected capacity
  float expectedCapacity = BatteryCapacity_Ah;
  float calculatedCapacity = CoulombCount_Ah_scaled / 100.0;  // Convert from scaled value

  // Sanity check - make sure we have reasonable values
  if (calculatedCapacity < 10 || expectedCapacity < 10) {
    queueConsoleMessage("SOC Gain: Invalid capacity values, skipping correction");
    return;
  }

  // Calculate error ratio
  float errorRatio = abs(expectedCapacity - calculatedCapacity) / expectedCapacity;

  // Check if error is too large to be reasonable
  if (errorRatio > MAX_REASONABLE_ERROR) {
    queueConsoleMessage("SOC Gain: Error too large (" + String(errorRatio * 100, 1) + "%), ignoring correction");
    return;
  }

  // Calculate desired correction factor
  float desiredCorrectionFactor = expectedCapacity / calculatedCapacity;
  float currentFactor = DynamicShuntGainFactor;
  float newFactor = currentFactor * desiredCorrectionFactor;

  // Limit the adjustment rate per cycle
  float maxChange = currentFactor * MAX_GAIN_ADJUSTMENT_PER_CYCLE;
  if (newFactor > currentFactor + maxChange) {
    newFactor = currentFactor + maxChange;
    queueConsoleMessage("SOC Gain: Correction limited to maximum change rate");
  } else if (newFactor < currentFactor - maxChange) {
    newFactor = currentFactor - maxChange;
    queueConsoleMessage("SOC Gain: Correction limited to maximum change rate");
  }

  // Apply bounds checking
  if (newFactor > MAX_DYNAMIC_GAIN_FACTOR) {
    newFactor = MAX_DYNAMIC_GAIN_FACTOR;
    queueConsoleMessage("SOC Gain: Factor hit maximum limit (" + String(MAX_DYNAMIC_GAIN_FACTOR, 2) + "), check system");
  } else if (newFactor < MIN_DYNAMIC_GAIN_FACTOR) {
    newFactor = MIN_DYNAMIC_GAIN_FACTOR;
    queueConsoleMessage("SOC Gain: Factor hit minimum limit (" + String(MIN_DYNAMIC_GAIN_FACTOR, 2) + "), check system");
  }

  // Apply the correction
  DynamicShuntGainFactor = newFactor;
  lastGainCorrectionTime = now;
  queueConsoleMessage("SOC Gain: Corrected from " + String(currentFactor, 4) + " to " + String(newFactor, 4) + " (Calc:" + String(calculatedCapacity, 1) + "Ah, Expected:" + String(expectedCapacity, 1) + "Ah)");
}
void handleSocGainReset() {
  if (ResetDynamicShuntGain == 1) {
    DynamicShuntGainFactor = 1.0;
    lastGainCorrectionTime = 0;
    ResetDynamicShuntGain = 0;  // Clear the momentary flag
    queueConsoleMessage("SOC Gain: Dynamic gain factor reset to 1.0");
  }
}
void checkAutoZeroTriggers() {
  // Only check if feature is enabled
  if (AutoAltCurrentZero == 0) {
    return;
  }

  // Don't start if already in progress
  if (autoZeroStartTime > 0) {
    return;
  }

  // Make sure engine is running
  if (RPM < 200) {
    return;
  }

  unsigned long now = millis();
  bool shouldTrigger = false;
  String triggerReason = "";

  // Check time-based trigger (1 hour)
  if (now - lastAutoZeroTime >= AUTO_ZERO_INTERVAL) {
    shouldTrigger = true;
    triggerReason = "scheduled interval";
  }

  // Check temperature-based trigger (20°F change)
  float currentTemp = (TempSource == 0) ? AlternatorTemperatureF : temperatureThermistor;
  if (lastAutoZeroTemp > -900 && abs(currentTemp - lastAutoZeroTemp) >= AUTO_ZERO_TEMP_DELTA) {
    shouldTrigger = true;
    triggerReason = "temperature change (" + String(abs(currentTemp - lastAutoZeroTemp), 1) + "°F)";
  }

  if (shouldTrigger) {
    startAutoZero(triggerReason);
  }
}
void startAutoZero(String reason) {
  autoZeroStartTime = millis();
  queueConsoleMessage("Auto-zero: Starting alternator current zeroing (" + reason + ")");
}
void processAutoZero() {
  // Cancel if feature gets disabled during active cycle
  if (AutoAltCurrentZero == 0) {
    if (autoZeroStartTime > 0) {
      autoZeroStartTime = 0;      // Cancel active cycle
      autoZeroAccumulator = 0.0;  // Reset accumulator
      autoZeroSampleCount = 0;
      queueConsoleMessage("Auto-zero: Cancelled - feature disabled");
    }
    return;
  }

  // Only process if auto-zero is active
  if (autoZeroStartTime == 0) {
    return;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - autoZeroStartTime;

  if (elapsed < AUTO_ZERO_DURATION) {
    // Still in zeroing phase - force field to minimum
    dutyCycle = MinDuty;
    setDutyPercent((int)dutyCycle);
    digitalWrite(4, 0);  // Disable field completely for more accurate zero

    // Accumulate readings after 2 second settling time
    if (elapsed > 2000) {
      autoZeroAccumulator += MeasuredAmps;
      autoZeroSampleCount++;
    }
  } else {
    // Zeroing complete - calculate average and restore normal operation
    float currentTemp = (TempSource == 0) ? AlternatorTemperatureF : temperatureThermistor;

    // Calculate average of accumulated samples
    if (autoZeroSampleCount > 0) {
      DynamicAltCurrentZero = autoZeroAccumulator / autoZeroSampleCount;
    } else {
      DynamicAltCurrentZero = MeasuredAmps;  // Fallback to single reading
    }

    lastAutoZeroTime = now;
    lastAutoZeroTemp = currentTemp;
    autoZeroStartTime = 0;      // Clear active flag
    autoZeroAccumulator = 0.0;  // Reset accumulator
    autoZeroSampleCount = 0;
    queueConsoleMessage("Auto-zero: Complete, new zero offset: " + String(DynamicAltCurrentZero, 3) + "A (avg of " + String(autoZeroSampleCount) + " samples)");
  }
}
void handleAltZeroReset() {
  if (ResetDynamicAltZero == 1) {
    DynamicAltCurrentZero = 0.0;
    lastAutoZeroTime = 0;
    lastAutoZeroTemp = -999.0;
    autoZeroStartTime = 0;    // Cancel any active auto-zero
    ResetDynamicAltZero = 0;  // Clear the momentary flag
    queueConsoleMessage("Auto-zero: Dynamic alternator zero offset reset to 0.0");
  }
}
void calculateThermalStress() {
  unsigned long now = millis();
  if (now - lastThermalUpdateTime < THERMAL_UPDATE_INTERVAL) {
    return;  // Not time to update yet
  }

  float elapsedSeconds = (now - lastThermalUpdateTime) / 1000.0f;  //
  lastThermalUpdateTime = now;

  // Calculate component temperatures
  float T_winding_F = TempToUse + WindingTempOffset;
  float T_bearing_F = TempToUse + 40.0f;  //
  float T_brush_F = TempToUse + 50.0f;    //

  // Calculate alternator RPM
  float Alt_RPM = RPM * PulleyRatio;

  // Calculate individual component lives
  float T_winding_K = (T_winding_F - 32.0f) * 5.0f / 9.0f + 273.15f;  //   to all
  float L_insul = L_REF_INSUL * exp(EA_INSULATION / BOLTZMANN_K * (1.0f / T_winding_K - 1.0f / T_REF_K));
  L_insul = min(L_insul, 10000.0f);  //

  float L_grease_base = L_REF_GREASE * pow(0.5f, (T_bearing_F - 158.0f) / 18.0f);  //   to all
  float L_grease = L_grease_base * (6000.0f / max(Alt_RPM, 100.0f));               //
  L_grease = min(L_grease, 10000.0f);                                              //

  float temp_factor = 1.0f + 0.0025f * (T_brush_F - 150.0f);                                //   to all
  float L_brush = (L_REF_BRUSH * 6000.0f / max(Alt_RPM, 100.0f)) / max(temp_factor, 0.1f);  //
  L_brush = min(L_brush, 10000.0f);                                                         //

  // Calculate damage rates (damage per hour)
  float insul_damage_rate = 1.0f / L_insul;    //
  float grease_damage_rate = 1.0f / L_grease;  //
  float brush_damage_rate = 1.0f / L_brush;    //

  // Accumulate damage over elapsed time
  float hours_elapsed = elapsedSeconds / 3600.0f;  //
  CumulativeInsulationDamage += insul_damage_rate * hours_elapsed;
  CumulativeGreaseDamage += grease_damage_rate * hours_elapsed;
  CumulativeBrushDamage += brush_damage_rate * hours_elapsed;

  // Constrain damage to 0-1 range
  CumulativeInsulationDamage = constrain(CumulativeInsulationDamage, 0.0f, 1.0f);  //
  CumulativeGreaseDamage = constrain(CumulativeGreaseDamage, 0.0f, 1.0f);          //
  CumulativeBrushDamage = constrain(CumulativeBrushDamage, 0.0f, 1.0f);            //

  // Calculate remaining life percentages
  InsulationLifePercent = (1.0f - CumulativeInsulationDamage) * 100.0f;  //
  GreaseLifePercent = (1.0f - CumulativeGreaseDamage) * 100.0f;          //
  BrushLifePercent = (1.0f - CumulativeBrushDamage) * 100.0f;            //

  // Calculate predicted life hours (minimum of current rates)
  float min_damage_rate = max({ insul_damage_rate, grease_damage_rate, brush_damage_rate });
  PredictedLifeHours = 1.0f / min_damage_rate;  //

  // Set indicator color
  if (PredictedLifeHours > 5000.0f) {         //
    LifeIndicatorColor = 0;                   // Green
  } else if (PredictedLifeHours > 1000.0f) {  //
    LifeIndicatorColor = 1;                   // Yellow
  } else {
    LifeIndicatorColor = 2;  // Red
  }
}
bool fetchWeatherData() {
  // Validate GPS coordinates
  if (LatitudeNMEA == 0.0 && LongitudeNMEA == 0.0) {
    weatherLastError = "No GPS coordinates available";
    weatherDataValid = 0;
    queueConsoleMessage("GPS: No GPS coordinates available for weather");
    return false;
  }
  if (currentMode != MODE_CLIENT) {
    weatherLastError = "Not in client mode";
    weatherDataValid = 0;
    queueConsoleMessage("Weather: Must be in client mode");
    return false;
  }
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    weatherLastError = "WiFi not connected";
    weatherDataValid = 0;
    queueConsoleMessage("Weather: WiFi not connected");
    return false;
  }
  // Test internet speed before attempting download
  if (!testInternetSpeed()) {
    weatherLastError = "Internet connection too slow or unavailable";
    weatherDataValid = 0;
    nextWeatherUpdate = millis() + 3600000;  // Retry in 1 hour
    return false;
  }
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  String url = "https://api.open-meteo.com/v1/forecast?";
  url += "latitude=" + String(LatitudeNMEA, 6);
  url += "&longitude=" + String(LongitudeNMEA, 6);
  url += "&daily=shortwave_radiation_sum";
  url += "&timezone=auto";
  Serial.println("Open-Meteo Forecast URL: " + url);
  http.begin(client, url);
  http.setTimeout(WeatherTimeoutMs);
  esp_task_wdt_reset();
  int httpResponseCode = http.GET();
  weatherHttpResponseCode = httpResponseCode;
  esp_task_wdt_reset();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    esp_task_wdt_reset();
    queueConsoleMessage("GPS: " + String(LatitudeNMEA, 6) + "," + String(LongitudeNMEA, 6));
    queueConsoleMessage("RAW RESPONSE: " + payload);
    Serial.println("Open-Meteo Response Code: " + String(httpResponseCode));
    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      weatherLastError = "JSON parse error: " + String(error.c_str());
      weatherDataValid = 0;
      Serial.println("Open-Meteo JSON error: " + String(error.c_str()));
      http.end();
      return false;
    }
    // Extract daily shortwave radiation data
    JsonArray radiation = doc["daily"]["shortwave_radiation_sum"];
    if (radiation.size() < 3) {
      weatherLastError = "Insufficient forecast data";
      weatherDataValid = 0;
      http.end();
      return false;
    }
    // Get daily solar irradiance in MJ/m²/day
    float mjToday = radiation[0];
    float mjTomorrow = radiation[1];
    float mjDay2 = radiation[2];
    // Convert MJ/m²/day to kWh using corrected solar panel math
    // Formula: kWh = (MJ/m²/day × 0.278 MJ→kWh) ÷ 1000 × PanelWatts × PerformanceRatio
    pKwHrToday = (mjToday * MJ_TO_KWH_CONVERSION / STC_IRRADIANCE) * SolarWatts * performanceRatio;
    pKwHrTomorrow = (mjTomorrow * MJ_TO_KWH_CONVERSION / STC_IRRADIANCE) * SolarWatts * performanceRatio;
    pKwHr2days = (mjDay2 * MJ_TO_KWH_CONVERSION / STC_IRRADIANCE) * SolarWatts * performanceRatio;
    // Store solar irradiance in UV variables for compatibility (convert MJ to Wh/m²)
    UVToday = mjToday * 278.0f;  // MJ/m²/day to Wh/m²/day
    UVTomorrow = mjTomorrow * 278.0f;
    UVDay2 = mjDay2 * 278.0f;
    weatherLastUpdate = millis();
    weatherDataValid = 1;
    weatherLastError = "Success";
    // Save solar data to LittleFS
    writeFile(LittleFS, "/UVToday.txt", String(UVToday, 1).c_str());
    writeFile(LittleFS, "/UVTomorrow.txt", String(UVTomorrow, 1).c_str());
    writeFile(LittleFS, "/UVDay2.txt", String(UVDay2, 1).c_str());
    writeFile(LittleFS, "/pKwHrToday.txt", String(pKwHrToday, 2).c_str());
    writeFile(LittleFS, "/pKwHrTomorrow.txt", String(pKwHrTomorrow, 2).c_str());
    writeFile(LittleFS, "/pKwHr2days.txt", String(pKwHr2days, 2).c_str());
    writeFile(LittleFS, "/weatherDataValid.txt", "1");
    String logMsg = "Solar forecast - Today: " + String(UVToday, 0) + "Wh/m² (" + String(pKwHrToday, 2) + "kWh), Tomorrow: " + String(UVTomorrow, 0) + "Wh/m² (" + String(pKwHrTomorrow, 2) + "kWh), Day 2: " + String(UVDay2, 0) + "Wh/m² (" + String(pKwHr2days, 2) + "kWh)";
    Serial.println(logMsg);
    queueConsoleMessage(logMsg);
    http.end();
    return true;
  }

  else {
    weatherLastError = "HTTP error: " + String(httpResponseCode);
    weatherDataValid = 0;
    writeFile(LittleFS, "/weatherDataValid.txt", "0");
    Serial.println("Open-Meteo HTTP error: " + String(httpResponseCode));
    // Provide specific error messages
    if (httpResponseCode == 429) {
      weatherLastError = "API rate limit exceeded (429)";
      queueConsoleMessage("Open-Meteo: Rate limit exceeded - try again later");
    } else if (httpResponseCode == -1) {
      weatherLastError = "Connection timeout or failed";
      queueConsoleMessage("Open-Meteo: Connection timeout - check internet");
    } else {
      // NEW: Better error message for other HTTP errors
      queueConsoleMessage("Open-Meteo: HTTP error " + String(httpResponseCode));
    }
    http.end();
    return false;
  }
}

void analyzeWeatherMode() {
  //Analyze weather data and decide alternator mode
  // If weather mode is disabled or the weather data is invalid, it sets the mode to normal (0) and exits.
  // Otherwise, it checks the UV index for today, tomorrow, and the next day.
  //If 2 or more days have a UV index above the configured threshold, it sets the mode to high UV mode (1), which disables the alternator.
  if (!weatherDataValid || !weatherModeEnabled) {  // if weather mode is not enabled, or weather data is invalid
    currentWeatherMode = 0;
    return;
  }
  // Count days above threshold
  int highUVDays = 0;
  if (pKwHrToday >= UVThresholdHigh) highUVDays++;
  if (pKwHrTomorrow >= UVThresholdHigh) highUVDays++;
  if (pKwHr2days >= UVThresholdHigh) highUVDays++;

  if (highUVDays >= 2) {
    currentWeatherMode = 1;  // Disable alternator
  } else {
    currentWeatherMode = 0;  // Normal operation
  }
}
// weather mode update function, called from Loop
void updateWeatherMode() {
  static unsigned long lastWeatherModeCheck = 0;
  unsigned long now = millis();

  // Throttle to once every 2 seconds
  if (now - lastWeatherModeCheck < 2000) return;
  lastWeatherModeCheck = now;

  // Check if it's time for an update
  if (weatherModeEnabled && weatherDataValid && (now - weatherLastUpdate < WeatherUpdateInterval)) {
    //If weather mode is enabled, the weather data is valid, and the time since the last update is less than the update interval, then...
    analyzeWeatherMode();
    queueConsoleMessage("Weather: data still fresh, returning...");
    return;
  }

  if (weatherModeEnabled && (now >= nextWeatherUpdate)) {
    //If weather mode is enabled and the current time is greater than the scheduled time for the next weather update, then...
    Serial.println("Weather: Fetching new data...");
    queueConsoleMessage("Weather: Updating forecast...");

    if (fetchWeatherData()) {
      analyzeWeatherMode();
      nextWeatherUpdate = now + WeatherUpdateInterval;
    } else {
      nextWeatherUpdate = now + 1800000;  // 30 minutes
      currentWeatherMode = 0;
      queueConsoleMessage("Weather update failed: " + weatherLastError);
    }
  }
}
//initialize weather mode settings from LittleFS
void initWeatherModeSettings() {
  // // Load usage count
  // if (!LittleFS.exists("/ApiKeyUsageCount.txt")) {
  //   writeFile(LittleFS, "/ApiKeyUsageCount.txt", "0");
  //   ApiKeyUsageCount = 0;
  // } else {
  //   ApiKeyUsageCount = readFile(LittleFS, "/ApiKeyUsageCount.txt").toInt();
  // }
  // Serial.print("ApiKeyUsageCount: ");
  // Serial.println(ApiKeyUsageCount);


  // if (!LittleFS.exists("/WeatherApiKey.txt")) {
  //   writeFile(LittleFS, "/WeatherApiKey.txt", "047a2182230e702a9b02974d8e5fb7f5");
  //   WeatherApiKey = "047a2182230e702a9b02974d8e5fb7f5";
  // } else {
  //   WeatherApiKey = readFile(LittleFS, "/WeatherApiKey.txt");
  //   WeatherApiKey.trim();
  // }
  // Serial.print("WeatherApiKey: ");
  // Serial.println(WeatherApiKey);

  if (!LittleFS.exists("/weatherModeEnabled.txt")) {
    writeFile(LittleFS, "/weatherModeEnabled.txt", String(weatherModeEnabled).c_str());
  } else {
    weatherModeEnabled = readFile(LittleFS, "/weatherModeEnabled.txt").toInt();
  }

  if (!LittleFS.exists("/UVThresholdHigh.txt")) {
    writeFile(LittleFS, "/UVThresholdHigh.txt", String(UVThresholdHigh, 1).c_str());
  } else {
    UVThresholdHigh = readFile(LittleFS, "/UVThresholdHigh.txt").toFloat();
  }

  if (!LittleFS.exists("/performanceRatio.txt")) {
    writeFile(LittleFS, "/performanceRatio.txt", String(performanceRatio, 1).c_str());
  } else {
    performanceRatio = readFile(LittleFS, "/performanceRatio.txt").toFloat();
  }

  if (!LittleFS.exists("/SolarWatts.txt")) {
    writeFile(LittleFS, "/SolarWatts.txt", String(SolarWatts, 1).c_str());
  } else {
    SolarWatts = readFile(LittleFS, "/SolarWatts.txt").toInt();
  }

  if (!LittleFS.exists("/UVToday.txt")) {
    writeFile(LittleFS, "/UVToday.txt", "0.0");
  } else {
    UVToday = readFile(LittleFS, "/UVToday.txt").toFloat();
  }

  if (!LittleFS.exists("/UVTomorrow.txt")) {
    writeFile(LittleFS, "/UVTomorrow.txt", "0.0");
  } else {
    UVTomorrow = readFile(LittleFS, "/UVTomorrow.txt").toFloat();
  }

  if (!LittleFS.exists("/UVDay2.txt")) {
    writeFile(LittleFS, "/UVDay2.txt", "0.0");
  } else {
    UVDay2 = readFile(LittleFS, "/UVDay2.txt").toFloat();
  }

  if (!LittleFS.exists("/weatherDataValid.txt")) {
    writeFile(LittleFS, "/weatherDataValid.txt", "0");
  } else {
    weatherDataValid = readFile(LittleFS, "/weatherDataValid.txt").toInt();
  }

  if (!LittleFS.exists("/WeatherUpdateInterval.txt")) {
    writeFile(LittleFS, "/WeatherUpdateInterval.txt", String(WeatherUpdateInterval).c_str());
  } else {
    WeatherUpdateInterval = readFile(LittleFS, "/WeatherUpdateInterval.txt").toInt();
  }

  if (!LittleFS.exists("/WeatherTimeoutMs.txt")) {
    writeFile(LittleFS, "/WeatherTimeoutMs.txt", String(WeatherTimeoutMs).c_str());
  } else {
    WeatherTimeoutMs = readFile(LittleFS, "/WeatherTimeoutMs.txt").toInt();
  }
}
//Manual weather update trigger for web interface
void triggerWeatherUpdate() {
  nextWeatherUpdate = 0;  // Force immediate update on next cycle
  queueConsoleMessage("Weather: Manual update triggered");
}
//INA228 functions to compensate for lack of library features related to ALERT pin
uint16_t readINA228AlertRegister(uint8_t i2cAddress) {
  Wire.beginTransmission(i2cAddress);
  Wire.write(0x0B);                                     // DIAG_ALRT (correct register)
  if (Wire.endTransmission(false) != 0) return 0xFFFF;  // keep repeated start
  if (Wire.requestFrom(i2cAddress, (uint8_t)2) != 2) return 0xFFFF;
  return (Wire.read() << 8) | Wire.read();
}
bool clearINA228AlertLatch(uint8_t i2cAddress) {
  // Read CONFIG
  Wire.beginTransmission(i2cAddress);
  Wire.write(0x00);  // CONFIG register
  Wire.endTransmission(false);
  Wire.requestFrom(i2cAddress, (uint8_t)2);
  if (Wire.available() < 2) return false;
  uint16_t config = (Wire.read() << 8) | Wire.read();
  // Set ALERT_LATCH_CLEAR bit (bit 3)
  config |= 0x0008;
  // Write CONFIG back
  Wire.beginTransmission(i2cAddress);
  Wire.write(0x00);
  Wire.write(config >> 8);
  Wire.write(config & 0xFF);
  return Wire.endTransmission() == 0;
}
// Add this function to your important functions file
void checkTempTaskHealth() {
  static unsigned long lastTempHealthCheck = 0;
  unsigned long now = millis();

  // Check every 5 seconds
  if (now - lastTempHealthCheck < 5000) return;
  lastTempHealthCheck = now;

  // Check if TempTask is alive
  if (now - lastTempTaskHeartbeat > TEMP_TASK_TIMEOUT) {
    if (tempTaskHealthy) {  // First time detecting the problem
      tempTaskHealthy = false;
      queueConsoleMessage("CRITICAL: TempTask hung up - task not responding for " + String((now - lastTempTaskHeartbeat) / 1000) + " seconds");

      // Emergency safety action
      if (!IgnoreTemperature) {
        queueConsoleMessage("SAFETY: Reducing field due to temp monitoring failure");
        // Reduce field output as safety measure    ABSTRACT OUT THESE 3 LINES LATER
        digitalWrite(4, 0);      //  disable field
        dutyCycle = MinDuty;     // restart duty cycle from minimum
        digitalWrite(21, HIGH);  // Sound alarm
      }
    }
  } else {
    // TempTask is responding
    if (!tempTaskHealthy) {  // Was unhealthy, now recovered
      tempTaskHealthy = true;
      queueConsoleMessage("TempTask: Recovered and responding normally");
      digitalWrite(21, LOW);  // Clear alarm if only temp issue
      //Later, does the field need to be re-enabled here??? Fix later
    }
  }
}

void updateINA228OvervoltageThreshold() {
  // Only update if INA228 is connected
  if (INADisconnected != 0) {
    queueConsoleMessage("INA228: Cannot update threshold - chip not connected");
    return;
  }

  // Update the hardware limit based on current BulkVoltage setting
  VoltageHardwareLimit = BulkVoltage + 0.1;

  // Calculate threshold in LSB units for INA228 with proper rounding
  const double LSB = 0.003125;                                           // 3.125 mV/LSB
  uint16_t thresholdLSB = (uint16_t)(VoltageHardwareLimit / LSB + 0.5);  // Round instead of truncate

  // Diagnostic messages
  queueConsoleMessage("INA228 threshold calc: " + String(VoltageHardwareLimit, 3) + "V / " + String(LSB, 6) + " = " + String(thresholdLSB) + " LSB");
  queueConsoleMessage("INA228 effective threshold: " + String(thresholdLSB * LSB, 3) + "V");

  // Program overvoltage threshold and clear under-voltage
  INA.setBusOvervoltageTH(thresholdLSB);
  INA.setBusUndervoltageTH(0x0000);  // Clear under-voltage threshold (fix accidental setting)

  // Configure DIAG_ALRT behavior explicitly for predictable operation
  INA.clearDiagnoseAlertBit(INA228_DIAG_SLOW_ALERT);      // Compare on instantaneous (non-averaged) readings for immediate response
  INA.clearDiagnoseAlertBit(INA228_DIAG_ALERT_LATCH);     // Transparent mode - alerts clear when condition clears
  INA.clearDiagnoseAlertBit(INA228_DIAG_ALERT_POLARITY);  // Active-low open-drain (default)
  INA.setDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT);    // Enable BUSOL reporting

  // Verify what was actually written to the chip
  uint16_t readback_BOVL = INA.getBusOvervoltageTH();
  uint16_t readback_BUVL = INA.getBusUndervoltageTH();

  queueConsoleMessage("INA228 readback: BOVL=0x" + String(readback_BOVL, HEX) + " (" + String(readback_BOVL * LSB, 3) + "V), BUVL=0x" + String(readback_BUVL, HEX));

  // Verify writes were successful
  if (readback_BOVL != thresholdLSB) {
    queueConsoleMessage("WARNING: BOVL write failed - expected 0x" + String(thresholdLSB, HEX) + ", got 0x" + String(readback_BOVL, HEX));
  }
  if (readback_BUVL != 0) {
    queueConsoleMessage("WARNING: BUVL not cleared - still 0x" + String(readback_BUVL, HEX));
  }

  queueConsoleMessage("INA228: Overvoltage threshold updated to " + String(VoltageHardwareLimit, 2) + "V");
}

void checkWebFilesExist() {
  const char* criticalFiles[] = {
    "/index.html",
    "/styles.css",
    "/script.js",
    "/uPlot.min.css",
    "/uPlot.iife.min.js"
  };

  int missingCount = 0;
  Serial.println("=== CHECKING WEB FILES ===");

  // First ensure web filesystem is mounted
  if (!ensureWebFS()) {
    Serial.println("ERROR: Web filesystem not mounted!");
    queueConsoleMessage("CRITICAL: Web filesystem mount failed!");
    return;
  }

  for (int i = 0; i < 5; i++) {
    if (!webFS.exists(criticalFiles[i])) {
      Serial.printf("MISSING: %s\n", criticalFiles[i]);
      missingCount++;
    } else {
      Serial.printf("Found: %s\n", criticalFiles[i]);
    }
  }

  if (missingCount > 0) {
    Serial.println("===============================================");
    Serial.println("ERROR: Missing web interface files!");
    Serial.printf("Missing %d critical files from web partition\n", missingCount);
    Serial.println("Web files should be in factory_fs or prod_fs partition");
    Serial.println("Currently using: " + String(usingFactoryWebFiles ? "factory_fs" : "prod_fs"));
    Serial.println("===============================================");

    queueConsoleMessage("CRITICAL: " + String(missingCount) + " web files missing from web partition!");

    // Optional: Flash LED or sound alarm to make it obvious
    for (int i = 0; i < 10; i++) {
      digitalWrite(2, HIGH);
      delay(200);
      digitalWrite(2, LOW);
      delay(200);
    }
  } else {
    Serial.println("All critical web files found");
    Serial.println("Using web files from: " + String(usingFactoryWebFiles ? "factory_fs" : "prod_fs"));
    queueConsoleMessage("Web interface files verified OK");
  }
}

void initializeHardware() {  // Helper function to organize hardware initialization

  Serial.println("Starting hardware initialization...");
  // Force I2C initialization with correct pins
  Wire.end();  // End any existing I2C
  Wire.begin(9, 10);
  delay(100);
  Serial.println("I2C initialized on SDA=9, SCL=10");
  delay(100);  // Give I2C time to initialize

  //BMP390
  if (!bmp.begin_I2C(BMP3_ADDR)) {
    Serial.println("BMP3XX not found");
    while (1) delay(1000);
  }
  // Optional: configure oversampling and filter
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);

  //NMEA2K
  OutputStream = &Serial;
  //   while (!Serial)
  //  NMEA2000.SetN2kCANReceiveFrameBufSize(50); // was commented
  // Do not forward bus messages at all
  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
  NMEA2000.SetForwardStream(OutputStream);
  // Set false below, if you do not want to see messages parsed to HEX withing library
  NMEA2000.EnableForward(false);  // was false
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
  //  NMEA2000.SetN2kCANMsgBufSize(2);
  NMEA2000.Open();
  Serial.println("NMEA2K Running...");


  //Victron VeDirect and NMEA0183
  Serial1.begin(19200, SERIAL_8N1, 7, -1, 1);  // This is the reading of Victron VEDirect
  Serial2.begin(19200, SERIAL_8N1, 6, -1, 0);  // ... note the "0" at end for normal logic.  This is the reading of the combined NMEA0183 data from YachtDevices
  Serial2.flush();                             // why don't i do this for Serial 1??

  //INA228
  if (!INA.begin()) {
    Serial.println("Could not connect INA. Fix and Reboot");
    queueConsoleMessage("WARNING: Could not connect INA228 Battery Voltage/Amp measuring chip");
    INADisconnected = 1;
    // while (1)
    ;
  } else {
    INADisconnected = 0;
  }
  // at least 529ms for an update with these settings for average and conversion time
  INA.setMode(11);                       // Bh = Continuous shunt and bus voltage
  INA.setAverage(4);                     //0h = 1, 1h = 4, 2h = 16, 3h = 64, 4h = 128, 5h = 256, 6h = 512, 7h = 1024     Applies to all channels
  INA.setBusVoltageConversionTime(7);    // Sets the conversion time of the bus voltage measurement: 0h = 50 µs, 1h = 84 µs, 2h = 150 µs, 3h = 280 µs, 4h = 540 µs, 5h = 1052 µs, 6h = 2074 µs, 7h = 4120 µs
  INA.setShuntVoltageConversionTime(7);  // Sets the conversion time of the bus voltage measurement: 0h = 50 µs, 1h = 84 µs, 2h = 150 µs, 3h = 280 µs, 4h = 540 µs, 5h = 1052 µs, 6h = 2074 µs, 7h = 4120 µs
  // Set initial overvoltage threshold
  updateINA228OvervoltageThreshold();
  queueConsoleMessage("INA228 Direct Hardware Overvoltage Protection Enabled");

  // if (setupDisplay()) {
  //   Serial.println("Display ready for use");
  // } else {
  //   Serial.println("Continuing without display");
  // }

  unsigned long now = millis();
  for (int i = 0; i < MAX_DATA_INDICES; i++) {
    dataTimestamps[i] = now;  // Start with current time
  }

  //ADS1115
  //Connection check
  if (!adc.testConnection()) {
    Serial.println("ADS1115 Connection failed and would have triggered a return if it wasn't commented out");
    queueConsoleMessage("WARNING: ADS1115 Analog Input chip failed");
    ADS1115Disconnected = 1;
    // return;
  } else {
    ADS1115Disconnected = 0;
  }
  //Gain parameter.
  adc.setGain(ADS1115_REG_CONFIG_PGA_6_144V);
  // ADS1115_REG_CONFIG_PGA_6_144V(0x0000)  // +/-6.144V range = Gain 2/3
  // ADS1115_REG_CONFIG_PGA_4_096V(0x0200)  // +/-4.096V range = Gain 1
  // ADS1115_REG_CONFIG_PGA_2_048V(0x0400)  // +/-2.048V range = Gain 2 (default)
  // ADS1115_REG_CONFIG_PGA_1_024V(0x0600)  // +/-1.024V range = Gain 4
  // ADS1115_REG_CONFIG_PGA_0_512V(0x0800)  // +/-0.512V range = Gain 8
  // ADS1115_REG_CONFIG_PGA_0_256V(0x0A00)  // +/-0.256V range = Gain 16
  //Sample rate parameter
  // adc.setSampleRate(ADS1115_REG_CONFIG_DR_8SPS);  //Set the slowest and most accurate sample rate, 8 THIS WAS PRIOR SETTING
  // ADS1115_REG_CONFIG_DR_8SPS(0x0000)              // 8 SPS(Sample per Second), or a sample every 125ms
  // ADS1115_REG_CONFIG_DR_16SPS(0x0020)             // 16 SPS, or every 62.5ms
  // ADS1115_REG_CONFIG_DR_32SPS(0x0040)             // 32 SPS, or every 31.3ms
  adc.setSampleRate(ADS1115_REG_CONFIG_DR_64SPS);  // (0x0060)             // 64 SPS, or every 15.6ms
                                                   //The ADS1115 hardware does internal averaging - slower conversion times = more internal averaging = less noise. So we want the slowest rate that still meets your 10 readings/second requirement.
                                                   //State machine reality:
                                                   //Triggers conversion on current channel
                                                   //Waits ADSConversionDelay
                                                   //Reads result, moves to next channel
                                                   //Cycles through 4 channels before returning to MeasuredAmps
                                                   //Hardware timing constraints:
                                                   //8 SPS = 125ms actual conversion time
                                                   //16 SPS = 62.5ms actual conversion time
                                                   //32 SPS = 31.3ms actual conversion time
                                                   //You can't set ADSConversionDelay shorter than the actual hardware conversion time - the chip won't be done yet.
                                                   //For 10 MeasuredAmps readings/second:
                                                   //Need MeasuredAmps every 100ms
                                                   //With 4 channels: 100ms ÷ 4 = 25ms per channel
                                                   //So you need ≤25ms conversion time
                                                   //32 SPS (31.3ms) is too slow
                                                   //64 SPS (15.6ms) works - gives maximum averaging while meeting timing
  // ADS1115_REG_CONFIG_DR_128SPS(0x0080)            // 128 SPS, or every 7.8ms  (default)
  // ADS1115_REG_CONFIG_DR_250SPS(0x00A0)            // 250 SPS, or every 4ms, note that noise free resolution is reduced to ~14.75-16bits, see table 2 in datasheet
  // ADS1115_REG_CONFIG_DR_475SPS(0x00C0)            // 475 SPS, or every 2.1ms, note that noise free resolution is reduced to ~14.3-15.5bits, see table 2 in datasheet
  // ADS1115_REG_CONFIG_DR_860SPS(0x00E0)            // 860 SPS, or every 1.16ms, note that noise free resolution is reduced to ~13.8-15bits, see table 2 in datasheet

  delay(100);  // Give chip time to configure
  if (!adc.testConnection()) {
    Serial.println("ADS1115 failed after configuration");
  } else {
    Serial.println("ADS1115 configured successfully");
  }

  // Add this after your existing ADS1115 initialization in initializeHardware()
  // Verify configuration was applied correctly

  // Read back the config register to verify settings
  Wire.beginTransmission(0x48);  // ADS1115 default address
  Wire.write(0x01);              // Config register
  Wire.endTransmission(false);
  Wire.requestFrom(0x48, 2);
  if (Wire.available() >= 2) {
    uint16_t configReg = (Wire.read() << 8) | Wire.read();
    queueConsoleMessage("ADS1115 Config Register: 0x" + String(configReg, HEX));

    // Decode key bits
    uint8_t mux = (configReg >> 12) & 0x07;
    uint8_t pga = (configReg >> 9) & 0x07;
    uint8_t mode = (configReg >> 8) & 0x01;

    queueConsoleMessage("ADS1115 MUX: " + String(mux) + ", PGA: " + String(pga) + ", Mode: " + String(mode));

    // Expected values:
    // MUX should be 4 (single-ended A0) when you read channel 0
    // PGA should be 0 (±6.144V range)
    // Mode should be 0 (continuous) or 1 (single-shot)
  } else {
    queueConsoleMessage("ADS1115 Config readback failed - I2C error");
  }
  //onewire
  sensors.begin();
  sensors.setResolution(12);
  sensors.getAddress(tempDeviceAddress, 0);
  if (sensors.getDeviceCount() == 0) {
    Serial.println("WARNING: No DS18B20 sensors found on the bus.");
    queueConsoleMessage("WARNING: No DS18B20 sensors found on the bus");
    sensors.setWaitForConversion(false);  // this is critical!
  }

  xTaskCreatePinnedToCore(
    TempTask,
    "TempTask",
    4096,  // was 4096
    NULL,
    0,  // Priority lower than normal (execute if nothing else to do, all the 1's are idle)
    &tempTaskHandle,
    0  // Run on Core 0, which is the one doing Wifi and system tasks, and theoretically has more idle points than Core 1 and "loop()"
  );

  Serial.println("Hardware initialization complete");
}
void InitSystemSettings() {  // load all settings from LittleFS.  If no files exist, create them and populate with the hardcoded values
  if (!LittleFS.exists("/BatteryCapacity_Ah.txt")) {
    writeFile(LittleFS, "/BatteryCapacity_Ah.txt", String(BatteryCapacity_Ah).c_str());
  } else {
    BatteryCapacity_Ah = readFile(LittleFS, "/BatteryCapacity_Ah.txt").toInt();
  }
  PeukertRatedCurrent_A = BatteryCapacity_Ah / 20.0f;

  if (!LittleFS.exists("/ChargeEfficiency.txt")) {
    writeFile(LittleFS, "/ChargeEfficiency.txt", String(ChargeEfficiency_scaled).c_str());
  } else {
    ChargeEfficiency_scaled = readFile(LittleFS, "/ChargeEfficiency.txt").toInt();
  }


  if (!LittleFS.exists("/TailCurrent.txt")) {
    writeFile(LittleFS, "/TailCurrent.txt", String(TailCurrent).c_str());
  } else {
    TailCurrent = readFile(LittleFS, "/TailCurrent.txt").toFloat();
  }

  if (!LittleFS.exists("/FuelEfficiency.txt")) {
    writeFile(LittleFS, "/FuelEfficiency.txt", String(FuelEfficiency_scaled).c_str());
  } else {
    FuelEfficiency_scaled = readFile(LittleFS, "/FuelEfficiency.txt").toInt();
  }
  //////////////////////////////////
  if (!LittleFS.exists("/TemperatureLimitF.txt")) {
    writeFile(LittleFS, "/TemperatureLimitF.txt", String(TemperatureLimitF).c_str());
  } else {
    TemperatureLimitF = readFile(LittleFS, "/TemperatureLimitF.txt").toInt();
  }
  if (!LittleFS.exists("/ManualDutyTarget.txt")) {
    writeFile(LittleFS, "/ManualDutyTarget.txt", String(ManualDutyTarget).c_str());
  } else {
    ManualDutyTarget = readFile(LittleFS, "/ManualDutyTarget.txt").toInt();  //
  }
  if (!LittleFS.exists("/BulkVoltage.txt")) {
    writeFile(LittleFS, "/BulkVoltage.txt", String(BulkVoltage).c_str());
  } else {
    BulkVoltage = readFile(LittleFS, "/BulkVoltage.txt").toFloat();
  }
  if (!LittleFS.exists("/TargetAmps.txt")) {
    writeFile(LittleFS, "/TargetAmps.txt", String(TargetAmps).c_str());
  } else {
    TargetAmps = readFile(LittleFS, "/TargetAmps.txt").toInt();
  }
  if (!LittleFS.exists("/SwitchingFrequency.txt")) {
    writeFile(LittleFS, "/SwitchingFrequency.txt", String(SwitchingFrequency).c_str());
  } else {
    SwitchingFrequency = readFile(LittleFS, "/SwitchingFrequency.txt").toInt();
  }
  if (!LittleFS.exists("/FloatVoltage.txt")) {
    writeFile(LittleFS, "/FloatVoltage.txt", String(FloatVoltage).c_str());
  } else {
    FloatVoltage = readFile(LittleFS, "/FloatVoltage.txt").toFloat();
  }
  if (!LittleFS.exists("/dutyStep.txt")) {
    writeFile(LittleFS, "/dutyStep.txt", String(dutyStep).c_str());
  } else {
    dutyStep = readFile(LittleFS, "/dutyStep.txt").toFloat();
  }
  if (!LittleFS.exists("/FieldAdjustmentInterval.txt")) {
    writeFile(LittleFS, "/FieldAdjustmentInterval.txt", String(FieldAdjustmentInterval).c_str());
  } else {
    FieldAdjustmentInterval = readFile(LittleFS, "/FieldAdjustmentInterval.txt").toFloat();
  }
  if (!LittleFS.exists("/ManualFieldToggle.txt")) {
    writeFile(LittleFS, "/ManualFieldToggle.txt", String(ManualFieldToggle).c_str());
  } else {
    ManualFieldToggle = readFile(LittleFS, "/ManualFieldToggle.txt").toInt();
  }
  if (!LittleFS.exists("/SwitchControlOverride.txt")) {
    writeFile(LittleFS, "/SwitchControlOverride.txt", String(SwitchControlOverride).c_str());
  } else {
    SwitchControlOverride = readFile(LittleFS, "/SwitchControlOverride.txt").toInt();
  }
  if (!LittleFS.exists("/IgnitionOverride.txt")) {
    writeFile(LittleFS, "/IgnitionOverride.txt", String(IgnitionOverride).c_str());
  } else {
    IgnitionOverride = readFile(LittleFS, "/IgnitionOverride.txt").toInt();
  }
  if (!LittleFS.exists("/ForceFloat.txt")) {
    writeFile(LittleFS, "/ForceFloat.txt", String(ForceFloat).c_str());
  } else {
    ForceFloat = readFile(LittleFS, "/ForceFloat.txt").toInt();
  }
  if (!LittleFS.exists("/OnOff.txt")) {
    writeFile(LittleFS, "/OnOff.txt", String(OnOff).c_str());
  } else {
    OnOff = readFile(LittleFS, "/OnOff.txt").toInt();
  }
  if (!LittleFS.exists("/HiLow.txt")) {
    writeFile(LittleFS, "/HiLow.txt", String(HiLow).c_str());
  } else {
    HiLow = readFile(LittleFS, "/HiLow.txt").toInt();
  }
  if (!LittleFS.exists("/AmpSrc.txt")) {
    writeFile(LittleFS, "/AmpSrc.txt", String(AmpSrc).c_str());
  } else {
    AmpSrc = readFile(LittleFS, "/AmpSrc.txt").toInt();
  }
  if (!LittleFS.exists("/InvertAltAmps.txt")) {
    writeFile(LittleFS, "/InvertAltAmps.txt", String(InvertAltAmps).c_str());
  } else {
    InvertAltAmps = readFile(LittleFS, "/InvertAltAmps.txt").toInt();
  }
  if (!LittleFS.exists("/InvertBattAmps.txt")) {
    writeFile(LittleFS, "/InvertBattAmps.txt", String(InvertBattAmps).c_str());
  } else {
    InvertBattAmps = readFile(LittleFS, "/InvertBattAmps.txt").toInt();
  }
  if (!LittleFS.exists("/LimpHome.txt")) {
    writeFile(LittleFS, "/LimpHome.txt", String(LimpHome).c_str());
  } else {
    LimpHome = readFile(LittleFS, "/LimpHome.txt").toInt();
  }
  if (!LittleFS.exists("/VeData.txt")) {
    writeFile(LittleFS, "/VeData.txt", String(VeData).c_str());
  } else {
    VeData = readFile(LittleFS, "/VeData.txt").toInt();
  }
  if (!LittleFS.exists("/NMEA0183Data.txt")) {
    writeFile(LittleFS, "/NMEA0183Data.txt", String(NMEA0183Data).c_str());
  } else {
    NMEA0183Data = readFile(LittleFS, "/NMEA0183Data.txt").toInt();
  }
  if (!LittleFS.exists("/NMEA2KData.txt")) {
    writeFile(LittleFS, "/NMEA2KData.txt", String(NMEA2KData).c_str());
  } else {
    NMEA2KData = readFile(LittleFS, "/NMEA2KData.txt").toInt();
  }
  if (!LittleFS.exists("/TargetAmpL.txt")) {
    writeFile(LittleFS, "/TargetAmpL.txt", String(TargetAmpL).c_str());
  } else {
    TargetAmpL = readFile(LittleFS, "/TargetAmpL.txt").toInt();
  }

  //New May 17
  if (!LittleFS.exists("/CurrentThreshold.txt")) {
    writeFile(LittleFS, "/CurrentThreshold.txt", String(CurrentThreshold).c_str());
  } else {
    CurrentThreshold = readFile(LittleFS, "/CurrentThreshold.txt").toFloat();
  }
  if (!LittleFS.exists("/PeukertExponent.txt")) {
    writeFile(LittleFS, "/PeukertExponent.txt", String(PeukertExponent_scaled).c_str());
  } else {
    PeukertExponent_scaled = readFile(LittleFS, "/PeukertExponent.txt").toInt();
  }
  if (!LittleFS.exists("/ChargeEfficiency.txt")) {
    writeFile(LittleFS, "/ChargeEfficiency.txt", String(ChargeEfficiency_scaled).c_str());
  } else {
    ChargeEfficiency_scaled = readFile(LittleFS, "/ChargeEfficiency.txt").toInt();
  }
  if (!LittleFS.exists("/ChargedVoltage.txt")) {
    writeFile(LittleFS, "/ChargedVoltage.txt", String(ChargedVoltage_Scaled).c_str());
  } else {
    ChargedVoltage_Scaled = readFile(LittleFS, "/ChargedVoltage.txt").toInt();
  }
  if (!LittleFS.exists("/TailCurrent.txt")) {
    writeFile(LittleFS, "/TailCurrent.txt", String(TailCurrent).c_str());
  } else {
    TailCurrent = readFile(LittleFS, "/TailCurrent.txt").toFloat();
  }
  if (!LittleFS.exists("/ChargedDetectionTime.txt")) {
    writeFile(LittleFS, "/ChargedDetectionTime.txt", String(ChargedDetectionTime).c_str());
  } else {
    ChargedDetectionTime = readFile(LittleFS, "/ChargedDetectionTime.txt").toInt();
  }
  if (!LittleFS.exists("/IgnoreTemperature.txt")) {
    writeFile(LittleFS, "/IgnoreTemperature.txt", String(IgnoreTemperature).c_str());
  } else {
    IgnoreTemperature = readFile(LittleFS, "/IgnoreTemperature.txt").toInt();
  }
  if (!LittleFS.exists("/bmsLogic.txt")) {
    writeFile(LittleFS, "/bmsLogic.txt", String(bmsLogic).c_str());
  } else {
    bmsLogic = readFile(LittleFS, "/bmsLogic.txt").toInt();
  }
  if (!LittleFS.exists("/bmsLogicLevelOff.txt")) {
    writeFile(LittleFS, "/bmsLogicLevelOff.txt", String(bmsLogicLevelOff).c_str());
  } else {
    bmsLogicLevelOff = readFile(LittleFS, "/bmsLogicLevelOff.txt").toInt();
  }
  if (!LittleFS.exists("/AlarmActivate.txt")) {
    writeFile(LittleFS, "/AlarmActivate.txt", String(AlarmActivate).c_str());
  } else {
    AlarmActivate = readFile(LittleFS, "/AlarmActivate.txt").toInt();
  }
  if (!LittleFS.exists("/TempAlarm.txt")) {
    writeFile(LittleFS, "/TempAlarm.txt", String(TempAlarm).c_str());
  } else {
    TempAlarm = readFile(LittleFS, "/TempAlarm.txt").toInt();
  }
  if (!LittleFS.exists("/VoltageAlarmHigh.txt")) {
    writeFile(LittleFS, "/VoltageAlarmHigh.txt", String(VoltageAlarmHigh).c_str());
  } else {
    VoltageAlarmHigh = readFile(LittleFS, "/VoltageAlarmHigh.txt").toInt();
  }
  if (!LittleFS.exists("/VoltageAlarmLow.txt")) {
    writeFile(LittleFS, "/VoltageAlarmLow.txt", String(VoltageAlarmLow).c_str());
  } else {
    VoltageAlarmLow = readFile(LittleFS, "/VoltageAlarmLow.txt").toInt();
  }
  if (!LittleFS.exists("/CurrentAlarmHigh.txt")) {
    writeFile(LittleFS, "/CurrentAlarmHigh.txt", String(CurrentAlarmHigh).c_str());
  } else {
    CurrentAlarmHigh = readFile(LittleFS, "/CurrentAlarmHigh.txt").toInt();
  }
  if (!LittleFS.exists("/FourWay.txt")) {
    writeFile(LittleFS, "/FourWay.txt", String(FourWay).c_str());
  } else {
    FourWay = readFile(LittleFS, "/FourWay.txt").toInt();
  }
  if (!LittleFS.exists("/RPMScalingFactor.txt")) {
    writeFile(LittleFS, "/RPMScalingFactor.txt", String(RPMScalingFactor).c_str());
  } else {
    RPMScalingFactor = readFile(LittleFS, "/RPMScalingFactor.txt").toInt();
  }

  if (!LittleFS.exists("/FieldResistance.txt")) {
    writeFile(LittleFS, "/FieldResistance.txt", String(FieldResistance).c_str());
  } else {
    FieldResistance = readFile(LittleFS, "/FieldResistance.txt").toInt();
  }


  if (!LittleFS.exists("/ResetTemp.txt")) {
    writeFile(LittleFS, "/ResetTemp.txt", String(ResetTemp).c_str());
  } else {
    ResetTemp = readFile(LittleFS, "/ResetTemp.txt").toInt();
  }
  if (!LittleFS.exists("/ResetVoltage.txt")) {
    writeFile(LittleFS, "/ResetVoltage.txt", String(ResetVoltage).c_str());
  } else {
    ResetVoltage = readFile(LittleFS, "/ResetVoltage.txt").toInt();
  }
  if (!LittleFS.exists("/ResetCurrent.txt")) {
    writeFile(LittleFS, "/ResetCurrent.txt", String(ResetCurrent).c_str());
  } else {
    ResetCurrent = readFile(LittleFS, "/ResetCurrent.txt").toInt();
  }
  if (!LittleFS.exists("/ResetEngineRunTime.txt")) {
    writeFile(LittleFS, "/ResetEngineRunTime.txt", String(ResetEngineRunTime).c_str());
  } else {
    ResetEngineRunTime = readFile(LittleFS, "/ResetEngineRunTime.txt").toInt();
  }
  if (!LittleFS.exists("/ResetAlternatorOnTime.txt")) {
    writeFile(LittleFS, "/ResetAlternatorOnTime.txt", String(ResetAlternatorOnTime).c_str());
  } else {
    ResetAlternatorOnTime = readFile(LittleFS, "/ResetAlternatorOnTime.txt").toInt();
  }
  if (!LittleFS.exists("/ResetEnergy.txt")) {
    writeFile(LittleFS, "/ResetEnergy.txt", String(ResetEnergy).c_str());
  } else {
    ResetEnergy = readFile(LittleFS, "/ResetEnergy.txt").toInt();
  }

  if (!LittleFS.exists("/MaximumAllowedBatteryAmps.txt")) {
    writeFile(LittleFS, "/MaximumAllowedBatteryAmps.txt", String(MaximumAllowedBatteryAmps).c_str());
  } else {
    MaximumAllowedBatteryAmps = readFile(LittleFS, "/MaximumAllowedBatteryAmps.txt").toInt();
  }

  if (!LittleFS.exists("/ManualSOCPoint.txt")) {
    writeFile(LittleFS, "/ManualSOCPoint.txt", String(ManualSOCPoint).c_str());
  } else {
    ManualSOCPoint = readFile(LittleFS, "/ManualSOCPoint.txt").toInt();
  }

  if (!LittleFS.exists("/ManualLifePercentage.txt")) {  // manual override of alterantor lifetime estimates this is pointless
    writeFile(LittleFS, "/ManualLifePercentage.txt", String(ManualLifePercentage).c_str());
  } else {
    ManualLifePercentage = readFile(LittleFS, "/ManualLifePercentage.txt").toInt();
  }

  if (!LittleFS.exists("/BatteryVoltageSource.txt")) {
    writeFile(LittleFS, "/BatteryVoltageSource.txt", String(BatteryVoltageSource).c_str());
  } else {
    BatteryVoltageSource = readFile(LittleFS, "/BatteryVoltageSource.txt").toInt();
  }

  if (!LittleFS.exists("/ShuntResistanceMicroOhm.txt")) {
    writeFile(LittleFS, "/ShuntResistanceMicroOhm.txt", String(ShuntResistanceMicroOhm).c_str());
  } else {
    ShuntResistanceMicroOhm = readFile(LittleFS, "/ShuntResistanceMicroOhm.txt").toInt();
  }
  if (!LittleFS.exists("/maxPoints.txt")) {
    writeFile(LittleFS, "/maxPoints.txt", String(maxPoints).c_str());
  } else {
    maxPoints = readFile(LittleFS, "/maxPoints.txt").toInt();
  }
  if (!LittleFS.exists("/Ymin1.txt")) {
    writeFile(LittleFS, "/Ymin1.txt", String(Ymin1).c_str());
  } else {
    Ymin1 = readFile(LittleFS, "/Ymin1.txt").toInt();
  }

  if (!LittleFS.exists("/Ymax1.txt")) {
    writeFile(LittleFS, "/Ymax1.txt", String(Ymax1).c_str());
  } else {
    Ymax1 = readFile(LittleFS, "/Ymax1.txt").toInt();
  }

  if (!LittleFS.exists("/Ymin2.txt")) {
    writeFile(LittleFS, "/Ymin2.txt", String(Ymin2).c_str());
  } else {
    Ymin2 = readFile(LittleFS, "/Ymin2.txt").toInt();
  }

  if (!LittleFS.exists("/Ymax2.txt")) {
    writeFile(LittleFS, "/Ymax2.txt", String(Ymax2).c_str());
  } else {
    Ymax2 = readFile(LittleFS, "/Ymax2.txt").toInt();
  }

  if (!LittleFS.exists("/Ymin3.txt")) {
    writeFile(LittleFS, "/Ymin3.txt", String(Ymin3).c_str());
  } else {
    Ymin3 = readFile(LittleFS, "/Ymin3.txt").toInt();
  }

  if (!LittleFS.exists("/Ymax3.txt")) {
    writeFile(LittleFS, "/Ymax3.txt", String(Ymax3).c_str());
  } else {
    Ymax3 = readFile(LittleFS, "/Ymax3.txt").toInt();
  }

  if (!LittleFS.exists("/Ymin4.txt")) {
    writeFile(LittleFS, "/Ymin4.txt", String(Ymin4).c_str());
  } else {
    Ymin4 = readFile(LittleFS, "/Ymin4.txt").toInt();
  }

  if (!LittleFS.exists("/Ymax4.txt")) {
    writeFile(LittleFS, "/Ymax4.txt", String(Ymax4).c_str());
  } else {
    Ymax4 = readFile(LittleFS, "/Ymax4.txt").toInt();
  }




  if (!LittleFS.exists("/MaxDuty.txt")) {
    writeFile(LittleFS, "/MaxDuty.txt", String(MaxDuty).c_str());
  } else {
    MaxDuty = readFile(LittleFS, "/MaxDuty.txt").toInt();
  }
  if (!LittleFS.exists("/MinDuty.txt")) {
    writeFile(LittleFS, "/MinDuty.txt", String(MinDuty).c_str());
  } else {
    MinDuty = readFile(LittleFS, "/MinDuty.txt").toInt();
  }
  if (!LittleFS.exists("/R_fixed.txt")) {
    writeFile(LittleFS, "/R_fixed.txt", String(R_fixed).c_str());
  } else {
    R_fixed = readFile(LittleFS, "/R_fixed.txt").toFloat();
  }
  if (!LittleFS.exists("/Beta.txt")) {
    writeFile(LittleFS, "/Beta.txt", String(Beta).c_str());
  } else {
    Beta = readFile(LittleFS, "/Beta.txt").toFloat();
  }
  if (!LittleFS.exists("/T0_C.txt")) {
    writeFile(LittleFS, "/T0_C.txt", String(T0_C).c_str());
  } else {
    T0_C = readFile(LittleFS, "/T0_C.txt").toFloat();
  }
  if (!LittleFS.exists("/TempSource.txt")) {
    writeFile(LittleFS, "/TempSource.txt", String(TempSource).c_str());
  } else {
    TempSource = readFile(LittleFS, "/TempSource.txt").toInt();
  }
  if (!LittleFS.exists("/AlternatorCOffset.txt")) {
    writeFile(LittleFS, "/AlternatorCOffset.txt", String(AlternatorCOffset).c_str());
  } else {
    AlternatorCOffset = readFile(LittleFS, "/AlternatorCOffset.txt").toInt();
  }

  if (!LittleFS.exists("/BatteryCOffset.txt")) {
    writeFile(LittleFS, "/BatteryCOffset.txt", String(BatteryCOffset).c_str());
  } else {
    BatteryCOffset = readFile(LittleFS, "/BatteryCOffset.txt").toInt();
  }

  if (!LittleFS.exists("/MaxTemperatureThermistor.txt")) {
    writeFile(LittleFS, "/MaxTemperatureThermistor.txt", String(MaxTemperatureThermistor).c_str());
  } else {
    MaxTemperatureThermistor = readFile(LittleFS, "/MaxTemperatureThermistor.txt").toInt();  // Use toInt()
  }
  if (!LittleFS.exists("/AlarmLatchEnabled.txt")) {
    writeFile(LittleFS, "/AlarmLatchEnabled.txt", String(AlarmLatchEnabled).c_str());
  } else {
    AlarmLatchEnabled = readFile(LittleFS, "/AlarmLatchEnabled.txt").toInt();
  }

  if (!LittleFS.exists("/bulkCompleteTime.txt")) {
    writeFile(LittleFS, "/bulkCompleteTime.txt", String(bulkCompleteTime).c_str());
  } else {
    bulkCompleteTime = readFile(LittleFS, "/bulkCompleteTime.txt").toInt();
  }
  if (!LittleFS.exists("/FLOAT_DURATION.txt")) {
    writeFile(LittleFS, "/FLOAT_DURATION.txt", String(FLOAT_DURATION).c_str());
  } else {
    FLOAT_DURATION = readFile(LittleFS, "/FLOAT_DURATION.txt").toInt();
  }

  // ADD: Dynamic correction settings (add with other settings)
  if (!LittleFS.exists("/AutoShuntGainCorrection.txt")) {  // BOOLEAN
    writeFile(LittleFS, "/AutoShuntGainCorrection.txt", String(AutoShuntGainCorrection).c_str());
  } else {
    AutoShuntGainCorrection = readFile(LittleFS, "/AutoShuntGainCorrection.txt").toInt();
  }

  if (!LittleFS.exists("/AutoAltCurrentZero.txt")) {  // BOOLEAN
    writeFile(LittleFS, "/AutoAltCurrentZero.txt", String(AutoAltCurrentZero).c_str());
  } else {
    AutoAltCurrentZero = readFile(LittleFS, "/AutoAltCurrentZero.txt").toInt();
  }

  if (!LittleFS.exists("/WindingTempOffset.txt")) {
    writeFile(LittleFS, "/WindingTempOffset.txt", String(WindingTempOffset, 1).c_str());
  } else {
    WindingTempOffset = readFile(LittleFS, "/WindingTempOffset.txt").toFloat();
  }

  if (!LittleFS.exists("/PulleyRatio.txt")) {
    writeFile(LittleFS, "/PulleyRatio.txt", String(PulleyRatio, 2).c_str());
  } else {
    PulleyRatio = readFile(LittleFS, "/PulleyRatio.txt").toFloat();
  }
  if (!LittleFS.exists("/BatteryCurrentSource.txt")) {
    writeFile(LittleFS, "/BatteryCurrentSource.txt", String(BatteryCurrentSource).c_str());
  } else {
    BatteryCurrentSource = readFile(LittleFS, "/BatteryCurrentSource.txt").toInt();
  }

  if (!LittleFS.exists("/timeAxisModeChanging.txt")) {
    writeFile(LittleFS, "/timeAxisModeChanging.txt", String(timeAxisModeChanging).c_str());
  } else {
    timeAxisModeChanging = readFile(LittleFS, "/timeAxisModeChanging.txt").toInt();
  }


  // Update webgaugesinterval to be user-configurable
  if (!LittleFS.exists("/webgaugesinterval.txt")) {
    writeFile(LittleFS, "/webgaugesinterval.txt", String(webgaugesinterval).c_str());
  } else {
    webgaugesinterval = readFile(LittleFS, "/webgaugesinterval.txt").toInt();
    webgaugesinterval = constrain(webgaugesinterval, 1, 10000000);
  }

  if (!LittleFS.exists("/plotTimeWindow.txt")) {
    writeFile(LittleFS, "/plotTimeWindow.txt", String(plotTimeWindow).c_str());
  } else {
    plotTimeWindow = readFile(LittleFS, "/plotTimeWindow.txt").toInt();
    plotTimeWindow = constrain(plotTimeWindow, 1, 1000000);  // 10s to 10min
  }


  if (!LittleFS.exists("/LearningMode.txt")) {
    writeFile(LittleFS, "/LearningMode.txt", String(LearningMode).c_str());
  } else {
    LearningMode = readFile(LittleFS, "/LearningMode.txt").toInt();
  }

  if (!LittleFS.exists("/LearningPaused.txt")) {
    writeFile(LittleFS, "/LearningPaused.txt", String(LearningPaused).c_str());
  } else {
    LearningPaused = readFile(LittleFS, "/LearningPaused.txt").toInt();
  }

  if (!LittleFS.exists("/IgnoreLearningDuringPenalty.txt")) {
    writeFile(LittleFS, "/IgnoreLearningDuringPenalty.txt", String(IgnoreLearningDuringPenalty).c_str());
  } else {
    IgnoreLearningDuringPenalty = readFile(LittleFS, "/IgnoreLearningDuringPenalty.txt").toInt();
  }

  if (!LittleFS.exists("/EXTRA.txt")) {
    writeFile(LittleFS, "/EXTRA.txt", String(EXTRA).c_str());
  } else {
    EXTRA = readFile(LittleFS, "/EXTRA.txt").toInt();
  }

  if (!LittleFS.exists("/LearningDryRunMode.txt")) {
    writeFile(LittleFS, "/LearningDryRunMode.txt", String(LearningDryRunMode).c_str());
  } else {
    LearningDryRunMode = readFile(LittleFS, "/LearningDryRunMode.txt").toInt();
  }

  if (!LittleFS.exists("/AutoSaveLearningTable.txt")) {
    writeFile(LittleFS, "/AutoSaveLearningTable.txt", String(AutoSaveLearningTable).c_str());
  } else {
    AutoSaveLearningTable = readFile(LittleFS, "/AutoSaveLearningTable.txt").toInt();
  }

  if (!LittleFS.exists("/LearningUpwardEnabled.txt")) {
    writeFile(LittleFS, "/LearningUpwardEnabled.txt", String(LearningUpwardEnabled).c_str());
  } else {
    LearningUpwardEnabled = readFile(LittleFS, "/LearningUpwardEnabled.txt").toInt();
  }

  if (!LittleFS.exists("/LearningDownwardEnabled.txt")) {
    writeFile(LittleFS, "/LearningDownwardEnabled.txt", String(LearningDownwardEnabled).c_str());
  } else {
    LearningDownwardEnabled = readFile(LittleFS, "/LearningDownwardEnabled.txt").toInt();
  }

  if (!LittleFS.exists("/EnableNeighborLearning.txt")) {
    writeFile(LittleFS, "/EnableNeighborLearning.txt", String(EnableNeighborLearning).c_str());
  } else {
    EnableNeighborLearning = readFile(LittleFS, "/EnableNeighborLearning.txt").toInt();
  }

  if (!LittleFS.exists("/EnableAmbientCorrection.txt")) {
    writeFile(LittleFS, "/EnableAmbientCorrection.txt", String(EnableAmbientCorrection).c_str());
  } else {
    EnableAmbientCorrection = readFile(LittleFS, "/EnableAmbientCorrection.txt").toInt();
  }

  if (!LittleFS.exists("/LearningFailsafeMode.txt")) {
    writeFile(LittleFS, "/LearningFailsafeMode.txt", String(LearningFailsafeMode).c_str());
  } else {
    LearningFailsafeMode = readFile(LittleFS, "/LearningFailsafeMode.txt").toInt();
  }

  if (!LittleFS.exists("/ShowLearningDebugMessages.txt")) {
    writeFile(LittleFS, "/ShowLearningDebugMessages.txt", String(ShowLearningDebugMessages).c_str());
  } else {
    ShowLearningDebugMessages = readFile(LittleFS, "/ShowLearningDebugMessages.txt").toInt();
  }

  if (!LittleFS.exists("/LogAllLearningEvents.txt")) {
    writeFile(LittleFS, "/LogAllLearningEvents.txt", String(LogAllLearningEvents).c_str());
  } else {
    LogAllLearningEvents = readFile(LittleFS, "/LogAllLearningEvents.txt").toInt();
  }

  if (!LittleFS.exists("/AlternatorNominalAmps.txt")) {
    writeFile(LittleFS, "/AlternatorNominalAmps.txt", String(AlternatorNominalAmps).c_str());
  } else {
    AlternatorNominalAmps = readFile(LittleFS, "/AlternatorNominalAmps.txt").toInt();
  }

  if (!LittleFS.exists("/LearningUpStep.txt")) {
    writeFile(LittleFS, "/LearningUpStep.txt", String(LearningUpStep, 2).c_str());
  } else {
    LearningUpStep = readFile(LittleFS, "/LearningUpStep.txt").toFloat();
  }

  if (!LittleFS.exists("/LearningDownStep.txt")) {
    writeFile(LittleFS, "/LearningDownStep.txt", String(LearningDownStep, 2).c_str());
  } else {
    LearningDownStep = readFile(LittleFS, "/LearningDownStep.txt").toFloat();
  }

  if (!LittleFS.exists("/AmbientTempCorrectionFactor.txt")) {
    writeFile(LittleFS, "/AmbientTempCorrectionFactor.txt", String(AmbientTempCorrectionFactor, 2).c_str());
  } else {
    AmbientTempCorrectionFactor = readFile(LittleFS, "/AmbientTempCorrectionFactor.txt").toFloat();
  }

  if (!LittleFS.exists("/AmbientTempBaseline.txt")) {
    writeFile(LittleFS, "/AmbientTempBaseline.txt", String(AmbientTempBaseline, 2).c_str());
  } else {
    AmbientTempBaseline = readFile(LittleFS, "/AmbientTempBaseline.txt").toFloat();
  }

  if (!LittleFS.exists("/MinLearningInterval.txt")) {
    writeFile(LittleFS, "/MinLearningInterval.txt", String(MinLearningInterval).c_str());
  } else {
    MinLearningInterval = readFile(LittleFS, "/MinLearningInterval.txt").toInt();
  }

  if (!LittleFS.exists("/SafeOperationThreshold.txt")) {
    writeFile(LittleFS, "/SafeOperationThreshold.txt", String(SafeOperationThreshold).c_str());
  } else {
    SafeOperationThreshold = readFile(LittleFS, "/SafeOperationThreshold.txt").toInt();
  }

  if (!LittleFS.exists("/PidKp.txt")) {
    writeFile(LittleFS, "/PidKp.txt", String(PidKp, 3).c_str());
  } else {
    PidKp = readFile(LittleFS, "/PidKp.txt").toFloat();
  }

  if (!LittleFS.exists("/PidKi.txt")) {
    writeFile(LittleFS, "/PidKi.txt", String(PidKi, 3).c_str());
  } else {
    PidKi = readFile(LittleFS, "/PidKi.txt").toFloat();
  }

  if (!LittleFS.exists("/PidKd.txt")) {
    writeFile(LittleFS, "/PidKd.txt", String(PidKd, 3).c_str());
  } else {
    PidKd = readFile(LittleFS, "/PidKd.txt").toFloat();
  }

  if (!LittleFS.exists("/PidSampleTime.txt")) {
    writeFile(LittleFS, "/PidSampleTime.txt", String(PidSampleTime).c_str());
  } else {
    PidSampleTime = readFile(LittleFS, "/PidSampleTime.txt").toInt();
  }

  // Add after other learning settings:
  if (!LittleFS.exists("/LearningSettlingPeriod.txt")) {
    writeFile(LittleFS, "/LearningSettlingPeriod.txt", String(LearningSettlingPeriod).c_str());
  } else {
    LearningSettlingPeriod = readFile(LittleFS, "/LearningSettlingPeriod.txt").toInt();
  }

  if (!LittleFS.exists("/LearningRPMChangeThreshold.txt")) {
    writeFile(LittleFS, "/LearningRPMChangeThreshold.txt", String(LearningRPMChangeThreshold).c_str());
  } else {
    LearningRPMChangeThreshold = readFile(LittleFS, "/LearningRPMChangeThreshold.txt").toInt();
  }

  if (!LittleFS.exists("/LearningTempHysteresis.txt")) {
    writeFile(LittleFS, "/LearningTempHysteresis.txt", String(LearningTempHysteresis).c_str());
  } else {
    LearningTempHysteresis = readFile(LittleFS, "/LearningTempHysteresis.txt").toInt();
  }



  if (!LittleFS.exists("/MaxTableValue.txt")) {
    writeFile(LittleFS, "/MaxTableValue.txt", String(MaxTableValue, 2).c_str());
  } else {
    MaxTableValue = readFile(LittleFS, "/MaxTableValue.txt").toFloat();
  }

  if (!LittleFS.exists("/MinTableValue.txt")) {
    writeFile(LittleFS, "/MinTableValue.txt", String(MinTableValue, 2).c_str());
  } else {
    MinTableValue = readFile(LittleFS, "/MinTableValue.txt").toFloat();
  }

  if (!LittleFS.exists("/MaxPenaltyPercent.txt")) {
    writeFile(LittleFS, "/MaxPenaltyPercent.txt", String(MaxPenaltyPercent, 2).c_str());
  } else {
    MaxPenaltyPercent = readFile(LittleFS, "/MaxPenaltyPercent.txt").toFloat();
  }

  if (!LittleFS.exists("/MaxPenaltyDuration.txt")) {
    writeFile(LittleFS, "/MaxPenaltyDuration.txt", String(MaxPenaltyDuration).c_str());
  } else {
    MaxPenaltyDuration = readFile(LittleFS, "/MaxPenaltyDuration.txt").toInt();
  }

  if (!LittleFS.exists("/NeighborLearningFactor.txt")) {
    writeFile(LittleFS, "/NeighborLearningFactor.txt", String(NeighborLearningFactor, 3).c_str());
  } else {
    NeighborLearningFactor = readFile(LittleFS, "/NeighborLearningFactor.txt").toFloat();
  }

  if (!LittleFS.exists("/LearningRPMSpacing.txt")) {
    writeFile(LittleFS, "/LearningRPMSpacing.txt", String(LearningRPMSpacing).c_str());
  } else {
    LearningRPMSpacing = readFile(LittleFS, "/LearningRPMSpacing.txt").toInt();
  }

  if (!LittleFS.exists("/LearningMemoryDuration.txt")) {
    writeFile(LittleFS, "/LearningMemoryDuration.txt", String(LearningMemoryDuration).c_str());
  } else {
    LearningMemoryDuration = readFile(LittleFS, "/LearningMemoryDuration.txt").toInt();
  }

  if (!LittleFS.exists("/LearningTableSaveInterval.txt")) {
    writeFile(LittleFS, "/LearningTableSaveInterval.txt", String(LearningTableSaveInterval).c_str());
  } else {
    LearningTableSaveInterval = readFile(LittleFS, "/LearningTableSaveInterval.txt").toInt();
  }
}

void TempTask(void* parameter) {
  // NO watchdog registration - keep it separate from main loop watchdog

  // Use static variables to reduce stack usage
  static uint8_t scratchPad[9];
  static unsigned long lastTempRead = 0;

  for (;;) {
    unsigned long now = millis();

    // Update heartbeat at start of each cycle - this shows the task is alive
    lastTempTaskHeartbeat = now;
    tempTaskHealthy = true;

    // Only read temperature every 10 seconds (increased from 5)
    if (now - lastTempRead < 10000) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;  // Skip to next iteration, but heartbeat was updated above
    }

    // Step 1: Trigger conversion
    sensors.requestTemperaturesByAddress(tempDeviceAddress);


    // Step 2: Wait with frequent watchdog feeding - FIXED to feed every iteration
    for (int i = 0; i < 25; i++) {  // 25 x 200ms = 5000ms
      vTaskDelay(pdMS_TO_TICKS(200));
      // Update heartbeat during long wait to show task is still alive (instead of watchdog feed)
      lastTempTaskHeartbeat = millis();
    }

    // Step 3: Read result
    if (sensors.readScratchPad(tempDeviceAddress, scratchPad)) {
      int16_t raw = (scratchPad[1] << 8) | scratchPad[0];
      float tempC = raw / 16.0;
      float tempF = tempC * 1.8 + 32.0;

      if (tempF > -50 && tempF < 300) {
        AlternatorTemperatureF = tempF;
        if (AlternatorTemperatureF > MaxAlternatorTemperatureF) {
          MaxAlternatorTemperatureF = AlternatorTemperatureF;
        }
        MARK_FRESH(IDX_ALTERNATOR_TEMP);
      } else {
        AlternatorTemperatureF = -99;
      }
    } else {
      AlternatorTemperatureF = -99;
    }

    lastTempRead = now;
    // Final heartbeat update before next cycle
    lastTempTaskHeartbeat = millis();
  }
}

void SystemTime(const tN2kMsg& N2kMsg) {
  unsigned char SID;
  uint16_t SystemDate;
  double SystemTime;
  tN2kTimeSource TimeSource;

  if (ParseN2kSystemTime(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
    OutputStream->println("System time:");
    PrintLabelValWithConversionCheckUnDef("  SID: ", SID, 0, true);
    PrintLabelValWithConversionCheckUnDef("  days since 1.1.1970: ", SystemDate, 0, true);
    PrintLabelValWithConversionCheckUnDef("  seconds since midnight: ", SystemTime, 0, true);
    OutputStream->print("  time source: ");
    PrintN2kEnumType(TimeSource, OutputStream);
  } else {
    OutputStream->print("Failed to parse PGN: ");
    OutputStream->println(N2kMsg.PGN);
  }
}

void setDutyPercent(int percent) {
  static uint32_t lastFrequency = 0;
  static int lastPercent = -1;
  static bool pwmInitialized = false;

  percent = constrain(percent, 0, 100);
  uint32_t duty = (((1UL << pwmResolution) - 1) * percent) / 100;  // Fixed: dynamic max duty

  if (percent != lastPercent) {
    lastPercent = percent;
  }

  // Initialize PWM once if not done yet
  if (!pwmInitialized) {
    pwmInitialized = ledcAttach(pwmPin, (uint32_t)SwitchingFrequency, pwmResolution);
    if (!pwmInitialized) {
      queueConsoleMessage("ERROR: PWM attach failed");
      return;  // Fixed: Don't continue if attach failed
    }
    lastFrequency = (uint32_t)SwitchingFrequency;  // Fixed: Set to avoid unnecessary frequency change
  }

  if ((uint32_t)SwitchingFrequency != lastFrequency) {  // Fixed: Cast float to uint32_t for comparison
    // Use ledcChangeFrequency instead of detach/attach for v3.x
    uint32_t actualFreq = ledcChangeFrequency(pwmPin, (uint32_t)SwitchingFrequency, pwmResolution);
    lastFrequency = (uint32_t)SwitchingFrequency;  // Fixed: Cast and store
    delayMicroseconds(100);                        // gives time for hardware to reset (esp. under load), no proof this is needed, but does not hurt
    queueConsoleMessage("PWM frequency applied to hardware: " + String(actualFreq) + "Hz");
  }
  ledcWrite(pwmPin, duty);  // ← In v3.x, first parameter is the pin number
}
void ReadAnalogInputs() {

  // INA228 Battery Monitor
  static unsigned long lastINARead_local = 0;

  if (millis() - lastINARead_local >= AnalogInputReadInterval) {  // could go down to 600 here, but this logic belongs in Loop anyway
    if (INADisconnected == 0) {
      int start33 = micros();  // Start timing analog input reading
      lastINARead_local = millis();

      // Check for INA228 hardware overvoltage alert
      if (INA.getDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT)) {  // this is direct hardware protection for an overvoltage condition, bypassing the ESP32 entirely
        queueConsoleMessage("WARNING: INA228 overvoltage tripped!  Field MOSFET disabled until corrected");
        INA.clearDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT);  // Clear the alert bit for next detection
      }

      // // Debug: Check what the INA228 chip is actually reporting for overvoltage detection
      // uint16_t alertStatus = readINA228AlertRegister(INA.getAddress());
      // if (alertStatus == 0xFFFF) {
      //   static unsigned long lastI2CWarning = 0;
      //   if (millis() - lastI2CWarning > 10000) {
      //     queueConsoleMessage("INA228 DIAG_ALRT: I2C read error");
      //     lastI2CWarning = millis();
      //   }

      // } else if (alertStatus & 0x0010) {  // Check BUSOL bit (bit 4) instead of bit 3
      //   queueConsoleMessage("INA228 DIAG_ALRT: 0x" + String(alertStatus, HEX) + " BUSOL at " + String(IBV, 2) + "V");

      // } else if (alertStatus != 0) {
      //   queueConsoleMessage("INA228 DIAG_ALRT: 0x" + String(alertStatus, HEX) + " (other alert) at " + String(IBV, 2) + "V");
      // }

      try {
        IBV = INA.getBusVoltage();
        ShuntVoltage_mV = INA.getShuntVoltage() * 1000;

        // Sanity check the readings
        if (!isnan(IBV) && IBV > 5.0 && IBV < 70.0 && !isnan(ShuntVoltage_mV)) {
          Bcur = ShuntVoltage_mV * 1000.0f / ShuntResistanceMicroOhm;
          Bcur = Bcur + BatteryCOffset;
          // Apply inversioin if needed
          if (InvertBattAmps == 1) {
            Bcur = -Bcur;
          }
          // Apply dynamic gain correction only when enabled AND using INA228 shunt
          if (AutoShuntGainCorrection == 1 && AmpSrc == 1) {
            Bcur = Bcur * DynamicShuntGainFactor;
          }
          BatteryCurrent_scaled = Bcur * 100;  // Store raw value for battery monitoring
          // Only mark fresh on successful, valid readings
          MARK_FRESH(IDX_IBV);
          MARK_FRESH(IDX_BCUR);
          if (IBV > IBVMax) {  // keep track of high water mark in Ina Battery Voltage
            IBVMax = IBV;
          }
        }

        int end33 = micros();               // End timing
        AnalogReadTime2 = end33 - start33;  // Store elapsed time

        static uint32_t lastReset = 0;
        if (millis() - lastReset >= AinputTrackerTime) {
          AnalogReadTime = 0;
          lastReset = millis();
        }

        if (AnalogReadTime2 > AnalogReadTime) {
          AnalogReadTime = AnalogReadTime2;
        }

      } catch (...) {
        // INA228 read failed - do not call MARK_FRESH
        static unsigned long lastINAFailureWarning = 0;
        if (millis() - lastINAFailureWarning > 10000) {
          Serial.println("INA228 read failed");
          queueConsoleMessage("INA228 read failed");
          lastINAFailureWarning = millis();
        }
      }
    }
  }

  //ADS1115 reading is based on trigger→wait→read so as to not waste time
  // State machine cycles through 4 channels with 20ms conversion time each, providing ~100ms update rate per sensor
  if (ADS1115Disconnected != 0) {
    // Throttled error message to prevent console spam
    static unsigned long lastADSWarning = 0;
    if (millis() - lastADSWarning > 10000) {  // Only warn every 10 seconds
      queueConsoleMessage("theADS1115 was not connected and triggered a return");
      lastADSWarning = millis();
    }
    return;  // Early exit - no MARK_FRESH calls
  }

  unsigned long now = millis();
  switch (adsState) {
    case ADS_IDLE:
      switch (adsCurrentChannel) {
        case 0: adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_0); break;
        case 1: adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_1); break;
        case 2: adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_2); break;
        case 3: adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_3); break;
      }
      adc.triggerConversion();
      adsStartTime = now;
      adsState = ADS_WAITING_FOR_CONVERSION;
      break;

    case ADS_WAITING_FOR_CONVERSION:
      if (now - adsStartTime >= ADSConversionDelay) {
        Raw = adc.getConversion();

        switch (adsCurrentChannel) {
          case 0:
            Channel0V = Raw / 32767.0 * 6.144 * 21.0401;  // divider 1,000,000Ω / 49.9kΩ, scale ≈21.0401
            BatteryV = Channel0V;
            if (BatteryV > 5.0 && BatteryV < 70.0) {  // Sanity check
              MARK_FRESH(IDX_BATTERY_V);              // Only mark fresh on valid reading
            }
            break;
          case 1:
            Channel1V = Raw / 32767.0 * 6.144 * 2.0;  // divider is 768kΩ / 768kΩ, ratio = 0.5, scale = 2.000
            MeasuredAmps = (Channel1V - 2.5) * 100;   // alternator current
            if (InvertAltAmps == 1) {
              MeasuredAmps = MeasuredAmps * -1;  // swap sign if necessary
            }
            MeasuredAmps = MeasuredAmps - AlternatorCOffset;
            // Apply dynamic zero correction only when enabled
            if (AutoAltCurrentZero == 1) {
              MeasuredAmps = MeasuredAmps - DynamicAltCurrentZero;
            }
            if (MeasuredAmps > -500 && MeasuredAmps < 500) {  // Sanity check
              MARK_FRESH(IDX_MEASURED_AMPS);                  // Only mark fresh on valid reading
            }
            if (MeasuredAmps > MeasuredAmpsMax) {  // update the high water mark
              MeasuredAmpsMax = MeasuredAmps;
            }
            break;
          case 2:
            Channel2V = Raw / 32767.0 * 2 * 6.144 * RPMScalingFactor;
            RPM = Channel2V;
            if (RPM < 100) {
              RPM = 0;
            }

            // Temporary RPM fix LATER !!!!
            //RPM = 1800;


            if (RPM >= 0 && RPM < 10000) {  // Sanity check
              MARK_FRESH(IDX_RPM);          // Only mark fresh on valid reading
            }
            break;
          case 3:
            Channel3V = Raw / 32767.0 * 6.144 * 833 * 2;
            temperatureThermistor = thermistorTempC(Channel3V);
            if (temperatureThermistor > 500) {
              temperatureThermistor = -99;
            }
            if (Channel3V > 150) {
              Channel3V = -99;
            }
            if (Channel3V > 0 && Channel3V < 100) {  // Sanity check for Channel3V
              MARK_FRESH(IDX_CHANNEL3V);
            }
            if (temperatureThermistor > -50 && temperatureThermistor < 200) {  // Sanity check for temp
              MARK_FRESH(IDX_THERMISTOR_TEMP);
            }
            break;
        }

        adsCurrentChannel = (adsCurrentChannel + 1) % 4;
        adsState = ADS_IDLE;
      }
      break;
  }

  static unsigned long lastReaddd = 0;
  unsigned long nowww = millis();

  if (nowww - lastReaddd >= 10000) {  // 10 seconds
    lastReaddd = nowww;

    if (bmp.performReading()) {
      ambientTemp = bmp.temperature;          //
      baroPressure = (bmp.pressure / 100.0);  // mBar

    } else {
      Serial.println("Barometric P and ambient T Reading failed- BMP380 malfunction?");
      queueConsoleMessage("Barometric P and ambient T Reading failed- BMP380 malfunction?");
    }
  }
}

void ReadAnalogInputs_Fake() {  //this one is for when hardware isn't really present
  static unsigned long lastFakeUpdate = 0;
  static float fakeVoltage = 13.2;
  static float fakeCurrent = 25.0;
  static float fakeRPM = 1800;
  static float fakeTemp = 45.0;

  // Update every 100ms to match real sensor timing
  if (millis() - lastFakeUpdate < 100) {
    return;
  }
  lastFakeUpdate = millis();

  // Generate fake battery voltage (12-14.5V range, simulating charging)
  fakeVoltage += (random(-20, 20) / 100.0);
  if (fakeVoltage < 12.0) fakeVoltage = 12.0;
  if (fakeVoltage > 14.5) fakeVoltage = 14.5;
  BatteryV = fakeVoltage;
  IBV = fakeVoltage + (random(-10, 10) / 100.0);  // INA voltage slightly different
  MARK_FRESH(IDX_BATTERY_V);
  MARK_FRESH(IDX_IBV);

  // Generate fake alternator current (0-80A range)
  fakeCurrent += (random(-30, 30) / 10.0);
  if (fakeCurrent < 0) fakeCurrent = 0;
  if (fakeCurrent > 80) fakeCurrent = 80;
  MeasuredAmps = fakeCurrent;
  MeasuredAmpsMax = max(MeasuredAmpsMax, MeasuredAmps);
  MARK_FRESH(IDX_MEASURED_AMPS);

  // Generate fake battery current (similar to alt current but can be negative)
  Bcur = fakeCurrent * 0.9 + (random(-50, 50) / 10.0);
  BatteryCurrent_scaled = Bcur * 100;
  MARK_FRESH(IDX_BCUR);

  // Generate fake RPM (1500-2500 range)
  fakeRPM += (random(-100, 100));
  if (fakeRPM < 1500) fakeRPM = 1500;
  if (fakeRPM > 2500) fakeRPM = 2500;
  RPM = fakeRPM;
  MARK_FRESH(IDX_RPM);

  // Generate fake temperatures
  // Generate fake temperatures
  fakeTemp += (random(-10, 10) / 10.0);
  if (fakeTemp < 20) fakeTemp = 20;
  if (fakeTemp > 80) fakeTemp = 80;
  temperatureThermistor = fakeTemp;
  ambientTemp = fakeTemp - 5;                                   // Ambient slightly cooler
  AlternatorTemperatureF = (fakeTemp + 10) * 9.0 / 5.0 + 32.0;  // Convert to Fahrenheit, ~10C hotter
  MARK_FRESH(IDX_THERMISTOR_TEMP);
  MARK_FRESH(IDX_ALTERNATOR_TEMP);

  // Fake barometric pressure (even tho this hardware is almost always present)
  baroPressure = 1013 + (random(-50, 50) / 10.0);

  // Fake other channels
  Channel0V = BatteryV;
  Channel1V = 2.5 + (MeasuredAmps / 100.0);
  Channel2V = RPM / RPMScalingFactor;
  Channel3V = 50 + (random(-20, 20));
  MARK_FRESH(IDX_CHANNEL3V);
  IBVMax = max(IBVMax, IBV);
}

void AdjustFieldLearnMode() {
  // === TIMING CONTROL ===
  static unsigned long lastPidUpdate = 0;
  unsigned long currentMillis = millis();
  bool pidUpdateDue = (currentMillis - lastPidUpdate >= PidSampleTime);

  // === INITIALIZATION AND SETUP ===

  // Clear overheat history if requested
  if (ClearOverheatHistory == 1) {
    clearOverheatHistoryAction();
    ClearOverheatHistory = 0;
  }

  // One-time PID initialization
  if (!pidInitialized) {
    currentPID.SetOutputLimits(MinDuty, MaxDuty);
    currentPID.SetSampleTime(PidSampleTime);
    currentPID.SetTunings(PidKp, PidKi, PidKd);
    Serial.println("Just set PID tunings: Kp=" + String(PidKp) + ", Ki=" + String(PidKi) + ", Kd=" + String(PidKd));
    Serial.println("PID reports Kp=" + String(currentPID.GetKp()));
    pidOutput = 50;  // Keep at 50 for RPM signal loss recovery
    currentPID.SetMode(AUTOMATIC);
    pidInitialized = true;
    loadLearningTableFromNVS();
    queueConsoleMessage("PID: Initialized with Kp=" + String(PidKp, 3) + ", Ki=" + String(PidKi, 3) + ", Kd=" + String(PidKd, 3));
  }

  // Reset learning table if requested
  if (ResetLearningTable == 1) {
    resetLearningTableToDefaults();
    ResetLearningTable = 0;
  }

  // === BASIC ENABLE/DISABLE LOGIC ===

  chargingEnabled = (Ignition == 1 && OnOff == 1);

  // === SAFETY CHECKS - SENSOR HEALTH (run every loop for fast response) ===

  // Check if temperature sensor is responding
  unsigned long tempAge = currentTime - dataTimestamps[IDX_ALTERNATOR_TEMP];
  bool tempDataVeryStale = (tempAge > 30000);  // 30 seconds timeout
  if (tempDataVeryStale) {
    static unsigned long lastTempStaleWarning = 0;
    digitalWrite(21, HIGH);                         // Sound alarm
    if (millis() - lastTempStaleWarning > 10000) {  // Throttle warnings to 10s intervals
      Serial.println("Onewire sensor stale, sensor dead or disconnected");
      queueConsoleMessage("OneWire sensor stale, sensor dead or disconnected");
      lastTempStaleWarning = millis();
    }
  }

  // Check battery voltage sensor redundancy
  if (abs(BatteryV - IBV) > 0.1) {  // NOTE: Threshold set to 0.1V, may need tuning later
    static unsigned long lastVoltageDisagreementWarning = 0;
    digitalWrite(21, HIGH);  // Sound alarm
    digitalWrite(4, 0);      // Disable field
    dutyCycle = MinDuty;
    setDutyPercent((int)dutyCycle);

    if (millis() - lastVoltageDisagreementWarning > 10000) {  // Throttle to 10s intervals
      char msg[128];
      snprintf(msg, sizeof(msg),
               "Battery Voltage disagreement! BatteryV=%.3f V, IBV=%.3f V. Field shut off for safety!",
               BatteryV, IBV);
      queueConsoleMessage(msg);
      lastVoltageDisagreementWarning = millis();
    }
    return;
  }

  // Block field control during current sensor auto-zero calibration
  if (autoZeroStartTime > 0) {
    return;
  }

  // === PREPARATION ===

  updateChargingStage();  // Update bulk/float charging stage
  float currentBatteryVoltage = getBatteryVoltage();

  // === EMERGENCY SHUTDOWNS (run every loop for immediate response) ===

  // Emergency field collapse - voltage spike protection
  if (currentBatteryVoltage > (ChargingVoltageTarget + 0.2)) {
    digitalWrite(4, 0);  // Kill field immediately
    dutyCycle = MinDuty;
    setDutyPercent((int)dutyCycle);
    fieldCollapseTime = millis();
    queueConsoleMessage("EMERGENCY: Field collapsed - voltage spike (" + String(currentBatteryVoltage, 2) + "V) - disabled for 10 seconds");
    return;
  }

  // Maintain field collapse lockout for 10 seconds after emergency shutdown
  if (fieldCollapseTime > 0 && (millis() - fieldCollapseTime) < FIELD_COLLAPSE_DELAY) {
    digitalWrite(4, 0);
    dutyCycle = MinDuty;
    setDutyPercent((int)dutyCycle);
    return;
  }

  // Emergency temperature shutdown - 10°F above normal limit
  if (!IgnoreTemperature && TempToUse > (TemperatureLimitF + 10)) {
    digitalWrite(4, 0);  // Kill field immediately
    dutyCycle = MinDuty;
    setDutyPercent((int)dutyCycle);
    fieldCollapseTime = millis();  // Reuse 10-second lockout mechanism
    queueConsoleMessage("EMERGENCY: Temp " + String(TempToUse) + "°F exceeded limit by 10° - field disabled for 10 seconds");
    return;
  }

  // Clear field collapse flag after delay expires
  if (fieldCollapseTime > 0 && (millis() - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) {
    fieldCollapseTime = 0;
    queueConsoleMessage("Field collapse delay expired - normal operation resumed");
  }

  // === BMS OVERRIDE LOGIC ===

  // Allow external Battery Management System to disable charging
  if (bmsLogic == 1) {
    bmsSignalActive = !digitalRead(36);  // Inverted due to optocoupler
    if (bmsLogicLevelOff == 0) {
      chargingEnabled = chargingEnabled && bmsSignalActive;  // BMS LOW = stop charging
    } else {
      chargingEnabled = chargingEnabled && !bmsSignalActive;  // BMS HIGH = stop charging
    }
  }

  // === MAIN FIELD CONTROL ===

  if (chargingEnabled) {
    digitalWrite(4, 1);  // Enable field MOSFET

    if (ManualFieldToggle == 0) {  // Automatic PID mode

      // --- TARGET CALCULATION PIPELINE (only when PID update is due) ---
      if (pidUpdateDue) {

        // Step 1: Get base target from learning table (RPM-dependent)
        getLearningTargetFromRPM();
        if (learningTargetFromRPM > 0) {
          uTargetAmps = learningTargetFromRPM;
        } else {
          uTargetAmps = 10;  // Safe fallback for invalid RPM range
        }

        // Step 2: Apply ambient temperature correction
        if (EnableAmbientCorrection == 1) {
          ambientTempCorrection = (ambientTemp - AmbientTempBaseline) * AmbientTempCorrectionFactor;
          uTargetAmps += ambientTempCorrection;
        } else {
          ambientTempCorrection = 0;
        }

        // Step 3: Apply overheat penalty (temporary reduction after overheating event)
        if (overheatingPenaltyTimer > 0) {
          uTargetAmps -= overheatingPenaltyAmps;
          overheatingPenaltyTimer -= PidSampleTime;
          if (overheatingPenaltyTimer <= 0) {
            overheatingPenaltyTimer = 0;
            overheatingPenaltyAmps = 0;
            queueConsoleMessage("Learning: Overheat penalty expired");
          }
        }

        // Store pre-override target for display purposes
        finalLearningTarget = uTargetAmps;

        // Step 4: Apply HiLow switch (50% reduction in Low mode)
        if (HiLow == 0) {  // Low mode
          uTargetAmps *= 0.5;
          LearningPaused = true;  // Prevent learning from corrupted low-power data
        }

        // Step 5: Apply special mode overrides
        if (ForceFloat == 1) {
          uTargetAmps = 0;  // Float charging - target zero battery current
        }
        if (weatherModeEnabled == 1 && currentWeatherMode == 1) {
          uTargetAmps = -99;  // Solar detected - pause charging (forces minimum duty)
        }

        // Step 6: Select current measurement source
        if (ForceFloat == 1) {
          targetCurrent = Bcur;  // Float mode uses battery current
        } else {
          targetCurrent = getTargetAmps();  // Normal mode uses configured sensor
        }

        // Step 7: Select temperature measurement source
        if (TempSource == 0) {
          TempToUse = AlternatorTemperatureF;
        } else {
          TempToUse = temperatureThermistor;
        }

        // --- PID CONTROL ---

        pidInput = targetCurrent;   // Current reading (process variable)
        pidSetpoint = uTargetAmps;  // Desired target (setpoint)

        bool pidComputed = currentPID.Compute();  // Calculate new duty cycle
        dutyCycle = pidOutput;                    // PID library updates pidOutput

        // --- LEARNING LOGIC ---

        // Only learn when PID computed, learning enabled, and not paused
        if (pidComputed && LearningMode == 1 && !LearningPaused) {
          processLearningLogic();
        }

        lastPidUpdate = currentMillis;  // Mark timing for next cycle
      }

    } else {  // Manual override mode
      dutyCycle = ManualDutyTarget;
      uTargetAmps = 0;
      dutyCycle = constrain(dutyCycle, 0, 100);
    }

  } else {
    // Charging disabled - shutdown field
    digitalWrite(4, 0);
    dutyCycle = MinDuty;
    uTargetAmps = 0;
  }

  // === SAFETY OVERRIDES (throttled to PID sample rate to avoid fighting PID) ===
  if (pidUpdateDue) {

    // Temperature protection - aggressive reduction
    if (!IgnoreTemperature && TempToUse > TemperatureLimitF && dutyCycle > (MinDuty + 2 * dutyStep)) {
      dutyCycle -= 2 * dutyStep;
      static unsigned long lastTempProtectionWarning = 0;
      if (millis() - lastTempProtectionWarning > 10000) {
        queueConsoleMessage("Temp limit reached, backing off...");
        lastTempProtectionWarning = millis();
      }
    }

    // Voltage protection - most aggressive reduction
    if (currentBatteryVoltage > ChargingVoltageTarget && dutyCycle > (MinDuty + 3 * dutyStep)) {
      dutyCycle -= 3 * dutyStep;
      static unsigned long lastVoltageProtectionWarning = 0;
      if (millis() - lastVoltageProtectionWarning > 10000) {
        queueConsoleMessage("Voltage limit reached, backing off...");
        lastVoltageProtectionWarning = millis();
      }
    }

    // Battery current protection
    if (Bcur > MaximumAllowedBatteryAmps && dutyCycle > (MinDuty + dutyStep)) {
      dutyCycle -= dutyStep;
      static unsigned long lastCurrentProtectionWarning = 0;
      if (millis() - lastCurrentProtectionWarning > 10000) {
        queueConsoleMessage("Battery current limit reached, backing off...");
        lastCurrentProtectionWarning = millis();
      }
    }
  }

  Serial.println("Final dutyCycle after safety checks but before constrains: " + String(dutyCycle) + "%");

  // === FINAL OUTPUT ===

  // Enforce hard limits
  dutyCycle = constrain(dutyCycle, MinDuty, MaxDuty);

  // Apply duty cycle to PWM hardware (only when changed or on PID update)
  // NOTE: This still runs every loop, but could be optimized to only call when dutyCycle changes.  Probably not worth the complexity right now.
  setDutyPercent((int)dutyCycle);

  // Calculate field voltage and current for monitoring
  vvout = dutyCycle / 100 * currentBatteryVoltage;
  iiout = vvout / FieldResistance;

  // Mark calculated values as fresh for telemetry
  MARK_FRESH(IDX_DUTY_CYCLE);
  MARK_FRESH(IDX_FIELD_VOLTS);
  MARK_FRESH(IDX_FIELD_AMPS);

  // Update field status indicator for display
  fieldActiveStatus = (chargingEnabled && (fieldCollapseTime == 0 || (millis() - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) && (dutyCycle > 0)) ? 1 : 0;
}

void getLearningTargetFromRPM() {
  currentRPMTableIndex = -1;  // Reset global

  // Find which RPM range we're in
  for (int i = 0; i < RPM_TABLE_SIZE - 1; i++) {
    if (RPM >= rpmTableRPMPoints[i] && RPM < rpmTableRPMPoints[i + 1]) {
      currentRPMTableIndex = i;
      break;
    }
  }

  // Handle edge case: RPM above highest table point
  if (RPM >= rpmTableRPMPoints[RPM_TABLE_SIZE - 1]) {
    currentRPMTableIndex = RPM_TABLE_SIZE - 1;
  }

  // Guard against invalid index
  if (currentRPMTableIndex < 0 || currentRPMTableIndex >= RPM_TABLE_SIZE) {
    currentRPMTableIndex = -1;
    learningTargetFromRPM = -1;  // Signal invalid RPM range
    return;
  }

  learningTargetFromRPM = rpmCurrentTable[currentRPMTableIndex];
}

void processLearningLogic() {
  // === RPM TRANSITION FILTERING ===
  // Check if RPM has changed significantly
  if (abs(RPM - lastStableRPM) > LearningRPMChangeThreshold) {
    lastSignificantRPMChange = millis();
    lastStableRPM = RPM;
    if (ShowLearningDebugMessages) {
      queueConsoleMessage("Learning: RPM transition detected (" + String(RPM) + " RPM) - settling for " + String(LearningSettlingPeriod / 1000) + "s");
    }
  }
  // Block learning during settling period
  if (millis() - lastSignificantRPMChange < LearningSettlingPeriod) {
    return;  // Exit early, don't learn during transitions
  }
  // Skip learning during penalty period
  if (IgnoreLearningDuringPenalty && overheatingPenaltyTimer > 0) {
    return;
  }
  // Skip if invalid RPM range
  if (currentRPMTableIndex < 0 || currentRPMTableIndex >= RPM_TABLE_SIZE) {
    return;
  }
  // Throttle learning updates
  if (millis() - lastTableUpdateTime < MinLearningInterval) {
    return;
  }
  // Track session time for this RPM range
  unsigned long sessionTime = millis() - learningSessionStartTime;
  // === OVERHEAT DETECTION ===
  if (TempToUse > TemperatureLimitF) {
    if (!LearningDownwardEnabled) return;  // Skip if downward learning disabled
    // Record overheat event
    lastOverheatTime[currentRPMTableIndex] = millis();
    overheatCount[currentRPMTableIndex]++;
    totalOverheats++;
    totalLearningEvents++;
    // Apply learning reduction to current point
    rpmCurrentTable[currentRPMTableIndex] -= LearningDownStep;
    // Apply neighbor reductions if enabled
    if (EnableNeighborLearning) {
      float neighborReduction = LearningDownStep * NeighborLearningFactor;
      if (currentRPMTableIndex > 0) {
        rpmCurrentTable[currentRPMTableIndex - 1] -= neighborReduction;
      }
      if (currentRPMTableIndex < RPM_TABLE_SIZE - 1) {
        rpmCurrentTable[currentRPMTableIndex + 1] -= neighborReduction;
      }
    }
    // Start overheat penalty timer
    overheatingPenaltyTimer = MaxPenaltyDuration;
    overheatingPenaltyAmps = AlternatorNominalAmps * (MaxPenaltyPercent / 100.0);
    // Bound table values to reasonable limits
    for (int i = 0; i < RPM_TABLE_SIZE; i++) {
      rpmCurrentTable[i] = constrain(rpmCurrentTable[i], MinTableValue, MaxTableValue);
    }
    lastTableUpdateTime = millis();
    if (AutoSaveLearningTable) {
      saveLearningTableToNVS();
    }
    if (ShowLearningDebugMessages) {
      queueConsoleMessage("Learning: Overheat at " + String(rpmTableRPMPoints[currentRPMTableIndex]) + " RPM, reduced to " + String(rpmCurrentTable[currentRPMTableIndex], 1) + "A");
    }
    return;
  }
  // === TEMPERATURE HYSTERESIS - Don't learn upward if still in warm zone ===
  if (TempToUse > (TemperatureLimitF - LearningTempHysteresis)) {
    // Still too hot for upward learning
    if (LearningUpwardEnabled) {
      return;  // Don't increase current until we're clearly below threshold
    }
  }
  // === SAFE OPERATION LEARNING ===
  if (LearningUpwardEnabled && sessionTime > SafeOperationThreshold) {
    // Add to cumulative safe operation time
    cumulativeNoOverheatTime[currentRPMTableIndex] += sessionTime;
    totalSafeHours += sessionTime / 3600000;  // Convert ms to hours
    // Every SafeOperationThreshold of cumulative safe operation, increase by UpStep
    unsigned long safeIntervals = cumulativeNoOverheatTime[currentRPMTableIndex] / SafeOperationThreshold;
    if (safeIntervals > 0) {
      float increase = safeIntervals * LearningUpStep;
      rpmCurrentTable[currentRPMTableIndex] += increase;
      totalLearningEvents++;
      // Reset cumulative timer (we've "used up" this safe time)
      cumulativeNoOverheatTime[currentRPMTableIndex] = 0;
      // Bound to reasonable limits
      rpmCurrentTable[currentRPMTableIndex] = constrain(rpmCurrentTable[currentRPMTableIndex], MinTableValue, MaxTableValue);
      lastTableUpdateTime = millis();
      if (AutoSaveLearningTable) {
        saveLearningTableToNVS();
      }
      if (ShowLearningDebugMessages) {
        queueConsoleMessage("Learning: Safe operation at " + String(rpmTableRPMPoints[currentRPMTableIndex]) + " RPM, increased to " + String(rpmCurrentTable[currentRPMTableIndex], 1) + "A");
      }
    }
  }
  // Calculate diagnostic values
  calculateLearningDiagnostics();
  // Reset session timer for next cycle
  learningSessionStartTime = millis();
}

void calculateLearningDiagnostics() {
  // Calculate average table value
  float sum = 0;
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    sum += rpmCurrentTable[i];
  }
  averageTableValue = sum / RPM_TABLE_SIZE;

  // Find time since last overheat
  timeSinceLastOverheat = 0;
  unsigned long mostRecentOverheat = 0;
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    if (lastOverheatTime[i] > mostRecentOverheat) {
      mostRecentOverheat = lastOverheatTime[i];
    }
  }
  if (mostRecentOverheat > 0) {
    timeSinceLastOverheat = millis() - mostRecentOverheat;
  }
}
void resetLearningTableToDefaults() {
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    rpmCurrentTable[i] = defaultCurrentValues[i];
    rpmTableRPMPoints[i] = defaultRPMValues[i];  // Reset RPM points too
    lastOverheatTime[i] = 0;
    overheatCount[i] = 0;
    cumulativeNoOverheatTime[i] = 0;
  }

  // Clear any active penalty
  overheatingPenaltyTimer = 0;
  overheatingPenaltyAmps = 0;

  // Reset diagnostics
  totalLearningEvents = 0;
  totalOverheats = 0;
  totalSafeHours = 0;

  if (AutoSaveLearningTable) {
    saveLearningTableToNVS();
  }

  queueConsoleMessage("Learning: Table reset to factory defaults");
}

void loadLearningTableFromNVS() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("learning", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("Learning: No saved table, using defaults");
    resetLearningTableToDefaults();
    return;
  }

  size_t required_size = sizeof(rpmCurrentTable);
  err = nvs_get_blob(nvs_handle, "rpmTable", rpmCurrentTable, &required_size);
  if (err != ESP_OK) {
    resetLearningTableToDefaults();
    nvs_close(nvs_handle);
    return;
  }

  // Load RPM breakpoints
  required_size = sizeof(rpmTableRPMPoints);
  err = nvs_get_blob(nvs_handle, "rpmPoints", rpmTableRPMPoints, &required_size);
  if (err != ESP_OK) {
    // If RPM points missing, reset everything to maintain consistency
    resetLearningTableToDefaults();
    nvs_close(nvs_handle);
    return;
  }

  required_size = sizeof(overheatCount);
  nvs_get_blob(nvs_handle, "overheatCount", overheatCount, &required_size);

  required_size = sizeof(lastOverheatTime);
  nvs_get_blob(nvs_handle, "lastOverheat", lastOverheatTime, &required_size);

  required_size = sizeof(cumulativeNoOverheatTime);
  nvs_get_blob(nvs_handle, "cumulativeTime", cumulativeNoOverheatTime, &required_size);

  // Load diagnostic counters
  uint32_t temp_uint32;
  if (nvs_get_u32(nvs_handle, "totalEvents", &temp_uint32) == ESP_OK) totalLearningEvents = temp_uint32;
  if (nvs_get_u32(nvs_handle, "totalOverheats", &temp_uint32) == ESP_OK) totalOverheats = temp_uint32;
  if (nvs_get_u32(nvs_handle, "totalSafeHours", &temp_uint32) == ESP_OK) totalSafeHours = temp_uint32;

  nvs_close(nvs_handle);
  queueConsoleMessage("Learning: Table loaded from NVS");
}

void saveLearningTableToNVS() {
  static unsigned long lastSaveTime = 0;
  if (millis() - lastSaveTime < LearningTableSaveInterval * 1000) {  //throttling
    return;                                                          // Too soon, skip save
  }
  lastSaveTime = millis();
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("learning", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("Learning: Failed to save table");
    return;
  }

  // Save current limits and RPM breakpoints
  nvs_set_blob(nvs_handle, "rpmTable", rpmCurrentTable, sizeof(rpmCurrentTable));
  nvs_set_blob(nvs_handle, "rpmPoints", rpmTableRPMPoints, sizeof(rpmTableRPMPoints));

  // Save historical data
  nvs_set_blob(nvs_handle, "overheatCount", overheatCount, sizeof(overheatCount));
  nvs_set_blob(nvs_handle, "lastOverheat", lastOverheatTime, sizeof(lastOverheatTime));
  nvs_set_blob(nvs_handle, "cumulativeTime", cumulativeNoOverheatTime, sizeof(cumulativeNoOverheatTime));

  // Save diagnostic counters
  nvs_set_u32(nvs_handle, "totalEvents", (uint32_t)totalLearningEvents);
  nvs_set_u32(nvs_handle, "totalOverheats", (uint32_t)totalOverheats);
  nvs_set_u32(nvs_handle, "totalSafeHours", (uint32_t)totalSafeHours);

  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
}

void clearOverheatHistoryAction() {
  // Clear all overheat history arrays and counters
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    overheatCount[i] = 0;
    lastOverheatTime[i] = 0;
  }
  totalOverheats = 0;
  // Save cleared data to NVS if auto-save enabled
  if (AutoSaveLearningTable) {
    saveLearningTableToNVS();
  }
  queueConsoleMessage("Overheat History: All history cleared");
}


bool testInternetSpeed() {
  // Internet speed test - checks if connection is fast enough for operations
  // Returns true if speed >= 5 Kbps, false otherwise
  // Feeds watchdog during test to prevent timeout

  // First check if WiFi is even connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Speed test failed: WiFi not connected");
    queueConsoleMessage("Internet check failed: WiFi not connected");
    return false;
  }

  Serial.println("Testing internet speed...");
  esp_task_wdt_reset();  // Feed watchdog before test

  WiFiClient client;

  // Test 1: Can we connect at all? (3 second timeout)
  unsigned long connectStart = millis();
  if (!client.connect("1.1.1.1", 80, 3000)) {
    Serial.println("Speed test failed: Cannot reach internet");
    queueConsoleMessage("Internet check failed: No internet access on WiFi network");
    esp_task_wdt_reset();
    return false;
  }
  unsigned long connectTime = millis() - connectStart;
  client.stop();

  esp_task_wdt_reset();  // Feed watchdog after connectivity test

  // Test 2: Download small file and measure speed
  HTTPClient http;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();  // Don't validate cert for speed test

  // Use a reliable small file (Cloudflare's trace - about 200-300 bytes)
  String testUrl = "http://cloudflare.com/cdn-cgi/trace";

  http.begin(secureClient, testUrl);
  http.setTimeout(5000);  // 5 second timeout for speed test

  unsigned long downloadStart = millis();
  int httpCode = http.GET();

  esp_task_wdt_reset();  // Feed watchdog after GET request

  if (httpCode != 200) {
    http.end();
    Serial.printf("Speed test failed: HTTP error %d\n", httpCode);
    queueConsoleMessage("Internet check failed: Connection error (HTTP " + String(httpCode) + ")");
    return false;
  }

  String payload = http.getString();
  unsigned long downloadTime = millis() - downloadStart;
  int bytesReceived = payload.length();

  http.end();
  esp_task_wdt_reset();  // Feed watchdog after completing test

  // Calculate speed in bytes per second
  float bytesPerSecond = 0;
  if (downloadTime > 0) {
    bytesPerSecond = (bytesReceived * 1000.0) / downloadTime;  // bytes/sec
  }

  float kbps = (bytesPerSecond * 8.0) / 1000.0;  // Convert to kilobits per second

  Serial.printf("Speed test result: %.2f Kbps (%d bytes in %lu ms)\n", kbps, bytesReceived, downloadTime);

  // Require minimum 5 Kbps
  if (kbps < 5.0) {
    Serial.printf("Speed test failed: Connection too slow (%.2f Kbps < 5 Kbps minimum)\n", kbps);
    queueConsoleMessage("Internet too slow: " + String(kbps, 1) + " Kbps (need 5+ Kbps)");
    return false;
  }

  Serial.printf("Speed test passed: %.2f Kbps\n", kbps);
  queueConsoleMessage("Internet speed OK: " + String(kbps, 1) + " Kbps");
  return true;
}

void StuffToDoAtSomePoint() {
  //every reset button has a pointless flag and an echo.  I did not delete them for fear of breaking the payload and they hardly cost anything to keep
  //Battery Voltage Source drop down menu- make this text update on page re-load instead of just an echo number
  // Is it possible to power up if the igniton is off?  A bunch of setup functions may fail?
}
