/* 
 * AI_SUMMARY: Alternator Regulator Project - This file is the majority of the JS served by the ESP32
 * AI_PURPOSE: Realtime control of GPIO (settings always have echos) and viewing of sensor data and calculated values.  
 * AI_INPUTS: Payloads from the ESP32, including some variables used in this javascript such as webgaugesinterval (the ESP32 data delivery interval),timeAxisModeChanging (toggles a different style of X axis on plots generated here), plotTimeWindow (length of time to plot on X axis).  Server-Sent Events via /events endpoint
 * AI_OUTPUTS: Values submitted back to ESP32 via HTTP GET/POST requests to various endpoints
 * AI_DEPENDENCIES: 
 * AI_RISKS: Variable naming is inconsisent, need to be careful not to assume consistent patterns.   Unit conversion can be confusing and propogate to many places, have to trace dependencies in variables FULLY to every end point
 * AI_OPTIMIZE: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat.  When impossible, always mimik my style and coding patterns.
 */

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

            // Keep existing jank detection
            if (totalDuration > 10) {
                console.error(`🚨 JANKINESS FOUND: ${totalDuration.toFixed(2)}ms plot rendering!`);
            }

            plotUpdateQueue.clear();
            plotUpdateScheduled = false;
        });
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
        if (element) {
            element.textContent = newValue;
        }
        return true; // Updated
    }
    return false; // No change
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

        // Current/voltage/RPM plots (every cycle)
        if (typeof currentTempPlot !== 'undefined') {
            const battCurrent = 'Bcur' in data ? parseFloat(data.Bcur) / 100 : 0;
            const altCurrent = 'MeasuredAmps' in data ? parseFloat(data.MeasuredAmps) / 100 : 0;
            const fieldCurrent = 'iiout' in data ? parseFloat(data.iiout) / 10 : 0;

            // Shift all data left and add new data at the end
            for (let i = 1; i < currentTempData[1].length; i++) {
                if (useTimestamps) {
                    currentTempData[0][i - 1] = currentTempData[0][i]; // Shift timestamps
                }
                currentTempData[1][i - 1] = currentTempData[1][i];
                currentTempData[2][i - 1] = currentTempData[2][i];
                currentTempData[3][i - 1] = currentTempData[3][i];
            }
            // Add new data at the end (rightmost position)
            const lastIndex = currentTempData[1].length - 1;
            if (useTimestamps) {
                currentTempData[0][lastIndex] = now; // New timestamp
            }
            currentTempData[1][lastIndex] = battCurrent;
            currentTempData[2][lastIndex] = altCurrent;
            currentTempData[3][lastIndex] = fieldCurrent;

            plotUpdates.push('current');
        }

        if (typeof voltagePlot !== 'undefined') {
            const adsBattV = 'BatteryV' in data ? parseFloat(data.BatteryV) / 100 : 0;
            const inaBattV = 'IBV' in data ? parseFloat(data.IBV) / 100 : 0;

            // Shift all data left and add new data at the end
            for (let i = 1; i < voltageData[1].length; i++) {
                if (useTimestamps) {
                    voltageData[0][i - 1] = voltageData[0][i]; // Shift timestamps
                }
                voltageData[1][i - 1] = voltageData[1][i];
                voltageData[2][i - 1] = voltageData[2][i];
            }
            const lastIndex = voltageData[1].length - 1;
            if (useTimestamps) {
                voltageData[0][lastIndex] = now; // New timestamp
            }
            voltageData[1][lastIndex] = adsBattV;
            voltageData[2][lastIndex] = inaBattV;

            plotUpdates.push('voltage');
        }

        if (typeof rpmPlot !== 'undefined') {
            const rpmValue = 'RPM' in data ? parseFloat(data.RPM) : 0;

            // Shift all data left and add new data at the end
            for (let i = 1; i < rpmData[1].length; i++) {
                if (useTimestamps) {
                    rpmData[0][i - 1] = rpmData[0][i]; // Shift timestamps
                }
                rpmData[1][i - 1] = rpmData[1][i];
            }
            const lastIndex = rpmData[1].length - 1;
            if (useTimestamps) {
                rpmData[0][lastIndex] = now; // New timestamp
            }
            rpmData[1][lastIndex] = rpmValue;

            plotUpdates.push('rpm');
        }

        if (typeof temperaturePlot !== 'undefined') {
            const altTemp = 'AlternatorTemperatureF' in data ? parseFloat(data.AlternatorTemperatureF) : 0;

            // Shift all data left and add new data at the end
            for (let i = 1; i < temperatureData[1].length; i++) {
                if (useTimestamps) {
                    temperatureData[0][i - 1] = temperatureData[0][i]; // Shift timestamps
                }
                temperatureData[1][i - 1] = temperatureData[1][i];
            }
            const lastIndex = temperatureData[1].length - 1;
            if (useTimestamps) {
                temperatureData[0][lastIndex] = now; // New timestamp
            }
            temperatureData[1][lastIndex] = altTemp;

            plotUpdates.push('temperature');
        }

        // Queue all plot updates at once
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

        // All echo updates with change detection - COMPLETE LIST
        const echoUpdates = [
            { key: 'TemperatureLimitF', id: 'TemperatureLimitF_echo', transform: v => v },
            { key: 'FullChargeVoltage', id: 'FullChargeVoltage_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'TargetAmps', id: 'TargetAmps_echo', transform: v => v },
            { key: 'TargetFloatVoltage', id: 'TargetFloatVoltage_echo', transform: v => (v / 100).toFixed(2) },
            { key: 'SwitchingFrequency', id: 'SwitchingFrequency_echo', transform: v => v },
            { key: 'interval', id: 'interval_echo', transform: v => (v / 100).toFixed(3) },
            { key: 'FieldAdjustmentInterval', id: 'FieldAdjustmentInterval_echo', transform: v => v },
            { key: 'ManualDuty', id: 'ManualDuty_echo', transform: v => v },
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
            { key: 'AmpControlByRPM', id: 'AmpControlByRPM_echo', transform: v => v },
            { key: 'RPM1', id: 'RPM1_echo', transform: v => v },
            { key: 'RPM2', id: 'RPM2_echo', transform: v => v },
            { key: 'RPM3', id: 'RPM3_echo', transform: v => v },
            { key: 'RPM4', id: 'RPM4_echo', transform: v => v },
            { key: 'Amps1', id: 'Amps1_echo', transform: v => v },
            { key: 'Amps2', id: 'Amps2_echo', transform: v => v },
            { key: 'Amps3', id: 'Amps3_echo', transform: v => v },
            { key: 'Amps4', id: 'Amps4_echo', transform: v => v },
            { key: 'RPM5', id: 'RPM5_echo', transform: v => v },
            { key: 'RPM6', id: 'RPM6_echo', transform: v => v },
            { key: 'RPM7', id: 'RPM7_echo', transform: v => v },
            { key: 'Amps5', id: 'Amps5_echo', transform: v => v },
            { key: 'Amps6', id: 'Amps6_echo', transform: v => v },
            { key: 'Amps7', id: 'Amps7_echo', transform: v => v },
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
            { key: 'WindingTempOffset', id: 'WindingTempOffset_echo', transform: v => (v / 10).toFixed(1) },
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
            { key: 'Ymax4', id: 'Ymax4_echo', transform: v => v }

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

        console.log(`[PROF] Echo updates: ${updatesCount} of ${echoUpdates.length + specialDisplays.length} elements changed`);
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
let userInitiatedToggles = new Set(); // Track user-initiated changes
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

const fieldIndexes = {
    AlternatorTemperatureF: 0,
    dutyCycle: 1,
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
    SOC_percent: 23,
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
    bmsLogic: 56,
    bmsLogicLevelOff: 57,
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
    ancientResetReason: 141,       // Integer code
    ManualLifePercentage: 142,
    totalPowerCycles: 143,
    lastWifiSessionDuration: 144,
    EXTRA: 145,
    BatteryCurrentSource: 146,
    timeAxisModeChanging: 147,
    webgaugesinterval: 148,
    plotTimeWindow: 149,
    Ymin1: 150,
    Ymax1: 151,
    Ymin2: 152,
    Ymax2: 153,
    Ymin3: 154,
    Ymax3: 155,
    Ymin4: 156,
    Ymax4: 157

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
                alert("Factory reset complete!\n\nPage will reload in 500ms to show default values.");
                setTimeout(() => {
                    window.location.reload();
                }, 500);
            } else {
                alert("Factory reset failed: " + text);
                button.disabled = false;
                button.textContent = 'Restore Defaults';
                button.style.backgroundColor = '#555555';
            }
        })
        .catch(err => {
            alert("Error during factory reset: " + err.message);
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
function initPlotDataStructures() {
    // Get actual values from ESP32 data, or use defaults
    const intervalMs = window._lastKnownInterval || 200;
    const timeWindowSec = window._lastKnownTimeWindow || 8; // This is in SECONDS
    const timeWindowMs = timeWindowSec * 1000; // Convert to milliseconds
    // Calculate correct buffer size
    const maxPoints = Math.ceil(timeWindowMs / intervalMs);
    const intervalSec = intervalMs / 1000;

    console.log(`Buffer: ${maxPoints} points, ${timeWindowMs / 1000}s window, ${intervalMs}ms interval`);

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


function updateTogglesFromData(data) {
    try {
        if (!data) return;

        const now = Date.now();
        const IGNORE_DURATION = 5000; // 5 seconds

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
        updateCheckbox("AmpControlByRPM_checkbox", data.AmpControlByRPM, "AmpControlByRPM");
        updateCheckbox("InvertAltAmps_checkbox", data.InvertAltAmps, "InvertAltAmps");
        updateCheckbox("InvertBattAmps_checkbox", data.InvertBattAmps, "InvertBattAmps");
        updateCheckbox("IgnitionOverride_checkbox", data.IgnitionOverride, "IgnitionOverride");
        updateCheckbox("TempSource_checkbox", data.TempSource, "TempSource");
        updateCheckbox("AlarmLatchEnabled_checkbox", data.AlarmLatchEnabled, "AlarmLatchEnabled");
        updateCheckbox("ForceFloat_checkbox", data.ForceFloat, "ForceFloat");
        updateCheckbox("AutoShuntGainCorrection_checkbox", data.AutoShuntGainCorrection, "AutoShuntGainCorrection");
        updateCheckbox("AutoAltCurrentZero_checkbox", data.AutoAltCurrentZero, "AutoAltCurrentZero");
        updateCheckbox("timeAxisModeChanging_checkbox", data.timeAxisModeChanging, "timeAxisModeChanging");
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

window.addEventListener("load", function () {
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

            if (values.length !== Object.keys(fieldIndexes).length) {
                console.warn("CSV mismatch: ESP32/HTML field count diverged");
                return;
            }
            const data = {
                AlternatorTemperatureF: values[fieldIndexes.AlternatorTemperatureF], // 0
                dutyCycle: values[fieldIndexes.dutyCycle],
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
                SOC_percent: values[fieldIndexes.SOC_percent],
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
                bmsLogic: values[fieldIndexes.bmsLogic],
                bmsLogicLevelOff: values[fieldIndexes.bmsLogicLevelOff],
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
                ancientResetReason: values[fieldIndexes.ancientResetReason],
                ManualLifePercentage: values[fieldIndexes.ManualLifePercentage],
                totalPowerCycles: values[fieldIndexes.totalPowerCycles],
                AlarmTest: values[fieldIndexes.AlarmTest],
                alarmLatchState: values[fieldIndexes.alarmLatchState],
                ResetAlarmLatch: values[fieldIndexes.ResetAlarmLatch],
                LatitudeNMEA: values[fieldIndexes.LatitudeNMEA],
                LongitudeNMEA: values[fieldIndexes.LongitudeNMEA],
                SatelliteCountNMEA: values[fieldIndexes.SatelliteCountNMEA],
                lastWifiSessionDuration: values[fieldIndexes.lastWifiSessionDuration],
                lastSessionEndTime: values[fieldIndexes.lastSessionEndTime],
                BatteryCurrentSource: values[fieldIndexes.BatteryCurrentSource],
                timeAxisModeChanging: values[fieldIndexes.timeAxisModeChanging],
                webgaugesinterval: values[fieldIndexes.webgaugesinterval],
                plotTimeWindow: values[fieldIndexes.plotTimeWindow],
                Ymin1: values[fieldIndexes.Ymin1],
                Ymax1: values[fieldIndexes.Ymax1],
                Ymin2: values[fieldIndexes.Ymin2],
                Ymax2: values[fieldIndexes.Ymax2],
                Ymin3: values[fieldIndexes.Ymin3],
                Ymax3: values[fieldIndexes.Ymax3],
                Ymin4: values[fieldIndexes.Ymin4],
                Ymax4: values[fieldIndexes.Ymax4]

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

            if (configCheckCounter >= checkInterval) {
                configCheckCounter = 0; // Reset counter
                updatePlotConfiguration(data);
            }

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
                    // Special handling for reset reason lookups
                    else if (key === "LastResetReason") {
                        newTextContent = resetReasonLookup[value] || `Unknown (${value})`;
                    }
                    else if (key === "ancientResetReason") {
                        newTextContent = resetReasonLookup[value] || `Unknown (${value})`;
                    }
                    // Values scaled by 100 on server (existing logic)
                    else if (["BatteryV", "Channel3V", "IBV", "VictronVoltage", "vvout", "FullChargeVoltage", "VictronCurrent",
                        "TargetFloatVoltage", "CurrentThreshold", "IBVMax", "Bcur", "MeasuredAmps", "MeasuredAmpsMax", "SOC_percent", "AlternatorCOffset", "BatteryCOffset", "R_fixed", "Beta", "T0_C"].includes(key)) {
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
                        const totalSeconds = value;
                        const hours = Math.floor(totalSeconds / 3600);
                        const minutes = Math.floor((totalSeconds % 3600) / 60);
                        const seconds = totalSeconds % 60;
                        newTextContent = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
                    }
                    // Session duration in minutes (new logic)
                    else if (["LastSessionDuration", "CurrentSessionDuration", "CurrentWifiSessionDuration"].includes(key)) {
                        if (value < 60) {
                            newTextContent = `${value}`;
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

            // Critical fields - update every cycle (real-time)
            const criticalFields = [
                ["MeasAmpsID", "MeasuredAmps"],          // Alternator Current
                ["BatteryVID", "BatteryV"],              // ADS Battery Voltage  
                ["IBVID", "IBV"],                        // INA Battery Voltage
                ["BCurrID", "Bcur"],                     // Battery Current
                ["RPMID", "RPM"],                        // Engine Speed
                ["LoopTimeID", "LoopTime"],              // Loop Time
                ["WifiHeartBeatID", "WifiHeartBeat"]     // WifiHeartbeat
            ];

            // Other fields - update every 4th cycle (800ms at 200ms rate)
            const otherFields = [
                ["AltTempID", "AlternatorTemperatureF"],
                ["dutyCycleID", "dutyCycle"],
                ["ADS3ID", "Channel3V"],
                ["VictronVoltageID", "VictronVoltage"],
                ["SendWifiTimeID", "SendWifiTime"],
                ["AnalogReadTimeID", "AnalogReadTime"],
                ["VeTimeID", "VeTime"],
                ["MaximumLoopTimeID", "MaximumLoopTime"],
                ["GPSHID", "HeadingNMEA"],
                ["FieldVoltsID", "vvout"],
                ["FieldAmpsID", "iiout"],
                ["FreeHeapID", "FreeHeap"],
                ["IBVMaxID", "IBVMax"],
                ["MeasuredAmpsMaxID", "MeasuredAmpsMax"],
                ["RPMMaxID", "RPMMax"],
                ["SOC_percentID", "SOC_percent"],
                ["EngineRunTimeID", "EngineRunTime"],
                ["EngineCyclesID", "EngineCycles"],
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
                ["header-voltage", "IBV"],
                ["header-soc", "SOC_percent"],
                ["header-alt-current", "MeasuredAmps"],
                ["header-batt-current", "Bcur"],
                ["header-alt-temp", "AlternatorTemperatureF"],
                ["header-rpm", "RPM"],
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
                ["ancientResetReasonID", "ancientResetReason"],
                ["totalPowerCyclesID", "totalPowerCycles"]
            ];

            // Update critical fields every cycle
            updateFields(criticalFields);

            // Update other fields every 4th cycle
            if (window.updateCounter % 4 === 0) {
                updateFields(otherFields);
            }

            // Increment echo counter
            window.echoUpdateCounter++;

            // Update echos decimation
            if (window.echoUpdateCounter % 20 === 0) { // Changed from 10 to 20 (slower updates)
                updateAllEchosOptimized(data);

                // Also update these less frequent items
                updateLifeIndicators(data); // for the alternator lifetime calculation
                updateAlarmStatus(data);

                // Field status indicator
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
            }

            // Update toggle states more frequently (every 5th cycle = 1 second)
            if (window.echoUpdateCounter % 5 === 0) {
                updateTogglesFromData(data);
            }

            // Keep critical state updates on every cycle
            if (currentAdminPassword === "") {
                document.getElementById('settings-section').classList.add("locked");
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


            // something to do with grayed out settings
            if (currentAdminPassword === "") {
                document.getElementById('settings-section').classList.add("locked");
            }

            processCSVDataOptimized(data);

            // Connection status check
            const nowMs = Date.now();
            const timeSinceLastEvent = nowMs - lastEventTime;
            if (timeSinceLastEvent > 5000) {
                updateInlineStatus(false);
            } else {
                updateInlineStatus(true);
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
    document.getElementById("AmpControlByRPM_checkbox").checked = (document.getElementById("AmpControlByRPM").value === "1");
    document.getElementById("InvertAltAmps_checkbox").checked = (document.getElementById("InvertAltAmps").value === "1");
    document.getElementById("InvertBattAmps_checkbox").checked = (document.getElementById("InvertBattAmps").value === "1");
    document.getElementById("IgnitionOverride_checkbox").checked = (document.getElementById("IgnitionOverride").value === "1");
    document.getElementById("TempSource_checkbox").checked = (document.getElementById("TempSource").value === "1");
    document.getElementById("admin_password").addEventListener("change", updatePasswordFields);
    document.getElementById("timeAxisModeChanging_checkbox").checked = (document.getElementById("timeAxisModeChanging").value === "1");

    setupInputValidation(); // Client side input validation of settings

    // Start frame time monitoring
    if (ENABLE_DETAILED_PROFILING) {
        startFrameTimeMonitoring();
        // Log performance summary every 10 seconds during development
        setInterval(() => {
            const avgFrameTime = frameTimeTracker.frameTimes.reduce((a, b) => a + b, 0) / frameTimeTracker.frameTimes.length;
            console.log(`[PERF SUMMARY] Avg frame: ${avgFrameTime.toFixed(2)}ms, Worst: ${frameTimeTracker.worstFrame.toFixed(2)}ms, FPS: ${(1000 / avgFrameTime).toFixed(1)}`);
            frameTimeTracker.worstFrame = 0; // Reset worst frame counter
        }, 10000);
    }

    setInterval(reportPlotRenderingStats, plotRenderTracker.interval);


});// <-- This is the end of the window load event listener

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


