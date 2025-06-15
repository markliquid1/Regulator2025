/* 
 * AI_SUMMARY: Alternator Regulator Project - This file is the majority of the JS served by the ESP32
 * AI_PURPOSE: Realtime control of GPIO (settings always have echos) and viewing of sensor data and calculated values.  
 * AI_INPUTS: Payloads from the ESP32.  Server-Sent Events via /events endpoint
 * AI_OUTPUTS: Values submitted back to ESP32 via HTTP GET/POST requests to various endpoints
 * AI_DEPENDENCIES: 
 * AI_RISKS: Variable naming is inconsisent, need to be careful not to assume consistent patterns.   Unit conversion can be very confusing and propogate to many places, have to trace dependencies in variables FULLY to every end point
 * AI_OPTIMIZE: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat.  When impossible, always mimik my style and coding patterns.
 */


let rpmAmpsInitialized = false; // this is for the RPM/AMP speed based charging table
// for faster wifi reconnects after scheduled shut downs:
let reconnectInterval;
let reconnectionAttempts = 0;
const maxReconnectionAttempts = 15; // Try for 30 seconds (15 attempts × 2 seconds)
// Connection monitoring variables
let lastEventTime = Date.now();
const connectionCheckInterval = 10000; // Check every 10 seconds

// to prevent double toggling
let toggleStates = {}; // Track current toggle states
let userInitiatedToggles = new Set(); // Track user-initiated changes

//feedback on button clicking
function submitMessage() {
    // Use 'this' which refers to the element that called the function
    if (this && this.classList) {
        this.classList.add('button-pulse');
        setTimeout(() => this.classList.remove('button-pulse'), 600);
    }
}

//this allows user to turn the alternator OFF even without entering a passoword, for safety reasons
function handleAlternatorToggle(checkbox) {
    const isGoingOn = checkbox.checked; // true = turning ON, false = turning OFF
    const checkboxId = checkbox.id;
    const isLocked = !currentAdminPassword; // System is locked if no password

    // Always allow turning OFF (safety feature)
    if (!isGoingOn) {
        if (handleUserToggle(checkboxId, 'OnOff', 'OnOff')) {
            return true; // Allow form submission
        }
    }

    // For turning ON, require unlocked system
    if (isGoingOn && isLocked) {
        // alert("Settings must be unlocked to turn alternator ON");   // i find this intrusive
        checkbox.checked = false; // Revert the toggle
        return false; // Prevent form submission
    }

    // System is unlocked and we're turning ON, or we're turning OFF - proceed normally
    return handleUserToggle(checkboxId, 'OnOff', 'OnOff');
}


//something to do with passwords
function togglePassword(inputId) {
    const input = document.getElementById(inputId);
    const checkbox = event.target;
    input.type = checkbox.checked ? 'text' : 'password';
}
//password hiding stuff
function hideSettingsAccess() {
    console.log("Password accepted, hiding access section");
    document.getElementById('settings-access-section').style.display = 'none';
}

function showSettingsAccess() {
    document.getElementById('settings-access-section').style.display = 'block';
}


function lockSettingsManually() {
    const section = document.getElementById('settings-section');
    if (section) section.classList.add("locked");
    // DON'T lock the header control - we want to allow turning OFF the alternator
    // const headerControl = document.querySelector('.alternator-control');
    // if (headerControl) headerControl.classList.add("locked");
    showSettingsAccess();
    console.log("just reshowed settings access");
    const lockStatus = document.getElementById('lock-status');
    if (lockStatus) {
        lockStatus.textContent = "Settings are Locked";
        lockStatus.className = "lock-status-locked";
    }
    currentAdminPassword = "";
}
// Touch support: tap once to show tooltip, tap elsewhere to hide, for tooltips tool tip tooltip tips
document.addEventListener("click", function (e) {
    document.querySelectorAll(".tooltip").forEach(el => el.classList.remove("active"));
    const tip = e.target.closest(".tooltip");
    if (tip) {
        tip.classList.add("active");
        e.stopPropagation();
    }
});


// Reset reason lookup tables
const resetReasonLookup = {
    0: "Unknown",
    1: "Software reset (controlled)",
    2: "Woke from deep sleep",
    3: "External reset (button)",
    4: "Watchdog reset (code hung)",
    5: "Panic/Exception reset (crash)",
    6: "Brownout reset (power issue)",
    7: "Scheduled maintenance restart",
    8: "Other reset"
};

const wifiResetReasonLookup = {
    0: "Unknown WiFi disconnect",
    1: "WiFi signal lost",
    2: "WiFi password problem",
    3: "Can't connect to WiFi",
    4: "Other WiFi issue"
};

function debounce(fn, delay) {
    let timer = null;
    return function (...args) {
        clearTimeout(timer);
        timer = setTimeout(() => fn.apply(this, args), delay);
    };
}

let currentAdminPassword = "";

function updatePasswordFields() {
    const password = currentAdminPassword || document.getElementById("admin_password").value;
    const passwordFields = document.querySelectorAll('.password_field');
    passwordFields.forEach(field => {
        field.value = password;
    });
}

const fieldIndexes = {
    AlternatorTemperatureF: 0,
    DutyCycle: 1,
    BatteryV: 2,
    MeasuredAmps: 3,
    RPM: 4,
    Channel3V: 5,
    IBV: 6,
    Bcur: 7,
    VictronVoltage: 8,
    LoopTime: 9,
    WifiStrength: 10,
    WifiHeartBeat: 11,
    SendWifiTime: 12,
    AnalogReadTime: 13,
    VeTime: 14,
    MaximumLoopTime: 15,
    HeadingNMEA: 16,
    vvout: 17,
    iiout: 18,
    FreeHeap: 19,
    IBVMax: 20,
    MeasuredAmpsMax: 21,
    RPMMax: 22,
    SoC_percent: 23,
    EngineRunTime: 24,
    EngineCycles: 25,
    AlternatorOnTime: 26,
    AlternatorFuelUsed: 27,
    ChargedEnergy: 28,
    DischargedEnergy: 29,
    AlternatorChargedEnergy: 30,
    MaxAlternatorTemperatureF: 31,
    TemperatureLimitF: 32,
    FullChargeVoltage: 33,
    TargetAmps: 34,
    TargetFloatVoltage: 35,
    SwitchingFrequency: 36,
    interval: 37,
    FieldAdjustmentInterval: 38,
    ManualDuty: 39,
    SwitchControlOverride: 40,
    OnOff: 41,
    ManualFieldToggle: 42,
    HiLow: 43,
    LimpHome: 44,
    VeData: 45,
    NMEA0183Data: 46,
    NMEA2KData: 47,
    TargetAmpL: 48,
    CurrentThreshold: 49,
    PeukertExponent: 50,
    ChargeEfficiency: 51,
    ChargedVoltage: 52,
    TailCurrent: 53,
    ChargedDetectionTime: 54,
    IgnoreTemperature: 55,
    BMSLogic: 56,
    BMSLogicLevelOff: 57,
    AlarmActivate: 58,
    TempAlarm: 59,
    VoltageAlarmHigh: 60,
    VoltageAlarmLow: 61,
    CurrentAlarmHigh: 62,
    FourWay: 63,
    RPMScalingFactor: 64,
    ResetTemp: 65,
    ResetVoltage: 66,
    ResetCurrent: 67,
    ResetEngineRunTime: 68,
    ResetAlternatorOnTime: 69,
    ResetEnergy: 70,
    MaximumAllowedBatteryAmps: 71,
    ManualSOCPoint: 72,
    BatteryVoltageSource: 73,
    AmpControlByRPM: 74,
    RPM1: 75,
    RPM2: 76,
    RPM3: 77,
    RPM4: 78,
    Amps1: 79,
    Amps2: 80,
    Amps3: 81,
    Amps4: 82,
    RPM5: 83,
    RPM6: 84,
    RPM7: 85,
    Amps5: 86,
    Amps6: 87,
    Amps7: 88,
    ShuntResistanceMicroOhm: 89,
    InvertAltAmps: 90,
    InvertBattAmps: 91,
    MaxDuty: 92,
    MinDuty: 93,
    FieldResistance: 94,
    maxPoints: 95,
    AlternatorCOffset: 96,
    BatteryCOffset: 97,
    BatteryCapacity_Ah: 98,
    AmpSrc: 99,
    R_fixed: 100,
    Beta: 101,
    T0_C: 102,
    TempSource: 103,
    IgnitionOverride: 104,
    temperatureThermistor: 105,
    MaxTemperatureThermistor: 106,
    VictronCurrent: 107,
    AlarmTest: 108,
    AlarmLatchEnabled: 109,
    AlarmLatchState: 110,
    ResetAlarmLatch: 111,
    ForceFloat: 112,
    bulkCompleteTime: 113,
    FLOAT_DURATION: 114,
    LatitudeNMEA: 115,
    LongitudeNMEA: 116,
    SatelliteCountNMEA: 117,
    GPIO33_Status: 118,
    fieldActiveStatus: 119,
    timeToFullChargeMin: 120,
    timeToFullDischargeMin: 121,
    AutoShuntGainCorrection: 122, // boolean
    DynamicShuntGainFactor: 123, // actual factor
    AutoAltCurrentZero: 124,     // boolean
    DynamicAltCurrentZero: 125,  // actual factor
    InsulationLifePercent: 126,
    GreaseLifePercent: 127,
    BrushLifePercent: 128,
    PredictedLifeHours: 129,
    LifeIndicatorColor: 130,
    WindingTempOffset: 131,
    PulleyRatio: 132,
    LastSessionDuration: 133,
    LastSessionMaxLoopTime: 134,
    LastSessionMinHeap: 135,
    WifiReconnectsThisSession: 136,
    WifiReconnectsTotal: 137,
    CurrentSessionDuration: 138,
    CurrentWifiSessionDuration: 139,
    LastResetReason: 140,          // Integer code
    LastWifiResetReason: 141,       // Integer code
    ManualLifePercentage: 142,
    totalPowerCycles: 143,
    lastWifiSessionDuration: 144,
    lastSessionEndTime: 145,
    BatteryCurrentSource: 146,  // NEW
    InvertBatteryMonitorAmps: 147,  // NEW
    webgaugesinterval: 148,  // NEW
    plotTimeWindow: 149  // NEW

};

