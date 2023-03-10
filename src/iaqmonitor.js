/*! envstation.js JavaScript file for ESP32 Environment Station */

var AXISCOL = "#333333";
var DATUMCOL = "#a9a9a9";
var sInterval = 5000;
var gInterval = 5000;
var lInterval = 3600000;
var sTimer = 0;
var gTimer = 0;
var lTimer = 0;
var tempMin = -10;
var tempMax = 40;
var presMin = 980;
var presMax = 1040;
var humyMin = 0;
var humyMax = 100;
var sIAQMin = 0;
var sIAQMax = 150;
var CO2Min = 300;
var CO2Max = 1000;
var bVOCMin = 0;
var bVOCMax = 5;

// Utility function - left pads numeric string to 2 digits
function leftPad(i) {
    "use strict";

    if (i < 10) {
        i = "0" + i;
    }
    return i;

}

// Utility function - rounds number up (ud = 1) or down (ud = 0) to nearest 10
function roundTen(f, ud) {
    "use strict";

    if (ud === 0) { // Round down
        f = Math.floor((f - 0.5) / 10) * 10;
    }
    if (ud === 1) { // Round up
        f = Math.ceil((f + 0.5) / 10) * 10;
    }
    return f;

}

// Utility function - rounds number to specified decimal places
function roundTo(f, decPlace) {
    "use strict";

    f = f > 0 ? (Math.floor((f * Math.pow(10, decPlace) + 0.5))) / Math.pow(10, decPlace) :
        (Math.floor((f * Math.pow(10, decPlace) - 0.5))) / Math.pow(10, decPlace);
    return f;

}

// Utility function - reformats ISO 8601 date string to specified format
function formatDate(dateStr, format) {
    "use strict";

    var date = new Date(dateStr);
    var fdate;
    var hh;
    var mm;
    switch (format) {
        case 1: // "M-D hh:mm"
            hh = leftPad(date.getHours());
            mm = leftPad(date.getMinutes());
            fdate = (date.getMonth() + 1) + "-" + date.getDate() + " " + hh + ":"
                + mm;
            break;
        default: // full date
            fdate = date;
    }
    return fdate;

}

// Utility function - formats uptime as H:MM:SS
function formatUptime(duration) {
    "use strict";

    var hrs = Math.floor(duration / 3600);
    var mins = Math.floor((duration % 3600) / 60);
    var secs = Math.floor(duration % 60);
    var fuptime = "";

    fuptime += "" + hrs + ":" + (mins < 10 ? "0" : "");
    fuptime += "" + mins + ":" + (secs < 10 ? "0" : "");
    fuptime += "" + secs;
    return fuptime;

}

// Calculate approximate dew point using Magnus formula with Sonntag constants
function calcDewPoint(temp, rh) {
    "use strict";

    var b = 17.62;
    var c = 243.12;
    var gamma = Math.log(rh / 100) + ((b * temp) / (c + temp));
    var dewp = (c * gamma) / (b - gamma);
    return dewp;

}

// Execute REST GET request to retrieve sensor data
function getSensor() {
    "use strict";

    var obj;
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/sensor');
    xhr.onload = function () {
        if (xhr.status === 200) {
            obj = JSON.parse(xhr.responseText);
            document.getElementById('upTime').innerHTML = formatUptime(obj.uptime);
            document.getElementById('currDate').innerHTML = formatDate(obj.time, 0);
            document.getElementById('tempValue').innerHTML = roundTo(obj.temp, 1);
            document.getElementById('presValue').innerHTML = roundTo(obj.pres, 1);
            document.getElementById('humyValue').innerHTML = roundTo(obj.humy, 1);
            document.getElementById('sIAQValue').innerHTML = roundTo(obj.IAQ, 1);
            document.getElementById('IAQAccuracy').innerHTML = obj.IAQacc;
            document.getElementById('CO2Value').innerHTML = roundTo(obj.CO2, 1);
            document.getElementById('bVOCValue').innerHTML = roundTo(obj.VOC, 1);
            document.getElementById('dewpValue').innerHTML = roundTo(calcDewPoint(obj.temp, obj.humy), 1);
        }
        else {
            alert('Request failed.  Returned status of ' + xhr.status);
        }
    };
    xhr.send();

}

// Set interval timers for sensor reading and graph updates
function setTimers() {
    "use strict";

    clearInterval(sTimer);
    sTimer = setInterval("getSensor()", sInterval);
    clearInterval(gTimer);
    gTimer = setInterval("getLog()", gInterval);

}

// Execute REST GET request to retrieve server side configuration parameters
function getConfig() {
    "use strict";

    var obj;
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/config');
    xhr.onload = function () {
        if (xhr.status === 200) {
            obj = JSON.parse(xhr.responseText);
            sInterval = parseInt(obj.sensorInt);
            sInterval = isNaN(sInterval) ? 5000 : sInterval;
            gInterval = parseInt(obj.graphInt);
            gInterval = isNaN(gInterval) ? 5000 : gInterval;
            lInterval = parseInt(obj.logInt);
            lInterval = isNaN(lInterval) ? 3600000 : lInterval;

            document.getElementById("sIntInput").value = sInterval;
            document.getElementById("gIntInput").value = gInterval;
            document.getElementById("lIntInput").value = lInterval;

            setTimers();
        }
    };
    xhr.send();

}

