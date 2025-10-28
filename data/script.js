/* XREG_START */
/* 
 * AI_SUMMARY: Alternator Regulator Project - This file is the majority of the JS served by the ESP32
 * AI_PURPOSE: Realtime control of GPIO (settings always have echos) and viewing of sensor data and calculated values.  
 * AI_INPUTS: Payloads from the ESP32, including some variables used in this javascript such as webgaugesinterval (the ESP32 data delivery interval),timeAxisModeChanging (toggles a different style of X axis on plots generated here), plotTimeWindow (length of time to plot on X axis).  Server-Sent Events via /events endpoint
 * AI_OUTPUTS: Values submitted back to ESP32 via HTTP GET/POST requests to various endpoints
 * AI_DEPENDENCIES: 
 * AI_RISKS: Variable naming is inconsisent, need to be careful not to assume consistent patterns.   Unit conversion / scaling can be confusing and propogate to many places, have to trace dependencies in variables FULLY to every end point
 * AI_OPTIMIZE: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat.  When impossible, always mimik my style and coding patterns.
 * CRITICAL_INSTRUCTION_FOR_AI:: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat. When impossible, always mimick my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instructions for me to edit if function is long, to conserve tokens. Always specify which option you chose.  Never re-write the entire file, this just wastes my tokens.
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

//Profiling system
// Call every 30 seconds to see performance trends
setInterval(showPerformanceReport, 30000);
const ENABLE_DETAILED_PROFILING = true;
let performanceMetrics = {
    plotUpdates: [],
    domUpdates: [],
    echoUpdates: [],
    dataProcessing: []
};

// To add/remove Unix time labels on X axis.  No labels = cleaner render
// Time axis mode toggle
let useTimestamps = false; // Default to relative time (faster)
let timeAxisModeChanging = false; // Prevent conflicts during switch



function toggleTimeAxisMode(checkbox) {
    if (timeAxisModeChanging) return;

    timeAxisModeChanging = true;

    // Update the local variable immediately (don't wait for ESP32)
    useTimestamps = checkbox.checked;

    console.log(`Switching to ${useTimestamps ? 'timestamp' : 'relative time'} mode`);

    // Reinitialize data structures with new mode
    initPlotDataStructures();

    // Destroy and recreate all plots with new configuration
    if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
    if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
    if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
    if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

    //Reinitialize X-axis data for the new mode
    reinitializeXAxisForNewMode();

    timeAxisModeChanging = false;

}

//helper function
function reinitializeXAxisForNewMode() {
    // Get current interval from the last known data or use default
    const intervalMs = window._lastKnownInterval || 200; // fallback to 200ms
    const intervalSec = intervalMs / 1000;

    if (useTimestamps) {
        // SWITCHING TO TIMESTAMP MODE
        console.log("Initializing X-axis with timestamps");
        const now = Math.floor(Date.now() / 1000);

        // Initialize all plot X-axes with proper timestamps going back in time
        if (currentTempData && currentTempData[0]) {
            for (let i = 0; i < currentTempData[0].length; i++) {
                currentTempData[0][i] = now - (currentTempData[0].length - 1 - i) * intervalSec;
            }
        }

        if (voltageData && voltageData[0]) {
            for (let i = 0; i < voltageData[0].length; i++) {
                voltageData[0][i] = now - (voltageData[0].length - 1 - i) * intervalSec;
            }
        }

        if (rpmData && rpmData[0]) {
            for (let i = 0; i < rpmData[0].length; i++) {
                rpmData[0][i] = now - (rpmData[0].length - 1 - i) * intervalSec;
            }
        }

        if (temperatureData && temperatureData[0]) {
            for (let i = 0; i < temperatureData[0].length; i++) {
                temperatureData[0][i] = now - (temperatureData[0].length - 1 - i) * intervalSec;
            }
        }
    } else {
        // SWITCHING TO RELATIVE MODE
        console.log("Initializing X-axis with relative time");

        // Initialize all plot X-axes with relative time (seconds ago)
        if (currentTempData && currentTempData[0]) {
            for (let i = 0; i < currentTempData[0].length; i++) {
                currentTempData[0][i] = -(currentTempData[0].length - 1 - i) * intervalSec;
            }
        }

        if (voltageData && voltageData[0]) {
            for (let i = 0; i < voltageData[0].length; i++) {
                voltageData[0][i] = -(voltageData[0].length - 1 - i) * intervalSec;
            }
        }

        if (rpmData && rpmData[0]) {
            for (let i = 0; i < rpmData[0].length; i++) {
                rpmData[0][i] = -(rpmData[0].length - 1 - i) * intervalSec;
            }
        }

        if (temperatureData && temperatureData[0]) {
            for (let i = 0; i < temperatureData[0].length; i++) {
                temperatureData[0][i] = -(temperatureData[0].length - 1 - i) * intervalSec;
            }
        }
    }
}



// Plot Rendering Tracker -
let plotRenderTracker = {
    interval: 10000, // 10 seconds
    startTime: performance.now(),
    lastReportTime: performance.now(),

    // Per-plot counters
    plots: {
        current: { count: 0, totalTime: 0, maxTime: 0 },
        voltage: { count: 0, totalTime: 0, maxTime: 0 },
        rpm: { count: 0, totalTime: 0, maxTime: 0 },
        temperature: { count: 0, totalTime: 0, maxTime: 0 }
    },

    // Overall stats
    totalRenderTime: 0,
    peakRenderTime: 0,
    dataPointsProcessed: 0,
    queueCalls: 0
};

function reportPlotRenderingStats() {
    const now = performance.now();
    const intervalMs = now - plotRenderTracker.lastReportTime;
    const intervalSec = intervalMs / 1000;

    // Calculate stats for each plot
    const plotStats = {};
    let totalUpdates = 0;

    Object.entries(plotRenderTracker.plots).forEach(([name, plot]) => {
        const avgTime = plot.count > 0 ? (plot.totalTime / plot.count) : 0;
        plotStats[name] = {
            count: plot.count,
            avgTime: avgTime,
            maxTime: plot.maxTime,
            frequency: plot.count / intervalSec
        };
        totalUpdates += plot.count;
    });

    // Calculate overall metrics
    const totalRenderPercent = (plotRenderTracker.totalRenderTime / intervalMs) * 100;
    const avgDataRate = plotRenderTracker.dataPointsProcessed / intervalSec;
    const queueEfficiency = totalUpdates > 0 ? (totalUpdates / plotRenderTracker.queueCalls * 100) : 0;

    // Format the report
    const statsText = Object.entries(plotStats)
        .map(([name, stats]) =>
            `${name.charAt(0).toUpperCase()}=${stats.count} (${stats.avgTime.toFixed(1)}ms avg, ${stats.frequency.toFixed(1)}/s)`
        ).join(', ');

    console.log(`[PLOT RENDER REPORT] ${intervalSec.toFixed(1)}s: ${statsText} | Total: ${totalRenderPercent.toFixed(1)}% render time | Peak: ${plotRenderTracker.peakRenderTime.toFixed(1)}ms | Data: ${avgDataRate.toFixed(1)}/s | Queue eff: ${queueEfficiency.toFixed(0)}%`);

    // Reset counters for next interval
    Object.values(plotRenderTracker.plots).forEach(plot => {
        plot.count = 0;
        plot.totalTime = 0;
        plot.maxTime = 0;
    });

    plotRenderTracker.totalRenderTime = 0;
    plotRenderTracker.peakRenderTime = 0;
    plotRenderTracker.dataPointsProcessed = 0;
    plotRenderTracker.queueCalls = 0;
    plotRenderTracker.lastReportTime = now;
}

// Plot Y-axis limits (I am ready to make configurable from ESP32!)
let Ymin1 = -20, Ymax1 = 20; // Current plot
let Ymin2 = -20, Ymax2 = 20; // Voltage plot  
let Ymin3 = -20, Ymax3 = 20; // RPM plot
let Ymin4 = -20, Ymax4 = 20; // Temperature plot

// Circular buffer indices for efficiency
let currentTempIndex = 0;
let voltageIndex = 0;
let rpmIndex = 0;
let temperatureIndex = 0;

// Pre-calculated X-axis arrays
let xAxisData = [];

// Efficient circular buffer structure - no timestamps needed
let currentTempData, voltageData, rpmData, temperatureData;

//from chatgpt: Every call to init*Plot() attaches a new ResizeObserver with a 1000 ms debounce. 
//But these functions are reentrant if called twice, and you don't unregister observers. This can stack up and cause redundant resizing logic at runtime.
//Fix: Store and disconnect previous observers before creating new ones.
let currentTempResizeObserver = null;
let voltageResizeObserver = null;
let rpmResizeObserver = null;
let temperatureResizeObserver = null;

let currentTempPlot;
let voltagePlot;
let rpmPlot;
let temperaturePlot;


// Plot update batching system
const plotUpdateQueue = new Set();
let plotUpdateScheduled = false;


function queuePlotUpdate(plotName) {
    // Lightweight tracking - just increment counter
    plotRenderTracker.queueCalls++;

    plotUpdateQueue.add(plotName);

    if (!plotUpdateScheduled) {
        plotUpdateScheduled = true;
        requestAnimationFrame(() => {
            // Track render start
            const totalStart = performance.now();

            if (plotUpdateQueue.has('current') && currentTempPlot) {
                const start = performance.now();
                currentTempPlot.setData(currentTempData);
                const duration = performance.now() - start;

                // Update tracker (minimal overhead)
                const plot = plotRenderTracker.plots.current;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            if (plotUpdateQueue.has('voltage') && voltagePlot) {
                const start = performance.now();
                voltagePlot.setData(voltageData);
                const duration = performance.now() - start;

                const plot = plotRenderTracker.plots.voltage;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            if (plotUpdateQueue.has('rpm') && rpmPlot) {
                const start = performance.now();
                rpmPlot.setData(rpmData);
                const duration = performance.now() - start;

                const plot = plotRenderTracker.plots.rpm;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            if (plotUpdateQueue.has('temperature') && temperaturePlot) {
                const start = performance.now();
                temperaturePlot.setData(temperatureData);
                const duration = performance.now() - start;

                const plot = plotRenderTracker.plots.temperature;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            const totalDuration = performance.now() - totalStart;

            // Update overall tracker
            plotRenderTracker.totalRenderTime += totalDuration;
            plotRenderTracker.peakRenderTime = Math.max(plotRenderTracker.peakRenderTime, totalDuration);

            plotUpdateQueue.clear();
            plotUpdateScheduled = false;
        });
    }
}

//ota update stuff

// Software Update functionality
let availableVersions = {};

// Load available versions from server
function loadAvailableVersions() {
    // Require unlock first
    if (!currentAdminPassword) {
        alert('Please unlock settings first (enter admin password)');
        return;
    }

    const versionList = document.getElementById('version-list');
    const versionLoading = document.getElementById('version-loading');

    if (window._lastKnownMode === 1) { // MODE_AP
        document.getElementById('version-loading').style.display = 'block';
        document.getElementById('version-loading').innerHTML =
            '<div style="color: #ff9800; text-align: center; padding: 20px;">' +
            'Software updates unavailable in AP mode<br>' +
            '<small>Connect device to ship\'s WiFi for updates</small>' +
            '</div>';
        document.getElementById('version-list').style.display = 'none';
        return;
    }

    document.getElementById('version-loading').style.display = 'block';
    document.getElementById('version-list').style.display = 'none';

    fetch('https://ota.xengineering.net/api/firmware/versions.php')
        .then(response => response.json())
        .then(data => {
            availableVersions = data.versions || {};
            displayAvailableVersions();
        })
        .catch(error => {
            console.error('Error loading versions:', error);
            document.getElementById('version-loading').innerHTML =
                '<div style="color: red;">Error loading versions. Check internet connection.</div>';
        });
}

function getStoredPassword() {
    // Get password from an existing form that already has it populated
    const existingPasswordField = document.querySelector('.password_field');
    return existingPasswordField ? existingPasswordField.value : '';
}
// Display available versions
function displayAvailableVersions() {
    const versionList = document.getElementById('version-list');
    const versionLoading = document.getElementById('version-loading');

    if (Object.keys(availableVersions).length === 0) {
        versionLoading.innerHTML = '<div>No versions available</div>';
        return;
    }

    // Get current version for comparison
    const currentVersionStr = document.getElementById('current-version-display').textContent;

    // Filter out the current version, but allow all others (including downgrades)
    const sortedVersions = Object.keys(availableVersions)
        .filter(version => version !== currentVersionStr)
        .sort((a, b) => {
            // Parse version strings like "0.2.12" into comparable numbers
            const parseVersion = (v) => {
                const parts = v.split('.').map(Number);
                return parts[0] * 1000000 + parts[1] * 1000 + parts[2];
            };
            return parseVersion(b) - parseVersion(a); // Sort descending (highest first)
        });

    if (sortedVersions.length === 0) {
        versionLoading.innerHTML = '<div>No other versions available</div>';
        return;
    }

    let html = '<div style="margin: 10px 0;">Click a version to update:</div>';
    sortedVersions.forEach(version => {
        const versionData = availableVersions[version];
        const notes = versionData.notes || 'No description available';
        const size = versionData.file_size ? (versionData.file_size / (1024 * 1024)).toFixed(1) + ' MB' : '';

        html += `
            <div style="border: 1px solid #ccc; margin: 10px 0; padding: 15px; border-radius: 5px;">
                <div style="display: flex; justify-content: space-between; align-items: center;">
                    <div>
                        <strong>Version ${version}</strong><br>
                        <span style="color: #666; font-size: 0.9em;">${notes}</span><br>
                        <span style="color: #888; font-size: 0.8em;">${size}</span>
                    </div>
                    <form action="/get" method="GET" target="hidden-form" onsubmit="return confirmUpdate('${version}')">
                        <input type="hidden" name="password" value="${currentAdminPassword}">
                        <input type="hidden" name="UpdateToVersion" value="${version}">
                        <input type="submit" value="Change to ${version}" class="btn-primary">
                    </form>
                </div>
            </div>
        `;
    });

    versionLoading.style.display = 'none';
    versionList.innerHTML = html;
    versionList.style.display = 'block';
}

// Confirm update
function confirmUpdate(version) {
    // Password is guaranteed to exist because button can only be clicked after unlock
    return confirm(`Update process takes 2-3 minutes and involves one or more auto re-boots.  Do not interfere.  When finished, web interface will be accessible in the usual way, and Software Update tab will show the new version # is active.  If process fails, you may try again with better internet.  If the whole thing bricks, you may start fresh with the factory golden image.`);
}

// Decode and display version/device ID from CSV data
// Track previous values to detect changes
let prevVersionInt = -1;
let prevDeviceIdUpper = -1;
let prevDeviceIdLower = -1;

function updateVersionInfo() {
    // Get current values
    const versionInt = parseInt(document.getElementById('firmwareVersionIntID').textContent) || 0;
    const deviceIdUpper = parseInt(document.getElementById('deviceIdUpperID').textContent) || 0;
    const deviceIdLower = parseInt(document.getElementById('deviceIdLowerID').textContent) || 0;

    // Only update if something changed
    if (versionInt !== prevVersionInt || deviceIdUpper !== prevDeviceIdUpper || deviceIdLower !== prevDeviceIdLower) {

        // Decode version
        const major = Math.floor(versionInt / 10000);
        const minor = Math.floor((versionInt % 10000) / 100);
        const patch = versionInt % 100;
        const versionStr = `${major}.${minor}.${patch}`;

        // Decode device ID (12-char MAC)
        const deviceIdHex = deviceIdUpper.toString(16).padStart(8, '0') +
            deviceIdLower.toString(16).padStart(8, '0');
        const macAddress = deviceIdHex.slice(4).toUpperCase();

        // Update display
        document.getElementById('current-version-display').textContent = versionStr;
        document.getElementById('device-id-display').textContent = macAddress;

        // Update previous values
        prevVersionInt = versionInt;
        prevDeviceIdUpper = deviceIdUpper;
        prevDeviceIdLower = deviceIdLower;
    }
}


//profiling stuff
function profileOperation(operationName, fn) {
    if (!ENABLE_DETAILED_PROFILING) return fn();

    const startTime = performance.now();
    const result = fn();
    const duration = performance.now() - startTime;

    if (duration > 1) { // Only log operations taking >1ms
        if (!performanceMetrics[operationName]) {
            performanceMetrics[operationName] = [];
        }
        performanceMetrics[operationName].push(duration);

        // Keep only last 50 measurements
        if (performanceMetrics[operationName].length > 50) {
            performanceMetrics[operationName].shift();
        }

        console.log(`[PROF] ${operationName}: ${duration.toFixed(2)}ms`);
    }

    return result;
}
function showPerformanceReport() {
    console.log('\n=== PERFORMANCE REPORT ===');
    Object.entries(performanceMetrics).forEach(([operation, times]) => {
        if (times.length > 0) {
            const avg = times.reduce((a, b) => a + b) / times.length;
            const max = Math.max(...times);
            console.log(`${operation}: avg=${avg.toFixed(2)}ms, max=${max.toFixed(2)}ms, samples=${times.length}`);
        }
    });
}
// ECHO UPDATE - Only update changed values
let lastEchoValues = new Map();

function updateEchoIfChanged(elementId, newValue) {
    const cacheKey = elementId;
    const lastValue = lastEchoValues.get(cacheKey);

    if (lastValue !== newValue) {
        lastEchoValues.set(cacheKey, newValue);
        const element = document.getElementById(elementId);
        if (element) {  // Add this null check
            element.textContent = newValue;
        } else {
            console.warn(`Element not found: ${elementId}`);  // Debug which element is missing
        }
        return true;
    }
    return false;
}

function startFrameTimeMonitoring() {
    function frame() {
        trackFrameTime();
        requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
}

//DOM Updates
function scheduleDOMUpdateOptimized(elementId, newContent) {
    // Check if content actually changed before scheduling
    const element = document.getElementById(elementId);
    if (element && element.textContent === newContent) {
        return false; // No change needed
    }

    pendingDOMUpdates.set(elementId, newContent);

    if (!domUpdateScheduled) {
        domUpdateScheduled = true;
        requestAnimationFrame(() => {
            profileOperation('domUpdates', () => {
                let updateCount = 0;
                for (const [id, content] of pendingDOMUpdates) {
                    const element = document.getElementById(id);
                    if (element && element.textContent !== content) {
                        element.textContent = content;
                        updateCount++;
                    }
                }
                // console.log(`[PROF] DOM updates: ${updateCount} elements updated`);
            });

            pendingDOMUpdates.clear();
            domUpdateScheduled = false;
        });
    }
    return true; // Update scheduled
}

// PERFORMANCE MONITORING
let frameTimeTracker = {
    lastFrameTime: performance.now(),
    frameTimes: [],
    worstFrame: 0
};

function trackFrameTime() {
    const now = performance.now();
    const frameTime = now - frameTimeTracker.lastFrameTime;
    frameTimeTracker.lastFrameTime = now;

    if (frameTimeTracker.frameTimes.length > 0) { // Skip first frame
        frameTimeTracker.frameTimes.push(frameTime);
        frameTimeTracker.worstFrame = Math.max(frameTimeTracker.worstFrame, frameTime);

        // Keep last 100 frames
        if (frameTimeTracker.frameTimes.length > 100) {
            frameTimeTracker.frameTimes.shift();
        }

        // Log if frame took too long
        if (frameTime > 20) { // More than 20ms = under 50fps
            console.warn(`[PERF] Slow frame: ${frameTime.toFixed(2)}ms`);
        }
    }
}

//globals for axes configuration from ESP32
const CONFIG_CHECK_INTERVAL_SECONDS = 1.0;  // How often to check for config changes (seconds)

// Cache variables for change detection
let cachedPlotTimeWindow = null;
let cachedWebgaugesInterval = null;
let cachedYmin1 = null, cachedYmax1 = null;
let cachedYmin2 = null, cachedYmax2 = null;
let cachedYmin3 = null, cachedYmax3 = null;
let cachedYmin4 = null, cachedYmax4 = null;

// Counters for timing
let configCheckCounter = 0;
// Function to calculate how many loops equal the config check interval
function getConfigCheckInterval(webgaugesIntervalMs) {
    if (!webgaugesIntervalMs || webgaugesIntervalMs <= 0) return 5; // fallback
    return Math.max(1, Math.round((CONFIG_CHECK_INTERVAL_SECONDS * 1000) / webgaugesIntervalMs));
}

// wrapper function
function processCSVDataOptimized(data) {
    return profileOperation('dataProcessing', () => {

        // Increment data points counter
        plotRenderTracker.dataPointsProcessed++;

        // Batch all plot updates
        const plotUpdates = [];
        const now = useTimestamps ? Math.floor(Date.now() / 1000) : null;

        // ALWAYS UPDATE DATA STRUCTURES - Current/Temperature plot data
        const battCurrent = 'Bcur' in data ? parseFloat(data.Bcur) / 100 : 0;
        const altCurrent = 'MeasuredAmps' in data ? parseFloat(data.MeasuredAmps) / 100 : 0;
        const fieldCurrent = 'iiout' in data ? parseFloat(data.iiout) / 10 : 0;

        // Shift all current data left and add new data at the end
        for (let i = 1; i < currentTempData[1].length; i++) {
            if (useTimestamps) {
                currentTempData[0][i - 1] = currentTempData[0][i]; // Shift timestamps
            }
            currentTempData[1][i - 1] = currentTempData[1][i];
            currentTempData[2][i - 1] = currentTempData[2][i];
            currentTempData[3][i - 1] = currentTempData[3][i];
        }
        // Add new current data at the end (rightmost position)
        const lastCurrentIndex = currentTempData[1].length - 1;
        if (useTimestamps) {
            currentTempData[0][lastCurrentIndex] = now; // New timestamp
        }
        currentTempData[1][lastCurrentIndex] = battCurrent;
        currentTempData[2][lastCurrentIndex] = altCurrent;
        currentTempData[3][lastCurrentIndex] = fieldCurrent;

        // Only update visual plot if it exists
        if (typeof currentTempPlot !== 'undefined') {
            plotUpdates.push('current');
        }

        // ALWAYS UPDATE DATA STRUCTURES - Voltage plot data
        const adsBattV = 'BatteryV' in data ? parseFloat(data.BatteryV) / 100 : 0;
        const inaBattV = 'IBV' in data ? parseFloat(data.IBV) / 100 : 0;

        // Shift all voltage data left and add new data at the end
        for (let i = 1; i < voltageData[1].length; i++) {
            if (useTimestamps) {
                voltageData[0][i - 1] = voltageData[0][i]; // Shift timestamps
            }
            voltageData[1][i - 1] = voltageData[1][i];
            voltageData[2][i - 1] = voltageData[2][i];
        }
        const lastVoltageIndex = voltageData[1].length - 1;
        if (useTimestamps) {
            voltageData[0][lastVoltageIndex] = now; // New timestamp
        }
        voltageData[1][lastVoltageIndex] = adsBattV;
        voltageData[2][lastVoltageIndex] = inaBattV;

        // Only update visual plot if it exists
        if (typeof voltagePlot !== 'undefined') {
            plotUpdates.push('voltage');
        }

        // ALWAYS UPDATE DATA STRUCTURES - RPM plot data
        const rpmValue = 'RPM' in data ? parseFloat(data.RPM) : 0;

        // Shift all RPM data left and add new data at the end
        for (let i = 1; i < rpmData[1].length; i++) {
            if (useTimestamps) {
                rpmData[0][i - 1] = rpmData[0][i]; // Shift timestamps
            }
            rpmData[1][i - 1] = rpmData[1][i];
        }
        const lastRPMIndex = rpmData[1].length - 1;
        if (useTimestamps) {
            rpmData[0][lastRPMIndex] = now; // New timestamp
        }
        rpmData[1][lastRPMIndex] = rpmValue;

        // Only update visual plot if it exists
        if (typeof rpmPlot !== 'undefined') {
            plotUpdates.push('rpm');
        }

        // ALWAYS UPDATE DATA STRUCTURES - Temperature plot data
        const altTemp = 'AlternatorTemperatureF' in data ? parseFloat(data.AlternatorTemperatureF) : 0;

        // Shift all temperature data left and add new data at the end
        for (let i = 1; i < temperatureData[1].length; i++) {
            if (useTimestamps) {
                temperatureData[0][i - 1] = temperatureData[0][i]; // Shift timestamps
            }
            temperatureData[1][i - 1] = temperatureData[1][i];
        }
        const lastTempIndex = temperatureData[1].length - 1;
        if (useTimestamps) {
            temperatureData[0][lastTempIndex] = now; // New timestamp
        }
        temperatureData[1][lastTempIndex] = altTemp;

        // Only update visual plot if it exists
        if (typeof temperaturePlot !== 'undefined') {
            plotUpdates.push('temperature');
        }

        // Queue all plot updates at once (only for existing plots)
        plotUpdates.forEach(plotName => queuePlotUpdate(plotName));

        return plotUpdates.length;
    });
}



// Function to update plot configuration when parameters change
function updatePlotConfiguration(data) {



    let configChanged = false;

    // Check for any axis limit changes
    const axisChanged = (data.Ymin1 !== cachedYmin1 || data.Ymax1 !== cachedYmax1 ||
        data.Ymin2 !== cachedYmin2 || data.Ymax2 !== cachedYmax2 ||
        data.Ymin3 !== cachedYmin3 || data.Ymax3 !== cachedYmax3 ||
        data.Ymin4 !== cachedYmin4 || data.Ymax4 !== cachedYmax4);

    if (axisChanged) {
        //  console.log('AXIS LIMITS CHANGED - Recreating plots');

        // Update all global axis variables
        Ymin1 = data.Ymin1; Ymax1 = data.Ymax1;
        Ymin2 = data.Ymin2; Ymax2 = data.Ymax2;
        Ymin3 = data.Ymin3; Ymax3 = data.Ymax3;
        Ymin4 = data.Ymin4; Ymax4 = data.Ymax4;

        // Update cached values
        cachedYmin1 = data.Ymin1; cachedYmax1 = data.Ymax1;
        cachedYmin2 = data.Ymin2; cachedYmax2 = data.Ymax2;
        cachedYmin3 = data.Ymin3; cachedYmax3 = data.Ymax3;
        cachedYmin4 = data.Ymin4; cachedYmax4 = data.Ymax4;

        // Destroy and recreate all plots
        if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
        if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
        if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
        if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

        configChanged = true;
    }

    // Check for buffer size changes (requires fresh start)
    if (data.plotTimeWindow !== cachedPlotTimeWindow ||
        data.webgaugesinterval !== cachedWebgaugesInterval) {

        cachedPlotTimeWindow = data.plotTimeWindow;
        cachedWebgaugesInterval = data.webgaugesinterval;

        // Reinitialize plots with new timing parameters
        reinitializePlotsWithNewTiming(data);

        // Destroy and recreate all plots to fix X-axis labels
        if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
        if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
        if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
        if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

        configChanged = true;
    }

    if (configChanged) {
        //  console.log('Plot configuration updated');
    }
}

// Function to reinitialize plots when timing parameters change
function reinitializePlotsWithNewTiming(data) {
    // Calculate new buffer size
    const newMaxPoints = Math.ceil((data.plotTimeWindow * 1000) / data.webgaugesinterval);
    const now = Math.floor(Date.now() / 1000);
    const intervalSec = data.webgaugesinterval / 1000;

    // Recalculate X-axis (timestamps going back in time)
    xAxisData = [];
    if (useTimestamps) {
        const now = Math.floor(Date.now() / 1000);
        for (let i = 0; i < newMaxPoints; i++) {
            xAxisData[i] = now - (newMaxPoints - 1 - i) * intervalSec;
        }
    } else {
        for (let i = 0; i < newMaxPoints; i++) {
            xAxisData[i] = -(newMaxPoints - 1 - i) * intervalSec;
        }
    }

    // Reinitialize circular buffers with new size
    currentTempData = [
        [...xAxisData], // X values (timestamps)
        new Array(newMaxPoints).fill(0), // Battery current
        new Array(newMaxPoints).fill(0), // Alt current  
        new Array(newMaxPoints).fill(0)  // Field current
    ];

    voltageData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0), // ADS voltage
        new Array(newMaxPoints).fill(0)  // INA voltage
    ];

    rpmData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0)  // RPM
    ];

    temperatureData = [
        [...xAxisData], // X values  
        new Array(newMaxPoints).fill(0)  // Temperature
    ];

    // Reset circular buffer indices
    currentTempIndex = 0;
    voltageIndex = 0;
    rpmIndex = 0;
    temperatureIndex = 0;

    // Update plots - they'll auto-scale with time: true
    if (currentTempPlot) {
        currentTempPlot.setData(currentTempData);
    }
    if (voltagePlot) {
        voltagePlot.setData(voltageData);
    }
    if (rpmPlot) {
        rpmPlot.setData(rpmData);
    }
    if (temperaturePlot) {
        temperaturePlot.setData(temperatureData);
    }

    console.log(`Plots reinitialized: ${newMaxPoints} points, ${data.plotTimeWindow / 1000}s window`);
}


// Function to update all echo values
function updateAllEchosOptimized(data) {
    return profileOperation('echoUpdates', () => {
        let updatesCount = 0;

        // All echo updates with change detection 
        const echoUpdates = [
            { key: 'TemperatureLimitF', id: 'TemperatureLimitF_echo', transform: v => v },
            { key: 'BulkVoltage', id: 'BulkVoltage_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'TargetAmps', id: 'TargetAmps_echo', transform: v => v },
            { key: 'FloatVoltage', id: 'FloatVoltage_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'SwitchingFrequency', id: 'SwitchingFrequency_echo', transform: v => v },
            { key: 'dutyStep', id: 'dutyStep_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'FieldAdjustmentInterval', id: 'FieldAdjustmentInterval_echo', transform: v => v },
            { key: 'ManualDutyTarget', id: 'ManualDutyTarget_echo', transform: v => v },
            { key: 'SwitchControlOverride', id: 'SwitchControlOverride_echo', transform: v => v },
            { key: 'OnOff', id: 'OnOff_echo', transform: v => v },
            { key: 'ManualFieldToggle', id: 'ManualFieldToggle_echo', transform: v => v },
            { key: 'HiLow', id: 'HiLow_echo', transform: v => v },
            { key: 'LimpHome', id: 'LimpHome_echo', transform: v => v },
            { key: 'VeData', id: 'VeData_echo', transform: v => v },
            { key: 'NMEA0183Data', id: 'NMEA0183Data_echo', transform: v => v },
            { key: 'NMEA2KData', id: 'NMEA2KData_echo', transform: v => v },
            { key: 'TargetAmpL', id: 'TargetAmpL_echo', transform: v => v },
            { key: 'CurrentThreshold', id: 'CurrentThreshold_echo', transform: v => v / 100 },
            { key: 'PeukertExponent', id: 'PeukertExponent_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'ChargeEfficiency', id: 'ChargeEfficiency_echo', transform: v => v + '%' },
            { key: 'ChargedVoltage', id: 'ChargedVoltage_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'TailCurrent', id: 'TailCurrent_echo', transform: v => v },
            { key: 'ChargedDetectionTime', id: 'ChargedDetectionTime_echo', transform: v => v },
            { key: 'IgnoreTemperature', id: 'IgnoreTemperature_echo', transform: v => v },
            { key: 'bmsLogic', id: 'bmsLogic_echo', transform: v => v },
            { key: 'bmsLogicLevelOff', id: 'bmsLogicLevelOff_echo', transform: v => v },
            { key: 'AlarmActivate', id: 'AlarmActivate_echo', transform: v => v },
            { key: 'TempAlarm', id: 'TempAlarm_echo', transform: v => v },
            { key: 'VoltageAlarmHigh', id: 'VoltageAlarmHigh_echo', transform: v => v },
            { key: 'VoltageAlarmLow', id: 'VoltageAlarmLow_echo', transform: v => v },
            { key: 'CurrentAlarmHigh', id: 'CurrentAlarmHigh_echo', transform: v => v },
            { key: 'FourWay', id: 'FourWay_echo', transform: v => v },
            { key: 'RPMScalingFactor', id: 'RPMScalingFactor_echo', transform: v => v },
            { key: 'ResetTemp', id: 'ResetTemp_echo', transform: v => v },
            { key: 'ResetVoltage', id: 'ResetVoltage_echo', transform: v => v },
            { key: 'ResetCurrent', id: 'ResetCurrent_echo', transform: v => v },
            { key: 'ResetEngineRunTime', id: 'ResetEngineRunTime_echo', transform: v => v },
            { key: 'ResetAlternatorOnTime', id: 'ResetAlternatorOnTime_echo', transform: v => v },
            { key: 'ResetEnergy', id: 'ResetEnergy_echo', transform: v => v },
            { key: 'MaximumAllowedBatteryAmps', id: 'MaximumAllowedBatteryAmps_echo', transform: v => v },
            { key: 'ManualSOCPoint', id: 'ManualSOCPoint_echo', transform: v => v },
            { key: 'BatteryVoltageSource', id: 'BatteryVoltageSource_echo', transform: v => v },
            { key: 'ShuntResistanceMicroOhm', id: 'ShuntResistanceMicroOhm_echo', transform: v => v },
            { key: 'InvertAltAmps', id: 'InvertAltAmps_echo', transform: v => v },
            { key: 'InvertBattAmps', id: 'InvertBattAmps_echo', transform: v => v },
            { key: 'MaxDuty', id: 'MaxDuty_echo', transform: v => v },
            { key: 'MinDuty', id: 'MinDuty_echo', transform: v => v },
            { key: 'FieldResistance', id: 'FieldResistance_echo', transform: v => v },
            { key: 'maxPoints', id: 'maxPoints_echo', transform: v => v },
            { key: 'AlternatorCOffset', id: 'AlternatorCOffset_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'BatteryCOffset', id: 'BatteryCOffset_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'BatteryCapacity_Ah', id: 'BatteryCapacity_Ah_echo', transform: v => v },
            { key: 'R_fixed', id: 'R_fixed_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'Beta', id: 'Beta_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'T0_C', id: 'T0_C_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'TempSource', id: 'TempSource_echo', transform: v => v },
            { key: 'IgnitionOverride', id: 'IgnitionOverride_echo', transform: v => v },
            { key: 'AmpSrc', id: 'AmpSrc_echo', transform: v => v },
            { key: 'AlarmLatchEnabled', id: 'AlarmLatchEnabled_echo', transform: v => v },
            { key: 'AlarmTest', id: 'AlarmTest_echo', transform: v => v },
            { key: 'ResetAlarmLatch', id: 'ResetAlarmLatch_echo', transform: v => v },
            { key: 'ForceFloat', id: 'ForceFloat_echo', transform: v => v },
            { key: 'bulkCompleteTime', id: 'bulkCompleteTime_echo', transform: v => v },
            { key: 'FLOAT_DURATION', id: 'FLOAT_DURATION_echo', transform: v => Math.round(v / 3600) },
            { key: 'AutoShuntGainCorrection', id: 'AutoShuntGainCorrection_echo', transform: v => v },
            { key: 'AutoAltCurrentZero', id: 'AutoAltCurrentZero_echo', transform: v => v },
            { key: 'WindingTempOffset', id: 'WindingTempOffset_echo', transform: v => v },
            { key: 'PulleyRatio', id: 'PulleyRatio_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'ManualLifePercentage', id: 'ManualLifePercentage_echo', transform: v => v },
            { key: 'BatteryCurrentSource', id: 'BatteryCurrentSource_echo', transform: v => v },
            { key: 'timeAxisModeChanging', id: 'timeAxisModeChanging_echo', transform: v => v },
            { key: 'webgaugesinterval', id: 'webgaugesinterval_echo', transform: v => v },
            { key: 'plotTimeWindow', id: 'plotTimeWindow_echo', transform: v => v },
            { key: 'Ymin1', id: 'Ymin1_echo', transform: v => v },
            { key: 'Ymax1', id: 'Ymax1_echo', transform: v => v },
            { key: 'Ymin2', id: 'Ymin2_echo', transform: v => v },
            { key: 'Ymax2', id: 'Ymax2_echo', transform: v => v },
            { key: 'Ymin3', id: 'Ymin3_echo', transform: v => v },
            { key: 'Ymax3', id: 'Ymax3_echo', transform: v => v },
            { key: 'Ymin4', id: 'Ymin4_echo', transform: v => v },
            { key: 'Ymax4', id: 'Ymax4_echo', transform: v => v },
            { key: 'weatherModeEnabled', id: 'weatherModeEnabled_echo', transform: v => v },
            { key: 'SolarWatts', id: 'SolarWatts_echo', transform: v => v },
            { key: 'performanceRatio', id: 'performanceRatio_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'UVThresholdHigh', id: 'UVThresholdHigh_echo', transform: v => v },
            // PID Learning System echo updates (add these to the echoUpdates array)
            { key: 'LearningMode', id: 'LearningMode_echo', transform: v => v },
            { key: 'LearningPaused', id: 'LearningPaused_echo', transform: v => v },
            { key: 'LearningDryRunMode', id: 'LearningDryRunMode_echo', transform: v => v },
            { key: 'LearningUpwardEnabled', id: 'LearningUpwardEnabled_echo', transform: v => v },
            { key: 'LearningDownwardEnabled', id: 'LearningDownwardEnabled_echo', transform: v => v },
            { key: 'EnableNeighborLearning', id: 'EnableNeighborLearning_echo', transform: v => v },
            { key: 'AutoSaveLearningTable', id: 'AutoSaveLearningTable_echo', transform: v => v },
            { key: 'ShowLearningDebugMessages', id: 'ShowLearningDebugMessages_echo', transform: v => v },
            { key: 'EXTRA', id: 'EXTRA_echo', transform: v => v },
            { key: 'AlternatorNominalAmps', id: 'AlternatorNominalAmps_echo', transform: v => v },
            { key: 'LearningUpStep', id: 'LearningUpStep_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'LearningDownStep', id: 'LearningDownStep_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'MinLearningInterval', id: 'MinLearningInterval_echo', transform: v => (v / 1000).toFixed(2) },
            { key: 'SafeOperationThreshold', id: 'SafeOperationThreshold_echo', transform: v => (v / 1000).toFixed(2) },
            { key: 'PidKp', id: 'PidKp_echo', transform: v => (v / 1000).toFixed(3) },
            { key: 'PidKi', id: 'PidKi_echo', transform: v => (v / 1000).toFixed(3) },
            { key: 'PidKd', id: 'PidKd_echo', transform: v => (v / 1000).toFixed(3) },
            { key: 'PidSampleTime', id: 'PidSampleTime_echo', transform: v => v },
            { key: 'IgnoreLearningDuringPenalty', id: 'IgnoreLearningDuringPenalty_echo', transform: v => v },
            { key: 'AmbientTempCorrectionFactor', id: 'AmbientTempCorrectionFactor_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'AmbientTempBaseline', id: 'AmbientTempBaseline_echo', transform: v => v },
            { key: 'MaxTableValue', id: 'MaxTableValue_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'MinTableValue', id: 'MinTableValue_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'MaxPenaltyPercent', id: 'MaxPenaltyPercent_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'MaxPenaltyDuration', id: 'MaxPenaltyDuration_echo', transform: v => v },
            { key: 'NeighborLearningFactor', id: 'NeighborLearningFactor_echo', transform: v => (v / 1000).toFixed(3) },
            { key: 'LearningRPMSpacing', id: 'LearningRPMSpacing_echo', transform: v => v },
            { key: 'LearningTableSaveInterval', id: 'LearningTableSaveInterval_echo', transform: v => v },
            { key: 'pidInput', id: 'pidInput_display', transform: v => (v / 100).toFixed(1) },
            { key: 'pidSetpoint', id: 'pidSetpoint_display', transform: v => (v / 100).toFixed(1) },
            { key: 'pidOutput', id: 'pidOutput_display', transform: v => (v / 100).toFixed(1) }

        ];

        // Special display elements (not echos but similar update pattern)
        const specialDisplays = [
            { key: 'DynamicShuntGainFactor', id: 'DynamicShuntGainFactor_display', transform: v => (v / 1000).toFixed(3) },
            { key: 'DynamicAltCurrentZero', id: 'DynamicAltCurrentZero_display', transform: v => (v / 1000).toFixed(3) + ' A' },
            { key: 'totalPowerCycles', id: 'totalPowerCyclesID', transform: v => v },
            {
                key: 'AlarmLatchState',
                id: 'AlarmLatchState_display',
                transform: v => v,
                special: (el, value) => {
                    if (value === 1) {
                        el.textContent = 'LATCHED';
                        el.style.color = '#ff0000';
                    } else {
                        el.textContent = 'Normal';
                        el.style.color = 'var(--accent)';
                    }
                }
            }
        ];

        // Update only changed echo values
        echoUpdates.forEach(update => {
            if (update.key in data) {
                const newValue = update.transform(data[update.key]);
                if (updateEchoIfChanged(update.id, newValue)) {
                    updatesCount++;
                }
            }
        });

        // Update special displays
        specialDisplays.forEach(display => {
            if (display.key in data) {
                const el = document.getElementById(display.id);
                if (el) {
                    if (display.special) {
                        display.special(el, data[display.key]);
                        updatesCount++;
                    } else {
                        const newValue = display.transform(data[display.key]);
                        if (updateEchoIfChanged(display.id, newValue)) {
                            updatesCount++;
                        }
                    }
                }
            }
        });

        //  console.log(`[PROF] Echo updates: ${updatesCount} of ${echoUpdates.length + specialDisplays.length} elements changed`);
        return updatesCount;
    });
}

// end axes configuration functions
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
let userInitiatedToggles = new Map();
let lastValues = new Map(); // Cache for DOM updates
// DOM update batching system
let pendingDOMUpdates = new Map();
let domUpdateScheduled = false;

function scheduleDOMUpdate(elementId, newContent) {
    pendingDOMUpdates.set(elementId, newContent);

    if (!domUpdateScheduled) {
        domUpdateScheduled = true;
        requestAnimationFrame(() => {

            for (const [id, content] of pendingDOMUpdates) {
                const element = document.getElementById(id);
                if (element && element.textContent !== content) {
                    element.textContent = content;
                }
            }
            pendingDOMUpdates.clear();
            domUpdateScheduled = false;
        });
    }
}

//feedback on button clicking
function submitMessage() {
    // Use 'this' which refers to the element that called the function
    if (this && this.classList) {
        this.classList.add('button-pulse');
        setTimeout(() => this.classList.remove('button-pulse'), 600);
    }
}

//this allows user to turn the alternator OFF even without entering a passoword, for safety reasons, but not ON
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
    const plotControls = document.getElementById('plots-controls');
    if (plotControls) plotControls.classList.add("locked");
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
        "This will restore settings to factory defaults.\n" +
        "Device will restart.\n" +
        "All your settings will be lost.\n\n" +
        "Are you sure you want to continue?"
    );
    if (!confirmation) return;

    const button = document.getElementById('factory-reset-btn');
    button.disabled = true;
    button.textContent = 'Resetting...';
    button.style.backgroundColor = '#6c757d';

    const formData = new FormData();
    formData.append("password", currentAdminPassword);

    // Add timeout to fetch request
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 10000); // 10 second timeout

    fetch("/factoryReset", {
        method: "POST",
        body: formData,
        signal: controller.signal
    })
        .then(response => {
            clearTimeout(timeoutId);
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}: ${response.statusText}`);
            }
            return response.text();
        })
        .then(text => {
            console.log("Factory reset response:", text);
            if (text.trim() === "OK") {
                alert("Factory reset complete!\n\nThe device will restart.\nPage will reload in 5 seconds.");
                setTimeout(() => {
                    window.location.reload();
                }, 5000);
            } else {
                throw new Error("Unexpected response: " + text);
            }
        })
        .catch(err => {
            clearTimeout(timeoutId);
            console.error("Factory reset error:", err);

            // Timeout or network error means the device is resetting (expected behavior)
            if (err.name === 'AbortError' || err.message.includes('Failed to fetch')) {
                alert("Factory reset complete!\n\nThe device is restarting.\n\nPage will reload in some seconds.");
                setTimeout(() => {
                    window.location.reload();
                }, 8000);
            } else {
                // Unexpected error
                alert("Error during factory reset:\n\n" + err.message);
                button.disabled = false;
                button.textContent = 'Restore Defaults';
                button.style.backgroundColor = '#555555';
            }
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

//Mirror for Alarm
function updateAlarmStatus(data) {
    const alarmLed = document.getElementById('alarm-led');
    const alarmText = document.getElementById('alarm-status-text');

    if (!alarmLed || !alarmText) return;

    const isAlarming = data.Alarm_Status === 1; // Mirror Alarm directly

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
function initPlotDataStructures() {
    // Get actual values from ESP32 data, or use defaults
    const intervalMs = window._lastKnownInterval || 200;
    const timeWindowSec = window._lastKnownTimeWindow || 8; // This is in SECONDS
    const timeWindowMs = timeWindowSec * 1000; // Convert to milliseconds
    // Calculate correct buffer size
    const maxPoints = Math.ceil(timeWindowMs / intervalMs);
    const intervalSec = intervalMs / 1000;

    //console.log(`Buffer: ${maxPoints} points, ${timeWindowMs / 1000}s window, ${intervalMs}ms interval`);

    if (useTimestamps) {
        // TIMESTAMP MODE
        const now = Math.floor(Date.now() / 1000);
        xAxisData = [];
        for (let i = 0; i < maxPoints; i++) {
            xAxisData[i] = now - (maxPoints - 1 - i) * intervalSec;
        }
    } else {
        // RELATIVE TIME MODE
        xAxisData = [];
        for (let i = 0; i < maxPoints; i++) {
            xAxisData[i] = -(maxPoints - 1 - i) * intervalSec;
        }
    }

    // Initialize all buffers with correct size
    currentTempData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)
    ];

    voltageData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)
    ];

    rpmData = [
        [...xAxisData],
        new Array(maxPoints).fill(0)
    ];

    temperatureData = [
        [...xAxisData],
        new Array(maxPoints).fill(0)
    ];
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
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
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
        scales: useTimestamps ? {
            x: { time: true },
            current: { auto: false, range: [Ymin1, Ymax1] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            current: { auto: false, range: [Ymin1, Ymax1] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "current", // Use appropriate scale name for each plot
                label: "Amperes", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "current", // Use appropriate scale name for each plot
                label: "Amperes", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
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

                        if (currentTempResizeObserver) {
                            currentTempResizeObserver.disconnect();
                        }
                        currentTempResizeObserver = new ResizeObserver(resizePlot);
                        currentTempResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    currentTempPlot = new uPlot(opts, currentTempData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(currentTempPlot);
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
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
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
        scales: useTimestamps ? {
            x: { time: true },
            voltage: { auto: false, range: [Ymin2, Ymax2] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            voltage: { auto: false, range: [Ymin2, Ymax2] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "voltage", // Use appropriate scale name for each plot
                label: "Volts", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "voltage", // Use appropriate scale name for each plot
                label: "Volts", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
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

                        if (voltageResizeObserver) {
                            voltageResizeObserver.disconnect();
                        }
                        voltageResizeObserver = new ResizeObserver(resizePlot);
                        voltageResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    voltagePlot = new uPlot(opts, voltageData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(voltagePlot);
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
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
            {
                label: "RPM",
                stroke: "#E91E63",
                width: 2,
                scale: "rpm"
            }
        ],
        scales: useTimestamps ? {
            x: { time: true },
            rpm: { auto: false, range: [Ymin3, Ymax3] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            rpm: { auto: false, range: [Ymin3, Ymax3] }
        },

        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "rpm", // Use appropriate scale name for each plot
                label: "revs/min", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "rpm", // Use appropriate scale name for each plot
                label: "revs/min", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createCustomLegend('rpm-plot', [
                            { label: "RPM", color: "#E91E63" }
                        ]);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("rpm-plot");
                            if (plotEl && rpmPlot) {
                                rpmPlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);

                        if (rpmResizeObserver) {
                            rpmResizeObserver.disconnect();
                        }
                        rpmResizeObserver = new ResizeObserver(resizePlot);
                        rpmResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    rpmPlot = new uPlot(opts, rpmData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(rpmPlot);
}

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
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
            {
                label: "Alt. Temp (°F)",
                stroke: "#FF5722",
                width: 2,
                scale: "temperature"
            }
        ],
        scales: useTimestamps ? {
            x: { time: true },
            temperature: { auto: false, range: [Ymin4, Ymax4] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            temperature: { auto: false, range: [Ymin4, Ymax4] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "temperature", // Use appropriate scale name for each plot
                label: "F", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "temperature", // Use appropriate scale name for each plot
                label: "F", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ],
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

                        if (temperatureResizeObserver) {
                            temperatureResizeObserver.disconnect();
                        }
                        temperatureResizeObserver = new ResizeObserver(resizePlot);
                        temperatureResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    temperaturePlot = new uPlot(opts, temperatureData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(temperaturePlot);
}

//Staleness stuff
// Helper function to apply staleness styling using age directly
function applyStaleStyleByAge(elementId, ageMs) {
    const element = document.getElementById(elementId);
    if (!element) {
        console.warn(`Element ${elementId} not found for stale styling`);
        return;
    }
    const staleThreshold = 11000; // 11 seconds, don't go lower without making sure digital temp sensor tempTask is going to keep up
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
// Function to update all staleness styling
function updateAllStalenessStyles() {
    if (!window.sensorAges) return;

    // Apply staleness using ages directly 
    applyStaleStyleByAge("HeadingNMEAID", window.sensorAges.heading);                      // GPS heading
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
    applyStaleStyleByAge("dutyCycleID", window.sensorAges.dutyCycle);               // Field duty cycle
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

// Start the staleness detection system - call this from window.load
function startStalenessDetection() {
    // Update staleness styling every 2 seconds
    setInterval(updateAllStalenessStyles, 2000);
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
                const plotControls = document.getElementById('plots-controls');
                if (plotControls) plotControls.classList.remove("locked");
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


function triggerWeatherUpdate() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("TriggerWeatherUpdate", "1");

    fetch("/get?" + new URLSearchParams(formData).toString())
        .then(() => console.log("Weather update triggered"))
        .catch(err => console.error("Weather update failed:", err));
}


function updateTogglesFromData(data) {
    try {
        if (!data) return;

        const now = Date.now();
        // Update time axis mode when ESP32 value changes
        if (data.timeAxisModeChanging !== undefined) {
            const newTimeAxisMode = (data.timeAxisModeChanging === 1);
            if (useTimestamps !== newTimeAxisMode) {
                useTimestamps = newTimeAxisMode;
                console.log(`Time axis mode updated from ESP32: ${useTimestamps ? 'UNIX timestamps' : 'relative time'}`);

                // Reinitialize plots with new mode
                initPlotDataStructures();
                if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
                if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
                if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
                if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }
                reinitializeXAxisForNewMode();
            }
        }
        const updateCheckbox = (id, value, dataKey) => {
            if (value === undefined) return;

            const lastUserAction = userInitiatedToggles.get(dataKey);

            // Skip if user action was recent
            if (lastUserAction && (now - lastUserAction) < IGNORE_DURATION) {
                return; // Skip server update
            }

            // Clean up old entries automatically
            if (lastUserAction && (now - lastUserAction) >= IGNORE_DURATION) {
                userInitiatedToggles.delete(dataKey);
            }

            const checkbox = document.getElementById(id);
            if (!checkbox) return;

            const shouldBeChecked = (value === 1);
            if (checkbox.checked !== shouldBeChecked) {
                checkbox.checked = shouldBeChecked;
                toggleStates[dataKey] = value;
            }
        };

        // Update all toggle checkboxes with their data keys (keep existing calls)
        updateCheckbox("ManualFieldToggle_checkbox", data.ManualFieldToggle, "ManualFieldToggle");
        updateCheckbox("SwitchControlOverride_checkbox", data.SwitchControlOverride, "SwitchControlOverride");
        updateCheckbox("header-alternator-enable", data.OnOff, "OnOff");
        updateCheckbox("LimpHome_checkbox", data.LimpHome, "LimpHome");
        updateCheckbox("HiLow_checkbox", data.HiLow, "HiLow");
        updateCheckbox("VeData_checkbox", data.VeData, "VeData");
        updateCheckbox("NMEA0183Data_checkbox", data.NMEA0183Data, "NMEA0183Data");
        updateCheckbox("NMEA2KData_checkbox", data.NMEA2KData, "NMEA2KData");
        updateCheckbox("IgnoreTemperature_checkbox", data.IgnoreTemperature, "IgnoreTemperature");
        updateCheckbox("bmsLogic_checkbox", data.bmsLogic, "bmsLogic");
        updateCheckbox("bmsLogicLevelOff_checkbox", data.bmsLogicLevelOff, "bmsLogicLevelOff");
        updateCheckbox("AlarmActivate_checkbox", data.AlarmActivate, "AlarmActivate");
        updateCheckbox("InvertAltAmps_checkbox", data.InvertAltAmps, "InvertAltAmps");
        updateCheckbox("InvertBattAmps_checkbox", data.InvertBattAmps, "InvertBattAmps");
        updateCheckbox("IgnitionOverride_checkbox", data.IgnitionOverride, "IgnitionOverride");
        updateCheckbox("TempSource_checkbox", data.TempSource, "TempSource");
        updateCheckbox("AlarmLatchEnabled_checkbox", data.AlarmLatchEnabled, "AlarmLatchEnabled");
        updateCheckbox("ForceFloat_checkbox", data.ForceFloat, "ForceFloat");
        updateCheckbox("AutoShuntGainCorrection_checkbox", data.AutoShuntGainCorrection, "AutoShuntGainCorrection");
        updateCheckbox("AutoAltCurrentZero_checkbox", data.AutoAltCurrentZero, "AutoAltCurrentZero");
        updateCheckbox("timeAxisModeChanging_checkbox", data.timeAxisModeChanging, "timeAxisModeChanging");
        updateCheckbox("weatherModeEnabled_checkbox", data.weatherModeEnabled, "weatherModeEnabled");
        updateCheckbox("LearningMode_checkbox", data.LearningMode, "LearningMode");
        if (data.LearningMode !== undefined) {
            updateLearningTableEditability(data.LearningMode === 1);
        }
        updateCheckbox("LearningPaused_checkbox", data.LearningPaused, "LearningPaused");
        updateCheckbox("LearningDryRunMode_checkbox", data.LearningDryRunMode, "LearningDryRunMode");
        updateCheckbox("LearningUpwardEnabled_checkbox", data.LearningUpwardEnabled, "LearningUpwardEnabled");
        updateCheckbox("LearningDownwardEnabled_checkbox", data.LearningDownwardEnabled, "LearningDownwardEnabled");
        updateCheckbox("EnableNeighborLearning_checkbox", data.EnableNeighborLearning, "EnableNeighborLearning");
        updateCheckbox("AutoSaveLearningTable_checkbox", data.AutoSaveLearningTable, "AutoSaveLearningTable");
        updateCheckbox("ShowLearningDebugMessages_checkbox", data.ShowLearningDebugMessages, "ShowLearningDebugMessages");
        updateCheckbox("EXTRA_checkbox", data.EXTRA, "EXTRA");
        // updateCheckbox("IgnoreLearningDuringPenalty_checkbox", data.IgnoreLearningDuringPenalty, "IgnoreLearningDuringPenalty");




        // // Apply the ESP32 state to the plot system
        // if (data.timeAxisModeChanging !== undefined) {
        //     useTimestamps = (data.timeAxisModeChanging === 1);
        //     // Optionally reinitialize plots here if you want real-time updates
        // }
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

        // Use timestamp instead of Set for better control
        userInitiatedToggles.set(dataKey, Date.now());

        return true;
    }
    return false;
}

// Initialize learning table flag BEFORE any event handlers
if (typeof window.learningTableInitialized === 'undefined') {
    window.learningTableInitialized = false;
}

// this is known as the window.load event in AI speak
window.addEventListener("load", function () {
    // DEBUG CODE
    const originalGetElementById = document.getElementById;
    let warnedElements = new Set();

    document.getElementById = function (id) {
        const element = originalGetElementById.call(document, id);
        if (!element && !warnedElements.has(id)) {
            console.error(`MISSING ELEMENT: ${id}`);
            console.trace();
            warnedElements.add(id);
        }
        return element;
    };
    // END DEBUG CODE
    startStalenessDetection(); // stalness detectioin detect stale values

    initPlotDataStructures(); // Initialize efficient data structures
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
    const plotControls = document.getElementById('plots-controls');
    if (plotControls) plotControls.classList.add("locked");
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

    // Simple WiFi disconnect tracking - ping every 10 seconds
    setInterval(() => {
        fetch('/ping?t=' + Date.now()).catch(() => { });
    }, 10000);

    initCurrentTempPlot();
    initVoltagePlot();
    initRPMPlot();
    initTemperaturePlot();

    // Initialize learning table editability based on initial state
    // Note: This reads from hidden input which may not be populated yet
    const initialLearningMode = document.getElementById("LearningMode")?.value === "1";
    updateLearningTableEditability(initialLearningMode); // Safe to call with undefined

    // Set up one-time listener for when actual state arrives from ESP32
    let editabilityInitialized = false;
    const editabilityCheck = setInterval(() => {
        if (!editabilityInitialized) {
            const checkbox = document.getElementById("LearningMode_checkbox");
            // Check if checkbox exists and has been set by CSVData2
            if (checkbox && checkbox.checked !== null) {  // Changed this line
                updateLearningTableEditability(checkbox.checked);
                editabilityInitialized = true;
                clearInterval(editabilityCheck);
                console.log('Learning table editability initialized from ESP32');
            }
        }
    }, 100); // Check every 100ms until CSVData2 arrives

    // Clean up if taking too long
    setTimeout(() => clearInterval(editabilityCheck), 5000);



    // Track if learning table has unsaved changes
    let learningTableHasChanges = false;

    // Add change listeners to all learning table inputs
    document.querySelectorAll('#learning-table-form input[type="number"]').forEach(input => {
        input.addEventListener('input', function () {
            learningTableHasChanges = true;
            showSaveTableButton();
        });
    });

    // Show save button when changes detected
    function showSaveTableButton() {
        const saveBtn = document.querySelector('.btn-save-table');
        if (saveBtn) {
            saveBtn.style.display = 'inline-block';
        }
    }

    // Hide save button after form submission
    document.getElementById('learning-table-form').addEventListener('submit', function () {
        learningTableHasChanges = false;
        const saveBtn = document.querySelector('.btn-save-table');
        if (saveBtn) {
            saveBtn.style.display = 'none';
        }
    });

    // Warn before leaving page with unsaved changes
    window.addEventListener('beforeunload', function (e) {
        if (learningTableHasChanges) {
            e.preventDefault();
            e.returnValue = 'You have unsaved changes to the learning table. Are you sure you want to leave?';
            return e.returnValue;
        }
    });




    if (!!window.EventSource) {

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
        let failedReconnectCount = 0;


        source.addEventListener('error', function (e) {
            console.log('EventSource error, readyState:', this.readyState);

            if (this.readyState === EventSource.CONNECTING) {
                // Browser is already trying to reconnect - let it work
                console.log('Browser attempting automatic reconnection...');
                failedReconnectCount++;

                // Only show recovery dialog after many failures
                if (failedReconnectCount > 6) {  //could increase but trying to save battery
                    this.close();
                    showRecoveryOptions();
                }
            } else if (this.readyState === EventSource.CLOSED) {
                // Connection permanently closed - show recovery
                showRecoveryOptions();
            }
        }, false);

        // Reset counter on successful connection
        source.addEventListener('open', function () {
            failedReconnectCount = 0;
            console.log('EventSource reconnected successfully');
        });

        // Initial status - set to connected if we made it this far
        updateInlineStatus(true);

        setInterval(function () {
            const timeSinceLastEvent = Date.now() - lastEventTime;
            if (timeSinceLastEvent > 8000) { // 8 seconds without data = disconnected
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

            // CSVData now has 37 values instead of 170
            if (values.length !== 37) {
                // Throttle CSV mismatch warnings to every 10 seconds
                if (!window.lastCsvWarningTime) window.lastCsvWarningTime = 0;
                const now = Date.now();

                if ((now - window.lastCsvWarningTime) > 10000) { // 10 seconds
                    console.warn(`CSV mismatch: Got ${values.length} values but expected 37 fields for CSVData`);
                    console.warn("First few values:", values.slice(0, 10));
                    console.warn("Last few values:", values.slice(-10));
                    window.lastCsvWarningTime = now;
                }
                return;
            }
            //CSVData
            const data = {
                AlternatorTemperatureF: values[0],  // 0
                dutyCycle: values[1],               // 1
                BatteryV: values[2],                // 2
                MeasuredAmps: values[3],            // 3
                RPM: values[4],                     // 4
                Channel3V: values[5],               // 5
                IBV: values[6],                     // 6
                Bcur: values[7],                    // 7
                VictronVoltage: values[8],          // 8
                LoopTime: values[9],                // 9
                WifiStrength: values[10],           // 10
                WifiHeartBeat: values[11],          // 11
                SendWifiTime: values[12],           // 12
                AnalogReadTime: values[13],         // 13
                VeTime: values[14],                 // 14
                MaximumLoopTime: values[15],        // 15
                HeadingNMEA: values[16],            // 16
                vvout: values[17],                  // 17
                iiout: values[18],                  // 18
                FreeHeap: values[19],               // 19
                EngineCycles: values[20],           // 20
                Alarm_Status: values[21],          // 21
                fieldActiveStatus: values[22],      // 22
                CurrentSessionDuration: values[23], // 23
                timeAxisModeChanging: values[24],   // 24 - PLOT CONTROL
                webgaugesinterval: values[25],      // 25 - PLOT CONTROL
                plotTimeWindow: values[26],         // 26 - PLOT CONTROL
                Ymin1: values[27],                  // 27 - PLOT CONTROL
                Ymax1: values[28],                  // 28 - PLOT CONTROL
                Ymin2: values[29],                  // 29 - PLOT CONTROL
                Ymax2: values[30],                  // 30 - PLOT CONTROL
                Ymin3: values[31],                  // 31 - PLOT CONTROL
                Ymax3: values[32],                  // 32 - PLOT CONTROL
                Ymin4: values[33],                  // 33 - PLOT CONTROL
                Ymax4: values[34],                  // 34 - PLOT CONTROL
                currentMode: values[35],             // 35 - DEVICE MODE 
                currentPartitionType: values[36]     // 36 - PARTITION TYPE

            };

            // Track the interval for toggle functionality
            if (data.webgaugesinterval) {
                window._lastKnownInterval = data.webgaugesinterval;
            }
            if (data.plotTimeWindow) {
                window._lastKnownTimeWindow = data.plotTimeWindow;
            }
            //i'm in the CSV handler here
            // Configuration check logic 
            configCheckCounter++;
            const checkInterval = getConfigCheckInterval(data.webgaugesinterval);

            //load the current software update versions avaialble 
            if (data.currentMode !== undefined) {
                window._lastKnownMode = data.currentMode;
            }

            if (configCheckCounter >= checkInterval) {
                configCheckCounter = 0; // Reset counter
                updatePlotConfiguration(data);
            }

            // Cache mode for other functions to use
            if (data.currentMode !== undefined) {
                window._lastKnownMode = data.currentMode;
            }
            window._debugData = data;
            const fieldIndicator = document.getElementById('field-status');
            if (fieldIndicator) {
                // Static variable to track last log time (persists between function calls)
                if (!fieldIndicator.lastLogTime) {
                    fieldIndicator.lastLogTime = 0;
                }

                const now = Date.now();
                const shouldLog = (now - fieldIndicator.lastLogTime) > 10000; // 10 seconds

                if (data.fieldActiveStatus === 1) {
                    fieldIndicator.textContent = 'ACTIVE';
                    fieldIndicator.className = 'reading-value field-status-active';

                    if (shouldLog) {
                        //   console.log('Field Status: ACTIVE - Field control enabled and operating');
                        fieldIndicator.lastLogTime = now;
                    }
                } else {
                    fieldIndicator.textContent = 'OFF';
                    fieldIndicator.className = 'reading-value field-status-inactive';

                    if (shouldLog) {
                        //  console.log('Field Status: OFF - Field control disabled or below minimum duty');
                        fieldIndicator.lastLogTime = now;
                    }
                }
            }

            // Enhanced updateFields 
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const value = data[key];
                    if (value === undefined) continue;
                    // Calculate what the new text content would be
                    let newTextContent;
                    if (isNaN(value)) {
                        newTextContent = "—";
                    }
                    // Values scaled by 100 on server (existing logic)
                    else if (["BatteryV", "MeasuredAmps", "Bcur", "Channel3V", "IBV", "VictronVoltage", "vvout"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Values scaled by 10 on server (existing logic)  
                    else if (["iiout"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    // Special handling for currentMode
                    else if (key === "currentMode") {
                        const modeNames = ["CONFIG", "AP", "CLIENT"];
                        newTextContent = modeNames[value] || `Unknown (${value})`;
                    }
                    // Special handling for currentPartitionType
                    else if (key === "currentPartitionType") {
                        const partitionNames = ["Factory Golden", "User Updateable"];
                        newTextContent = partitionNames[value] || `Unknown (${value})`;
                    }
                    // Default: display as integer (existing logic)
                    else {
                        newTextContent = Math.round(value);
                    }
                    // ONLY UPDATE DOM IF VALUE ACTUALLY CHANGED
                    const cacheKey = `${elementId}_${key}`;
                    if (lastValues.get(cacheKey) !== newTextContent) {
                        lastValues.set(cacheKey, newTextContent);
                        scheduleDOMUpdateOptimized(elementId, newTextContent);
                    }
                }
            };


            // Counter for echo throttling (separate from display throttling)
            if (typeof window.echoUpdateCounter === 'undefined') window.echoUpdateCounter = 0;

            // Add counter for throttling
            if (typeof window.updateCounter === 'undefined') window.updateCounter = 0;
            window.updateCounter++;

            // Critical fields - update every cycle (real-time) - ONLY original critical fields that are in payload1
            const criticalFields = [
                ["MeasAmpsID", "MeasuredAmps"],          // Alternator Current 
                ["BatteryVID", "BatteryV"],              // ADS Battery Voltage 
                ["IBVID", "IBV"],                        // INA Battery Voltage 
                ["BCurrID", "Bcur"],                     // Battery Current 
                ["RPMID", "RPM"],                        // Engine Speed 
                ["RPMID2", "RPM"],     // have to use 2 because it appears in two HTML displays (Dumb rule)
                ["LoopTimeID", "LoopTime"],              // Loop Time 
                ["WifiHeartBeatID", "WifiHeartBeat"],     // WifiHeartbeat 
                ["ADS3ID", "Channel3V"],
                ["FieldVoltsID", "vvout"],
                ["FieldVoltsID2", "vvout"], // have to use 2 because it appears in two HTML displays (Dumb rule)
                ["FieldAmpsID", "iiout"],
                ["FieldAmpsID2", "iiout"],// have to use 2 because it appears in two HTML displays (Dumb rule)
                ["FreeHeapID", "FreeHeap"]
            ];


            // Update alarm status, this is GPIO21 buzzer/alarm
            updateAlarmStatus(data);

            // Other fields - update every 4th cycle - ALL remaining payload1 variables (were non-critical OR new)
            const otherFields = [
                ["AltTempID", "AlternatorTemperatureF"],
                ["dutyCycleID", "dutyCycle"],
                ["dutyCycleID2", "dutyCycle"], // have to use 2 because it appears in two HTML displays (Dumb rule)
                ["VictronVoltageID", "VictronVoltage"],
                ["SendWifiTimeID", "SendWifiTime"],
                ["AnalogReadTimeID", "AnalogReadTime"],
                ["VeTimeID", "VeTime"],
                ["MaximumLoopTimeID", "MaximumLoopTime"],
                ["HeadingNMEAID", "HeadingNMEA"],
                ["EngineCyclesID", "EngineCycles"],
                ["header-voltage", "IBV"],
                ["header-alt-current", "MeasuredAmps"],
                ["header-batt-current", "Bcur"],
                ["header-alt-temp", "AlternatorTemperatureF"],
                ["header-rpm", "RPM"],
                ["WifiStrengthID", "WifiStrength"],
                ["CurrentSessionDurationID", "CurrentSessionDuration"],
                ["currentModeID", "currentMode"],
                ["currentPartitionTypeID", "currentPartitionType"]

            ];

            // Update critical fields every cycle
            updateFields(criticalFields);
            processCSVDataOptimized(data); // this is for plotting

            // Update other stuff every 4th cycle
            if (window.updateCounter % 4 === 0) {
                updateFields(otherFields);
                updateAllEchosOptimized(data);
                updateTogglesFromData(data);
            }

            // One-time check to hide tabs in AP mode
            if (typeof window.tabsProcessed === 'undefined') {
                window.tabsProcessed = false;
            }

            if (!window.tabsProcessed && data.currentMode !== undefined) {
                window.tabsProcessed = true;

                // MODE_AP = 1, hide internet-dependent features
                if (data.currentMode === 1) {
                    const softwareTab = document.querySelector('.sub-tab[onclick*="softwareupdate"]');
                    if (softwareTab) {
                        softwareTab.style.display = 'none';
                    }

                    const weatherTab = document.querySelector('.sub-tab[onclick*="weather"]');
                    if (weatherTab) {
                        weatherTab.style.display = 'none';
                    }

                    console.log('AP mode detected - internet-dependent tabs hidden');
                }
            }

            // Update learning table editability immediately when mode changes
            // (Don't wait for 4th cycle - user needs instant visual feedback)
            if (data.LearningMode !== undefined) {
                const checkbox = document.getElementById("LearningMode_checkbox");
                if (checkbox) {
                    const currentUIState = checkbox.checked;
                    const incomingState = (data.LearningMode === 1);

                    // Only update editability if mode actually changed
                    if (currentUIState !== incomingState) {
                        updateLearningTableEditability(incomingState);
                    }
                }
            }

            // Keep critical state updates on every cycle
            if (currentAdminPassword === "") {
                document.getElementById('settings-section').classList.add("locked");
            }

            // Connection status check
            const nowMs = Date.now();
            const timeSinceLastEvent = nowMs - lastEventTime;
            if (timeSinceLastEvent > 5000) {
                updateInlineStatus(false);
            } else {
                updateInlineStatus(true);
            }
        }, false);

        source.addEventListener('CSVData2', function (e) {
            // Parse CSV data into an array
            const values = e.data.split(',').map(Number);

            // CSVData2 has 90 values
            if (values.length !== 90) {
                // Throttle CSV mismatch warnings to every 10 seconds
                if (!window.lastCsv2WarningTime) window.lastCsv2WarningTime = 0;
                const now = Date.now();

                if ((now - window.lastCsv2WarningTime) > 10000) { // 10 seconds
                    console.warn(`CSV2 mismatch: Got ${values.length} values but expected 90 fields for CSVData2`);
                    console.warn("First few values:", values.slice(0, 10));
                    console.warn("Last few values:", values.slice(-10));
                    window.lastCsv2WarningTime = now;
                }
                return;
            }
            //CSVData2  
            const data = {
                IBVMax: values[0],                      // 0
                MeasuredAmpsMax: values[1],             // 1
                RPMMax: values[2],                      // 2
                SOC_percent: values[3],                 // 3
                EngineRunTime: values[4],               // 4
                AlternatorOnTime: values[5],            // 5
                AlternatorFuelUsed: values[6],          // 6
                ChargedEnergy: values[7],               // 7
                DischargedEnergy: values[8],            // 8
                AlternatorChargedEnergy: values[9],     // 9
                MaxAlternatorTemperatureF: values[10],  // 10
                temperatureThermistor: values[11],      // 11
                MaxTemperatureThermistor: values[12],   // 12
                VictronCurrent: values[13],             // 13
                timeToFullChargeMin: values[14],        // 14
                timeToFullDischargeMin: values[15],     // 15
                LatitudeNMEA: values[16],               // 16
                LongitudeNMEA: values[17],              // 17
                SatelliteCountNMEA: values[18],         // 18
                bulkCompleteTime: values[19],           // 19
                LastSessionDuration: values[20],        // 20
                LastSessionMaxLoopTime: values[21],     // 21
                lastSessionMinHeap: values[22],         // 22
                wifiReconnectsTotal: values[23],        // 23 -  
                LastResetReason: values[24],            // 24 -  
                ancientResetReason: values[25],         // 25 - 
                totalPowerCycles: values[26],           // 26 -  
                MinFreeHeap: values[27],                // 27 - 
                currentWeatherMode: values[28],         // 28 - 
                UVToday: values[29],                    // 29 - 
                UVTomorrow: values[30],                 // 30 -  
                UVDay2: values[31],                     // 31 -  
                weatherDataValid: values[32],           // 32 -  
                SolarWatts: values[33],                 // 33 -  
                performanceRatio: values[34],           // 34 -  
                OnOff: values[35],                      // 35 -  
                ManualFieldToggle: values[36],          // 36 -  
                HiLow: values[37],                      // 37 -  
                LimpHome: values[38],                   // 38 -  
                VeData: values[39],                     // 39 -  
                NMEA0183Data: values[40],               // 40 - 
                NMEA2KData: values[41],                 // 41 -  
                AlarmActivate: values[42],              // 42 -  
                TempAlarm: values[43],                  // 43 -  
                VoltageAlarmHigh: values[44],           // 44 -  
                VoltageAlarmLow: values[45],            // 45 -  
                CurrentAlarmHigh: values[46],           // 46 -  
                AlarmTest: values[47],                  // 47 -  
                AlarmLatchEnabled: values[48],          // 48 -  
                AlarmLatchState: values[49],            // 49 -  
                ResetAlarmLatch: values[50],            // 50 -  
                ForceFloat: values[51],                 // 51 -  
                ResetTemp: values[52],                  // 52 -  
                ResetVoltage: values[53],               // 53 -  
                ResetCurrent: values[54],               // 54 -  
                ResetEngineRunTime: values[55],         // 55 -  
                ResetAlternatorOnTime: values[56],      // 56 -  
                ResetEnergy: values[57],                // 57 - 
                ManualSOCPoint: values[58],             // 58 - 
                LearningMode: values[59],               // 59 - 
                LearningPaused: values[60],             // 60 - 
                IgnoreLearningDuringPenalty: values[61], // 61 - 
                ShowLearningDebugMessages: values[62],  // 62 - 
                LogAllLearningEvents: values[63],       // 63 -
                EXTRA: values[64],    // 64 -
                LearningDryRunMode: values[65],         // 65 - 
                AutoSaveLearningTable: values[66],      // 66 - 
                ResetLearningTable: values[67],
                ClearOverheatHistory: values[68],
                AutoShuntGainCorrection: values[69],
                DynamicShuntGainFactor: values[70],
                AutoAltCurrentZero: values[71],
                DynamicAltCurrentZero: values[72],
                InsulationLifePercent: values[73],
                GreaseLifePercent: values[74],
                BrushLifePercent: values[75],
                PredictedLifeHours: values[76],
                LifeIndicatorColor: values[77],
                WindingTempOffset: values[78],
                ManualLifePercentage: values[79],
                UVThresholdHigh: values[80],
                weatherModeEnabled: values[81],
                pKwHrToday: values[82],
                pKwHrTomorrow: values[83],
                pKwHr2days: values[84],
                ambientTemp: values[85],
                baroPressure: values[86],
                firmwareVersionInt: values[87],
                deviceIdUpper: values[88],
                deviceIdLower: values[89]
            };

            // Enhanced updateFields for CSVData2 - using your existing patterns
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const value = data[key];
                    if (value === undefined) continue;

                    // Calculate what the new text content would be
                    let newTextContent;
                    if (isNaN(value)) {
                        newTextContent = "—";
                    }
                    // Special handling for reset reason lookups
                    else if (key === "LastResetReason") {
                        newTextContent = resetReasonLookup[value] || `Unknown (${value})`;
                    }
                    else if (key === "ancientResetReason") {
                        newTextContent = resetReasonLookup[value] || `Unknown (${value})`;
                    }
                    // Values scaled by 100 on server
                    else if (["IBVMax", "MeasuredAmpsMax", "SOC_percent", "VictronCurrent", "performanceRatio", "UVThresholdHigh"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // GPS coordinates scaled by 1,000,000 on server
                    else if (["LatitudeNMEA", "LongitudeNMEA"].includes(key)) {
                        newTextContent = (value / 1000000).toFixed(6);
                    }
                    // Energy values
                    else if (["ChargedEnergy", "DischargedEnergy", "AlternatorChargedEnergy"].includes(key)) {
                        newTextContent = value;
                    }
                    // Fuel in mL
                    else if (["AlternatorFuelUsed"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    else if (["DynamicShuntGainFactor", "DynamicAltCurrentZero"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    else if (["InsulationLifePercent", "GreaseLifePercent", "BrushLifePercent"].includes(key)) {
                        newTextContent = (value / 100).toFixed(1);
                    }
                    else if (["WindingTempOffset"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    else if (["pKwHrToday", "pKwHrTomorrow", "pKwHr2days"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Time values
                    else if (["EngineRunTime", "AlternatorOnTime"].includes(key)) {
                        const totalSeconds = value;
                        const hours = Math.floor(totalSeconds / 3600);
                        const minutes = Math.floor((totalSeconds % 3600) / 60);
                        const seconds = totalSeconds % 60;
                        newTextContent = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
                    }
                    // Session duration in minutes
                    else if (["LastSessionDuration"].includes(key)) {
                        if (value < 60) {
                            newTextContent = `${value}`;
                        } else {
                            const hours = Math.floor(value / 60);
                            const minutes = value % 60;
                            newTextContent = `${hours}h ${minutes}m`;
                        }
                    }
                    // Default: display as integer
                    else {
                        newTextContent = Math.round(value);
                    }

                    // ONLY UPDATE DOM IF VALUE ACTUALLY CHANGED - using your existing pattern
                    const cacheKey = `${elementId}_${key}`;
                    if (lastValues.get(cacheKey) !== newTextContent) {
                        lastValues.set(cacheKey, newTextContent);
                        scheduleDOMUpdateOptimized(elementId, newTextContent);
                    }
                }
            };


            // Other fields for CSVData2 - update every cycle (no throttling since server controls timing)
            const otherFields = [
                ["IBVMaxID", "IBVMax"],
                ["MeasuredAmpsMaxID", "MeasuredAmpsMax"],
                ["RPMMaxID", "RPMMax"],
                ["EngineRunTimeID", "EngineRunTime"],
                ["AlternatorOnTimeID", "AlternatorOnTime"],
                ["AlternatorFuelUsedID", "AlternatorFuelUsed"],
                ["ChargedEnergyID", "ChargedEnergy"],
                ["DischargedEnergyID", "DischargedEnergy"],
                ["AlternatorChargedEnergyID", "AlternatorChargedEnergy"],
                ["MaxAlternatorTemperatureF_ID", "MaxAlternatorTemperatureF"],
                ["temperatureThermistorID", "temperatureThermistor"],
                ["MaxTemperatureThermistorID", "MaxTemperatureThermistor"],
                ["VictronCurrentID", "VictronCurrent"],
                ["LatitudeNMEA_ID", "LatitudeNMEA"],
                ["LongitudeNMEA_ID", "LongitudeNMEA"],
                ["SatelliteCountNMEA_ID", "SatelliteCountNMEA"],
                ["timeToFullChargeMinID", "timeToFullChargeMin"],
                ["timeToFullDischargeMinID", "timeToFullDischargeMin"],
                ["LastSessionDurationID", "LastSessionDuration"],
                ["LastSessionMaxLoopTimeID", "LastSessionMaxLoopTime"],
                ["lastSessionMinHeapID", "lastSessionMinHeap"],
                ["wifiReconnectsTotalID", "wifiReconnectsTotal"],
                ["LastResetReasonID", "LastResetReason"],
                ["ancientResetReasonID", "ancientResetReason"],
                ["totalPowerCyclesID", "totalPowerCycles"],
                ["MinFreeHeapID", "MinFreeHeap"],
                ["UVTodayID", "UVToday"],
                ["UVTomorrowID", "UVTomorrow"],
                ["UVDay2ID", "UVDay2"],
                ["weatherDataValidID", "weatherDataValid"],
                ["DynamicShuntGainFactor_display", "DynamicShuntGainFactor"],
                ["DynamicAltCurrentZero_display", "DynamicAltCurrentZero"],
                ["InsulationLifePercentID", "InsulationLifePercent"],
                ["GreaseLifePercentID", "GreaseLifePercent"],
                ["BrushLifePercentID", "BrushLifePercent"],
                ["PredictedLifeHoursID", "PredictedLifeHours"],
                ["pKwHrToday_ID", "pKwHrToday"],
                ["pKwHrTomorrow_ID", "pKwHrTomorrow"],
                ["pKwHr2days_ID", "pKwHr2days"],
                ["SOC_percentID", "SOC_percent"],
                ["header-soc", "SOC_percent"],
                ["ambientTempID", "ambientTemp"],
                ["baroPressureID", "baroPressure"],
                ["firmwareVersionIntID", "firmwareVersionInt"],
                ["deviceIdUpperID", "deviceIdUpper"],
                ["deviceIdLowerID", "deviceIdLower"]


            ];
            //CSVData2

            // Update PID stuff
            updateLearningStatus(data);

            // Update other fields every cycle (no throttling)
            updateFields(otherFields);

            // Update echos every cycle (since CSVData3 already runs slowly at 2 seconds)
            updateAllEchosOptimized(data);

            // Update toggle states 
            updateTogglesFromData(data);

            // Update life indicators, this is for Alternator health 
            updateLifeIndicators(data);

            updateVersionInfo();


        }, false);

        source.addEventListener('CSVData3', function (e) {
            const processingStart = performance.now();
            // Parse CSV data into an array
            const values = e.data.split(',').map(Number);

            // CSVData3
            if (values.length !== 125) {
                // Throttle CSV mismatch warnings to every 10 seconds
                if (!window.lastCsv3WarningTime) window.lastCsv3WarningTime = 0;
                const now = Date.now();

                if ((now - window.lastCsv3WarningTime) > 10000) { // 10 seconds
                    console.warn(`CSV3 mismatch: Got ${values.length} values but expected 125 fields for CSVData3`);
                    console.warn("First few values:", values.slice(0, 10));
                    console.warn("Last few values:", values.slice(-10));
                    window.lastCsv3WarningTime = now;
                }
                return;
            }
            //CSVData3
            const data = {
                TemperatureLimitF: values[0],           // 0
                BulkVoltage: values[1],                 // 1
                TargetAmps: values[2],                  // 2
                FloatVoltage: values[3],                // 3
                SwitchingFrequency: values[4],          // 4
                dutyStep: values[5],                    // 5
                FieldAdjustmentInterval: values[6],     // 6
                ManualDutyTarget: values[7],            // 7
                SwitchControlOverride: values[8],       // 8
                TargetAmpL: values[9],                  // 9
                CurrentThreshold: values[10],           // 10
                PeukertExponent: values[11],            // 11
                ChargeEfficiency: values[12],           // 12
                ChargedVoltage: values[13],             // 13
                TailCurrent: values[14],                // 14
                ChargedDetectionTime: values[15],       // 15
                IgnoreTemperature: values[16],          // 16
                bmsLogic: values[17],                   // 17
                bmsLogicLevelOff: values[18],           // 18
                FourWay: values[19],                    // 19
                RPMScalingFactor: values[20],           // 20
                MaximumAllowedBatteryAmps: values[21],  // 21
                BatteryVoltageSource: values[22],       // 22
                LearningUpwardEnabled: values[23],      // 23
                LearningDownwardEnabled: values[24],    // 24
                AlternatorNominalAmps: values[25],      // 25
                LearningUpStep: values[26],             // 26
                LearningDownStep: values[27],           // 27
                AmbientTempCorrectionFactor: values[28], // 28
                AmbientTempBaseline: values[29],        // 29
                MinLearningInterval: values[30],        // 30
                SafeOperationThreshold: values[31],     // 31
                PidKp: values[32],                      // 32
                PidKi: values[33],                      // 33
                PidKd: values[34],                      // 34
                PidSampleTime: values[35],              // 35
                MaxTableValue: values[36],              // 36
                MinTableValue: values[37],              // 37
                MaxPenaltyPercent: values[38],          // 38
                MaxPenaltyDuration: values[39],         // 39
                NeighborLearningFactor: values[40],     // 40
                LearningRPMSpacing: values[41],         // 41
                LearningMemoryDuration: values[42],     // 42
                EnableNeighborLearning: values[43],     // 43
                EnableAmbientCorrection: values[44],    // 44
                LearningFailsafeMode: values[45],       // 45
                LearningTableSaveInterval: values[46],  // 46
                rpmCurrentTable0: values[47],           // 47
                rpmCurrentTable1: values[48],           // 48
                rpmCurrentTable2: values[49],           // 49
                rpmCurrentTable3: values[50],           // 50
                rpmCurrentTable4: values[51],           // 51
                rpmCurrentTable5: values[52],           // 52
                rpmCurrentTable6: values[53],           // 53
                rpmCurrentTable7: values[54],           // 54
                rpmCurrentTable8: values[55],           // 55
                rpmCurrentTable9: values[56],           // 56
                currentRPMTableIndex: values[57],       // 57
                pidInitialized: values[58],             // 58
                ShuntResistanceMicroOhm: values[59],    // 59
                InvertAltAmps: values[60],              // 60
                InvertBattAmps: values[61],             // 61
                MaxDuty: values[62],                    // 62
                MinDuty: values[63],                    // 63
                FieldResistance: values[64],            // 64
                maxPoints: values[65],                  // 65
                AlternatorCOffset: values[66],          // 66
                BatteryCOffset: values[67],             // 67
                BatteryCapacity_Ah: values[68],         // 68
                AmpSrc: values[69],                     // 69
                R_fixed: values[70],                    // 70
                Beta: values[71],                       // 71
                T0_C: values[72],                       // 72
                TempSource: values[73],                 // 73
                IgnitionOverride: values[74],           // 74
                FLOAT_DURATION: values[75],             // 75
                PulleyRatio: values[76],                // 76
                BatteryCurrentSource: values[77],       // 77
                overheatCount0: values[78],             // 78
                overheatCount1: values[79],             // 79
                overheatCount2: values[80],             // 80
                overheatCount3: values[81],             // 81
                overheatCount4: values[82],             // 82
                overheatCount5: values[83],             // 83
                overheatCount6: values[84],             // 84
                overheatCount7: values[85],             // 85
                overheatCount8: values[86],             // 86
                overheatCount9: values[87],             // 87
                safeHours0: values[88],                 // 88
                safeHours1: values[89],                 // 89
                safeHours2: values[90],                 // 90
                safeHours3: values[91],                 // 91
                safeHours4: values[92],                 // 92
                safeHours5: values[93],                 // 93
                safeHours6: values[94],                 // 94
                safeHours7: values[95],                 // 95
                safeHours8: values[96],                 // 96
                safeHours9: values[97],                 // 97
                totalLearningEvents: values[98],        // 98
                totalOverheats: values[99],             // 99
                totalSafeHours: values[100],            // 100
                averageTableValue: values[101],         // 101
                timeSinceLastOverheat: values[102],     // 102
                learningTargetFromRPM: values[103],     // 103
                ambientTempCorrection: values[104],     // 104
                finalLearningTarget: values[105],       // 105
                overheatingPenaltyTimer: values[106],   // 106
                overheatingPenaltyAmps: values[107],    // 107
                pidInput: values[108],                  // 108
                pidSetpoint: values[109],               // 109
                pidOutput: values[110],                 // 110
                TempToUse: values[111],                 // 111
                rpmTableRPMPoints0: values[112],        // 112
                rpmTableRPMPoints1: values[113],        // 113
                rpmTableRPMPoints2: values[114],        // 114
                rpmTableRPMPoints3: values[115],        // 115
                rpmTableRPMPoints4: values[116],        // 116
                rpmTableRPMPoints5: values[117],        // 117
                rpmTableRPMPoints6: values[118],        // 118
                rpmTableRPMPoints7: values[119],        // 119
                rpmTableRPMPoints8: values[120],        // 120
                rpmTableRPMPoints9: values[121],        // 121
                LearningSettlingPeriod: values[122],    // 122
                LearningRPMChangeThreshold: values[123], // 123
                LearningTempHysteresis: values[124]     // 124
            };

            if (data.pidSetpoint !== undefined && data.pidInput !== undefined) {
                data.pidError = data.pidSetpoint / 100 - data.pidInput / 100; // scaling hack needs fixing later
            }

            // Enhanced updateFields for CSVData3 - using your existing patterns
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const value = data[key];
                    if (value === undefined) continue;

                    // Calculate what the new text content would be
                    let newTextContent;
                    if (isNaN(value)) {
                        newTextContent = "—";
                    }
                    // Values scaled by 100 on server
                    else if (["BulkVoltage", "FloatVoltage", "CurrentThreshold"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Values scaled by 100 for learning system
                    else if (["LearningUpStep", "LearningDownStep", "AmbientTempCorrectionFactor", "MaxTableValue", "MinTableValue", "MaxPenaltyPercent", "dutyStep"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Values scaled by 1000 for PID parameters
                    else if (["PidKp", "PidKi", "PidKd", "MinLearningInterval", "SafeOperationThreshold", "NeighborLearningFactor"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    // Values scaled by 1000 for time intervals
                    else if (["timeSinceLastOverheat", "overheatingPenaltyTimer", "LearningSettlingPeriod", "MaxPenaltyDuration", "LearningTableSaveInterval"].includes(key)) {
                        newTextContent = (value * 1000).toFixed(0);
                    }
                    // Values scaled by 86400000 for days
                    else if (["LearningMemoryDuration"].includes(key)) {
                        newTextContent = (value * 86400000).toFixed(0);
                    }
                    // Values scaled by 3600000 for hours (C divides by 3600000, so JS multiplies by 3600000)
                    else if (key.startsWith("safeHours")) {
                        newTextContent = (value * 3600000).toFixed(0);
                    }
                    // RPM Current Table values scaled by 100
                    else if (key.startsWith("rpmCurrentTable")) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["AlternatorCOffset", "BatteryCOffset"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["R_fixed", "Beta", "T0_C"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["PulleyRatio"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Settings echo values
                    else if (["TargetAmps", "TargetAmpL"].includes(key)) {
                        newTextContent = value;
                    }
                    else if (["averageTableValue"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["learningTargetFromRPM", "ambientTempCorrection", "finalLearningTarget", "overheatingPenaltyAmps"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Values scaled by 100 for PID system
                    else if (["pidInput", "pidSetpoint", "pidOutput"].includes(key)) {
                        newTextContent = (value / 100).toFixed(1);
                    }
                    else {
                        newTextContent = Math.round(value);
                    }
                    // ONLY UPDATE DOM IF VALUE ACTUALLY CHANGED - using your existing pattern
                    const cacheKey = `${elementId}_${key}`;
                    if (lastValues.get(cacheKey) !== newTextContent) {
                        lastValues.set(cacheKey, newTextContent);
                        scheduleDOMUpdateOptimized(elementId, newTextContent);
                    }
                }
            };


            // Other fields for CSVData3 - update every cycle (no throttling since server controls timing)
            const otherFields = [
                // Note: Many of these might not have corresponding HTML elements yet
                // This list includes potential element IDs that might exist

                ["currentRPMTableIndex_display", "currentRPMTableIndex"],
                ["currentRPMTableIndex_display2", "currentRPMTableIndex"], // another duplicate DOM item workaround (multiple html indicators for same variable)
                ["pidInitialized_display", "pidInitialized"],
                ["overheatCount0_display", "overheatCount0"],
                ["overheatCount1_display", "overheatCount1"],
                ["overheatCount2_display", "overheatCount2"],
                ["overheatCount3_display", "overheatCount3"],
                ["overheatCount4_display", "overheatCount4"],
                ["overheatCount5_display", "overheatCount5"],
                ["overheatCount6_display", "overheatCount6"],
                ["overheatCount7_display", "overheatCount7"],
                ["overheatCount8_display", "overheatCount8"],
                ["overheatCount9_display", "overheatCount9"],
                ["safeHours0_display", "safeHours0"],
                ["safeHours1_display", "safeHours1"],
                ["safeHours2_display", "safeHours2"],
                ["safeHours3_display", "safeHours3"],
                ["safeHours4_display", "safeHours4"],
                ["safeHours5_display", "safeHours5"],
                ["safeHours6_display", "safeHours6"],
                ["safeHours7_display", "safeHours7"],
                ["safeHours8_display", "safeHours8"],
                ["safeHours9_display", "safeHours9"],
                ["totalLearningEvents_display", "totalLearningEvents"],
                ["totalOverheats_display", "totalOverheats"],
                ["totalSafeHours_display", "totalSafeHours"],
                ["averageTableValue_display", "averageTableValue"],
                ["timeSinceLastOverheat_display", "timeSinceLastOverheat"],
                ["learningTargetFromRPM_display", "learningTargetFromRPM"],
                ["ambientTempCorrection_display", "ambientTempCorrection"],
                ["finalLearningTarget_display", "finalLearningTarget"],
                ["overheatingPenaltyTimer_display", "overheatingPenaltyTimer"],
                ["overheatingPenaltyAmps_display", "overheatingPenaltyAmps"],
                ["pidInput_display", "pidInput"],
                ["pidSetpoint_display", "pidSetpoint"],
                ["pidOutput_display", "pidOutput"],
                ["pidError_display", "pidError"],
                ["TempToUse_display", "TempToUse"],
                ["LearningSettlingPeriod_display", "LearningSettlingPeriod"],
                ["LearningRPMChangeThreshold_display", "LearningRPMChangeThreshold"],
                ["LearningTempHysteresis_display", "LearningTempHysteresis"]

            ];

            //CSVData3
            updateFields(otherFields);
            // Update learning table inputs
            // CRITICAL: Read mode from incoming data, not DOM (which may be stale)
            const learningModeActive = data.LearningMode === 1;

            for (let i = 0; i < 10; i++) {
                // RPM BREAKPOINTS: Only initialize once, never update during learning
                // (These are fixed speed points, not learned values)
                if (!window.learningTableInitialized) {
                    const rpmInput = document.getElementById(`rpmTableRPMPoints${i}_input`);
                    if (rpmInput && data[`rpmTableRPMPoints${i}`] !== undefined) {
                        rpmInput.value = data[`rpmTableRPMPoints${i}`];
                    }
                }

                // CURRENT LIMITS: Initialize once, then update during active learning
                // (These are the learned values that change)
                if (!window.learningTableInitialized || learningModeActive) {
                    const currentInput = document.getElementById(`rpmCurrentTable${i}_input`);
                    if (currentInput && data[`rpmCurrentTable${i}`] !== undefined) {
                        currentInput.value = Math.round(data[`rpmCurrentTable${i}`] / 100);
                    }
                }
            }

            // Mark as initialized ONLY if we actually received valid table data
            if (!window.learningTableInitialized) {
                // Verify we got real data before locking the flag
                const hasValidData = (data.rpmTableRPMPoints0 !== undefined &&
                    data.rpmCurrentTable0 !== undefined);

                if (hasValidData) {
                    window.learningTableInitialized = true;
                    console.log('Learning table initialized from ESP32 data');
                } else {
                    console.log('Waiting for complete table data from ESP32...');
                }
            }



            updateAllEchosOptimized(data);
            updateTogglesFromData(data);

            // Update PID stuff
            updateLearningTableHighlight(data);
            updatePidInitializedDisplay(data);

            const processingTime = performance.now() - processingStart;
            if (processingTime > 5) { // ← ADD THIS LINE
                console.log(`[CSV3] Processing took ${processingTime.toFixed(1)}ms`);
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

    /*     // Add this function for dark mode toggle
    function toggleDarkMode() {
        document.body.classList.toggle('dark-mode');
        const isDark = document.body.classList.contains('dark-mode');
        localStorage.setItem('darkMode', isDark ? '1' : '0');
        
        // Update all existing plots for dark mode
        if (currentTempPlot) updateUplotTheme(currentTempPlot);
        if (voltagePlot) updateUplotTheme(voltagePlot);
        if (rpmPlot) updateUplotTheme(rpmPlot);
        if (temperaturePlot) updateUplotTheme(temperaturePlot);
    } */

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
    document.getElementById("bmsLogic_checkbox").checked = (document.getElementById("bmsLogic").value === "1");
    document.getElementById("bmsLogicLevelOff_checkbox").checked = (document.getElementById("bmsLogicLevelOff").value === "1");
    document.getElementById("AlarmActivate_checkbox").checked = (document.getElementById("AlarmActivate").value === "1");
    document.getElementById("InvertAltAmps_checkbox").checked = (document.getElementById("InvertAltAmps").value === "1");
    document.getElementById("InvertBattAmps_checkbox").checked = (document.getElementById("InvertBattAmps").value === "1");
    document.getElementById("IgnitionOverride_checkbox").checked = (document.getElementById("IgnitionOverride").value === "1");
    document.getElementById("TempSource_checkbox").checked = (document.getElementById("TempSource").value === "1");
    document.getElementById("admin_password").addEventListener("change", updatePasswordFields);
    document.getElementById("timeAxisModeChanging_checkbox").checked = (document.getElementById("timeAxisModeChanging").value === "1");
    document.getElementById("weatherModeEnabled_checkbox").checked = (document.getElementById("weatherModeEnabled").value === "1");
    document.getElementById("LearningMode_checkbox").checked = (document.getElementById("LearningMode").value === "1");
    document.getElementById("LearningPaused_checkbox").checked = (document.getElementById("LearningPaused").value === "1");
    document.getElementById("LearningDryRunMode_checkbox").checked = (document.getElementById("LearningDryRunMode").value === "1");
    document.getElementById("LearningUpwardEnabled_checkbox").checked = (document.getElementById("LearningUpwardEnabled").value === "1");
    document.getElementById("LearningDownwardEnabled_checkbox").checked = (document.getElementById("LearningDownwardEnabled").value === "1");
    document.getElementById("EnableNeighborLearning_checkbox").checked = (document.getElementById("EnableNeighborLearning").value === "1");
    document.getElementById("AutoSaveLearningTable_checkbox").checked = (document.getElementById("AutoSaveLearningTable").value === "1");
    document.getElementById("ShowLearningDebugMessages_checkbox").checked = (document.getElementById("ShowLearningDebugMessages").value === "1");
    document.getElementById("EXTRA_checkbox").checked = (document.getElementById("EXTRA").value === "1");
    document.getElementById("AutoAltCurrentZero_checkbox").checked = (document.getElementById("AutoAltCurrentZero").value === "1");
    document.getElementById("IgnoreLearningDuringPenalty_checkbox").checked = (document.getElementById("IgnoreLearningDuringPenalty").value === "1");

    setupInputValidation(); // Client side input validation of settings

    // Start frame time monitoring
    if (ENABLE_DETAILED_PROFILING) {
        startFrameTimeMonitoring();
        // Log performance summary every 10 seconds during development
        setInterval(() => {
            const avgFrameTime = frameTimeTracker.frameTimes.reduce((a, b) => a + b, 0) / frameTimeTracker.frameTimes.length;
            //   console.log(`[PERF SUMMARY] Avg frame: ${avgFrameTime.toFixed(2)}ms, Worst: ${frameTimeTracker.worstFrame.toFixed(2)}ms, FPS: ${(1000 / avgFrameTime).toFixed(1)}`);
            frameTimeTracker.worstFrame = 0; // Reset worst frame counter
        }, 10000);
    }

    setInterval(reportPlotRenderingStats, plotRenderTracker.interval);


});// <-- This is the end of the window load event listener