//Factory Reset Logic
function factoryReset() {
    if (!currentAdminPassword) {
        alert("You must be logged in to perform a factory reset.");
        return;
    }
    //Alarm Momentery Button Logic
    function handleAlarmTest() {
        const button = document.getElementById('alarm-test-btn');
        button.disabled = true;
        button.textContent = 'Testing...';
        button.style.backgroundColor = '#6c757d';
        setTimeout(() => {
            button.disabled = false;
            button.textContent = 'Test Buzzer (2s)';
            button.style.backgroundColor = '#555555';
        }, 3000);
        submitMessage();
        return true; // Allow form submission
    }
    function handleResetLatch() {
        const button = document.getElementById('reset-latch-btn');
        button.disabled = true;
        button.textContent = 'Resetting...';
        button.style.backgroundColor = '#6c757d';

        setTimeout(() => {
            button.disabled = false;
            button.textContent = 'Reset Latch';
            button.style.backgroundColor = '#555555';
        }, 2000);

        submitMessage();
        return true; // Allow form submission
    }

    const confirmation = confirm(
        "⚠️ FACTORY RESET WARNING ⚠️\n\n" +
        "This will restore settings to hardcoded defaults.\n" +
        "All your custom configurations will be lost.\n\n" +
        "Are you sure you want to continue?"
    );
    if (!confirmation) return;

    const button = document.getElementById('factory-reset-btn');
    button.disabled = true;
    button.textContent = 'Resetting...';
    button.style.backgroundColor = '#6c757d';

    const formData = new FormData();
    formData.append("password", currentAdminPassword);

    fetch("/factoryReset", {
        method: "POST",
        body: formData
    })
        .then(response => response.text())
        .then(text => {
            if (text.trim() === "OK") {
                alert("✅ Factory reset complete!\n\nPage will reload in 500ms to show default values.");
                setTimeout(() => {
                    window.location.reload();
                }, 500);
            } else {
                alert("❌ Factory reset failed: " + text);
                button.disabled = false;
                button.textContent = 'Restore Defaults';
                button.style.backgroundColor = '#555555';
            }
        })
        .catch(err => {
            alert("❌ Error during factory reset: " + err.message);
            button.disabled = false;
            button.textContent = 'Restore Defaults';
            button.style.backgroundColor = '#555555';
        });
}


function updateInlineStatus(isConnected) {
    const cornerStatus = document.getElementById('corner-status');

    if (cornerStatus) {
        if (isConnected) {
            cornerStatus.className = 'corner-status corner-status-connected';
            cornerStatus.textContent = 'WIFI CONNECTED';
        } else {
            cornerStatus.className = 'corner-status corner-status-disconnected';
            cornerStatus.textContent = 'WIFI DISCONNECTED';
        }
    }
}
// Current Plot Data
// [timestamps, battery current, alternator current, field current]
const currentTempData = [[], [], [], []];
let currentTempPlot;

// Voltage Plot Data
// [timestamps, ADS Battery V, Victron Battery V, INA Battery V]
const voltageData = [[], [], []];
let voltagePlot;

// RPM Plot Data
const rpmData = [[], []];
let rpmPlot;

let lastPlotUpdate = 0; // Timestamp of last time we updated the plot
const PLOT_UPDATE_INTERVAL_MS = 1000; // Only update every 2 seconds (adjust as needed)


//Reset buttons
// Single unified reset function
function resetParameter(parameterName) {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }

    updatePasswordFields();

    // Find the hidden input and button by parameter name
    const hiddenInput = document.getElementById(parameterName);
    const button = document.getElementById(parameterName + '_button');

    if (!hiddenInput || !button) {
        console.error(`Reset elements not found for parameter: ${parameterName}`);
        return;
    }

    hiddenInput.value = '1';
    button.closest('form').submit();
    submitMessage();
}




//Console
function clearConsole() {
    document.getElementById("consoleOutput").innerHTML = "";
}

//Mirror for GPIO33 Alarm
function updateAlarmStatus(data) {
    const alarmLed = document.getElementById('alarm-led');
    const alarmText = document.getElementById('alarm-status-text');

    if (!alarmLed || !alarmText) return;

    const isAlarming = data.GPIO33_Status === 1; // Mirror GPIO33 directly

    if (isAlarming) {
        // ALARMING: Red light and text
        alarmLed.style.backgroundColor = '#ff0000';
        alarmLed.style.boxShadow = '0 0 15px rgba(255, 0, 0, 0.8)';
        alarmText.textContent = 'Alarming!';
        alarmText.style.color = '#ff0000';
    } else {
        // SILENT: Green light and text (using your theme's green)
        alarmLed.style.backgroundColor = '#00a19a';
        alarmLed.style.boxShadow = '0 0 10px rgba(0, 161, 154, 0.5)';
        alarmText.textContent = 'Silent';
        alarmText.style.color = '#00a19a';
    }
}

//Graying when wifi disconnects
function markAllReadingsStale() {
    // Find all elements with the "reading" class (your sensor values)
    document.querySelectorAll('.reading span, .reading-value').forEach(element => {
        element.style.opacity = "0.4";
        element.style.color = "#999999";
        element.style.fontStyle = "italic";
    });
    console.log('All readings marked as stale due to data timeout');
}

function clearStaleMarkings() {
    document.querySelectorAll('.reading span, .reading-value').forEach(element => {
        element.style.opacity = "1.0";
        element.style.color = "var(--reading)";
        element.style.fontStyle = "normal";
    });
}



//Plots

function scheduleCurrentTempPlotUpdate() {
    if (!window.currentTempUpdateScheduled && currentTempPlot) {
        window.currentTempUpdateScheduled = true;
        requestAnimationFrame(() => {
            currentTempPlot.setData(currentTempData);
            window.currentTempUpdateScheduled = false;
        });
    }
}

function scheduleVoltagePlotUpdate() {
    if (!window.voltageUpdateScheduled && voltagePlot) {
        window.voltageUpdateScheduled = true;
        requestAnimationFrame(() => {
            voltagePlot.setData(voltageData);
            window.voltageUpdateScheduled = false;
        });
    }
}

function scheduleRPMPlotUpdate() {
    if (!window.rpmUpdateScheduled && rpmPlot) {
        window.rpmUpdateScheduled = true;
        requestAnimationFrame(() => {
            rpmPlot.setData(rpmData);
            window.rpmUpdateScheduled = false;
        });
    }
}

function scheduleTemperaturePlotUpdate() {
    if (!window.temperatureUpdateScheduled && temperaturePlot) {
        window.temperatureUpdateScheduled = true;
        requestAnimationFrame(() => {
            temperaturePlot.setData(temperatureData);
            window.temperatureUpdateScheduled = false;
        });
    }
}



function initCurrentTempPlot() {
    const plotEl = document.getElementById('current-temp-plot');
    if (!plotEl) {
        console.error("Current & Temperature plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "Current History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 }, // timestamp - hidden
            {
                label: "Battery Current (A)",
                stroke: "#4CAF50",
                width: 2,
                scale: "current"
            },
            {
                label: "Alternator Current (A)",
                stroke: "#2196F3",
                width: 2,
                scale: "current"
            },
            {
                label: "Field Current (A)",
                stroke: "#9C27B0",
                width: 2,
                scale: "current"
            }
        ],
        axes: [
            {},
            {
                scale: "current",
                label: "Amperes",
                grid: { show: true },
                side: 3
            }
        ],
        scales: {
            x: { time: true },
            current: { auto: true }
        },
        // DISABLE the built-in legend completely
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        // Create custom legend after plot is created
                        createCustomLegend('current-temp-plot', [
                            { label: "Battery Current (A)", color: "#4CAF50" },
                            { label: "Alternator Current (A)", color: "#2196F3" },
                            { label: "Field Current (A)", color: "#9C27B0" }
                        ]);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("current-temp-plot");
                            if (plotEl && currentTempPlot) {
                                currentTempPlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);
                        new ResizeObserver(resizePlot).observe(plotEl);
                    }
                ]
            }
        }]
    };

    currentTempPlot = new uPlot(opts, currentTempData, plotEl);
    const startTime = Math.floor(Date.now() / 1000);
    currentTempData[0].push(startTime);
    currentTempData[1].push(0);
    currentTempData[2].push(0);
    currentTempData[3].push(0);
    currentTempPlot.setData(currentTempData);
}

function initVoltagePlot() {
    const plotEl = document.getElementById('voltage-plot');
    if (!plotEl) {
        console.error("Voltage plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "Battery Voltage History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 }, // timestamp - hidden
            {
                label: "ADS Battery (V)",
                stroke: "#FF9800",
                width: 2,
                scale: "voltage"
            },
            {
                label: "INA Battery (V)",
                stroke: "#607D8B",
                width: 2,
                scale: "voltage"
            }
        ],
        axes: [
            {},
            {
                scale: "voltage",
                label: "Volts",
                grid: { show: true }
            }
        ],
        scales: {
            x: { time: true },
            voltage: { auto: true }
        },
        // DISABLE the built-in legend completely
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        // Create custom legend after plot is created
                        createCustomLegend('voltage-plot', [
                            { label: "ADS Battery (V)", color: "#FF9800" },
                            { label: "INA Battery (V)", color: "#607D8B" }
                        ]);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("voltage-plot");
                            if (plotEl && voltagePlot) {
                                voltagePlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);
                        new ResizeObserver(resizePlot).observe(plotEl);
                    }
                ]
            }
        }]
    };

    voltagePlot = new uPlot(opts, voltageData, plotEl);
    const startTime = Math.floor(Date.now() / 1000);
    voltageData[0].push(startTime);
    voltageData[1].push(0);
    voltageData[2].push(0);
    voltagePlot.setData(voltageData);
}

function initRPMPlot() {
    const plotEl = document.getElementById('rpm-plot');
    if (!plotEl) {
        console.error("RPM plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "RPM History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 }, // timestamp - hidden
            {
                label: "RPM",
                stroke: "#E91E63",
                width: 2,
                scale: "rpm"
            }
        ],
        axes: [
            {},
            {
                scale: "rpm",
                label: "rev/min",
                grid: { show: true }
            }
        ],
        scales: {
            x: { time: true },
            rpm: { auto: true }
        },
        // DISABLE the built-in legend completely
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        // Create custom legend after plot is created
                        createCustomLegend('rpm-plot', [
                            { label: "RPM", color: "#E91E63" }
                        ]);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("rpm-plot");
                            if (plotEl && rpmPlot) {
                                rpmPlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);
                        new ResizeObserver(resizePlot).observe(plotEl);
                    }
                ]
            }
        }]
    };

    rpmPlot = new uPlot(opts, rpmData, plotEl);
    const startTime = Math.floor(Date.now() / 1000);
    rpmData[0].push(startTime);
    rpmData[1].push(0);
    rpmPlot.setData(rpmData);
}


// Temperature Plot Data
const temperatureData = [[], []];
let temperaturePlot;

function initTemperaturePlot() {
    const plotEl = document.getElementById('temperature-plot');
    if (!plotEl) {
        console.error("Temperature plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "Alternator Temperature History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 }, // timestamp - hidden
            {
                label: "Alt. Temp (°F)",
                stroke: "#FF5722",
                width: 2,
                scale: "temperature"
            }
        ],
        axes: [
            {},
            {
                scale: "temperature",
                label: "°Fahrenheit",
                grid: { show: true }
            }
        ],
        scales: {
            x: { time: true },
            temperature: { auto: true }
        },
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createCustomLegend('temperature-plot', [
                            { label: "Alt. Temp (°F)", color: "#FF5722" }
                        ]);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("temperature-plot");
                            if (plotEl && temperaturePlot) {
                                temperaturePlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);
                        new ResizeObserver(resizePlot).observe(plotEl);
                    }
                ]
            }
        }]
    };

    temperaturePlot = new uPlot(opts, temperatureData, plotEl);
    const startTime = Math.floor(Date.now() / 1000);
    temperatureData[0].push(startTime);
    temperatureData[1].push(75);
    temperaturePlot.setData(temperatureData);
}

