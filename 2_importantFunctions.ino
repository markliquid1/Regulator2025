/**
 * AI_SUMMARY: Important functions to support Xregulator
 * AI_PURPOSE: 
 * AI_INPUTS: 
 * AI_OUTPUTS: 
 * AI_DEPENDENCIES: 
 * AI_RISKS: 
 * AI_OPTIMIZE: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat.  When impossible, always mimik my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instrucutions for me to edit if function is long.  Always specify which option you chose.
 */


bool ensureLittleFS() {
  if (littleFSMounted) {
    return true;
  }
  Serial.println("Initializing LittleFS...");
  if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
    Serial.println("CRITICAL: LittleFS mount failed! Attempting format...");
    if (!LittleFS.begin(true)) {
      Serial.println("CRITICAL: LittleFS format failed - filesystem unavailable");
      littleFSMounted = false;
      return false;
    } else {
      Serial.println("LittleFS formatted and mounted successfully");
      littleFSMounted = true;
      return true;
    }
  } else {
    Serial.println("LittleFS mounted successfully");
  }
  littleFSMounted = true;
  return true;
}
void initializeHardware() {  // Helper function to organize hardware initialization

  Serial.println("Starting hardware initialization...");
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

  //Victron VeDirect
  Serial1.begin(19200, SERIAL_8N1, 25, -1, 0);  // ... note the "0" at end for normal logic.  This is the reading of the combined NMEA0183 data from YachtDevices
  Serial2.begin(19200, SERIAL_8N1, 26, -1, 1);  // This is the reading of Victron VEDirect
  Serial2.flush();

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
  INA.setMode(11);                                                      // Bh = Continuous shunt and bus voltage
  INA.setAverage(4);                                                    //0h = 1, 1h = 4, 2h = 16, 3h = 64, 4h = 128, 5h = 256, 6h = 512, 7h = 1024     Applies to all channels
  INA.setBusVoltageConversionTime(7);                                   // Sets the conversion time of the bus voltage measurement: 0h = 50 µs, 1h = 84 µs, 2h = 150 µs, 3h = 280 µs, 4h = 540 µs, 5h = 1052 µs, 6h = 2074 µs, 7h = 4120 µs
  INA.setShuntVoltageConversionTime(7);                                 // Sets the conversion time of the bus voltage measurement: 0h = 50 µs, 1h = 84 µs, 2h = 150 µs, 3h = 280 µs, 4h = 540 µs, 5h = 1052 µs, 6h = 2074 µs, 7h = 4120 µs
  uint16_t thresholdLSB = (uint16_t)(VoltageHardwareLimit / 0.003125);  //  INA228 uses 3.125mV per LSB for bus voltage
  INA.setBusOvervoltageTH(thresholdLSB);
  INA.setDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT);  // Enable bus overvoltage alert
  queueConsoleMessage("INA228 Direct Hardware Overvoltage Protection Enabled");


  if (setupDisplay()) {
    Serial.println("Display ready for use");
  } else {
    Serial.println("Continuing without display");
  }

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
  adc.setSampleRate(ADS1115_REG_CONFIG_DR_8SPS);  //Set the slowest and most accurate sample rate, 8
  // ADS1115_REG_CONFIG_DR_8SPS(0x0000)              // 8 SPS(Sample per Second), or a sample every 125ms
  // ADS1115_REG_CONFIG_DR_16SPS(0x0020)             // 16 SPS, or every 62.5ms
  // ADS1115_REG_CONFIG_DR_32SPS(0x0040)             // 32 SPS, or every 31.3ms
  // ADS1115_REG_CONFIG_DR_64SPS(0x0060)             // 64 SPS, or every 15.6ms
  // ADS1115_REG_CONFIG_DR_128SPS(0x0080)            // 128 SPS, or every 7.8ms  (default)
  // ADS1115_REG_CONFIG_DR_250SPS(0x00A0)            // 250 SPS, or every 4ms, note that noise free resolution is reduced to ~14.75-16bits, see table 2 in datasheet
  // ADS1115_REG_CONFIG_DR_475SPS(0x00C0)            // 475 SPS, or every 2.1ms, note that noise free resolution is reduced to ~14.3-15.5bits, see table 2 in datasheet
  // ADS1115_REG_CONFIG_DR_860SPS(0x00E0)            // 860 SPS, or every 1.16ms, note that noise free resolution is reduced to ~13.8-15bits, see table 2 in datasheet

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
  if (!LittleFS.exists("/AlternatorTemperatureLimitF.txt")) {
    writeFile(LittleFS, "/AlternatorTemperatureLimitF.txt", String(AlternatorTemperatureLimitF).c_str());
  } else {
    AlternatorTemperatureLimitF = readFile(LittleFS, "/AlternatorTemperatureLimitF.txt").toInt();
  }
  if (!LittleFS.exists("/ManualDuty.txt")) {
    writeFile(LittleFS, "/ManualDuty.txt", String(ManualDutyTarget).c_str());
  } else {
    ManualDutyTarget = readFile(LittleFS, "/ManualDuty.txt").toInt();  //
  }
  if (!LittleFS.exists("/FullChargeVoltage.txt")) {
    writeFile(LittleFS, "/FullChargeVoltage.txt", String(ChargingVoltageTarget).c_str());
  } else {
    ChargingVoltageTarget = readFile(LittleFS, "/FullChargeVoltage.txt").toFloat();
  }
  if (!LittleFS.exists("/TargetAmps.txt")) {
    writeFile(LittleFS, "/TargetAmps.txt", String(TargetAmps).c_str());
  } else {
    TargetAmps = readFile(LittleFS, "/TargetAmps.txt").toInt();
  }
  if (!LittleFS.exists("/SwitchingFrequency.txt")) {
    writeFile(LittleFS, "/SwitchingFrequency.txt", String(fffr).c_str());
  } else {
    fffr = readFile(LittleFS, "/SwitchingFrequency.txt").toInt();
  }
  if (!LittleFS.exists("/TargetFloatVoltage.txt")) {
    writeFile(LittleFS, "/TargetFloatVoltage.txt", String(TargetFloatVoltage).c_str());
  } else {
    TargetFloatVoltage = readFile(LittleFS, "/TargetFloatVoltage.txt").toFloat();
  }
  if (!LittleFS.exists("/interval.txt")) {
    writeFile(LittleFS, "/interval.txt", String(interval).c_str());
  } else {
    interval = readFile(LittleFS, "/interval.txt").toFloat();
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

  if (!LittleFS.exists("/AmpControlByRPM.txt")) {
    writeFile(LittleFS, "/AmpControlByRPM.txt", String(AmpControlByRPM).c_str());
  } else {
    AmpControlByRPM = readFile(LittleFS, "/AmpControlByRPM.txt").toInt();
  }
  if (!LittleFS.exists("/RPM1.txt")) {
    writeFile(LittleFS, "/RPM1.txt", String(RPM1).c_str());
  } else {
    RPM1 = readFile(LittleFS, "/RPM1.txt").toInt();
  }

  if (!LittleFS.exists("/RPM2.txt")) {
    writeFile(LittleFS, "/RPM2.txt", String(RPM2).c_str());
  } else {
    RPM2 = readFile(LittleFS, "/RPM2.txt").toInt();
  }

  if (!LittleFS.exists("/RPM3.txt")) {
    writeFile(LittleFS, "/RPM3.txt", String(RPM3).c_str());
  } else {
    RPM3 = readFile(LittleFS, "/RPM3.txt").toInt();
  }

  if (!LittleFS.exists("/RPM4.txt")) {
    writeFile(LittleFS, "/RPM4.txt", String(RPM4).c_str());
  } else {
    RPM4 = readFile(LittleFS, "/RPM4.txt").toInt();
  }

  if (!LittleFS.exists("/RPM5.txt")) {
    writeFile(LittleFS, "/RPM5.txt", String(RPM5).c_str());
  } else {
    RPM5 = readFile(LittleFS, "/RPM5.txt").toInt();
  }

  if (!LittleFS.exists("/RPM6.txt")) {
    writeFile(LittleFS, "/RPM6.txt", String(RPM6).c_str());
  } else {
    RPM6 = readFile(LittleFS, "/RPM6.txt").toInt();
  }

  if (!LittleFS.exists("/RPM7.txt")) {
    writeFile(LittleFS, "/RPM7.txt", String(RPM7).c_str());
  } else {
    RPM7 = readFile(LittleFS, "/RPM7.txt").toInt();
  }

  if (!LittleFS.exists("/Amps1.txt")) {
    writeFile(LittleFS, "/Amps1.txt", String(Amps1).c_str());
  } else {
    Amps1 = readFile(LittleFS, "/Amps1.txt").toInt();
  }

  if (!LittleFS.exists("/Amps2.txt")) {
    writeFile(LittleFS, "/Amps2.txt", String(Amps2).c_str());
  } else {
    Amps2 = readFile(LittleFS, "/Amps2.txt").toInt();
  }

  if (!LittleFS.exists("/Amps3.txt")) {
    writeFile(LittleFS, "/Amps3.txt", String(Amps3).c_str());
  } else {
    Amps3 = readFile(LittleFS, "/Amps3.txt").toInt();
  }

  if (!LittleFS.exists("/Amps4.txt")) {
    writeFile(LittleFS, "/Amps4.txt", String(Amps4).c_str());
  } else {
    Amps4 = readFile(LittleFS, "/Amps4.txt").toInt();
  }

  if (!LittleFS.exists("/Amps5.txt")) {
    writeFile(LittleFS, "/Amps5.txt", String(Amps5).c_str());
  } else {
    Amps5 = readFile(LittleFS, "/Amps5.txt").toInt();
  }

  if (!LittleFS.exists("/Amps6.txt")) {
    writeFile(LittleFS, "/Amps6.txt", String(Amps6).c_str());
  } else {
    Amps6 = readFile(LittleFS, "/Amps6.txt").toInt();
  }
  if (!LittleFS.exists("/Amps7.txt")) {
    writeFile(LittleFS, "/Amps7.txt", String(Amps7).c_str());
  } else {
    Amps7 = readFile(LittleFS, "/Amps7.txt").toInt();
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
}

void ReadAnalogInputs() {
  // INA228 Battery Monitor
  if (millis() - lastINARead >= AnalogInputReadInterval) {  // could go down to 600 here, but this logic belongs in Loop anyway
    if (INADisconnected == 0) {
      int start33 = micros();  // Start timing analog input reading
      lastINARead = millis();
      if (INA.getDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT)) {  // this is direct hardware protection for an overvoltage condition, bypassing the ESP32 entirely
        queueConsoleMessage("WARNING: INA228 overvoltage tripped!  Field MOSFET disabled until corrected");
        INA.clearDiagnoseAlertBit(INA228_DIAG_BUS_OVER_LIMIT);  // Clear the alert bit for next detection
      }
      try {
        IBV = INA.getBusVoltage();
        ShuntVoltage_mV = INA.getShuntVoltage_mV();

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
        Serial.println("INA228 read failed");
        queueConsoleMessage("INA228 read failed");
      }
    }
  }

  //ADS1115 reading is based on trigger→wait→read so as to not waste time
  if (ADS1115Disconnected != 0) {
    queueConsoleMessage("theADS1115 was not connected and triggered a return");
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
            Channel0V = Raw / 32767.0 * 6.144 / 0.0697674419;  // voltage divider is 1,000,000 and 75,000 ohms
            BatteryV = Channel0V;
            if (BatteryV > 5.0 && BatteryV < 70.0) {  // Sanity check
              MARK_FRESH(IDX_BATTERY_V);              // Only mark fresh on valid reading
            }
            break;
          case 1:
            Channel1V = Raw / 32767.0 * 6.144 * 2;   // voltage divider is 2:1, so this gets us to volts
            MeasuredAmps = (Channel1V - 2.5) * 100;  // alternator current
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
            break;
          case 2:
            Channel2V = Raw / 32767.0 * 2 * 6.144 * RPMScalingFactor;
            RPM = Channel2V;
            if (RPM < 100) {
              RPM = 0;
            }
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

        if (adsCurrentChannel == 0) {
          prev_millis3 = now;  // finished full cycle
        }
      }
      break;
  }
}

void ReadVEData() {
  if (VeData == 1) {
    if (millis() - prev_millis33 > 2000) {  // read VE data every 2 seconds

      int start1 = micros();      // Start timing VeData
      bool dataReceived = false;  // Track if we got any valid data

      while (Serial2.available()) {
        myve.rxData(Serial2.read());
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
        yield();  // not sure what this does
      }
      int end1 = micros();     // End timing
      VeTime = end1 - start1;  // Store elapsed time
      prev_millis33 = millis();
    }
  }
}
void TempTask(void *parameter) {
  esp_task_wdt_add(NULL);

  // Use static variables to reduce stack usage
  static uint8_t scratchPad[9];
  static unsigned long lastTempRead = 0;

  for (;;) {
    unsigned long now = millis();

    // Only read temperature every 10 seconds (increased from 5)
    if (now - lastTempRead < 10000) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      esp_task_wdt_reset();
      continue;
    }

    // Step 1: Trigger conversion
    sensors.requestTemperaturesByAddress(tempDeviceAddress);


    // Step 2: Wait with frequent watchdog feeding - FIXED to feed every iteration
    for (int i = 0; i < 25; i++) {  // 25 x 200ms = 5000ms
      vTaskDelay(pdMS_TO_TICKS(200));
      esp_task_wdt_reset();  // Feed watchdog EVERY iteration, not just every 5th
    }

    // Step 3: Read result
    if (sensors.readScratchPad(tempDeviceAddress, scratchPad)) {
      int16_t raw = (scratchPad[1] << 8) | scratchPad[0];
      float tempC = raw / 16.0;
      float tempF = tempC * 1.8 + 32.0;

      if (tempF > -50 && tempF < 300) {
        AlternatorTemperatureF = tempF;
        MARK_FRESH(IDX_ALTERNATOR_TEMP);
      } else {
        AlternatorTemperatureF = -99;
      }
    } else {
      AlternatorTemperatureF = -99;
    }

    lastTempRead = now;
    esp_task_wdt_reset();
  }
}