// Function to handle Reset Learning Table button with confirmation
function handleResetLearningTable() {
    const confirmation = confirm(
        "⚠️ RESET LEARNING TABLE ⚠️\n\n" +
        "This will reset all learned current limits and speed ranges to default values.\n" +
        "All learning progress will be lost.\n\n" +
        "Are you sure you want to continue?"
    );

    if (!confirmation) {
        return false;
    }

    // CRITICAL: Force re-initialization on next CSVData3
    window.learningTableInitialized = false;
    console.log('Learning table reset requested - will reinitialize from ESP32 defaults');

    return true; // Allow form submission to ESP32
}

// Function to update learning status indicator
function updateLearningStatus(data) {
    const indicator = document.getElementById('learning-status-indicator');
    if (!indicator) return;

    const isLearningActive = data.LearningMode === 1 && data.LearningPaused !== 1;
    const isDryRun = data.LearningDryRunMode === 1;

    if (isLearningActive && isDryRun) {
        indicator.textContent = '🔶 Dry Run';
        indicator.className = 'learning-status learning-status-dryrun';
    } else if (isLearningActive) {
        indicator.textContent = '🟢 Learning';
        indicator.className = 'learning-status learning-status-active';
    } else if (data.LearningPaused === 1) {
        indicator.textContent = '🟡 Paused';
        indicator.className = 'learning-status learning-status-paused';
    } else {
        indicator.textContent = '⚪ Inactive';
        indicator.className = 'learning-status learning-status-inactive';
    }
}

