/**
 * AI_SUMMARY: Important functions for AI to parse to help solve the problem. There is another file called 3_nonImportantFunctions that can be shared if necessary.
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

// // // this one is for ESP32 regular
// bool ensureLittleFS() {
//   if (littleFSMounted) {
//     return true;
//   }
//   Serial.println("Initializing LittleFS...");
//   if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {  /// NOTE THE PRESENCE OF spiffs
//     Serial.println("CRITICAL: LittleFS mount failed! Attempting format...");
//     if (!LittleFS.begin(true)) {
//       Serial.println("CRITICAL: LittleFS format failed - filesystem unavailable");
//       littleFSMounted = false;
//       return false;
//     } else {
//       Serial.println("LittleFS formatted and mounted successfully");
//       littleFSMounted = true;
//       return true;
//     }
//   } else {
//     Serial.println("LittleFS mounted successfully");
//   }
//   littleFSMounted = true;
//   return true;
// }


// // this one is for ESP32-S3
bool ensureLittleFS() {
  if (littleFSMounted) {
    return true;
  }
  Serial.println("Initializing LittleFS...");
  if (!LittleFS.begin(true, "/littlefs", 10, "userdata")) {  // max 10 files open at once, probably not an issue, but may be something to keep an eye on later
    Serial.println("CRITICAL: LittleFS mount failed - filesystem unavailable");
    littleFSMounted = false;
    return false;
  }
  Serial.println("LittleFS mounted successfully");
  littleFSMounted = true;
  return true;
}


bool validateWebFile(const char *filename) {
  File file = webFS.open(filename, "r");
  if (!file) {
    Serial.printf("DEBUG: %s - file not found\n", filename);
    return false;
  }

  if (file.size() < 50) {
    Serial.printf("DEBUG: %s - file too small (%d bytes)\n", filename, file.size());
    file.close();
    return false;
  }

  // Read first line
  String start = file.readStringUntil('\n');
  Serial.printf("DEBUG: %s - first line: '%s'\n", filename, start.c_str());

  // Read last 30 bytes
  file.seek(-30, SeekEnd);
  String end = file.readString();
  Serial.printf("DEBUG: %s - last 30 bytes: '%s'\n", filename, end.c_str());
  file.close();

  bool hasStart = start.indexOf("XREG_START") >= 0;
  bool hasEnd = end.indexOf("XREG_END") >= 0;

  Serial.printf("DEBUG: %s - start marker: %s, end marker: %s\n",
                filename, hasStart ? "FOUND" : "MISSING", hasEnd ? "FOUND" : "MISSING");

  return hasStart && hasEnd;
}

bool validateWebFilesystem() {
  return validateWebFile("/index.html") && validateWebFile("/styles.css") && validateWebFile("/script.js") && validateWebFile("/uPlot.min.css") && validateWebFile("/uPlot.iife.min.js");
}
bool ensureWebFS() {
  static bool webMounted = false;
  if (webMounted) {
    return true;
  }

  // Check which partition we're running from
  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

  bool runningFromFactory = (running_partition == factory_partition);

  if (runningFromFactory) {
    // Factory firmware uses factory_fs
    if (webFS.begin(true, "/web", 10, "factory_fs")) {
      if (validateWebFilesystem()) {
        usingFactoryWebFiles = true;
        Serial.println("Web filesystem: factory_fs validated and mounted");
        webMounted = true;
        return true;
      }
    }
    Serial.println("ERROR: Factory web files failed validation");
    return false;
  } else {
    // OTA firmware uses prod_fs with factory_fs fallback
    if (webFS.begin(true, "/web", 10, "prod_fs")) {
      if (validateWebFilesystem()) {
        usingFactoryWebFiles = false;
        Serial.println("Web filesystem: prod_fs validated and mounted");
        webMounted = true;
        return true;
      } else {
        Serial.println("prod_fs mounted but validation failed - forcing factory rollback");
        webFS.end();
        queueConsoleMessage("CRITICAL: prod_fs corrupted, forcing factory rollback");
        esp_ota_mark_app_invalid_rollback_and_reboot();
        // System reboots here
      }
    }

    // If we reach here, prod_fs couldn't mount or failed validation
    // Try fallback to factory_fs (corrupted prod_fs scenario)
    if (webFS.begin(true, "/web", 10, "factory_fs")) {
      if (validateWebFilesystem()) {
        usingFactoryWebFiles = true;
        Serial.println("Web filesystem: factory_fs validated and mounted (fallback from prod_fs)");
        webMounted = true;
        return true;
      } else {
        Serial.println("ERROR: Both web filesystems are corrupted");
        webFS.end();
        return false;
      }
    }

    return false;
  }
}
void switchToFactoryWebFiles() {
  webFS.end();
  usingFactoryWebFiles = true;
  if (!webFS.begin(true, "/web", 10, "factory_fs")) {
    Serial.println("ERROR: Failed to mount factory web files");
    events.send("CRITICAL: Failed to mount factory web files", "console", millis());
  } else {
    Serial.println("Switched to factory web files");
    events.send("Switched to factory web files for recovery", "console", millis());
  }
}

void performFactoryReset() {
  Serial.println("FACTORY RESET: Switching to factory firmware and web files");
  events.send("FACTORY RESET: Switching to factory firmware and web files", "console", millis());

  const esp_partition_t *factory_partition =
    esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

  if (factory_partition) {
    esp_ota_set_boot_partition(factory_partition);
    switchToFactoryWebFiles();
    Serial.println("Factory reset complete - restarting in 3 seconds");
    events.send("Factory reset complete - restarting in 3 seconds", "console", millis());
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("ERROR: Factory partition not found");
    events.send("ERROR: Factory partition not found", "console", millis());
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

void loadAPCredentials(bool forceDefaults = false) {
  if (forceDefaults) {
    esp32_ap_ssid = "ALTERNATOR_WIFI";
    esp32_ap_password = "alternator123";
    Serial.println("Using default AP credentials for password recovery or first boot");
    return;
  }

  // Load custom credentials from files
  if (LittleFS.exists(AP_SSID_FILE)) {
    esp32_ap_ssid = readFile(LittleFS, AP_SSID_FILE);
    esp32_ap_ssid.trim();
    if (esp32_ap_ssid.length() == 0) {
      esp32_ap_ssid = "ALTERNATOR_WIFI";
    }
  } else {
    esp32_ap_ssid = "ALTERNATOR_WIFI";
  }

  if (LittleFS.exists(AP_PASSWORD_FILE)) {
    esp32_ap_password = readFile(LittleFS, AP_PASSWORD_FILE);
    esp32_ap_password.trim();
    if (esp32_ap_password.length() == 0) {
      esp32_ap_password = "alternator123";
    }
  } else {
    esp32_ap_password = "alternator123";
  }

  Serial.println("Loaded AP credentials: " + esp32_ap_ssid + " / " + esp32_ap_password);
}


void setupWiFi() {  // Function to set up WiFi with new GPIO-based mode selection
  Serial.println("\n=== WiFi Setup Starting ===");

  // Configuration Options Summary - GPIO Boot Mode Selection
  // GPIO41 - Boot from Factory Partition (emergency recovery from bad OTA)
  // GPIO45 - Force Configuration Mode (WiFi setup/password recovery - alternator disabled for safety)
  // GPIO46 - Select AP vs Client Mode:
  //   - LOW  = AP Mode (creates own WiFi network, full alternator operation, emergency access without credentials)
  //   - HIGH = Client Mode (connects to ship's WiFi network, full alternator operation)
  //
  // EMERGENCY ACCESS WITHOUT CREDENTIALS:
  //   Hold GPIO46 LOW during boot → AP mode → Full alternator functionality
  //   Connect to "ALTERNATOR_WIFI" (or custom SSID) with password "alternator123" (or custom)
  //   Access full interface at http://192.168.4.1
  //
  // WHY CONFIG MODE DISABLES ALTERNATOR:
  //   CONFIG mode (GPIO45 LOW or no credentials) intentionally prevents alternator operation
  //   This is a safety feature - prevents running with unconfigured/unknown settings
  //   Use GPIO46 LOW (AP mode) for emergency operation without reconfiguring

  // GPIO45 = Configuration Mode Override (always checked first)
  pinMode(45, INPUT_PULLUP);
  bool forceConfigMode = (digitalRead(45) == LOW);

  if (forceConfigMode) {
    Serial.println("=== GPIO45 LOW: FORCED CONFIGURATION MODE ===");
    Serial.println("=== ALTERNATOR DISABLED FOR SAFETY - Use GPIO46 LOW for emergency operation ===");
    loadAPCredentials(true);  // Force defaults for password recovery
    setupAccessPoint();
    setupWiFiConfigServer();  // Always serve config interface when GPIO45 is low.  This can be used to reset lost passwords
    currentMode = MODE_CONFIG;
    return;  // Exit setupWiFi() - alternator will not run
  }

  // GPIO46 = Operational Mode Selection (checked early to allow emergency AP access on first boot)
  // This check happens before isFirstBoot to ensure AP mode works even on first boot
  // Rationale: If user has no WiFi or WiFi router is down, they can still run the regulator in AP mode
  // This allows emergency operation: GPIO41 low = factory firmware, GPIO41 high = OTA firmware (if valid OTA exists, else factory)
  // GPIO46 low = AP mode regardless of any credentials of any kind
  // Settings persist in userdata partition; AP credentials default to ALTERNATOR_WIFI/alternator123

  pinMode(46, INPUT_PULLUP);
  bool requestAPMode = (digitalRead(46) == LOW);

  if (requestAPMode) {
    Serial.println("=== GPIO46 LOW: OPERATIONAL AP MODE ===");
    loadAPCredentials(false);  // Load custom credentials for normal AP operation
    setupAccessPoint();
    currentMode = MODE_AP;

    // Serve full alternator interface (operational AP mode)
    if (webFS.exists("/index.html")) {
      Serial.println("Serving full alternator interface in AP mode");
      setupServer();
    } else {
      Serial.println("No index.html found - serving basic landing page");
      setupCaptivePortalLanding();
    }
    return;
  }

  // Check if this is truly first boot (no configuration has ever been done)
  bool isFirstBoot = !LittleFS.exists("/first_config_done.txt");

  if (isFirstBoot) {
    Serial.println("=== FIRST BOOT DETECTED: FORCING CONFIGURATION MODE ===");
    Serial.println("User must configure credentials before accessing interface");
    loadAPCredentials(true);  // Use defaults so user can learn them
    setupAccessPoint();
    setupWiFiConfigServer();
    currentMode = MODE_CONFIG;
    return;
  }

  // Check if the device has ever been configured with WiFi credentials before
  // Test if the WIFI_SSID_FILE exists in LittleFS storage
  // This file should contain the saved WiFi SSID from a previous configuration.
  // If it doesn't exist, then the device has never been configured and should enter configuration mode.
  bool hasClientConfig = LittleFS.exists(WIFI_SSID_FILE);
  // Initialize variables to hold the saved SSID and password.
  // These will remain empty unless valid credentials are loaded from storage later.
  String saved_ssid = "";      // Holds the WiFi network name (SSID) read from file
  String saved_password = "";  // Holds the WiFi password read from file

  if (hasClientConfig) {
    saved_ssid = readFile(LittleFS, WIFI_SSID_FILE);
    saved_password = readFile(LittleFS, WIFI_PASS_FILE);
    saved_ssid.trim();
    saved_password.trim();
  }

  // GPIO46 HIGH = Client Mode Requested
  Serial.println("=== GPIO46 HIGH: CLIENT MODE REQUESTED ===");

  // If no saved credentials, enter config mode
  if (!hasClientConfig || saved_ssid.length() == 0) {
    Serial.println("=== NO CLIENT CREDENTIALS: ENTERING CONFIGURATION MODE ===");
    Serial.println("Reason: " + String(!hasClientConfig ? "No saved credentials file" : "Empty SSID"));
    loadAPCredentials(false);  // Use custom AP credentials for config mode
    setupAccessPoint();
    setupWiFiConfigServer();  // Serve WiFi configuration interface
    currentMode = MODE_CONFIG;
    return;
  }

  // Normal operation - load custom AP credentials in case we need them later
  loadAPCredentials(false);

  // Attempt client connection with timeout (safe for 15s watchdog)
  if (connectToWiFi(saved_ssid.c_str(), saved_password.c_str(), 10000)) {
    currentMode = MODE_CLIENT;
    Serial.println("=== CLIENT MODE SUCCESS ===");
    Serial.println("WiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    setupServer();
  } else {
    Serial.println("=== CLIENT MODE FAILED ===");
    Serial.println("WiFi connection failed - will retry periodically (no AP fallback)");
    currentMode = MODE_CLIENT;  // Keep trying client mode
    // No automatic AP fallback - let intelligent reconnection logic handle it
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

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (strlen(password) > 0) {
    WiFi.begin(ssid, password);
  } else {
    WiFi.begin(ssid);  // Open network
  }

  unsigned long startTime = millis();
  int attempts = 0;
  const int maxAttempts = timeout / 500;  // Check every 500ms

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    attempts++;

    // Print progress every 2 seconds
    if (attempts % 4 == 0) {
      Serial.printf("WiFi Status: %d, attempt %d/%d\n", WiFi.status(), attempts, maxAttempts);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connection successful!");                         // PRESERVES: Your success message
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());  // PRESERVES: Your IP logging
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());               // PRESERVES: Your signal logging
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
  delay(500);
  if (apStarted) {
    Serial.println("=== ACCESS POINT STARTED SUCCESSFULLY ===");
    Serial.print("AP SSID: ");
    Serial.println(esp32_ap_ssid);
    Serial.print("AP Password: ");
    Serial.println(esp32_ap_password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Start DNS server for captive portal
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("AP IP before DNS start: " + apIP.toString());
    bool dnsStarted = dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server start result: " + String(dnsStarted));
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
  Serial.println("=== IMPORTANT: Alternator is DISABLED in CONFIG mode ===");
  Serial.println("=== For emergency operation: Power down, hold GPIO46 LOW, power up ===");
  Serial.println("=== This enters AP mode with full alternator functionality ===\n");
  // Main config page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("=== CONFIG PAGE REQUEST ===");
    Serial.println("Client IP: " + request->client()->remoteIP().toString());
    Serial.println("User-Agent: " + request->header("User-Agent"));
    Serial.println("Serving WiFi configuration page");
    request->send_P(200, "text/html", WIFI_CONFIG_HTML);
  });


  // Enhanced WiFi configuration handler (replace existing /wifi handler)
  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("\n=== WIFI CONFIG POST RECEIVED ===");

    String ssid = "";
    String password = "";
    String ap_password = "";
    String hotspot_ssid = "";

    // Get parameters
    if (request->hasParam("ap_password", true)) {
      ap_password = request->getParam("ap_password", true)->value();
      ap_password.trim();
    }

    if (request->hasParam("hotspot_ssid", true)) {
      hotspot_ssid = request->getParam("hotspot_ssid", true)->value();
      hotspot_ssid.trim();
    }

    if (request->hasParam("ssid", true)) {
      ssid = request->getParam("ssid", true)->value();
      ssid.trim();
    }

    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
      password.trim();
    }

    // Validate AP password - BLOCK if invalid
    if (ap_password.length() > 0 && ap_password.length() < 8) {
      String errorPage = "<!DOCTYPE html><html><head><title>Configuration Error</title></head><body>";
      errorPage += "<h1>Configuration Error</h1>";
      errorPage += "<p><strong>AP password must be at least 8 characters or left blank for default.</strong></p>";
      errorPage += "<p><a href='javascript:history.back()'>Go Back and Try Again</a></p>";
      errorPage += "</body></html>";
      request->send(400, "text/html", errorPage);
      return;
    }
    // Set default only if user left it completely blank
    if (ap_password.length() == 0) {
      ap_password = "alternator123";
    }

    // Save configuration
    Serial.println("=== SAVING CONFIGURATION ===");
    Serial.println("SSID: '" + ssid + "' (length: " + String(ssid.length()) + ")");
    Serial.println("Password: '" + password + "' (length: " + String(password.length()) + ")");

    // Check LittleFS status before attempting writes
    Serial.println("LittleFS mounted: " + String(littleFSMounted ? "YES" : "NO"));
    if (!littleFSMounted) {
      Serial.println("Attempting to mount LittleFS...");
      if (!ensureLittleFS()) {
        Serial.println("CRITICAL: LittleFS mount failed");
        request->send(500, "text/plain", "Filesystem error - cannot save configuration");
        return;
      }
    }

    // Check free space
    Serial.println("LittleFS total bytes: " + String(LittleFS.totalBytes()));
    Serial.println("LittleFS used bytes: " + String(LittleFS.usedBytes()));
    Serial.println("LittleFS free bytes: " + String(LittleFS.totalBytes() - LittleFS.usedBytes()));

    // Update AP credentials
    writeFile(LittleFS, AP_PASSWORD_FILE, ap_password.c_str());
    esp32_ap_password = ap_password;  // Update memory

    if (hotspot_ssid.length() > 0) {
      writeFile(LittleFS, AP_SSID_FILE, hotspot_ssid.c_str());
      esp32_ap_ssid = hotspot_ssid;
    } else {
      if (LittleFS.exists(AP_SSID_FILE)) {
        LittleFS.remove(AP_SSID_FILE);
      }
      esp32_ap_ssid = "ALTERNATOR_WIFI";
    }

    // Save client credentials (even if empty - user might set them later)
    writeFile(LittleFS, "/ssid.txt", ssid.c_str());
    writeFile(LittleFS, "/pass.txt", password.c_str());

    // Verify what was actually saved
    String savedSSID = readFile(LittleFS, "/ssid.txt");
    String savedPass = readFile(LittleFS, "/pass.txt");
    Serial.println("Verification - SSID: '" + savedSSID + "'");
    Serial.println("Verification - Password: '" + savedPass + "'");
    delay(1000);  // Give time for serial output

    // Send success response
    request->send(200, "text/plain", "Configuration saved! Device will restart in 3 seconds.");

    Serial.println("=== CONFIGURATION SAVED - RESTARTING ===");
    // Mark that first configuration has been completed
    writeFile(LittleFS, "/first_config_done.txt", "1");
    delay(3000);  // Give time for response to be sent
    ESP.restart();
  });

  // Enhanced 404 handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    Serial.println("=== WIFI CONFIG SERVER REQUEST DEBUG ===");
    Serial.print("Request for: ");
    Serial.println(path);
    Serial.println("Client IP: " + request->client()->remoteIP().toString());
    Serial.println("Method: " + String(request->method() == HTTP_GET ? "GET" : "POST"));

    if (webFS.exists(path)) {
      String contentType = "text/html";
      if (path.endsWith(".css")) contentType = "text/css";
      else if (path.endsWith(".js")) contentType = "application/javascript";
      else if (path.endsWith(".json")) contentType = "application/json";
      else if (path.endsWith(".png")) contentType = "image/png";
      else if (path.endsWith(".jpg")) contentType = "image/jpeg";

      Serial.println("File found in webFS, serving with content-type: " + contentType);
      request->send(webFS, path, contentType);
    } else {
      Serial.print("File not found in LittleFS: ");
      Serial.println(path);
      Serial.println("Redirecting to captive portal: " + WiFi.softAPIP().toString());
      // Only redirect in AP mode
      if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        request->redirect("http://" + WiFi.softAPIP().toString());
      } else {
        request->send(404, "text/plain", "Not Found");
      }
    }
  });

  server.begin();
  Serial.println("=== WIFI CONFIG SERVER STARTED ===");
  Serial.println("=== WIFI CONFIG SERVER SETUP COMPLETE ===");
}

void setupServer() {
  Serial.println("=== SETTING UP MAIN SERVER ===");

  ensureWebFS();  // Mount appropriate web partition (prod_fs with factory_fs fallback)

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
      "/TemperatureLimitF.txt",
      "/ManualDutyTarget.txt",
      "/BulkVoltage.txt",
      "/TargetAmps.txt",
      "/SwitchingFrequency.txt",
      "/FloatVoltage.txt",
      "/dutyStep.txt",
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
      "/scheduledRestart.txt",
      // Learning Toggle Switches
      "/LearningMode.txt",
      "/LearningPaused.txt",
      "/IgnoreLearningDuringPenalty.txt",
      "/EXTRA.txt",
      "/LearningDryRunMode.txt",
      "/AutoSaveLearningTable.txt",
      "/LearningUpwardEnabled.txt",
      "/LearningDownwardEnabled.txt",
      "/EnableNeighborLearning.txt",
      "/EnableAmbientCorrection.txt",
      "/LearningFailsafeMode.txt",
      "/ShowLearningDebugMessages.txt",
      "/LogAllLearningEvents.txt",

      // Learning Numerical Settings
      "/AlternatorNominalAmps.txt",
      "/LearningUpStep.txt",
      "/LearningDownStep.txt",
      "/AmbientTempCorrectionFactor.txt",
      "/AmbientTempBaseline.txt",
      "/MinLearningInterval.txt",
      "/SafeOperationThreshold.txt",
      "/PidKp.txt",
      "/PidKi.txt",
      "/PidKd.txt",
      "/PidSampleTime.txt",
      "/MaxTableValue.txt",
      "/MinTableValue.txt",
      "/MaxPenaltyPercent.txt",
      "/MaxPenaltyDuration.txt",
      "/NeighborLearningFactor.txt",
      "/LearningRPMSpacing.txt",
      "/LearningMemoryDuration.txt",
      "/LearningTableSaveInterval.txt",

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

  // Simple ping handler for disconnect tracking
  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
    lastPingTime = millis();
    request->send(200, "text/plain", "ok");
  });


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(webFS, "/index.html", "text/html");
  });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    bool foundParameter = false;
    String inputMessage;
    if (request->hasParam("TemperatureLimitF")) {
      foundParameter = true;
      inputMessage = request->getParam("TemperatureLimitF")->value();
      writeFile(LittleFS, "/TemperatureLimitF.txt", inputMessage.c_str());
      TemperatureLimitF = inputMessage.toInt();
    } else if (request->hasParam("ManualDutyTarget")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualDutyTarget")->value();
      writeFile(LittleFS, "/ManualDutyTarget.txt", inputMessage.c_str());
      ManualDutyTarget = inputMessage.toInt();
    } else if (request->hasParam("BulkVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("BulkVoltage")->value();
      writeFile(LittleFS, "/BulkVoltage.txt", inputMessage.c_str());
      BulkVoltage = inputMessage.toFloat();
      updateINA228OvervoltageThreshold();  // important!  update the hardware overvoltage limit provided by INA228
    } else if (request->hasParam("TargetAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("TargetAmps")->value();
      writeFile(LittleFS, "/TargetAmps.txt", inputMessage.c_str());
      TargetAmps = inputMessage.toInt();
    } else if (request->hasParam("SwitchingFrequency")) {
      foundParameter = true;
      inputMessage = request->getParam("SwitchingFrequency")->value();
      int requestedFreq = inputMessage.toInt();
      writeFile(LittleFS, "/SwitchingFrequency.txt", String(requestedFreq).c_str());
      SwitchingFrequency = requestedFreq;  // Update the target
      queueConsoleMessage("Frequency target set to " + String(SwitchingFrequency) + "Hz");
    } else if (request->hasParam("FloatVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("FloatVoltage")->value();
      writeFile(LittleFS, "/FloatVoltage.txt", inputMessage.c_str());
      FloatVoltage = inputMessage.toFloat();
    } else if (request->hasParam("dutyStep")) {
      foundParameter = true;
      inputMessage = request->getParam("dutyStep")->value();
      writeFile(LittleFS, "/dutyStep.txt", inputMessage.c_str());
      dutyStep = inputMessage.toFloat();
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
      if (pidInitialized) {
        currentPID.SetOutputLimits(MinDuty, MaxDuty);
      }
      queueConsoleMessage("Max Duty updated to: " + String(MaxDuty) + "%");
    } else if (request->hasParam("MinDuty")) {
      foundParameter = true;
      inputMessage = request->getParam("MinDuty")->value();
      writeFile(LittleFS, "/MinDuty.txt", inputMessage.c_str());
      MinDuty = inputMessage.toInt();
      if (pidInitialized) {
        currentPID.SetOutputLimits(MinDuty, MaxDuty);
      }
      queueConsoleMessage("Min Duty updated to: " + String(MinDuty) + "%");
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
    } else if (request->hasParam("AlarmLatchEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("AlarmLatchEnabled")->value();
      writeFile(LittleFS, "/AlarmLatchEnabled.txt", inputMessage.c_str());
      AlarmLatchEnabled = inputMessage.toInt();
    } else if (request->hasParam("AlarmTest")) {
      foundParameter = true;
      AlarmTest = 1;  // Set the flag - don't save to file as it's momentary
      queueConsoleMessage("ALARM TEST: Initiated from web interface");
      inputMessage = "1";  // Return confirmation
    } else if (request->hasParam("ResetAlarmLatch")) {
      foundParameter = true;
      ResetAlarmLatch = 1;  // Set the flag - don't save to file as it's momentary
      queueConsoleMessage("ALARM LATCH: Reset requested from web interface NO FUNCTION!");
      inputMessage = "1";  // Return confirmation
    }
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
      IBVMax = 0;
      writeFile(LittleFS, "/IBVMax.txt", "0");
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
    } else if (request->hasParam("ManualLifePercentage")) {
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
    } else if (request->hasParam("webgaugesinterval")) {
      foundParameter = true;
      inputMessage = request->getParam("webgaugesinterval")->value();
      int newInterval = inputMessage.toInt();
      newInterval = constrain(newInterval, 1, 10000000);
      writeFile(LittleFS, "/webgaugesinterval.txt", String(newInterval).c_str());
      webgaugesinterval = newInterval;
      queueConsoleMessage("Update interval set to: " + String(newInterval) + "ms");
    } else if (request->hasParam("BatteryCurrentSource")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCurrentSource")->value();
      writeFile(LittleFS, "/BatteryCurrentSource.txt", inputMessage.c_str());
      BatteryCurrentSource = inputMessage.toInt();
      queueConsoleMessage("Battery current source changed to: " + String(BatteryCurrentSource));  // Debug line
    } else if (request->hasParam("totalPowerCycles")) {
      foundParameter = true;
      inputMessage = request->getParam("totalPowerCycles")->value();
      writeFile(LittleFS, "/totalPowerCycles.txt", inputMessage.c_str());
      totalPowerCycles = inputMessage.toInt();
    } else if (request->hasParam("timeAxisModeChanging")) {
      foundParameter = true;
      inputMessage = request->getParam("timeAxisModeChanging")->value();
      writeFile(LittleFS, "/timeAxisModeChanging.txt", inputMessage.c_str());
      timeAxisModeChanging = inputMessage.toInt();
      queueConsoleMessage("Time axis mode changed to: " + String(timeAxisModeChanging ? "UNIX timestamps" : "relative time"));
    } else if (request->hasParam("plotTimeWindow")) {
      foundParameter = true;
      inputMessage = request->getParam("plotTimeWindow")->value();
      writeFile(LittleFS, "/plotTimeWindow.txt", inputMessage.c_str());
      plotTimeWindow = inputMessage.toInt();
    }

    else if (request->hasParam("weatherModeEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("weatherModeEnabled")->value();
      writeFile(LittleFS, "/weatherModeEnabled.txt", inputMessage.c_str());
      weatherModeEnabled = inputMessage.toInt();
      queueConsoleMessage("Weather Mode " + String(weatherModeEnabled ? "enabled" : "disabled"));
    }
    else if (request->hasParam("UVThresholdHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("UVThresholdHigh")->value();
      writeFile(LittleFS, "/UVThresholdHigh.txt", inputMessage.c_str());
      UVThresholdHigh = inputMessage.toFloat();
    } else if (request->hasParam("SolarWatts")) {
      foundParameter = true;
      inputMessage = request->getParam("SolarWatts")->value();
      writeFile(LittleFS, "/SolarWatts.txt", inputMessage.c_str());
      SolarWatts = inputMessage.toFloat();
    } else if (request->hasParam("performanceRatio")) {
      foundParameter = true;
      inputMessage = request->getParam("performanceRatio")->value();
      writeFile(LittleFS, "/performanceRatio.txt", inputMessage.c_str());
      performanceRatio = inputMessage.toFloat();
    } else if (request->hasParam("TriggerWeatherUpdate")) {
      foundParameter = true;
      queueConsoleMessage("Weather: Manual update triggered");
      fetchWeatherData();  // Direct call- use API to get weather
      //inputMessage = "1"; // could use this sort of echo back to JS if set up, which it currently isn't
    }


    else if (request->hasParam("ResetLearningTable")) {
      foundParameter = true;
      ResetLearningTable = 1;  // Set flag - action handled in AdjustFieldLearnMode()
      queueConsoleMessage("Learning Table: Reset requested from web interface");
      inputMessage = "1";
    } else if (request->hasParam("ClearOverheatHistory")) {
      foundParameter = true;
      ClearOverheatHistory = 1;  // Set flag
      queueConsoleMessage("Overheat History: Clear requested from web interface");
      inputMessage = "1";
    }


    else if (request->hasParam("LearningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningMode")->value();
      writeFile(LittleFS, "/LearningMode.txt", inputMessage.c_str());
      LearningMode = inputMessage.toInt();
    } else if (request->hasParam("LearningPaused")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningPaused")->value();
      writeFile(LittleFS, "/LearningPaused.txt", inputMessage.c_str());
      LearningPaused = inputMessage.toInt();
    } else if (request->hasParam("IgnoreLearningDuringPenalty")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreLearningDuringPenalty")->value();
      writeFile(LittleFS, "/IgnoreLearningDuringPenalty.txt", inputMessage.c_str());
      IgnoreLearningDuringPenalty = inputMessage.toInt();
    } else if (request->hasParam("EXTRA")) {
      foundParameter = true;
      inputMessage = request->getParam("EXTRA")->value();
      writeFile(LittleFS, "/EXTRA.txt", inputMessage.c_str());
      EXTRA = inputMessage.toInt();
    } else if (request->hasParam("LearningDryRunMode")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDryRunMode")->value();
      writeFile(LittleFS, "/LearningDryRunMode.txt", inputMessage.c_str());
      LearningDryRunMode = inputMessage.toInt();
    } else if (request->hasParam("AutoSaveLearningTable")) {
      foundParameter = true;
      inputMessage = request->getParam("AutoSaveLearningTable")->value();
      writeFile(LittleFS, "/AutoSaveLearningTable.txt", inputMessage.c_str());
      AutoSaveLearningTable = inputMessage.toInt();
    } else if (request->hasParam("LearningUpwardEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningUpwardEnabled")->value();
      writeFile(LittleFS, "/LearningUpwardEnabled.txt", inputMessage.c_str());
      LearningUpwardEnabled = inputMessage.toInt();
    } else if (request->hasParam("LearningDownwardEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDownwardEnabled")->value();
      writeFile(LittleFS, "/LearningDownwardEnabled.txt", inputMessage.c_str());
      LearningDownwardEnabled = inputMessage.toInt();
    } else if (request->hasParam("EnableNeighborLearning")) {
      foundParameter = true;
      inputMessage = request->getParam("EnableNeighborLearning")->value();
      writeFile(LittleFS, "/EnableNeighborLearning.txt", inputMessage.c_str());
      EnableNeighborLearning = inputMessage.toInt();
    } else if (request->hasParam("EnableAmbientCorrection")) {
      foundParameter = true;
      inputMessage = request->getParam("EnableAmbientCorrection")->value();
      writeFile(LittleFS, "/EnableAmbientCorrection.txt", inputMessage.c_str());
      EnableAmbientCorrection = inputMessage.toInt();
    } else if (request->hasParam("LearningFailsafeMode")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningFailsafeMode")->value();
      writeFile(LittleFS, "/LearningFailsafeMode.txt", inputMessage.c_str());
      LearningFailsafeMode = inputMessage.toInt();
    } else if (request->hasParam("ShowLearningDebugMessages")) {
      foundParameter = true;
      inputMessage = request->getParam("ShowLearningDebugMessages")->value();
      writeFile(LittleFS, "/ShowLearningDebugMessages.txt", inputMessage.c_str());
      ShowLearningDebugMessages = inputMessage.toInt();
    } else if (request->hasParam("LogAllLearningEvents")) {
      foundParameter = true;
      inputMessage = request->getParam("LogAllLearningEvents")->value();
      writeFile(LittleFS, "/LogAllLearningEvents.txt", inputMessage.c_str());
      LogAllLearningEvents = inputMessage.toInt();
    }



    // RPM breakpoint and current table handlers (consolidated)
    bool learningTableUpdated = false;

    for (int i = 0; i < 10; i++) {
      String rpmParam = "rpmTableRPMPoints" + String(i);
      if (request->hasParam(rpmParam.c_str())) {
        foundParameter = true;
        inputMessage = request->getParam(rpmParam.c_str())->value();
        rpmTableRPMPoints[i] = inputMessage.toInt();
        learningTableUpdated = true;
      }

      String currentParam = "rpmCurrentTable" + String(i);
      if (request->hasParam(currentParam.c_str())) {
        foundParameter = true;
        inputMessage = request->getParam(currentParam.c_str())->value();
        rpmCurrentTable[i] = inputMessage.toFloat();
        learningTableUpdated = true;
      }
    }

    // Save once after processing all parameters
    if (learningTableUpdated && AutoSaveLearningTable) {
      saveLearningTableToNVS();
      queueConsoleMessage("Learning table updated from web interface");
    }


    else if (request->hasParam("AlternatorNominalAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorNominalAmps")->value();
      writeFile(LittleFS, "/AlternatorNominalAmps.txt", inputMessage.c_str());
      AlternatorNominalAmps = inputMessage.toInt();
    } else if (request->hasParam("LearningUpStep")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningUpStep")->value();
      writeFile(LittleFS, "/LearningUpStep.txt", inputMessage.c_str());
      LearningUpStep = inputMessage.toFloat();
    } else if (request->hasParam("LearningDownStep")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDownStep")->value();
      writeFile(LittleFS, "/LearningDownStep.txt", inputMessage.c_str());
      LearningDownStep = inputMessage.toFloat();
    } else if (request->hasParam("AmbientTempCorrectionFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("AmbientTempCorrectionFactor")->value();
      writeFile(LittleFS, "/AmbientTempCorrectionFactor.txt", inputMessage.c_str());
      AmbientTempCorrectionFactor = inputMessage.toFloat();
    } else if (request->hasParam("AmbientTempBaseline")) {
      foundParameter = true;
      inputMessage = request->getParam("AmbientTempBaseline")->value();
      writeFile(LittleFS, "/AmbientTempBaseline.txt", inputMessage.c_str());
      AmbientTempBaseline = inputMessage.toFloat();
    } else if (request->hasParam("MinLearningInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("MinLearningInterval")->value();
      int temp = inputMessage.toInt() * 1000;  // from seconds (entry into html) to ms (for this whole code)
      writeFile(LittleFS, "/MinLearningInterval.txt", String(temp).c_str());
      MinLearningInterval = temp;
    } else if (request->hasParam("SafeOperationThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("SafeOperationThreshold")->value();
      int temp = inputMessage.toInt() * 1000;  // from seconds (entry into html) to ms (for this whole code)
      writeFile(LittleFS, "/SafeOperationThreshold.txt", String(temp).c_str());
      SafeOperationThreshold = temp;
    } else if (request->hasParam("PidKp")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKp")->value();
      writeFile(LittleFS, "/PidKp.txt", inputMessage.c_str());
      PidKp = inputMessage.toFloat();
      if (pidInitialized) {
        currentPID.SetTunings(PidKp, PidKi, PidKd);
      }
      queueConsoleMessage("PID Kp updated to: " + String(PidKp, 6));
    } else if (request->hasParam("PidKi")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKi")->value();
      writeFile(LittleFS, "/PidKi.txt", inputMessage.c_str());
      PidKi = inputMessage.toFloat();
      if (pidInitialized) {
        currentPID.SetTunings(PidKp, PidKi, PidKd);
      }
      queueConsoleMessage("PID Ki updated to: " + String(PidKi, 6));
    } else if (request->hasParam("PidKd")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKd")->value();
      writeFile(LittleFS, "/PidKd.txt", inputMessage.c_str());
      PidKd = inputMessage.toFloat();
      if (pidInitialized) {
        currentPID.SetTunings(PidKp, PidKi, PidKd);
      }
      queueConsoleMessage("PID Kd updated to: " + String(PidKd, 6));
    } else if (request->hasParam("PidSampleTime")) {
      foundParameter = true;
      inputMessage = request->getParam("PidSampleTime")->value();
      writeFile(LittleFS, "/PidSampleTime.txt", inputMessage.c_str());
      PidSampleTime = inputMessage.toInt();
      if (pidInitialized) {
        currentPID.SetSampleTime(PidSampleTime);
      }
      queueConsoleMessage("PID Sample Time updated to: " + String(PidSampleTime) + "ms");
    }

    else if (request->hasParam("LearningSettlingPeriod")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningSettlingPeriod")->value();
      int temp = inputMessage.toInt() * 1000;  // Convert seconds to ms
      writeFile(LittleFS, "/LearningSettlingPeriod.txt", String(temp).c_str());
      LearningSettlingPeriod = temp;
    } else if (request->hasParam("LearningRPMChangeThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningRPMChangeThreshold")->value();
      writeFile(LittleFS, "/LearningRPMChangeThreshold.txt", inputMessage.c_str());
      LearningRPMChangeThreshold = inputMessage.toInt();
    } else if (request->hasParam("LearningTempHysteresis")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningTempHysteresis")->value();
      writeFile(LittleFS, "/LearningTempHysteresis.txt", inputMessage.c_str());
      LearningTempHysteresis = inputMessage.toInt();
    }

    else if (request->hasParam("MaxTableValue")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxTableValue")->value();
      writeFile(LittleFS, "/MaxTableValue.txt", inputMessage.c_str());
      MaxTableValue = inputMessage.toFloat();
    } else if (request->hasParam("MinTableValue")) {
      foundParameter = true;
      inputMessage = request->getParam("MinTableValue")->value();
      writeFile(LittleFS, "/MinTableValue.txt", inputMessage.c_str());
      MinTableValue = inputMessage.toFloat();
    } else if (request->hasParam("MaxPenaltyPercent")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxPenaltyPercent")->value();
      writeFile(LittleFS, "/MaxPenaltyPercent.txt", inputMessage.c_str());
      MaxPenaltyPercent = inputMessage.toFloat();
    } else if (request->hasParam("MaxPenaltyDuration")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxPenaltyDuration")->value();
      int temp = inputMessage.toInt() * 1000;
      writeFile(LittleFS, "/MaxPenaltyDuration.txt", String(temp).c_str());
      MaxPenaltyDuration = temp;
    } else if (request->hasParam("NeighborLearningFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("NeighborLearningFactor")->value();
      writeFile(LittleFS, "/NeighborLearningFactor.txt", inputMessage.c_str());
      NeighborLearningFactor = inputMessage.toFloat();
    } else if (request->hasParam("LearningRPMSpacing")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningRPMSpacing")->value();
      writeFile(LittleFS, "/LearningRPMSpacing.txt", inputMessage.c_str());
      LearningRPMSpacing = inputMessage.toInt();
    } else if (request->hasParam("LearningMemoryDuration")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningMemoryDuration")->value();
      writeFile(LittleFS, "/LearningMemoryDuration.txt", inputMessage.c_str());
      LearningMemoryDuration = inputMessage.toInt();
    } else if (request->hasParam("LearningTableSaveInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningTableSaveInterval")->value();
      int temp = inputMessage.toInt() * 1000;
      writeFile(LittleFS, "/LearningTableSaveInterval.txt", String(temp).c_str());
      LearningTableSaveInterval = temp;
    }

    else if (request->hasParam("UpdateToVersion")) {
      foundParameter = true;
      String selectedVersionStr = request->getParam("UpdateToVersion")->value();

      // Convert to char array to avoid String issues
      char selectedVersion[32];
      strncpy(selectedVersion, selectedVersionStr.c_str(), 31);
      selectedVersion[31] = '\0';

      // Debug info about timing and system state
      unsigned long timeSinceLastNVSSave = millis() - lastNVSSaveTime;
      Serial.printf("UPDATE DEBUG: Time since last NVS save: %lu ms\n", timeSinceLastNVSSave);
      Serial.printf("UPDATE DEBUG: Free heap: %u bytes\n", ESP.getFreeHeap());

      // Add memory diagnostics
      size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
      Serial.printf("UPDATE DEBUG: Free internal: %u, Largest block: %u\n", freeInternal, largestBlock);
      // Check which partition we're currently running on
      const esp_partition_t *running_partition = esp_ota_get_running_partition();
      const esp_partition_t *factory_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

      if (running_partition == factory_partition) {
        // Case: Running on factory partition - direct update
        Serial.printf("Direct update to version %s from factory partition\n", selectedVersion);
        performOTAUpdateToVersion(selectedVersion);
        inputMessage = "Update initiated from factory partition";

      } else {
        // Case: Running on ota_0 partition - use separate namespace to avoid conflicts
        Serial.println("UPDATE DEBUG: Storing update request in NVS");

        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("update_req", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to open NVS namespace 'update_req': %s\n", esp_err_to_name(err));
          request->send(500, "text/plain", "NVS open failed");
          return;
        }

        // Clear any existing update flags first
        nvs_erase_key(nvs_handle, "target_ver");
        nvs_erase_key(nvs_handle, "update_flag");

        // Set target version
        err = nvs_set_str(nvs_handle, "target_ver", selectedVersion);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to set target_ver: %s\n", esp_err_to_name(err));
          nvs_close(nvs_handle);
          request->send(500, "text/plain", "NVS write failed");
          return;
        }

        // Set update flag
        uint8_t updateFlag = 1;
        err = nvs_set_u8(nvs_handle, "update_flag", updateFlag);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to set update_flag: %s\n", esp_err_to_name(err));
          nvs_close(nvs_handle);
          request->send(500, "text/plain", "NVS write failed");
          return;
        }

        // Set wake flag so WiFi stays on after reboot
        uint8_t wakeFlag = 1;
        err = nvs_set_u8(nvs_handle, "wake_flag", wakeFlag);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to set wake_flag: %s\n", esp_err_to_name(err));
        }

        // Commit and close
        err = nvs_commit(nvs_handle);

        // Verify the write worked
        nvs_handle_t verify_handle;
        if (nvs_open("update_req", NVS_READONLY, &verify_handle) == ESP_OK) {
          uint8_t verifyFlag = 0;
          char verifyVersion[32] = { 0 };
          size_t verifySize = 32;

          if (nvs_get_u8(verify_handle, "update_flag", &verifyFlag) == ESP_OK && nvs_get_str(verify_handle, "target_ver", verifyVersion, &verifySize) == ESP_OK) {
            Serial.printf("UPDATE DEBUG: NVS write verified - flag: %u, version: %s\n", verifyFlag, verifyVersion);
          } else {
            Serial.println("UPDATE DEBUG: NVS write verification FAILED");
          }
          nvs_close(verify_handle);
        }

        Serial.printf("Update to version %s requested - rebooting to factory\n", selectedVersion);

        // Switch to factory partition and reboot
        esp_ota_set_boot_partition(factory_partition);
        inputMessage = "Rebooting to factory for update";

        // Send response first, then reboot
        request->send(200, "text/plain", inputMessage);
        Serial.println("=== RESTART SEQUENCE ===");
        Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
        Serial.printf("Active tasks: %u\n", uxTaskGetNumberOfTasks());
        Serial.println("Closing event connections...");
        events.close();
        delay(1000);
        Serial.println("Restarting now...");
        Serial.println("========================================");
        Serial.println("NOTE: Task termination backtrace next is expected and harmless");
        Serial.println("========================================");
        Serial.flush();
        delay(5000);  // Extra delay to ensure message prints
        ESP.restart();
        return;
      }
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
    Serial.println("=== MAIN SERVER onNotFound HANDLER ===");
    Serial.print("Request for: ");
    Serial.println(path);

    if (webFS.exists(path)) {
      String contentType = "text/html";
      if (path.endsWith(".css")) contentType = "text/css";
      else if (path.endsWith(".js")) contentType = "application/javascript";
      else if (path.endsWith(".json")) contentType = "application/json";
      else if (path.endsWith(".png")) contentType = "image/png";
      else if (path.endsWith(".jpg")) contentType = "image/jpeg";

      Serial.println("File found in webFS, serving with content-type: " + contentType);
      request->send(webFS, path, contentType);
    } else {
      Serial.print("File not found in webFS: ");
      Serial.println(path);
      Serial.println("Redirecting to captive portal: " + WiFi.softAPIP().toString());
      // Only redirect in AP mode
      if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        request->redirect("http://" + WiFi.softAPIP().toString());
      } else {
        request->send(404, "text/plain", "Not Found");
      }
    }
  });

  // Setup event source for real-time updates
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });

  // Diagnostic endpoint to check partition and version
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    String info = "Partition: " + String(running ? running->label : "unknown") + "\n";
    info += "Version: " + String(FIRMWARE_VERSION) + "\n";
    info += "Free heap: " + String(ESP.getFreeHeap()) + "\n";
    request->send(200, "text/plain", info);
  });

  server.addHandler(&events);
  server.begin();
  Serial.println("=== CONFIG SERVER STARTED ===");
  Serial.println("=== MAIN SERVER SETUP COMPLETE ===");
}

void ensurePreferredBootPartition() {
  //called during startup, in setup() function.
  //Purpose: Decides which firmware partition to boot from and handles emergency recovery.
  //User Flow Permutations:
  //Normal Operation (GPIO41 not pressed)
  //If running from factory → switch to ota_0 (if valid firmware exists there)
  //If running from ota_0 → stay on ota_0
  //Goal: Always prefer the updated firmware in ota_0
  //Emergency Recovery (GPIO41 pressed during boot)
  //Force boot to factory partition regardless of current location
  //Clear any stuck OTA update flags in memory
  //Stay in factory mode (safe, known-good firmware)
  //Corrupted ota_0 Firmware
  //If ota_0 has invalid/corrupted firmware → stay on factory
  //Prevents boot to broken firmware
  const esp_partition_t *ota0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  const esp_partition_t *current_boot = esp_ota_get_boot_partition();

  // Check GPIO41 for manual factory reset
  pinMode(41, INPUT_PULLUP);
  bool forceFactory = (digitalRead(41) == LOW);  //When pin 41 is low, forceFactory = 1

  if (forceFactory) {
    // Clear any pending update flags to prevent boot loops
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
      nvs_erase_key(nvs_handle, "update_flag");
      nvs_erase_key(nvs_handle, "target_ver");
      nvs_commit(nvs_handle);
      nvs_close(nvs_handle);
    }


    if (current_boot != factory_partition) {
      esp_ota_set_boot_partition(factory_partition);
      Serial.println("Boot: GPIO41 forced factory partition - restarting...");
      events.send("Boot: GPIO41 forced factory partition - restarting...", "console", millis());
      Serial.printf("GPIO41 state: %d\n", digitalRead(41));
      delay(1500);
      ESP.restart();
    } else {
      Serial.println("Boot: GPIO41 detected - staying in factory mode");
      events.send("Boot: GPIO41 detected - staying in factory mode", "console", millis());
      Serial.printf("GPIO41 status: %d\n", digitalRead(41));
    }


    return;  // <<<< CRITICAL: Exit function, don't run normal logic
  }

  // Check if ota_0 has valid firmware
  esp_app_desc_t app_desc;
  esp_err_t err = esp_ota_get_partition_description(ota0_partition, &app_desc);
  bool ota0_valid = (err == ESP_OK);

  if (ota0_valid && current_boot != ota0_partition) {
    esp_ota_set_boot_partition(ota0_partition);
    Serial.println("Boot: Switched to ota_0 partition !!");
    events.send("Boot: Switched to ota_0 partition", "console", millis());
  } else if (!ota0_valid && current_boot != factory_partition) {
    esp_ota_set_boot_partition(factory_partition);
    Serial.println("Boot: ota_0 invalid, using factory partition");
    events.send("Boot: ota_0 invalid, using factory partition", "console", millis());
  }
}

void dnsHandleRequest() {  //process dns request for captive portals
  if (currentMode == MODE_AP || currentMode == MODE_CONFIG) {
    // Only process if there might actually be a request waiting
    // This is a workaround for ESP32-S3 DNS library blocking issues
    static unsigned long lastDNSProcess = 0;
    if (millis() - lastDNSProcess > 100) {  // Throttle to every 100ms
      dnsServer.processNextRequest();
      lastDNSProcess = millis();
    }
  }
}

void checkWiFiConnection() {  // Intelligent WiFi reconnection with exponential backoff and signal awareness
  // Only attempt reconnection in client mode
  if (currentMode != MODE_CLIENT) return;

  if (WiFi.status() == WL_CONNECTED) {
    // Update signal strength while connected for future reference
    wifiRecon.lastSignalStrength = WiFi.RSSI();

    // Reset reconnection state on successful connection
    if (wifiRecon.attemptCount > 0) {
      Serial.println("WiFi reconnected successfully!");
      queueConsoleMessage("WiFi reconnected after " + String(wifiRecon.attemptCount) + " attempts");
    }
    wifiRecon.attemptCount = 0;
    wifiRecon.currentInterval = wifiRecon.minInterval;
    wifiRecon.giveUpMode = false;
    return;
  }

  // WiFi is disconnected - check if we should give up temporarily
  if (wifiRecon.giveUpMode) {
    // Only retry every 5 minutes in give-up mode
    if (millis() - wifiRecon.lastAttempt < 300000) return;
    Serial.println("WiFi: Exiting give-up mode, attempting fresh reconnection burst");
    wifiRecon.giveUpMode = false;  // Try one more burst
    wifiRecon.attemptCount = 0;
    wifiRecon.currentInterval = wifiRecon.minInterval;
  }

  // Signal strength awareness - don't waste time if signal was poor
  if (wifiRecon.lastSignalStrength != -999 && wifiRecon.lastSignalStrength < wifiRecon.minSignalThreshold) {
    // Signal was too weak - use longer intervals to avoid battery drain
    if (millis() - wifiRecon.lastAttempt < 60000) return;  // Wait 1 minute between attempts
    Serial.printf("WiFi: Poor signal (%d dBm), using extended retry interval\n", wifiRecon.lastSignalStrength);
  } else {
    // Normal exponential backoff interval check
    if (millis() - wifiRecon.lastAttempt < wifiRecon.currentInterval) return;
  }

  wifiRecon.lastAttempt = millis();
  wifiRecon.attemptCount++;

  Serial.printf("WiFi reconnection attempt #%d (interval: %lums, last signal: %d dBm)\n",
                wifiRecon.attemptCount, wifiRecon.currentInterval, wifiRecon.lastSignalStrength);

  // Load saved credentials
  String saved_ssid = readFile(LittleFS, WIFI_SSID_FILE);
  String saved_password = readFile(LittleFS, WIFI_PASS_FILE);
  saved_ssid.trim();
  saved_password.trim();

  if (saved_ssid.length() == 0) {
    Serial.println("WiFi: No saved SSID found for reconnection");
    return;
  }

  // Use timeout but still safe for 15-second watchdog (10s + 5s safety margin)
  bool connected = connectToWiFi(saved_ssid.c_str(), saved_password.c_str(), 10000);

  if (connected) {
    Serial.println("WiFi reconnection successful!");
    return;  // Success will be handled in next loop iteration
  }

  // Failed - check if we should give up temporarily
  if (wifiRecon.attemptCount >= wifiRecon.maxAttempts) {
    Serial.println("WiFi: Max attempts reached, entering give-up mode for 5 minutes");
    queueConsoleMessage("WiFi: Max reconnection attempts (" + String(wifiRecon.maxAttempts) + ") reached, will retry in 5 minutes");
    wifiRecon.giveUpMode = true;
    return;
  }

  // Intelligent exponential backoff with caps
  // Progression: 2s, 4s, 8s, 16s, 32s, 60s, 120s, 300s (max)
  if (wifiRecon.currentInterval < 32000) {
    wifiRecon.currentInterval *= 2;  // Double until 32 seconds
  } else if (wifiRecon.currentInterval < 60000) {
    wifiRecon.currentInterval = 60000;  // Jump to 1 minute
  } else if (wifiRecon.currentInterval < 120000) {
    wifiRecon.currentInterval = 120000;  // Jump to 2 minutes
  } else {
    wifiRecon.currentInterval = wifiRecon.maxInterval;  // Cap at 5 minutes
  }

  Serial.printf("WiFi: Next attempt in %lu seconds\n", wifiRecon.currentInterval / 1000);

  // Helpful signal quality logging for debugging
  if (wifiRecon.lastSignalStrength != -999) {
    if (wifiRecon.lastSignalStrength < -80) {
      queueConsoleMessage("WiFi: Weak signal (" + String(wifiRecon.lastSignalStrength) + " dBm) may be causing disconnections");
    }
  }
}


void SendWifiData() {
  unsigned long start66 = micros();

  // Static variables for timing control
  static unsigned long prev_millis5 = 0;            // CSVData timing
  static unsigned long lastConsoleMessageTime = 0;  // Console timing
  static unsigned long lastpayload2send = 0;        // CSVData2 timing
  static unsigned long lastpayload3send = 0;        // CSVData3 timing
  static unsigned long lastTimestampSend = 0;       // TimestampData timing
  static unsigned long lastEventSourceSend = 0;     // Global cooldown tracker

  // Configuration
  const unsigned long EVENTSOURCE_COOLDOWN = 8;         // 8ms between any EventSource sends
  const unsigned long CONSOLE_MESSAGE_INTERVAL = 1000;  // 1 second for console

  unsigned long now = millis();

  // Don't process if WiFi is off or disconnected
  if (WiFi.getMode() == WIFI_OFF) {
    return;  // WiFi is completely off (low power mode) - exit immediately
  }
  if (currentMode == MODE_CLIENT && WiFi.status() != WL_CONNECTED) {
    return;  // Client mode but not connected to network - exit immediately
  }


  // Check if we can send anything (global cooldown)
  bool canSendNow = (now - lastEventSourceSend >= EVENTSOURCE_COOLDOWN);

  if (!canSendNow) {
    return;  // Still in cooldown period
  }

  bool sentSomething = false;

  // PRIORITY 1: CSVData (highest priority - real-time data)
  if (!sentSomething && now - prev_millis5 >= webgaugesinterval && events.count() > 0) {
    WifiStrength = WiFi.RSSI();
    WifiHeartBeat = WifiHeartBeat + 1;
    static char payload1[420];
    //// Payload goes here
    snprintf(payload1, sizeof(payload1),
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
             SafeInt(AlternatorTemperatureF),  // 0
             SafeInt(dutyCycle),               // 1
             SafeInt(BatteryV, 100),           // 2
             SafeInt(MeasuredAmps, 100),       // 3
             SafeInt(RPM),                     // 4
             SafeInt(Channel3V, 100),          // 5
             SafeInt(IBV, 100),                // 6
             SafeInt(Bcur, 100),               // 7
             SafeInt(VictronVoltage, 100),     // 8
             SafeInt(LoopTime),                // 9
             SafeInt(WifiStrength),            // 10
             SafeInt(WifiHeartBeat),           // 11
             SafeInt(SendWifiTime),            // 12
             SafeInt(AnalogReadTime),          // 13
             SafeInt(VeTime),                  // 14
             SafeInt(MaximumLoopTime),         // 15
             SafeInt(HeadingNMEA),             // 16
             SafeInt(vvout, 100),              // 17
             SafeInt(iiout, 10),               // 18
             SafeInt(FreeHeap),                // 19
             SafeInt(EngineCycles),            // 20
             SafeInt(Alarm_Status),            // 21
             SafeInt(fieldActiveStatus),       // 22
             SafeInt(CurrentSessionDuration),  // 23
             SafeInt(timeAxisModeChanging),    // 24 - PLOT CONTROL
             SafeInt(webgaugesinterval),       // 25 - PLOT CONTROL
             SafeInt(plotTimeWindow),          // 26 - PLOT CONTROL
             SafeInt(Ymin1),                   // 27 - PLOT CONTROL
             SafeInt(Ymax1),                   // 28 - PLOT CONTROL
             SafeInt(Ymin2),                   // 29 - PLOT CONTROL
             SafeInt(Ymax2),                   // 30 - PLOT CONTROL
             SafeInt(Ymin3),                   // 31 - PLOT CONTROL
             SafeInt(Ymax3),                   // 32 - PLOT CONTROL
             SafeInt(Ymin4),                   // 33 - PLOT CONTROL
             SafeInt(Ymax4),                   // 34 - PLOT CONTROL
             SafeInt((int)currentMode),        // 35 - DEVICE MODE
             SafeInt(currentPartitionType)     // 36 - PARTITION TYPE

    );

    events.send(payload1, "CSVData");
    SendWifiTime = micros() - start66;
    prev_millis5 = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }

  // PRIORITY 2: Console (important for debugging)
  if (!sentSomething && now - lastConsoleMessageTime >= CONSOLE_MESSAGE_INTERVAL && consoleCount > 0) {

    int localCount = consoleCount;
    int messagesToSend = (2 < localCount) ? 2 : localCount;

    for (int i = 0; i < messagesToSend; i++) {
      if (consoleCount > 0) {
        events.send(consoleQueue[consoleTail].message, "console");
        consoleTail = (consoleTail + 1) % CONSOLE_QUEUE_SIZE;
        consoleCount--;
      }
    }

    lastConsoleMessageTime = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }

  // PRIORITY 3: CSVData2 (status data - every 2 seconds)
  if (!sentSomething && now - lastpayload2send >= 2000) {

    static char payload2[1200];
    //payload goes here
    snprintf(payload2, sizeof(payload2),
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",

             SafeInt(IBVMax, 100),  //0
             SafeInt(MeasuredAmpsMax, 100),
             SafeInt(RPMMax),  //2
             SafeInt(SOC_percent),
             SafeInt(EngineRunTime),       //4
             SafeInt(AlternatorOnTime),    //5
             SafeInt(AlternatorFuelUsed),  //6
             SafeInt(ChargedEnergy),
             SafeInt(DischargedEnergy),  //8
             SafeInt(AlternatorChargedEnergy),
             SafeInt(MaxAlternatorTemperatureF),  //10
             SafeInt(temperatureThermistor),
             SafeInt(MaxTemperatureThermistor),  //12
             SafeInt(VictronCurrent, 100),
             SafeInt(timeToFullChargeMin),  //14
             SafeInt(timeToFullDischargeMin),
             SafeInt(LatitudeNMEA * 1000000),  //16
             SafeInt(LongitudeNMEA * 1000000),
             SafeInt(SatelliteCountNMEA),  //18
             SafeInt(bulkCompleteTime),
             SafeInt(LastSessionDuration),  //20
             SafeInt(LastSessionMaxLoopTime),
             SafeInt(lastSessionMinHeap),  //22
             SafeInt(wifiReconnectsTotal),
             SafeInt(LastResetReason),  //24
             SafeInt(ancientResetReason),
             SafeInt(totalPowerCycles),  //26
             SafeInt(MinFreeHeap),
             SafeInt(currentWeatherMode),  //28
             SafeInt(UVToday),
             SafeInt(UVTomorrow),  //30
             SafeInt(UVDay2),
             SafeInt(weatherDataValid),  //32
             SafeInt(SolarWatts),
             SafeInt(performanceRatio, 100),  //34
             SafeInt(OnOff),
             SafeInt(ManualFieldToggle),  //36
             SafeInt(HiLow),
             SafeInt(LimpHome),  //38
             SafeInt(VeData),
             SafeInt(NMEA0183Data),  //40
             SafeInt(NMEA2KData),
             SafeInt(AlarmActivate),  //42
             SafeInt(TempAlarm),
             SafeInt(VoltageAlarmHigh),  //44
             SafeInt(VoltageAlarmLow),
             SafeInt(CurrentAlarmHigh),  //46
             SafeInt(AlarmTest),
             SafeInt(AlarmLatchEnabled),  //48
             SafeInt(alarmLatch ? 1 : 0),
             SafeInt(ResetAlarmLatch),  //50
             SafeInt(ForceFloat),
             SafeInt(ResetTemp),  //52
             SafeInt(ResetVoltage),
             SafeInt(ResetCurrent),  //54
             SafeInt(ResetEngineRunTime),
             SafeInt(ResetAlternatorOnTime),  //56
             SafeInt(ResetEnergy),
             SafeInt(ManualSOCPoint),  //58
             SafeInt(LearningMode),
             SafeInt(LearningPaused),  //60
             SafeInt(IgnoreLearningDuringPenalty),
             SafeInt(ShowLearningDebugMessages),  //62
             SafeInt(LogAllLearningEvents),
             SafeInt(EXTRA),  //64
             SafeInt(LearningDryRunMode),
             SafeInt(AutoSaveLearningTable),  //66
             SafeInt(ResetLearningTable),
             SafeInt(ClearOverheatHistory),  //68
             SafeInt(AutoShuntGainCorrection),
             SafeInt(DynamicShuntGainFactor, 1000),  //70
             SafeInt(AutoAltCurrentZero),
             SafeInt(DynamicAltCurrentZero, 1000),  //72
             SafeInt(InsulationLifePercent, 100),
             SafeInt(GreaseLifePercent, 100),  //74
             SafeInt(BrushLifePercent, 100),
             SafeInt(PredictedLifeHours),  //76
             SafeInt(LifeIndicatorColor),
             SafeInt(WindingTempOffset),  //78
             SafeInt(ManualLifePercentage),
             SafeInt(UVThresholdHigh, 100),  //80
             SafeInt(weatherModeEnabled),
             SafeInt(pKwHrToday, 100),  //82
             SafeInt(pKwHrTomorrow, 100),
             SafeInt(pKwHr2days, 100),     // 84
             SafeInt(ambientTemp),         // 85
             SafeInt(baroPressure),        //86
             SafeInt(firmwareVersionInt),  //87
             SafeInt(deviceIdUpper),       //88
             SafeInt(deviceIdLower)        //89
    );
    events.send(payload2, "CSVData2");
    lastpayload2send = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }

  // PRIORITY 4: CSVData3 (settings data - every 2 seconds)
  if (!sentSomething && now - lastpayload3send >= 2000) {

    static char payload3[1100];
    snprintf(payload3, sizeof(payload3),
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",

             SafeInt(TemperatureLimitF),                      // 0
             SafeInt(BulkVoltage, 100),                       // 1
             SafeInt(TargetAmps),                             // 2
             SafeInt(FloatVoltage, 100),                      // 3
             SafeInt(SwitchingFrequency),                     // 4
             SafeInt(dutyStep, 100),                          // 5
             SafeInt(FieldAdjustmentInterval),                // 6
             SafeInt(ManualDutyTarget),                       // 7
             SafeInt(SwitchControlOverride),                  // 8
             SafeInt(TargetAmpL),                             // 9
             SafeInt(CurrentThreshold, 100),                  // 10
             SafeInt(PeukertExponent_scaled),                 // 11
             SafeInt(ChargeEfficiency_scaled),                // 12
             SafeInt(ChargedVoltage_Scaled),                  // 13
             SafeInt(TailCurrent),                            // 14
             SafeInt(ChargedDetectionTime),                   // 15
             SafeInt(IgnoreTemperature),                      // 16
             SafeInt(bmsLogic),                               // 17
             SafeInt(bmsLogicLevelOff),                       // 18
             SafeInt(FourWay),                                // 19
             SafeInt(RPMScalingFactor),                       // 20
             SafeInt(MaximumAllowedBatteryAmps),              // 21
             SafeInt(BatteryVoltageSource),                   // 22
             SafeInt(LearningUpwardEnabled),                  // 23
             SafeInt(LearningDownwardEnabled),                // 24
             SafeInt(AlternatorNominalAmps),                  // 25
             SafeInt(LearningUpStep, 100),                    // 26
             SafeInt(LearningDownStep, 100),                  // 27
             SafeInt(AmbientTempCorrectionFactor, 100),       // 28
             SafeInt(AmbientTempBaseline),                    // 29
             SafeInt(MinLearningInterval),                    // 30
             SafeInt(SafeOperationThreshold),                 // 31
             SafeInt(PidKp, 1000),                            // 32
             SafeInt(PidKi, 1000),                            // 33
             SafeInt(PidKd, 1000),                            // 34
             SafeInt(PidSampleTime),                          // 35
             SafeInt(MaxTableValue, 100),                     // 36
             SafeInt(MinTableValue, 100),                     // 37
             SafeInt(MaxPenaltyPercent, 100),                 // 38
             SafeInt(MaxPenaltyDuration / 1000),              // 39
             SafeInt(NeighborLearningFactor, 1000),           // 40
             SafeInt(LearningRPMSpacing),                     // 41
             SafeInt(LearningMemoryDuration / 86400000),      // 42
             SafeInt(EnableNeighborLearning),                 // 43
             SafeInt(EnableAmbientCorrection),                // 44
             SafeInt(LearningFailsafeMode),                   // 45
             SafeInt(LearningTableSaveInterval / 1000),       // 46
             SafeInt(rpmCurrentTable[0], 100),                // 47
             SafeInt(rpmCurrentTable[1], 100),                // 48
             SafeInt(rpmCurrentTable[2], 100),                // 49
             SafeInt(rpmCurrentTable[3], 100),                // 50
             SafeInt(rpmCurrentTable[4], 100),                // 51
             SafeInt(rpmCurrentTable[5], 100),                // 52
             SafeInt(rpmCurrentTable[6], 100),                // 53
             SafeInt(rpmCurrentTable[7], 100),                // 54
             SafeInt(rpmCurrentTable[8], 100),                // 55
             SafeInt(rpmCurrentTable[9], 100),                // 56
             SafeInt(currentRPMTableIndex),                   // 57
             SafeInt(pidInitialized ? 1 : 0),                 // 58
             SafeInt(ShuntResistanceMicroOhm),                // 59
             SafeInt(InvertAltAmps),                          // 60
             SafeInt(InvertBattAmps),                         // 61
             SafeInt(MaxDuty),                                // 62
             SafeInt(MinDuty),                                // 63
             SafeInt(FieldResistance),                        // 64
             SafeInt(maxPoints),                              // 65
             SafeInt(AlternatorCOffset, 100),                 // 66
             SafeInt(BatteryCOffset, 100),                    // 67
             SafeInt(BatteryCapacity_Ah),                     // 68
             SafeInt(AmpSrc),                                 // 69
             SafeInt(R_fixed, 100),                           // 70
             SafeInt(Beta, 100),                              // 71
             SafeInt(T0_C, 100),                              // 72
             SafeInt(TempSource),                             // 73
             SafeInt(IgnitionOverride),                       // 74
             SafeInt(FLOAT_DURATION),                         // 75
             SafeInt(PulleyRatio, 100),                       // 76
             SafeInt(BatteryCurrentSource),                   // 77
             SafeInt(overheatCount[0]),                       // 78
             SafeInt(overheatCount[1]),                       // 79
             SafeInt(overheatCount[2]),                       // 80
             SafeInt(overheatCount[3]),                       // 81
             SafeInt(overheatCount[4]),                       // 82
             SafeInt(overheatCount[5]),                       // 83
             SafeInt(overheatCount[6]),                       // 84
             SafeInt(overheatCount[7]),                       // 85
             SafeInt(overheatCount[8]),                       // 86
             SafeInt(overheatCount[9]),                       // 87
             SafeInt(cumulativeNoOverheatTime[0] / 3600000),  // 88
             SafeInt(cumulativeNoOverheatTime[1] / 3600000),  // 89
             SafeInt(cumulativeNoOverheatTime[2] / 3600000),  // 90
             SafeInt(cumulativeNoOverheatTime[3] / 3600000),  // 91
             SafeInt(cumulativeNoOverheatTime[4] / 3600000),  // 92
             SafeInt(cumulativeNoOverheatTime[5] / 3600000),  // 93
             SafeInt(cumulativeNoOverheatTime[6] / 3600000),  // 94
             SafeInt(cumulativeNoOverheatTime[7] / 3600000),  // 95
             SafeInt(cumulativeNoOverheatTime[8] / 3600000),  // 96
             SafeInt(cumulativeNoOverheatTime[9] / 3600000),  // 97
             SafeInt(totalLearningEvents),                    // 98
             SafeInt(totalOverheats),                         // 99
             SafeInt(totalSafeHours),                         // 100
             SafeInt(averageTableValue * 100),                // 101
             SafeInt(timeSinceLastOverheat / 1000),           // 102
             SafeInt(learningTargetFromRPM, 100),             // 103
             SafeInt(ambientTempCorrection, 100),             // 104
             SafeInt(finalLearningTarget, 100),               // 105
             SafeInt(overheatingPenaltyTimer / 1000),         // 106
             SafeInt(overheatingPenaltyAmps, 100),            // 107
             SafeInt(pidInput, 100),                          // 108
             SafeInt(pidSetpoint, 100),                       // 109
             SafeInt(pidOutput, 100),                         // 110
             SafeInt(TempToUse),                              // 111
             SafeInt(rpmTableRPMPoints[0]),                   // 112
             SafeInt(rpmTableRPMPoints[1]),                   // 113
             SafeInt(rpmTableRPMPoints[2]),                   // 114
             SafeInt(rpmTableRPMPoints[3]),                   // 115
             SafeInt(rpmTableRPMPoints[4]),                   // 116
             SafeInt(rpmTableRPMPoints[5]),                   // 117
             SafeInt(rpmTableRPMPoints[6]),                   // 118
             SafeInt(rpmTableRPMPoints[7]),                   // 119
             SafeInt(rpmTableRPMPoints[8]),                   // 120
             SafeInt(rpmTableRPMPoints[9]),                   // 121
             SafeInt(LearningSettlingPeriod / 1000),          // 122
             SafeInt(LearningRPMChangeThreshold),             // 123
             SafeInt(LearningTempHysteresis)                  // 124

    );

    events.send(payload3, "CSVData3");
    lastpayload3send = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }

  // PRIORITY 5: TimestampData (staleness data - every 3 seconds)
  if (!sentSomething && now - lastTimestampSend >= 3000) {

    static char timestampPayload[400];
    snprintf(timestampPayload, sizeof(timestampPayload),
             "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
             (dataTimestamps[IDX_HEADING_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_HEADING_NMEA]),        // 0
             (dataTimestamps[IDX_LATITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LATITUDE_NMEA]),      // 1
             (dataTimestamps[IDX_LONGITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LONGITUDE_NMEA]),    // 2
             (dataTimestamps[IDX_SATELLITE_COUNT] == 0) ? 999999 : (now - dataTimestamps[IDX_SATELLITE_COUNT]),  // 3
             (dataTimestamps[IDX_VICTRON_VOLTAGE] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_VOLTAGE]),  // 4
             (dataTimestamps[IDX_VICTRON_CURRENT] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_CURRENT]),  // 5
             (dataTimestamps[IDX_ALTERNATOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_ALTERNATOR_TEMP]),  // 6
             (dataTimestamps[IDX_THERMISTOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_THERMISTOR_TEMP]),  // 7
             (dataTimestamps[IDX_RPM] == 0) ? 999999 : (now - dataTimestamps[IDX_RPM]),                          // 8
             (dataTimestamps[IDX_MEASURED_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_MEASURED_AMPS]),      // 9
             (dataTimestamps[IDX_BATTERY_V] == 0) ? 999999 : (now - dataTimestamps[IDX_BATTERY_V]),              // 10
             (dataTimestamps[IDX_IBV] == 0) ? 999999 : (now - dataTimestamps[IDX_IBV]),                          // 11
             (dataTimestamps[IDX_BCUR] == 0) ? 999999 : (now - dataTimestamps[IDX_BCUR]),                        // 12
             (dataTimestamps[IDX_CHANNEL3V] == 0) ? 999999 : (now - dataTimestamps[IDX_CHANNEL3V]),              // 13
             (dataTimestamps[IDX_DUTY_CYCLE] == 0) ? 999999 : (now - dataTimestamps[IDX_DUTY_CYCLE]),            // 14
             (dataTimestamps[IDX_FIELD_VOLTS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_VOLTS]),          // 15
             (dataTimestamps[IDX_FIELD_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_AMPS])             // 16
    );

    events.send(timestampPayload, "TimestampData");
    lastTimestampSend = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }
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


  Serial.println("QUEUE DEBUG: " + message);


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
  // Throttle: only save every X (currently 2) minutes
  unsigned long currentTime = millis();
  if (currentTime - lastNVSSaveTime < NVS_SAVE_INTERVAL) {
    return;
  }

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("ERROR: Failed to open NVS");
    Serial.println("ERROR: Failed to open NVS");
    return;
  }

  bool anyChanges = false;

  // Energy Tracking
  if (prev_ChargedEnergy != (uint32_t)ChargedEnergy) {
    nvs_set_u32(nvs_handle, "ChargedEnergy", (uint32_t)ChargedEnergy);
    prev_ChargedEnergy = (uint32_t)ChargedEnergy;
    anyChanges = true;
  }
  if (prev_DischrgdEnergy != (uint32_t)DischargedEnergy) {
    nvs_set_u32(nvs_handle, "DischrgdEnergy", (uint32_t)DischargedEnergy);
    prev_DischrgdEnergy = (uint32_t)DischargedEnergy;
    anyChanges = true;
  }
  if (prev_AltChrgdEnergy != (uint32_t)AlternatorChargedEnergy) {
    nvs_set_u32(nvs_handle, "AltChrgdEnergy", (uint32_t)AlternatorChargedEnergy);
    prev_AltChrgdEnergy = (uint32_t)AlternatorChargedEnergy;
    anyChanges = true;
  }
  if (prev_AltFuelUsed != (int32_t)AlternatorFuelUsed) {
    nvs_set_i32(nvs_handle, "AltFuelUsed", (int32_t)AlternatorFuelUsed);
    prev_AltFuelUsed = (int32_t)AlternatorFuelUsed;
    anyChanges = true;
  }

  // Runtime Tracking
  if (prev_EngineRunTime != (int32_t)EngineRunTime) {
    nvs_set_i32(nvs_handle, "EngineRunTime", (int32_t)EngineRunTime);
    prev_EngineRunTime = (int32_t)EngineRunTime;
    anyChanges = true;
  }
  if (prev_EngineCycles != (int32_t)EngineCycles) {
    nvs_set_i32(nvs_handle, "EngineCycles", (int32_t)EngineCycles);
    prev_EngineCycles = (int32_t)EngineCycles;
    anyChanges = true;
  }
  if (prev_AltOnTime != (int32_t)AlternatorOnTime) {
    nvs_set_i32(nvs_handle, "AltOnTime", (int32_t)AlternatorOnTime);
    prev_AltOnTime = (int32_t)AlternatorOnTime;
    anyChanges = true;
  }

  // Battery State
  if (prev_SOC_percent != (int32_t)SOC_percent) {
    nvs_set_i32(nvs_handle, "SOC_percent", (int32_t)SOC_percent);
    prev_SOC_percent = (int32_t)SOC_percent;
    anyChanges = true;
  }
  if (prev_CoulombCount != (int32_t)CoulombCount_Ah_scaled) {
    nvs_set_i32(nvs_handle, "CoulombCount", (int32_t)CoulombCount_Ah_scaled);
    prev_CoulombCount = (int32_t)CoulombCount_Ah_scaled;
    anyChanges = true;
  }

  // Session Health Stats
  if (prev_SessionDur != (uint32_t)CurrentSessionDuration) {
    nvs_set_u32(nvs_handle, "SessionDur", (uint32_t)CurrentSessionDuration);
    prev_SessionDur = (uint32_t)CurrentSessionDuration;
    anyChanges = true;
  }
  if (prev_MaxLoop != (int32_t)MaxLoopTime) {
    nvs_set_i32(nvs_handle, "MaxLoop", (int32_t)MaxLoopTime);
    prev_MaxLoop = (int32_t)MaxLoopTime;
    anyChanges = true;
  }
  if (prev_MinHeap != (int32_t)MinFreeHeap) {
    nvs_set_i32(nvs_handle, "MinHeap", (int32_t)MinFreeHeap);
    prev_MinHeap = (int32_t)MinFreeHeap;
    anyChanges = true;
  }

  // System Health Counters
  if (prev_PowerCycles != (int32_t)totalPowerCycles) {
    nvs_set_i32(nvs_handle, "PowerCycles", (int32_t)totalPowerCycles);
    prev_PowerCycles = (int32_t)totalPowerCycles;
    anyChanges = true;
  }

  // Thermal Stress
  if (prev_InsulDamage != CumulativeInsulationDamage) {
    nvs_set_blob(nvs_handle, "InsulDamage", &CumulativeInsulationDamage, sizeof(float));
    prev_InsulDamage = CumulativeInsulationDamage;
    anyChanges = true;
  }
  if (prev_GreaseDamage != CumulativeGreaseDamage) {
    nvs_set_blob(nvs_handle, "GreaseDamage", &CumulativeGreaseDamage, sizeof(float));
    prev_GreaseDamage = CumulativeGreaseDamage;
    anyChanges = true;
  }
  if (prev_BrushDamage != CumulativeBrushDamage) {
    nvs_set_blob(nvs_handle, "BrushDamage", &CumulativeBrushDamage, sizeof(float));
    prev_BrushDamage = CumulativeBrushDamage;
    anyChanges = true;
  }

  // Dynamic Learning
  if (prev_ShuntGain != DynamicShuntGainFactor) {
    nvs_set_blob(nvs_handle, "ShuntGain", &DynamicShuntGainFactor, sizeof(float));
    prev_ShuntGain = DynamicShuntGainFactor;
    anyChanges = true;
  }
  if (prev_AltZero != DynamicAltCurrentZero) {
    nvs_set_blob(nvs_handle, "AltZero", &DynamicAltCurrentZero, sizeof(float));
    prev_AltZero = DynamicAltCurrentZero;
    anyChanges = true;
  }
  if (prev_LastGainTime != (uint32_t)lastGainCorrectionTime) {
    nvs_set_u32(nvs_handle, "LastGainTime", (uint32_t)lastGainCorrectionTime);
    prev_LastGainTime = (uint32_t)lastGainCorrectionTime;
    anyChanges = true;
  }
  if (prev_LastZeroTime != (uint32_t)lastAutoZeroTime) {
    nvs_set_u32(nvs_handle, "LastZeroTime", (uint32_t)lastAutoZeroTime);
    prev_LastZeroTime = (uint32_t)lastAutoZeroTime;
    anyChanges = true;
  }
  if (prev_LastZeroTemp != lastAutoZeroTemp) {
    nvs_set_blob(nvs_handle, "LastZeroTemp", &lastAutoZeroTemp, sizeof(float));
    prev_LastZeroTemp = lastAutoZeroTemp;
    anyChanges = true;
  }

  if (anyChanges) {
    nvs_commit(nvs_handle);
  }
  nvs_close(nvs_handle);

  lastNVSSaveTime = currentTime;
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

  // System Health Counters
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

void initNVSCache() {
  prev_ChargedEnergy = (uint32_t)ChargedEnergy;
  prev_DischrgdEnergy = (uint32_t)DischargedEnergy;
  prev_AltChrgdEnergy = (uint32_t)AlternatorChargedEnergy;
  prev_AltFuelUsed = (int32_t)AlternatorFuelUsed;
  prev_EngineRunTime = (int32_t)EngineRunTime;
  prev_EngineCycles = (int32_t)EngineCycles;
  prev_AltOnTime = (int32_t)AlternatorOnTime;
  prev_SOC_percent = (int32_t)SOC_percent;
  prev_CoulombCount = (int32_t)CoulombCount_Ah_scaled;
  prev_SessionDur = (uint32_t)CurrentSessionDuration;
  prev_MaxLoop = (int32_t)MaxLoopTime;
  prev_MinHeap = (int32_t)MinFreeHeap;
  prev_PowerCycles = (int32_t)totalPowerCycles;
  prev_InsulDamage = CumulativeInsulationDamage;
  prev_GreaseDamage = CumulativeGreaseDamage;
  prev_BrushDamage = CumulativeBrushDamage;
  prev_ShuntGain = DynamicShuntGainFactor;
  prev_AltZero = DynamicAltCurrentZero;
  prev_LastGainTime = (uint32_t)lastGainCorrectionTime;
  prev_LastZeroTime = (uint32_t)lastAutoZeroTime;
  prev_LastZeroTemp = lastAutoZeroTemp;
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

  nvs_close(nvs_handle);
}

//OTA UPDATES--
//  Device ID generation
String getDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  return String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
}

// Base64 decode function
bool base64Decode(const String &input, uint8_t *output, size_t outputSize, size_t *decodedLength) {
  size_t inputLen = input.length();
  size_t expectedLen = (inputLen * 3) / 4;

  if (expectedLen > outputSize) {
    Serial.printf("Base64 decode: output buffer too small (%d needed, %d available)\n", expectedLen, outputSize);
    return false;
  }

  int ret = mbedtls_base64_decode(output, outputSize, decodedLength,
                                  (const unsigned char *)input.c_str(), inputLen);

  if (ret != 0) {
    Serial.printf("Base64 decode failed: %d\n", ret);
    return false;
  }

  return true;
}

// Verify firmware signature using RSA public key
bool verifyPackageSignature(uint8_t *packageData, size_t packageSize, const String &signatureBase64) {
  Serial.println("🛡️ Starting package signature verification...");

  mbedtls_pk_context pk;

  // Decode the signature from base64
  uint8_t signature[520];  // Buffer for RSA-4096 signatures
  size_t sigLength;

  if (!base64Decode(signatureBase64, signature, sizeof(signature), &sigLength)) {
    Serial.println("SECURITY: Failed to decode signature");
    return false;
  }

  if (sigLength != 512) {
    Serial.printf("SECURITY: Invalid signature length: %d (expected 512)\n", sigLength);
    return false;
  }

  // Hash the complete package using SHA-256
  uint8_t hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);

  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == NULL) {
    Serial.println("SECURITY: Failed to get SHA-256 info");
    mbedtls_md_free(&ctx);
    return false;
  }

  int ret = mbedtls_md_setup(&ctx, info, 0);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to setup MD context: %d\n", ret);
    mbedtls_md_free(&ctx);
    return false;
  }

  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, packageData, packageSize);
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  Serial.println("Package hash computed successfully");

  // Parse and verify with RSA public key
  mbedtls_pk_init(&pk);

  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)OTA_PUBLIC_KEY, strlen(OTA_PUBLIC_KEY) + 1);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to parse public key: -0x%04x\n", -ret);
    mbedtls_pk_free(&pk);
    return false;
  }

  // Verify the signature
  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sigLength);
  mbedtls_pk_free(&pk);

  if (ret == 0) {
    Serial.println("SECURITY: Package signature verification PASSED ✅");
    return true;
  } else {
    Serial.printf("SECURITY: Package signature verification FAILED ❌ (error -0x%04x)\n", -ret);
    return false;
  }
}


UpdateInfo parseUpdateResponse(const String &jsonResponse) {
  UpdateInfo info = { false, "", "", "", "", 0 };

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonResponse);

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return info;
  }

  if (doc["hasUpdate"].as<bool>()) {
    info.hasUpdate = true;
    info.version = doc["version"].as<String>();
    info.firmwareUrl = doc["firmwareUrl"].as<String>();
    info.signatureUrl = doc["signatureUrl"].as<String>();
    info.changelog = doc["changelog"].as<String>();
    info.firmwareSize = doc["firmwareSize"].as<size_t>();

    Serial.printf("🔄 UPDATE AVAILABLE: %s -> %s\n", FIRMWARE_VERSION, info.version.c_str());
    Serial.printf("📦 Firmware size: %d bytes\n", info.firmwareSize);
    Serial.printf("📝 Changelog: %s\n", info.changelog.c_str());
  }

  return info;
}

void checkForPendingManualUpdate() {

  if (currentMode == MODE_CONFIG || currentMode == MODE_AP) {
    return;  // Exit immediately if there's no chance of an internet connection anyway
  }

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("update_req", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    return;
  }

  uint8_t updateRequested = 0;
  size_t required_size = 64;
  char targetVersion[64] = { 0 };

  if (nvs_get_u8(nvs_handle, "update_flag", &updateRequested) == ESP_OK && updateRequested == 1) {
    if (nvs_get_str(nvs_handle, "target_ver", targetVersion, &required_size) == ESP_OK) {
      Serial.println("UPDATE: Found pending update for version: " + String(targetVersion));
      events.send("UPDATE: Found pending update for version: " + String(targetVersion), "console", millis());
      // Clear the flags BEFORE attempting (prevents boot loops if update fails)
      nvs_close(nvs_handle);
      nvs_handle_t clear_handle;
      if (nvs_open("update_req", NVS_READWRITE, &clear_handle) == ESP_OK) {
        nvs_erase_key(clear_handle, "update_flag");
        nvs_erase_key(clear_handle, "target_ver");
        nvs_erase_key(clear_handle, "wake_flag");
        nvs_commit(clear_handle);
        nvs_close(clear_handle);
      }

      performOTAUpdateToVersion(targetVersion);
      return;
    }
  }
  nvs_close(nvs_handle);
}

// Check for and perform OTA update
void checkForOTAUpdate() {
  // NEW (combine into single check):
  if (currentMode != MODE_CLIENT) {
    Serial.println("OTA: Skipping update check - only available in client mode");
    return;
  }
  // Skip if not connected to internet
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: Skipping update check - no WiFi connection");
    return;
  }
  Serial.println("🔍 === CHECKING FOR OTA UPDATES ===");
  Serial.printf("💾 Free heap: %d bytes\n", ESP.getFreeHeap());

  HTTPClient http;
  WiFiClientSecure client;

  // Use proper SSL validation (production security)
  client.setCACert(server_root_ca);

  // Request update information
  String url = String(OTA_SERVER_URL) + "/api/firmware/check.php";
  Serial.println("🌐 Connecting to: " + url);
  http.begin(client, url);

  // Add required headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "XRegulator/2.0");
  http.addHeader("Device-ID", getDeviceId());
  http.addHeader("Current-Version", FIRMWARE_VERSION);
  http.addHeader("Hardware-Version", "ESP32-WROOM-32");

  // Set timeout
  http.setTimeout(30000);  // 30 seconds

  int httpCode = http.GET();
  Serial.printf("📡 HTTP Response Code: %d\n", httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("📨 Server response: " + response);

    UpdateInfo updateInfo = parseUpdateResponse(response);

    if (updateInfo.hasUpdate) {
      Serial.println("🚀 === STARTING OTA UPDATE PROCESS ===");
      performOTAUpdate(updateInfo);
    } else {
      Serial.println("❌ No update information in response");
    }

  } else if (httpCode == 204) {
    Serial.println("✅ No updates available (HTTP 204)");
  } else if (httpCode == 429) {
    Serial.println("⏰ Rate limited - try again later (HTTP 429)");
  } else if (httpCode < 0) {
    Serial.printf("❌ HTTP connection failed: %s\n", http.errorToString(httpCode).c_str());
  } else {
    Serial.printf("❌ Update check failed with HTTP code: %d\n", httpCode);
    String response = http.getString();
    Serial.println("Error response: " + response);
  }

  http.end();
}

// Perform actual firmware download and install
// Initialize streaming extractor
bool initStreamingExtractor(StreamingExtractor *extractor) {
  memset(extractor, 0, sizeof(StreamingExtractor));

  extractor->inTarHeader = true;
  extractor->tarHeaderPos = 0;
  extractor->inPadding = false;
  extractor->paddingRemaining = 0;

  // Initialize hash for signature verification
  mbedtls_md_init(&extractor->hashCtx);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_setup(&extractor->hashCtx, info, 0);
  mbedtls_md_starts(&extractor->hashCtx);
  extractor->hashStarted = true;

  Serial.println("✅ Streaming tar extractor initialized");
  return true;
}

// Parse tar header to get file info
bool parseTarHeader(StreamingExtractor *extractor) {
  // Validate header first (but skip the all-zeros check since it's handled in processDataChunk)
  if (memcmp(extractor->tarHeader + 257, "ustar", 5) != 0) {
    Serial.println("❌ Invalid tar header: missing ustar magic");
    Serial.print("Header start: ");
    for (int i = 0; i < 20; i++) {
      Serial.printf("%02x ", extractor->tarHeader[i]);
    }
    Serial.println();
    return false;
  }

  Serial.println("✅ Valid tar header with ustar magic");

  // Extract filename (starts at offset 0, max 100 chars)
  char rawFilename[101];
  memcpy(rawFilename, extractor->tarHeader, 100);
  rawFilename[100] = '\0';

  // Check file type (position 156 in tar header)
  char typeFlag = extractor->tarHeader[156];

  // Clean filename
  String filename = String(rawFilename);
  filename.trim();

  // Remove leading "./" if present
  if (filename.startsWith("./")) {
    filename = filename.substring(2);
  }

  // Skip directories
  if (typeFlag == '5' || filename.endsWith("/")) {
    Serial.println("📁 Skipping directory entry");
    extractor->currentFileName = "";
    extractor->currentFileSize = 0;
    extractor->currentFilePos = 0;
    extractor->isCurrentFileFirmware = false;
    return true;
  }

  // Only process regular files
  if (typeFlag != '0' && typeFlag != '\0') {
    Serial.printf("⚠️  Skipping file type '%c' for: %s\n", typeFlag, filename.c_str());
    extractor->currentFileName = "";
    extractor->currentFileSize = 0;
    extractor->currentFilePos = 0;
    extractor->isCurrentFileFirmware = false;
    return true;
  }

  extractor->currentFileName = filename;

  // Extract file size (starts at offset 124, 12 chars, octal)
  char sizeStr[13];
  memcpy(sizeStr, extractor->tarHeader + 124, 12);
  sizeStr[12] = '\0';

  // Parse octal size manually
  extractor->currentFileSize = 0;
  for (int i = 0; i < 12 && sizeStr[i] != '\0' && sizeStr[i] != ' '; i++) {
    if (sizeStr[i] >= '0' && sizeStr[i] <= '7') {
      extractor->currentFileSize = extractor->currentFileSize * 8 + (sizeStr[i] - '0');
    }
  }

  extractor->currentFilePos = 0;

  // Check if this is the firmware file
  extractor->isCurrentFileFirmware = extractor->currentFileName.equals("firmware.bin");

  Serial.printf("📁 Found file: %s (%d bytes) type='%c'\n",
                extractor->currentFileName.c_str(), extractor->currentFileSize, typeFlag);

  // Route file to appropriate destination
  if (extractor->currentFileName.equals("firmware.bin")) {
    // Initialize OTA partition write
    extractor->otaPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    esp_ota_begin(extractor->otaPartition, extractor->currentFileSize, &extractor->otaHandle);
    extractor->otaStarted = true;
    extractor->isCurrentFileFirmware = true;
  } else if (extractor->currentFileName.indexOf('.') > 0) {
    // Mount prod_fs for web file updates
    if (!extractor->prodFSMounted) {
      webFS.end();  // End any existing mount
      if (webFS.begin(true, "/web", 10, "prod_fs")) {
        extractor->prodFSMounted = true;
        Serial.println("OTA: Mounted prod_fs for web file updates");
      } else {
        Serial.println("OTA: Failed to mount prod_fs");
        return false;
      }
    }
    String filePath = "/" + extractor->currentFileName;
    extractor->currentWebFile = webFS.open(filePath, "w");
  }
  return true;
}

// Process tar data chunk
bool processDataChunk(StreamingExtractor *extractor, uint8_t *data, size_t dataSize) {
  size_t processed = 0;

  while (processed < dataSize) {
    if (extractor->inPadding) {
      // Skip padding bytes
      size_t toSkip = min(extractor->paddingRemaining, dataSize - processed);
      processed += toSkip;
      extractor->paddingRemaining -= toSkip;

      if (extractor->paddingRemaining == 0) {
        extractor->inPadding = false;
        extractor->inTarHeader = true;
        extractor->tarHeaderPos = 0;
        Serial.println("✅ Padding skipped, ready for next header");
      }

    } else if (extractor->inTarHeader) {
      // Still reading tar header
      size_t headerRemaining = 512 - extractor->tarHeaderPos;
      size_t toCopy = min(headerRemaining, dataSize - processed);

      memcpy(extractor->tarHeader + extractor->tarHeaderPos, data + processed, toCopy);
      extractor->tarHeaderPos += toCopy;
      processed += toCopy;

      if (extractor->tarHeaderPos >= 512) {
        // Complete header received - check for end of archive
        bool allZeros = true;
        for (int i = 0; i < 512; i++) {
          if (extractor->tarHeader[i] != 0) {
            allZeros = false;
            break;
          }
        }

        if (allZeros) {
          Serial.println("✅ End of tar archive detected - extraction complete!");
          return true;  // SUCCESS! Archive is complete
        }

        // Not end of archive, parse the header
        if (!parseTarHeader(extractor)) {
          Serial.println("❌ Failed to parse tar header");
          return false;  // Actual parsing error
        }
        extractor->inTarHeader = false;
      }

    } else {
      // Reading file data
      size_t fileRemaining = extractor->currentFileSize - extractor->currentFilePos;
      size_t dataRemaining = dataSize - processed;
      size_t toWrite = min(fileRemaining, dataRemaining);

      if (toWrite > 0 && extractor->currentFileName.length() > 0) {
        // Only process if we have a valid filename (not a skipped entry)
        if (extractor->isCurrentFileFirmware && extractor->otaStarted) {
          // Write to firmware partition
          esp_err_t err = esp_ota_write(extractor->otaHandle, data + processed, toWrite);
          if (err != ESP_OK) {
            Serial.printf("❌ OTA write failed: %s\n", esp_err_to_name(err));
            return false;
          }

        } else if (extractor->currentWebFile) {
          // Write to web file
          size_t written = extractor->currentWebFile.write(data + processed, toWrite);
          if (written != toWrite) {
            Serial.printf("❌ Web file write failed: %d/%d bytes\n", written, toWrite);
          }
        }
      }

      if (toWrite > 0) {
        extractor->currentFilePos += toWrite;
        processed += toWrite;
      }

      // Check if current file is complete
      if (extractor->currentFilePos >= extractor->currentFileSize) {
        if (extractor->isCurrentFileFirmware && extractor->otaStarted) {
          Serial.println("✅ Firmware extraction completed");
        } else if (extractor->currentWebFile) {
          extractor->currentWebFile.close();
          Serial.printf("✅ Web file completed: %s (%d bytes)\n",
                        extractor->currentFileName.c_str(), extractor->currentFileSize);
        } else if (extractor->currentFileName.length() > 0) {
          Serial.printf("✅ Skipped file completed: %s\n", extractor->currentFileName.c_str());
        }

        // Calculate padding (files are padded to 512-byte boundaries)
        size_t padding = (512 - (extractor->currentFileSize % 512)) % 512;

        if (padding > 0) {
          Serial.printf("📏 File complete, need to skip %d padding bytes\n", padding);
          extractor->inPadding = true;
          extractor->paddingRemaining = padding;
        } else {
          Serial.println("📏 File complete, no padding needed");
          extractor->inTarHeader = true;
          extractor->tarHeaderPos = 0;
        }
      }
    }
  }

  return true;
}
void prepareForOTA() {
  Serial.println("Preparing system for OTA update...");

  // Log memory distribution
  Serial.print("Internal SRAM free: ");
  Serial.print(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.println(" bytes");

  Serial.print("PSRAM free: ");
  Serial.print(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.println(" bytes");

  Serial.print("DMA-capable free: ");
  Serial.print(heap_caps_get_free_size(MALLOC_CAP_DMA));
  Serial.println(" bytes");

  delay(100);
  esp_task_wdt_reset();
}

bool validateHeapIntegrity() {
  size_t free = ESP.getFreeHeap();
  size_t total = ESP.getHeapSize();

  // With PSRAM, total can be 8MB+
  if (free > total) {
    Serial.print("ERROR: Heap corruption detected (free=");
    Serial.print(free);
    Serial.print(" > total=");
    Serial.print(total);
    Serial.println(")");
    return false;
  }

  // Check for reasonable PSRAM presence
  size_t psramSize = ESP.getPsramSize();
  size_t freePsram = ESP.getFreePsram();

  Serial.print("Internal heap: ");
  Serial.print(free);
  Serial.print("/");
  Serial.print(total);
  Serial.println(" bytes free");

  Serial.print("PSRAM: ");
  Serial.print(freePsram);
  Serial.print("/");
  Serial.print(psramSize);
  Serial.println(" bytes free");

  Serial.print("Largest free block: ");
  Serial.print(ESP.getMaxAllocHeap());
  Serial.println(" bytes");

  return true;
}

void performStreamingOTAUpdate(const UpdateInfo &updateInfo, const String &signatureBase64) {
  //Called by: performOTAUpdate() when on factory partition
  //Purpose: Actually downloads and installs the firmware
  //Action:
  //1) Downloads firmware from updateInfo.firmwareUrl
  //2) Streams and extracts TAR file
  //3) Verifies signature
  // 4) Installs to OTA partition
  Serial.println("=== STARTING STREAMING TAR UPDATE ===");
  Serial.println("=== STREAMING OTA DEBUG ===");
  int wifiStatus = WiFi.status();
  Serial.print("1. WiFi status: ");
  Serial.println(wifiStatus);
  Serial.print("2. Current mode: ");
  Serial.println(currentMode);
  size_t freeHeap2 = ESP.getFreeHeap();
  Serial.print("3. Free heap2: ");
  Serial.print(freeHeap2);
  Serial.println(" bytes");
  Serial.print("4. URL length: ");
  Serial.println(updateInfo.firmwareUrl.length());
  Serial.print("5. Signature length: ");
  Serial.println(signatureBase64.length());

  // CRITICAL: Verify WiFi is connected before attempting SSL/HTTP operations
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: WiFi not connected - cannot perform OTA update");
    queueConsoleMessage("OTA FAILED: WiFi disconnected before update started");
    return;
  }

  // Additional safety check for current mode
  if (currentMode != MODE_CLIENT) {
    Serial.println("ERROR: Not in client mode - cannot perform OTA update");
    queueConsoleMessage("OTA FAILED: Must be in client mode for updates");
    return;
  }

  Serial.printf("WiFi status: %d, Signal: %d dBm\n", WiFi.status(), WiFi.RSSI());

  unsigned long downloadStartTime = millis();

  StreamingExtractor extractor;
  if (!initStreamingExtractor(&extractor)) {
    return;
  }

  Serial.println("6. Creating WiFiClientSecure...");
  WiFiClientSecure client;

  Serial.println("7. Setting CA cert...");
  client.setCACert(server_root_ca);

  // CRITICAL: Set matching timeouts BEFORE connection attempt
  client.setTimeout(30000);           // 30 seconds in milliseconds
  client.setHandshakeTimeout(30000);  // Must match setTimeout

  Serial.println("8. Pre-flight memory check...");
  size_t freeHeap = ESP.getFreeHeap();
  Serial.print("Free heap: ");
  Serial.print(freeHeap);
  Serial.println(" bytes");

  Serial.println("Less than 50kb is not great, worked at 46kb in the past!");

  Serial.println("9. Creating HTTPClient...");
  HTTPClient http;

  // Add watchdog reset before expensive SSL operation
  esp_task_wdt_reset();

  if (!validateHeapIntegrity()) {
    Serial.println("ABORT: Memory subsystem corrupted");
    return;
  }

  Serial.println("10. Attempting http.begin() with SSL...");
  bool beginSuccess = http.begin(client, updateInfo.firmwareUrl);

  if (!beginSuccess) {
    Serial.println("ABORT: http.begin() failed - SSL handshake error");
    queueConsoleMessage("OTA FAILED: Cannot establish secure connection");
    return;
  }

  Serial.println("11. SSL handshake completed successfully");

  // Now safe to add headers
  http.addHeader("Device-ID", getDeviceId());
  http.addHeader("Current-Version", FIRMWARE_VERSION);
  http.setTimeout(60000);

  // Continue with your existing download logic...
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("Package download failed: %d\n", httpCode);
    http.end();
    return;
  }

  int contentLength = http.getSize();
  WiFiClient *stream = http.getStreamPtr();

  // Streaming tar extraction
  const size_t CHUNK_SIZE = 1024;
  uint8_t inputBuffer[CHUNK_SIZE];

  int totalDownloaded = 0;
  unsigned long lastProgress = 0;
  bool success = true;

  while (totalDownloaded < contentLength && success) {
    if (!client.connected()) {
      Serial.println("Connection lost during download");
      success = false;
      break;
    }
    esp_task_wdt_reset();
    // Read chunk from network
    int available = stream->available();
    if (available > 0) {
      int toRead = min(available, (int)min(CHUNK_SIZE, (size_t)(contentLength - totalDownloaded)));
      int actualRead = stream->readBytes(inputBuffer, toRead);

      if (actualRead > 0) {
        totalDownloaded += actualRead;

        // Update hash for signature verification
        mbedtls_md_update(&extractor.hashCtx, inputBuffer, actualRead);

        // Process tar data directly (no decompression needed)
        if (!processDataChunk(&extractor, inputBuffer, actualRead)) {
          success = false;
          break;
        }

        // Progress indication
        if (millis() - lastProgress > 2000) {
          Serial.printf("Progress: %d%% (%d/%d bytes)\n",
                        (totalDownloaded * 100) / contentLength, totalDownloaded, contentLength);
          lastProgress = millis();
          esp_task_wdt_reset();
        }
      }
    } else {
      delay(10);
      esp_task_wdt_reset();
    }
  }

  http.end();

  unsigned long downloadDuration = (millis() - downloadStartTime) / 1000;
  Serial.printf("SPEED: Download completed in %lu seconds\n", downloadDuration);

  if (!success) {
    Serial.println("Streaming extraction failed");
    if (extractor.otaStarted) {
      esp_ota_abort(extractor.otaHandle);
    }
    goto cleanup;
  }

  Serial.println("Streaming download and extraction completed");

  // Verify signature - declare variables at top to avoid goto issues
  uint8_t hash[32];
  uint8_t signature[520];
  size_t sigLength;
  mbedtls_pk_context pk;
  int ret;

  mbedtls_md_finish(&extractor.hashCtx, hash);

  // Decode and verify signature (reuse existing verification function)
  if (!base64Decode(signatureBase64, signature, sizeof(signature), &sigLength)) {
    Serial.println("SECURITY: Failed to decode signature");
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    goto cleanup;
  }

  mbedtls_pk_init(&pk);
  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)OTA_PUBLIC_KEY, strlen(OTA_PUBLIC_KEY) + 1);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to parse public key: -0x%04x\n", -ret);
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    mbedtls_pk_free(&pk);
    goto cleanup;
  }

  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sigLength);
  mbedtls_pk_free(&pk);

  if (ret != 0) {
    Serial.printf("SECURITY: Signature verification FAILED (error -0x%04x)\n", -ret);
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    goto cleanup;
  }

  Serial.println("SECURITY: Signature verification PASSED");

  // Finalize OTA
  if (extractor.otaStarted) {
    esp_err_t err = esp_ota_end(extractor.otaHandle);
    if (err != ESP_OK) {
      Serial.printf("OTA end failed: %s\n", esp_err_to_name(err));
      goto cleanup;
    }

    err = esp_ota_set_boot_partition(extractor.otaPartition);
    if (err != ESP_OK) {
      Serial.printf("Set boot partition failed: %s\n", esp_err_to_name(err));
      goto cleanup;
    }
  }

  Serial.println("=== STREAMING OTA UPDATE SUCCESSFUL ===");
  Serial.printf("Updated from %s to %s\n", FIRMWARE_VERSION, updateInfo.version.c_str());
  Serial.println("Restarting in 3 seconds...");

cleanup:
  // Cleanup
  if (extractor.hashStarted) {
    mbedtls_md_free(&extractor.hashCtx);
  }
  if (extractor.currentWebFile) {
    extractor.currentWebFile.close();
  }
  if (extractor.prodFSMounted) {
    webFS.end();
  }

  if (success && extractor.otaStarted) {
    delay(3000);
    ESP.restart();
  }
}

void performOTAUpdate(const UpdateInfo &updateInfo) {
  //Purpose: Handles the partition switching and signature download
  //Action:
  //1) Downloads signature from server
  //2) Checks if running on ota_0 or factory partition
  //3) If on ota_0: switches to factory and reboots
  //4) If on factory: calls performStreamingOTAUpdate()
  Serial.println("🔒 === STARTING SECURE PACKAGE UPDATE ===");
  prepareForOTA();
  // Download signature first
  HTTPClient http;
  WiFiClientSecure client;
  client.setCACert(server_root_ca);

  Serial.printf("📥 Downloading signature from: %s\n", updateInfo.signatureUrl.c_str());
  http.begin(client, updateInfo.signatureUrl);
  http.addHeader("Device-ID", getDeviceId());
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("❌ Signature download failed: %d\n", httpCode);
    http.end();
    return;
  }

  String signatureBase64 = http.getString();
  http.end();
  signatureBase64.trim();
  Serial.printf("✅ Signature downloaded (%d chars)\n", signatureBase64.length());
  // Warn user before blocking operation
  Serial.println("UPDATE: Starting firmware download, web interface will be unresponsive");
  events.send("UPDATE: Downloading firmware - interface will freeze 60-90 sec", "console", millis());
  delay(3000);
  // Perform streaming update
  // Check if we need to restart to factory first
  const esp_partition_t *ota0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  const esp_partition_t *running_partition = esp_ota_get_running_partition();

  if (running_partition == ota0_partition) {
    Serial.println("OTA: Currently on ota_0, switching to factory for update...");
    events.send("OTA: Switching to factory partition for update", "console", millis());

    // Switch to factory partition and web files
    esp_ota_set_boot_partition(factory_partition);
    switchToFactoryWebFiles();

    Serial.println("OTA: Restarting to factory, then will download update to ota_0");
    events.send("OTA: Restarting to factory - device will reboot now", "console", millis());
    delay(3000);
    ESP.restart();
    // Execution stops here
  }

  // If we reach here, we're running from factory - proceed with update
  performStreamingOTAUpdate(updateInfo, signatureBase64);
}


void performOTAUpdateToVersion(const char *targetVersion) {
  //This performs an OTA update to specific version
  // Called by: The /get handler when UpdateToVersion parameter is received
  //Purpose: Initiates a targeted version update
  //Action:
  //1) Makes HTTP request to check.php
  //2) Sends Target-Version as a URL parameter
  //3) Calls performOTAUpdate() if version matches
  Serial.println();
  Serial.println();
  Serial.printf("🎯 === PERFORMING TARGETED OTA UPDATE TO %s ===\n", targetVersion);
  Serial.println("OTA: Starting targeted update to version " + String(targetVersion));
  events.send("OTA: Starting update to version " + String(targetVersion), "console", millis());

  // Skip if not connected to internet
  if (currentMode != MODE_CLIENT || WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: Cannot update - no WiFi connection");
    events.send("OTA: Cannot update - no WiFi connection", "console", millis());
    return;
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setCACert(server_root_ca);

  // Request specific version using Target-Version header (per your documentation)
  //String url = String(OTA_SERVER_URL) + "/api/firmware/check.php"; // OLD WRONG REMOVE LATER
  String url = String(OTA_SERVER_URL) + "/api/firmware/check.php?requestedVersion=" + String(targetVersion);
  Serial.println("🌐 Requesting specific version from: " + url);

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "XRegulator/2.0");
  http.addHeader("Device-ID", getDeviceId());
  http.addHeader("Current-Version", FIRMWARE_VERSION);
  //http.addHeader("Target-Version", targetVersion);  // This triggers server's targeted response DELETE LATER
  http.addHeader("Hardware-Version", "ESP32-WROOM-32");
  http.setTimeout(30000);

  int httpCode = http.GET();
  Serial.printf("📡 HTTP Response Code: %d\n", httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("📨 Server response: " + response);

    UpdateInfo updateInfo = parseUpdateResponse(response);

    if (updateInfo.hasUpdate && updateInfo.version == String(targetVersion)) {
      Serial.printf("=== STARTING TARGETED UPDATE TO %s ===\n", targetVersion);
      events.send("OTA: Beginning download of version " + String(targetVersion), "console", millis());
      performOTAUpdate(updateInfo);  // Use your existing OTA logic
    } else if (!updateInfo.hasUpdate) {
      Serial.println("OTA: Server says no update available for version " + String(targetVersion));
      events.send("OTA: Server says no update available for version " + String(targetVersion), "console", millis());
    } else {
      Serial.println("OTA: Version mismatch - requested " + String(targetVersion) + ", got " + updateInfo.version);
      events.send("OTA: Version mismatch - requested " + String(targetVersion) + ", got " + updateInfo.version, "console", millis());
    }

  } else if (httpCode == 404) {
    Serial.println("OTA: Version " + String(targetVersion) + " not found on server");
    events.send("OTA: Version " + String(targetVersion) + " not found on server", "console", millis());
  } else if (httpCode == 429) {
    Serial.println("OTA: Rate limited - try again later");
    events.send("OTA: Rate limited - try again later", "console", millis());
  } else if (httpCode < 0) {
    Serial.println("OTA: Connection failed - " + http.errorToString(httpCode));
    events.send("OTA: Connection failed - " + http.errorToString(httpCode), "console", millis());
  } else {
    Serial.println("OTA: Update request failed with HTTP code: " + String(httpCode));
    events.send("OTA: Update request failed with HTTP code: " + String(httpCode), "console", millis());
  }

  http.end();
}

// Validate tar header checksum and magic
bool isValidTarHeader(uint8_t *header) {
  // Check for all zeros (end of archive)
  bool allZeros = true;
  for (int i = 0; i < 512; i++) {
    if (header[i] != 0) {
      allZeros = false;
      break;
    }
  }
  if (allZeros) {
    Serial.println("📦 End of tar archive detected");
    return false;  // End of archive
  }

  // Check for ustar magic at offset 257
  if (memcmp(header + 257, "ustar", 5) != 0) {
    Serial.println("❌ Invalid tar header: missing ustar magic");
    // Print some header bytes for debugging
    Serial.print("Header start: ");
    for (int i = 0; i < 20; i++) {
      Serial.printf("%02x ", header[i]);
    }
    Serial.println();
    return false;
  }

  Serial.println("✅ Valid tar header with ustar magic");
  return true;
}

//DDELETE LATER
void printPartitionInfo() {
  Serial.println("=== PARTITION SUMMARY ===");

  // Get current running partition
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
    Serial.printf("🚀 RUNNING APP: %s - %d bytes (%.2f MB) at 0x%X\n",
                  running->label, running->size, running->size / 1024.0 / 1024.0, running->address);
  }

  // List ALL partitions in the table
  Serial.println("\n📋 ALL PARTITIONS IN TABLE:");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  int partitionCount = 0;

  while (it != NULL) {
    const esp_partition_t *partition = esp_partition_get(it);
    const char *type_str = (partition->type == ESP_PARTITION_TYPE_APP) ? "APP" : (partition->type == ESP_PARTITION_TYPE_DATA) ? "DATA"
                                                                                                                              : "OTHER";

    Serial.printf("  %s (%s) - %s: 0x%X -> 0x%X (%d bytes, %.2f MB)\n",
                  partition->label, type_str,
                  getSubtypeString(partition->type, partition->subtype).c_str(),
                  partition->address, partition->address + partition->size,
                  partition->size, partition->size / 1024.0 / 1024.0);
    partitionCount++;
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  Serial.printf("Total partitions found: %d\n", partitionCount);

  // Check specific expected partitions
  Serial.println("\n🔍 EXPECTED PARTITION VERIFICATION:");
  checkExpectedPartition("factory", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, 0x400000);
  checkExpectedPartition("ota_0", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x400000);
  checkExpectedPartition("factory_fs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x200000);
  checkExpectedPartition("prod_fs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x200000);
  checkExpectedPartition("userdata", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x3D0000);

  // OTA partition info
  Serial.println("\n🔄 OTA PARTITION STATUS:");
  const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
  if (ota_partition) {
    Serial.printf("Next OTA target: %s at 0x%X\n", ota_partition->label, ota_partition->address);
  }

  // Boot partition info
  const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
  if (boot_partition) {
    Serial.printf("Boot partition: %s at 0x%X\n", boot_partition->label, boot_partition->address);
  }

  // Flash and memory info
  Serial.println("\n💾 FLASH & MEMORY INFO:");
  Serial.printf("Flash size: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
  Serial.printf("Current sketch size: %d bytes (%.2f MB)\n",
                ESP.getSketchSize(), ESP.getSketchSize() / 1024.0 / 1024.0);
  Serial.printf("Free sketch space: %d bytes (%.2f MB)\n",
                ESP.getFreeSketchSpace(), ESP.getFreeSketchSpace() / 1024.0 / 1024.0);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("=== END PARTITION SUMMARY ===\n");
}
// Helper function to get human-readable subtype names
String getSubtypeString(esp_partition_type_t type, esp_partition_subtype_t subtype) {
  if (type == ESP_PARTITION_TYPE_APP) {
    switch (subtype) {
      case ESP_PARTITION_SUBTYPE_APP_FACTORY: return "factory";
      case ESP_PARTITION_SUBTYPE_APP_OTA_0: return "ota_0";
      case ESP_PARTITION_SUBTYPE_APP_OTA_1: return "ota_1";
      default: return "app_unknown";
    }
  } else if (type == ESP_PARTITION_TYPE_DATA) {
    switch (subtype) {
      case ESP_PARTITION_SUBTYPE_DATA_NVS: return "nvs";
      case ESP_PARTITION_SUBTYPE_DATA_OTA: return "otadata";
      case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS: return "littlefs";
      case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: return "spiffs";
      case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: return "coredump";
      default: return "data_unknown";
    }
  }
  return "unknown";
}
// Helper to verify expected partitions exist with correct sizes
void checkExpectedPartition(const char *name, esp_partition_type_t type, esp_partition_subtype_t subtype, size_t expectedSize) {
  const esp_partition_t *partition = esp_partition_find_first(type, subtype, name);
  if (partition) {
    bool sizeOK = (partition->size == expectedSize);
    Serial.printf("  ✅ %s: Found at 0x%X, size %d bytes %s\n",
                  name, partition->address, partition->size,
                  sizeOK ? "✓" : "❌ SIZE MISMATCH!");
  } else {
    Serial.printf("  ❌ %s: NOT FOUND!\n", name);
  }
}