void AdjustField() {  // Function 6: AdjustField() - PWM Field Control with Freshness Tracking

  if (millis() - prev_millis22 > FieldAdjustmentInterval) {  // adjust field every FieldAdjustmentInterval milliseconds
                                                             //Block any field control/changes during auto-zero of current sensor
    // Check temperature data freshness for safety
    unsigned long tempAge = millis() - dataTimestamps[IDX_ALTERNATOR_TEMP];
    bool tempDataVeryStale = (tempAge > 30000);  // 30 seconds
    if (tempDataVeryStale) {
      Serial.println("Onewire sensor stale, sensor dead or disconnected");
      queueConsoleMessage("OneWire sensor stale, sensor dead or disconnected");
      digitalWrite(33, HIGH);  // sound alarm
    }

    if (autoZeroStartTime > 0) {
      prev_millis22 = millis();
      return;  // Let processAutoZero() handle field control
    }
    // Update charging stage (bulk/float logic)
    updateChargingStage();
    float currentBatteryVoltage = getBatteryVoltage();
    // Emergency field collapse - voltage spike protection
    if (currentBatteryVoltage > (ChargingVoltageTarget + 0.2) && chargingEnabled) {
      digitalWrite(4, 0);  // Immediately disable field
      dutyCycle = MinDuty;
      setDutyPercent((int)dutyCycle);
      fieldCollapseTime = millis();  // Record when collapse happened
      queueConsoleMessage("EMERGENCY: Field collapsed - voltage spike (" + String(currentBatteryVoltage, 2) + "V) - disabled for 10 seconds");
      return;  // Exit function immediately
    }
    // Check if we're still in collapse delay period
    if (fieldCollapseTime > 0 && (millis() - fieldCollapseTime) < FIELD_COLLAPSE_DELAY) {
      digitalWrite(4, 0);  // Keep field off
      dutyCycle = MinDuty;
      setDutyPercent((int)dutyCycle);
      return;  // Exit function, don't do normal field control
    }
    // Clear the collapse flag after delay expires
    if (fieldCollapseTime > 0 && (millis() - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) {
      fieldCollapseTime = 0;
      queueConsoleMessage("Field collapse delay expired - normal operation resumed");
    }
    // Check if charging should be enabled (ignition on, system enabled, BMS allows)
    chargingEnabled = (Ignition == 1 && OnOff == 1);

    // Check BMS override if BMS logic is enabled--- this is a manual human setting in the user interface
    if (bmsLogic == 1) {
      // If BMS signal is active (based on bmsLogicLevelOff setting)
      bmsSignalActive = !digitalRead(36);  // this is the signal from the BMS itself (need "!"" because of optocouplers)
      if (bmsLogicLevelOff == 0) {
        // BMS gives LOW signal when charging NOT desired
        chargingEnabled = chargingEnabled && bmsSignalActive;
      } else {
        // BMS gives HIGH signal when charging NOT desired
        chargingEnabled = chargingEnabled && !bmsSignalActive;
      }
    }
    if (chargingEnabled) {           // if the BMS doesn't want charging, this is skipped, but otherwise....
      digitalWrite(4, 1);            // Enable the Field FieldEnable
      if (ManualFieldToggle == 0) {  // Automatic mode       // Should move this outside the BMS logic at some point..
        // Step 1: Determine base target amps from Hi/Low setting
        if (HiLow == 1) {
          uTargetAmps = TargetAmps;  // Normal target
        } else {
          uTargetAmps = TargetAmpL;  // Low target
        }
        // Step 2: Apply RPM-based modification if enabled
        if (AmpControlByRPM == 1 && RPM > 100 && RPM < 6000) {
          int rpmBasedAmps = interpolateAmpsFromRPM(RPM);

          // Apply RPM curve but respect Hi/Low setting
          if (HiLow == 0) {
            // In Low mode, use lesser of RPM curve or low setting
            uTargetAmps = min(rpmBasedAmps, (int)(TargetAmpL));
          } else {
            // In Normal mode, use RPM curve directly
            uTargetAmps = rpmBasedAmps;
          }
        }

        // Step 2.4: Apply ForceFloat override if enabled
        if (ForceFloat == 1) {
          // Force float mode: target 0 amps at battery (perfect float charging)
          uTargetAmps = 0;
        }

        // Step 2.5, figure out the actual amps reading of whichever value we are controlling on
        if (ForceFloat == 1) {
          // Force float mode: use battery current (should be ~0)
          targetCurrent = Bcur;
        } else {
          // Normal mode: use configured current source
          targetCurrent = getTargetAmps();
        }
        //Step 2.6 figure out the actual temp reading o whichever value we are controlling on
        if (TempSource == 0) {
          TempToUse = AlternatorTemperatureF;
        }
        if (TempSource == 1) {
          TempToUse = temperatureThermistor;
        }
        // Step 3: Apply control logic to adjust duty cycle
        // Increase duty cycle if below target and not at maximum
        if (targetCurrent < uTargetAmps && dutyCycle < (MaxDuty - dutyStep)) {
          dutyCycle += dutyStep;
        }
        // Decrease duty cycle if above target and not at minimum
        if (targetCurrent > uTargetAmps && dutyCycle > (MinDuty + dutyStep)) {
          dutyCycle -= dutyStep;
        }
        // Temperature protection (more aggressive reduction)
        if (!IgnoreTemperature && TempToUse > AlternatorTemperatureLimitF && dutyCycle > (MinDuty + 2 * dutyStep)) {
          dutyCycle -= 2 * dutyStep;
          queueConsoleMessage("Temp limit reached, backing off...");
        }
        // Voltage protection (most aggressive reduction)
        // float currentBatteryVoltage = getBatteryVoltage();  // was this not redundant?  delete later
        if (currentBatteryVoltage > ChargingVoltageTarget && dutyCycle > (MinDuty + 3 * dutyStep)) {
          dutyCycle -= 3 * dutyStep;
          queueConsoleMessage("Voltage limit reached, backing off...");
        }
        // Battery current protection (safety limit)
        if (Bcur > MaximumAllowedBatteryAmps && dutyCycle > (MinDuty + dutyStep)) {
          dutyCycle -= dutyStep;
          queueConsoleMessage("Battery current limit reached, backing off...");
        }
        // Ensure duty cycle stays within bounds
        dutyCycle = constrain(dutyCycle, MinDuty, MaxDuty);  //Critical that no charging can happen at MinDuty!!
      }

      else {  // Manual override mode
        dutyCycle = ManualDutyTarget;
        uTargetAmps = -99;  // just useful for debugging, delete later
        // Ensure duty cycle stays within bounds
        dutyCycle = constrain(dutyCycle, MinDuty, MaxDuty);
      }
    } else {
      // Charging disabled: shut down field and reset for next enable
      digitalWrite(4, 0);  // Disable the Field (FieldEnable)
      dutyCycle = MinDuty;
      uTargetAmps = -999;  // just useful for debugging, delete later
    }
    // Apply the calculated duty cycle
    setDutyPercent((int)dutyCycle);
    dutyCycle = dutyCycle;                            //shoddy work, oh well
    vvout = dutyCycle / 100 * currentBatteryVoltage;  //
    iiout = vvout / FieldResistance;

    // Mark calculated values as fresh - these are always current when calculated
    MARK_FRESH(IDX_DUTY_CYCLE);
    MARK_FRESH(IDX_FIELD_VOLTS);
    MARK_FRESH(IDX_FIELD_AMPS);

    // Update timer (only once)
    prev_millis22 = millis();

    // /// delete this whole thing later
    // static unsigned long lastRunTime2g = 0;
    // const unsigned long interval2g = 10000;  // 10 seconds

    // if (millis() - lastRunTime2g >= interval2g) {
    //   lastRunTime2g = millis();

    //   if (dutyCycle <= (MinDuty + 1.0)) {
    //     //Serial.println();
    //     // String msg = " utargetAmps=" + String((float)uTargetAmps, 1)
    //     //           + " targetCurrent=" + String(targetCurrent, 1)
    //     //           + " currentBatteryVoltage=" + String(currentBatteryVoltage, 2) + "V"
    //     //           + " TempToUse=" + String((float)TempToUse, 1) + "F"
    //     //          + " dutyCycle=" + String(dutyCycle, 1);
    //     //  queueConsoleMessage(msg);      GREAT DEBUG TOOL ADD BACK IN LATER
    //   }
    // }
    fieldActiveStatus = (
                          // Basic enables
                          (Ignition == 1) && (OnOff == 1) &&

                          // BMS logic (if enabled)
                          (bmsLogic == 0 || (bmsLogicLevelOff == 0 ? bmsSignalActive : !bmsSignalActive)) &&

                          // Not in emergency field collapse
                          (fieldCollapseTime == 0 || (millis() - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) &&

                          // Duty cycle is meaningfully above minimum
                          (dutyCycle > (MinDuty + 1.0)) &&

                          // Physical field enable pin is actually HIGH
                          (digitalRead(4) == HIGH))
                          ? 1
                          : 0;
  }
}
void setDutyPercent(int percent) {  // Function to set PWM duty cycle by percentage
  percent = constrain(percent, 0, 100);
  uint32_t duty = (65535UL * percent) / 100;
  ledcWrite(pwmPin, duty);  // In v3.x, first parameter is the pin number
}
bool setupDisplay() {
  // Add delay for ESP32 stabilization
  delay(100);
  try {
    // Initialize SPI 
    SPI.begin();
    delay(50);                  // Let SPI settle
    SPI.setFrequency(1000000);  // Start slow for stability
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI.endTransaction();
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    displayAvailable = true;
    Serial.println("Display initialized successfully");
    queueConsoleMessage("Display initialized successfully");
    return true;
  } catch (...) {
    Serial.println("Display initialization failed - exception caught");
    queueConsoleMessage("Display initialization failed - exception caught");
    displayAvailable = false;
    return false;
  }
}
void UpdateDisplay() {
  // Double-check display availability
  if (!displayAvailable) {
    return;
  }

  // Add a try-catch around all display operations
  try {
    if (millis() - prev_millis66 > 3000) {  // update display every 3 seconds
      unsigned long displayStart = millis();

      // Try display operations with timeout
      u8g2.clearBuffer();
      if (millis() - displayStart > 2000) {
        Serial.println("Display timeout - disabling display");
        queueConsoleMessage("Display timeout - disabling display");
        displayAvailable = false;
        prev_millis66 = millis();
        return;
      }
      u8g2.setFont(u8g2_font_6x10_tf);
      // Row 1 (y=10)
      u8g2.drawStr(0, 10, "Vlts:");
      u8g2.setCursor(35, 10);
      u8g2.print(BatteryV, 2);
      u8g2.drawStr(79, 10, "R:");
      u8g2.setCursor(90, 10);
      u8g2.print(RPM, 0);
      // Row 2 (y=20)
      u8g2.drawStr(0, 20, "Acur:");
      u8g2.setCursor(35, 20);
      u8g2.print(MeasuredAmps, 1);
      u8g2.drawStr(79, 20, "VV:");
      u8g2.setCursor(90, 20);
      u8g2.print(VictronVoltage, 2);
      // Check timeout partway through
      if (millis() - displayStart > 2000) {
        Serial.println("Display timeout during updates - disabling display");
        queueConsoleMessage("Display timeout during updates - disabling display");
        displayAvailable = false;
        prev_millis66 = millis();
        return;
      }
      // Row 3 (y=30)
      u8g2.drawStr(0, 30, "Temp:");
      u8g2.setCursor(35, 30);
      u8g2.print(AlternatorTemperatureF, 1);
      u8g2.drawStr(79, 30, "t:");
      u8g2.setCursor(90, 30);
      u8g2.print("extra");
      // Row 4 (y=40)
      u8g2.drawStr(0, 40, "PWM%:");
      u8g2.setCursor(35, 40);
      u8g2.print(dutyCycle, 1);
      u8g2.drawStr(79, 40, "H:");
      u8g2.setCursor(90, 40);
      u8g2.print(HeadingNMEA);
      // Row 5 (y=50)
      u8g2.drawStr(0, 50, "Vout:");
      u8g2.setCursor(35, 50);
      u8g2.print(vvout, 2);
      // Row 6 (y=60)
      u8g2.drawStr(0, 60, "Bcur:");
      u8g2.setCursor(35, 60);
      u8g2.print(Bcur, 1);
      // Final timeout check before sendBuffer()
      if (millis() - displayStart > 2000) {
        Serial.println("Display timeout before sendBuffer - disabling display");
        queueConsoleMessage("Display timeout before sendBuffer - disabling display");
        displayAvailable = false;
        prev_millis66 = millis();
        return;
      }
      u8g2.sendBuffer();
      // Log if display operations took a long time
      unsigned long totalTime = millis() - displayStart;
      if (totalTime > 1000) {
        Serial.println("Display took: " + String(totalTime) + "ms");
      }
      prev_millis66 = millis();
    }
  } catch (...) {
    Serial.println("Display operation failed - disabling display");
    queueConsoleMessage("Display operation failed - disabling display");
    displayAvailable = false;
    prev_millis66 = millis();
  }
}

void SystemTime(const tN2kMsg &N2kMsg) {
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

String readFile(fs::FS &fs, const char *path) {
  if (!littleFSMounted && !ensureLittleFS()) {
    Serial.println("Cannot read file - LittleFS not available: " + String(path));
    return String();
  }

  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- empty file or failed to open file: " + String(path));
    return String();
  }

  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  return fileContent;
}
void writeFile(fs::FS &fs, const char *path, const char *message) {
  if (!littleFSMounted && !ensureLittleFS()) {
    Serial.println("Cannot write file - LittleFS not available: " + String(path));
    return;
  }
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing: " + String(path));
    return;
  }
  if (file.print(message)) {
    // Success - file written
  } else {
    Serial.println("- write failed for: " + String(path));
  }
  delay(2);
  file.close();
}

int SafeInt(float f, int scale = 1) {
  // where this is matters!!   Put utility functions like SafeInt() above setup() and loop() , according to ChatGPT.  And I proved it matters.
  return isnan(f) || isinf(f) ? -1 : (int)(f * scale);
}

void printBasicTaskStackInfo() {
  numTasks = uxTaskGetNumberOfTasks();
  if (numTasks > MAX_TASKS) numTasks = MAX_TASKS;  // Prevent overflow

  tasksCaptured = uxTaskGetSystemState(taskArray, numTasks, NULL);

  // Serial.println(F("\n===== TASK STACK REMAINING (BYTES) ====="));
  // Serial.println(F("Task Name        | Core | Stack Remaining | Alert"));

  char coreIdBuffer[8];  // Buffer for core ID display

  for (int i = 0; i < tasksCaptured; i++) {
    const char *taskName = taskArray[i].pcTaskName;
    stackBytes = taskArray[i].usStackHighWaterMark * sizeof(StackType_t);
    core = taskArray[i].xCoreID;

    // Format core ID
    if (core < 0 || core > 16) {
      snprintf(coreIdBuffer, sizeof(coreIdBuffer), "N/A");
    } else {
      snprintf(coreIdBuffer, sizeof(coreIdBuffer), "%d", core);
    }

    const char *alert = "";
    if (
      strcmp(taskName, "IDLE0") == 0 || strcmp(taskName, "IDLE1") == 0 || strcmp(taskName, "ipc0") == 0 || strcmp(taskName, "ipc1") == 0) {
      if (stackBytes < 256) {
        alert = "LOW STACK";
      }
    } else {
      if (stackBytes < 256) {
        alert = "LOW STACK";
      } else if (stackBytes < 512) {
        alert = "WARN";
      }
    }


    //   Serial.printf("%-16s |  %-3s  |     %5d B     | %s\n",
    //                 taskName,
    //                 coreIdBuffer,
    //                 stackBytes,
    //                 alert);
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
    updateCpuLoad();            //~200–250 for 10 tasks
    testTaskStats();            //unknown
    prev_millis544 = now544;
  }
}
void updateSystemHealthMetrics() {
  unsigned long now = millis();

  // Update session minimum heap
  if (FreeHeap < sessionMinHeap) {
    sessionMinHeap = FreeHeap;
  }

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
      const char *taskName = taskArray[i].pcTaskName;

      if (stackBytes < 256) {
        queueConsoleMessage("CRITICAL: " + String(taskName) + " stack very low (" + String(stackBytes) + "B)");
        stackWarningIssued = true;
      } else if (stackBytes < 412) {  // was 512 but triggering too much
        queueConsoleMessage("WARNING: " + String(taskName) + " stack low (" + String(stackBytes) + "B)");
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




bool checkFactoryReset() {
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
  delay(100);  // Let pin settle

  if (digitalRead(FACTORY_RESET_PIN) == LOW) {  // GPIO35 shorted to GND
    Serial.println("FACTORY RESET: GPIO35 detected shorted to GND");

    // Delete ALL settings files including WiFi mode
    LittleFS.remove(WIFI_MODE_FILE);
    LittleFS.remove("/ssid.txt");
    LittleFS.remove("/pass.txt");
    LittleFS.remove(AP_PASSWORD_FILE);
    LittleFS.remove(AP_SSID_FILE);
    LittleFS.remove("/password.txt");
    LittleFS.remove("/password.hash");

    // Reset variables to defaults
    permanentAPMode = 0;
    esp32_ap_password = "alternator123";
    esp32_ap_ssid = "ALTERNATOR_WIFI";

    Serial.println("FACTORY RESET: All settings cleared, entering config mode");
    queueConsoleMessage("FACTORY RESET: All settings cleared via hardware reset");
    return true;  // Factory reset performed
  }
  return false;  // Normal boot
}

void setupWiFi() {  // Function to set up WiFi - tries to connect to saved network, falls back to AP mode

  Serial.println("\n=== WiFi Setup Starting ===");

  // Check for factory reset first
  bool factoryResetPerformed = checkFactoryReset();

  // Check if we have a saved mode selection
  bool hasSavedMode = LittleFS.exists(WIFI_MODE_FILE);
  String savedMode = "";

  if (hasSavedMode && !factoryResetPerformed) {
    savedMode = readFile(LittleFS, WIFI_MODE_FILE);
    savedMode.trim();
    Serial.println("Found saved WiFi mode: '" + savedMode + "'");
  }

  //Commented out during development, edit later
  // // Force config mode if: no saved mode OR factory reset performed
  // if (!hasSavedMode || factoryResetPerformed || savedMode.length() == 0) {
  //   Serial.println("=== ENTERING CONFIGURATION MODE ===");
  //   Serial.println("Reason: " + String(factoryResetPerformed ? "Factory reset" : "No saved mode"));

  //   setupAccessPoint();
  //   setupWiFiConfigServer();
  //   currentWiFiMode = AWIFI_MODE_AP;
  //   permanentAPMode = 0;
  //   return;
  // }

  // DEVELOPMENT MODE: Auto-connect to hardcoded network on fresh flash
  if (!hasSavedMode || factoryResetPerformed || savedMode.length() == 0) {
    Serial.println("=== DEVELOPMENT MODE: AUTO-CONNECTING TO HARDCODED NETWORK ===");

    // Try to connect to hardcoded credentials
    if (connectToWiFi("MN2G", "5FENYC8ABC", 8000)) {
      // Success - save as client mode for future boots
      writeFile(LittleFS, WIFI_MODE_FILE, "client");
      writeFile(LittleFS, "/ssid.txt", "MN2G");
      writeFile(LittleFS, "/pass.txt", "5FENYC8ABC");
      currentWiFiMode = AWIFI_MODE_CLIENT;
      setupServer();
      Serial.println("=== DEVELOPMENT MODE: Connected and saved credentials ===");
      return;
    } else {
      // Failed - fall back to config mode
      Serial.println("=== DEVELOPMENT MODE: Failed, entering config mode ===");
      setupAccessPoint();
      setupWiFiConfigServer();
      currentWiFiMode = AWIFI_MODE_AP;
      permanentAPMode = 0;
      return;
    }
  }

  // We have a saved mode - proceed with normal operation
  if (savedMode == "ap") {
    Serial.println("=== PERMANENT AP MODE ===");
    permanentAPMode = 1;
    setupAccessPoint();
    currentWiFiMode = AWIFI_MODE_AP;

    if (LittleFS.exists("/index.html")) {
      Serial.println("Serving full alternator interface in AP mode");
      setupServer();
    } else {
      Serial.println("No index.html - serving landing page");
      setupCaptivePortalLanding();
    }

  } else if (savedMode == "client") {
    Serial.println("=== CLIENT MODE ===");

    // Load saved credentials
    String saved_ssid = readFile(LittleFS, WIFI_SSID_FILE);
    String saved_password = readFile(LittleFS, WIFI_PASS_FILE);
    saved_ssid.trim();
    saved_password.trim();

    // Simple validation
    if (saved_ssid.length() == 0) {
      Serial.println("ERROR: Client mode selected but no SSID saved - entering config mode");
      setupAccessPoint();
      setupWiFiConfigServer();
      currentWiFiMode = AWIFI_MODE_AP;
      permanentAPMode = 0;
      return;
    }

    // Try to connect
    if (connectToWiFi(saved_ssid.c_str(), saved_password.c_str(), 8000)) {
      currentWiFiMode = AWIFI_MODE_CLIENT;
      Serial.println("=== CLIENT MODE SUCCESS ===");
      Serial.println("WiFi connected successfully!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      setupServer();
    } else {
      Serial.println("=== CLIENT MODE FAILED ===");
      Serial.println("WiFi connection failed - starting configuration mode");
      setupAccessPoint();
      setupWiFiConfigServer();
      currentWiFiMode = AWIFI_MODE_AP;
      permanentAPMode = 0;
    }
  }

  Serial.println("=== WiFi Setup Complete ===");
}

bool connectToWiFi(const char *ssid, const char *password, unsigned long timeout) {
  if (!ssid || strlen(ssid) == 0) {
    Serial.println("ERROR: No SSID provided for WiFi connection");  // PRESERVES: Your error message style
    return false;
  }

  Serial.printf("Connecting to WiFi: %s\n", ssid);
  Serial.printf("Password length: %d\n", strlen(password));

  // PRESERVES: Your WiFi setup sequence
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Note: Removed WiFi.setAutoConnect(false) as you noted it doesn't exist

  // PRESERVES: Your connection logic
  if (strlen(password) > 0) {
    WiFi.begin(ssid, password);
  } else {
    WiFi.begin(ssid);  // Open network
  }

  unsigned long startTime = millis();
  int attempts = 0;
  const int maxAttempts = timeout / 500;  // Check every 500ms instead of 250ms

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    attempts++;

    // Print progress every 2 seconds (less spam than your original)
    if (attempts % 4 == 0) {
      Serial.printf("WiFi Status: %d, attempt %d/%d\n", WiFi.status(), attempts, maxAttempts);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connection successful!");                         // PRESERVES: Your success message
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());  // PRESERVES: Your IP logging
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());               // PRESERVES: Your signal logging
    wifiSessionStartTime = millis();
    // mDNS setup
    if (MDNS.begin("alternator")) {
      Serial.println("mDNS responder started");  //
      MDNS.addService("http", "tcp", 80);
    }
    return true;
  } else {
    Serial.printf("WiFi connection failed after %lu ms\n", timeout);  // PRESERVES: Your failure message style
    Serial.printf("Final status: %d\n", WiFi.status());               // PRESERVES: Your debug info
    return false;
  }
}
void setupCaptivePortalLanding() {
  Serial.println("Setting up captive portal landing...");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Much smaller landing page to reduce memory usage
    String page = "<!DOCTYPE html><html><head><title>Alternator Regulator Connected</title>";
    page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    page += "<style>";
    page += "body{font-family:Arial;text-align:center;padding:20px;background:#f5f5f5;line-height:1.6}";
    page += ".card{background:white;padding:24px;border-left:4px solid #ff6600;border-radius:8px;max-width:500px;margin:0 auto;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
    page += "h1{color:#333;margin-bottom:1rem;font-size:24px}";
    page += ".success-box{background:#d4edda;border:1px solid #c3e6cb;color:#155724;padding:16px;border-radius:8px;margin:20px 0}";
    page += ".big-button{display:inline-block;background:#ff6600;color:white;padding:16px 32px;text-decoration:none;border-radius:8px;font-weight:bold;font-size:18px;margin:20px 0}";
    page += ".big-button:hover{background:#e65c00}";
    page += ".bookmark-info{background:#f8f9fa;border:1px solid #dee2e6;padding:12px;border-radius:8px;margin:16px 0;font-size:14px}";
    page += ".ip-address{font-family:monospace;font-size:20px;font-weight:bold;color:#ff6600;background:#f8f9fa;padding:8px 12px;border-radius:8px;display:inline-block;margin:8px 0}";
    page += "</style></head><body>";
    page += "<div class='card'>";
    page += "<h1>Alternator Regulator</h1>";
    page += "<div class='success-box'><strong>Successfully Connected!</strong><br>You are now connected to the alternator regulator's WiFi network.</div>";
    page += "<p>Access the full alternator control interface:</p>";
    page += "<a href='http://192.168.4.1' class='big-button'>Open Alternator Interface</a>";
    page += "<div class='bookmark-info'><strong>For easy future access:</strong><br>Bookmark this address: <span class='ip-address'>192.168.4.1</span></div>";
    page += "<p style='margin-top:24px;font-size:14px;color:#666'><strong>Network:</strong> ALTERNATOR_WIFI<br><strong>Device IP:</strong> 192.168.4.1</p>";
    page += "</div></body></html>";
    request->send(200, "text/html", page);
  });
  server.begin();
  Serial.println("Landing page ready");
}
void setupAccessPoint() {  // Function to set up the device as an access point

  Serial.println("=== SETTING UP ACCESS POINT ===");
  Serial.println("Using SSID: '" + esp32_ap_ssid + "'");
  Serial.println("Using password: '" + esp32_ap_password + "'");
  Serial.println("Password length: " + String(esp32_ap_password.length()));
  Serial.println("SSID length: " + String(esp32_ap_ssid.length()));

  WiFi.mode(WIFI_AP);

  bool apStarted = WiFi.softAP(esp32_ap_ssid.c_str(), esp32_ap_password.c_str());

  if (apStarted) {
    Serial.println("=== ACCESS POINT STARTED SUCCESSFULLY ===");
    Serial.print("AP SSID: ");
    Serial.println(esp32_ap_ssid);
    Serial.print("AP Password: ");
    Serial.println(esp32_ap_password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.println("DNS server started for captive portal");

    Serial.println("=== AP SETUP COMPLETE ===");
  } else {
    Serial.println("=== ACCESS POINT FAILED TO START ===");
    Serial.println("This is a critical error!");
    // Try with default settings as fallback
    Serial.println("Trying with default settings as fallback...");
    WiFi.softAP("ALTERNATOR_WIFI", "alternator123");
  }
}
void setupWiFiConfigServer() {
  Serial.println("\n=== SETTING UP WIFI CONFIG SERVER ===");

  // Main config page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("=== CONFIG PAGE REQUEST ===");
    Serial.println("Client IP: " + request->client()->remoteIP().toString());
    Serial.println("User-Agent: " + request->header("User-Agent"));
    Serial.println("Serving WiFi configuration page");
    request->send_P(200, "text/html", WIFI_CONFIG_HTML);
  });

  // Enhanced WiFi configuration handler with custom SSID support
  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("\n=== WIFI CONFIG POST RECEIVED ===");
    Serial.println("Client IP: " + request->client()->remoteIP().toString());

    // Debug: Print all received parameters
    int params = request->params();
    Serial.println("Parameters received: " + String(params));
    for (int i = 0; i < params; i++) {
      auto p = request->getParam(i);
      if (p->name() == "ap_password" || p->name() == "password") {
        // Don't log passwords in plain text
      } else {
        Serial.println("Param " + String(i) + ": " + p->name() + " = '" + p->value() + "'");
      }
    }

    String mode = "client";
    String ssid = "";
    String password = "";
    String ap_password = "";
    String hotspot_ssid = "";

    // Get parameters
    if (request->hasParam("mode", true)) {
      mode = request->getParam("mode", true)->value();
      mode.trim();
      Serial.println("Selected mode: " + mode);
    }

    if (request->hasParam("ap_password", true)) {
      ap_password = request->getParam("ap_password", true)->value();
      ap_password.trim();
      Serial.println("AP password provided, length: " + String(ap_password.length()));
    }

    if (request->hasParam("hotspot_ssid", true)) {
      hotspot_ssid = request->getParam("hotspot_ssid", true)->value();
      hotspot_ssid.trim();
      Serial.println("Custom hotspot SSID: '" + hotspot_ssid + "'");
    }

    if (request->hasParam("ssid", true)) {
      ssid = request->getParam("ssid", true)->value();
      ssid.trim();
      Serial.println("Ship WiFi SSID: '" + ssid + "'");
    }

    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
      password.trim();
      Serial.println("Ship WiFi password length: " + String(password.length()));
    }

    // CORRECTED VALIDATION LOGIC
    if (mode == "ap") {
      // AP mode: validate or use default AP password
      if (ap_password.length() == 0) {
        ap_password = "alternator123";
        Serial.println("AP mode: Using default password");
      } else if (ap_password.length() < 8) {
        Serial.println("ERROR: AP password too short (WPA2 requires 8+ characters)");
        request->send(400, "text/html",
                      "<html><body><h2>Error</h2><p>Password must be at least 8 characters for WPA2.</p>"
                      "<a href='/'>Go Back</a></body></html>");
        return;
      }
    } else {
      // Client mode: ensure valid AP password for fallback, validate client credentials
      if (ap_password.length() == 0) {
        ap_password = "alternator123";
        Serial.println("Client mode: Using default AP password for fallback");
      } else if (ap_password.length() < 8) {
        Serial.println("WARNING: Provided AP password too short, using default");
        ap_password = "alternator123";
      }

      // Validate client mode credentials
      if (ssid.length() == 0) {
        Serial.println("ERROR: Client mode requires ship WiFi SSID");
        request->send(400, "text/html",
                      "<html><body><h2>Error</h2><p>Ship WiFi credentials required for client mode.</p>"
                      "<a href='/'>Go Back</a></body></html>");
        return;
      }
    }

    // Save configuration
    Serial.println("=== SAVING CONFIGURATION ===");
    Serial.println("Updating in-memory values first...");
    esp32_ap_password = ap_password;  // Update memory FIRST
    Serial.println("New AP password in memory: length=" + String(esp32_ap_password.length()));

    writeFile(LittleFS, AP_PASSWORD_FILE, ap_password.c_str());
    Serial.println("AP password saved to file");

    // Save custom SSID if provided
    if (hotspot_ssid.length() > 0) {
      writeFile(LittleFS, AP_SSID_FILE, hotspot_ssid.c_str());
      esp32_ap_ssid = hotspot_ssid;
      Serial.println("Custom AP SSID saved: " + esp32_ap_ssid);
    } else {
      if (LittleFS.exists(AP_SSID_FILE)) {
        LittleFS.remove(AP_SSID_FILE);
      }
      esp32_ap_ssid = "ALTERNATOR_WIFI";
      Serial.println("Using default SSID");
    }

    if (mode == "ap") {
      // Permanent AP mode
      Serial.println("=== CONFIGURING PERMANENT AP MODE ===");
      writeFile(LittleFS, WIFI_MODE_FILE, "ap");
      permanentAPMode = 1;
      Serial.println("Permanent AP mode saved");

      // Send simple success response
      request->send(200, "text/plain", "Configuration saved! Please restart device.");

      Serial.println("=== RESTARTING FOR AP MODE ===");
      Serial.println("Waiting 5 seconds for filesystem sync...");
      delay(5000);  // Give filesystem time to sync
      ESP.restart();

    } else {
      // Client mode
      Serial.println("=== CONFIGURING CLIENT MODE ===");
      writeFile(LittleFS, "/ssid.txt", ssid.c_str());
      writeFile(LittleFS, "/pass.txt", password.c_str());
      writeFile(LittleFS, WIFI_MODE_FILE, "client");
      permanentAPMode = 0;

      // Send simple success response
      request->send(200, "text/plain", "Configuration saved! Please restart device.");

      Serial.println("=== RESTARTING FOR CLIENT MODE ===");
      ESP.restart();
    }
  });

  // Enhanced 404 handler (your existing one with debugging)
  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    Serial.println("=== SERVER REQUEST DEBUG ===");
    Serial.print("Request for: ");
    Serial.println(path);
    Serial.println("Client IP: " + request->client()->remoteIP().toString());
    Serial.println("Method: " + String(request->method() == HTTP_GET ? "GET" : "POST"));

    if (LittleFS.exists(path)) {
      String contentType = "text/html";
      if (path.endsWith(".css")) contentType = "text/css";
      else if (path.endsWith(".js")) contentType = "application/javascript";
      else if (path.endsWith(".json")) contentType = "application/json";
      else if (path.endsWith(".png")) contentType = "image/png";
      else if (path.endsWith(".jpg")) contentType = "image/jpeg";

      Serial.println("File found in LittleFS, serving with content-type: " + contentType);
      request->send(LittleFS, path, contentType);
    } else {
      Serial.print("File not found in LittleFS: ");
      Serial.println(path);
      Serial.println("Redirecting to captive portal: " + WiFi.softAPIP().toString());
      request->redirect("http://" + WiFi.softAPIP().toString());
    }
  });

  server.begin();
  Serial.println("=== CONFIG SERVER STARTED ===");
}