// Custom legend creation function
function createCustomLegend(plotId, legendItems) {
    const plotContainer = document.getElementById(plotId);
    if (!plotContainer) return;

    // Remove any existing custom legend
    const existingLegend = plotContainer.querySelector('.custom-legend');
    if (existingLegend) {
        existingLegend.remove();
    }

    // Create new custom legend
    const legendDiv = document.createElement('div');
    legendDiv.className = 'custom-legend';
    legendDiv.style.cssText = `
display: flex;
justify-content: center;
gap: 15px;
margin-top: 10px;
flex-wrap: wrap;
`;

    legendItems.forEach(item => {
        const legendItem = document.createElement('div');
        legendItem.style.cssText = `
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
`;

        const colorBox = document.createElement('div');
        colorBox.style.cssText = `
  width: 16px;
  height: 3px;
  background-color: ${item.color};
  border-radius: 1px;
`;

        const label = document.createElement('span');
        label.textContent = item.label;
        label.style.color = 'var(--text-dark)';

        legendItem.appendChild(colorBox);
        legendItem.appendChild(label);
        legendDiv.appendChild(legendItem);
    });

    plotContainer.appendChild(legendDiv);
}


function setAdminPassword() {
    const button = document.getElementById('admin_password_set');
    const input = document.getElementById('admin_password');
    const msg = document.getElementById('admin_password_msg');
    const password = input.value;

    if (!password) {
        msg.textContent = "Missing Password";
        msg.style.color = "#00a19a";
        setTimeout(() => { msg.textContent = ""; }, 2000);
        return false;
    }

    button.disabled = true;

    const formData = new FormData();
    formData.append("password", password);

    fetch("/checkPassword", {
        method: "POST",
        body: formData
    })
        .then(response => response.text())
        .then(text => {
            if (text.trim() === "OK") {

                currentAdminPassword = password;
                // Unlock settings
                const section = document.getElementById('settings-section');
                if (section) section.classList.remove("locked");
                document.querySelector('.permanent-header').classList.remove('locked');  // remove this bullshit probably

                // Also unlock header controls
                const headerControl = document.querySelector('.alternator-control');
                if (headerControl) headerControl.classList.remove("locked");

                const lockStatus = document.getElementById('lock-status');
                if (lockStatus) {
                    lockStatus.textContent = "Settings are Unlocked";
                    lockStatus.className = "lock-status-unlocked";
                }

                // Show password change and lock sections
                // document.getElementById('change-password-section').style.display = 'block';
                //document.getElementById('lock-button-section').style.display = 'block';

                updatePasswordFields();
                msg.textContent = "Password Accepted";
                msg.style.color = "#00a19a";

                hideSettingsAccess();
            }


            else {
                msg.textContent = "Wrong Password";
                msg.style.color = "#00a19a";
            }
            input.value = "";
            setTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        })
        .catch(err => {
            msg.textContent = "Error";
            msg.style.color = "#00a19a";
            setTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        });

    return false;
}

function setNewPassword() {
    const button = document.getElementById('newpassword_set');
    const input = document.getElementById('newpassword');
    const msg = document.getElementById('newpassword_msg');
    const currentPassword = currentAdminPassword;

    if (!currentPassword || !input.value) {
        msg.textContent = "Missing fields";
        msg.style.color = "#00a19a";
        setTimeout(() => { msg.textContent = ""; }, 2000);
        return false;
    }

    button.disabled = true;

    const formData = new FormData();
    formData.append("password", currentPassword);
    formData.append("newpassword", input.value);

    fetch("/setPassword", {
        method: "POST",
        body: formData
    })
        .then(response => response.text())
        .then(text => {
            if (text.trim() === "OK") {
                msg.textContent = "Password Changed";
                msg.style.color = "#00a19a";
            } else {
                msg.textContent = "Wrong Password";
                msg.style.color = "#00a19a";
            }
            input.value = "";
            setTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        })
        .catch(err => {
            msg.textContent = "Error";
            msg.style.color = "#00a19a";
            setTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        });

    return false; // prevent form submit
}


//prevent double toggling here
function updateTogglesFromData(data) {
    // Add periodic cleanup at the start
    if (userInitiatedToggles.size > 10) { // If too many pending
        userInitiatedToggles.clear();
        console.log("Cleared stale toggle states");
    }
    try {
        if (!data) return;

        const updateCheckbox = (id, value, dataKey) => {
            if (value === undefined) return;

            const checkbox = document.getElementById(id);
            if (!checkbox) return;

            const shouldBeChecked = (value === 1);

            // Skip update if this was a user-initiated change
            if (userInitiatedToggles.has(dataKey)) {
                userInitiatedToggles.delete(dataKey);
                return;
            }

            // Only update if state actually changed
            if (checkbox.checked !== shouldBeChecked) {
                checkbox.checked = shouldBeChecked;
                toggleStates[dataKey] = value;
            }
        };

        // Update all toggle checkboxes with their data keys
        updateCheckbox("ManualFieldToggle_checkbox", data.ManualFieldToggle, "ManualFieldToggle");
        updateCheckbox("SwitchControlOverride_checkbox", data.SwitchControlOverride, "SwitchControlOverride");
        //updateCheckbox("OnOff_checkbox", data.OnOff, "OnOff");
        updateCheckbox("header-alternator-enable", data.OnOff, "OnOff"); /// this one is for the banner aka header
        updateCheckbox("LimpHome_checkbox", data.LimpHome, "LimpHome");
        updateCheckbox("HiLow_checkbox", data.HiLow, "HiLow");
        updateCheckbox("VeData_checkbox", data.VeData, "VeData");
        updateCheckbox("NMEA0183Data_checkbox", data.NMEA0183Data, "NMEA0183Data");
        updateCheckbox("NMEA2KData_checkbox", data.NMEA2KData, "NMEA2KData");
        updateCheckbox("IgnoreTemperature_checkbox", data.IgnoreTemperature, "IgnoreTemperature");
        updateCheckbox("BMSLogic_checkbox", data.BMSLogic, "BMSLogic");
        updateCheckbox("BMSLogicLevelOff_checkbox", data.BMSLogicLevelOff, "BMSLogicLevelOff");
        updateCheckbox("AlarmActivate_checkbox", data.AlarmActivate, "AlarmActivate");
        updateCheckbox("AmpControlByRPM_checkbox", data.AmpControlByRPM, "AmpControlByRPM");
        updateCheckbox("InvertAltAmps_checkbox", data.InvertAltAmps, "InvertAltAmps");
        updateCheckbox("InvertBattAmps_checkbox", data.InvertBattAmps, "InvertBattAmps");
        updateCheckbox("IgnitionOverride_checkbox", data.IgnitionOverride, "IgnitionOverride");
        updateCheckbox("TempSource_checkbox", data.TempSource, "TempSource");
        updateCheckbox("AlarmLatchEnabled_checkbox", data.AlarmLatchEnabled, "AlarmLatchEnabled");
        updateCheckbox("ForceFloat_checkbox", data.ForceFloat, "ForceFloat");
        updateCheckbox("AutoShuntGainCorrection_checkbox", data.AutoShuntGainCorrection, "AutoShuntGainCorrection");
        updateCheckbox("AutoAltCurrentZero_checkbox", data.AutoAltCurrentZero, "AutoAltCurrentZero");
        updateCheckbox("InvertBatteryMonitorAmps_checkbox", data.InvertBatteryMonitorAmps, "InvertBatteryMonitorAmps");

    } catch (e) {
        console.log("Error updating toggle states: " + e.message);
    }
}

//function to handle user toggle changes without double toggle
function handleUserToggle(checkboxId, hiddenInputId, dataKey) {
    const checkbox = document.getElementById(checkboxId);
    const hiddenInput = document.getElementById(hiddenInputId);

    if (checkbox && hiddenInput) {
        const newValue = checkbox.checked ? '1' : '0';
        hiddenInput.value = newValue;

        // Mark this as user-initiated to prevent server override
        userInitiatedToggles.add(dataKey);

        // Store the new state
        toggleStates[dataKey] = parseInt(newValue);

        // Clear the flag after a reasonable delay (longer than your server response time)
        setTimeout(() => {
            userInitiatedToggles.delete(dataKey);
        }, 5000); // Increased timeout

        return true; // Allow form submission
    }
    return false;
}

