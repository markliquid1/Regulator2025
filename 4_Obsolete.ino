    // // Temporary debug - remove it later
    // static unsigned long lastForceRead = 0;
    // if (millis() - lastForceRead > 1000) {  // Every 1 seconds
    //   Serial.println("=== FORCED INA228 READ ===");
    //   try {
    //     float testVoltage = INA.getBusVoltage();
    //     float testCurrent = INA.getShuntVoltage() * 1000;
    //     Serial.printf("Direct read: %.3fV, %.3fmV shunt\n", testVoltage, testCurrent);
    //   } catch (...) {
    //     Serial.println("INA228 direct read FAILED");
    //   }
    //   lastForceRead = millis();
    // }





// bool setupDisplay() {
//   // Add delay for ESP32 stabilization
//   delay(100);
//   try {
//     // Initialize SPI
//     SPI.begin();
//     delay(50);                  // Let SPI settle
//     SPI.setFrequency(1000000);  // Start slow for stability
//     SPI.setDataMode(SPI_MODE0);
//     SPI.setBitOrder(MSBFIRST);
//     SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
//     SPI.endTransaction();
//     u8g2.begin();
//     u8g2.clearBuffer();
//     u8g2.sendBuffer();
//     displayAvailable = true;
//     Serial.println("Display initialized successfully");
//     queueConsoleMessage("Display initialized successfully");
//     return true;
//   } catch (...) {
//     Serial.println("Display initialization failed - exception caught");
//     queueConsoleMessage("Display initialization failed - exception caught");
//     displayAvailable = false;
//     return false;
//   }
// }
// void UpdateDisplay() {
//   // Double-check display availability
//   if (!displayAvailable) {
//     return;
//   }
//   static unsigned long lastDisplayUpdate = 0;
//   const unsigned long DISPLAY_UPDATE_INTERVAL = 3000;  // 3 seconds
//   const unsigned long DISPLAY_TIMEOUT = 2000;          // 2 seconds

//   unsigned long currentTime = millis();

//   // Check if it's time to update display
//   if (currentTime - lastDisplayUpdate <= DISPLAY_UPDATE_INTERVAL) {
//     return;
//   }

//   // Add a try-catch around all display operations
//   try {
//     unsigned long displayStart = currentTime;

//     // Try display operations with timeout
//     u8g2.clearBuffer();
//     if (millis() - displayStart > DISPLAY_TIMEOUT) {
//       Serial.println("Display timeout - disabling display");
//       queueConsoleMessage("Display timeout - disabling display");
//       displayAvailable = false;
//       lastDisplayUpdate = millis();
//       return;
//     }

//     u8g2.setFont(u8g2_font_6x10_tf);

//     // Row 1 (y=10)
//     u8g2.drawStr(0, 10, "Vlts:");
//     u8g2.setCursor(35, 10);
//     u8g2.print(BatteryV, 2);
//     u8g2.drawStr(79, 10, "R:");
//     u8g2.setCursor(90, 10);
//     u8g2.print(RPM, 0);

//     // Row 2 (y=20)
//     u8g2.drawStr(0, 20, "Acur:");
//     u8g2.setCursor(35, 20);
//     u8g2.print(MeasuredAmps, 1);
//     u8g2.drawStr(79, 20, "VV:");
//     u8g2.setCursor(90, 20);
//     u8g2.print(VictronVoltage, 2);

//     // Check timeout partway through
//     if (millis() - displayStart > DISPLAY_TIMEOUT) {
//       Serial.println("Display timeout during updates - disabling display");
//       queueConsoleMessage("Display timeout during updates - disabling display");
//       displayAvailable = false;
//       lastDisplayUpdate = millis();
//       return;
//     }

//     // Row 3 (y=30)
//     u8g2.drawStr(0, 30, "Temp:");
//     u8g2.setCursor(35, 30);
//     u8g2.print(AlternatorTemperatureF, 1);
//     u8g2.drawStr(79, 30, "t:");
//     u8g2.setCursor(90, 30);
//     u8g2.print("extra");