void setupServer() {

  //Factory Reset Logic
  server.on("/factoryReset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true)) {
      request->send(400, "text/plain", "Missing password");
      return;
    }
    String password = request->getParam("password", true)->value();
    if (!validatePassword(password.c_str())) {
      request->send(403, "text/plain", "FAIL");
      return;
    }
    queueConsoleMessage("FACTORY RESET: Restoring all defaults...");
    // Delete ALL settings files - let InitSystemSettings() recreate with defaults
    const char *settingsFiles[] = {
      "/AlternatorTemperatureLimitF.txt",
      "/ManualDuty.txt",
      "/FullChargeVoltage.txt",
      "/TargetAmps.txt",
      "/SwitchingFrequency.txt",
      "/TargetFloatVoltage.txt",
      "/interval.txt",
      "/FieldAdjustmentInterval.txt",
      "/ManualFieldToggle.txt",
      "/SwitchControlOverride.txt",
      "/IgnitionOverride.txt",
      "/ForceFloat.txt",
      "/OnOff.txt",
      "/HiLow.txt",
      "/AmpSrc.txt",
      "/InvertAltAmps.txt",
      "/InvertBattAmps.txt",
      "/LimpHome.txt",
      "/VeData.txt",
      "/NMEA0183Data.txt",
      "/NMEA2KData.txt",
      "/TargetAmpL.txt",
      "/CurrentThreshold.txt",
      "/PeukertExponent.txt",
      "/ChargeEfficiency.txt",
      "/ChargedVoltage.txt",
      "/TailCurrent.txt",
      "/ChargedDetectionTime.txt",
      "/IgnoreTemperature.txt",
      "/bmsLogic.txt",
      "/bmsLogicLevelOff.txt",
      "/AlarmActivate.txt",
      "/TempAlarm.txt",
      "/VoltageAlarmHigh.txt",
      "/VoltageAlarmLow.txt",
      "/CurrentAlarmHigh.txt",
      "/FourWay.txt",
      "/RPMScalingFactor.txt",
      "/FieldResistance.txt",
      "/AlternatorCOffset.txt",
      "/BatteryCOffset.txt",
      "/BatteryVoltageSource.txt",
      "/R_fixed.txt",
      "/Beta.txt",
      "/T0_C.txt",
      "/TempSource.txt",
      "/AlarmLatchEnabled.txt",
      "/bulkCompleteTime.txt",
      "/FLOAT_DURATION.txt",
      "/ResetTemp.txt",
      "/ResetVoltage.txt",
      "/ResetCurrent.txt",
      "/ResetEngineRunTime.txt",
      "/ResetAlternatorOnTime.txt",
      "/ResetEnergy.txt",
      "/MaximumAllowedBatteryAmps.txt",
      "/ManualSOCPoint.txt",
      "/AmpControlByRPM.txt",
      "/RPM1.txt",
      "/RPM2.txt",
      "/RPM3.txt",
      "/RPM4.txt",
      "/RPM5.txt",
      "/RPM6.txt",
      "/RPM7.txt",
      "/Amps1.txt",
      "/Amps2.txt",
      "/Amps3.txt",
      "/Amps4.txt",
      "/Amps5.txt",
      "/Amps6.txt",
      "/Amps7.txt",
      "/ShuntResistanceMicroOhm.txt",
      "/maxPoints.txt",
      "/Ymin1.txt",
      "/Ymax1.txt",
      "/Ymin2.txt",
      "/Ymax2.txt",
      "/Ymin3.txt",
      "/Ymax3.txt",
      "/Ymin4.txt",
      "/Ymax4.txt",
      "/MaxDuty.txt",
      "/MinDuty.txt",
      "/MaxTemperatureThermistor.txt",
      "/AutoShuntGainCorrection.txt",
      "/AutoAltCurrentZero.txt",
      "/WindingTempOffset.txt",
      "/PulleyRatio.txt",
      "/BatteryCapacity_Ah.txt",
      "/ManualLifePercentage.txt",
      "/timeAxisModeChanging",
      // Persistent Variables (InitPersistentVariables)
      "/IBVMax.txt",
      "/MeasuredAmpsMax.txt",
      "/RPMMax.txt",
      "/SOC_percent.txt",
      "/EngineRunTime.txt",
      "/EngineCycles.txt",
      "/AlternatorOnTime.txt",
      "/AlternatorFuelUsed.txt",
      "/ChargedEnergy.txt",
      "/DischargedEnergy.txt",
      "/AlternatorChargedEnergy.txt",
      "/MaxAlternatorTemperatureF.txt",
      "/DynamicShuntGainFactor.txt",
      "/DynamicAltCurrentZero.txt",
      "/lastGainCorrectionTime.txt",
      "/lastAutoZeroTime.txt",
      "/lastAutoZeroTemp.txt",
      "/CumulativeInsulationDamage.txt",
      "/CumulativeGreaseDamage.txt",
      "/CumulativeBrushDamage.txt",
      "/LastSessionDuration.txt",
      "/LastSessionMaxLoopTime.txt",
      "/lastSessionMinHeap.txt",
      "/wifiReconnectsTotal.txt",
      "/LastResetReason.txt",
      "/ancientResetReason.txt",
      // Other files created in SaveAllData and various functions
      "/BatteryCapacity.txt",
      "/FuelEfficiency.txt",
      "/AltEnergy.txt",
      "/FuelUsed.txt",
      // WiFi and System files
      "/ssid.txt",
      "/pass.txt",
      "/password.txt",
      "/wifimode.txt",
      "/apssid.txt",
      "/appass.txt",
      "/scheduledRestart.txt"
    };
    // Delete all settings files
    for (int i = 0; i < sizeof(settingsFiles) / sizeof(settingsFiles[0]); i++) {
      if (LittleFS.exists(settingsFiles[i])) {
        LittleFS.remove(settingsFiles[i]);
      }
    }
    // Clear NVS data
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
      nvs_erase_all(nvs_handle);
      nvs_commit(nvs_handle);
      nvs_close(nvs_handle);
      queueConsoleMessage("FACTORY RESET: NVS data cleared");
    }
    // Reinitialize with defaults
    InitSystemSettings();
    queueConsoleMessage("FACTORY RESET: Complete! All settings restored to defaults.");
    request->send(200, "text/plain", "OK");
  });

  /**
 * AI_SUMMARY: Master settings form handler - processes 80+ different setting changes from web interface with password validation and immediate persistence
 * AI_PURPOSE: Single mega-handler for ALL setting changes from web forms - validates password, identifies which setting changed, converts values, saves to LittleFS, updates global variables
 * AI_INPUTS: HTTP GET params (password + one setting), setting values from web forms, currentAdminPassword global
 * AI_OUTPUTS: HTTP 200 response with confirmation, LittleFS file writes, updated global variables, console messages for critical changes
 * AI_DEPENDENCIES: LittleFS filesystem, writeFile() function, 80+ global setting variables, currentAdminPassword validation, hardcoded parameter names matching HTML form names exactly
 * AI_RISKS: MASSIVE if/else chain (80+ parameters) - easy to miss edge cases. Parameter names must match HTML exactly or silent failures. No input validation beyond basic type conversion. Password bypass for safety (OnOff can be turned Off only (not on!) without password). Inconsistent scaling on input (some ×100, some direct). File write failures not always handled. Missing parameters cause silent no-ops.
 * AI_OPTIMIZE: When adding new settings: 1)Add HTML form with exact parameter name, 2)Add if/else block here, 3)Add LittleFS file handling, 4)Add to InitSystemSettings(). Scaling conversion needed for float settings (ChargingVoltageTarget, etc.). Consider parameter validation and error responses. The password field auto-population is clever but fragile. RPM/Amps table uses separate non-else-if blocks due to multiple variables updates coming from one UI button press.
 */


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on(
    "/get", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
        request->send(403, "text/plain", "Forbidden");
        return;
      }
      bool foundParameter = false;
      String inputMessage;
      if (request->hasParam("AlternatorTemperatureLimitF")) {
        foundParameter = true;
        inputMessage = request->getParam("AlternatorTemperatureLimitF")->value();
        writeFile(LittleFS, "/AlternatorTemperatureLimitF.txt", inputMessage.c_str());
        AlternatorTemperatureLimitF = inputMessage.toInt();
      } else if (request->hasParam("ManualDuty")) {
        foundParameter = true;
        inputMessage = request->getParam("ManualDuty")->value();
        writeFile(LittleFS, "/ManualDuty.txt", inputMessage.c_str());
        ManualDutyTarget = inputMessage.toInt();
      } else if (request->hasParam("FullChargeVoltage")) {
        foundParameter = true;
        inputMessage = request->getParam("FullChargeVoltage")->value();
        writeFile(LittleFS, "/FullChargeVoltage.txt", inputMessage.c_str());
        ChargingVoltageTarget = inputMessage.toFloat();
      } else if (request->hasParam("TargetAmps")) {
        foundParameter = true;
        inputMessage = request->getParam("TargetAmps")->value();
        writeFile(LittleFS, "/TargetAmps.txt", inputMessage.c_str());
        TargetAmps = inputMessage.toInt();
      } else if (request->hasParam("SwitchingFrequency")) {
        foundParameter = true;
        inputMessage = request->getParam("SwitchingFrequency")->value();
        int requestedFreq = inputMessage.toInt();
        // Limit to 1100Hz to prevent ESP32 PWM shutdown
        //There are solutions for this later if necessary, use lower-level ESP-IDF functions, not worth it right now
        if (requestedFreq > 1100) {
          requestedFreq = 1100;
          queueConsoleMessage("Frequency limited to 1100Hz maximum");
        }
        writeFile(LittleFS, "/SwitchingFrequency.txt", String(requestedFreq).c_str());
        fffr = requestedFreq;
        ledcDetach(pwmPin);
        delay(50);
        ledcAttach(pwmPin, fffr, pwmResolution);
        queueConsoleMessage("Switching frequency set to " + String(fffr) + "Hz");
      } else if (request->hasParam("TargetFloatVoltage")) {
        foundParameter = true;
        inputMessage = request->getParam("TargetFloatVoltage")->value();
        writeFile(LittleFS, "/TargetFloatVoltage.txt", inputMessage.c_str());
        TargetFloatVoltage = inputMessage.toFloat();
      } else if (request->hasParam("interval")) {
        foundParameter = true;
        inputMessage = request->getParam("interval")->value();
        writeFile(LittleFS, "/interval.txt", inputMessage.c_str());
        interval = inputMessage.toFloat();
        dutyStep = interval;  // Apply new step size immediately
      } else if (request->hasParam("FieldAdjustmentInterval")) {
        foundParameter = true;
        inputMessage = request->getParam("FieldAdjustmentInterval")->value();
        writeFile(LittleFS, "/FieldAdjustmentInterval.txt", inputMessage.c_str());
        FieldAdjustmentInterval = inputMessage.toFloat();
      } else if (request->hasParam("ManualFieldToggle")) {
        foundParameter = true;
        inputMessage = request->getParam("ManualFieldToggle")->value();
        writeFile(LittleFS, "/ManualFieldToggle.txt", inputMessage.c_str());
        ManualFieldToggle = inputMessage.toInt();
      } else if (request->hasParam("SwitchControlOverride")) {
        foundParameter = true;
        inputMessage = request->getParam("SwitchControlOverride")->value();
        writeFile(LittleFS, "/SwitchControlOverride.txt", inputMessage.c_str());
        SwitchControlOverride = inputMessage.toInt();
      } else if (request->hasParam("ForceFloat")) {
        foundParameter = true;
        inputMessage = request->getParam("ForceFloat")->value();
        writeFile(LittleFS, "/ForceFloat.txt", inputMessage.c_str());
        ForceFloat = inputMessage.toInt();
        queueConsoleMessage("ForceFloat mode " + String(ForceFloat ? "enabled" : "disabled"));
      } else if (request->hasParam("OnOff")) {
        foundParameter = true;
        inputMessage = request->getParam("OnOff")->value();
        writeFile(LittleFS, "/OnOff.txt", inputMessage.c_str());
        OnOff = inputMessage.toInt();
      } else if (request->hasParam("HiLow")) {
        foundParameter = true;
        inputMessage = request->getParam("HiLow")->value();
        writeFile(LittleFS, "/HiLow.txt", inputMessage.c_str());
        HiLow = inputMessage.toInt();
      } else if (request->hasParam("InvertAltAmps")) {
        foundParameter = true;
        inputMessage = request->getParam("InvertAltAmps")->value();
        writeFile(LittleFS, "/InvertAltAmps.txt", inputMessage.c_str());
        InvertAltAmps = inputMessage.toInt();
      } else if (request->hasParam("InvertBattAmps")) {
        foundParameter = true;
        inputMessage = request->getParam("InvertBattAmps")->value();
        writeFile(LittleFS, "/InvertBattAmps.txt", inputMessage.c_str());
        InvertBattAmps = inputMessage.toInt();
      } else if (request->hasParam("MaxDuty")) {
        foundParameter = true;
        inputMessage = request->getParam("MaxDuty")->value();
        writeFile(LittleFS, "/MaxDuty.txt", inputMessage.c_str());
        MaxDuty = inputMessage.toInt();
      } else if (request->hasParam("MinDuty")) {
        foundParameter = true;
        inputMessage = request->getParam("MinDuty")->value();
        writeFile(LittleFS, "/MinDuty.txt", inputMessage.c_str());
        MinDuty = inputMessage.toInt();
      } else if (request->hasParam("LimpHome")) {
        foundParameter = true;
        inputMessage = request->getParam("LimpHome")->value();
        writeFile(LittleFS, "/LimpHome.txt", inputMessage.c_str());
        LimpHome = inputMessage.toInt();
      } else if (request->hasParam("VeData")) {
        foundParameter = true;
        inputMessage = request->getParam("VeData")->value();
        writeFile(LittleFS, "/VeData.txt", inputMessage.c_str());
        VeData = inputMessage.toInt();
      } else if (request->hasParam("NMEA0183Data")) {
        foundParameter = true;
        inputMessage = request->getParam("NMEA0183Data")->value();
        writeFile(LittleFS, "/NMEA0183Data.txt", inputMessage.c_str());
        NMEA0183Data = inputMessage.toInt();
      } else if (request->hasParam("NMEA2KData")) {
        foundParameter = true;
        inputMessage = request->getParam("NMEA2KData")->value();
        writeFile(LittleFS, "/NMEA2KData.txt", inputMessage.c_str());
        NMEA2KData = inputMessage.toInt();
      } else if (request->hasParam("TargetAmpL")) {
        foundParameter = true;
        inputMessage = request->getParam("TargetAmpL")->value();
        writeFile(LittleFS, "/TargetAmpL.txt", inputMessage.c_str());
        TargetAmpL = inputMessage.toInt();
      } else if (request->hasParam("CurrentThreshold")) {
        foundParameter = true;
        inputMessage = request->getParam("CurrentThreshold")->value();
        writeFile(LittleFS, "/CurrentThreshold.txt", inputMessage.c_str());
        CurrentThreshold = inputMessage.toFloat();
      } else if (request->hasParam("PeukertExponent")) {
        foundParameter = true;
        inputMessage = request->getParam("PeukertExponent")->value();
        writeFile(LittleFS, "/PeukertExponent.txt", inputMessage.c_str());
        PeukertExponent_scaled = (int)(inputMessage.toFloat() * 100);
      } else if (request->hasParam("ChargeEfficiency")) {
        foundParameter = true;
        inputMessage = request->getParam("ChargeEfficiency")->value();
        writeFile(LittleFS, "/ChargeEfficiency.txt", inputMessage.c_str());
        ChargeEfficiency_scaled = inputMessage.toInt();
      } else if (request->hasParam("ChargedVoltage")) {
        foundParameter = true;
        inputMessage = request->getParam("ChargedVoltage")->value();
        writeFile(LittleFS, "/ChargedVoltage.txt", inputMessage.c_str());
        ChargedVoltage_Scaled = (int)(inputMessage.toFloat() * 100);
      } else if (request->hasParam("TailCurrent")) {
        foundParameter = true;
        inputMessage = request->getParam("TailCurrent")->value();
        writeFile(LittleFS, "/TailCurrent.txt", inputMessage.c_str());
        TailCurrent = inputMessage.toFloat();
      } else if (request->hasParam("ChargedDetectionTime")) {
        foundParameter = true;
        inputMessage = request->getParam("ChargedDetectionTime")->value();
        writeFile(LittleFS, "/ChargedDetectionTime.txt", inputMessage.c_str());
        ChargedDetectionTime = inputMessage.toInt();
      } else if (request->hasParam("IgnoreTemperature")) {
        foundParameter = true;
        inputMessage = request->getParam("IgnoreTemperature")->value();
        writeFile(LittleFS, "/IgnoreTemperature.txt", inputMessage.c_str());
        IgnoreTemperature = inputMessage.toInt();
      } else if (request->hasParam("bmsLogic")) {
        foundParameter = true;
        inputMessage = request->getParam("bmsLogic")->value();
        writeFile(LittleFS, "/bmsLogic.txt", inputMessage.c_str());
        bmsLogic = inputMessage.toInt();
      } else if (request->hasParam("bmsLogicLevelOff")) {
        foundParameter = true;
        inputMessage = request->getParam("bmsLogicLevelOff")->value();
        writeFile(LittleFS, "/bmsLogicLevelOff.txt", inputMessage.c_str());
        bmsLogicLevelOff = inputMessage.toInt();
      } else if (request->hasParam("AlarmActivate")) {
        foundParameter = true;
        inputMessage = request->getParam("AlarmActivate")->value();
        writeFile(LittleFS, "/AlarmActivate.txt", inputMessage.c_str());
        AlarmActivate = inputMessage.toInt();
      } else if (request->hasParam("TempAlarm")) {
        foundParameter = true;
        inputMessage = request->getParam("TempAlarm")->value();
        writeFile(LittleFS, "/TempAlarm.txt", inputMessage.c_str());
        TempAlarm = inputMessage.toInt();
      } else if (request->hasParam("VoltageAlarmHigh")) {
        foundParameter = true;
        inputMessage = request->getParam("VoltageAlarmHigh")->value();
        writeFile(LittleFS, "/VoltageAlarmHigh.txt", inputMessage.c_str());
        VoltageAlarmHigh = inputMessage.toInt();
      } else if (request->hasParam("VoltageAlarmLow")) {
        foundParameter = true;
        inputMessage = request->getParam("VoltageAlarmLow")->value();
        writeFile(LittleFS, "/VoltageAlarmLow.txt", inputMessage.c_str());
        VoltageAlarmLow = inputMessage.toInt();
      } else if (request->hasParam("CurrentAlarmHigh")) {
        foundParameter = true;
        inputMessage = request->getParam("CurrentAlarmHigh")->value();
        writeFile(LittleFS, "/CurrentAlarmHigh.txt", inputMessage.c_str());
        CurrentAlarmHigh = inputMessage.toInt();
      } else if (request->hasParam("FourWay")) {
        foundParameter = true;
        inputMessage = request->getParam("FourWay")->value();
        writeFile(LittleFS, "/FourWay.txt", inputMessage.c_str());
        FourWay = inputMessage.toInt();
      } else if (request->hasParam("RPMScalingFactor")) {
        foundParameter = true;
        inputMessage = request->getParam("RPMScalingFactor")->value();
        writeFile(LittleFS, "/RPMScalingFactor.txt", inputMessage.c_str());
        RPMScalingFactor = inputMessage.toInt();
      } else if (request->hasParam("FieldResistance")) {
        foundParameter = true;
        inputMessage = request->getParam("FieldResistance")->value();
        writeFile(LittleFS, "/FieldResistance.txt", inputMessage.c_str());
        FieldResistance = inputMessage.toInt();
      } else if (request->hasParam("AlternatorCOffset")) {
        foundParameter = true;
        inputMessage = request->getParam("AlternatorCOffset")->value();
        writeFile(LittleFS, "/AlternatorCOffset.txt", inputMessage.c_str());
        AlternatorCOffset = inputMessage.toFloat();
      } else if (request->hasParam("BatteryCOffset")) {
        foundParameter = true;
        inputMessage = request->getParam("BatteryCOffset")->value();
        writeFile(LittleFS, "/BatteryCOffset.txt", inputMessage.c_str());
        BatteryCOffset = inputMessage.toFloat();
      } else if (request->hasParam("AmpSrc")) {
        foundParameter = true;
        inputMessage = request->getParam("AmpSrc")->value();
        writeFile(LittleFS, "/AmpSrc.txt", inputMessage.c_str());
        AmpSrc = inputMessage.toInt();
        queueConsoleMessage("AmpSrc changed to: " + String(AmpSrc));  // debug line
      } else if (request->hasParam("BatteryVoltageSource")) {
        foundParameter = true;
        inputMessage = request->getParam("BatteryVoltageSource")->value();
        writeFile(LittleFS, "/BatteryVoltageSource.txt", inputMessage.c_str());
        BatteryVoltageSource = inputMessage.toInt();
        queueConsoleMessage("Battery voltage source changed to: " + String(BatteryVoltageSource));  // Debug line
      } else if (request->hasParam("R_fixed")) {
        foundParameter = true;
        inputMessage = request->getParam("R_fixed")->value();
        writeFile(LittleFS, "/R_fixed.txt", inputMessage.c_str());
        R_fixed = inputMessage.toFloat();
      } else if (request->hasParam("Beta")) {
        foundParameter = true;
        inputMessage = request->getParam("Beta")->value();
        writeFile(LittleFS, "/Beta.txt", inputMessage.c_str());
        Beta = inputMessage.toFloat();
      } else if (request->hasParam("T0_C")) {
        foundParameter = true;
        inputMessage = request->getParam("T0_C")->value();
        writeFile(LittleFS, "/T0_C.txt", inputMessage.c_str());
        T0_C = inputMessage.toFloat();
      } else if (request->hasParam("TempSource")) {
        foundParameter = true;
        inputMessage = request->getParam("TempSource")->value();
        writeFile(LittleFS, "/TempSource.txt", inputMessage.c_str());
        TempSource = inputMessage.toInt();
      } else if (request->hasParam("IgnitionOverride")) {
        foundParameter = true;
        inputMessage = request->getParam("IgnitionOverride")->value();
        writeFile(LittleFS, "/IgnitionOverride.txt", inputMessage.c_str());
        IgnitionOverride = inputMessage.toInt();
      }
      else if (request->hasParam("AlarmLatchEnabled")) {
        foundParameter = true;
        inputMessage = request->getParam("AlarmLatchEnabled")->value();
        writeFile(LittleFS, "/AlarmLatchEnabled.txt", inputMessage.c_str());
        AlarmLatchEnabled = inputMessage.toInt();
      }
      else if (request->hasParam("AlarmTest")) {
        foundParameter = true;
        AlarmTest = 1;  // Set the flag - don't save to file as it's momentary
        queueConsoleMessage("ALARM TEST: Initiated from web interface");
        inputMessage = "1";  // Return confirmation
      }
      else if (request->hasParam("ResetAlarmLatch")) {
        foundParameter = true;
        ResetAlarmLatch = 1;  // Set the flag - don't save to file as it's momentary
        queueConsoleMessage("ALARM LATCH: Reset requested from web interface NO FUNCTION!");
        inputMessage = "1";  // Return confirmation
      }

      // else if (request->hasParam("ResetAlarmLatch")) { // this whole thing is hopefullly obsolete
      //   inputMessage = request->getParam("ResetAlarmLatch")->value();
      //  ResetAlarmLatch = inputMessage.toInt();  // Don't save to file - momentary action
      //   resetAlarmLatch();                       // Call the reset function
      //  }

      else if (request->hasParam("bulkCompleteTime")) {
        foundParameter = true;
        inputMessage = request->getParam("bulkCompleteTime")->value();
        writeFile(LittleFS, "/bulkCompleteTime.txt", inputMessage.c_str());
        bulkCompleteTime = inputMessage.toInt();
      }  // When sending to ESP32
      else if (request->hasParam("FLOAT_DURATION")) {
        foundParameter = true;
        inputMessage = request->getParam("FLOAT_DURATION")->value();
        int hours = inputMessage.toInt();
        int seconds = hours * 3600;  // Convert to seconds
        writeFile(LittleFS, "/FLOAT_DURATION.txt", String(seconds).c_str());
        FLOAT_DURATION = seconds;
      }

      // Reset button Code
      else if (request->hasParam("ResetThermTemp")) {
        foundParameter = true;
        MaxTemperatureThermistor = 0;
        writeFile(LittleFS, "/MaxTemperatureThermistor.txt", "0");
        queueConsoleMessage("Max Thermistor Temp: Reset requested from web interface");
      } else if (request->hasParam("ResetTemp")) {
        foundParameter = true;
        MaxAlternatorTemperatureF = 0;                               // reset the variable on ESP32 mem
        writeFile(LittleFS, "/MaxAlternatorTemperatureF.txt", "0");  // not pointless
        queueConsoleMessage("Max Alterantor Temp: Reset requested from web interface");
      } else if (request->hasParam("ResetVoltage")) {
        foundParameter = true;
        IBVMax = 0;                               // reset the variable on ESP32 mem
        writeFile(LittleFS, "/IBVMax.txt", "0");  // not pointless
        queueConsoleMessage("Max Voltage: Reset requested from web interface");
      } else if (request->hasParam("ResetCurrent")) {
        foundParameter = true;
        MeasuredAmpsMax = 0;                               // reset the variable on ESP32 mem
        writeFile(LittleFS, "/MeasuredAmpsMax.txt", "0");  // not pointless
        queueConsoleMessage("Max Battery Current: Reset requested from web interface");
      } else if (request->hasParam("ResetEngineRunTime")) {
        foundParameter = true;
        EngineRunTime = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Engine Run Time: Reset requested from web interface");
      } else if (request->hasParam("ResetAlternatorOnTime")) {
        foundParameter = true;
        AlternatorOnTime = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Alternator On Time: Reset requested from web interface");
      } else if (request->hasParam("ResetEnergy")) {
        foundParameter = true;
        ChargedEnergy = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Battery Charged Energy: Reset requested from web interface");
      } else if (request->hasParam("ResetDischargedEnergy")) {
        foundParameter = true;
        DischargedEnergy = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Battery Discharged Energy: Reset requested from web interface");
      } else if (request->hasParam("ResetFuelUsed")) {
        foundParameter = true;
        AlternatorFuelUsed = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Fuel Used: Reset requested from web interface");
      } else if (request->hasParam("ResetAlternatorChargedEnergy")) {
        foundParameter = true;
        AlternatorChargedEnergy = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Alternator Charged Energy: Reset requested from web interface");
      } else if (request->hasParam("ResetEngineCycles")) {
        foundParameter = true;
        EngineCycles = 0;  // reset the variable on ESP32 mem
        queueConsoleMessage("Engine Cycles: Reset requested from web interface");
      } else if (request->hasParam("ResetRPMMax")) {
        foundParameter = true;
        RPMMax = 0;                               // reset the variable on ESP32 mem
        writeFile(LittleFS, "/RPMMax.txt", "0");  // not pointless
        queueConsoleMessage("Max Engine Speed: Reset requested from web interface");
      } else if (request->hasParam("MaximumAllowedBatteryAmps")) {
        foundParameter = true;
        inputMessage = request->getParam("MaximumAllowedBatteryAmps")->value();
        writeFile(LittleFS, "/MaximumAllowedBatteryAmps.txt", inputMessage.c_str());
        MaximumAllowedBatteryAmps = inputMessage.toInt();
      } else if (request->hasParam("ManualSOCPoint")) {
        foundParameter = true;
        inputMessage = request->getParam("ManualSOCPoint")->value();
        writeFile(LittleFS, "/ManualSOCPoint.txt", inputMessage.c_str());
        ManualSOCPoint = inputMessage.toInt();
        SOC_percent = ManualSOCPoint * 100;                              // Convert to scaled format (SOC_percent is stored × 100)
        CoulombCount_Ah_scaled = (ManualSOCPoint * BatteryCapacity_Ah);  // Update coulomb count to match
        queueConsoleMessage("SoC manually set to: " + String(ManualSOCPoint) + "%");
      } else if (request->hasParam("BatteryCapacity_Ah")) {
        foundParameter = true;
        inputMessage = request->getParam("BatteryCapacity_Ah")->value();
        writeFile(LittleFS, "/BatteryCapacity_Ah.txt", inputMessage.c_str());
        BatteryCapacity_Ah = inputMessage.toInt();
        PeukertRatedCurrent_A = BatteryCapacity_Ah / 20.0f;  // Recalculate when capacity changes
        queueConsoleMessage("Battery capacity set to: " + inputMessage + " Ah");
      } else if (request->hasParam("AmpControlByRPM")) {
        foundParameter = true;
        inputMessage = request->getParam("AmpControlByRPM")->value();
        writeFile(LittleFS, "/AmpControlByRPM.txt", inputMessage.c_str());
        AmpControlByRPM = inputMessage.toInt();
      } else if (request->hasParam("ShuntResistanceMicroOhm")) {
        foundParameter = true;
        inputMessage = request->getParam("ShuntResistanceMicroOhm")->value();
        writeFile(LittleFS, "/ShuntResistanceMicroOhm.txt", inputMessage.c_str());
        ShuntResistanceMicroOhm = inputMessage.toInt();
      } else if (request->hasParam("maxPoints")) {
        foundParameter = true;
        inputMessage = request->getParam("maxPoints")->value();
        writeFile(LittleFS, "/maxPoints.txt", inputMessage.c_str());
        maxPoints = inputMessage.toInt();
      } else if (request->hasParam("Ymin1")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymin1")->value();
        writeFile(LittleFS, "/Ymin1.txt", inputMessage.c_str());
        Ymin1 = inputMessage.toInt();
      } else if (request->hasParam("Ymax1")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymax1")->value();
        writeFile(LittleFS, "/Ymax1.txt", inputMessage.c_str());
        Ymax1 = inputMessage.toInt();
      } else if (request->hasParam("Ymin2")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymin2")->value();
        writeFile(LittleFS, "/Ymin2.txt", inputMessage.c_str());
        Ymin2 = inputMessage.toInt();
      } else if (request->hasParam("Ymax2")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymax2")->value();
        writeFile(LittleFS, "/Ymax2.txt", inputMessage.c_str());
        Ymax2 = inputMessage.toInt();
      } else if (request->hasParam("Ymin3")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymin3")->value();
        writeFile(LittleFS, "/Ymin3.txt", inputMessage.c_str());
        Ymin3 = inputMessage.toInt();
      } else if (request->hasParam("Ymax3")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymax3")->value();
        writeFile(LittleFS, "/Ymax3.txt", inputMessage.c_str());
        Ymax3 = inputMessage.toInt();
      } else if (request->hasParam("Ymin4")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymin4")->value();
        writeFile(LittleFS, "/Ymin4.txt", inputMessage.c_str());
        Ymin4 = inputMessage.toInt();
      } else if (request->hasParam("Ymax4")) {
        foundParameter = true;
        inputMessage = request->getParam("Ymax4")->value();
        writeFile(LittleFS, "/Ymax4.txt", inputMessage.c_str());
        Ymax4 = inputMessage.toInt();
      }

      // Dynamic gain corrections
      else if (request->hasParam("AutoShuntGainCorrection")) {  // boolean flag!
        foundParameter = true;
        inputMessage = request->getParam("AutoShuntGainCorrection")->value();
        writeFile(LittleFS, "/AutoShuntGainCorrection.txt", inputMessage.c_str());
        AutoShuntGainCorrection = inputMessage.toInt();
      } else if (request->hasParam("AutoAltCurrentZero")) {  // BOOLEAN
        foundParameter = true;
        inputMessage = request->getParam("AutoAltCurrentZero")->value();
        writeFile(LittleFS, "/AutoAltCurrentZero.txt", inputMessage.c_str());
        AutoAltCurrentZero = inputMessage.toInt();
      } else if (request->hasParam("ResetDynamicShuntGain")) {
        foundParameter = true;
        ResetDynamicShuntGain = 1;  // Set the flag - don't save to file as it's momentary
        queueConsoleMessage("SOC Gain: Reset requested from web interface");
        inputMessage = "1";  // Return confirmation
      } else if (request->hasParam("ResetDynamicAltZero")) {
        foundParameter = true;
        ResetDynamicAltZero = 1;  // Set the flag - don't save to file as it's momentary
        queueConsoleMessage("Alt Zero: Reset requested from web interface");
        inputMessage = "1";  // Return confirmation
      } else if (request->hasParam("WindingTempOffset")) {
        foundParameter = true;
        inputMessage = request->getParam("WindingTempOffset")->value();
        writeFile(LittleFS, "/WindingTempOffset.txt", inputMessage.c_str());
        WindingTempOffset = inputMessage.toFloat();
      } else if (request->hasParam("PulleyRatio")) {
        foundParameter = true;
        inputMessage = request->getParam("PulleyRatio")->value();
        writeFile(LittleFS, "/PulleyRatio.txt", inputMessage.c_str());
        PulleyRatio = inputMessage.toFloat();
      }

      else if (request->hasParam("ManualLifePercentage")) {
        foundParameter = true;
        inputMessage = request->getParam("ManualLifePercentage")->value();
        writeFile(LittleFS, "/ManualLifePercentage.txt", inputMessage.c_str());
        ManualLifePercentage = inputMessage.toInt();
        float life_fraction = ManualLifePercentage / 100.00;
        CumulativeInsulationDamage = 1.0 - life_fraction;
        CumulativeGreaseDamage = 1.0 - life_fraction;
        CumulativeBrushDamage = 1.0 - life_fraction;
        // Save to persistent storage
        writeFile(LittleFS, "/CumulativeInsulationDamage.txt", String(CumulativeInsulationDamage, 6).c_str());
        writeFile(LittleFS, "/CumulativeGreaseDamage.txt", String(CumulativeGreaseDamage, 6).c_str());
        writeFile(LittleFS, "/CumulativeBrushDamage.txt", String(CumulativeBrushDamage, 6).c_str());
        queueConsoleMessage("Alternator life manually set to " + String(ManualLifePercentage) + "%");
      }

      else if (request->hasParam("webgaugesinterval")) {
        foundParameter = true;
        inputMessage = request->getParam("webgaugesinterval")->value();
        int newInterval = inputMessage.toInt();
        newInterval = constrain(newInterval, 1, 10000000);
        writeFile(LittleFS, "/webgaugesinterval.txt", String(newInterval).c_str());
        webgaugesinterval = newInterval;
        queueConsoleMessage("Update interval set to: " + String(newInterval) + "ms");
      }

      else if (request->hasParam("BatteryCurrentSource")) {
        foundParameter = true;
        inputMessage = request->getParam("BatteryCurrentSource")->value();
        writeFile(LittleFS, "/BatteryCurrentSource.txt", inputMessage.c_str());
        BatteryCurrentSource = inputMessage.toInt();
        queueConsoleMessage("Battery current source changed to: " + String(BatteryCurrentSource));  // Debug line
      }

      else if (request->hasParam("totalPowerCycles")) {
        foundParameter = true;
        inputMessage = request->getParam("totalPowerCycles")->value();
        writeFile(LittleFS, "/totalPowerCycles.txt", inputMessage.c_str());
        totalPowerCycles = inputMessage.toInt();
      }

      else if (request->hasParam("lastWifiSessionDuration")) {
        foundParameter = true;
        inputMessage = request->getParam("lastWifiSessionDuration")->value();
        writeFile(LittleFS, "/lastWifiSessionDuration.txt", inputMessage.c_str());
        lastWifiSessionDuration = inputMessage.toInt();
      }

      else if (request->hasParam("timeAxisModeChanging")) {
        foundParameter = true;
        inputMessage = request->getParam("timeAxisModeChanging")->value();
        writeFile(LittleFS, "/timeAxisModeChanging.txt", inputMessage.c_str());
        timeAxisModeChanging = inputMessage.toInt();
        queueConsoleMessage("Time axis mode changed to: " + String(timeAxisModeChanging ? "UNIX timestamps" : "relative time"));
      }

      else if (request->hasParam("plotTimeWindow")) {
        foundParameter = true;
        inputMessage = request->getParam("plotTimeWindow")->value();
        writeFile(LittleFS, "/plotTimeWindow.txt", inputMessage.c_str());
        plotTimeWindow = inputMessage.toInt();
      }

      // Handle RPM/AMPS table - use separate if statements, not else if, becuase we are sending more than 1 value at a time, unlike all the others!
      if (request->hasParam("RPM1")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM1")->value();
        writeFile(LittleFS, "/RPM1.txt", inputMessage.c_str());
        RPM1 = inputMessage.toInt();
      }
      if (request->hasParam("RPM2")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM2")->value();
        writeFile(LittleFS, "/RPM2.txt", inputMessage.c_str());
        RPM2 = inputMessage.toInt();
      }
      if (request->hasParam("RPM3")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM3")->value();
        writeFile(LittleFS, "/RPM3.txt", inputMessage.c_str());
        RPM3 = inputMessage.toInt();
      }
      if (request->hasParam("RPM4")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM4")->value();
        writeFile(LittleFS, "/RPM4.txt", inputMessage.c_str());
        RPM4 = inputMessage.toInt();
      }
      if (request->hasParam("RPM5")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM5")->value();
        writeFile(LittleFS, "/RPM5.txt", inputMessage.c_str());
        RPM5 = inputMessage.toInt();
      }
      if (request->hasParam("RPM6")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM6")->value();
        writeFile(LittleFS, "/RPM6.txt", inputMessage.c_str());
        RPM6 = inputMessage.toInt();
      }
      if (request->hasParam("RPM7")) {
        foundParameter = true;
        inputMessage = request->getParam("RPM7")->value();
        writeFile(LittleFS, "/RPM7.txt", inputMessage.c_str());
        RPM7 = inputMessage.toInt();
      }
      if (request->hasParam("Amps1")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps1")->value();
        writeFile(LittleFS, "/Amps1.txt", inputMessage.c_str());
        Amps1 = inputMessage.toInt();
      }
      if (request->hasParam("Amps2")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps2")->value();
        writeFile(LittleFS, "/Amps2.txt", inputMessage.c_str());
        Amps2 = inputMessage.toInt();
      }
      if (request->hasParam("Amps3")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps3")->value();
        writeFile(LittleFS, "/Amps3.txt", inputMessage.c_str());
        Amps3 = inputMessage.toInt();
      }
      if (request->hasParam("Amps4")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps4")->value();
        writeFile(LittleFS, "/Amps4.txt", inputMessage.c_str());
        Amps4 = inputMessage.toInt();
      }
      if (request->hasParam("Amps5")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps5")->value();
        writeFile(LittleFS, "/Amps5.txt", inputMessage.c_str());
        Amps5 = inputMessage.toInt();
      }
      if (request->hasParam("Amps6")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps6")->value();
        writeFile(LittleFS, "/Amps6.txt", inputMessage.c_str());
        Amps6 = inputMessage.toInt();
      }
      if (request->hasParam("Amps7")) {
        foundParameter = true;
        inputMessage = request->getParam("Amps7")->value();
        writeFile(LittleFS, "/Amps7.txt", inputMessage.c_str());
        Amps7 = inputMessage.toInt();
      }
      if (!foundParameter) {
        inputMessage = "No message sent, the request_hasParam found no match";
      }
      request->send(200, "text/plain", inputMessage);
    });

  server.on("/setPassword", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true) || !request->hasParam("newpassword", true)) {
      request->send(400, "text/plain", "Missing fields");
      return;
    }

    String password = request->getParam("password", true)->value();
    String newPassword = request->getParam("newpassword", true)->value();

    password.trim();
    newPassword.trim();

    if (newPassword.length() == 0) {
      request->send(400, "text/plain", "Empty new password");
      return;
    }

    // Validate the existing admin password first
    if (!validatePassword(password.c_str())) {
      request->send(403, "text/plain", "FAIL");  // Wrong password
      return;
    }

    // Save the plaintext password for recovery
    File plainFile = LittleFS.open("/password.txt", "w");
    if (plainFile) {
      plainFile.println(newPassword);
      plainFile.close();
    }

    // Create and save the hash
    char hash[65] = { 0 };
    sha256(newPassword.c_str(), hash);

    File file = LittleFS.open("/password.hash", "w");
    if (!file) {
      request->send(500, "text/plain", "Failed to open password file");
      return;
    }

    file.println(hash);
    file.close();

    // Update RAM copy
    strncpy(requiredPassword, newPassword.c_str(), sizeof(requiredPassword) - 1);
    strncpy(storedPasswordHash, hash, sizeof(storedPasswordHash) - 1);

    request->send(200, "text/plain", "OK");
  });

  server.on("/checkPassword", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true)) {
      request->send(400, "text/plain", "Missing password");
      return;
    }
    String password = request->getParam("password", true)->value();
    password.trim();

    if (validatePassword(password.c_str())) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(403, "text/plain", "FAIL");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();

    if (LittleFS.exists(path)) {
      String contentType = "text/html";
      if (path.endsWith(".css")) contentType = "text/css";
      else if (path.endsWith(".js")) contentType = "application/javascript";
      else if (path.endsWith(".json")) contentType = "application/json";
      else if (path.endsWith(".png")) contentType = "image/png";
      else if (path.endsWith(".jpg")) contentType = "image/jpeg";

      Serial.println("File found in LittleFS, serving with content-type: " + contentType);
      request->send(LittleFS, path, contentType);
    } else {
      Serial.print("File not found in LittleFS: ");
      Serial.println(path);
      Serial.println("Redirecting to captive portal: " + WiFi.softAPIP().toString());
      request->redirect("http://" + WiFi.softAPIP().toString());
    }
  });

  // Setup event source for real-time updates
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  server.addHandler(&events);
  server.begin();
  Serial.println("=== CONFIG SERVER STARTED ===");
}