window.addEventListener("load", function () {     //AM I IN
    //give initial values that will cause graying until proven otherwise
    window.sensorAges = {
        heading: 999999,
        latitude: 999999,
        longitude: 999999,
        satellites: 999999,
        victronVoltage: 999999,
        victronCurrent: 999999,
        alternatorTemp: 999999,
        thermistorTemp: 999999,
        rpm: 999999,
        measuredAmps: 999999,
        batteryV: 999999,
        ibv: 999999,
        bcur: 999999,
        channel3V: 999999,
        dutyCycle: 999999,
        fieldVolts: 999999,
        fieldAmps: 999999
    };

    document.getElementById("AlarmLatchEnabled_checkbox").checked = (document.getElementById("AlarmLatchEnabled").value === "1");
    document.getElementById("ForceFloat_checkbox").checked = (document.getElementById("ForceFloat").value === "1");


    //gray out Settings on a reload
    const settingsSection = document.getElementById('settings-section');
    if (settingsSection) {
        settingsSection.classList.add("locked");
    }
    const lockStatus = document.getElementById('lock-status');
    if (lockStatus) {
        lockStatus.textContent = "Settings are Locked";
        lockStatus.className = "lock-status-locked";
    }
    if (localStorage.getItem('darkMode') === '1') {
        document.body.classList.add('dark-mode');
        const darkToggle = document.getElementById("DarkMode_checkbox");
        if (darkToggle) darkToggle.checked = true;
    }

    showMainTab('livedata'); // loads the first tab instead of it being blank

    initCurrentTempPlot();
    initVoltagePlot();
    initRPMPlot();
    initTemperaturePlot();

    if (!!window.EventSource) {
        const telemetryFields = [
            //Telemetry: Live sensor data that changes frequently
            ["AltTempID", "AlternatorTemperatureF"],         // 0
            ["DutyCycleID", "DutyCycle"],                    // 1
            ["BatteryVID", "BatteryV"],                      // 2
            ["MeasAmpsID", "MeasuredAmps"],                  // 3
            ["RPMID", "RPM"],                                // 4
            ["ADS3ID", "Channel3V"],                         // 5
            ["IBVID", "IBV"],                                // 6
            ["BCurrID", "Bcur"],                             // 7
            ["VictronVoltageID", "VictronVoltage"],          // 8
            ["LoopTimeID", "LoopTime"],                      // 9
            ["WifiStrengthID", "WifiStrength"],              // 10
            ["WifiHeartBeatID", "WifiHeartBeat"],            // 11
            ["SendWifiTimeID", "SendWifiTime"],              // 12
            ["AnalogReadTimeID", "AnalogReadTime"],          // 13
            ["VeTimeID", "VeTime"],                          // 14
            ["MaximumLoopTimeID", "MaximumLoopTime"],        // 15
            ["GPSHID", "HeadingNMEA"],                       // 16
            ["FieldVoltsID", "vvout"],                       // 17
            ["FieldAmpsID", "iiout"],                        // 18
            ["FreeHeapID", "FreeHeap"],                      // 19
            ["IBVMaxID", "IBVMax"],                          // 20
            ["MeasuredAmpsMaxID", "MeasuredAmpsMax"],        // 21
            ["RPMMaxID", "RPMMax"],                          // 22
            ["SoC_percentID", "SoC_percent"],                // 23
            ["EngineRunTimeID", "EngineRunTime"],            // 24
            ["EngineCyclesID", "EngineCycles"],              // 25
            ["AlternatorOnTimeID", "AlternatorOnTime"],      // 26
            ["AlternatorFuelUsedID", "AlternatorFuelUsed"],  // 27
            ["ChargedEnergyID", "ChargedEnergy"],            // 28
            ["DischargedEnergyID", "DischargedEnergy"],      // 29
            ["AlternatorChargedEnergyID", "AlternatorChargedEnergy"], // 30
            ["MaxAlternatorTemperatureF_ID", "MaxAlternatorTemperatureF"], // 31
            ["temperatureThermistorID", "temperatureThermistor"], //32
            ["MaxTemperatureThermistorID", "MaxTemperatureThermistor"], //33
            ["VictronCurrentID", "VictronCurrent"],          // 34
            ["LatitudeNMEA_ID", "LatitudeNMEA"],     // 115
            ["LongitudeNMEA_ID", "LongitudeNMEA"],   // 116  
            ["SatelliteCountNMEA_ID", "SatelliteCountNMEA"], // 117
            ["header-voltage", "IBV"],                       // Add this
            ["header-soc", "SoC_percent"],                   // Add this  
            ["header-alt-current", "MeasuredAmps"],          // Add this
            ["header-batt-current", "Bcur"],                 // Add this
            ["header-alt-temp", "AlternatorTemperatureF"],   // Add this
            ["header-rpm", "RPM"],                          // Add this
            ["timeToFullChargeMinID", "timeToFullChargeMin"],
            ["timeToFullDischargeMinID", "timeToFullDischargeMin"],
            ["DynamicShuntGainFactorID", "DynamicShuntGainFactor"],
            ["DynamicAltCurrentZeroID", "DynamicAltCurrentZero"],
            ["InsulationLifePercentID", "InsulationLifePercent"],
            ["GreaseLifePercentID", "GreaseLifePercent"],
            ["BrushLifePercentID", "BrushLifePercent"],
            ["PredictedLifeHoursID", "PredictedLifeHours"],
            ["LastSessionDurationID", "LastSessionDuration"],
            ["LastSessionMaxLoopTimeID", "LastSessionMaxLoopTime"],
            ["LastSessionMinHeapID", "LastSessionMinHeap"],
            ["WifiReconnectsThisSessionID", "WifiReconnectsThisSession"],
            ["WifiReconnectsTotalID", "WifiReconnectsTotal"],
            ["CurrentSessionDurationID", "CurrentSessionDuration"],
            ["CurrentWifiSessionDurationID", "CurrentWifiSessionDuration"],
            ["LastResetReasonID", "LastResetReason"],
            ["LastWifiResetReasonID", "LastWifiResetReason"],
            ["totalPowerCyclesID", "totalPowerCycles"]

        ];

        // this is the numerical displays, with scaling done locally
        const source = new EventSource('/events');


        //Console
        source.addEventListener("console", function (event) {
            const timestamp = new Date().toLocaleTimeString();
            const consoleDiv = document.getElementById("consoleOutput");
            const line = document.createElement("div");
            line.textContent = `[${timestamp}] ${event.data}`;
            consoleDiv.appendChild(line);
            consoleDiv.scrollTop = consoleDiv.scrollHeight;
            while (consoleDiv.children.length > 100) {
                consoleDiv.removeChild(consoleDiv.firstChild);
            }
        });

        source.addEventListener('open', function () {
            console.log('EventSource connected successfully');
            updateInlineStatus(true);

            // Reset reconnection state on successful connection
            if (reconnectInterval) {
                clearInterval(reconnectInterval);
                reconnectInterval = null;
            }
            reconnectionAttempts = 0;
        }, false);

        source.addEventListener('error', function () {
            console.log('EventSource error - showing recovery options');
            updateInlineStatus(false);
            source.close();
            showRecoveryOptions();
        }, false);

        // Initial status - set to connected if we made it this far
        updateInlineStatus(true);

        setInterval(function () {
            const timeSinceLastEvent = Date.now() - lastEventTime;
            if (timeSinceLastEvent > 5000) { // 5 seconds without data = disconnected
                updateInlineStatus(false);
                markAllReadingsStale(); //gray out
            }
        }, 2000); // Check every 2 seconds
        source.addEventListener('CSVData', function (e) {
            // Update timestamp when data is received
            lastEventTime = Date.now();

            // Immediately mark as connected when data arrives
            updateInlineStatus(true);
            clearStaleMarkings(); // ungray
            // Parse CSV data into an array
            const values = e.data.split(',').map(Number);

            if (values.length !== Object.keys(fieldIndexes).length) {
                console.warn("CSV mismatch: ESP32/HTML field count diverged");
                return;
            }
            const data = {
                AlternatorTemperatureF: values[fieldIndexes.AlternatorTemperatureF], // 0
                DutyCycle: values[fieldIndexes.DutyCycle],
                BatteryV: values[fieldIndexes.BatteryV], // 2
                MeasuredAmps: values[fieldIndexes.MeasuredAmps],
                RPM: values[fieldIndexes.RPM], // 4
                Channel3V: values[fieldIndexes.Channel3V],
                IBV: values[fieldIndexes.IBV], // 6
                Bcur: values[fieldIndexes.Bcur],
                VictronVoltage: values[fieldIndexes.VictronVoltage], // 8
                LoopTime: values[fieldIndexes.LoopTime], //9
                WifiStrength: values[fieldIndexes.WifiStrength], // 10
                WifiHeartBeat: values[fieldIndexes.WifiHeartBeat],
                SendWifiTime: values[fieldIndexes.SendWifiTime], // 12
                AnalogReadTime: values[fieldIndexes.AnalogReadTime],
                VeTime: values[fieldIndexes.VeTime], // 14
                MaximumLoopTime: values[fieldIndexes.MaximumLoopTime],
                HeadingNMEA: values[fieldIndexes.HeadingNMEA], // 16
                vvout: values[fieldIndexes.vvout],
                iiout: values[fieldIndexes.iiout], // 18
                FreeHeap: values[fieldIndexes.FreeHeap],
                IBVMax: values[fieldIndexes.IBVMax], // 20
                MeasuredAmpsMax: values[fieldIndexes.MeasuredAmpsMax],
                RPMMax: values[fieldIndexes.RPMMax], // 22
                SoC_percent: values[fieldIndexes.SoC_percent],
                EngineRunTime: values[fieldIndexes.EngineRunTime],
                EngineCycles: values[fieldIndexes.EngineCycles],
                AlternatorOnTime: values[fieldIndexes.AlternatorOnTime],
                AlternatorFuelUsed: values[fieldIndexes.AlternatorFuelUsed],
                ChargedEnergy: values[fieldIndexes.ChargedEnergy],
                DischargedEnergy: values[fieldIndexes.DischargedEnergy],
                AlternatorChargedEnergy: values[fieldIndexes.AlternatorChargedEnergy],
                MaxAlternatorTemperatureF: values[fieldIndexes.MaxAlternatorTemperatureF],
                TemperatureLimitF: values[fieldIndexes.TemperatureLimitF],
                FullChargeVoltage: values[fieldIndexes.FullChargeVoltage],
                TargetAmps: values[fieldIndexes.TargetAmps],
                TargetFloatVoltage: values[fieldIndexes.TargetFloatVoltage],
                SwitchingFrequency: values[fieldIndexes.SwitchingFrequency],
                interval: values[fieldIndexes.interval],
                FieldAdjustmentInterval: values[fieldIndexes.FieldAdjustmentInterval],
                ManualDuty: values[fieldIndexes.ManualDuty],
                SwitchControlOverride: values[fieldIndexes.SwitchControlOverride],
                OnOff: values[fieldIndexes.OnOff],
                ManualFieldToggle: values[fieldIndexes.ManualFieldToggle],
                HiLow: values[fieldIndexes.HiLow],
                LimpHome: values[fieldIndexes.LimpHome],
                VeData: values[fieldIndexes.VeData],
                NMEA0183Data: values[fieldIndexes.NMEA0183Data],
                NMEA2KData: values[fieldIndexes.NMEA2KData],
                TargetAmpL: values[fieldIndexes.TargetAmpL],
                CurrentThreshold: values[fieldIndexes.CurrentThreshold],
                PeukertExponent: values[fieldIndexes.PeukertExponent],
                ChargeEfficiency: values[fieldIndexes.ChargeEfficiency],
                ChargedVoltage: values[fieldIndexes.ChargedVoltage],
                TailCurrent: values[fieldIndexes.TailCurrent],
                ChargedDetectionTime: values[fieldIndexes.ChargedDetectionTime],
                IgnoreTemperature: values[fieldIndexes.IgnoreTemperature],
                BMSLogic: values[fieldIndexes.BMSLogic],
                BMSLogicLevelOff: values[fieldIndexes.BMSLogicLevelOff],
                AlarmActivate: values[fieldIndexes.AlarmActivate],
                TempAlarm: values[fieldIndexes.TempAlarm],
                VoltageAlarmHigh: values[fieldIndexes.VoltageAlarmHigh],
                VoltageAlarmLow: values[fieldIndexes.VoltageAlarmLow],
                CurrentAlarmHigh: values[fieldIndexes.CurrentAlarmHigh],
                FourWay: values[fieldIndexes.FourWay],
                RPMScalingFactor: values[fieldIndexes.RPMScalingFactor],
                ResetTemp: values[fieldIndexes.ResetTemp],
                ResetVoltage: values[fieldIndexes.ResetVoltage],
                ResetCurrent: values[fieldIndexes.ResetCurrent],
                ResetEngineRunTime: values[fieldIndexes.ResetEngineRunTime],
                ResetAlternatorOnTime: values[fieldIndexes.ResetAlternatorOnTime],
                ResetEnergy: values[fieldIndexes.ResetEnergy],
                MaximumAllowedBatteryAmps: values[fieldIndexes.MaximumAllowedBatteryAmps],
                ManualSOCPoint: values[fieldIndexes.ManualSOCPoint],
                BatteryVoltageSource: values[fieldIndexes.BatteryVoltageSource],
                AmpControlByRPM: values[fieldIndexes.AmpControlByRPM],
                RPM1: values[fieldIndexes.RPM1],
                RPM2: values[fieldIndexes.RPM2],
                RPM3: values[fieldIndexes.RPM3],
                RPM4: values[fieldIndexes.RPM4],
                Amps1: values[fieldIndexes.Amps1],
                Amps2: values[fieldIndexes.Amps2],
                Amps3: values[fieldIndexes.Amps3],
                Amps4: values[fieldIndexes.Amps4],
                RPM5: values[fieldIndexes.RPM5],
                RPM6: values[fieldIndexes.RPM6],
                RPM7: values[fieldIndexes.RPM7],
                Amps5: values[fieldIndexes.Amps5],
                Amps6: values[fieldIndexes.Amps6],
                Amps7: values[fieldIndexes.Amps7],
                ShuntResistanceMicroOhm: values[fieldIndexes.ShuntResistanceMicroOhm],
                InvertAltAmps: values[fieldIndexes.InvertAltAmps],
                InvertBattAmps: values[fieldIndexes.InvertBattAmps],
                MaxDuty: values[fieldIndexes.MaxDuty],
                MinDuty: values[fieldIndexes.MinDuty],
                FieldResistance: values[fieldIndexes.FieldResistance],
                maxPoints: values[fieldIndexes.maxPoints],
                AlternatorCOffset: values[fieldIndexes.AlternatorCOffset],
                BatteryCOffset: values[fieldIndexes.BatteryCOffset],
                BatteryCapacity_Ah: values[fieldIndexes.BatteryCapacity_Ah],
                AmpSrc: values[fieldIndexes.AmpSrc],
                R_fixed: values[fieldIndexes.R_fixed],
                Beta: values[fieldIndexes.Beta],
                T0_C: values[fieldIndexes.T0_C],
                TempSource: values[fieldIndexes.TempSource],
                IgnitionOverride: values[fieldIndexes.IgnitionOverride],
                temperatureThermistor: values[fieldIndexes.temperatureThermistor],
                MaxTemperatureThermistor: values[fieldIndexes.MaxTemperatureThermistor],
                VictronCurrent: values[fieldIndexes.VictronCurrent],
                AlarmLatchEnabled: values[fieldIndexes.AlarmLatchEnabled],
                ForceFloat: values[fieldIndexes.ForceFloat],
                bulkCompleteTime: values[fieldIndexes.bulkCompleteTime],
                FLOAT_DURATION: values[fieldIndexes.FLOAT_DURATION],
                GPIO33_Status: values[fieldIndexes.GPIO33_Status],
                fieldActiveStatus: values[fieldIndexes.fieldActiveStatus],
                timeToFullChargeMin: values[fieldIndexes.timeToFullChargeMin],
                timeToFullDischargeMin: values[fieldIndexes.timeToFullDischargeMin],
                AutoShuntGainCorrection: values[fieldIndexes.AutoShuntGainCorrection],
                DynamicShuntGainFactor: values[fieldIndexes.DynamicShuntGainFactor],
                AutoAltCurrentZero: values[fieldIndexes.AutoAltCurrentZero],
                DynamicAltCurrentZero: values[fieldIndexes.DynamicAltCurrentZero],
                InsulationLifePercent: values[fieldIndexes.InsulationLifePercent],
                GreaseLifePercent: values[fieldIndexes.GreaseLifePercent],
                BrushLifePercent: values[fieldIndexes.BrushLifePercent],
                PredictedLifeHours: values[fieldIndexes.PredictedLifeHours],
                LifeIndicatorColor: values[fieldIndexes.LifeIndicatorColor],
                WindingTempOffset: values[fieldIndexes.WindingTempOffset],
                PulleyRatio: values[fieldIndexes.PulleyRatio],
                LastSessionDuration: values[fieldIndexes.LastSessionDuration],
                LastSessionMaxLoopTime: values[fieldIndexes.LastSessionMaxLoopTime],
                LastSessionMinHeap: values[fieldIndexes.LastSessionMinHeap],
                WifiReconnectsThisSession: values[fieldIndexes.WifiReconnectsThisSession],
                WifiReconnectsTotal: values[fieldIndexes.WifiReconnectsTotal],
                CurrentSessionDuration: values[fieldIndexes.CurrentSessionDuration],
                CurrentWifiSessionDuration: values[fieldIndexes.CurrentWifiSessionDuration],
                LastResetReason: values[fieldIndexes.LastResetReason],
                LastWifiResetReason: values[fieldIndexes.LastWifiResetReason],
                ManualLifePercentage: values[fieldIndexes.ManualLifePercentage],
                totalPowerCycles: values[fieldIndexes.totalPowerCycles]

            };

            window._debugData = data;

            // Check for stale data and apply visual effects
            if (window.sensorAges) {

                // Helper function to apply staleness styling using age directly
                function applyStaleStyleByAge(elementId, ageMs) {
                    const element = document.getElementById(elementId);
                    if (!element) {
                        console.warn(`Element ${elementId} not found for stale styling`);
                        return;
                    }
                    const staleThreshold = 7100; // 7.1 seconds, don't go lower without making sure digital temp sensor tempTask is going to keep up
                    const isStale = (ageMs > staleThreshold);
                    if (isStale) {
                        element.style.opacity = "0.4";
                        element.style.color = "#999999";
                        element.style.fontStyle = "italic";
                        element.title = `Data is ${Math.round(ageMs / 1000)} seconds old`;
                    } else {
                        element.style.opacity = "1.0";
                        element.style.color = "var(--reading)";
                        element.style.fontStyle = "normal";
                        element.title = "";
                    }
                }

                // Apply staleness using ages directly 
                applyStaleStyleByAge("GPSHID", window.sensorAges.heading);                      // GPS heading
                applyStaleStyleByAge("LatitudeNMEA_ID", window.sensorAges.latitude);            // GPS latitude
                applyStaleStyleByAge("LongitudeNMEA_ID", window.sensorAges.longitude);          // GPS longitude
                applyStaleStyleByAge("SatelliteCountNMEA_ID", window.sensorAges.satellites);    // GPS satellite count
                applyStaleStyleByAge("VictronVoltageID", window.sensorAges.victronVoltage);     // Victron voltage
                applyStaleStyleByAge("VictronCurrentID", window.sensorAges.victronCurrent);     // Victron current
                applyStaleStyleByAge("AltTempID", window.sensorAges.alternatorTemp);            // Alternator temperature
                applyStaleStyleByAge("temperatureThermistorID", window.sensorAges.thermistorTemp); // Thermistor temperature
                applyStaleStyleByAge("RPMID", window.sensorAges.rpm);                           // Engine RPM
                applyStaleStyleByAge("MeasAmpsID", window.sensorAges.measuredAmps);             // Alternator current
                applyStaleStyleByAge("BatteryVID", window.sensorAges.batteryV);                 // ADS battery voltage
                applyStaleStyleByAge("IBVID", window.sensorAges.ibv);                           // INA battery voltage
                applyStaleStyleByAge("BCurrID", window.sensorAges.bcur);                        // Battery current
                applyStaleStyleByAge("ADS3ID", window.sensorAges.channel3V);                    // ADS Channel 3 voltage
                applyStaleStyleByAge("DutyCycleID", window.sensorAges.dutyCycle);               // Field duty cycle
                applyStaleStyleByAge("FieldVoltsID", window.sensorAges.fieldVolts);             // Field voltage (calculated)
                applyStaleStyleByAge("FieldAmpsID", window.sensorAges.fieldAmps);               // Field current (calculated)

                //banner
                applyStaleStyleByAge("header-voltage", window.sensorAges.ibv);
                applyStaleStyleByAge("header-soc", window.sensorAges.soc);
                applyStaleStyleByAge("header-alt-current", window.sensorAges.measuredAmps);
                applyStaleStyleByAge("header-batt-current", window.sensorAges.bcur);
                applyStaleStyleByAge("header-alt-temp", window.sensorAges.alternatorTemp);
                applyStaleStyleByAge("header-rpm", window.sensorAges.rpm);

            }

            updateAlarmStatus(data);

            const fieldIndicator = document.getElementById('field-status');
            if (fieldIndicator) {
                if (data.fieldActiveStatus === 1) {
                    fieldIndicator.textContent = 'ACTIVE';
                    fieldIndicator.className = 'reading-value field-status-active';
                } else {
                    fieldIndicator.textContent = 'OFF';
                    fieldIndicator.className = 'reading-value field-status-inactive';
                }
            }

            let lastValues = {};

            // Modify your existing updateFields with MINIMAL changes:
            // Enhanced updateFields function that handles reset reason lookups
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const el = document.getElementById(elementId);
                    if (!el) continue;
                    const value = data[key];

                    // Calculate what the new text content would be
                    let newTextContent;
                    if (value === undefined || isNaN(value)) {
                        newTextContent = "—";
                    }
                    // Special handling for reset reason lookups
                    else if (key === "LastResetReason") {
                        newTextContent = resetReasonLookup[value] || `Unknown (${value})`;
                    }
                    else if (key === "LastWifiResetReason") {
                        newTextContent = wifiResetReasonLookup[value] || `Unknown WiFi issue (${value})`;
                    }
                    // Values scaled by 100 on server (existing logic)
                    else if (["BatteryV", "Channel3V", "IBV", "VictronVoltage", "vvout", "FullChargeVoltage", "VictronCurrent",
                        "TargetFloatVoltage", "IBVMax", "Bcur", "MeasuredAmps", "MeasuredAmpsMax", "SoC_percent", "AlternatorCOffset", "BatteryCOffset", "R_fixed", "Beta", "T0_C"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Values scaled by 10 on server (existing logic)  
                    else if (["iiout"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    // Energy values (existing logic)
                    else if (["ChargedEnergy", "DischargedEnergy", "AlternatorChargedEnergy"].includes(key)) {
                        newTextContent = value;
                    }
                    // Fuel in mL (existing logic)
                    else if (["AlternatorFuelUsed"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    // Time values (existing logic)
                    else if (["EngineRunTime", "AlternatorOnTime"].includes(key)) {
                        const totalSeconds = value; //
                        const hours = Math.floor(totalSeconds / 3600);
                        const minutes = Math.floor((totalSeconds % 3600) / 60);
                        const seconds = totalSeconds % 60;
                        newTextContent = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
                    }
                    // Session duration in minutes (new logic)
                    else if (["LastSessionDuration", "CurrentSessionDuration", "CurrentWifiSessionDuration"].includes(key)) {
                        if (value < 60) {
                            newTextContent = `${value}`;  // Less than 1 hour, show minutes
                        } else {
                            const hours = Math.floor(value / 60);
                            const minutes = value % 60;
                            newTextContent = `${hours}h ${minutes}m`;
                        }
                    }
                    // Cool dynamic adjustments (existing logic)
                    else if (["DynamicShuntGainFactor", "DynamicAltCurrentZero"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    // Alternator life parameters (existing logic)
                    else if (["InsulationLifePercent", "GreaseLifePercent", "BrushLifePercent"].includes(key)) {
                        newTextContent = (value / 100).toFixed(1);
                    }
                    else if (["WindingTempOffset"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    else if (["PulleyRatio"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Settings echo values (existing logic)
                    else if (["TargetAmps", "TargetAmpL"].includes(key)) {
                        newTextContent = value;
                    }
                    // Default: display as integer (existing logic)
                    else {
                        newTextContent = Math.round(value);
                    }

                    // ONLY UPDATE DOM IF VALUE ACTUALLY CHANGED
                    if (lastValues[elementId] !== newTextContent) {
                        el.textContent = newTextContent;
                        lastValues[elementId] = newTextContent;
                    }
                }
            };

            updateFields(telemetryFields);  // only visible span updates
            updateLifeIndicators(data); // for the alternator lifetime calculation

            // document.getElementById('header-alternator-enable').checked = (data.OnOff === 1); // no idea, don't ask

            if ('TemperatureLimitF' in data) {
                document.getElementById('TemperatureLimitF_echo').textContent = data.TemperatureLimitF;
            }
            if ('FullChargeVoltage' in data) {
                document.getElementById('FullChargeVoltage_echo').textContent = (data.FullChargeVoltage / 100).toFixed(2);
            }
            if ('TargetAmps' in data) {
                document.getElementById('TargetAmps_echo').textContent = data.TargetAmps;
            }
            if ('TargetFloatVoltage' in data) {
                document.getElementById('TargetFloatVoltage_echo').textContent = (data.TargetFloatVoltage / 100).toFixed(2);
            }
            if ('SwitchingFrequency' in data) {
                document.getElementById('SwitchingFrequency_echo').textContent = data.SwitchingFrequency;
            }
            if ('interval' in data) {
                document.getElementById('interval_echo').textContent = (data.interval / 100).toFixed(3);
            }
            if ('FieldAdjustmentInterval' in data) {
                document.getElementById('FieldAdjustmentInterval_echo').textContent = data.FieldAdjustmentInterval;
            }
            if ('ManualDuty' in data) {
                document.getElementById('ManualDuty_echo').textContent = data.ManualDuty;
            }
            if ('SwitchControlOverride' in data) {
                document.getElementById('SwitchControlOverride_echo').textContent = data.SwitchControlOverride;
            }
            if ('OnOff' in data) {
                document.getElementById('OnOff_echo').textContent = data.OnOff;
            }
            if ('ManualFieldToggle' in data) {
                document.getElementById('ManualFieldToggle_echo').textContent = data.ManualFieldToggle;
            }
            if ('HiLow' in data) {
                document.getElementById('HiLow_echo').textContent = data.HiLow;
            }
            if ('LimpHome' in data) {
                document.getElementById('LimpHome_echo').textContent = data.LimpHome;
            }
            if ('VeData' in data) {
                document.getElementById('VeData_echo').textContent = data.VeData;
            }
            if ('NMEA0183Data' in data) {
                document.getElementById('NMEA0183Data_echo').textContent = data.NMEA0183Data;
            }
            if ('NMEA2KData' in data) {
                document.getElementById('NMEA2KData_echo').textContent = data.NMEA2KData;
            }
            if ('TargetAmpL' in data) {
                document.getElementById('TargetAmpL_echo').textContent = data.TargetAmpL;
            }
            if ('CurrentThreshold' in data) {
                document.getElementById('CurrentThreshold_echo').textContent = data.CurrentThreshold;
            }
            if ('PeukertExponent' in data) {
                document.getElementById('PeukertExponent_echo').textContent = (data.PeukertExponent / 100).toFixed(2);
            }
            if ('ChargeEfficiency' in data) {
                document.getElementById('ChargeEfficiency_echo').textContent = data.ChargeEfficiency + '%';
            }
            if ('ChargedVoltage' in data) {
                document.getElementById('ChargedVoltage_echo').textContent = (data.ChargedVoltage / 100).toFixed(2);
            }
            if ('TailCurrent' in data) {
                document.getElementById('TailCurrent_echo').textContent = data.TailCurrent;
            }
            if ('ChargedDetectionTime' in data) {
                document.getElementById('ChargedDetectionTime_echo').textContent = data.ChargedDetectionTime;
            }
            if ('IgnoreTemperature' in data) {
                document.getElementById('IgnoreTemperature_echo').textContent = data.IgnoreTemperature;
            }
            if ('BMSLogic' in data) {
                document.getElementById('BMSLogic_echo').textContent = data.BMSLogic;
            }
            if ('BMSLogicLevelOff' in data) {
                document.getElementById('BMSLogicLevelOff_echo').textContent = data.BMSLogicLevelOff;
            }
            if ('AlarmActivate' in data) {
                document.getElementById('AlarmActivate_echo').textContent = data.AlarmActivate;
            }
            if ('TempAlarm' in data) {
                document.getElementById('TempAlarm_echo').textContent = data.TempAlarm;
            }
            if ('VoltageAlarmHigh' in data) {
                document.getElementById('VoltageAlarmHigh_echo').textContent = data.VoltageAlarmHigh;
            }
            if ('VoltageAlarmLow' in data) {
                document.getElementById('VoltageAlarmLow_echo').textContent = data.VoltageAlarmLow;
            }
            if ('CurrentAlarmHigh' in data) {
                document.getElementById('CurrentAlarmHigh_echo').textContent = data.CurrentAlarmHigh;
            }
            if ('FourWay' in data) {
                document.getElementById('FourWay_echo').textContent = data.FourWay;
            }
            if ('RPMScalingFactor' in data) {
                document.getElementById('RPMScalingFactor_echo').textContent = data.RPMScalingFactor;
            }
            if ('ResetTemp' in data) {
                document.getElementById('ResetTemp_echo').textContent = data.ResetTemp;
            }
            if ('ResetVoltage' in data) {
                document.getElementById('ResetVoltage_echo').textContent = data.ResetVoltage;
            }
            if ('ResetCurrent' in data) {
                document.getElementById('ResetCurrent_echo').textContent = data.ResetCurrent;
            }
            if ('ResetEngineRunTime' in data) {
                document.getElementById('ResetEngineRunTime_echo').textContent = data.ResetEngineRunTime;
            }
            if ('ResetAlternatorOnTime' in data) {
                document.getElementById('ResetAlternatorOnTime_echo').textContent = data.ResetAlternatorOnTime;
            }
            if ('ResetEnergy' in data) {
                document.getElementById('ResetEnergy_echo').textContent = data.ResetEnergy;
            }
            if ('MaximumAllowedBatteryAmps' in data) {
                document.getElementById('MaximumAllowedBatteryAmps_echo').textContent = (data.MaximumAllowedBatteryAmps);
            }
            if ('ManualSOCPoint' in data) {
                document.getElementById('ManualSOCPoint_echo').textContent = data.ManualSOCPoint;
            }
            if ('BatteryVoltageSource' in data) {
                document.getElementById('BatteryVoltageSource_echo').textContent = data.BatteryVoltageSource;
            }
            if ('AmpControlByRPM' in data) {
                document.getElementById('AmpControlByRPM_echo').textContent = data.AmpControlByRPM;
            }
            if ('RPM1' in data) {
                document.getElementById('RPM1_echo').textContent = data.RPM1;
            }
            if ('RPM2' in data) {
                document.getElementById('RPM2_echo').textContent = data.RPM2;
            }
            if ('RPM3' in data) {
                document.getElementById('RPM3_echo').textContent = data.RPM3;
            }
            if ('RPM4' in data) {
                document.getElementById('RPM4_echo').textContent = data.RPM4;
            }
            if ('Amps1' in data) {
                document.getElementById('Amps1_echo').textContent = data.Amps1;
            }
            if ('Amps2' in data) {
                document.getElementById('Amps2_echo').textContent = data.Amps2;
            }
            if ('Amps3' in data) {
                document.getElementById('Amps3_echo').textContent = data.Amps3;
            }
            if ('Amps4' in data) {
                document.getElementById('Amps4_echo').textContent = data.Amps4;
            }
            if ('RPM5' in data) {
                document.getElementById('RPM5_echo').textContent = data.RPM5;
            }
            if ('RPM6' in data) {
                document.getElementById('RPM6_echo').textContent = data.RPM6;
            }
            if ('RPM7' in data) {
                document.getElementById('RPM7_echo').textContent = data.RPM7;
            }
            if ('Amps5' in data) {
                document.getElementById('Amps5_echo').textContent = data.Amps5;
            }
            if ('Amps6' in data) {
                document.getElementById('Amps6_echo').textContent = data.Amps6;
            }
            if ('Amps7' in data) {
                document.getElementById('Amps7_echo').textContent = data.Amps7;
            }
            if ('ShuntResistanceMicroOhm' in data) {
                document.getElementById('ShuntResistanceMicroOhm_echo').textContent = data.ShuntResistanceMicroOhm;
            }
            if ('InvertAltAmps' in data) {
                document.getElementById('InvertAltAmps_echo').textContent = data.InvertAltAmps;
            }
            if ('InvertBattAmps' in data) {
                document.getElementById('InvertBattAmps_echo').textContent = data.InvertBattAmps;
            }
            if ('MaxDuty' in data) {
                document.getElementById('MaxDuty_echo').textContent = data.MaxDuty;
            }
            if ('MinDuty' in data) {
                document.getElementById('MinDuty_echo').textContent = data.MinDuty;
            }
            if ('FieldResistance' in data) {
                document.getElementById('FieldResistance_echo').textContent = data.FieldResistance;
            }
            if ('maxPoints' in data) {
                document.getElementById('maxPoints_echo').textContent = data.maxPoints;
            }
            if ('AlternatorCOffset' in data) {
                document.getElementById('AlternatorCOffset_echo').textContent = (data.AlternatorCOffset / 100).toFixed(2);
            }
            if ('BatteryCOffset' in data) {
                document.getElementById('BatteryCOffset_echo').textContent = (data.BatteryCOffset / 100).toFixed(2);
            }
            if ('BatteryCapacity_Ah' in data) {
                document.getElementById('BatteryCapacity_Ah_echo').textContent = data.BatteryCapacity_Ah;
            }
            if ('R_fixed' in data) {
                document.getElementById('R_fixed_echo').textContent = (data.R_fixed / 100).toFixed(2);
            }
            if ('Beta' in data) {
                document.getElementById('Beta_echo').textContent = (data.Beta / 100).toFixed(2);
            }
            if ('T0_C' in data) {
                document.getElementById('T0_C_echo').textContent = (data.T0_C / 100).toFixed(2);
            }
            if ('TempSource' in data) {
                document.getElementById('TempSource_echo').textContent = data.TempSource;
            }
            if ('IgnitionOverride' in data) {
                document.getElementById('IgnitionOverride_echo').textContent = data.IgnitionOverride;
            }
            if ('AmpSrc' in data) {
                document.getElementById('AmpSrc_echo').textContent = data.AmpSrc;
            }
            if ('AlarmLatchEnabled' in data) {
                document.getElementById('AlarmLatchEnabled_echo').textContent = data.AlarmLatchEnabled;
            }
            if ('AlarmTest' in data) {
                document.getElementById('AlarmTest_echo').textContent = data.AlarmTest;
            }
            if ('ResetAlarmLatch' in data) {
                document.getElementById('ResetAlarmLatch_echo').textContent = data.ResetAlarmLatch;
            }
            if ('ForceFloat' in data) {
                document.getElementById('ForceFloat_echo').textContent = data.ForceFloat;
            }
            if ('bulkCompleteTime' in data) {
                document.getElementById('bulkCompleteTime_echo').textContent = data.bulkCompleteTime;
            }
            if ('FLOAT_DURATION' in data) {
                document.getElementById('FLOAT_DURATION_echo').textContent = Math.round(data.FLOAT_DURATION / 3600);
            }
            if ('AutoShuntGainCorrection' in data) {
                document.getElementById('AutoShuntGainCorrection_echo').textContent = data.AutoShuntGainCorrection; // BOOLEAN
            }
            if ('AutoAltCurrentZero' in data) {
                document.getElementById('AutoAltCurrentZero_echo').textContent = data.AutoAltCurrentZero;
            }
            if ('WindingTempOffset' in data) {
                document.getElementById('WindingTempOffset_echo').textContent = (data.WindingTempOffset / 10).toFixed(1);
            }
            if ('PulleyRatio' in data) {
                document.getElementById('PulleyRatio_echo').textContent = (data.PulleyRatio / 100).toFixed(2);
            }
            if ('ManualLifePercentage' in data) {
                document.getElementById('ManualLifePercentage_echo').textContent = data.ManualLifePercentage;
            }

            if ('DynamicShuntGainFactor' in data) {
                const displayEl = document.getElementById('DynamicShuntGainFactor_display');
                if (displayEl) {
                    displayEl.textContent = (data.DynamicShuntGainFactor / 1000).toFixed(3);
                }
            }
            if ('DynamicAltCurrentZero' in data) {
                const displayEl = document.getElementById('DynamicAltCurrentZero_display');
                if (displayEl) {
                    displayEl.textContent = (data.DynamicAltCurrentZero / 1000).toFixed(3) + ' A';
                }
            }

            if ('BatteryCurrentSource' in data) {
                document.getElementById('BatteryCurrentSource_echo').textContent = data.BatteryCurrentSource;
            }
            if ('InvertBatteryMonitorAmps' in data) {
                document.getElementById('InvertBatteryMonitorAmps_echo').textContent = data.InvertBatteryMonitorAmps;
            }
            if ('webgaugesinterval' in data) {
                document.getElementById('webgaugesinterval_echo').textContent = data.webgaugesinterval;
            }
            if ('plotTimeWindow' in data) {
                document.getElementById('plotTimeWindow_echo').textContent = data.plotTimeWindow;
            }
            if ('totalPowerCycles' in data) {
                const el = document.getElementById('totalPowerCyclesID');
                if (el) el.textContent = data.totalPowerCycles;
            }

            if ('AlarmLatchState' in data) {
                const latchDisplay = document.getElementById('AlarmLatchState_display');
                if (data.AlarmLatchState === 1) {
                    latchDisplay.textContent = 'LATCHED';
                    latchDisplay.style.color = '#ff0000';
                } else {
                    latchDisplay.textContent = 'Normal';
                    latchDisplay.style.color = 'var(--accent)';
                }
            }

            // Initialize RPM/Amps fields once on page load
            if (!rpmAmpsInitialized) {
                const rpmAmpFields = [
                    { name: "RPM1", id: "RPM1_input" },
                    { name: "RPM2", id: "RPM2_input" },
                    { name: "RPM3", id: "RPM3_input" },
                    { name: "RPM4", id: "RPM4_input" },
                    { name: "Amps1", id: "Amps1_input" },
                    { name: "Amps2", id: "Amps2_input" },
                    { name: "Amps3", id: "Amps3_input" },
                    { name: "Amps4", id: "Amps4_input" },
                    { name: "RPM5", id: "RPM5_input" },
                    { name: "RPM6", id: "RPM6_input" },
                    { name: "RPM7", id: "RPM7_input" },
                    { name: "Amps5", id: "Amps5_input" },
                    { name: "Amps6", id: "Amps6_input" },
                    { name: "Amps7", id: "Amps7_input" }
                ];

                // Check if we have received actual data from ESP32 (not just initial zeros)
                let hasValidData = false;
                for (const field of rpmAmpFields) {
                    if (data[field.name] !== undefined && data[field.name] > 0) {
                        hasValidData = true;
                        break;
                    }
                }

                // Only initialize if we have real data from ESP32, or this is the first data packet
                if (hasValidData || data.AlternatorTemperatureF !== undefined) {
                    for (const field of rpmAmpFields) {
                        const inputEl = document.getElementById(field.id);
                        if (inputEl && data[field.name] !== undefined) {
                            inputEl.value = data[field.name];
                        }
                    }
                    rpmAmpsInitialized = true;
                }
            }

            const now = Math.floor(Date.now() / 1000);

            updateTogglesFromData(data); // update toggle switches

            // something to do with grayed out settings
            if (currentAdminPassword === "") {
                document.getElementById('settings-section').classList.add("locked");
            }

            // Update Current plot data arrays
            if (typeof currentTempPlot !== 'undefined') {
                const battCurrent = 'Bcur' in data ? parseFloat(data.Bcur) / 100 : null;
                const altCurrent = 'MeasuredAmps' in data ? parseFloat(data.MeasuredAmps) / 100 : null;
                const fieldCurrent = 'iiout' in data ? parseFloat(data.iiout) / 10 : null;
                // const altTemp = 'AlternatorTemperatureF' in data ? parseFloat(data.AlternatorTemperatureF) : null;

                currentTempData[0].push(now);
                currentTempData[1].push(battCurrent);
                currentTempData[2].push(altCurrent);
                currentTempData[3].push(fieldCurrent);

                //Ensure we always have a valid maxPoints, default to 40 if not set or invalid
                const currentMaxPoints = (data.maxPoints && data.maxPoints > 0) ? data.maxPoints : 40;
                if (currentTempData[0].length > currentMaxPoints) {
                    currentTempData[0].shift();
                    currentTempData[1].shift();
                    currentTempData[2].shift();
                    currentTempData[3].shift();
                }
            }

            // Update Voltage plot data arrays
            if (typeof voltagePlot !== 'undefined') {
                const adsBattV = 'BatteryV' in data ? parseFloat(data.BatteryV) / 100 : null;
                const inaBattV = 'IBV' in data ? parseFloat(data.IBV) / 100 : null;

                voltageData[0].push(now);
                voltageData[1].push(adsBattV);
                voltageData[2].push(inaBattV);

                // Ensure we always have a valid maxPoints, default to 40 if not set or invalid
                const currentMaxPoints = (data.maxPoints && data.maxPoints > 0) ? data.maxPoints : 40;
                if (voltageData[0].length > currentMaxPoints) {
                    voltageData[0].shift();
                    voltageData[1].shift();
                    voltageData[2].shift();
                }
            }

            // Update RPM plot data arrays
            if (typeof rpmPlot !== 'undefined') {
                const rpmValue = 'RPM' in data ? parseFloat(data.RPM) : null;

                rpmData[0].push(now);
                rpmData[1].push(rpmValue);

                // Ensure we always have a valid maxPoints, default to 40 if not set or invalid
                const currentMaxPoints = (data.maxPoints && data.maxPoints > 0) ? data.maxPoints : 40;
                if (rpmData[0].length > currentMaxPoints) {
                    rpmData[0].shift();
                    rpmData[1].shift();
                }
            }

            // Update Temperature plot data arrays
            if (typeof temperaturePlot !== 'undefined') {
                const altTemp = 'AlternatorTemperatureF' in data ? parseFloat(data.AlternatorTemperatureF) : null;

                temperatureData[0].push(now);
                temperatureData[1].push(altTemp);

                // Ensure we always have a valid maxPoints, default to 40 if not set or invalid
                const currentMaxPoints = (data.maxPoints && data.maxPoints > 0) ? data.maxPoints : 40;
                if (temperatureData[0].length > currentMaxPoints) {
                    temperatureData[0].shift();
                    temperatureData[1].shift();
                }
            }

            // Only update the plots if enough time has passed - now update all three plots
            const nowMs = Date.now();
            if (nowMs - lastPlotUpdate > PLOT_UPDATE_INTERVAL_MS) {
                scheduleCurrentTempPlotUpdate();
                scheduleVoltagePlotUpdate();
                scheduleRPMPlotUpdate();
                scheduleTemperaturePlotUpdate();

                // Add connection status check here - it will run once per second with your other updates
                const timeSinceLastEvent = nowMs - lastEventTime;
                if (timeSinceLastEvent > 5000) { // 5 seconds without new data = disconnected
                    updateInlineStatus(false);
                } else {
                    updateInlineStatus(true);
                }

                lastPlotUpdate = nowMs;
            }
        }, false);

        source.addEventListener('TimestampData', function (e) {
            const ages = e.data.split(',').map(Number); // Now these are ages in milliseconds
            window.sensorAges = {
                heading: ages[0],
                latitude: ages[1],
                longitude: ages[2],
                satellites: ages[3],
                victronVoltage: ages[4],
                victronCurrent: ages[5],
                alternatorTemp: ages[6],  // This is how old the temp reading is
                thermistorTemp: ages[7],
                rpm: ages[8],
                measuredAmps: ages[9],
                batteryV: ages[10],
                ibv: ages[11],
                bcur: ages[12],
                channel3V: ages[13],
                dutyCycle: ages[14],
                fieldVolts: ages[15],
                fieldAmps: ages[16]
            };
        }, false);

    }

    // uplot plot minor details like dark mode and gentle space
    function updateUplotTheme(plot) {
        const textColor = getComputedStyle(document.body).getPropertyValue('--text-dark').trim();
        const gridColor = getComputedStyle(document.body).getPropertyValue('--border').trim();
        // Update axis label divs
        plot.root.querySelectorAll('.u-label').forEach(el => {
            el.style.color = textColor;
        });
        // Update tick label text (SVG)
        plot.root.querySelectorAll('.u-axis text').forEach(el => {
            el.setAttribute('fill', textColor);
        });
        // Update axis strokes (SVG)
        plot.root.querySelectorAll('.u-axis path').forEach(el => {
            el.setAttribute('stroke', textColor);
        });
        // Update grid lines (SVG)
        plot.root.querySelectorAll('.u-grid line').forEach(el => {
            el.setAttribute('stroke', gridColor);
        });
    }

    // Improve legend appearance with clean lines and responsive spacing
    const style = document.createElement('style');
    style.textContent = `
.u-legend {
display: flex;
flex-wrap: wrap;
justify-content: center;
gap: 10px;
max-width: 100%;
box-sizing: border-box;
}

.u-legend .u-series {
display: flex;
align-items: center;
gap: 6px;
}

.u-legend .u-marker {
display: inline-block;
width: 18px;
height: 3px;
background-color: currentColor;
border-radius: 1px;
}

.plot-container {
padding: 10px;
box-sizing: border-box;
max-width: 1000px;     /* NEW: limit width on wide screens */
margin: 0 auto;       /* center it */
}

@media (max-width: 768px) {
.plot-container {
padding: 5px;
max-width: 100%;     /* allow full width on mobile */
}
}

`;
    document.head.appendChild(style);

    // Initialize toggle states on page load
    document.getElementById("ManualFieldToggle_checkbox").checked = (document.getElementById("ManualFieldToggle").value === "1");
    document.getElementById("SwitchControlOverride_checkbox").checked = (document.getElementById("SwitchControlOverride").value === "1");
    //document.getElementById("OnOff_checkbox").checked = (document.getElementById("OnOff").value === "1");
    document.getElementById("LimpHome_checkbox").checked = (document.getElementById("LimpHome").value === "1");
    document.getElementById("HiLow_checkbox").checked = (document.getElementById("HiLow").value === "1");
    document.getElementById("VeData_checkbox").checked = (document.getElementById("VeData").value === "1");
    document.getElementById("NMEA0183Data_checkbox").checked = (document.getElementById("NMEA0183Data").value === "1");
    document.getElementById("NMEA2KData_checkbox").checked = (document.getElementById("NMEA2KData").value === "1");
    document.getElementById("IgnoreTemperature_checkbox").checked = (document.getElementById("IgnoreTemperature").value === "1");
    document.getElementById("BMSLogic_checkbox").checked = (document.getElementById("BMSLogic").value === "1");
    document.getElementById("BMSLogicLevelOff_checkbox").checked = (document.getElementById("BMSLogicLevelOff").value === "1");
    document.getElementById("AlarmActivate_checkbox").checked = (document.getElementById("AlarmActivate").value === "1");
    document.getElementById("AmpControlByRPM_checkbox").checked = (document.getElementById("AmpControlByRPM").value === "1");
    document.getElementById("InvertAltAmps_checkbox").checked = (document.getElementById("InvertAltAmps").value === "1");
    document.getElementById("InvertBattAmps_checkbox").checked = (document.getElementById("InvertBattAmps").value === "1");
    document.getElementById("IgnitionOverride_checkbox").checked = (document.getElementById("IgnitionOverride").value === "1");
    document.getElementById("TempSource_checkbox").checked = (document.getElementById("TempSource").value === "1");
    document.getElementById("admin_password").addEventListener("change", updatePasswordFields);
    document.getElementById("InvertBatteryMonitorAmps_checkbox").checked = (document.getElementById("InvertBatteryMonitorAmps").value === "1");

    setupInputValidation(); // Client side input validation of settings



}); // ← ADD THIS LINE

// graying of obsolete data debug
function debugSensorAges() {
    console.log('Current sensor ages:', window.sensorAges);
    if (!window.sensorAges) {
        console.error('sensorAges is undefined!');
    }
}

//something to do with tabs
function showMainTab(tabName) {
    // Hide all tab contents
    const tabContents = document.querySelectorAll('.tab-content');
    tabContents.forEach(tab => tab.classList.remove('active'));

    // Remove active class from all main tabs
    const mainTabs = document.querySelectorAll('.main-tab');
    mainTabs.forEach(tab => tab.classList.remove('active'));

    // Show selected tab content
    document.getElementById(tabName).classList.add('active');

    // Find and activate the correct button by looking for the one that calls this tab
    mainTabs.forEach(tab => {
        if (tab.getAttribute('onclick').includes(tabName)) {
            tab.classList.add('active');
        }
    });
}

function showSubTab(parentTab, subTabName) {
    // Hide all sub-tab contents for this parent
    const subTabContents = document.querySelectorAll(`#${parentTab} .sub-tab-content`);
    subTabContents.forEach(tab => tab.classList.remove('active'));

    // Remove active class from all sub-tabs
    const subTabs = document.querySelectorAll(`#${parentTab} .sub-tab`);
    subTabs.forEach(tab => tab.classList.remove('active'));

    // Show selected sub-tab content
    document.getElementById(`${parentTab}-${subTabName}`).classList.add('active');

    // Add active class to clicked sub-tab
    event.target.classList.add('active');
}
// Frozen page response stuff
function showRecoveryOptions() {
    // Don't show multiple recovery dialogs
    if (document.getElementById('recoveryDialog')) return;

    const recoveryDiv = document.createElement('div');
    recoveryDiv.id = 'recoveryDialog';
    recoveryDiv.innerHTML = `
<div style="position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); 
            background: var(--card-light); padding: 20px; border-radius: var(--radius); 
            box-shadow: 0 4px 8px rgba(0,0,0,0.3); z-index: 10000; border: 2px solid var(--accent);">
  <h3 style="margin-top: 0; color: var(--text-dark);">Connection Lost</h3>
  <p style="color: var(--text-dark);">Lost connection to alternator regulator.</p>
  <button onclick="retryConnection()" style="background-color: var(--accent); color: white; border: none; padding: 8px 16px; border-radius: var(--radius); cursor: pointer; margin-right: 10px;">Retry Connection</button>
  <button onclick="enterOfflineMode()" style="background-color: #555555; color: white; border: none; padding: 8px 16px; border-radius: var(--radius); cursor: pointer;">Continue Offline</button>
  <p style="font-size: 12px; color: var(--text-dark);"><small>If problem persists, the device may need to be power-cycled.</small></p>
</div>
`;
    document.body.appendChild(recoveryDiv);
}
function retryConnection() {
    closeRecovery();
    location.reload();
}
function closeRecovery() {
    const dialog = document.getElementById('recoveryDialog');
    if (dialog) {
        dialog.remove();
    }
}
function enterOfflineMode() {
    closeRecovery();
    // Don't run multiple times
    if (document.getElementById('offlineBanner')) return;
    // Disable all form controls and buttons except reconnect and tab navigation
    document.querySelectorAll('input[type="submit"], button, input[type="checkbox"], input[type="number"], input[type="text"], input[type="password"], select').forEach(el => {
        // Skip the dark mode toggle, reconnect buttons, and tab navigation
        if (!el.id.includes('DarkMode') &&
            !el.onclick?.toString().includes('location.reload') &&
            !el.classList.contains('main-tab') &&
            !el.classList.contains('sub-tab')) {
            el.disabled = true;
            el.style.opacity = '0.5';
            el.style.cursor = 'not-allowed';
        }
    });
    // Disable toggles in offline mode
    document.querySelectorAll('.switch input').forEach(el => {
        el.disabled = true;
        el.style.opacity = '0.5';
    });
    // Update corner status to show offline
    const cornerStatus = document.getElementById('corner-status');
    if (cornerStatus) {
        cornerStatus.className = 'corner-status corner-status-disconnected';
        cornerStatus.textContent = 'OFFLINE MODE';
        cornerStatus.style.backgroundColor = '#ff6600';
    }
    console.log('Entered offline mode - all readings marked as stale');
}
function validateRPMOrder() {
    // Find the specific form containing RPM inputs
    const rpmForm = document.querySelector('form input[name="RPM1"]')?.closest('form');
    if (!rpmForm) {
        console.warn('RPM form not found');
        return;
    }

    const saveButton = rpmForm.querySelector('input[type="submit"]');
    let valid = true;
    let errorMessage = '';

    // Clear previous error styles
    for (let i = 1; i <= 7; i++) {
        const input = document.getElementById(`RPM${i}_input`);
        if (input) {
            input.classList.remove('input-error');
            input.title = ''; // Clear tooltip
        }
    }

    // Check each pair for ascending order
    for (let i = 2; i <= 7; i++) {
        const prevInput = document.getElementById(`RPM${i - 1}_input`);
        const currInput = document.getElementById(`RPM${i}_input`);

        if (!prevInput || !currInput) continue;

        const prev = prevInput.valueAsNumber;
        const curr = currInput.valueAsNumber;

        // Only validate if both values are entered and valid
        if (!isNaN(prev) && !isNaN(curr) && prev > 0 && curr > 0) {
            if (curr <= prev) {
                currInput.classList.add('input-error');
                currInput.title = `RPM${i} must be greater than RPM${i - 1} (${prev})`;
                prevInput.classList.add('input-error');
                prevInput.title = `RPM${i - 1} must be less than RPM${i} (${curr})`;
                valid = false;
                errorMessage = 'RPM values must be in ascending order';
            }
        }
    }

    if (saveButton) {
        saveButton.disabled = !valid;
        saveButton.title = valid ? '' : errorMessage;
    }

    return valid;
}

function validateSettings() {
    const validations = [
        {
            inputs: ['TargetFloatVoltage', 'FullChargeVoltage'],
            check: (float, bulk) => !isNaN(float) && !isNaN(bulk) && float >= bulk,
            error: 'Float voltage must be less than bulk voltage'
        },
        {
            inputs: ['TargetAmpL', 'TargetAmps'],
            check: (low, normal) => !isNaN(low) && !isNaN(normal) && low > normal,
            error: 'Low amp target must be less than or equal to normal amp target'
        },
        {
            inputs: ['MinDuty', 'MaxDuty'],
            check: (min, max) => !isNaN(min) && !isNaN(max) && min >= max,
            error: 'Minimum duty cycle must be less than maximum duty cycle'
        }
    ];

    // Clear all previous error states
    document.querySelectorAll('.input-error').forEach(el => {
        el.classList.remove('input-error');
        el.title = '';
    });

    validations.forEach(validation => {
        const [input1Name, input2Name] = validation.inputs;
        const input1 = document.querySelector(`input[name="${input1Name}"]`);
        const input2 = document.querySelector(`input[name="${input2Name}"]`);

        if (!input1 || !input2) return;

        const value1 = input1.valueAsNumber;
        const value2 = input2.valueAsNumber;
        const button1 = input1.closest('form')?.querySelector('input[type="submit"]');
        const button2 = input2.closest('form')?.querySelector('input[type="submit"]');

        if (validation.check(value1, value2)) {
            // Add error styling
            input1.classList.add('input-error');
            input2.classList.add('input-error');
            input1.title = validation.error;
            input2.title = validation.error;

            // Disable buttons
            if (button1) {
                button1.disabled = true;
                button1.title = validation.error;
            }
            if (button2) {
                button2.disabled = true;
                button2.title = validation.error;
            }
        } else {
            // Re-enable buttons if no error
            if (button1) {
                button1.disabled = false;
                button1.title = '';
            }
            if (button2) {
                button2.disabled = false;
                button2.title = '';
            }
        }
    });
}

//  validation called in the window load event, for client side checking of input values
function setupInputValidation() {
    // Settings validation
    const settingsInputs = ["TargetFloatVoltage", "FullChargeVoltage", "TargetAmpL", "TargetAmps", "MinDuty", "MaxDuty"];

    settingsInputs.forEach(name => {
        const input = document.querySelector(`input[name="${name}"]`);
        if (input) {
            input.addEventListener("input", validateSettings);
            input.addEventListener("blur", validateSettings); // Also validate on blur
        } else {
            console.warn(`Validation input not found: ${name}`);
        }
    });

    // RPM validation
    for (let i = 1; i <= 7; i++) {
        const rpmInput = document.getElementById(`RPM${i}_input`);
        if (rpmInput) {
            rpmInput.addEventListener('input', validateRPMOrder);
            rpmInput.addEventListener('blur', validateRPMOrder);
        }
    }

    // Run initial validation
    setTimeout(() => {
        validateSettings();
        validateRPMOrder();
    }, 100); // Small delay to ensure all data is loaded
}
//Dynamic adjustment factors
function resetDynamicShuntGain() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }
    updatePasswordFields();

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("ResetDynamicShuntGain", "1");

    fetch("/get?" + new URLSearchParams(formData).toString())
        .then(() => {
            console.log("SOC gain factor reset requested");
        })
        .catch(err => console.error("Reset failed:", err));
}

function resetDynamicAltZero() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }
    updatePasswordFields();

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("ResetDynamicAltZero", "1");

    fetch("/get?" + new URLSearchParams(formData).toString())
        .then(() => {
            console.log("Alternator zero offset reset requested");
        })
        .catch(err => console.error("Reset failed:", err));
}

function updateLifeIndicators(data) {
    const indicatorColors = ['#00a19a', '#ff9800', '#f44336']; // Green, Yellow, Red
    const colorNames = ['Good', 'Caution', 'Critical'];

    const indicators = [
        { id: 'insulation-indicator', percent: data.InsulationLifePercent },
        { id: 'grease-indicator', percent: data.GreaseLifePercent },
        { id: 'brush-indicator', percent: data.BrushLifePercent },
        { id: 'predicted-indicator', hours: data.PredictedLifeHours }
    ];

    indicators.forEach(indicator => {
        const element = document.getElementById(indicator.id);
        if (!element) return;

        let colorIndex = 0; // Default green

        if (indicator.percent !== undefined) {
            // For percentage indicators
            if (indicator.percent < 20) colorIndex = 2;      // Red below 20%
            else if (indicator.percent < 50) colorIndex = 1; // Yellow below 50%
        } else if (indicator.hours !== undefined) {
            // For predicted hours indicator
            if (indicator.hours < 1000) colorIndex = 2;      // Red below 1000 hours
            else if (indicator.hours < 5000) colorIndex = 1; // Yellow below 5000 hours
        }

        element.style.color = indicatorColors[colorIndex];
        element.textContent = colorNames[colorIndex];
    });
}