// Execute REST PUT request to update server config (interval timers)
function putConfig() {
    "use strict";

    sInterval = parseInt(document.getElementById("sIntInput").value);
    gInterval = parseInt(document.getElementById("gIntInput").value);
    lInterval = parseInt(document.getElementById("lIntInput").value);

    var obj = {
        sensorInt: sInterval,
        graphInt: gInterval,
        logInt: lInterval,
    };
    var args = JSON.stringify(obj);

    var xhr = new XMLHttpRequest();
    xhr.open('PUT', '/config');
    xhr.onload = function () {
        if (xhr.status === 200) {
            setTimers();
            alert("Update Successful");
        }
        if (xhr.status === 500) {
            alert("Update Failed");
        }
        if (xhr.status === 404) {
            alert("Resource Not Found");
        }
    };
    xhr.send(args);

}

// Execute REST GET request to retrieve log file
function getLog() {
    "use strict";

    var obj;
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/log');
    xhr.onload = function () {
        if (xhr.status === 200) {
            obj = JSON.parse(xhr.responseText);
            plotGraphs(obj);
        };
    };
    xhr.send();

}

// Plot historical trend graphs from logged json data
// Autorange y-axis based on min and max log values
function plotGraphs(obj) {
    "use strict";

    var ROUNDUP = 1;
    var ROUNDDOWN = 0;
    var xmin;
    var xmax;
    var ymin;
    var ymax;
    var i;
    var col;

    // Log file available so plot data
    var data = new Array(obj.logfile.length);

    // Get earliest and latest log dates for X axis labels
    xmin = formatDate(obj.logfile[0].time, 1);
    xmax = formatDate(obj.logfile[obj.logfile.length - 1].time, 1);

    ymin = tempMin;
    ymax = tempMax;
    for (i = 0; i < obj.logfile.length; i += 1) {
        data[i] = obj.logfile[i].temp;
        ymin = (data[i] < ymin ? roundTen(data[i], ROUNDDOWN) : ymin);
        ymax = (data[i] > ymax ? roundTen(data[i], ROUNDUP) : ymax);
    }
    col = "#cc5555";
    drawGraph("tempChart", data, xmin, xmax, ymin, ymax, col);

    ymin = presMin;
    ymax = presMax;
    for (i = 0; i < obj.logfile.length; i += 1) {
        data[i] = obj.logfile[i].pres;
        ymin = (data[i] < ymin ? roundTen(data[i], ROUNDDOWN) : ymin);
        ymax = (data[i] > ymax ? roundTen(data[i], ROUNDUP) : ymax);
    }
    col = "#669900";
    drawGraph("presChart", data, xmin, xmax, ymin, ymax, col);

    ymin = humyMin;
    ymax = humyMax;
    for (i = 0; i < obj.logfile.length; i += 1) {
        data[i] = obj.logfile[i].humy;
        ymin = (data[i] < ymin ? roundTen(data[i], ROUNDDOWN) : ymin);
        ymax = (data[i] > ymax ? roundTen(data[i], ROUNDUP) : ymax);
    }
    col = "#0099ff";
    drawGraph("humyChart", data, xmin, xmax, ymin, ymax, col);

    ymin = sIAQMin;
    ymax = sIAQMax;
    for (i = 0; i < obj.logfile.length; i += 1) {
        data[i] = obj.logfile[i].IAQ;
        ymin = (data[i] < ymin ? roundTen(data[i], ROUNDDOWN) : ymin);
        ymax = (data[i] > ymax ? roundTen(data[i], ROUNDUP) : ymax);
    }
    col = "#b266ff";
    drawGraph("sIAQChart", data, xmin, xmax, ymin, ymax, col);

    ymin = CO2Min;
    ymax = CO2Max;
    for (i = 0; i < obj.logfile.length; i += 1) {
        data[i] = obj.logfile[i].CO2
        ymin = (data[i] < ymin ? roundTen(data[i], ROUNDDOWN) : ymin);
        ymax = (data[i] > ymax ? roundTen(data[i], ROUNDUP) : ymax);
    }
    col = "#aaaa00";
    drawGraph("CO2Chart", data, xmin, xmax, ymin, ymax, col);

    ymin = bVOCMin;
    ymax = bVOCMax;
    for (i = 0; i < obj.logfile.length; i += 1) {
        data[i] = obj.logfile[i].VOC;
        ymin = (data[i] < ymin ? roundTen(data[i], ROUNDDOWN) : ymin);
        ymax = (data[i] > ymax ? roundTen(data[i], ROUNDUP) : ymax);
    }
    col = "#cc6600";
    drawGraph("bVOCChart", data, xmin, xmax, ymin, ymax, col);

}