void dnsHandleRequest() {  //process dns request for captive portals
  if (currentWiFiMode == AWIFI_MODE_AP) {
    dnsServer.processNextRequest();
  }
}


void checkWiFiConnection() {
  static unsigned long lastWiFiCheckTime = 0;
  static bool reconnecting = false;
  static int reconnectAttempts = 0;
  static unsigned long firstDisconnectTime = 0;
  static wl_status_t lastWiFiStatus = WL_IDLE_STATUS;
  static unsigned long throttle123 = 0;  // Throttle CurrentWifiSessionDuration update

  wl_status_t currentStatus = WiFi.status();

  if (lastWiFiStatus == WL_CONNECTED && currentStatus != WL_CONNECTED) {
    // Just disconnected
    Serial.println("we just disconnected wifi");
    wifiSessionStartTime = 0;  // Reset WiFi session timer
    firstDisconnectTime = millis();
    reconnectAttempts = 0;
    saveNVSData();  // Save the zerod out values
  }
  lastWiFiStatus = currentStatus;

  if (currentWiFiMode == AWIFI_MODE_CLIENT && currentStatus != WL_CONNECTED) {
    if (reconnecting) return;

    // Aggressive reconnection strategy
    unsigned long reconnectInterval = 2000;  // Start with 2 seconds

    // Increase interval based on attempts
    if (reconnectAttempts > 5) reconnectInterval = 5000;    // 5 seconds after 5 attempts
    if (reconnectAttempts > 10) reconnectInterval = 10000;  // 10 seconds after 10 attempts
    if (reconnectAttempts > 20) reconnectInterval = 30000;  // 30 seconds after 20 attempts

    // After 10 minutes, show recovery dialog
    if (firstDisconnectTime > 0 && (millis() - firstDisconnectTime) > 600000) {
      if (reconnectAttempts % 10 == 0) {  // Only show occasionally
        queueConsoleMessage("WiFi disconnected for 10+ minutes. Consider restarting if needed.");
      }
    }

    if (millis() - lastWiFiCheckTime > reconnectInterval) {
      lastWiFiCheckTime = millis();
      reconnecting = true;
      reconnectAttempts++;

      String saved_ssid = readFile(LittleFS, WIFI_SSID_FILE);
      String saved_password = readFile(LittleFS, WIFI_PASS_FILE);

      if (connectToWiFi(saved_ssid.c_str(), saved_password.c_str(), 5000)) {  // 5 sec timeout
        wifiSessionStartTime = millis();
        wifiReconnectsThisSession++;  // Increment reconnect counter
        wifiReconnectsTotal++;
        Serial.println("wifiReconnectsTotal ");
        Serial.println(wifiReconnectsTotal);
        saveNVSData();  // Save the updated reconnect count
        reconnectAttempts = 0;
        firstDisconnectTime = 0;
        queueConsoleMessage("WiFi reconnected successfully (Reconnect #" + String(wifiReconnectsThisSession) + ")");
      }

      reconnecting = false;
    }
  } else if (currentWiFiMode == AWIFI_MODE_CLIENT && currentStatus == WL_CONNECTED) {
    // Connected - reset counters
    if (reconnectAttempts > 0) {
      reconnectAttempts = 0;
      firstDisconnectTime = 0;
      wifiSessionStartTime = millis();  // Reset WiFi session timer on reconnect
    }

    // Throttled update of session duration
    if (wifiSessionStartTime > 0 && millis() - throttle123 >= 1000) {
      CurrentWifiSessionDuration = (millis() - wifiSessionStartTime) / 1000 / 60;  //minutes
      throttle123 = millis();
    }
  }
}