//     // Row 4 (y=40)
//     u8g2.drawStr(0, 40, "PWM%:");
//     u8g2.setCursor(35, 40);
//     u8g2.print(dutyCycle, 1);
//     u8g2.drawStr(79, 40, "H:");
//     u8g2.setCursor(90, 40);
//     u8g2.print(HeadingNMEA);

//     // Row 5 (y=50)
//     u8g2.drawStr(0, 50, "Vout:");
//     u8g2.setCursor(35, 50);
//     u8g2.print(vvout, 2);

//     // Row 6 (y=60)
//     u8g2.drawStr(0, 60, "Bcur:");
//     u8g2.setCursor(35, 60);
//     u8g2.print(Bcur, 1);

//     // Final timeout check before sendBuffer()
//     if (millis() - displayStart > DISPLAY_TIMEOUT) {
//       Serial.println("Display timeout before sendBuffer - disabling display");
//       queueConsoleMessage("Display timeout before sendBuffer - disabling display");
//       displayAvailable = false;
//       lastDisplayUpdate = millis();
//       return;
//     }

//     u8g2.sendBuffer();

//     // Log if display operations took a long time
//     unsigned long totalTime = millis() - displayStart;
//     if (totalTime > 1000) {
//       Serial.println("Display took: " + String(totalTime) + "ms");
//     }

//     lastDisplayUpdate = millis();

//   } catch (...) {
//     Serial.println("Display operation failed - disabling display");
//     queueConsoleMessage("Display operation failed - disabling display");
//     displayAvailable = false;
//     lastDisplayUpdate = millis();
//   }
// }




// void AdjustField() {  // AdjustField() - PWM Field Control with Freshness Tracking
//   static unsigned long lastFieldAdjustment = 0;
//   unsigned long currentTime = millis();
//   // Check if it's time to adjust field
//   if (currentTime - lastFieldAdjustment <= FieldAdjustmentInterval) {
//     return;
//   }
//   chargingEnabled = (Ignition == 1 && OnOff == 1);  //Charging will be enabled only when both Ignition and OnOff are equal to 1

//   // Check temperature data freshness for safety
//   unsigned long tempAge = currentTime - dataTimestamps[IDX_ALTERNATOR_TEMP];
//   bool tempDataVeryStale = (tempAge > 30000);  // 30 seconds
//   if (tempDataVeryStale) {
//     //ABSTRACT OUT THESE LINES LATER except perhaps the specific error message
//     Serial.println("Onewire sensor stale, sensor dead or disconnected");
//     queueConsoleMessage("OneWire sensor stale, sensor dead or disconnected");
//     digitalWrite(21, HIGH);  // sound alarm
//   }

//   // Check battery sensor redundancy and quit if problem found

//   if (abs(BatteryV - IBV) > 5000) {  /// FIX LATER thereshold
//     //ABSTRACT OUT THESE LINES LATER except perhaps the specific error message
//     static unsigned long lastVoltageDisagreementWarning = 0;
//     digitalWrite(21, HIGH);  // sound alarm
//     digitalWrite(4, 0);      // disable field
//     dutyCycle = MinDuty;     // restart duty cycle from minimum

//     // Throttle console message to once every 10 seconds
//     if (millis() - lastVoltageDisagreementWarning > 10000) {
//       char msg[128];
//       snprintf(msg, sizeof(msg),
//                "Battery Voltage disagreement! BatteryV=%.3f V, IBV=%.3f V. Field shut off for safety!",
//                BatteryV, IBV);
//       queueConsoleMessage(msg);
//       lastVoltageDisagreementWarning = millis();
//     }
//     return;
//   }


//   //Block any field control/changes during auto-zero of current sensor
//   if (autoZeroStartTime > 0) {
//     lastFieldAdjustment = currentTime;
//     return;  // Let processAutoZero() handle field control
//   }

//   //Preparation
//   updateChargingStage();  // Update charging stage (bulk/float logic)
//   float currentBatteryVoltage = getBatteryVoltage();