// Get sIAQ description
function getIAQDesc(sIAQ) {
    "use strict";

    var desc = "Good";
    if (sIAQ > 50) {
        desc = "Fair";
    }
    if (sIAQ > 50) {
        desc = "Average";
    }
    if (sIAQ > 100) {
        desc = "Slightly bad";
    }
    if (sIAQ > 150) {
        desc = "Bad";
    }
    if (sIAQ > 200) {
        desc = "Worse";
    }
    if (sIAQ > 300) {
        desc = "Very bad";
    }
    return desc;
}

// Draw graph
function drawGraph(id, data, xmin, xmax, ymin, ymax, col) {
    "use strict";

    var SL = 1013.25; // standard sea level pressure
    var XOFFSET = 30; // Space required for X labels & ticks
    var YOFFSET = 25; // Space required for Y labels & ticks
    var YTICK = (ymax - ymin) / 10;
    var c = document.getElementById(id);
    var ctx = c.getContext("2d");
    var scale = (c.height - YOFFSET) / (ymax - ymin);
    var xt = 10 * (c.width - XOFFSET) / data.length;
    var yt = YTICK * (c.height - YOFFSET) / (ymax - ymin);
    var i;
    var ticktime = lInterval / 100;
    var tickunit = "secs";

    if (col == null) {
        col = "#000000";
    }

    ctx.translate(0.5, 0.5); // translate to sharpen lines

    // Draw axes
    ctx.clearRect(0, 0, c.width, c.height);
    ctx.beginPath();
    ctx.moveTo(XOFFSET, 0);
    ctx.lineTo(XOFFSET, c.height - YOFFSET);
    ctx.lineTo(c.width, c.height - YOFFSET);
    ctx.strokeStyle = AXISCOL;
    ctx.stroke();
    ctx.closePath();

    // Draw labels
    if (ticktime >= 60) {
        ticktime /= 60;
        tickunit = ticktime == 1 ? "min" : "mins";
    }
    if (ticktime >= 60) {
        ticktime /= 60;
        tickunit = ticktime == 1 ? "hour" : "hours";
    }
    ctx.fillStyle = col;
    ctx.font = "10px sans-serif";
    ctx.textAlign = "end";
    ctx.fillText(ymax, XOFFSET - 7, 10);
    ctx.fillText(ymin, XOFFSET - 7, c.height - YOFFSET + 5);
    ctx.fillStyle = AXISCOL;
    ctx.textAlign = "center";
    ctx.fillText(xmin, XOFFSET, c.height - 5);
    ctx.fillText("1 tick = " + ticktime + " " + tickunit, ((c.width + XOFFSET) / 2), c.height - 5);
    ctx.textAlign = "end";
    ctx.fillText(xmax, c.width - 2, c.height - 5);

    // Draw axis ticks
    ctx.beginPath();
    ctx.strokeStyle = AXISCOL;
    for (i = c.height - YOFFSET; i > 0; i -= yt) {
        ctx.moveTo(XOFFSET - 5, i);
        ctx.lineTo(XOFFSET, i);
        ctx.stroke();
    }
    for (i = XOFFSET; i < c.width; i += xt) {
        ctx.moveTo(i, c.height - YOFFSET);
        ctx.lineTo(i, c.height - YOFFSET + 3);
        ctx.stroke();
    }
    ctx.closePath();

    // Plot zero degrees datum on temperature chart
    if (c.id === "tempChart" && ymin < 0 && ymax > 0) {
        ctx.beginPath();
        ctx.strokeStyle = DATUMCOL;
        ctx.moveTo(XOFFSET, c.height - YOFFSET - ((0 - ymin) * scale));
        ctx.lineTo(c.width, c.height - YOFFSET - ((0 - ymin) * scale));
        ctx.stroke();
        ctx.closePath();
    }

    // Plot nominal sea level datum on pressure chart
    if (c.id === "presChart" && ymin < SL && ymax > SL) {
        ctx.beginPath();
        ctx.strokeStyle = DATUMCOL;
        ctx.moveTo(XOFFSET, c.height - YOFFSET - ((SL - ymin) * scale));
        ctx.lineTo(c.width, c.height - YOFFSET - ((SL - ymin) * scale));
        ctx.stroke();
        ctx.closePath();
    }

    // Plot data
    ctx.beginPath();
    ctx.strokeStyle = col;
    ctx.moveTo(XOFFSET, c.height - YOFFSET - ((data[0] - ymin) * scale));
    for (i = 1; i < data.length; i += 1) {
        ctx.lineTo(XOFFSET + (i * (c.width - XOFFSET) / data.length), c.height
            - YOFFSET - ((data[i] - ymin) * scale));
    }
    ctx.stroke();
    ctx.closePath();
    ctx.translate(-0.5, -0.5); // cancel translate

}

// Functions to call when body first loaded
function start() {
    "use strict";

    getConfig(); // Get config with refresh intervals
    getSensor(); // Get initial sensor reading
    getLog();    // Get initial log reading

}