/**
 * AI_SUMMARY: WiFi data transmission engine - builds 140+ field CSV payload, manages real-time EventSource updates, handles plot data, connection monitoring
 * AI_PURPOSE: Core web interface driver - packages all sensor/status data into CSV, sends via EventSource, updates 4 uPlot charts, manages WiFi connection status
 * AI_INPUTS: 140+ global variables (IBV, Bcur, MeasuredAmps, dutyCycle, etc.), timing controls (webgaugesinterval=1000ms), plot arrays, connection state
 * AI_OUTPUTS: EventSource "CSVData" (1400-byte CSV every 1sec), "TimestampData" (staleness ages every 3sec), 4 plot updates, connection status UI, console messages
 * AI_DEPENDENCIES: fieldIndexes[146] mapping array in HTML (CRITICAL - must stay in sync!), static payload[1400] buffer, currentTempData/voltageData/rpmData/temperatureData plot arrays, events EventSource object
 * AI_RISKS: 1400-byte WiFi packet limit at 95% capacity. Inconsistent scaling (voltage×100, current×100, fieldCurrent×10, dynamicFactors×1000, lifePercent×100). fieldIndexes array desync breaks entire interface. Static buffer prevents overflow but limits growth.  Complex conditional payload building easy to break.
 * AI_OPTIMIZE: MONSTER FUNCTION doing 6+ jobs - someday needs splitting up. When adding CSV fields: 1)Add to snprintf payload, 2)Add to fieldIndexes in HTML, 3)Update field count check. Scaling factors follow no pattern - always verify end-to-end. 
 */