//   // Emergency field collapse - voltage spike protection
//   if (currentBatteryVoltage > (ChargingVoltageTarget + 0.2)) {
//     digitalWrite(4, 0);  // Immediately disable field
//     dutyCycle = MinDuty;
//     setDutyPercent((int)dutyCycle);
//     fieldCollapseTime = currentTime;  // Record when collapse happened
//     queueConsoleMessage("EMERGENCY: Field collapsed - voltage spike (" + String(currentBatteryVoltage, 2) + "V) - disabled for 10 seconds");
//     return;  // Exit function immediately
//   }

//   // Check if we're still in collapse delay period
//   if (fieldCollapseTime > 0 && (currentTime - fieldCollapseTime) < FIELD_COLLAPSE_DELAY) {
//     digitalWrite(4, 0);  // Keep field off
//     dutyCycle = MinDuty;
//     setDutyPercent((int)dutyCycle);
//     return;  // Exit function, don't do normal field control
//   }

//   // Clear the collapse flag after delay expires
//   if (fieldCollapseTime > 0 && (currentTime - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) {
//     fieldCollapseTime = 0;
//     queueConsoleMessage("Field collapse delay expired - normal operation resumed");
//   }

//   // Check for and handle BMS override toggle by updating chargingEnabled accordingly
//   //bmsLogic is simply another way to turn the alternator OFF
//   if (bmsLogic == 1) {
//     // If BMS signal is active (based on bmsLogicLevelOff setting)
//     bmsSignalActive = !digitalRead(42);  // this is the signal from the BMS itself (need "!"" because of optocouplers)
//     if (bmsLogicLevelOff == 0) {
//       // BMS gives LOW signal when charging NOT desired
//       chargingEnabled = chargingEnabled && bmsSignalActive;
//     } else {
//       // BMS gives HIGH signal when charging NOT desired
//       chargingEnabled = chargingEnabled && !bmsSignalActive;
//     }
//   }

//   //now the normal logic to control field
//   if (chargingEnabled) {           // this could be 0 due to bmsLogic, Igntion, or OnOff
//     digitalWrite(4, 1);            // Enable the MOSFET
//     if (ManualFieldToggle == 0) {  // Automatic mode       // Should move this outside the BMS logic at some point..
//       // Step 1: Determine base target amps from Hi/Low setting
//       if (HiLow == 1) {
//         uTargetAmps = TargetAmps;  // Normal target
//       } else {
//         uTargetAmps = TargetAmpL;  // Low target
//       }
//       // Step 2: Apply RPM-based modification if enabled
//       //this was removed

//       // Step 2.4: Apply ForceFloat override if enabled
//       if (ForceFloat == 1) {
//         // Force float mode: target 0 amps at battery (perfect float charging)
//         uTargetAmps = 0;
//       }
//       // Step 2.45: Apply Weather Mode if enabled and active
//       if (weatherModeEnabled == 1 && currentWeatherMode == 1) {
//         // High solar detected - pause charging
//         uTargetAmps = -99;  // this ensures we're going to be pegged at min duty
//       }