// Function to update learning table highlighting
function updateLearningTableHighlight(data) {
    // Remove all active row classes
    document.querySelectorAll('.learning-table-row-active').forEach(row => {
        row.className = 'learning-table-row';
    });

    // Highlight current RPM index row if valid
    if (data.currentRPMTableIndex >= 0 && data.currentRPMTableIndex <= 9) {
        const rows = document.querySelectorAll('#learning-table-body tr');
        if (rows[data.currentRPMTableIndex]) {
            rows[data.currentRPMTableIndex].className = 'learning-table-row-active';
        }
    }
}

// Function to update PID initialized display
function updatePidInitializedDisplay(data) {
    const element = document.getElementById('pidInitialized_display');
    if (element && data.pidInitialized !== undefined) {
        element.textContent = data.pidInitialized === 1 ? 'Yes' : 'No';
        element.style.color = data.pidInitialized === 1 ? 'var(--accent)' : '#999';
    }
}




// graying of obsolete data debug
function debugSensorAges() {
    //  console.log('Current sensor ages:', window.sensorAges);
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
    // console.log('Entered offline mode - all readings marked as stale');
}


function validateSettings() {
    const validations = [
        {
            inputs: ['FloatVoltage', 'BulkVoltage'],
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
    const settingsInputs = ["FloatVoltage", "BulkVoltage", "TargetAmpL", "TargetAmps", "MinDuty", "MaxDuty"];

    settingsInputs.forEach(name => {
        const input = document.querySelector(`input[name="${name}"]`);
        if (input) {
            input.addEventListener("input", validateSettings);
            input.addEventListener("blur", validateSettings); // Also validate on blur
        } else {
            console.warn(`Validation input not found: ${name}`);
        }
    });


    // Run initial validation
    setTimeout(() => {
        validateSettings();
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
            //   console.log("SOC gain factor reset requested");
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
            //  console.log("Alternator zero offset reset requested");
        })
        .catch(err => console.error("Reset failed:", err));
}


function updateLearningTableEditability(learningModeEnabled) {
    const rpmInputs = document.querySelectorAll('[id^="rpmTableRPMPoints"][id$="_input"]');
    const ampInputs = document.querySelectorAll('[id^="rpmCurrentTable"][id$="_input"]');
    const submitButtons = document.querySelectorAll('.learning-table-edit-form input[type="submit"]');

    const isEditable = !learningModeEnabled; // Editable when learning is OFF

    // Enable/disable RPM inputs
    rpmInputs.forEach(input => {
        input.disabled = !isEditable;
        input.style.opacity = isEditable ? '1' : '0.5';
        input.style.cursor = isEditable ? 'text' : 'not-allowed';
    });

    // Enable/disable current limit inputs
    ampInputs.forEach(input => {
        input.disabled = !isEditable;
        input.style.opacity = isEditable ? '1' : '0.5';
        input.style.cursor = isEditable ? 'text' : 'not-allowed';
    });

    // Enable/disable submit buttons
    submitButtons.forEach(button => {
        button.disabled = !isEditable;
        button.style.opacity = isEditable ? '1' : '0.5';
        button.title = isEditable ? '' : 'Disable Learning Mode to edit table manually';
    });
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
/* XREG_END */