void SendWifiData() {
  static unsigned long prev_millis5 = 0;
  static unsigned long lastTimestampSend = 0;
  unsigned long now = millis();
  bool sendingPayloadThisCycle = false;
  // ===== Main CSV Payload =====
  if (now - prev_millis5 > webgaugesinterval) {


    // Calculate maxPoints for uPlotting based on time window and update rate
    int calculatedMaxPoints = (plotTimeWindow * 1000) / webgaugesinterval;
    calculatedMaxPoints = constrain(calculatedMaxPoints, 1, 10000000);
    WifiStrength = WiFi.RSSI();
    WifiHeartBeat++;
    processConsoleQueue();  // Process console message queue - send one message per interval
    if (WifiStrength >= -70) {
      int start66 = micros();     // Start timing the wifi section
                                  // Build CSV string with all data as integers
                                  // Format: multiply floats by 10, 100 or 1000 to preserve decimal precision as needed
                                  // CSV field order: see index.html -> fields[] mapping
      static char payload[1400];  // >1400 the wifi transmission won't fit in 1 packet , must be static to save heap!
      snprintf(payload, sizeof(payload),
               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",

               // Readings
               SafeInt(AlternatorTemperatureF),     // 0
               SafeInt(dutyCycle),                  // 1
               SafeInt(BatteryV, 100),              // 2
               SafeInt(MeasuredAmps, 100),          // 3
               SafeInt(RPM),                        // 4
               SafeInt(Channel3V, 100),             // 5
               SafeInt(IBV, 100),                   // 6
               SafeInt(Bcur, 100),                  // 7
               SafeInt(VictronVoltage, 100),        // 8
               SafeInt(LoopTime),                   // 9
               SafeInt(WifiStrength),               // 10
               SafeInt(WifiHeartBeat),              // 11
               SafeInt(SendWifiTime),               // 12
               SafeInt(AnalogReadTime),             // 13
               SafeInt(VeTime),                     // 14
               SafeInt(MaximumLoopTime),            // 15
               SafeInt(HeadingNMEA),                // 16
               SafeInt(vvout, 100),                 // 17
               SafeInt(iiout, 10),                  // 18
               SafeInt(FreeHeap),                   // 19
               SafeInt(IBVMax, 100),                // 20
               SafeInt(MeasuredAmpsMax, 100),       // 21
               SafeInt(RPMMax),                     // 22
               SafeInt(SOC_percent),                // 23
               SafeInt(EngineRunTime),              // 24
               SafeInt(EngineCycles),               // 25
               SafeInt(AlternatorOnTime),           // 26
               SafeInt(AlternatorFuelUsed),         // 27
               SafeInt(ChargedEnergy),              // 28
               SafeInt(DischargedEnergy),           // 29
               SafeInt(AlternatorChargedEnergy),    // 30
               SafeInt(MaxAlternatorTemperatureF),  // 31

               // Settings Echo
               SafeInt(AlternatorTemperatureLimitF),  // 32
               SafeInt(ChargingVoltageTarget, 100),   // 33
               SafeInt(TargetAmps),                   // 34
               SafeInt(TargetFloatVoltage, 100),      // 35
               SafeInt(fffr),                         // 36
               SafeInt(interval, 100),                // 37
               SafeInt(FieldAdjustmentInterval),      // 38
               SafeInt(ManualDutyTarget),             // 39
               SafeInt(SwitchControlOverride),        // 40
               SafeInt(OnOff),                        // 41
               SafeInt(ManualFieldToggle),            // 42
               SafeInt(HiLow),                        // 43
               SafeInt(LimpHome),                     // 44
               SafeInt(VeData),                       // 45
               SafeInt(NMEA0183Data),                 // 46
               SafeInt(NMEA2KData),                   // 47
               SafeInt(TargetAmpL),                   // 48

               // More Settings
               SafeInt(CurrentThreshold, 100),      // 49
               SafeInt(PeukertExponent_scaled),     // 50
               SafeInt(ChargeEfficiency_scaled),    // 51
               SafeInt(ChargedVoltage_Scaled),      // 52
               SafeInt(TailCurrent),                // 53
               SafeInt(ChargedDetectionTime),       // 54
               SafeInt(IgnoreTemperature),          // 55
               SafeInt(bmsLogic),                   // 56
               SafeInt(bmsLogicLevelOff),           // 57
               SafeInt(AlarmActivate),              // 58
               SafeInt(TempAlarm),                  // 59
               SafeInt(VoltageAlarmHigh),           // 60
               SafeInt(VoltageAlarmLow),            // 61
               SafeInt(CurrentAlarmHigh),           // 62
               SafeInt(FourWay),                    // 63
               SafeInt(RPMScalingFactor),           // 64
               SafeInt(ResetTemp),                  // 65
               SafeInt(ResetVoltage),               // 66
               SafeInt(ResetCurrent),               // 67
               SafeInt(ResetEngineRunTime),         // 68
               SafeInt(ResetAlternatorOnTime),      // 69
               SafeInt(ResetEnergy),                // 70
               SafeInt(MaximumAllowedBatteryAmps),  // 71
               SafeInt(ManualSOCPoint),             // 72
               SafeInt(BatteryVoltageSource),       //73
               SafeInt(AmpControlByRPM),            //74
               SafeInt(RPM1),                       //75
               SafeInt(RPM2),                       //76
               SafeInt(RPM3),                       //77
               SafeInt(RPM4),                       //78
               SafeInt(Amps1),                      //79
               SafeInt(Amps2),                      //80
               SafeInt(Amps3),                      //81
               SafeInt(Amps4),                      //82
               SafeInt(RPM5),                       //83
               SafeInt(RPM6),                       //84
               SafeInt(RPM7),                       //85
               SafeInt(Amps5),                      //86
               SafeInt(Amps6),                      //87
               SafeInt(Amps7),                      //88
               SafeInt(ShuntResistanceMicroOhm),    //89
               SafeInt(InvertAltAmps),              // 90
               SafeInt(InvertBattAmps),             //91
               SafeInt(MaxDuty),                    //92
               SafeInt(MinDuty),                    //93
               SafeInt(FieldResistance),            //94
               SafeInt(maxPoints),                  //95
               SafeInt(AlternatorCOffset, 100),     //96
               SafeInt(BatteryCOffset, 100),        //97
               SafeInt(BatteryCapacity_Ah),
               SafeInt(AmpSrc),
               SafeInt(R_fixed, 100),  //100
               SafeInt(Beta, 100),
               SafeInt(T0_C, 100),         //102
               SafeInt(TempSource),        //103
               SafeInt(IgnitionOverride),  //104
               // More Readings
               SafeInt(temperatureThermistor),         // 105
               SafeInt(MaxTemperatureThermistor),      // 106
               SafeInt(VictronCurrent, 100),           // 107
                                                       //AlarmStuff
               SafeInt(AlarmTest),                     // New index: 108
               SafeInt(AlarmLatchEnabled),             // New index: 109
               SafeInt(alarmLatch ? 1 : 0),            // New index: 110 (current latch state)
               SafeInt(ResetAlarmLatch),               // New index: 111
               SafeInt(ForceFloat),                    //  112
               SafeInt(bulkCompleteTime),              // 113
               SafeInt(FLOAT_DURATION),                // 114
               SafeInt(LatitudeNMEA * 1000000),        // 115 - Convert to integer with 6 decimal precision
               SafeInt(LongitudeNMEA * 1000000),       // 116 - Convert to integer with 6 decimal precision
               SafeInt(SatelliteCountNMEA),            // 117
               SafeInt(GPIO33_Status),                 //  118
               SafeInt(fieldActiveStatus),             // 119
               SafeInt(timeToFullChargeMin),           // 120
               SafeInt(timeToFullDischargeMin),        // 121
               SafeInt(AutoShuntGainCorrection),       // 122 BOOLEAN
               SafeInt(DynamicShuntGainFactor, 1000),  // 123 (multiply by 1000 for 3 decimal precision)
               SafeInt(AutoAltCurrentZero),            // 124 // BOOLEAN
               SafeInt(DynamicAltCurrentZero, 1000),   // 125 (multiply by 1000 for 3 decimal
               SafeInt(InsulationLifePercent, 100),    // XX% * 100 for 2 decimal precision
               SafeInt(GreaseLifePercent, 100),        // XX% * 100 for 2 decimal precision
               SafeInt(BrushLifePercent, 100),         // XX% * 100 for 2 decimal precision
               SafeInt(PredictedLifeHours),            // Integer hours
               SafeInt(LifeIndicatorColor),            // 0=green, 1=yellow, 2=red
               SafeInt(WindingTempOffset, 10),         // User setting * 10 for 1 decimal
               SafeInt(PulleyRatio, 100),              // 132    User setting * 100 for 2 decimals
               SafeInt(LastSessionDuration),           // Last session duration (minutes)
               SafeInt(LastSessionMaxLoopTime),        // Last session max loop (μs) //134
               SafeInt(lastSessionMinHeap),            // Last session min heap (KB)
               SafeInt(wifiReconnectsThisSession),     // WiFi reconnects this session
               SafeInt(wifiReconnectsTotal),           // WiFi reconnects total
               SafeInt(CurrentSessionDuration),        // 138
               SafeInt(CurrentWifiSessionDuration),    // Current WiFi session (minutes)
               SafeInt(LastResetReason),               // 140 it's already and int, but whatever
               SafeInt(ancientResetReason),            // 141
               SafeInt(ManualLifePercentage),          //142

               SafeInt(totalPowerCycles),         // 143
               SafeInt(lastWifiSessionDuration),  // 144
               SafeInt(EXTRA),                    // 145
               SafeInt(BatteryCurrentSource),     // 146
               SafeInt(timeAxisModeChanging),     // 147
               SafeInt(webgaugesinterval),        // 148
               SafeInt(plotTimeWindow),           // 149

               SafeInt(Ymin1),  // 150
               SafeInt(Ymax1),
               SafeInt(Ymin2),  //152
               SafeInt(Ymax2),
               SafeInt(Ymin3),  //154
               SafeInt(Ymax3),
               SafeInt(Ymin4),  //156
               SafeInt(Ymax4)   //157


      );
      // Send main sensor data payload
      events.send(payload, "CSVData");    // Changed event name to reflect new format
      SendWifiTime = micros() - start66;  // Calculate WiFi Send Time
      sendingPayloadThisCycle = true;     // Flag this as a bad function iteration to also send timestampPayload
    }
    prev_millis5 = now;
  }

  // keepalive sent every 25 seconds to prevent HTTP timeout on a wifi disconnection
  static unsigned long lastKeepalive = 0;
  if (millis() - lastKeepalive > 25000) {
    events.send(": keepalive\n", NULL);  // SSE comment = keepalive
    lastKeepalive = millis();
  }
  //            timestampPayload - NOW SENDS AGES INSTEAD OF TIMESTAMPS
  if (!sendingPayloadThisCycle && now - lastTimestampSend > 3000) {  // Every 3 seconds
    static char timestampPayload[400];                               // Smaller buffer for ages, must be static to save heap
    // Calculate ages (how long ago each sensor was updated) instead of absolute timestamps
    snprintf(timestampPayload, sizeof(timestampPayload),
             "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
             (dataTimestamps[IDX_HEADING_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_HEADING_NMEA]),        // 0 - Age of heading data
             (dataTimestamps[IDX_LATITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LATITUDE_NMEA]),      // 1 - Age of latitude data
             (dataTimestamps[IDX_LONGITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LONGITUDE_NMEA]),    // 2 - Age of longitude data
             (dataTimestamps[IDX_SATELLITE_COUNT] == 0) ? 999999 : (now - dataTimestamps[IDX_SATELLITE_COUNT]),  // 3 - Age of satellite count
             (dataTimestamps[IDX_VICTRON_VOLTAGE] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_VOLTAGE]),  // 4 - Age of Victron voltage
             (dataTimestamps[IDX_VICTRON_CURRENT] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_CURRENT]),  // 5 - Age of Victron current
             (dataTimestamps[IDX_ALTERNATOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_ALTERNATOR_TEMP]),  // 6 - Age of alternator temp
             (dataTimestamps[IDX_THERMISTOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_THERMISTOR_TEMP]),  // 7 - Age of thermistor temp
             (dataTimestamps[IDX_RPM] == 0) ? 999999 : (now - dataTimestamps[IDX_RPM]),                          // 8 - Age of RPM data
             (dataTimestamps[IDX_MEASURED_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_MEASURED_AMPS]),      // 9 - Age of alternator current
             (dataTimestamps[IDX_BATTERY_V] == 0) ? 999999 : (now - dataTimestamps[IDX_BATTERY_V]),              // 10 - Age of ADS battery voltage
             (dataTimestamps[IDX_IBV] == 0) ? 999999 : (now - dataTimestamps[IDX_IBV]),                          // 11 - Age of INA battery voltage
             (dataTimestamps[IDX_BCUR] == 0) ? 999999 : (now - dataTimestamps[IDX_BCUR]),                        // 12 - Age of battery current
             (dataTimestamps[IDX_CHANNEL3V] == 0) ? 999999 : (now - dataTimestamps[IDX_CHANNEL3V]),              // 13 - Age of ADS Ch3 voltage
             (dataTimestamps[IDX_DUTY_CYCLE] == 0) ? 999999 : (now - dataTimestamps[IDX_DUTY_CYCLE]),            // 14 - Age of duty cycle calculation
             (dataTimestamps[IDX_FIELD_VOLTS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_VOLTS]),          // 15 - Age of field voltage calculation
             (dataTimestamps[IDX_FIELD_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_AMPS])             // 16 - Age of field current calculation
    );
    events.send(timestampPayload, "TimestampData");
    lastTimestampSend = now;
  }
}