//       // Step 2.5, figure out the actual amps reading of whichever value we are controlling on
//       if (ForceFloat == 1) {
//         // Force float mode: use battery current (should be ~0)
//         targetCurrent = Bcur;
//       } else {
//         // Normal mode: use configured current source
//         targetCurrent = getTargetAmps();  // targetCurrent is actually the current reading of the active ("target") sensor, and uTargetAmps is our actual target
//       }
//       //Step 2.6 figure out the actual temp reading o whichever value we are controlling on
//       if (TempSource == 0) {
//         TempToUse = AlternatorTemperatureF;
//       }
//       if (TempSource == 1) {
//         TempToUse = temperatureThermistor;
//       }
//       // Step 3: Apply control logic to adjust duty cycle
//       // Increase duty cycle if below target and not at maximum
//       if (targetCurrent < uTargetAmps && dutyCycle < (MaxDuty - dutyStep)) {
//         dutyCycle += dutyStep;
//       }
//       // Decrease duty cycle if above target and not at minimum
//       if (targetCurrent > uTargetAmps && dutyCycle > (MinDuty + dutyStep)) {
//         dutyCycle -= dutyStep;
//       }
//       // Temperature protection (more aggressive reduction)
//       if (!IgnoreTemperature && TempToUse > TemperatureLimitF && dutyCycle > (MinDuty + 2 * dutyStep)) {
//         dutyCycle -= 2 * dutyStep;
//         static unsigned long lastTempProtectionWarning = 0;
//         if (millis() - lastTempProtectionWarning > 10000) {
//           queueConsoleMessage("Temp limit reached, backing off...");
//           lastTempProtectionWarning = millis();
//         }
//       }
//       // Voltage protection (most aggressive reduction)
//       if (currentBatteryVoltage > ChargingVoltageTarget && dutyCycle > (MinDuty + 3 * dutyStep)) {
//         dutyCycle -= 3 * dutyStep;
//         static unsigned long lastVoltageProtectionWarning = 0;
//         if (millis() - lastVoltageProtectionWarning > 10000) {
//           queueConsoleMessage("Voltage limit reached, backing off quickly!...");
//           lastVoltageProtectionWarning = millis();
//         }
//       }
//       // Battery current protection (safety limit)
//       if (Bcur > MaximumAllowedBatteryAmps && dutyCycle > (MinDuty + dutyStep)) {
//         dutyCycle -= dutyStep;
//         static unsigned long lastCurrentProtectionWarning = 0;
//         if (millis() - lastCurrentProtectionWarning > 10000) {
//           queueConsoleMessage("Battery current limit reached, backing off...");
//           lastCurrentProtectionWarning = millis();
//         }
//       }
//       // Ensure duty cycle stays within bounds
//       dutyCycle = constrain(dutyCycle, MinDuty, MaxDuty);  //Critical that no charging can happen at MinDuty!!
//     }

//     else {  // Manual override mode
//       Serial.printf("MANUAL MODE: ManualDutyTarget=%d, chargingEnabled=%d, fieldCollapseTime=%lu\n",
//                     ManualDutyTarget, chargingEnabled, fieldCollapseTime);
//       dutyCycle = ManualDutyTarget;
//       uTargetAmps = 0;
//       dutyCycle = constrain(dutyCycle, 0, 100);
//       Serial.printf("MANUAL MODE: Final dutyCycle=%.1f, calling setDutyPercent(%d)\n",
//                     dutyCycle, (int)dutyCycle);
//       setDutyPercent((int)dutyCycle);
//       Serial.printf("MANUAL MODE: Pin 4 state=%d, PWM duty sent=%d\n", digitalRead(4), (int)dutyCycle);
//     }
//   }

//   else {
//     // Charging disabled: shut down field and reset for next enable
//     digitalWrite(4, 0);  // Disable the Field (FieldEnable)
//     dutyCycle = MinDuty;
//     uTargetAmps = 0;  //
//   }

//   // Apply the calculated duty cycle
//   setDutyPercent((int)dutyCycle);
//   dutyCycle = dutyCycle;                            //shoddy work, oh well
//   vvout = dutyCycle / 100 * currentBatteryVoltage;  //
//   iiout = vvout / FieldResistance;

//   // Mark calculated values as fresh - these are always current when calculated
//   MARK_FRESH(IDX_DUTY_CYCLE);
//   MARK_FRESH(IDX_FIELD_VOLTS);
//   MARK_FRESH(IDX_FIELD_AMPS);

//   // Update timer (only once)
//   lastFieldAdjustment = currentTime;
//   // fieldActiveStatus = (chargingEnabled && (fieldCollapseTime == 0 || (currentTime - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) && (dutyCycle > (MinDuty + 1.0))) ? 1 : 0; // old delete later problematic
//   fieldActiveStatus = (chargingEnabled &&                                                                        //The overall charging system must be enabled
//                        (fieldCollapseTime == 0 || (currentTime - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) &&  //   "Either we're in normal operation, OR if there was an emergency voltage spike that shut down the field, enough time has passed that it's safe to operate again."
//                                                                                                                  //Above prevents the field status from showing "ACTIVE" during the 10-second emergency cooldown period after a voltage spike, even if the other conditions (charging enabled, duty > 0) are met.
//                        (dutyCycle > 0))
//                         ? 1
//                         : 0;  // The field duty cycle must be greater than zero (field is actually energized)
// }