void UpdateBatterySOC(unsigned long elapsedMillis) {
  // Convert elapsed milliseconds to seconds for calculations
  float elapsedSeconds = elapsedMillis / 1000.0f;

  // Update scaled values
  float currentBatteryVoltage = getBatteryVoltage();
  Voltage_scaled = currentBatteryVoltage * 100;

  // Use dedicated battery current function for SoC tracking
  float batteryCurrentForSoC = getBatteryCurrent();    // New function that only uses battery sources
  BatteryCurrent_scaled = batteryCurrentForSoC * 100;  // Now properly sourced and inverted

  AlternatorCurrent_scaled = MeasuredAmps * 100;
  BatteryPower_scaled = (Voltage_scaled * BatteryCurrent_scaled) / 100;  // W × 100

  // FIXED: Energy calculation using proper floating point math, then convert to integer
  float batteryPower_W = BatteryPower_scaled / 100.0f;
  float energyDelta_Wh = (batteryPower_W * elapsedSeconds) / 3600.0f;

  // Calculate alternator energy (this was missing in my previous version)
  float alternatorPower_W = (currentBatteryVoltage * MeasuredAmps);
  float altEnergyDelta_Wh = (alternatorPower_W * elapsedSeconds) / 3600.0f;

  // Calculate fuel used (convert Wh to mL for integer storage)
  if (altEnergyDelta_Wh > 0) {
    // 1. Convert watt-hours to joules (1 Wh = 3600 J)
    float energyJoules = altEnergyDelta_Wh * 3600.0f;
    // 2. Assume engine thermal efficiency is 30%
    const float engineEfficiency = 0.30f;
    // 3. Assume alternator mechanical-to-electrical efficiency is 50%
    const float alternatorEfficiency = 0.50f;
    // 4. Total system efficiency = engine × alternator = 0.30 × 0.50 = 0.15
    float fuelEnergyUsed_J = energyJoules / (engineEfficiency * alternatorEfficiency);
    // 5. Diesel energy content ≈ 36,000 J per mL
    const float dieselEnergy_J_per_mL = 36000.0f;
    // 6. Convert fuel energy to actual mL of diesel burned
    float fuelUsed_mL = fuelEnergyUsed_J / dieselEnergy_J_per_mL;
    // 7. Use accumulator to prevent losing small values
    static float fuelAccumulator = 0.0f;
    fuelAccumulator += fuelUsed_mL;
    if (fuelAccumulator >= 1.0f) {
      AlternatorFuelUsed += (int)fuelAccumulator;
      fuelAccumulator -= (int)fuelAccumulator;
    }
  }

  // Energy accumulation - use proper rounding to preserve precision
  static float chargedEnergyAccumulator = 0.0f;
  static float dischargedEnergyAccumulator = 0.0f;
  static float alternatorEnergyAccumulator = 0.0f;

  if (BatteryCurrent_scaled > 0) {
    // Charging - energy going into battery
    chargedEnergyAccumulator += energyDelta_Wh;
    if (chargedEnergyAccumulator >= 1.0f) {
      ChargedEnergy += (int)chargedEnergyAccumulator;
      chargedEnergyAccumulator -= (int)chargedEnergyAccumulator;
    }
  } else if (BatteryCurrent_scaled < 0) {
    // Discharging - energy coming out of battery
    dischargedEnergyAccumulator += abs(energyDelta_Wh);
    if (dischargedEnergyAccumulator >= 1.0f) {
      DischargedEnergy += (int)dischargedEnergyAccumulator;
      dischargedEnergyAccumulator -= (int)dischargedEnergyAccumulator;
    }
  }

  // For alternator energy:
  if (altEnergyDelta_Wh > 0) {
    alternatorEnergyAccumulator += altEnergyDelta_Wh;
    if (alternatorEnergyAccumulator >= 1.0f) {
      AlternatorChargedEnergy += (int)alternatorEnergyAccumulator;
      alternatorEnergyAccumulator -= (int)alternatorEnergyAccumulator;
    }
  }

  // Rest of the function continues as in the original...
  alternatorIsOn = (AlternatorCurrent_scaled > CurrentThreshold * 100);

  if (alternatorIsOn) {
    alternatorOnAccumulator += elapsedMillis;
    if (alternatorOnAccumulator >= 60000) {
      AlternatorOnTime += alternatorOnAccumulator / 60000;
      alternatorOnAccumulator %= 60000;
    }
  }

  alternatorWasOn = alternatorIsOn;

  // FIXED: Use floating point math for Ah calculations, then accumulate properly
  static float coulombAccumulator_Ah = 0.0f;

  // Calculate actual Ah change using floating point
  float batteryCurrent_A = BatteryCurrent_scaled / 100.0f;
  float deltaAh = (batteryCurrent_A * elapsedSeconds) / 3600.0f;

  if (BatteryCurrent_scaled >= 0) {
    // Charging - apply charge efficiency
    float batteryDeltaAh = deltaAh * (ChargeEfficiency_scaled / 100.0f);
    coulombAccumulator_Ah += batteryDeltaAh;
  } else {
    // Discharging - apply Peukert compensation only above threshold
    float dischargeCurrent_A = abs(batteryCurrent_A);

    // Check if current is above C/100 threshold for Peukert application
    float peukertThreshold = BatteryCapacity_Ah / 100.0f;  // C/100 rate

    if (dischargeCurrent_A > peukertThreshold) {
      // Apply Peukert equation
      float peukertExponent = PeukertExponent_scaled / 100.0f;  // Convert 112 back to 1.12

      // Constrain current to reasonable range
      if (dischargeCurrent_A > BatteryCapacity_Ah) dischargeCurrent_A = BatteryCapacity_Ah;  // Max 1C rate

      float currentRatio = PeukertRatedCurrent_A / dischargeCurrent_A;
      float peukertFactor = pow(currentRatio, peukertExponent - 1.0f);

      // Sanity limits: don't let Peukert factor go crazy
      peukertFactor = constrain(peukertFactor, 0.5f, 2.0f);

      float batteryDeltaAh = deltaAh * peukertFactor;
      coulombAccumulator_Ah += batteryDeltaAh;
    } else {
      // Below threshold - no Peukert compensation (factor = 1.0)
      coulombAccumulator_Ah += deltaAh;
    }
  }

  // Update the scaled coulomb count when we have accumulated enough change
  if (abs(coulombAccumulator_Ah) >= 0.01f) {
    int deltaAh_scaled = (int)(coulombAccumulator_Ah * 100.0f);
    CoulombCount_Ah_scaled += deltaAh_scaled;
    coulombAccumulator_Ah -= (deltaAh_scaled / 100.0f);
  }

  // Constrain and calculate SoC with decimal precision
  CoulombCount_Ah_scaled = constrain(CoulombCount_Ah_scaled, 0, BatteryCapacity_Ah * 100);
  float SoC_float = (float)CoulombCount_Ah_scaled / (BatteryCapacity_Ah * 100.0f) * 100.0f;
  SOC_percent = (int)(SoC_float * 100);  // Store as percentage × 100 for 2 decimal places

  // --- Full Charge Detection ---
  if ((abs(BatteryCurrent_scaled) <= (TailCurrent * BatteryCapacity_Ah / 100)) && (Voltage_scaled >= ChargedVoltage_Scaled)) {
    FullChargeTimer += elapsedSeconds;
    if (FullChargeTimer >= ChargedDetectionTime) {
      SOC_percent = 10000;  // 100.00% (scaled by 100)
      CoulombCount_Ah_scaled = BatteryCapacity_Ah * 100;
      FullChargeDetected = true;
      coulombAccumulator_Ah = 0.0f;
      queueConsoleMessage("BATTERY: Full charge detected - SoC reset to 100%");
      applySocGainCorrection();  // apply automatic dynamic correction to shunt sensor
    }
  } else {
    FullChargeTimer = 0;
    FullChargeDetected = false;
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


void sha256(const char *input, char *outputBuffer) {  // for security
  byte shaResult[32];
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)input, strlen(input));
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);

  for (int i = 0; i < 32; ++i) {
    sprintf(outputBuffer + (i * 2), "%02x", shaResult[i]);
  }
}

void loadPasswordHash() {
  // First try to load plaintext password (for auth)
  if (LittleFS.exists("/password.txt")) {
    File plainFile = LittleFS.open("/password.txt", "r");
    if (plainFile) {
      String pwdStr = plainFile.readStringUntil('\n');
      pwdStr.trim();
      strncpy(requiredPassword, pwdStr.c_str(), sizeof(requiredPassword) - 1);
      plainFile.close();
      Serial.println("Plaintext password loaded from LittleFS");
    }
  }

  // Now load the hash (for future use)
  if (LittleFS.exists("/password.hash")) {
    File file = LittleFS.open("/password.hash", "r");
    if (file) {
      size_t len = file.readBytesUntil('\n', storedPasswordHash, sizeof(storedPasswordHash) - 1);
      storedPasswordHash[len] = '\0';  // null-terminate
      file.close();
      Serial.println("Password hash loaded from LittleFS");
      return;
    }
  }

  // If we get here, no password files exist - set defaults
  strncpy(requiredPassword, "admin", sizeof(requiredPassword) - 1);
  sha256("admin", storedPasswordHash);
  Serial.println("No password file, using default admin password");
}

void savePasswordHash() {
  File file = LittleFS.open("/password.hash", "w");
  if (file) {
    file.println(storedPasswordHash);
    file.close();
    Serial.println("Password hash saved to LittleFS");
    queueConsoleMessage("Password hash saved to LittleFS");
  } else {
    Serial.println("Failed to open password.hash for writing");
    queueConsoleMessage("Failed to open password.hash for writing");
  }
}

void savePasswordPlaintext(const char *password) {
  File file = LittleFS.open("/password.txt", "w");
  if (file) {
    file.println(password);
    file.close();
    Serial.println("Password saved to LittleFS");
    queueConsoleMessage("Password saved");

  } else {
    Serial.println("Failed to open password.txt for writing");
    queueConsoleMessage("Password save failed");
  }
}

bool validatePassword(const char *password) {
  if (!password) return false;

  char hash[65] = { 0 };
  sha256(password, hash);

  return (strcmp(hash, storedPasswordHash) == 0);
}


void queueConsoleMessage(String message) {
  // Prevent stack overflow from oversized messages
  if (message.length() > 120) {
    message = message.substring(0, 120) + "...";
  }

  // Thread-safe circular buffer
  int nextHead = (consoleHead + 1) % CONSOLE_QUEUE_SIZE;
  int localTail = consoleTail;  // Copy volatile to local

  if (nextHead != localTail) {  // Not full
    strncpy(consoleQueue[consoleHead].message, message.c_str(), 127);
    consoleQueue[consoleHead].message[127] = '\0';
    consoleQueue[consoleHead].timestamp = millis();
    consoleHead = nextHead;

    int localCount = consoleCount;
    if (localCount < CONSOLE_QUEUE_SIZE) {
      consoleCount = localCount + 1;
    }
  }
  // If full, oldest message is automatically overwritten
}

void processConsoleQueue() {
  unsigned long now = millis();

  if (now - lastConsoleMessageTime >= CONSOLE_MESSAGE_INTERVAL) {
    int localCount = consoleCount;  // Copy volatile to local

    if (localCount > 0) {
      // Send up to 2 messages per interval
      int messagesToSend = (2 < localCount) ? 2 : localCount;

      for (int i = 0; i < messagesToSend; i++) {
        if (consoleCount > 0) {
          events.send(consoleQueue[consoleTail].message, "console");
          consoleTail = (consoleTail + 1) % CONSOLE_QUEUE_SIZE;
          consoleCount--;
        }
      }
      lastConsoleMessageTime = now;
    }
  }
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

int interpolateAmpsFromRPM(float currentRPM) {
  // Create arrays from the individual global variables for easier processing
  // These represent the user-configured RPM/Amps curve points from the web interface
  int rpmPoints[] = { RPM1, RPM2, RPM3, RPM4, RPM5, RPM6, RPM7 };
  int ampPoints[] = { Amps1, Amps2, Amps3, Amps4, Amps5, Amps6, Amps7 };

  // Find how many valid entries exist in the table (non-zero RPM values)
  // Users may not fill all 7 points, so we need to know where the table ends
  int validPoints = 0;
  for (int i = 0; i < 7; i++) {
    if (rpmPoints[i] > 0) {
      validPoints = i + 1;  // Count consecutive valid entries from start
    }
  }

  // Safety check: If no valid points configured, fall back to normal target amps
  if (validPoints == 0) {
    return TargetAmps;  // Use the standard charging amperage setting
  }

  // Handle edge cases: RPM below or above the configured table range

  // If current RPM is at or below the lowest configured point
  if (currentRPM <= rpmPoints[0]) {
    return ampPoints[0];  // Use the lowest configured amperage
  }

  // If current RPM is at or above the highest configured point
  if (currentRPM >= rpmPoints[validPoints - 1]) {
    return ampPoints[validPoints - 1];  // Use the highest configured amperage
  }

  // Linear interpolation between adjacent points in the table
  // Find which two points the current RPM falls between
  for (int i = 0; i < validPoints - 1; i++) {
    // Check if current RPM is between point i and point i+1
    if (currentRPM >= rpmPoints[i] && currentRPM <= rpmPoints[i + 1]) {
      // Calculate interpolation ratio (0.0 = at first point, 1.0 = at second point)
      float ratio = (currentRPM - rpmPoints[i]) / (float)(rpmPoints[i + 1] - rpmPoints[i]);

      // Linear interpolation formula: startValue + ratio * (endValue - startValue)
      // This gives smooth transitions between configured points
      return ampPoints[i] + (ratio * (ampPoints[i + 1] - ampPoints[i]));
    }
  }

  // Fallback safety net (should never reach here with valid data)
  // If somehow we get here, use the standard target amps
  return TargetAmps;
}
float getBatteryVoltage() {
  static unsigned long lastWarningTime = 0;
  const unsigned long WARNING_INTERVAL = 10000;  // 10 seconds between warnings
  float selectedVoltage = 0;
  switch (BatteryVoltageSource) {
    case 0:  // INA228
      if (INADisconnected == 0 && !isnan(IBV) && IBV > 8.0 && IBV < 70.0 && millis() > 5000) {
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
    selectedVoltage = 999;  // This is bs, but do something later
  }

  return selectedVoltage;
}

float getTargetAmps() {
  float targetValue = 0;

  switch (AmpSrc) {
    case 0:  // Alt Hall Effect Sensor
      targetValue = MeasuredAmps;
      break;

    case 1:  // Battery Shunt
      targetValue = Bcur;
      break;

    case 2:  // NMEA2K Batt
      queueConsoleMessage("C2a NMEA2K Battery current not implemented, using Battery Shunt");
      targetValue = Bcur;
      break;

    case 3:  // NMEA2K Alt
      queueConsoleMessage("C3a NMEA2K Alternator current not implemented, using Alt Hall Sensor");
      targetValue = MeasuredAmps;
      break;

    case 4:  // NMEA0183 Batt
      queueConsoleMessage("C4a NMEA0183 Battery current not implemented, using Battery Shunt");
      targetValue = Bcur;
      break;

    case 5:  // NMEA0183 Alt
      queueConsoleMessage("C5a NMEA0183 Alternator current not implemented, using Alt Hall Sensor");
      targetValue = MeasuredAmps;
      break;

    case 6:                             // Victron Batt
      if (abs(VictronCurrent) > 0.1) {  // Valid reading
        targetValue = VictronCurrent;
      } else {
        queueConsoleMessage("C6a Victron current not available, using Battery Shunt");
        targetValue = Bcur;
      }
      break;

    case 7:  // Other
      targetValue = MeasuredAmps;
      break;

    default:
      queueConsoleMessage("Invalid AmpSrc (" + String(AmpSrc) + "), using Alt Hall Sensor");
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

  digitalWrite(33, finalOutput ? HIGH : LOW);  // get rid of this bs
  // digitalWrite(33, finalOutput);     // go back to this later
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
    ChargingVoltageTarget = FullChargeVoltage;

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
    ChargingVoltageTarget = TargetFloatVoltage;

    // Return to bulk after time expires OR if voltage drops significantly
    if ((millis() - floatStartTime > FLOAT_DURATION * 1000) || (currentVoltage < TargetFloatVoltage - 0.5)) {
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

void initializeNVS() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    Serial.println("NVS init issue: erasing NVS and retrying...");
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      Serial.printf("ERROR: Failed to erase NVS (code %d)\n", err);
      return;
    }
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to initialize NVS (code %d)\n", err);
    return;
  }
  Serial.println("NVS initialized successfully");
  queueConsoleMessage("NVS initialized successfully");
}

void saveNVSData() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("ERROR: Failed to open NVS");
    Serial.println("ERROR: Failed to open NVS");
    return;
  }
  // Energy Tracking
  nvs_set_u32(nvs_handle, "ChargedEnergy", (uint32_t)ChargedEnergy);
  nvs_set_u32(nvs_handle, "DischrgdEnergy", (uint32_t)DischargedEnergy);
  nvs_set_u32(nvs_handle, "AltChrgdEnergy", (uint32_t)AlternatorChargedEnergy);
  nvs_set_i32(nvs_handle, "AltFuelUsed", (int32_t)AlternatorFuelUsed);

  // Runtime Tracking
  nvs_set_i32(nvs_handle, "EngineRunTime", (int32_t)EngineRunTime);
  nvs_set_i32(nvs_handle, "EngineCycles", (int32_t)EngineCycles);
  nvs_set_i32(nvs_handle, "AltOnTime", (int32_t)AlternatorOnTime);

  // Battery State
  nvs_set_i32(nvs_handle, "SOC_percent", (int32_t)SOC_percent);
  nvs_set_i32(nvs_handle, "CoulombCount", (int32_t)CoulombCount_Ah_scaled);

  // Session Health Stats
  nvs_set_u32(nvs_handle, "SessionDur", (uint32_t)CurrentSessionDuration);
  nvs_set_i32(nvs_handle, "MaxLoop", (int32_t)MaxLoopTime);
  nvs_set_i32(nvs_handle, "MinHeap", (int32_t)sessionMinHeap);
  nvs_set_u32(nvs_handle, "WifiDur", (uint32_t)CurrentWifiSessionDuration);

  // System Health Counters
  nvs_set_i32(nvs_handle, "WifiReconnects", (int32_t)wifiReconnectsTotal);
  nvs_set_i32(nvs_handle, "PowerCycles", (int32_t)totalPowerCycles);

  // Thermal Stress
  nvs_set_blob(nvs_handle, "InsulDamage", &CumulativeInsulationDamage, sizeof(float));
  nvs_set_blob(nvs_handle, "GreaseDamage", &CumulativeGreaseDamage, sizeof(float));
  nvs_set_blob(nvs_handle, "BrushDamage", &CumulativeBrushDamage, sizeof(float));

  // Dynamic Learning
  nvs_set_blob(nvs_handle, "ShuntGain", &DynamicShuntGainFactor, sizeof(float));
  nvs_set_blob(nvs_handle, "AltZero", &DynamicAltCurrentZero, sizeof(float));
  nvs_set_u32(nvs_handle, "LastGainTime", (uint32_t)lastGainCorrectionTime);
  nvs_set_u32(nvs_handle, "LastZeroTime", (uint32_t)lastAutoZeroTime);
  nvs_set_blob(nvs_handle, "LastZeroTemp", &lastAutoZeroTemp, sizeof(float));

  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
}

void loadNVSData() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("NVS: No existing data found, using defaults");
      Serial.println("NVS: No existing data found, using defaults");

    return;
  }

  size_t required_size;
  uint32_t temp_uint32;
  int32_t temp_int32;

  // Energy Tracking
  if (nvs_get_u32(nvs_handle, "ChargedEnergy", &temp_uint32) == ESP_OK) ChargedEnergy = temp_uint32;
  if (nvs_get_u32(nvs_handle, "DischrgdEnergy", &temp_uint32) == ESP_OK) DischargedEnergy = temp_uint32;
  if (nvs_get_u32(nvs_handle, "AltChrgdEnergy", &temp_uint32) == ESP_OK) AlternatorChargedEnergy = temp_uint32;
  if (nvs_get_i32(nvs_handle, "AltFuelUsed", &temp_int32) == ESP_OK) AlternatorFuelUsed = temp_int32;

  // Runtime Tracking
  if (nvs_get_i32(nvs_handle, "EngineRunTime", &temp_int32) == ESP_OK) EngineRunTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "EngineCycles", &temp_int32) == ESP_OK) EngineCycles = temp_int32;
  if (nvs_get_i32(nvs_handle, "AltOnTime", &temp_int32) == ESP_OK) AlternatorOnTime = temp_int32;

  // Battery State
  if (nvs_get_i32(nvs_handle, "SOC_percent", &temp_int32) == ESP_OK) SOC_percent = temp_int32;
  if (nvs_get_i32(nvs_handle, "CoulombCount", &temp_int32) == ESP_OK) CoulombCount_Ah_scaled = temp_int32;

  // Session Health Stats (✅ restore to prior-session variables)
  if (nvs_get_u32(nvs_handle, "SessionDur", &temp_uint32) == ESP_OK) LastSessionDuration = temp_uint32;
  if (nvs_get_i32(nvs_handle, "MaxLoop", &temp_int32) == ESP_OK) LastSessionMaxLoopTime = temp_int32;
  if (nvs_get_i32(nvs_handle, "MinHeap", &temp_int32) == ESP_OK) lastSessionMinHeap = temp_int32;
  if (nvs_get_u32(nvs_handle, "WifiDur", &temp_uint32) == ESP_OK) lastWifiSessionDuration = temp_uint32;

  // System Health Counters
  if (nvs_get_i32(nvs_handle, "WifiReconnects", &temp_int32) == ESP_OK) wifiReconnectsTotal = temp_int32;
  if (nvs_get_i32(nvs_handle, "PowerCycles", &temp_int32) == ESP_OK) totalPowerCycles = temp_int32;

  // Thermal Stress
  required_size = sizeof(float);
  nvs_get_blob(nvs_handle, "InsulDamage", &CumulativeInsulationDamage, &required_size);
  nvs_get_blob(nvs_handle, "GreaseDamage", &CumulativeGreaseDamage, &required_size);
  nvs_get_blob(nvs_handle, "BrushDamage", &CumulativeBrushDamage, &required_size);

  // Dynamic Learning
  nvs_get_blob(nvs_handle, "ShuntGain", &DynamicShuntGainFactor, &required_size);
  nvs_get_blob(nvs_handle, "AltZero", &DynamicAltCurrentZero, &required_size);
  if (nvs_get_u32(nvs_handle, "LastGainTime", &temp_uint32) == ESP_OK) lastGainCorrectionTime = temp_uint32;
  if (nvs_get_u32(nvs_handle, "LastZeroTime", &temp_uint32) == ESP_OK) lastAutoZeroTime = temp_uint32;
  nvs_get_blob(nvs_handle, "LastZeroTemp", &lastAutoZeroTemp, &required_size);

  nvs_close(nvs_handle);
  //queueConsoleMessage("NVS: Persistent data loaded successfully");
}

void RestoreLastSessionValues() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    Serial.println("There were no prior Session values stored in NVS");
  return;  // 
  }
  uint32_t temp_u32;
  int32_t temp_i32;

  if (nvs_get_u32(nvs_handle, "SessionDur", &temp_u32) == ESP_OK)
    LastSessionDuration = temp_u32;

  if (nvs_get_i32(nvs_handle, "MaxLoop", &temp_i32) == ESP_OK)
    LastSessionMaxLoopTime = temp_i32;

  if (nvs_get_i32(nvs_handle, "MinHeap", &temp_i32) == ESP_OK)
    lastSessionMinHeap = temp_i32;

  if (nvs_get_u32(nvs_handle, "WifiDur", &temp_u32) == ESP_OK)
    lastWifiSessionDuration = temp_u32;

  nvs_close(nvs_handle);
}

void StuffToDoAtSomePoint() {
  //every reset button has a pointless flag and an echo.  I did not delete them for fear of breaking the payload and they hardly cost anything to keep
  //Battery Voltage Source drop down menu- make this text update on page re-load instead of just an echo number
  // Is it possible to power up if the igniton is off?  A bunch of setup functions may fail?
}
