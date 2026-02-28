"use strict";
load('sbbsdefs.js');
require("key_defs.js", "KEY_UP");
require("key_defs.js", "KEY_DOWN");
require("key_defs.js", "KEY_LEFT");
require("key_defs.js", "KEY_RIGHT");
require("key_defs.js", "KEY_ESC");
require("key_defs.js", "KEY_HOME");
require("key_defs.js", "KEY_END");
"use strict";
function clamp(value, min, max) {
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}
function lerp(a, b, t) {
    return a + (b - a) * t;
}
function distance(a, b) {
    var dx = b.x - a.x;
    var dy = b.y - a.y;
    return Math.sqrt(dx * dx + dy * dy);
}
function distanceSquared(a, b) {
    var dx = b.x - a.x;
    var dy = b.y - a.y;
    return dx * dx + dy * dy;
}
function normalize(v) {
    var len = Math.sqrt(v.x * v.x + v.y * v.y);
    if (len === 0)
        return { x: 0, y: 0 };
    return { x: v.x / len, y: v.y / len };
}
function dot(a, b) {
    return a.x * b.x + a.y * b.y;
}
function rotate(p, angle) {
    var cos = Math.cos(angle);
    var sin = Math.sin(angle);
    return {
        x: p.x * cos - p.y * sin,
        y: p.x * sin + p.y * cos
    };
}
function degToRad(degrees) {
    return degrees * Math.PI / 180;
}
function radToDeg(radians) {
    return radians * 180 / Math.PI;
}
function wrapAngle(angle) {
    while (angle > Math.PI)
        angle -= 2 * Math.PI;
    while (angle < -Math.PI)
        angle += 2 * Math.PI;
    return angle;
}
function sign(x) {
    if (x > 0)
        return 1;
    if (x < 0)
        return -1;
    return 0;
}
"use strict";
var Rand = (function () {
    function Rand(seed) {
        this.seed = seed !== undefined ? seed : Date.now();
    }
    Rand.prototype.next = function () {
        this.seed = (this.seed * 1103515245 + 12345) & 0x7fffffff;
        return this.seed / 0x7fffffff;
    };
    Rand.prototype.nextInt = function (min, max) {
        return Math.floor(this.next() * (max - min + 1)) + min;
    };
    Rand.prototype.nextFloat = function (min, max) {
        return this.next() * (max - min) + min;
    };
    Rand.prototype.nextBool = function (probability) {
        var p = probability !== undefined ? probability : 0.5;
        return this.next() < p;
    };
    Rand.prototype.pick = function (array) {
        var index = this.nextInt(0, array.length - 1);
        return array[index];
    };
    Rand.prototype.shuffle = function (array) {
        for (var i = array.length - 1; i > 0; i--) {
            var j = this.nextInt(0, i);
            var temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
        return array;
    };
    Rand.prototype.setSeed = function (seed) {
        this.seed = seed;
    };
    return Rand;
}());
var globalRand = new Rand();
"use strict";
var DEBUG_LOG_FILE = "outrun_debug.log";
function repeatString(str, count) {
    var result = "";
    for (var i = 0; i < count; i++) {
        result += str;
    }
    return result;
}
var DebugLogger = (function () {
    function DebugLogger() {
        this.file = null;
        this.enabled = false;
        this.startTime = 0;
        this.logPath = "";
    }
    DebugLogger.prototype.init = function () {
        try {
            this.logPath = DEBUG_LOG_FILE;
            this.startTime = Date.now();
            if (typeof console !== 'undefined' && console.print) {
                console.print("DEBUG: Log path = " + this.logPath + "\r\n");
            }
            this.file = new File(this.logPath);
            if (!this.file.open("w")) {
                if (typeof console !== 'undefined' && console.print) {
                    console.print("DEBUG: Failed to open log file!\r\n");
                }
                this.enabled = false;
                return false;
            }
            this.enabled = true;
            this.writeRaw(repeatString("=", 60));
            this.writeRaw("OutRun ANSI Debug Log");
            this.writeRaw("Started: " + new Date().toISOString());
            this.writeRaw("Log file: " + this.logPath);
            this.writeRaw(repeatString("=", 60));
            this.writeRaw("");
            return true;
        }
        catch (e) {
            this.enabled = false;
            return false;
        }
    };
    DebugLogger.prototype.writeRaw = function (text) {
        if (!this.enabled || !this.file)
            return;
        try {
            this.file.writeln(text);
            this.file.flush();
        }
        catch (e) {
        }
    };
    DebugLogger.prototype.getElapsed = function () {
        var elapsed = (Date.now() - this.startTime) / 1000;
        return elapsed.toFixed(3);
    };
    DebugLogger.prototype.write = function (level, message) {
        if (!this.enabled || !this.file)
            return;
        var timestamp = "[" + this.getElapsed() + "s]";
        var line = timestamp + " " + level + " " + message;
        this.writeRaw(line);
    };
    DebugLogger.prototype.debug = function (message) {
        this.write("[DEBUG]", message);
    };
    DebugLogger.prototype.info = function (message) {
        this.write("[INFO ]", message);
    };
    DebugLogger.prototype.warn = function (message) {
        this.write("[WARN ]", message);
    };
    DebugLogger.prototype.error = function (message) {
        this.write("[ERROR]", message);
    };
    DebugLogger.prototype.exception = function (message, error) {
        this.write("[ERROR]", message);
        if (error) {
            this.writeRaw("  Exception: " + String(error));
            if (error.stack) {
                this.writeRaw("  Stack trace:");
                var stack = String(error.stack).split("\n");
                for (var i = 0; i < stack.length; i++) {
                    this.writeRaw("    " + stack[i]);
                }
            }
            if (error.fileName) {
                this.writeRaw("  File: " + error.fileName);
            }
            if (error.lineNumber) {
                this.writeRaw("  Line: " + error.lineNumber);
            }
        }
    };
    DebugLogger.prototype.logState = function (label, state) {
        this.write("[STATE]", label);
        try {
            var json = JSON.stringify(state, null, 2);
            var lines = json.split("\n");
            for (var i = 0; i < lines.length; i++) {
                this.writeRaw("  " + lines[i]);
            }
        }
        catch (e) {
            this.writeRaw("  (could not serialize state)");
        }
    };
    DebugLogger.prototype.logVehicle = function (vehicle) {
        this.debug("Vehicle: playerX=" + vehicle.playerX.toFixed(3) +
            " trackZ=" + vehicle.trackZ.toFixed(1) +
            " speed=" + vehicle.speed.toFixed(1) +
            " offRoad=" + vehicle.isOffRoad +
            " crashed=" + vehicle.isCrashed +
            " lap=" + vehicle.lap);
    };
    DebugLogger.prototype.logInput = function (key, action) {
        var keyStr = key;
        if (key === " ")
            keyStr = "SPACE";
        else if (key === "\r")
            keyStr = "ENTER";
        else if (key.charCodeAt(0) < 32)
            keyStr = "0x" + key.charCodeAt(0).toString(16);
        this.debug("Input: key='" + keyStr + "' action=" + action);
    };
    DebugLogger.prototype.separator = function (label) {
        if (label) {
            this.writeRaw("");
            this.writeRaw("--- " + label + " " + repeatString("-", 50 - label.length));
        }
        else {
            this.writeRaw(repeatString("-", 60));
        }
    };
    DebugLogger.prototype.close = function () {
        if (this.file) {
            this.writeRaw("");
            this.writeRaw(repeatString("=", 60));
            this.writeRaw("Log ended: " + new Date().toISOString());
            this.writeRaw(repeatString("=", 60));
            try {
                this.file.close();
            }
            catch (e) {
            }
            this.file = null;
        }
        this.enabled = false;
    };
    return DebugLogger;
}());
var debugLog = new DebugLogger();
"use strict";
var LogLevel;
(function (LogLevel) {
    LogLevel[LogLevel["DEBUG"] = 7] = "DEBUG";
    LogLevel[LogLevel["INFO"] = 6] = "INFO";
    LogLevel[LogLevel["NOTICE"] = 5] = "NOTICE";
    LogLevel[LogLevel["WARNING"] = 4] = "WARNING";
    LogLevel[LogLevel["ERROR"] = 3] = "ERROR";
    LogLevel[LogLevel["CRITICAL"] = 2] = "CRITICAL";
    LogLevel[LogLevel["ALERT"] = 1] = "ALERT";
    LogLevel[LogLevel["EMERGENCY"] = 0] = "EMERGENCY";
})(LogLevel || (LogLevel = {}));
var currentLogLevel = LogLevel.INFO;
function setLogLevel(level) {
    currentLogLevel = level;
}
function logMessage(level, message) {
    if (level <= currentLogLevel) {
        if (typeof log !== 'undefined') {
            log(level, "[OutRun] " + message);
        }
    }
}
function logDebug(message) {
    logMessage(LogLevel.DEBUG, message);
}
function logInfo(message) {
    logMessage(LogLevel.INFO, message);
}
function logWarning(message) {
    logMessage(LogLevel.WARNING, message);
}
function logError(message) {
    logMessage(LogLevel.ERROR, message);
}
"use strict";
function loadConfig() {
    var configPath = js.exec_dir + 'synthkart.ini';
    var config = {
        general: {
            gameName: 'OUTRUN'
        },
        highscores: {
            server: 'file',
            port: 10088,
            serviceName: 'synthkart',
            filePath: 'highscores.json'
        },
        ansiTunnel: {
            directory: 'ansi_art',
            maxRows: 2000,
            scrollSpeed: 0.5
        }
    };
    if (!file_exists(configPath)) {
        logWarning('Config file not found: ' + configPath + ', using defaults');
        return config;
    }
    try {
        var file = new File(configPath);
        if (!file.open('r')) {
            logWarning('Failed to open config file: ' + configPath);
            return config;
        }
        var currentSection = '';
        var line;
        while ((line = file.readln()) !== null) {
            line = line.trim();
            if (line === '' || line.charAt(0) === ';' || line.charAt(0) === '#') {
                continue;
            }
            if (line.charAt(0) === '[' && line.charAt(line.length - 1) === ']') {
                currentSection = line.substring(1, line.length - 1).toLowerCase().replace(/_/g, '');
                continue;
            }
            var equals = line.indexOf('=');
            if (equals === -1)
                continue;
            var key = line.substring(0, equals).trim().toLowerCase().replace(/_/g, '');
            var value = line.substring(equals + 1).trim();
            if (value.length >= 2 && value.charAt(0) === '"' && value.charAt(value.length - 1) === '"') {
                value = value.substring(1, value.length - 1);
            }
            if (currentSection === 'general') {
                if (key === 'gamename')
                    config.general.gameName = value;
            }
            else if (currentSection === 'highscores') {
                if (key === 'server') {
                    config.highscores.server = value;
                }
                else if (key === 'port') {
                    var p = parseInt(value, 10);
                    if (!isNaN(p) && p > 0)
                        config.highscores.port = p;
                }
                else if (key === 'servicename') {
                    config.highscores.serviceName = value;
                }
                else if (key === 'filepath') {
                    config.highscores.filePath = value;
                }
            }
            else if (currentSection === 'ansitunnel') {
                if (key === 'directory') {
                    config.ansiTunnel.directory = value;
                }
                else if (key === 'maxrows') {
                    var num = parseInt(value, 10);
                    if (!isNaN(num) && num > 0)
                        config.ansiTunnel.maxRows = num;
                }
                else if (key === 'scrollspeed') {
                    var speed = parseFloat(value);
                    if (!isNaN(speed) && speed > 0)
                        config.ansiTunnel.scrollSpeed = speed;
                }
            }
        }
        file.close();
    }
    catch (e) {
        logError('Error loading config file: ' + e);
    }
    if (config.highscores.filePath.charAt(0) !== '/' &&
        config.highscores.filePath.indexOf(':') === -1) {
        config.highscores.filePath = js.exec_dir + config.highscores.filePath;
    }
    if (config.ansiTunnel.directory.charAt(0) !== '/' &&
        config.ansiTunnel.directory.indexOf(':') === -1) {
        config.ansiTunnel.directory = js.exec_dir + config.ansiTunnel.directory;
    }
    logInfo('Config loaded: ansiDir=' + config.ansiTunnel.directory +
        ' hsServer=' + config.highscores.server);
    return config;
}
var OUTRUN_CONFIG = loadConfig();
"use strict";
var Clock = (function () {
    function Clock() {
        this.lastTime = this.now();
    }
    Clock.prototype.now = function () {
        return system.timer * 1000;
    };
    Clock.prototype.getDelta = function () {
        var currentTime = this.now();
        var delta = currentTime - this.lastTime;
        this.lastTime = currentTime;
        if (delta > 250) {
            delta = 250;
        }
        return delta;
    };
    Clock.prototype.reset = function () {
        this.lastTime = this.now();
    };
    return Clock;
}());
"use strict";
var FixedTimestep = (function () {
    function FixedTimestep(config) {
        this.tickDuration = 1000 / config.tickRate;
        this.maxTicks = config.maxTicksPerFrame;
        this.accumulator = 0;
    }
    FixedTimestep.prototype.update = function (deltaMs) {
        this.accumulator += deltaMs;
        var ticks = 0;
        while (this.accumulator >= this.tickDuration && ticks < this.maxTicks) {
            this.accumulator -= this.tickDuration;
            ticks++;
        }
        if (ticks >= this.maxTicks) {
            this.accumulator = 0;
        }
        return ticks;
    };
    FixedTimestep.prototype.getAlpha = function () {
        return this.accumulator / this.tickDuration;
    };
    FixedTimestep.prototype.getDt = function () {
        return this.tickDuration / 1000;
    };
    FixedTimestep.prototype.reset = function () {
        this.accumulator = 0;
    };
    return FixedTimestep;
}());
"use strict";
var GameAction;
(function (GameAction) {
    GameAction[GameAction["NONE"] = 0] = "NONE";
    GameAction[GameAction["ACCELERATE"] = 1] = "ACCELERATE";
    GameAction[GameAction["BRAKE"] = 2] = "BRAKE";
    GameAction[GameAction["STEER_LEFT"] = 3] = "STEER_LEFT";
    GameAction[GameAction["STEER_RIGHT"] = 4] = "STEER_RIGHT";
    GameAction[GameAction["ACCEL_LEFT"] = 5] = "ACCEL_LEFT";
    GameAction[GameAction["ACCEL_RIGHT"] = 6] = "ACCEL_RIGHT";
    GameAction[GameAction["BRAKE_LEFT"] = 7] = "BRAKE_LEFT";
    GameAction[GameAction["BRAKE_RIGHT"] = 8] = "BRAKE_RIGHT";
    GameAction[GameAction["USE_ITEM"] = 9] = "USE_ITEM";
    GameAction[GameAction["PAUSE"] = 10] = "PAUSE";
    GameAction[GameAction["QUIT"] = 11] = "QUIT";
})(GameAction || (GameAction = {}));
var InputMap = (function () {
    function InputMap() {
        this.bindings = {};
        this.setupDefaultBindings();
    }
    InputMap.prototype.setupDefaultBindings = function () {
        this.bind(KEY_UP, GameAction.ACCELERATE);
        this.bind(KEY_DOWN, GameAction.BRAKE);
        this.bind(KEY_LEFT, GameAction.STEER_LEFT);
        this.bind(KEY_RIGHT, GameAction.STEER_RIGHT);
        this.bind('7', GameAction.ACCEL_LEFT);
        this.bind('8', GameAction.ACCELERATE);
        this.bind('9', GameAction.ACCEL_RIGHT);
        this.bind('4', GameAction.STEER_LEFT);
        this.bind('5', GameAction.BRAKE);
        this.bind('6', GameAction.STEER_RIGHT);
        this.bind('1', GameAction.BRAKE_LEFT);
        this.bind('2', GameAction.BRAKE);
        this.bind('3', GameAction.BRAKE_RIGHT);
        this.bind(' ', GameAction.USE_ITEM);
        this.bind('p', GameAction.PAUSE);
        this.bind('P', GameAction.PAUSE);
        this.bind('\r', GameAction.PAUSE);
        this.bind('q', GameAction.QUIT);
        this.bind('Q', GameAction.QUIT);
        this.bind('x', GameAction.QUIT);
        this.bind('X', GameAction.QUIT);
        this.bind('\x1b', GameAction.QUIT);
    };
    InputMap.prototype.bind = function (key, action) {
        this.bindings[key] = action;
    };
    InputMap.prototype.getAction = function (key) {
        var action = this.bindings[key];
        return action !== undefined ? action : GameAction.NONE;
    };
    return InputMap;
}());
"use strict";
var Controls = (function () {
    function Controls(inputMap) {
        this.inputMap = inputMap;
        this.lastKeyTime = {};
        this.activeActions = {};
        this.justPressedActions = {};
        this.holdThreshold = 200;
        this.currentAccel = 0;
        this.currentSteer = 0;
        this.lastAccelAction = 1;
    }
    Controls.prototype.handleKey = function (key, now) {
        var action = this.inputMap.getAction(key);
        if (action !== GameAction.NONE) {
            if (!this.activeActions[action]) {
                this.justPressedActions[action] = true;
            }
            this.activeActions[action] = true;
            this.lastKeyTime[action] = now;
            this.applyAction(action);
        }
    };
    Controls.prototype.applyAction = function (action) {
        switch (action) {
            case GameAction.ACCELERATE:
                this.currentAccel = 1;
                this.lastAccelAction = 1;
                break;
            case GameAction.BRAKE:
                this.currentAccel = -1;
                this.lastAccelAction = -1;
                break;
            case GameAction.STEER_LEFT:
                this.currentSteer = -1;
                break;
            case GameAction.STEER_RIGHT:
                this.currentSteer = 1;
                break;
            case GameAction.ACCEL_LEFT:
                this.currentAccel = 1;
                this.currentSteer = -1;
                this.lastAccelAction = 1;
                break;
            case GameAction.ACCEL_RIGHT:
                this.currentAccel = 1;
                this.currentSteer = 1;
                this.lastAccelAction = 1;
                break;
            case GameAction.BRAKE_LEFT:
                this.currentAccel = -1;
                this.currentSteer = -1;
                this.lastAccelAction = -1;
                break;
            case GameAction.BRAKE_RIGHT:
                this.currentAccel = -1;
                this.currentSteer = 1;
                this.lastAccelAction = -1;
                break;
        }
    };
    Controls.prototype.update = function (now) {
        this.currentSteer = 0;
        for (var actionStr in this.lastKeyTime) {
            var action = parseInt(actionStr, 10);
            if (now - this.lastKeyTime[action] > this.holdThreshold) {
                this.activeActions[action] = false;
                delete this.lastKeyTime[action];
                if (action === GameAction.ACCELERATE || action === GameAction.BRAKE ||
                    action === GameAction.ACCEL_LEFT || action === GameAction.ACCEL_RIGHT ||
                    action === GameAction.BRAKE_LEFT || action === GameAction.BRAKE_RIGHT) {
                    this.currentAccel = 0;
                }
            }
            else if (this.activeActions[action]) {
                this.applyAction(action);
            }
        }
    };
    Controls.prototype.getAcceleration = function () {
        return this.currentAccel;
    };
    Controls.prototype.getSteering = function () {
        return this.currentSteer;
    };
    Controls.prototype.isActive = function (action) {
        return this.activeActions[action] === true;
    };
    Controls.prototype.wasJustPressed = function (action) {
        return this.justPressedActions[action] === true;
    };
    Controls.prototype.consumeJustPressed = function (action) {
        if (this.justPressedActions[action] === true) {
            this.justPressedActions[action] = false;
            return true;
        }
        return false;
    };
    Controls.prototype.endFrame = function () {
        this.justPressedActions = {};
    };
    Controls.prototype.clearAll = function () {
        this.activeActions = {};
        this.justPressedActions = {};
        this.lastKeyTime = {};
    };
    Controls.prototype.getLastAccelAction = function () {
        return this.lastAccelAction;
    };
    return Controls;
}());
"use strict";
var nextEntityId = 1;
function generateEntityId() {
    return nextEntityId++;
}
var Entity = (function () {
    function Entity() {
        this.id = generateEntityId();
        this.x = 0;
        this.y = 0;
        this.z = 0;
        this.rotation = 0;
        this.speed = 0;
        this.active = true;
    }
    return Entity;
}());
"use strict";
function neutralIntent() {
    return {
        accelerate: 0,
        steer: 0,
        useItem: false
    };
}
"use strict";
var HumanDriver = (function () {
    function HumanDriver(controls) {
        this.controls = controls;
        this.canMove = true;
    }
    HumanDriver.prototype.setCanMove = function (canMove) {
        this.canMove = canMove;
    };
    HumanDriver.prototype.update = function (_vehicle, _track, _dt) {
        if (!this.canMove) {
            return {
                accelerate: 0,
                steer: 0,
                useItem: false
            };
        }
        return {
            accelerate: this.controls.getAcceleration(),
            steer: this.controls.getSteering(),
            useItem: this.controls.wasJustPressed(GameAction.USE_ITEM)
        };
    };
    return HumanDriver;
}());
"use strict";
var CpuDriver = (function () {
    function CpuDriver(difficulty) {
        this.difficulty = clamp(difficulty, 0, 1);
    }
    CpuDriver.prototype.update = function (_vehicle, _track, _dt) {
        return {
            accelerate: 0.8 * this.difficulty,
            steer: 0,
            useItem: false
        };
    };
    return CpuDriver;
}());
"use strict";
var CommuterDriver = (function () {
    function CommuterDriver(speedFactor) {
        this.speedFactor = speedFactor !== undefined ? speedFactor : 0.3 + Math.random() * 0.2;
        this.driftAmount = 0.1 + Math.random() * 0.1;
        this.driftDirection = 0;
        this.driftTimer = 0;
        this.driftInterval = 2 + Math.random() * 3;
        this.active = false;
        this.activationRange = 400;
    }
    CommuterDriver.prototype.update = function (vehicle, _track, dt) {
        if (!this.active) {
            return {
                accelerate: 0,
                steer: 0,
                useItem: false
            };
        }
        this.driftTimer += dt;
        if (this.driftTimer >= this.driftInterval) {
            this.driftTimer = 0;
            var rand = Math.random();
            if (rand < 0.3) {
                this.driftDirection = -1;
            }
            else if (rand < 0.6) {
                this.driftDirection = 1;
            }
            else {
                this.driftDirection = 0;
            }
            if (vehicle.playerX > 0.5) {
                this.driftDirection = -1;
            }
            else if (vehicle.playerX < -0.5) {
                this.driftDirection = 1;
            }
        }
        return {
            accelerate: this.speedFactor,
            steer: this.driftDirection * this.driftAmount,
            useItem: false
        };
    };
    CommuterDriver.prototype.getSpeedFactor = function () {
        return this.speedFactor;
    };
    CommuterDriver.prototype.isActive = function () {
        return this.active;
    };
    CommuterDriver.prototype.activate = function () {
        this.active = true;
    };
    CommuterDriver.prototype.deactivate = function () {
        this.active = false;
    };
    CommuterDriver.prototype.getActivationRange = function () {
        return this.activationRange;
    };
    return CommuterDriver;
}());
"use strict";
var RacerDriver = (function () {
    function RacerDriver(skill, name) {
        this.skill = clamp(skill, 0.3, 1.0);
        this.name = name || this.generateName();
        this.targetSpeed = 0.90 + (this.skill * 0.10);
        this._aggression = 0.3 + (this.skill * 0.5);
        this._reactionDelay = 0.3 - (this.skill * 0.25);
        this.preferredLine = (Math.random() - 0.5) * 0.6;
        this.steerAmount = 0;
        this.variationTimer = 0;
        this.speedVariation = 0;
        this.canMove = false;
        this.itemUseCooldown = 0;
    }
    RacerDriver.prototype.setCanMove = function (canMove) {
        this.canMove = canMove;
    };
    RacerDriver.prototype.generateName = function () {
        var firstNames = [
            'Max', 'Alex', 'Sam', 'Jordan', 'Casey', 'Morgan', 'Taylor', 'Riley',
            'Ace', 'Blaze', 'Storm', 'Flash', 'Turbo', 'Nitro', 'Dash', 'Spike'
        ];
        var lastNames = [
            'Speed', 'Racer', 'Driver', 'Storm', 'Thunder', 'Blitz', 'Volt', 'Dash',
            'Rocket', 'Jet', 'Zoom', 'Rush', 'Gear', 'Torque', 'Drift', 'Burn'
        ];
        var firstName = firstNames[Math.floor(Math.random() * firstNames.length)];
        var lastName = lastNames[Math.floor(Math.random() * lastNames.length)];
        return firstName + ' ' + lastName;
    };
    RacerDriver.prototype.update = function (vehicle, _track, dt) {
        if (!this.canMove) {
            return {
                accelerate: 0,
                steer: 0,
                useItem: false
            };
        }
        if (this.itemUseCooldown > 0) {
            this.itemUseCooldown -= dt;
        }
        this.variationTimer += dt;
        if (this.variationTimer > 2 + Math.random() * 2) {
            this.variationTimer = 0;
            this.speedVariation = (Math.random() - 0.5) * 0.1 * (1 - this.skill);
        }
        var maxSpeedForAI = (this.targetSpeed + this.speedVariation) * VEHICLE_PHYSICS.MAX_SPEED;
        var accelerate;
        if (vehicle.speed < maxSpeedForAI * 0.95) {
            accelerate = 1;
        }
        else if (vehicle.speed > maxSpeedForAI * 1.05) {
            accelerate = -0.3;
        }
        else {
            accelerate = 0;
        }
        if (vehicle.isCrashed) {
            accelerate = 0.3;
        }
        var currentX = vehicle.playerX || 0;
        var targetX = this.preferredLine;
        var lineDiff = targetX - currentX;
        this.steerAmount = lineDiff * (0.5 + this.skill * 0.5);
        this.steerAmount = clamp(this.steerAmount, -0.8, 0.8);
        var wobble = (Math.random() - 0.5) * 0.1 * (1 - this.skill);
        this.steerAmount += wobble;
        if (currentX < -0.8) {
            this.steerAmount = 0.5;
        }
        else if (currentX > 0.8) {
            this.steerAmount = -0.5;
        }
        var shouldUseItem = false;
        if (vehicle.heldItem !== null && this.itemUseCooldown <= 0) {
            var useChance = 0.05 * dt;
            if (vehicle.racePosition > 2) {
                useChance *= 1.5;
            }
            if (Math.random() < useChance) {
                shouldUseItem = true;
                this.itemUseCooldown = 2 + Math.random() * 3;
            }
        }
        return {
            accelerate: clamp(accelerate, 0, 1),
            steer: this.steerAmount,
            useItem: shouldUseItem
        };
    };
    RacerDriver.prototype.getSkill = function () {
        return this.skill;
    };
    RacerDriver.prototype.getAggression = function () {
        return this._aggression;
    };
    RacerDriver.prototype.getReactionDelay = function () {
        return this._reactionDelay;
    };
    return RacerDriver;
}());
"use strict";
var CAR_COLORS = {
    'yellow': {
        id: 'yellow',
        name: 'Sunshine Yellow',
        body: YELLOW,
        trim: WHITE,
        effectFlash: LIGHTCYAN
    },
    'red': {
        id: 'red',
        name: 'Racing Red',
        body: LIGHTRED,
        trim: WHITE,
        effectFlash: YELLOW
    },
    'blue': {
        id: 'blue',
        name: 'Electric Blue',
        body: LIGHTBLUE,
        trim: LIGHTCYAN,
        effectFlash: YELLOW
    },
    'green': {
        id: 'green',
        name: 'Neon Green',
        body: LIGHTGREEN,
        trim: WHITE,
        effectFlash: LIGHTMAGENTA
    },
    'cyan': {
        id: 'cyan',
        name: 'Cyber Cyan',
        body: LIGHTCYAN,
        trim: WHITE,
        effectFlash: LIGHTMAGENTA
    },
    'magenta': {
        id: 'magenta',
        name: 'Synthwave Pink',
        body: LIGHTMAGENTA,
        trim: WHITE,
        effectFlash: LIGHTCYAN
    },
    'white': {
        id: 'white',
        name: 'Ghost White',
        body: WHITE,
        trim: LIGHTCYAN,
        effectFlash: YELLOW
    },
    'orange': {
        id: 'orange',
        name: 'Sunset Orange',
        body: YELLOW,
        trim: LIGHTRED,
        effectFlash: LIGHTCYAN
    }
};
var CAR_CATALOG = [
    {
        id: 'sports',
        name: 'TURBO GT',
        description: 'Balanced performance for all tracks',
        bodyStyle: 'sports',
        stats: {
            topSpeed: 1.0,
            acceleration: 1.0,
            handling: 1.0
        },
        availableColors: ['yellow', 'red', 'blue', 'green', 'cyan', 'magenta', 'white'],
        defaultColor: 'yellow',
        brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
        unlocked: true
    },
    {
        id: 'muscle',
        name: 'THUNDER V8',
        description: 'Raw power, slower handling',
        bodyStyle: 'muscle',
        stats: {
            topSpeed: 1.15,
            acceleration: 1.1,
            handling: 0.85
        },
        availableColors: ['red', 'yellow', 'blue', 'white', 'orange'],
        defaultColor: 'red',
        brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
        unlocked: true
    },
    {
        id: 'compact',
        name: 'SWIFT RS',
        description: 'Quick and nimble, lower top speed',
        bodyStyle: 'compact',
        stats: {
            topSpeed: 0.9,
            acceleration: 1.2,
            handling: 1.25
        },
        availableColors: ['cyan', 'green', 'magenta', 'yellow', 'white'],
        defaultColor: 'cyan',
        brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
        unlocked: true
    },
    {
        id: 'super',
        name: 'PHANTOM X',
        description: 'Elite supercar - unlock to drive',
        bodyStyle: 'super',
        stats: {
            topSpeed: 1.2,
            acceleration: 1.15,
            handling: 1.1
        },
        availableColors: ['white', 'red', 'blue', 'magenta'],
        defaultColor: 'white',
        brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
        unlocked: false,
        unlockHint: 'Win a Grand Prix to unlock'
    },
    {
        id: 'classic',
        name: 'RETRO 86',
        description: 'Old school charm, balanced feel',
        bodyStyle: 'classic',
        stats: {
            topSpeed: 0.95,
            acceleration: 1.0,
            handling: 1.05
        },
        availableColors: ['yellow', 'red', 'white', 'green', 'blue'],
        defaultColor: 'yellow',
        brakeLights: [{ x: 0, y: 1 }, { x: 4, y: 1 }],
        unlocked: true
    }
];
function getCarDefinition(carId) {
    for (var i = 0; i < CAR_CATALOG.length; i++) {
        if (CAR_CATALOG[i].id === carId) {
            return CAR_CATALOG[i];
        }
    }
    return null;
}
function getCarColor(colorId) {
    return CAR_COLORS[colorId] || null;
}
function getUnlockedCars() {
    var unlocked = [];
    for (var i = 0; i < CAR_CATALOG.length; i++) {
        if (CAR_CATALOG[i].unlocked) {
            unlocked.push(CAR_CATALOG[i]);
        }
    }
    return unlocked;
}
function getAllCars() {
    return CAR_CATALOG.slice();
}
function unlockCar(carId) {
    for (var i = 0; i < CAR_CATALOG.length; i++) {
        if (CAR_CATALOG[i].id === carId) {
            CAR_CATALOG[i].unlocked = true;
            return true;
        }
    }
    return false;
}
function isCarUnlocked(carId) {
    var car = getCarDefinition(carId);
    return car !== null && car.unlocked;
}
function getEffectFlashColor(colorId) {
    var color = getCarColor(colorId);
    return color ? color.effectFlash : LIGHTCYAN;
}
function applyCarStats(carId) {
    var car = getCarDefinition(carId);
    if (!car)
        return;
    if (typeof VEHICLE_PHYSICS._baseMaxSpeed === 'undefined') {
        VEHICLE_PHYSICS._baseMaxSpeed = VEHICLE_PHYSICS.MAX_SPEED;
        VEHICLE_PHYSICS._baseAccel = VEHICLE_PHYSICS.ACCEL;
        VEHICLE_PHYSICS._baseSteer = VEHICLE_PHYSICS.STEER_RATE;
    }
    VEHICLE_PHYSICS.MAX_SPEED = VEHICLE_PHYSICS._baseMaxSpeed * car.stats.topSpeed;
    VEHICLE_PHYSICS.ACCEL = VEHICLE_PHYSICS._baseAccel * car.stats.acceleration;
    VEHICLE_PHYSICS.STEER_RATE = VEHICLE_PHYSICS._baseSteer * car.stats.handling;
    logInfo('Applied car stats for ' + car.name +
        ': speed=' + VEHICLE_PHYSICS.MAX_SPEED.toFixed(0) +
        ', accel=' + VEHICLE_PHYSICS.ACCEL.toFixed(0) +
        ', steer=' + VEHICLE_PHYSICS.STEER_RATE.toFixed(2));
}
function resetCarStats() {
    if (typeof VEHICLE_PHYSICS._baseMaxSpeed !== 'undefined') {
        VEHICLE_PHYSICS.MAX_SPEED = VEHICLE_PHYSICS._baseMaxSpeed;
        VEHICLE_PHYSICS.ACCEL = VEHICLE_PHYSICS._baseAccel;
        VEHICLE_PHYSICS.STEER_RATE = VEHICLE_PHYSICS._baseSteer;
    }
}
"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var VEHICLE_PHYSICS = {
    MAX_SPEED: 300,
    ACCEL: 150,
    BRAKE: 250,
    DECEL: 20,
    OFFROAD_DECEL: 200,
    STEER_RATE: 2.0,
    STEER_SPEED_FACTOR: 0.3,
    CENTRIFUGAL: 0.6,
    ROAD_HALF_WIDTH: 1.0,
    OFFROAD_LIMIT: 1.8,
    CRASH_TIME: 1.5,
};
var Vehicle = (function (_super) {
    __extends(Vehicle, _super);
    function Vehicle() {
        var _this = _super.call(this) || this;
        _this.speed = 0;
        _this.playerX = 0;
        _this.trackZ = 0;
        _this.x = 0;
        _this.z = 0;
        _this.driver = null;
        _this.lap = 1;
        _this.checkpoint = 0;
        _this.racePosition = 1;
        _this.heldItem = null;
        _this.activeEffects = [];
        _this.color = YELLOW;
        _this.isOffRoad = false;
        _this.isCrashed = false;
        _this.crashTimer = 0;
        _this.flashTimer = 0;
        _this.boostTimer = 0;
        _this.boostMultiplier = 1.0;
        _this.boostMinSpeed = 0;
        _this.isNPC = false;
        _this.isRacer = false;
        _this.npcType = 'sedan';
        _this.npcColorIndex = 0;
        _this.carId = 'sports';
        _this.carColorId = 'yellow';
        return _this;
    }
    Vehicle.prototype.hasEffect = function (type) {
        for (var i = 0; i < this.activeEffects.length; i++) {
            if (this.activeEffects[i].type === type)
                return true;
        }
        return false;
    };
    Vehicle.prototype.addEffect = function (type, duration, sourceId) {
        this.removeEffect(type);
        this.activeEffects.push({ type: type, duration: duration, sourceVehicleId: sourceId });
        if (type === ItemType.LIGHTNING) {
            this.triggerCrash();
            logInfo("Vehicle " + this.id + " struck by lightning - crashed!");
        }
    };
    Vehicle.prototype.removeEffect = function (type) {
        for (var i = this.activeEffects.length - 1; i >= 0; i--) {
            if (this.activeEffects[i].type === type) {
                this.activeEffects.splice(i, 1);
            }
        }
    };
    Vehicle.prototype.updateEffects = function (dt) {
        for (var i = this.activeEffects.length - 1; i >= 0; i--) {
            var effect = this.activeEffects[i];
            if (effect.type === ItemType.LIGHTNING && this.isCrashed) {
                continue;
            }
            effect.duration -= dt;
            if (effect.duration <= 0) {
                var expiredType = effect.type;
                this.activeEffects.splice(i, 1);
                this.onEffectExpired(expiredType);
            }
        }
    };
    Vehicle.prototype.onEffectExpired = function (type) {
        switch (type) {
            case ItemType.STAR:
                this.boostTimer = 0;
                this.boostMultiplier = 1.0;
                this.boostMinSpeed = 0;
                logInfo("Star effect expired");
                break;
            case ItemType.MUSHROOM_GOLDEN:
            case ItemType.BULLET:
                this.boostTimer = 0;
                this.boostMultiplier = 1.0;
                this.boostMinSpeed = 0;
                if (this.heldItem !== null && this.heldItem.type === type) {
                    this.heldItem = null;
                    logInfo(ItemType[type] + " effect expired - item cleared from slot");
                }
                break;
            case ItemType.LIGHTNING:
                logInfo("Lightning effect expired");
                break;
        }
    };
    Vehicle.prototype.updatePhysics = function (road, intent, dt) {
        this.updateEffects(dt);
        if (this.flashTimer > 0) {
            this.flashTimer -= dt;
            if (this.flashTimer < 0)
                this.flashTimer = 0;
        }
        if (this.boostTimer > 0) {
            this.boostTimer -= dt;
            if (this.boostTimer <= 0) {
                this.boostTimer = 0;
                if (!this.hasEffect(ItemType.STAR) &&
                    !this.hasEffect(ItemType.BULLET) &&
                    !this.hasEffect(ItemType.MUSHROOM_GOLDEN)) {
                    this.boostMultiplier = 1.0;
                    this.boostMinSpeed = 0;
                }
            }
        }
        var lightningSlowdown = this.hasEffect(ItemType.LIGHTNING) ? 0.5 : 1.0;
        if (this.isCrashed) {
            this.crashTimer -= dt;
            if (this.crashTimer <= 0) {
                this.isCrashed = false;
                this.crashTimer = 0;
                this.playerX = 0;
                this.flashTimer = 0.5;
            }
            return;
        }
        if (intent.accelerate > 0) {
            this.speed += VEHICLE_PHYSICS.ACCEL * dt;
        }
        else if (intent.accelerate < 0) {
            this.speed -= VEHICLE_PHYSICS.BRAKE * dt;
        }
        else {
            this.speed -= VEHICLE_PHYSICS.DECEL * dt;
        }
        this.isOffRoad = Math.abs(this.playerX) > VEHICLE_PHYSICS.ROAD_HALF_WIDTH;
        if (this.isOffRoad) {
            this.speed -= VEHICLE_PHYSICS.OFFROAD_DECEL * dt;
            if (this.flashTimer <= 0) {
                this.flashTimer = 0.15;
            }
            if (this.speed < 10) {
                this.speed = 0;
                this.playerX = 0;
                this.isOffRoad = false;
                this.flashTimer = 0.5;
            }
        }
        var effectiveMaxSpeed = VEHICLE_PHYSICS.MAX_SPEED * this.boostMultiplier * lightningSlowdown;
        var minSpeed = this.boostMinSpeed > 0 ? this.boostMinSpeed : 0;
        this.speed = clamp(this.speed, minSpeed, effectiveMaxSpeed);
        var speedRatio = this.speed / VEHICLE_PHYSICS.MAX_SPEED;
        var hasBullet = this.hasEffect(ItemType.BULLET);
        if (hasBullet) {
            var autoPilotRate = 3.0;
            if (this.playerX < -0.05) {
                this.playerX += autoPilotRate * dt;
                if (this.playerX > 0)
                    this.playerX = 0;
            }
            else if (this.playerX > 0.05) {
                this.playerX -= autoPilotRate * dt;
                if (this.playerX < 0)
                    this.playerX = 0;
            }
        }
        else {
            if (this.speed >= 5) {
                var steerMult = 1.0 - (speedRatio * VEHICLE_PHYSICS.STEER_SPEED_FACTOR);
                var steerDelta = intent.steer * VEHICLE_PHYSICS.STEER_RATE * steerMult * dt;
                this.playerX += steerDelta;
            }
            var curve = road.getCurvature(this.trackZ);
            var centrifugal = curve * speedRatio * VEHICLE_PHYSICS.CENTRIFUGAL * dt;
            this.playerX += centrifugal;
        }
        var isInvincible = this.hasEffect(ItemType.STAR) || hasBullet;
        if (Math.abs(this.playerX) > VEHICLE_PHYSICS.OFFROAD_LIMIT && !isInvincible) {
            this.triggerCrash();
            return;
        }
        if (isInvincible && Math.abs(this.playerX) > VEHICLE_PHYSICS.ROAD_HALF_WIDTH) {
            this.playerX = clamp(this.playerX, -VEHICLE_PHYSICS.ROAD_HALF_WIDTH, VEHICLE_PHYSICS.ROAD_HALF_WIDTH);
        }
        this.trackZ += this.speed * dt;
        if (this.trackZ >= road.totalLength) {
            this.trackZ = this.trackZ % road.totalLength;
        }
        this.z = this.trackZ;
        this.x = this.playerX * 20;
    };
    Vehicle.prototype.triggerCrash = function () {
        debugLog.warn("CRASH! playerX=" + this.playerX.toFixed(3) + " (limit=" + VEHICLE_PHYSICS.OFFROAD_LIMIT + ")");
        debugLog.info("  trackZ=" + this.trackZ.toFixed(1) + " speed=" + this.speed.toFixed(1));
        debugLog.info("  Entering recovery for " + VEHICLE_PHYSICS.CRASH_TIME + " seconds");
        this.isCrashed = true;
        this.crashTimer = VEHICLE_PHYSICS.CRASH_TIME;
        this.speed = 0;
    };
    return Vehicle;
}(Entity));
"use strict";
var Road = (function () {
    function Road() {
        this.segments = [];
        this.segmentLength = 200;
        this.baseWidth = 2000;
        this.totalLength = 0;
        this.laps = 3;
        this.name = "Unnamed Road";
    }
    Road.prototype.getSegment = function (z) {
        var index = Math.floor(z / this.segmentLength) % this.segments.length;
        if (index < 0)
            index += this.segments.length;
        return this.segments[index];
    };
    Road.prototype.getSegmentIndex = function (z) {
        var index = Math.floor(z / this.segmentLength) % this.segments.length;
        if (index < 0)
            index += this.segments.length;
        return index;
    };
    Road.prototype.getCurvature = function (z) {
        return this.getSegment(z).curve;
    };
    Road.prototype.isOnRoad = function (x, z) {
        var segment = this.getSegment(z);
        var halfWidth = segment.width / 2;
        return Math.abs(x) <= halfWidth ? 1 : 0;
    };
    return Road;
}());
var RoadBuilder = (function () {
    function RoadBuilder() {
        this.road = new Road();
        this.currentZ = 0;
        this.currentCurve = 0;
        this.stripeCounter = 0;
        this.stripeLength = 3;
    }
    RoadBuilder.prototype.name = function (n) {
        this.road.name = n;
        return this;
    };
    RoadBuilder.prototype.laps = function (n) {
        this.road.laps = n;
        return this;
    };
    RoadBuilder.prototype.straight = function (numSegments) {
        for (var i = 0; i < numSegments; i++) {
            this.addSegment(0);
        }
        return this;
    };
    RoadBuilder.prototype.curve = function (numSegments, curvature) {
        for (var i = 0; i < numSegments; i++) {
            this.addSegment(curvature);
        }
        return this;
    };
    RoadBuilder.prototype.easeIn = function (numSegments, targetCurve) {
        var startCurve = this.currentCurve;
        for (var i = 0; i < numSegments; i++) {
            var t = i / numSegments;
            this.addSegment(lerp(startCurve, targetCurve, t));
        }
        this.currentCurve = targetCurve;
        return this;
    };
    RoadBuilder.prototype.easeOut = function (numSegments) {
        var startCurve = this.currentCurve;
        for (var i = 0; i < numSegments; i++) {
            var t = i / numSegments;
            this.addSegment(lerp(startCurve, 0, t));
        }
        this.currentCurve = 0;
        return this;
    };
    RoadBuilder.prototype.addSegment = function (curve) {
        this.road.segments.push({
            z: this.currentZ,
            curve: curve,
            hill: 0,
            width: 1.0,
            stripe: Math.floor(this.stripeCounter / this.stripeLength) % 2,
            spriteLeft: null,
            spriteRight: null
        });
        this.currentZ += this.road.segmentLength;
        this.stripeCounter++;
    };
    RoadBuilder.prototype.build = function () {
        this.road.totalLength = this.currentZ;
        return this.road;
    };
    return RoadBuilder;
}());
function createNeonCoastRoad() {
    return new RoadBuilder()
        .name("Neon Coast")
        .laps(3)
        .straight(30)
        .easeIn(10, 0.4)
        .curve(30, 0.4)
        .easeOut(10)
        .straight(40)
        .easeIn(8, -0.6)
        .curve(25, -0.6)
        .easeOut(8)
        .straight(25)
        .easeIn(6, 0.5)
        .curve(15, 0.5)
        .easeOut(6)
        .easeIn(6, -0.5)
        .curve(15, -0.5)
        .easeOut(6)
        .straight(35)
        .build();
}
"use strict";
var TRACK_THEMES = {
    'synthwave': {
        id: 'synthwave',
        name: 'Synthwave Sunset',
        sky: {
            top: { fg: MAGENTA, bg: BG_BLACK },
            horizon: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            gridColor: { fg: MAGENTA, bg: BG_BLACK }
        },
        sun: {
            color: { fg: YELLOW, bg: BG_RED },
            glowColor: { fg: LIGHTRED, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: CYAN, bg: BG_BLACK },
            stripe: { fg: WHITE, bg: BG_BLACK },
            edge: { fg: LIGHTRED, bg: BG_BLACK },
            grid: { fg: CYAN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: BROWN, bg: BG_BLACK },
            sceneryTypes: ['palm_tree', 'rock', 'grass'],
            sceneryDensity: 0.15
        },
        background: {
            type: 'mountains',
            color: { fg: MAGENTA, bg: BG_BLACK },
            highlightColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
        }
    },
    'midnight_city': {
        id: 'midnight_city',
        name: 'Midnight City',
        sky: {
            top: { fg: BLUE, bg: BG_BLACK },
            horizon: { fg: LIGHTBLUE, bg: BG_BLACK },
            gridColor: { fg: BLUE, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_BLUE },
            glowColor: { fg: LIGHTCYAN, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: YELLOW, bg: BG_BLACK },
            edge: { fg: WHITE, bg: BG_BLACK },
            grid: { fg: DARKGRAY, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: DARKGRAY, bg: BG_BLACK },
            sceneryTypes: ['building', 'streetlight', 'sign'],
            sceneryDensity: 0.2
        },
        background: {
            type: 'city',
            color: { fg: BLUE, bg: BG_BLACK },
            highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        }
    },
    'beach_paradise': {
        id: 'beach_paradise',
        name: 'Beach Paradise',
        sky: {
            top: { fg: LIGHTCYAN, bg: BG_BLACK },
            horizon: { fg: CYAN, bg: BG_BLACK },
            gridColor: { fg: CYAN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: YELLOW, bg: BG_BROWN },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.3
        },
        road: {
            surface: { fg: LIGHTGRAY, bg: BG_BLACK },
            stripe: { fg: WHITE, bg: BG_BLACK },
            edge: { fg: YELLOW, bg: BG_BLACK },
            grid: { fg: DARKGRAY, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: YELLOW, bg: BG_BLACK },
            sceneryTypes: ['palm_tree', 'beach_umbrella', 'wave'],
            sceneryDensity: 0.12
        },
        background: {
            type: 'ocean',
            color: { fg: CYAN, bg: BG_BLACK },
            highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        }
    },
    'forest_night': {
        id: 'forest_night',
        name: 'Forest Night',
        sky: {
            top: { fg: BLACK, bg: BG_BLACK },
            horizon: { fg: DARKGRAY, bg: BG_BLACK },
            gridColor: { fg: DARKGRAY, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_BLACK },
            glowColor: { fg: LIGHTGRAY, bg: BG_BLACK },
            position: 0.7
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: WHITE, bg: BG_BLACK },
            edge: { fg: BROWN, bg: BG_BLACK },
            grid: { fg: DARKGRAY, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: GREEN, bg: BG_BLACK },
            sceneryTypes: ['pine_tree', 'bush', 'rock'],
            sceneryDensity: 0.25
        },
        background: {
            type: 'forest',
            color: { fg: GREEN, bg: BG_BLACK },
            highlightColor: { fg: LIGHTGREEN, bg: BG_BLACK }
        }
    },
    'haunted_hollow': {
        id: 'haunted_hollow',
        name: 'Haunted Hollow',
        sky: {
            top: { fg: BLACK, bg: BG_BLACK },
            horizon: { fg: MAGENTA, bg: BG_BLACK },
            gridColor: { fg: DARKGRAY, bg: BG_BLACK }
        },
        sun: {
            color: { fg: RED, bg: BG_BLACK },
            glowColor: { fg: LIGHTRED, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: LIGHTRED, bg: BG_BLACK },
            edge: { fg: DARKGRAY, bg: BG_BLACK },
            grid: { fg: DARKGRAY, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: DARKGRAY, bg: BG_BLACK },
            sceneryTypes: ['deadtree', 'gravestone', 'pumpkin', 'skull', 'fence', 'candle'],
            sceneryDensity: 0.3
        },
        background: {
            type: 'cemetery',
            color: { fg: BLACK, bg: BG_BLACK },
            highlightColor: { fg: MAGENTA, bg: BG_BLACK }
        }
    },
    'winter_wonderland': {
        id: 'winter_wonderland',
        name: 'Winter Wonderland',
        sky: {
            top: { fg: BLUE, bg: BG_BLACK },
            horizon: { fg: WHITE, bg: BG_BLACK },
            gridColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_LIGHTGRAY },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.3
        },
        road: {
            surface: { fg: LIGHTGRAY, bg: BG_BLACK },
            stripe: { fg: LIGHTRED, bg: BG_BLACK },
            edge: { fg: WHITE, bg: BG_BLACK },
            grid: { fg: LIGHTCYAN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: WHITE, bg: BG_BLACK },
            sceneryTypes: ['snowpine', 'snowman', 'icecrystal', 'candycane', 'snowdrift', 'signpost'],
            sceneryDensity: 0.25
        },
        background: {
            type: 'mountains',
            color: { fg: WHITE, bg: BG_BLACK },
            highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        }
    },
    'cactus_canyon': {
        id: 'cactus_canyon',
        name: 'Cactus Canyon',
        sky: {
            top: { fg: BLUE, bg: BG_BLACK },
            horizon: { fg: YELLOW, bg: BG_BLACK },
            gridColor: { fg: BROWN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: YELLOW, bg: BG_BROWN },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.6
        },
        road: {
            surface: { fg: BROWN, bg: BG_BLACK },
            stripe: { fg: YELLOW, bg: BG_BLACK },
            edge: { fg: BROWN, bg: BG_BLACK },
            grid: { fg: BROWN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: YELLOW, bg: BG_BLACK },
            sceneryTypes: ['saguaro', 'barrel', 'tumbleweed', 'cowskull', 'desertrock', 'westernsign'],
            sceneryDensity: 0.2
        },
        background: {
            type: 'dunes',
            color: { fg: BROWN, bg: BG_BLACK },
            highlightColor: { fg: YELLOW, bg: BG_BLACK }
        }
    },
    'tropical_jungle': {
        id: 'tropical_jungle',
        name: 'Tropical Jungle',
        sky: {
            top: { fg: GREEN, bg: BG_BLACK },
            horizon: { fg: LIGHTGREEN, bg: BG_BLACK },
            gridColor: { fg: GREEN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: YELLOW, bg: BG_GREEN },
            glowColor: { fg: LIGHTGREEN, bg: BG_BLACK },
            position: 0.4
        },
        road: {
            surface: { fg: BROWN, bg: BG_BLACK },
            stripe: { fg: YELLOW, bg: BG_BLACK },
            edge: { fg: GREEN, bg: BG_BLACK },
            grid: { fg: BROWN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: GREEN, bg: BG_BLACK },
            sceneryTypes: ['jungle_tree', 'fern', 'vine', 'flower', 'parrot', 'banana'],
            sceneryDensity: 0.35
        },
        background: {
            type: 'forest',
            color: { fg: GREEN, bg: BG_BLACK },
            highlightColor: { fg: LIGHTGREEN, bg: BG_BLACK }
        }
    },
    'candy_land': {
        id: 'candy_land',
        name: 'Candy Land',
        sky: {
            top: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            horizon: { fg: LIGHTCYAN, bg: BG_BLACK },
            gridColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
        },
        sun: {
            color: { fg: YELLOW, bg: BG_MAGENTA },
            glowColor: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            stripe: { fg: WHITE, bg: BG_BLACK },
            edge: { fg: LIGHTCYAN, bg: BG_BLACK },
            grid: { fg: MAGENTA, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: LIGHTGREEN, bg: BG_BLACK },
            sceneryTypes: ['lollipop', 'candy_cane', 'gummy_bear', 'cupcake', 'ice_cream', 'cotton_candy'],
            sceneryDensity: 0.3
        },
        background: {
            type: 'mountains',
            color: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        }
    },
    'rainbow_road': {
        id: 'rainbow_road',
        name: 'Rainbow Road',
        sky: {
            top: { fg: BLUE, bg: BG_BLACK },
            horizon: { fg: LIGHTBLUE, bg: BG_BLACK },
            gridColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_BLUE },
            glowColor: { fg: LIGHTCYAN, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: LIGHTRED, bg: BG_BLACK },
            stripe: { fg: YELLOW, bg: BG_BLACK },
            edge: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            grid: { fg: LIGHTCYAN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: BLUE, bg: BG_BLACK },
            sceneryTypes: ['star', 'moon', 'planet', 'comet', 'nebula', 'satellite'],
            sceneryDensity: 0.2
        },
        background: {
            type: 'space',
            color: { fg: BLUE, bg: BG_BLACK },
            highlightColor: { fg: LIGHTMAGENTA, bg: BG_BLACK }
        }
    },
    'dark_castle': {
        id: 'dark_castle',
        name: 'Dark Castle',
        sky: {
            top: { fg: BLACK, bg: BG_BLACK },
            horizon: { fg: DARKGRAY, bg: BG_BLACK },
            gridColor: { fg: DARKGRAY, bg: BG_BLACK }
        },
        sun: {
            color: { fg: LIGHTGRAY, bg: BG_BLACK },
            glowColor: { fg: DARKGRAY, bg: BG_BLACK },
            position: 0.7
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: LIGHTGRAY, bg: BG_BLACK },
            edge: { fg: BROWN, bg: BG_BLACK },
            grid: { fg: DARKGRAY, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: DARKGRAY, bg: BG_BLACK },
            sceneryTypes: ['tower', 'battlement', 'torch', 'banner', 'gargoyle', 'portcullis'],
            sceneryDensity: 0.25
        },
        background: {
            type: 'castle',
            color: { fg: DARKGRAY, bg: BG_BLACK },
            highlightColor: { fg: LIGHTGRAY, bg: BG_BLACK }
        }
    },
    'villains_lair': {
        id: 'villains_lair',
        name: "Villain's Lair",
        sky: {
            top: { fg: RED, bg: BG_BLACK },
            horizon: { fg: LIGHTRED, bg: BG_BLACK },
            gridColor: { fg: RED, bg: BG_BLACK }
        },
        sun: {
            color: { fg: LIGHTRED, bg: BG_RED },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: LIGHTRED, bg: BG_BLACK },
            edge: { fg: RED, bg: BG_BLACK },
            grid: { fg: RED, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: RED, bg: BG_BLACK },
            sceneryTypes: ['lava_rock', 'flame', 'skull_pile', 'chain', 'spike', 'cauldron'],
            sceneryDensity: 0.28
        },
        background: {
            type: 'volcano',
            color: { fg: RED, bg: BG_BLACK },
            highlightColor: { fg: YELLOW, bg: BG_BLACK }
        }
    },
    'ancient_ruins': {
        id: 'ancient_ruins',
        name: 'Ancient Ruins',
        sky: {
            top: { fg: CYAN, bg: BG_BLACK },
            horizon: { fg: YELLOW, bg: BG_BLACK },
            gridColor: { fg: BROWN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: YELLOW, bg: BG_BROWN },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.4
        },
        road: {
            surface: { fg: BROWN, bg: BG_BLACK },
            stripe: { fg: YELLOW, bg: BG_BLACK },
            edge: { fg: LIGHTGRAY, bg: BG_BLACK },
            grid: { fg: BROWN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: YELLOW, bg: BG_BLACK },
            sceneryTypes: ['column', 'statue', 'obelisk', 'sphinx', 'hieroglyph', 'scarab'],
            sceneryDensity: 0.22
        },
        background: {
            type: 'pyramids',
            color: { fg: YELLOW, bg: BG_BLACK },
            highlightColor: { fg: BROWN, bg: BG_BLACK }
        }
    },
    'thunder_stadium': {
        id: 'thunder_stadium',
        name: 'Thunder Stadium',
        sky: {
            top: { fg: BLACK, bg: BG_BLACK },
            horizon: { fg: DARKGRAY, bg: BG_BLACK },
            gridColor: { fg: YELLOW, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_BLACK },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: BROWN, bg: BG_BLACK },
            stripe: { fg: WHITE, bg: BG_BLACK },
            edge: { fg: YELLOW, bg: BG_BLACK },
            grid: { fg: BROWN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: BROWN, bg: BG_BLACK },
            sceneryTypes: ['grandstand', 'tire_stack', 'hay_bale', 'flag_marshal', 'pit_crew', 'banner'],
            sceneryDensity: 0.35
        },
        background: {
            type: 'stadium',
            color: { fg: DARKGRAY, bg: BG_BLACK },
            highlightColor: { fg: YELLOW, bg: BG_BLACK }
        }
    },
    'glitch_circuit': {
        id: 'glitch_circuit',
        name: 'Glitch Circuit',
        sky: {
            top: { fg: BLACK, bg: BG_BLACK },
            horizon: { fg: LIGHTGREEN, bg: BG_BLACK },
            gridColor: { fg: GREEN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_GREEN },
            glowColor: { fg: LIGHTGREEN, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: LIGHTGREEN, bg: BG_BLACK },
            edge: { fg: LIGHTCYAN, bg: BG_BLACK },
            grid: { fg: GREEN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: GREEN, bg: BG_BLACK },
            sceneryTypes: ['building', 'palm_tree', 'tree', 'gravestone', 'cactus', 'lollipop', 'crystal', 'torch', 'column'],
            sceneryDensity: 0.30
        },
        background: {
            type: 'mountains',
            color: { fg: GREEN, bg: BG_BLACK },
            highlightColor: { fg: LIGHTGREEN, bg: BG_BLACK }
        }
    },
    'kaiju_rampage': {
        id: 'kaiju_rampage',
        name: 'Kaiju Rampage',
        sky: {
            top: { fg: DARKGRAY, bg: BG_BLACK },
            horizon: { fg: RED, bg: BG_BLACK },
            gridColor: { fg: BROWN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: LIGHTRED, bg: BG_RED },
            glowColor: { fg: YELLOW, bg: BG_BLACK },
            position: 0.3
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: YELLOW, bg: BG_BLACK },
            edge: { fg: LIGHTRED, bg: BG_BLACK },
            grid: { fg: BROWN, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: BROWN, bg: BG_BLACK },
            sceneryTypes: ['rubble', 'wrecked_car', 'fire', 'monster_footprint', 'fallen_building', 'tank'],
            sceneryDensity: 0.35
        },
        background: {
            type: 'destroyed_city',
            color: { fg: DARKGRAY, bg: BG_BLACK },
            highlightColor: { fg: LIGHTRED, bg: BG_BLACK }
        }
    },
    'ansi_tunnel': {
        id: 'ansi_tunnel',
        name: 'ANSI Tunnel',
        sky: {
            top: { fg: BLACK, bg: BG_BLACK },
            horizon: { fg: CYAN, bg: BG_BLACK },
            gridColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        },
        sun: {
            color: { fg: WHITE, bg: BG_BLACK },
            glowColor: { fg: CYAN, bg: BG_BLACK },
            position: 0.5
        },
        road: {
            surface: { fg: DARKGRAY, bg: BG_BLACK },
            stripe: { fg: LIGHTCYAN, bg: BG_BLACK },
            edge: { fg: CYAN, bg: BG_BLACK },
            grid: { fg: DARKGRAY, bg: BG_BLACK }
        },
        offroad: {
            groundColor: { fg: BLACK, bg: BG_BLACK },
            sceneryTypes: ['data_beacon', 'data_node', 'signal_pole', 'binary_pillar'],
            sceneryDensity: 0.15
        },
        background: {
            type: 'ansi',
            color: { fg: CYAN, bg: BG_BLACK },
            highlightColor: { fg: LIGHTCYAN, bg: BG_BLACK }
        }
    }
};
var TRACK_CATALOG = [
    {
        id: 'sunset_beach',
        name: 'Sunset Beach',
        description: 'Gentle coastal cruise - perfect for beginners',
        difficulty: 1,
        laps: 3,
        themeId: 'beach_paradise',
        estimatedLapTime: 25,
        npcCount: 4,
        sections: [
            { type: 'straight', length: 8 },
            { type: 'ease_in', length: 3, targetCurve: 0.3 },
            { type: 'curve', length: 6, curve: 0.3 },
            { type: 'ease_out', length: 3 },
            { type: 'straight', length: 5 }
        ]
    },
    {
        id: 'sugar_rush',
        name: 'Sugar Rush',
        description: 'Sweet sprint through candy land!',
        difficulty: 2,
        laps: 3,
        themeId: 'candy_land',
        estimatedLapTime: 28,
        npcCount: 4,
        sections: [
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: 0.4 },
            { type: 'curve', length: 5, curve: 0.4 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.35 },
            { type: 'curve', length: 5, curve: -0.35 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'thunder_stadium',
        name: 'Thunder Stadium',
        description: 'Oval speedway under the lights',
        difficulty: 1,
        laps: 4,
        themeId: 'thunder_stadium',
        estimatedLapTime: 22,
        npcCount: 6,
        hidden: true,
        sections: [
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 6, curve: 0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 6 }
        ]
    },
    {
        id: 'data_highway',
        name: 'Data Highway',
        description: 'Race through scrolling ANSI art at 28.8 Kbps!',
        difficulty: 2,
        laps: 3,
        themeId: 'ansi_tunnel',
        estimatedLapTime: 30,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 8 },
            { type: 'ease_in', length: 3, targetCurve: 0.35 },
            { type: 'curve', length: 6, curve: 0.35 },
            { type: 'ease_out', length: 3 },
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: -0.4 },
            { type: 'curve', length: 5, curve: -0.4 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'neon_coast',
        name: 'Neon Coast',
        description: 'Synthwave sunset along the coast',
        difficulty: 2,
        laps: 3,
        themeId: 'synthwave',
        estimatedLapTime: 38,
        npcCount: 6,
        sections: [
            { type: 'straight', length: 8 },
            { type: 'ease_in', length: 3, targetCurve: 0.4 },
            { type: 'curve', length: 8, curve: 0.4 },
            { type: 'ease_out', length: 3 },
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: -0.5 },
            { type: 'curve', length: 6, curve: -0.5 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'downtown_dash',
        name: 'Downtown Dash',
        description: 'Tight corners through city streets',
        difficulty: 3,
        laps: 3,
        themeId: 'midnight_city',
        estimatedLapTime: 42,
        npcCount: 7,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.7 },
            { type: 'curve', length: 5, curve: 0.7 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.75 },
            { type: 'curve', length: 5, curve: -0.75 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 6, curve: 0.5 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'winter_wonderland',
        name: 'Winter Wonderland',
        description: 'Icy roads through a frosty forest',
        difficulty: 2,
        laps: 3,
        themeId: 'winter_wonderland',
        estimatedLapTime: 35,
        npcCount: 4,
        sections: [
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 3, targetCurve: 0.35 },
            { type: 'curve', length: 7, curve: 0.35 },
            { type: 'ease_out', length: 3 },
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: -0.4 },
            { type: 'curve', length: 6, curve: -0.4 },
            { type: 'ease_out', length: 3 }
        ]
    },
    {
        id: 'cactus_canyon',
        name: 'Cactus Canyon',
        description: 'Blazing trails through desert canyons',
        difficulty: 3,
        laps: 3,
        themeId: 'cactus_canyon',
        estimatedLapTime: 40,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 7, curve: 0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.65 },
            { type: 'curve', length: 6, curve: -0.65 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: 0.4 },
            { type: 'curve', length: 4, curve: 0.4 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'twilight_grove',
        name: 'Twilight Grove',
        description: 'Winding paths under dual moons',
        difficulty: 3,
        laps: 3,
        themeId: 'forest_night',
        estimatedLapTime: 38,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: -0.4 },
            { type: 'curve', length: 5, curve: -0.4 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: 0.55 },
            { type: 'curve', length: 6, curve: 0.55 },
            { type: 'ease_out', length: 2 },
            { type: 'ease_in', length: 2, targetCurve: -0.45 },
            { type: 'curve', length: 5, curve: -0.45 },
            { type: 'ease_out', length: 3 }
        ]
    },
    {
        id: 'jungle_run',
        name: 'Jungle Run',
        description: 'Dense rainforest with tight turns',
        difficulty: 3,
        laps: 3,
        themeId: 'tropical_jungle',
        estimatedLapTime: 36,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 6, curve: 0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.55 },
            { type: 'curve', length: 5, curve: -0.55 },
            { type: 'ease_out', length: 2 },
            { type: 'ease_in', length: 2, targetCurve: 0.4 },
            { type: 'curve', length: 4, curve: 0.4 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'haunted_hollow',
        name: 'Haunted Hollow',
        description: 'Spooky cemetery with sharp turns',
        difficulty: 4,
        laps: 3,
        themeId: 'haunted_hollow',
        estimatedLapTime: 44,
        npcCount: 3,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: -0.5 },
            { type: 'curve', length: 5, curve: -0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 3 },
            { type: 'ease_in', length: 2, targetCurve: 0.7 },
            { type: 'curve', length: 6, curve: 0.7 },
            { type: 'ease_out', length: 2 },
            { type: 'ease_in', length: 2, targetCurve: -0.6 },
            { type: 'curve', length: 4, curve: -0.6 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 4, curve: 0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 3 }
        ]
    },
    {
        id: 'fortress_rally',
        name: 'Fortress Rally',
        description: 'Medieval castle walls and courtyards',
        difficulty: 4,
        laps: 3,
        themeId: 'dark_castle',
        estimatedLapTime: 45,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.45 },
            { type: 'curve', length: 6, curve: 0.45 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.7 },
            { type: 'curve', length: 5, curve: -0.7 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 3 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 5, curve: 0.5 },
            { type: 'ease_in', length: 2, targetCurve: -0.45 },
            { type: 'curve', length: 5, curve: -0.45 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'pharaohs_tomb',
        name: "Pharaoh's Tomb",
        description: 'Ancient pyramid mysteries',
        difficulty: 3,
        laps: 3,
        themeId: 'ancient_ruins',
        estimatedLapTime: 40,
        npcCount: 5,
        hidden: true,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.45 },
            { type: 'curve', length: 6, curve: 0.45 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.55 },
            { type: 'curve', length: 5, curve: -0.55 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: 0.4 },
            { type: 'curve', length: 4, curve: 0.4 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'mermaid_lagoon',
        name: 'Mermaid Lagoon',
        description: 'Race through a magical underwater grotto',
        difficulty: 3,
        laps: 3,
        themeId: 'underwater_grotto',
        estimatedLapTime: 42,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.4 },
            { type: 'curve', length: 5, curve: 0.4 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.5 },
            { type: 'curve', length: 6, curve: -0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 3 },
            { type: 'ease_in', length: 2, targetCurve: 0.45 },
            { type: 'curve', length: 5, curve: 0.45 },
            { type: 'ease_in', length: 2, targetCurve: -0.35 },
            { type: 'curve', length: 4, curve: -0.35 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'inferno_speedway',
        name: 'Inferno Speedway',
        description: 'Volcanic villain lair at your peril',
        difficulty: 5,
        laps: 3,
        themeId: 'villains_lair',
        estimatedLapTime: 48,
        npcCount: 6,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.6 },
            { type: 'curve', length: 7, curve: 0.6 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.7 },
            { type: 'curve', length: 6, curve: -0.7 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 3 },
            { type: 'ease_in', length: 2, targetCurve: 0.55 },
            { type: 'curve', length: 5, curve: 0.55 },
            { type: 'ease_in', length: 2, targetCurve: -0.5 },
            { type: 'curve', length: 5, curve: -0.5 },
            { type: 'ease_out', length: 2 }
        ]
    },
    {
        id: 'kaiju_rampage',
        name: 'Kaiju Rampage',
        description: 'Flee the monster through a crumbling city!',
        difficulty: 5,
        laps: 3,
        themeId: 'kaiju_rampage',
        estimatedLapTime: 50,
        npcCount: 5,
        sections: [
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: 0.75 },
            { type: 'curve', length: 5, curve: 0.75 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: -0.6 },
            { type: 'curve', length: 4, curve: -0.6 },
            { type: 'ease_in', length: 2, targetCurve: 0.55 },
            { type: 'curve', length: 4, curve: 0.55 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: -0.65 },
            { type: 'curve', length: 6, curve: -0.65 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 }
        ]
    },
    {
        id: 'celestial_circuit',
        name: 'Celestial Circuit',
        description: 'Epic cosmic journey through the stars',
        difficulty: 4,
        laps: 3,
        themeId: 'rainbow_road',
        estimatedLapTime: 70,
        npcCount: 7,
        sections: [
            { type: 'straight', length: 8 },
            { type: 'ease_in', length: 3, targetCurve: 0.5 },
            { type: 'curve', length: 10, curve: 0.5 },
            { type: 'ease_out', length: 3 },
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: -0.65 },
            { type: 'curve', length: 6, curve: -0.65 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.55 },
            { type: 'curve', length: 6, curve: 0.55 },
            { type: 'ease_in', length: 2, targetCurve: -0.55 },
            { type: 'curve', length: 6, curve: -0.55 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 7 }
        ]
    },
    {
        id: 'glitch_circuit',
        name: 'Glitch Circuit',
        description: 'Reality is corrupted. The simulation breaks.',
        difficulty: 5,
        laps: 3,
        themeId: 'glitch_circuit',
        estimatedLapTime: 75,
        npcCount: 6,
        sections: [
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 2, targetCurve: 0.6 },
            { type: 'curve', length: 5, curve: 0.6 },
            { type: 'ease_in', length: 2, targetCurve: -0.7 },
            { type: 'curve', length: 6, curve: -0.7 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 4 },
            { type: 'ease_in', length: 2, targetCurve: 0.55 },
            { type: 'curve', length: 4, curve: 0.55 },
            { type: 'ease_in', length: 2, targetCurve: -0.5 },
            { type: 'curve', length: 4, curve: -0.5 },
            { type: 'ease_in', length: 2, targetCurve: 0.45 },
            { type: 'curve', length: 4, curve: 0.45 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 6 },
            { type: 'ease_in', length: 3, targetCurve: -0.6 },
            { type: 'curve', length: 10, curve: -0.6 },
            { type: 'ease_out', length: 3 },
            { type: 'straight', length: 6 }
        ]
    },
    {
        id: 'test_oval',
        name: 'Test Oval',
        description: 'Simple oval for testing',
        difficulty: 1,
        laps: 2,
        themeId: 'synthwave',
        estimatedLapTime: 20,
        npcCount: 3,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.5 },
            { type: 'curve', length: 6, curve: 0.5 },
            { type: 'ease_out', length: 2 },
            { type: 'straight', length: 5 }
        ]
    },
    {
        id: 'quick_test',
        name: 'Quick Test',
        description: 'Ultra-short for quick tests',
        difficulty: 1,
        laps: 2,
        themeId: 'synthwave',
        estimatedLapTime: 12,
        npcCount: 2,
        sections: [
            { type: 'straight', length: 5 },
            { type: 'ease_in', length: 2, targetCurve: 0.4 },
            { type: 'curve', length: 4, curve: 0.4 },
            { type: 'ease_out', length: 2 }
        ]
    }
];
function buildRoadFromDefinition(def) {
    var builder = new RoadBuilder()
        .name(def.name)
        .laps(def.laps);
    for (var i = 0; i < def.sections.length; i++) {
        var section = def.sections[i];
        switch (section.type) {
            case 'straight':
                builder.straight(section.length);
                break;
            case 'curve':
                builder.curve(section.length, section.curve || 0);
                break;
            case 'ease_in':
                builder.easeIn(section.length, section.targetCurve || 0);
                break;
            case 'ease_out':
                builder.easeOut(section.length);
                break;
            case 's_curve':
                var halfLen = Math.floor(section.length / 6);
                builder
                    .easeIn(halfLen, 0.5)
                    .curve(halfLen * 2, 0.5)
                    .easeOut(halfLen)
                    .easeIn(halfLen, -0.5)
                    .curve(halfLen * 2, -0.5)
                    .easeOut(halfLen);
                break;
        }
    }
    return builder.build();
}
function getTrackDefinition(id) {
    for (var i = 0; i < TRACK_CATALOG.length; i++) {
        if (TRACK_CATALOG[i].id === id) {
            return TRACK_CATALOG[i];
        }
    }
    return null;
}
function getTrackTheme(trackDef) {
    return TRACK_THEMES[trackDef.themeId] || TRACK_THEMES['synthwave'];
}
function getAllTracks() {
    return TRACK_CATALOG;
}
function renderDifficultyStars(difficulty) {
    var stars = '';
    for (var i = 0; i < 5; i++) {
        stars += i < difficulty ? '*' : '.';
    }
    return stars;
}
"use strict";
var Track = (function () {
    function Track() {
        this.name = "Unnamed Track";
        this.centerline = [];
        this.width = 40;
        this.length = 0;
        this.checkpoints = [];
        this.spawnPoints = [];
        this.laps = 3;
        this.scenery = {
            sky: {
                type: 'sunset',
                sunAzimuth: 270,
                horizonColors: ['#ff00ff', '#ff6600', '#ffff00']
            },
            props: {
                palmTrees: 0.5,
                buildings: 0.3,
                billboards: 0.2
            },
            road: {
                color: '#333333',
                stripeColor: '#ffffff',
                stripeWidth: 2
            },
            skyline: {
                style: 'city',
                density: 0.5
            }
        };
    }
    Track.prototype.getCenterlineX = function (z) {
        if (this.centerline.length < 2)
            return 0;
        var wrappedZ = z % this.length;
        if (wrappedZ < 0)
            wrappedZ += this.length;
        var segmentLength = this.length / this.centerline.length;
        var segmentIndex = Math.floor(wrappedZ / segmentLength);
        var t = (wrappedZ % segmentLength) / segmentLength;
        var p1 = this.centerline[segmentIndex];
        var p2 = this.centerline[(segmentIndex + 1) % this.centerline.length];
        return lerp(p1.x, p2.x, t);
    };
    Track.prototype.getDirection = function (z) {
        if (this.centerline.length < 2)
            return 0;
        var segmentLength = this.length / this.centerline.length;
        var segmentIndex = Math.floor((z % this.length) / segmentLength);
        var p1 = this.centerline[segmentIndex];
        var p2 = this.centerline[(segmentIndex + 1) % this.centerline.length];
        return Math.atan2(p2.y - p1.y, p2.x - p1.x);
    };
    return Track;
}());
"use strict";
var TrackLoader = (function () {
    function TrackLoader() {
    }
    TrackLoader.prototype.load = function (trackId) {
        logInfo("TrackLoader: Loading track '" + trackId + "'");
        if (trackId === "neon_coast_01") {
            return this.loadNeonCoast();
        }
        logWarning("TrackLoader: Unknown track '" + trackId + "', using default");
        return this.loadNeonCoast();
    };
    TrackLoader.prototype.loadNeonCoast = function () {
        var track = new Track();
        track.name = "Neon Coast";
        track.width = 40;
        track.laps = 3;
        track.centerline = [
            { x: 0, y: 0 },
            { x: 100, y: 0 },
            { x: 200, y: 50 },
            { x: 250, y: 150 },
            { x: 200, y: 250 },
            { x: 100, y: 300 },
            { x: 0, y: 300 },
            { x: -100, y: 250 },
            { x: -150, y: 150 },
            { x: -100, y: 50 }
        ];
        track.length = this.calculateTrackLength(track.centerline);
        track.checkpoints = [
            { z: 0 },
            { z: track.length * 0.25 },
            { z: track.length * 0.5 },
            { z: track.length * 0.75 }
        ];
        track.spawnPoints = [
            { x: -5, z: -10 },
            { x: 5, z: -10 },
            { x: -5, z: -25 },
            { x: 5, z: -25 },
            { x: -5, z: -40 },
            { x: 5, z: -40 },
            { x: -5, z: -55 },
            { x: 5, z: -55 }
        ];
        track.scenery = {
            sky: {
                type: 'sunset',
                sunAzimuth: 270,
                horizonColors: ['#ff00ff', '#ff6600', '#ffff00']
            },
            props: {
                palmTrees: 0.7,
                buildings: 0.3,
                billboards: 0.2
            },
            road: {
                color: '#333333',
                stripeColor: '#ffffff',
                stripeWidth: 2
            },
            skyline: {
                style: 'city',
                density: 0.6
            }
        };
        return track;
    };
    TrackLoader.prototype.calculateTrackLength = function (centerline) {
        var length = 0;
        for (var i = 0; i < centerline.length; i++) {
            var p1 = centerline[i];
            var p2 = centerline[(i + 1) % centerline.length];
            length += distance(p1, p2);
        }
        return length;
    };
    TrackLoader.prototype.parseTrackJson = function (data) {
        var _a, _b, _c, _d, _e, _f, _g;
        var track = new Track();
        track.name = ((_a = data.meta) === null || _a === void 0 ? void 0 : _a.name) || "Unknown";
        track.laps = ((_b = data.race) === null || _b === void 0 ? void 0 : _b.laps) || 3;
        track.width = ((_c = data.geometry) === null || _c === void 0 ? void 0 : _c.width) || 40;
        track.centerline = (((_d = data.geometry) === null || _d === void 0 ? void 0 : _d.centerline) || []).map(function (p) {
            return { x: p[0], y: p[1] };
        });
        track.length = ((_e = data.geometry) === null || _e === void 0 ? void 0 : _e.length) || this.calculateTrackLength(track.centerline);
        track.checkpoints = (((_f = data.race) === null || _f === void 0 ? void 0 : _f.checkpoints) || []).map(function (z) {
            return { z: z };
        });
        track.spawnPoints = ((_g = data.race) === null || _g === void 0 ? void 0 : _g.spawnPoints) || [];
        if (data.scenery) {
            track.scenery = data.scenery;
        }
        return track;
    };
    return TrackLoader;
}());
"use strict";
var CheckpointTracker = (function () {
    function CheckpointTracker(track) {
        this.track = track;
        this.vehicleCheckpoints = {};
    }
    CheckpointTracker.prototype.initVehicle = function (vehicleId) {
        this.vehicleCheckpoints[vehicleId] = 0;
    };
    CheckpointTracker.prototype.checkProgress = function (vehicle) {
        var currentCheckpoint = this.vehicleCheckpoints[vehicle.id] || 0;
        var nextCheckpoint = this.track.checkpoints[currentCheckpoint];
        if (!nextCheckpoint) {
            return false;
        }
        var wrappedZ = vehicle.z % this.track.length;
        if (wrappedZ >= nextCheckpoint.z) {
            this.vehicleCheckpoints[vehicle.id] = currentCheckpoint + 1;
            vehicle.checkpoint = currentCheckpoint + 1;
            if (currentCheckpoint + 1 >= this.track.checkpoints.length) {
                this.vehicleCheckpoints[vehicle.id] = 0;
                return true;
            }
        }
        return false;
    };
    CheckpointTracker.prototype.getCurrentCheckpoint = function (vehicleId) {
        return this.vehicleCheckpoints[vehicleId] || 0;
    };
    CheckpointTracker.prototype.reset = function () {
        this.vehicleCheckpoints = {};
    };
    return CheckpointTracker;
}());
"use strict";
var SpawnPointManager = (function () {
    function SpawnPointManager(track) {
        this.track = track;
    }
    SpawnPointManager.prototype.getSpawnPosition = function (gridPosition) {
        if (gridPosition < this.track.spawnPoints.length) {
            var sp = this.track.spawnPoints[gridPosition];
            return { x: sp.x, z: sp.z };
        }
        var row = Math.floor(gridPosition / 2);
        var col = gridPosition % 2;
        return {
            x: col === 0 ? -5 : 5,
            z: -(row + 1) * 15
        };
    };
    SpawnPointManager.prototype.placeVehicle = function (vehicle, gridPosition) {
        var pos = this.getSpawnPosition(gridPosition);
        vehicle.x = pos.x;
        vehicle.z = pos.z;
        vehicle.rotation = 0;
        vehicle.speed = 0;
    };
    SpawnPointManager.prototype.placeVehicles = function (vehicles) {
        for (var i = 0; i < vehicles.length; i++) {
            this.placeVehicle(vehicles[i], i);
        }
    };
    return SpawnPointManager;
}());
"use strict";
var Kinematics = (function () {
    function Kinematics() {
    }
    Kinematics.update = function (entity, dt) {
        entity.x += Math.sin(entity.rotation) * entity.speed * dt;
        entity.z += Math.cos(entity.rotation) * entity.speed * dt;
    };
    Kinematics.applyFriction = function (entity, friction, dt) {
        if (entity.speed > 0) {
            entity.speed = Math.max(0, entity.speed - friction * dt);
        }
    };
    Kinematics.applyAcceleration = function (entity, accel, maxSpeed, dt) {
        entity.speed += accel * dt;
        entity.speed = clamp(entity.speed, 0, maxSpeed);
    };
    return Kinematics;
}());
"use strict";
var Steering = (function () {
    function Steering() {
    }
    Steering.apply = function (vehicle, input, steerSpeed, dt) {
        if (Math.abs(input) < 0.01)
            return;
        if (vehicle.speed < 1)
            return;
        var speedFactor = 1 - (vehicle.speed / VEHICLE_PHYSICS.MAX_SPEED) * 0.5;
        var steerAmount = input * steerSpeed * speedFactor * dt;
        vehicle.rotation += steerAmount;
        vehicle.rotation = wrapAngle(vehicle.rotation);
    };
    Steering.applyLateral = function (vehicle, input, lateralSpeed, dt) {
        if (Math.abs(input) < 0.01)
            return;
        if (vehicle.speed < 1)
            return;
        var speedFactor = vehicle.speed / VEHICLE_PHYSICS.MAX_SPEED;
        vehicle.x += input * lateralSpeed * speedFactor * dt;
    };
    return Steering;
}());
"use strict";
var Collision = (function () {
    function Collision() {
    }
    Collision.aabbOverlap = function (a, b) {
        return Math.abs(a.x - b.x) < (a.halfWidth + b.halfWidth) &&
            Math.abs(a.z - b.z) < (a.halfLength + b.halfLength);
    };
    Collision.vehicleToAABB = function (v) {
        return {
            x: v.x,
            z: v.z,
            halfWidth: 4,
            halfLength: 6
        };
    };
    Collision.isOffTrack = function (vehicle, track) {
        var centerX = track.getCenterlineX(vehicle.z);
        var lateralDist = Math.abs(vehicle.x - centerX);
        return lateralDist > track.width / 2;
    };
    Collision.resolveBoundary = function (vehicle, track) {
        var centerX = track.getCenterlineX(vehicle.z);
        var halfWidth = track.width / 2;
        var lateralDist = vehicle.x - centerX;
        if (Math.abs(lateralDist) > halfWidth) {
            var dir = lateralDist > 0 ? 1 : -1;
            vehicle.x = centerX + dir * halfWidth * 0.95;
            vehicle.speed *= 0.8;
        }
    };
    Collision.processCollisions = function (vehicles, track) {
        for (var i = 0; i < vehicles.length; i++) {
            this.resolveBoundary(vehicles[i], track);
        }
    };
    Collision.processVehicleCollisions = function (vehicles) {
        for (var i = 0; i < vehicles.length; i++) {
            for (var j = i + 1; j < vehicles.length; j++) {
                var a = vehicles[i];
                var b = vehicles[j];
                if (a.isCrashed || b.isCrashed)
                    continue;
                if (a.isNPC && b.isNPC)
                    continue;
                var aInvincible = this.isInvincible(a);
                var bInvincible = this.isInvincible(b);
                var latDist = Math.abs(a.playerX - b.playerX);
                var longDist = Math.abs(a.trackZ - b.trackZ);
                var collisionLat = 0.4;
                var collisionLong = 10;
                if (latDist < collisionLat && longDist < collisionLong) {
                    if (aInvincible && !bInvincible) {
                        this.applyCollisionDamage(b, a);
                    }
                    else if (bInvincible && !aInvincible) {
                        this.applyCollisionDamage(a, b);
                    }
                    else if (!aInvincible && !bInvincible) {
                        this.resolveVehicleCollision(a, b);
                    }
                }
            }
        }
    };
    Collision.isInvincible = function (vehicle) {
        var v = vehicle;
        if (!v.hasEffect)
            return false;
        return v.hasEffect(ItemType.STAR) || v.hasEffect(ItemType.BULLET);
    };
    Collision.applyCollisionDamage = function (victim, _hitter) {
        victim.speed = 0;
        var knockDirection = victim.playerX >= 0 ? 1 : -1;
        victim.playerX = knockDirection * (0.7 + Math.random() * 0.2);
        victim.flashTimer = 1.5;
        logInfo("Invincible collision! Vehicle " + victim.id + " knocked to edge at playerX=" + victim.playerX.toFixed(2) + ", speed=0!");
    };
    Collision.resolveVehicleCollision = function (a, b) {
        var aAhead = a.trackZ > b.trackZ;
        var faster = aAhead ? a : b;
        var slower = aAhead ? b : a;
        var pushForce = 0.15;
        if (a.playerX < b.playerX) {
            a.playerX -= pushForce;
            b.playerX += pushForce;
        }
        else {
            a.playerX += pushForce;
            b.playerX -= pushForce;
        }
        var speedTransfer = 20;
        if (!faster.isNPC && slower.isNPC) {
            faster.speed = Math.max(0, faster.speed - speedTransfer * 1.5);
            slower.speed = Math.min(VEHICLE_PHYSICS.MAX_SPEED, slower.speed + speedTransfer);
            faster.flashTimer = 0.3;
        }
        else if (faster.isNPC && !slower.isNPC) {
            slower.speed = Math.max(0, slower.speed - speedTransfer * 0.5);
            faster.speed = Math.max(0, faster.speed - speedTransfer);
            slower.flashTimer = 0.3;
        }
        else {
            faster.speed = Math.max(0, faster.speed - speedTransfer);
            slower.speed = Math.max(0, slower.speed - speedTransfer * 0.5);
        }
        if (a.flashTimer <= 0)
            a.flashTimer = 0.2;
        if (b.flashTimer <= 0)
            b.flashTimer = 0.2;
    };
    return Collision;
}());
"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var ItemType;
(function (ItemType) {
    ItemType[ItemType["NONE"] = 0] = "NONE";
    ItemType[ItemType["MUSHROOM"] = 1] = "MUSHROOM";
    ItemType[ItemType["GREEN_SHELL"] = 2] = "GREEN_SHELL";
    ItemType[ItemType["RED_SHELL"] = 3] = "RED_SHELL";
    ItemType[ItemType["BLUE_SHELL"] = 4] = "BLUE_SHELL";
    ItemType[ItemType["SHELL"] = 5] = "SHELL";
    ItemType[ItemType["BANANA"] = 6] = "BANANA";
    ItemType[ItemType["MUSHROOM_TRIPLE"] = 7] = "MUSHROOM_TRIPLE";
    ItemType[ItemType["GREEN_SHELL_TRIPLE"] = 8] = "GREEN_SHELL_TRIPLE";
    ItemType[ItemType["RED_SHELL_TRIPLE"] = 9] = "RED_SHELL_TRIPLE";
    ItemType[ItemType["SHELL_TRIPLE"] = 10] = "SHELL_TRIPLE";
    ItemType[ItemType["BANANA_TRIPLE"] = 11] = "BANANA_TRIPLE";
    ItemType[ItemType["MUSHROOM_GOLDEN"] = 12] = "MUSHROOM_GOLDEN";
    ItemType[ItemType["STAR"] = 13] = "STAR";
    ItemType[ItemType["LIGHTNING"] = 14] = "LIGHTNING";
    ItemType[ItemType["BULLET"] = 15] = "BULLET";
})(ItemType || (ItemType = {}));
function getBaseItemType(type) {
    switch (type) {
        case ItemType.MUSHROOM_TRIPLE:
        case ItemType.MUSHROOM_GOLDEN:
            return ItemType.MUSHROOM;
        case ItemType.GREEN_SHELL_TRIPLE:
            return ItemType.GREEN_SHELL;
        case ItemType.RED_SHELL_TRIPLE:
        case ItemType.SHELL_TRIPLE:
        case ItemType.SHELL:
            return ItemType.RED_SHELL;
        case ItemType.BANANA_TRIPLE:
            return ItemType.BANANA;
        case ItemType.BLUE_SHELL:
            return ItemType.BLUE_SHELL;
        default:
            return type;
    }
}
function getItemUses(type) {
    switch (type) {
        case ItemType.MUSHROOM_TRIPLE:
        case ItemType.GREEN_SHELL_TRIPLE:
        case ItemType.RED_SHELL_TRIPLE:
        case ItemType.SHELL_TRIPLE:
        case ItemType.BANANA_TRIPLE:
            return 3;
        default:
            return 1;
    }
}
function isDurationItem(type) {
    switch (type) {
        case ItemType.MUSHROOM_GOLDEN:
        case ItemType.STAR:
        case ItemType.LIGHTNING:
        case ItemType.BULLET:
            return true;
        default:
            return false;
    }
}
function getItemDuration(type) {
    switch (type) {
        case ItemType.MUSHROOM_GOLDEN: return 8.0;
        case ItemType.STAR: return 8.0;
        case ItemType.LIGHTNING: return 5.0;
        case ItemType.BULLET: return 8.0;
        default: return 0;
    }
}
function itemStaysInSlotWhileActive(type) {
    switch (type) {
        case ItemType.MUSHROOM_GOLDEN:
        case ItemType.BULLET:
            return true;
        default:
            return false;
    }
}
var Item = (function (_super) {
    __extends(Item, _super);
    function Item(type) {
        var _this = _super.call(this) || this;
        _this.type = type;
        _this.respawnTime = 2.5;
        _this.respawnCountdown = -1;
        _this.destructionTimer = 0;
        _this.destructionStartTime = 0;
        _this.pickedUpByPlayer = false;
        return _this;
    }
    Item.prototype.isAvailable = function () {
        return this.active && this.respawnCountdown < 0 && this.destructionTimer <= 0;
    };
    Item.prototype.isBeingDestroyed = function () {
        return this.destructionTimer > 0;
    };
    Item.prototype.pickup = function (byPlayer) {
        if (byPlayer === void 0) { byPlayer = false; }
        this.destructionTimer = 0.4;
        this.destructionStartTime = Date.now();
        this.pickedUpByPlayer = byPlayer;
    };
    Item.prototype.updateRespawn = function (dt) {
        if (this.destructionTimer > 0) {
            this.destructionTimer -= dt;
            if (this.destructionTimer <= 0) {
                this.respawnCountdown = this.respawnTime;
                this.destructionTimer = 0;
            }
        }
        else if (this.respawnCountdown >= 0) {
            this.respawnCountdown -= dt;
        }
    };
    return Item;
}(Entity));
"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var Mushroom = (function (_super) {
    __extends(Mushroom, _super);
    function Mushroom() {
        return _super.call(this, ItemType.MUSHROOM) || this;
    }
    Mushroom.applyEffect = function (vehicle) {
        vehicle.boostTimer = Mushroom.BOOST_DURATION;
        vehicle.boostMultiplier = Mushroom.BOOST_MULTIPLIER;
        vehicle.boostMinSpeed = Math.max(vehicle.speed, VEHICLE_PHYSICS.MAX_SPEED * 0.5);
        vehicle.speed = Math.min(vehicle.speed * 1.3, VEHICLE_PHYSICS.MAX_SPEED * Mushroom.BOOST_MULTIPLIER);
        logInfo("Mushroom boost activated! Duration: " + Mushroom.BOOST_DURATION + "s, minSpeed: " + vehicle.boostMinSpeed);
    };
    Mushroom.BOOST_MULTIPLIER = 1.4;
    Mushroom.BOOST_DURATION = 3.0;
    return Mushroom;
}(Item));
"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var ShellType;
(function (ShellType) {
    ShellType[ShellType["GREEN"] = 0] = "GREEN";
    ShellType[ShellType["RED"] = 1] = "RED";
    ShellType[ShellType["BLUE"] = 2] = "BLUE";
})(ShellType || (ShellType = {}));
var SHELL_PHYSICS = {
    MIN_SPEED: 300,
    GREEN_TARGET_SPEED: 400,
    RED_TARGET_SPEED: 550,
    BLUE_TARGET_SPEED: 700,
    ACCELERATION: 200,
    BACKWARD_SPEED: -250
};
var Shell = (function (_super) {
    __extends(Shell, _super);
    function Shell(shellType) {
        var _this = _super.call(this, ItemType.SHELL) || this;
        _this.shellType = shellType;
        _this.trackZ = 0;
        _this.playerX = 0;
        _this.speed = SHELL_PHYSICS.MIN_SPEED;
        _this.targetSpeed = SHELL_PHYSICS.GREEN_TARGET_SPEED;
        _this.ownerId = -1;
        _this.targetId = -1;
        _this.ttl = 10;
        _this.isDestroyed = false;
        _this.isBackward = false;
        return _this;
    }
    Shell.fireGreen = function (vehicle, backward) {
        if (backward === void 0) { backward = false; }
        var shell = new Shell(ShellType.GREEN);
        shell.ownerId = vehicle.id;
        shell.isBackward = backward;
        if (backward) {
            shell.trackZ = vehicle.trackZ - 15;
            shell.playerX = vehicle.playerX;
            shell.speed = SHELL_PHYSICS.BACKWARD_SPEED;
            shell.targetSpeed = SHELL_PHYSICS.BACKWARD_SPEED;
            logInfo("GREEN SHELL fired BACKWARD at speed=" + shell.speed.toFixed(0));
        }
        else {
            shell.trackZ = vehicle.trackZ + 15;
            shell.playerX = vehicle.playerX;
            shell.speed = SHELL_PHYSICS.MIN_SPEED;
            shell.targetSpeed = SHELL_PHYSICS.GREEN_TARGET_SPEED;
            logInfo("GREEN SHELL fired FORWARD, starting at speed=" + shell.speed.toFixed(0) + ", target=" + shell.targetSpeed);
        }
        return shell;
    };
    Shell.fireRed = function (vehicle, vehicles) {
        var shell = new Shell(ShellType.RED);
        shell.trackZ = vehicle.trackZ + 15;
        shell.playerX = vehicle.playerX;
        shell.ownerId = vehicle.id;
        shell.speed = SHELL_PHYSICS.MIN_SPEED;
        shell.targetSpeed = SHELL_PHYSICS.RED_TARGET_SPEED;
        shell.targetId = Shell.findNextVehicleAhead(vehicle, vehicles);
        logInfo("RED SHELL fired, starting at speed=" + shell.speed.toFixed(0) + ", target speed=" + shell.targetSpeed + ", homing to vehicle " + shell.targetId);
        return shell;
    };
    Shell.fireBlue = function (vehicle, vehicles) {
        var shell = new Shell(ShellType.BLUE);
        shell.trackZ = vehicle.trackZ + 15;
        shell.playerX = vehicle.playerX;
        shell.ownerId = vehicle.id;
        shell.speed = SHELL_PHYSICS.MIN_SPEED;
        shell.targetSpeed = SHELL_PHYSICS.BLUE_TARGET_SPEED;
        shell.targetId = Shell.findFirstPlace(vehicles);
        logInfo("BLUE SHELL fired, starting at speed=" + shell.speed.toFixed(0) + ", target speed=" + shell.targetSpeed + ", homing to 1st place (vehicle " + shell.targetId + ")");
        return shell;
    };
    Shell.findNextVehicleAhead = function (shooter, vehicles) {
        var bestId = -1;
        var bestDist = Infinity;
        for (var i = 0; i < vehicles.length; i++) {
            var v = vehicles[i];
            if (v.id === shooter.id)
                continue;
            var dist = v.trackZ - shooter.trackZ;
            if (dist > 0 && dist < bestDist) {
                bestDist = dist;
                bestId = v.id;
            }
        }
        return bestId;
    };
    Shell.findFirstPlace = function (vehicles) {
        for (var i = 0; i < vehicles.length; i++) {
            if (vehicles[i].racePosition === 1) {
                return vehicles[i].id;
            }
        }
        return -1;
    };
    Shell.prototype.update = function (dt, vehicles, roadLength) {
        if (this.isDestroyed)
            return true;
        this.ttl -= dt;
        if (this.ttl <= 0 && this.shellType !== ShellType.BLUE) {
            logInfo("Shell despawned (TTL)");
            return true;
        }
        if (!this.isBackward && this.speed < this.targetSpeed) {
            this.speed += SHELL_PHYSICS.ACCELERATION * dt;
            if (this.speed > this.targetSpeed) {
                this.speed = this.targetSpeed;
            }
        }
        this.trackZ += this.speed * dt;
        if (this.trackZ >= roadLength) {
            this.trackZ = this.trackZ % roadLength;
        }
        else if (this.trackZ < 0) {
            this.trackZ = roadLength + this.trackZ;
        }
        if (!this.isBackward && (this.shellType === ShellType.RED || this.shellType === ShellType.BLUE)) {
            var target = this.findVehicleById(vehicles, this.targetId);
            if (target) {
                var homingRate = 2.0;
                if (this.playerX < target.playerX - 0.05) {
                    this.playerX += homingRate * dt;
                }
                else if (this.playerX > target.playerX + 0.05) {
                    this.playerX -= homingRate * dt;
                }
            }
        }
        for (var i = 0; i < vehicles.length; i++) {
            var v = vehicles[i];
            if (v.id === this.ownerId)
                continue;
            if (v.isCrashed)
                continue;
            var isInvincible = false;
            for (var e = 0; e < v.activeEffects.length; e++) {
                var effectType = v.activeEffects[e].type;
                if (effectType === ItemType.STAR || effectType === ItemType.BULLET) {
                    isInvincible = true;
                    break;
                }
            }
            if (isInvincible)
                continue;
            var latDist = Math.abs(this.playerX - v.playerX);
            var longDist = Math.abs(this.trackZ - v.trackZ);
            if (latDist < 0.5 && longDist < 15) {
                this.applyHitToVehicle(v);
                this.isDestroyed = true;
                return true;
            }
        }
        return false;
    };
    Shell.prototype.findVehicleById = function (vehicles, id) {
        for (var i = 0; i < vehicles.length; i++) {
            if (vehicles[i].id === id)
                return vehicles[i];
        }
        return null;
    };
    Shell.prototype.applyHitToVehicle = function (vehicle) {
        vehicle.speed = 0;
        var knockDirection = vehicle.playerX >= 0 ? 1 : -1;
        vehicle.playerX = knockDirection * (0.7 + Math.random() * 0.2);
        vehicle.flashTimer = 1.5;
        var shellNames = ['GREEN', 'RED', 'BLUE'];
        var direction = this.isBackward ? ' (backward)' : '';
        logInfo(shellNames[this.shellType] + " SHELL" + direction + " hit vehicle " + vehicle.id + " - knocked to edge at playerX=" + vehicle.playerX.toFixed(2) + "!");
    };
    return Shell;
}(Item));
"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var Banana = (function (_super) {
    __extends(Banana, _super);
    function Banana() {
        var _this = _super.call(this, ItemType.BANANA) || this;
        _this.shellType = ShellType.GREEN;
        _this.trackZ = 0;
        _this.playerX = 0;
        _this.speed = 0;
        _this.ownerId = -1;
        _this.targetId = -1;
        _this.ttl = 60;
        _this.isDestroyed = false;
        return _this;
    }
    Banana.drop = function (vehicle) {
        var banana = new Banana();
        banana.trackZ = vehicle.trackZ - 20;
        banana.playerX = vehicle.playerX;
        banana.ownerId = vehicle.id;
        logInfo("BANANA dropped at trackZ=" + banana.trackZ.toFixed(0) + ", playerX=" + banana.playerX.toFixed(2));
        return banana;
    };
    Banana.prototype.update = function (dt, vehicles, roadLength) {
        if (this.isDestroyed)
            return true;
        this.ttl -= dt;
        if (this.ttl <= 0) {
            logInfo("Banana despawned (TTL)");
            return true;
        }
        if (this.trackZ < 0) {
            this.trackZ += roadLength;
        }
        else if (this.trackZ >= roadLength) {
            this.trackZ = this.trackZ % roadLength;
        }
        for (var i = 0; i < vehicles.length; i++) {
            var v = vehicles[i];
            if (v.id === this.ownerId)
                continue;
            if (v.isCrashed)
                continue;
            var isInvincible = false;
            for (var e = 0; e < v.activeEffects.length; e++) {
                var effectType = v.activeEffects[e].type;
                if (effectType === ItemType.STAR || effectType === ItemType.BULLET) {
                    isInvincible = true;
                    break;
                }
            }
            if (isInvincible)
                continue;
            var latDist = Math.abs(this.playerX - v.playerX);
            var longDist = Math.abs(this.trackZ - v.trackZ);
            if (latDist < 0.5 && longDist < 15) {
                this.applyHitToVehicle(v);
                this.isDestroyed = true;
                return true;
            }
        }
        return false;
    };
    Banana.prototype.applyHitToVehicle = function (vehicle) {
        vehicle.speed = 0;
        var knockDirection = vehicle.playerX >= 0 ? 1 : -1;
        vehicle.playerX = knockDirection * (0.7 + Math.random() * 0.2);
        vehicle.flashTimer = 2.0;
        logInfo("BANANA hit vehicle " + vehicle.id + " - spun out to edge at playerX=" + vehicle.playerX.toFixed(2) + "!");
    };
    return Banana;
}(Item));
"use strict";
var DEBUG_FORCE_ITEM = null;
var ItemSystem = (function () {
    function ItemSystem() {
        this.items = [];
        this.projectiles = [];
        this.callbacks = {};
    }
    ItemSystem.prototype.setCallbacks = function (callbacks) {
        this.callbacks = callbacks;
    };
    ItemSystem.prototype.initFromTrack = function (_track, road) {
        this.items = [];
        var trackLength = road.totalLength;
        var numRows = Math.max(2, Math.floor(trackLength / 1200));
        var spacing = trackLength / (numRows + 1);
        var rowPatterns = [
            [-12, 0, 12],
            [-10, 0, 10],
            [-14, -5, 5, 14],
            [-12, -4, 4, 12],
            [-14, -7, 0, 7, 14],
            [-10, 10],
            [-12, 12],
            [-14, -6, 2],
            [-2, 6, 14]
        ];
        for (var rowIndex = 1; rowIndex <= numRows; rowIndex++) {
            var z = spacing * rowIndex;
            var patternIdx = (rowIndex - 1) % rowPatterns.length;
            var pattern = rowPatterns[patternIdx];
            for (var j = 0; j < pattern.length; j++) {
                var x = pattern[j];
                x += (globalRand.next() - 0.5) * 2;
                var item = new Item(ItemType.NONE);
                item.x = x;
                item.z = z;
                item.respawnTime = 2.5;
                this.items.push(item);
            }
        }
        logInfo("ItemSystem: Placed " + this.items.length + " item boxes in " + numRows + " rows across track length " + trackLength);
    };
    ItemSystem.prototype.update = function (dt, vehicles, roadLength) {
        for (var i = 0; i < this.items.length; i++) {
            this.items[i].updateRespawn(dt);
        }
        if (vehicles && roadLength) {
            for (var j = this.projectiles.length - 1; j >= 0; j--) {
                var shell = this.projectiles[j];
                if (shell.update(dt, vehicles, roadLength)) {
                    this.projectiles.splice(j, 1);
                }
            }
        }
    };
    ItemSystem.prototype.getProjectiles = function () {
        return this.projectiles;
    };
    ItemSystem.prototype.checkPickups = function (vehicles, road) {
        for (var i = 0; i < vehicles.length; i++) {
            var vehicle = vehicles[i];
            if (vehicle.heldItem !== null)
                continue;
            for (var j = 0; j < this.items.length; j++) {
                var item = this.items[j];
                if (!item.isAvailable())
                    continue;
                var dz = vehicle.trackZ - item.z;
                var roadLen = road ? road.totalLength : 10000;
                if (dz > roadLen / 2) {
                    dz -= roadLen;
                }
                else if (dz < -roadLen / 2) {
                    dz += roadLen;
                }
                if (dz < -5 || dz > 15)
                    continue;
                var vehicleX = vehicle.x;
                var itemX = item.x;
                if (road) {
                    var seg = road.getSegment(item.z);
                    if (seg && seg.curve !== 0) {
                        var curveShift = seg.curve * Math.max(0, dz) * 0.3;
                        itemX += curveShift;
                    }
                }
                var dx = vehicleX - itemX;
                var lateralRadius = 12;
                if (Math.abs(dx) < lateralRadius) {
                    var isPlayer = !vehicle.isNPC;
                    item.pickup(isPlayer);
                    var itemType = this.randomItemType(vehicle.racePosition, vehicles.length);
                    vehicle.heldItem = {
                        type: itemType,
                        uses: getItemUses(itemType),
                        activated: false
                    };
                    logInfo("Vehicle picked up " + ItemType[itemType] + " (x" + vehicle.heldItem.uses + ")");
                }
            }
        }
    };
    ItemSystem.prototype.useItem = function (vehicle, allVehicles, fireBackward) {
        if (vehicle.heldItem === null)
            return;
        var itemType = vehicle.heldItem.type;
        var consumed = false;
        switch (itemType) {
            case ItemType.MUSHROOM:
            case ItemType.MUSHROOM_TRIPLE:
                Mushroom.applyEffect(vehicle);
                consumed = true;
                break;
            case ItemType.MUSHROOM_GOLDEN:
                if (!vehicle.heldItem.activated) {
                    vehicle.heldItem.activated = true;
                    this.applyDurationEffect(vehicle, itemType);
                    logInfo("Golden Mushroom ACTIVATED - unlimited boosts for duration!");
                }
                Mushroom.applyEffect(vehicle);
                return;
            case ItemType.STAR:
                this.applyDurationEffect(vehicle, itemType);
                vehicle.heldItem = null;
                return;
            case ItemType.LIGHTNING:
                if (allVehicles) {
                    this.applyLightning(vehicle, allVehicles);
                    vehicle.flashTimer = 0.3;
                }
                vehicle.heldItem = null;
                return;
            case ItemType.BULLET:
                if (!vehicle.heldItem.activated) {
                    vehicle.heldItem.activated = true;
                    this.applyDurationEffect(vehicle, itemType);
                    logInfo("Bullet Bill ACTIVATED - autopilot engaged!");
                }
                return;
            case ItemType.GREEN_SHELL:
            case ItemType.GREEN_SHELL_TRIPLE:
                {
                    var greenShell = Shell.fireGreen(vehicle, fireBackward === true);
                    this.projectiles.push(greenShell);
                }
                consumed = true;
                break;
            case ItemType.RED_SHELL:
            case ItemType.RED_SHELL_TRIPLE:
            case ItemType.SHELL:
            case ItemType.SHELL_TRIPLE:
                if (allVehicles) {
                    var redShell = Shell.fireRed(vehicle, allVehicles);
                    this.projectiles.push(redShell);
                }
                consumed = true;
                break;
            case ItemType.BLUE_SHELL:
                if (allVehicles) {
                    var blueShell = Shell.fireBlue(vehicle, allVehicles);
                    this.projectiles.push(blueShell);
                }
                consumed = true;
                break;
            case ItemType.BANANA:
            case ItemType.BANANA_TRIPLE:
                {
                    var banana = Banana.drop(vehicle);
                    this.projectiles.push(banana);
                }
                consumed = true;
                break;
        }
        if (consumed && vehicle.heldItem) {
            vehicle.heldItem.uses--;
            if (vehicle.heldItem.uses <= 0) {
                vehicle.heldItem = null;
            }
        }
    };
    ItemSystem.prototype.applyDurationEffect = function (vehicle, type) {
        var duration = getItemDuration(type);
        vehicle.addEffect(type, duration, vehicle.id);
        vehicle.boostMinSpeed = Math.max(vehicle.speed, VEHICLE_PHYSICS.MAX_SPEED * 0.5);
        switch (type) {
            case ItemType.MUSHROOM_GOLDEN:
                vehicle.boostMultiplier = 1.4;
                vehicle.speed = Math.min(vehicle.speed * 1.2, VEHICLE_PHYSICS.MAX_SPEED * 1.4);
                break;
            case ItemType.STAR:
                vehicle.boostMultiplier = 1.35;
                vehicle.speed = Math.min(vehicle.speed * 1.25, VEHICLE_PHYSICS.MAX_SPEED * 1.35);
                break;
            case ItemType.BULLET:
                vehicle.boostMultiplier = 1.6;
                vehicle.speed = VEHICLE_PHYSICS.MAX_SPEED * 1.6;
                vehicle.boostMinSpeed = VEHICLE_PHYSICS.MAX_SPEED * 1.5;
                break;
        }
        logInfo("Applied " + ItemType[type] + " effect for " + duration + "s, minSpeed: " + vehicle.boostMinSpeed);
    };
    ItemSystem.prototype.applyLightning = function (user, allVehicles) {
        var duration = getItemDuration(ItemType.LIGHTNING);
        var hitCount = 0;
        for (var i = 0; i < allVehicles.length; i++) {
            var v = allVehicles[i];
            if (v.id === user.id)
                continue;
            if (v.racePosition >= user.racePosition)
                continue;
            if (v.hasEffect && (v.hasEffect(ItemType.STAR) ||
                v.hasEffect(ItemType.BULLET))) {
                continue;
            }
            v.addEffect(ItemType.LIGHTNING, duration, user.id);
            hitCount++;
        }
        if (this.callbacks.onLightningStrike) {
            this.callbacks.onLightningStrike(hitCount);
        }
        logInfo("Lightning struck " + hitCount + " opponents ahead (positions 1-" + (user.racePosition - 1) + ")!");
    };
    ItemSystem.prototype.randomItemType = function (position, totalRacers) {
        if (DEBUG_FORCE_ITEM !== null) {
            return DEBUG_FORCE_ITEM;
        }
        var roll = globalRand.next();
        var positionFactor = totalRacers > 1 ? (position - 1) / (totalRacers - 1) : 0;
        if (positionFactor < 0.25) {
            if (roll < 0.40)
                return ItemType.BANANA;
            if (roll < 0.70)
                return ItemType.GREEN_SHELL;
            if (roll < 0.85)
                return ItemType.MUSHROOM;
            if (roll < 0.95)
                return ItemType.RED_SHELL;
            return ItemType.BANANA_TRIPLE;
        }
        else if (positionFactor < 0.50) {
            if (roll < 0.25)
                return ItemType.MUSHROOM;
            if (roll < 0.45)
                return ItemType.GREEN_SHELL;
            if (roll < 0.65)
                return ItemType.RED_SHELL;
            if (roll < 0.80)
                return ItemType.BANANA;
            if (roll < 0.90)
                return ItemType.MUSHROOM_TRIPLE;
            return ItemType.GREEN_SHELL_TRIPLE;
        }
        else if (positionFactor < 0.75) {
            if (roll < 0.20)
                return ItemType.MUSHROOM_TRIPLE;
            if (roll < 0.35)
                return ItemType.RED_SHELL;
            if (roll < 0.50)
                return ItemType.RED_SHELL_TRIPLE;
            if (roll < 0.65)
                return ItemType.MUSHROOM_GOLDEN;
            if (roll < 0.80)
                return ItemType.STAR;
            if (roll < 0.90)
                return ItemType.LIGHTNING;
            return ItemType.MUSHROOM_GOLDEN;
        }
        else {
            if (roll < 0.20)
                return ItemType.MUSHROOM_GOLDEN;
            if (roll < 0.40)
                return ItemType.STAR;
            if (roll < 0.55)
                return ItemType.BULLET;
            if (roll < 0.70)
                return ItemType.LIGHTNING;
            if (roll < 0.80)
                return ItemType.BLUE_SHELL;
            if (roll < 0.90)
                return ItemType.RED_SHELL_TRIPLE;
            return ItemType.MUSHROOM_GOLDEN;
        }
    };
    ItemSystem.prototype.getItemBoxes = function () {
        return this.items;
    };
    return ItemSystem;
}());
"use strict";
var Hud = (function () {
    function Hud() {
        this.startTime = 0;
        this.lapStartTime = 0;
        this.bestLapTime = Infinity;
    }
    Hud.prototype.init = function (currentTime) {
        this.startTime = currentTime;
        this.lapStartTime = currentTime;
        this.bestLapTime = Infinity;
    };
    Hud.prototype.onLapComplete = function (currentTime) {
        var lapTime = currentTime - this.lapStartTime;
        if (lapTime < this.bestLapTime) {
            this.bestLapTime = lapTime;
        }
        this.lapStartTime = currentTime;
    };
    Hud.prototype.compute = function (vehicle, track, road, vehicles, currentTime, countdown, raceMode) {
        var lapProgress = 0;
        if (road.totalLength > 0) {
            lapProgress = (vehicle.trackZ % road.totalLength) / road.totalLength;
            if (lapProgress < 0)
                lapProgress += 1.0;
        }
        var racers = vehicles.filter(function (v) { return !v.isNPC || v.isRacer; });
        var isCountdown = (countdown !== undefined && countdown > 0);
        var lapTime = currentTime - this.lapStartTime;
        var totalTime = currentTime - this.startTime;
        var displayLapTime = isCountdown ? 0 : Math.max(0, lapTime);
        var displayTotalTime = isCountdown ? 0 : Math.max(0, totalTime);
        return {
            speed: Math.round(vehicle.speed),
            speedMax: VEHICLE_PHYSICS.MAX_SPEED,
            lap: vehicle.lap,
            totalLaps: track.laps,
            lapProgress: lapProgress,
            position: vehicle.racePosition,
            totalRacers: racers.length,
            lapTime: displayLapTime,
            bestLapTime: this.bestLapTime === Infinity ? 0 : this.bestLapTime,
            totalTime: displayTotalTime,
            heldItem: vehicle.heldItem,
            raceFinished: vehicle.lap > track.laps,
            countdown: countdown || 0,
            raceMode: raceMode !== undefined ? raceMode : RaceMode.TIME_TRIAL
        };
    };
    Hud.formatTime = function (seconds) {
        var mins = Math.floor(seconds / 60);
        var secs = seconds % 60;
        var secStr = secs < 10 ? "0" + secs.toFixed(2) : secs.toFixed(2);
        return mins + ":" + secStr;
    };
    return Hud;
}());
"use strict";
var Minimap = (function () {
    function Minimap(config) {
        this.config = config;
        this.scaleX = 1;
        this.scaleY = 1;
        this.offsetX = 0;
        this.offsetY = 0;
    }
    Minimap.prototype.initForTrack = function (track) {
        var minX = Infinity, maxX = -Infinity;
        var minY = Infinity, maxY = -Infinity;
        for (var i = 0; i < track.centerline.length; i++) {
            var p = track.centerline[i];
            if (p.x < minX)
                minX = p.x;
            if (p.x > maxX)
                maxX = p.x;
            if (p.y < minY)
                minY = p.y;
            if (p.y > maxY)
                maxY = p.y;
        }
        var trackWidth = maxX - minX;
        var trackHeight = maxY - minY;
        var mapInnerW = this.config.width - 2;
        var mapInnerH = this.config.height - 2;
        this.scaleX = mapInnerW / (trackWidth || 1);
        this.scaleY = mapInnerH / (trackHeight || 1);
        var scale = Math.min(this.scaleX, this.scaleY);
        this.scaleX = scale;
        this.scaleY = scale;
        this.offsetX = -minX;
        this.offsetY = -minY;
    };
    Minimap.prototype.worldToMinimap = function (worldX, worldY) {
        return {
            x: Math.round((worldX + this.offsetX) * this.scaleX) + this.config.x + 1,
            y: Math.round((worldY + this.offsetY) * this.scaleY) + this.config.y + 1
        };
    };
    Minimap.prototype.getVehiclePositions = function (vehicles, track, playerId) {
        var result = [];
        for (var i = 0; i < vehicles.length; i++) {
            var v = vehicles[i];
            var progress = (v.z % track.length) / track.length;
            var centerlineIdx = Math.floor(progress * track.centerline.length);
            var centerPoint = track.centerline[centerlineIdx] || { x: 0, y: 0 };
            var pos = this.worldToMinimap(centerPoint.x + v.x * 0.1, centerPoint.y);
            result.push({
                x: pos.x,
                y: pos.y,
                isPlayer: v.id === playerId,
                color: v.color
            });
        }
        return result;
    };
    Minimap.prototype.getConfig = function () {
        return this.config;
    };
    return Minimap;
}());
"use strict";
var Speedometer = (function () {
    function Speedometer(maxBarLength) {
        this.maxBarLength = maxBarLength;
    }
    Speedometer.prototype.calculate = function (currentSpeed, maxSpeed) {
        var percentage = clamp(currentSpeed / maxSpeed, 0, 1);
        var filledLength = Math.round(percentage * this.maxBarLength);
        return {
            currentSpeed: Math.round(currentSpeed),
            maxSpeed: maxSpeed,
            percentage: percentage,
            barLength: this.maxBarLength,
            filledLength: filledLength
        };
    };
    Speedometer.prototype.generateBar = function (data) {
        var filled = '';
        var empty = '';
        for (var i = 0; i < data.filledLength; i++) {
            filled += '█';
        }
        for (var j = data.filledLength; j < data.barLength; j++) {
            empty += '░';
        }
        return filled + empty;
    };
    return Speedometer;
}());
"use strict";
var LapTimer = (function () {
    function LapTimer() {
        this.lapTimes = [];
        this.lapStartTime = 0;
        this.raceStartTime = 0;
        this.bestLapTime = null;
    }
    LapTimer.prototype.start = function (currentTime) {
        this.raceStartTime = currentTime;
        this.lapStartTime = currentTime;
        this.lapTimes = [];
        this.bestLapTime = null;
    };
    LapTimer.prototype.completeLap = function (currentTime) {
        var lapTime = currentTime - this.lapStartTime;
        this.lapTimes.push(lapTime);
        if (this.bestLapTime === null || lapTime < this.bestLapTime) {
            this.bestLapTime = lapTime;
        }
        this.lapStartTime = currentTime;
        return lapTime;
    };
    LapTimer.prototype.getData = function (currentTime, currentLap, totalLaps) {
        return {
            currentLap: currentLap,
            totalLaps: totalLaps,
            currentLapTime: currentTime - this.lapStartTime,
            bestLapTime: this.bestLapTime,
            lapTimes: this.lapTimes.slice(),
            totalTime: currentTime - this.raceStartTime
        };
    };
    LapTimer.format = function (seconds) {
        if (seconds === null || seconds === undefined)
            return "--:--.--";
        var mins = Math.floor(seconds / 60);
        var secs = Math.floor(seconds % 60);
        var ms = Math.floor((seconds % 1) * 100);
        var secStr = secs < 10 ? "0" + secs : "" + secs;
        var msStr = ms < 10 ? "0" + ms : "" + ms;
        return mins + ":" + secStr + "." + msStr;
    };
    return LapTimer;
}());
"use strict";
var PositionIndicator = (function () {
    function PositionIndicator() {
    }
    PositionIndicator.calculate = function (position, totalRacers) {
        return {
            position: position,
            totalRacers: totalRacers,
            suffix: this.getOrdinalSuffix(position)
        };
    };
    PositionIndicator.getOrdinalSuffix = function (n) {
        var s = ["th", "st", "nd", "rd"];
        var v = n % 100;
        return s[(v - 20) % 10] || s[v] || s[0];
    };
    PositionIndicator.format = function (data) {
        return data.position + data.suffix + " / " + data.totalRacers;
    };
    PositionIndicator.calculatePositions = function (vehicles) {
        var racers = vehicles.filter(function (v) { return !v.isNPC || v.isRacer; });
        var VISUAL_OFFSET = 200;
        var sorted = racers.slice().sort(function (a, b) {
            if (a.lap !== b.lap)
                return b.lap - a.lap;
            var aZ = a.trackZ + (a.isNPC ? 0 : VISUAL_OFFSET);
            var bZ = b.trackZ + (b.isNPC ? 0 : VISUAL_OFFSET);
            return bZ - aZ;
        });
        for (var i = 0; i < sorted.length; i++) {
            sorted[i].racePosition = i + 1;
        }
    };
    return PositionIndicator;
}());
"use strict";
if (typeof JSONdb === 'undefined') {
    load('json-db.js');
}
if (typeof JSONClient === 'undefined') {
    load('json-client.js');
}
var HighScoreType;
(function (HighScoreType) {
    HighScoreType["TRACK_TIME"] = "track_time";
    HighScoreType["LAP_TIME"] = "lap_time";
    HighScoreType["CIRCUIT_TIME"] = "circuit_time";
})(HighScoreType || (HighScoreType = {}));
var HighScoreManager = (function () {
    function HighScoreManager() {
        this.maxEntries = 10;
        var config = OUTRUN_CONFIG.highscores;
        this.serviceName = config.serviceName;
        this.useNetwork = config.server !== 'file' && config.server !== '';
        try {
            if (this.useNetwork) {
                this.client = new JSONClient(config.server, config.port);
                logInfo('High scores: connecting to ' + config.server + ':' + config.port +
                    ' service=' + this.serviceName);
            }
            else {
                this.localDb = new JSONdb(config.filePath);
                this.localDb.settings.KEEP_READABLE = true;
                this.localDb.load();
                logInfo('High scores: using local file ' + config.filePath);
            }
        }
        catch (e) {
            logError("Failed to initialize high score storage: " + e);
            this.localDb = null;
            this.client = null;
        }
    }
    HighScoreManager.prototype.getKey = function (type, identifier) {
        var sanitized = identifier.replace(/\s+/g, '_').toLowerCase();
        return type + '.' + sanitized;
    };
    HighScoreManager.prototype.getScoresLocal = function (key) {
        if (!this.localDb)
            return [];
        try {
            this.localDb.load();
            var data = this.localDb.masterData.data || {};
            var scores = data[key];
            return (scores && Array.isArray(scores)) ? scores : [];
        }
        catch (e) {
            logError("Failed to read local scores: " + e);
            return [];
        }
    };
    HighScoreManager.prototype.getScoresNetwork = function (key) {
        if (!this.client)
            return [];
        try {
            var scores = this.client.read(this.serviceName, key, 1);
            return (scores && Array.isArray(scores)) ? scores : [];
        }
        catch (e) {
            logError("Failed to read network scores: " + e);
            return [];
        }
    };
    HighScoreManager.prototype.getScores = function (type, identifier) {
        var key = this.getKey(type, identifier);
        var scores = this.useNetwork ? this.getScoresNetwork(key) : this.getScoresLocal(key);
        scores.sort(function (a, b) {
            return a.time - b.time;
        });
        return scores;
    };
    HighScoreManager.prototype.getTopScore = function (type, identifier) {
        var scores = this.getScores(type, identifier);
        if (scores.length > 0) {
            return scores[0];
        }
        return null;
    };
    HighScoreManager.prototype.checkQualification = function (type, identifier, time) {
        var scores = this.getScores(type, identifier);
        if (scores.length < this.maxEntries) {
            for (var i = 0; i < scores.length; i++) {
                if (time < scores[i].time) {
                    return i + 1;
                }
            }
            return scores.length + 1;
        }
        for (var i = 0; i < scores.length; i++) {
            if (time < scores[i].time) {
                return i + 1;
            }
        }
        return 0;
    };
    HighScoreManager.prototype.submitScore = function (type, identifier, playerName, time, trackName, circuitName) {
        var key = this.getKey(type, identifier);
        var scores = this.getScores(type, identifier);
        var entry = {
            playerName: playerName,
            time: time,
            date: Date.now()
        };
        if (trackName)
            entry.trackName = trackName;
        if (circuitName)
            entry.circuitName = circuitName;
        scores.push(entry);
        scores.sort(function (a, b) {
            return a.time - b.time;
        });
        if (scores.length > this.maxEntries) {
            scores = scores.slice(0, this.maxEntries);
        }
        var position = 0;
        for (var i = 0; i < scores.length; i++) {
            if (scores[i].time === time && scores[i].date === entry.date) {
                position = i + 1;
                break;
            }
        }
        if (position === 0) {
            return 0;
        }
        try {
            if (this.useNetwork) {
                this.client.write(this.serviceName, key, scores, 2);
            }
            else if (this.localDb) {
                var data = this.localDb.masterData.data || {};
                data[key] = scores;
                this.localDb.masterData.data = data;
                this.localDb.save();
            }
        }
        catch (e) {
            logError("Failed to save high score: " + e);
            return 0;
        }
        return position;
    };
    HighScoreManager.prototype.clearAll = function () {
        try {
            if (this.useNetwork) {
                logWarning("Cannot clear all scores on network storage");
            }
            else if (this.localDb) {
                this.localDb.masterData.data = {};
                this.localDb.save();
            }
        }
        catch (e) {
            logError("Failed to clear high scores: " + e);
        }
    };
    HighScoreManager.prototype.clear = function (type, identifier) {
        var key = this.getKey(type, identifier);
        try {
            if (this.useNetwork) {
                this.client.write(this.serviceName, key, [], 2);
            }
            else if (this.localDb) {
                this.localDb.load();
                var data = this.localDb.masterData.data || {};
                delete data[key];
                this.localDb.masterData.data = data;
                this.localDb.save();
            }
        }
        catch (e) {
            logError("Failed to clear high scores: " + e);
        }
    };
    return HighScoreManager;
}());
"use strict";
function displayHighScores(scores, title, trackOrCircuitName, playerPosition) {
    console.clear(LIGHTGRAY, false);
    var screenWidth = 80;
    var screenHeight = 24;
    var titleAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
    var headerAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
    var nameAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
    var timeAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
    var dateAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var boxAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
    var highlightNameAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLUE });
    var highlightTimeAttr = colorToAttr({ fg: WHITE, bg: BG_BLUE });
    var highlightDateAttr = colorToAttr({ fg: LIGHTGRAY, bg: BG_BLUE });
    var boxWidth = Math.min(70, screenWidth - 4);
    var boxHeight = 18;
    var boxX = Math.floor((screenWidth - boxWidth) / 2);
    var topY = Math.floor((screenHeight - boxHeight) / 2);
    console.gotoxy(boxX, topY);
    console.attributes = boxAttr;
    console.print(GLYPH.DBOX_TL);
    for (var i = 1; i < boxWidth - 1; i++) {
        console.print(GLYPH.DBOX_H);
    }
    console.print(GLYPH.DBOX_TR + "\r\n");
    for (var j = 1; j < boxHeight - 1; j++) {
        console.gotoxy(boxX, topY + j);
        console.print(GLYPH.DBOX_V);
        console.gotoxy(boxX + boxWidth - 1, topY + j);
        console.print(GLYPH.DBOX_V + "\r\n");
    }
    console.gotoxy(boxX, topY + boxHeight - 1);
    console.print(GLYPH.DBOX_BL);
    for (var i = 1; i < boxWidth - 1; i++) {
        console.print(GLYPH.DBOX_H);
    }
    console.print(GLYPH.DBOX_BR + "\r\n");
    console.gotoxy(boxX + Math.floor((boxWidth - title.length) / 2), topY + 2);
    console.attributes = titleAttr;
    console.print(title);
    console.gotoxy(boxX + Math.floor((boxWidth - trackOrCircuitName.length) / 2), topY + 3);
    console.attributes = headerAttr;
    console.print(trackOrCircuitName);
    console.gotoxy(boxX + 3, topY + 5);
    console.attributes = headerAttr;
    console.print("RANK  PLAYER NAME           TIME        DATE");
    var startY = topY + 6;
    for (var i = 0; i < 10; i++) {
        console.gotoxy(boxX + 3, startY + i);
        var isHighlighted = (playerPosition !== undefined && playerPosition === i + 1);
        if (i < scores.length) {
            var score = scores[i];
            var rank = (i + 1) + ".";
            if (i < 9)
                rank = " " + rank;
            console.attributes = isHighlighted ? highlightNameAttr : nameAttr;
            console.print(rank + "   ");
            var name = score.playerName;
            if (name.length > 18) {
                name = name.substring(0, 15) + "...";
            }
            while (name.length < 18) {
                name += " ";
            }
            console.print(name + "  ");
            console.attributes = isHighlighted ? highlightTimeAttr : timeAttr;
            var timeStr = LapTimer.format(score.time);
            console.print(timeStr);
            while (timeStr.length < 10) {
                timeStr += " ";
                console.print(" ");
            }
            console.print("  ");
            console.attributes = isHighlighted ? highlightDateAttr : dateAttr;
            var date = new Date(score.date);
            var dateStr = (date.getMonth() + 1) + "/" + date.getDate() + "/" + date.getFullYear();
            console.print(dateStr);
            if (isHighlighted) {
                console.attributes = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
                console.print(" <-- YOU!");
            }
        }
        else {
            console.attributes = emptyAttr;
            var rank = (i + 1) + ".";
            if (i < 9)
                rank = " " + rank;
            console.print(rank + "   ---");
        }
    }
    console.gotoxy(boxX + Math.floor((boxWidth - 24) / 2), topY + boxHeight - 2);
    console.attributes = headerAttr;
    console.print("Press any key to continue");
}
function showHighScoreList(type, identifier, title, trackOrCircuitName, highScoreManager, playerPosition) {
    var scores = highScoreManager.getScores(type, identifier);
    displayHighScores(scores, title, trackOrCircuitName, playerPosition);
    console.line_counter = 0;
    console.inkey(K_NONE, 300000);
}
function displayTopScoreLine(label, score, x, y, labelAttr, valueAttr) {
    console.gotoxy(x, y);
    console.attributes = labelAttr;
    console.print(label + ": ");
    console.attributes = valueAttr;
    if (score) {
        console.print(LapTimer.format(score.time) + " by " + score.playerName);
    }
    else {
        console.print("--:--.-- (No record)");
    }
}
function showTwoColumnHighScores(trackId, trackName, highScoreManager, trackTimePosition, lapTimePosition) {
    console.clear(LIGHTGRAY, false);
    var viewWidth = 80;
    var viewHeight = 24;
    var trackScores = highScoreManager.getScores(HighScoreType.TRACK_TIME, trackId);
    var lapScores = highScoreManager.getScores(HighScoreType.LAP_TIME, trackId);
    var titleAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
    var headerAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
    var colHeaderAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
    var rankAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
    var nameAttr = colorToAttr({ fg: LIGHTGRAY, bg: BG_BLACK });
    var timeAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
    var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
    var boxAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
    var highlightAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLUE });
    var promptAttr = colorToAttr({ fg: LIGHTMAGENTA, bg: BG_BLACK });
    var newScoreAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
    var boxWidth = 76;
    var boxHeight = 20;
    var boxX = Math.floor((viewWidth - boxWidth) / 2);
    var topY = Math.floor((viewHeight - boxHeight) / 2);
    console.gotoxy(boxX, topY);
    console.attributes = boxAttr;
    console.print(GLYPH.DBOX_TL);
    for (var i = 1; i < boxWidth - 1; i++) {
        console.print(GLYPH.DBOX_H);
    }
    console.print(GLYPH.DBOX_TR);
    for (var j = 1; j < boxHeight - 1; j++) {
        console.gotoxy(boxX, topY + j);
        console.print(GLYPH.DBOX_V);
        console.gotoxy(boxX + boxWidth - 1, topY + j);
        console.print(GLYPH.DBOX_V);
    }
    console.gotoxy(boxX, topY + boxHeight - 1);
    console.print(GLYPH.DBOX_BL);
    for (var i = 1; i < boxWidth - 1; i++) {
        console.print(GLYPH.DBOX_H);
    }
    console.print(GLYPH.DBOX_BR);
    var title = "=== HIGH SCORES ===";
    console.gotoxy(boxX + Math.floor((boxWidth - title.length) / 2), topY + 1);
    console.attributes = titleAttr;
    console.print(title);
    console.gotoxy(boxX + Math.floor((boxWidth - trackName.length) / 2), topY + 2);
    console.attributes = headerAttr;
    console.print(trackName);
    var leftColX = boxX + 3;
    var rightColX = boxX + 40;
    console.gotoxy(leftColX, topY + 4);
    console.attributes = colHeaderAttr;
    console.print("TRACK TIME");
    console.gotoxy(rightColX, topY + 4);
    console.print("BEST LAP");
    console.gotoxy(boxX + 37, topY + 4);
    console.attributes = boxAttr;
    console.print(GLYPH.BOX_V);
    for (var j = 5; j < boxHeight - 3; j++) {
        console.gotoxy(boxX + 37, topY + j);
        console.print(GLYPH.BOX_V);
    }
    var startY = topY + 5;
    for (var i = 0; i < 10; i++) {
        console.gotoxy(leftColX, startY + i);
        var trackHighlighted = (trackTimePosition > 0 && trackTimePosition === i + 1);
        if (i < trackScores.length) {
            var score = trackScores[i];
            var rank = (i + 1) + ".";
            if (i < 9)
                rank = " " + rank;
            console.attributes = trackHighlighted ? highlightAttr : rankAttr;
            console.print(rank + " ");
            var name = score.playerName;
            if (name.length > 11)
                name = name.substring(0, 10) + ".";
            while (name.length < 11)
                name += " ";
            console.attributes = trackHighlighted ? highlightAttr : nameAttr;
            console.print(name + " ");
            console.attributes = trackHighlighted ? highlightAttr : timeAttr;
            console.print(LapTimer.format(score.time));
            var d = new Date(score.date);
            var dateStr = " " + (d.getMonth() + 1) + "/" + d.getDate() + "/" + d.getFullYear();
            console.attributes = trackHighlighted ? highlightAttr : emptyAttr;
            console.print(dateStr);
            if (trackHighlighted) {
                console.attributes = newScoreAttr;
                console.print("*");
            }
        }
        else {
            console.attributes = emptyAttr;
            var rank = (i + 1) + ".";
            if (i < 9)
                rank = " " + rank;
            console.print(rank + " ---");
        }
        console.gotoxy(rightColX, startY + i);
        var lapHighlighted = (lapTimePosition > 0 && lapTimePosition === i + 1);
        if (i < lapScores.length) {
            var score = lapScores[i];
            var rank = (i + 1) + ".";
            if (i < 9)
                rank = " " + rank;
            console.attributes = lapHighlighted ? highlightAttr : rankAttr;
            console.print(rank + " ");
            var name = score.playerName;
            if (name.length > 11)
                name = name.substring(0, 10) + ".";
            while (name.length < 11)
                name += " ";
            console.attributes = lapHighlighted ? highlightAttr : nameAttr;
            console.print(name + " ");
            console.attributes = lapHighlighted ? highlightAttr : timeAttr;
            console.print(LapTimer.format(score.time));
            var d = new Date(score.date);
            var dateStr = " " + (d.getMonth() + 1) + "/" + d.getDate() + "/" + d.getFullYear();
            console.attributes = lapHighlighted ? highlightAttr : emptyAttr;
            console.print(dateStr);
            if (lapHighlighted) {
                console.attributes = newScoreAttr;
                console.print(" *");
            }
        }
        else {
            console.attributes = emptyAttr;
            var rank = (i + 1) + ".";
            if (i < 9)
                rank = " " + rank;
            console.print(rank + " ---");
        }
    }
    if (trackTimePosition > 0 || lapTimePosition > 0) {
        console.gotoxy(boxX + 3, topY + boxHeight - 3);
        console.attributes = newScoreAttr;
        console.print("* = Your new high score!");
    }
    var prompt = "Press any key to continue";
    console.gotoxy(boxX + Math.floor((boxWidth - prompt.length) / 2), topY + boxHeight - 2);
    console.attributes = promptAttr;
    console.print(prompt);
    console.line_counter = 0;
    console.inkey(K_NONE, 300000);
}
"use strict";
var PALETTE = {
    SKY_TOP: { fg: MAGENTA, bg: BG_BLACK },
    SKY_MID: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    SKY_HORIZON: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    SKY_GRID: { fg: MAGENTA, bg: BG_BLACK },
    SKY_GRID_GLOW: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    SUN_CORE: { fg: YELLOW, bg: BG_RED },
    SUN_GLOW: { fg: LIGHTRED, bg: BG_BLACK },
    MOUNTAIN: { fg: MAGENTA, bg: BG_BLACK },
    MOUNTAIN_HIGHLIGHT: { fg: LIGHTMAGENTA, bg: BG_BLACK },
    GRID_LINE: { fg: CYAN, bg: BG_BLACK },
    GRID_GLOW: { fg: LIGHTCYAN, bg: BG_BLACK },
    ROAD_LIGHT: { fg: LIGHTCYAN, bg: BG_BLACK },
    ROAD_DARK: { fg: CYAN, bg: BG_BLACK },
    ROAD_STRIPE: { fg: WHITE, bg: BG_BLACK },
    ROAD_EDGE: { fg: LIGHTRED, bg: BG_BLACK },
    ROAD_GRID: { fg: CYAN, bg: BG_BLACK },
    OFFROAD_GRASS: { fg: GREEN, bg: BG_BLACK },
    OFFROAD_DIRT: { fg: BROWN, bg: BG_BLACK },
    OFFROAD_ROCK: { fg: DARKGRAY, bg: BG_BLACK },
    OFFROAD_TREE: { fg: LIGHTGREEN, bg: BG_BLACK },
    OFFROAD_TREE_TRUNK: { fg: BROWN, bg: BG_BLACK },
    OFFROAD_CACTUS: { fg: GREEN, bg: BG_BLACK },
    PLAYER_BODY: { fg: YELLOW, bg: BG_BLACK },
    PLAYER_TRIM: { fg: WHITE, bg: BG_BLACK },
    ENEMY_BODY: { fg: LIGHTCYAN, bg: BG_BLACK },
    HUD_FRAME: { fg: LIGHTCYAN, bg: BG_BLACK },
    HUD_TEXT: { fg: WHITE, bg: BG_BLACK },
    HUD_VALUE: { fg: YELLOW, bg: BG_BLACK },
    HUD_LABEL: { fg: LIGHTGRAY, bg: BG_BLACK },
    ITEM_BOX: { fg: YELLOW, bg: BG_BLACK },
    ITEM_MUSHROOM: { fg: LIGHTRED, bg: BG_BLACK },
    ITEM_SHELL: { fg: LIGHTGREEN, bg: BG_BLACK }
};
function makeAttr(fg, bg) {
    return fg | bg;
}
function getFg(attr) {
    return attr & 0x0F;
}
function getBg(attr) {
    return attr & 0xF0;
}
function colorToAttr(color) {
    return color.fg | color.bg;
}
"use strict";
var GLYPH = {
    FULL_BLOCK: String.fromCharCode(219),
    LOWER_HALF: String.fromCharCode(220),
    UPPER_HALF: String.fromCharCode(223),
    LEFT_HALF: String.fromCharCode(221),
    RIGHT_HALF: String.fromCharCode(222),
    LIGHT_SHADE: String.fromCharCode(176),
    MEDIUM_SHADE: String.fromCharCode(177),
    DARK_SHADE: String.fromCharCode(178),
    BOX_H: String.fromCharCode(196),
    BOX_V: String.fromCharCode(179),
    BOX_TL: String.fromCharCode(218),
    BOX_TR: String.fromCharCode(191),
    BOX_BL: String.fromCharCode(192),
    BOX_BR: String.fromCharCode(217),
    BOX_VR: String.fromCharCode(195),
    BOX_VL: String.fromCharCode(180),
    BOX_HD: String.fromCharCode(194),
    BOX_HU: String.fromCharCode(193),
    BOX_CROSS: String.fromCharCode(197),
    DBOX_H: String.fromCharCode(205),
    DBOX_V: String.fromCharCode(186),
    DBOX_TL: String.fromCharCode(201),
    DBOX_TR: String.fromCharCode(187),
    DBOX_BL: String.fromCharCode(200),
    DBOX_BR: String.fromCharCode(188),
    DBOX_VR: String.fromCharCode(204),
    DBOX_VL: String.fromCharCode(185),
    DBOX_HD: String.fromCharCode(203),
    DBOX_HU: String.fromCharCode(202),
    DBOX_CROSS: String.fromCharCode(206),
    BOX_VD_HD: String.fromCharCode(209),
    BOX_VD_HU: String.fromCharCode(207),
    TRIANGLE_UP: '^',
    TRIANGLE_DOWN: 'v',
    TRIANGLE_LEFT: '<',
    TRIANGLE_RIGHT: '>',
    DIAMOND: '*',
    BULLET: 'o',
    CIRCLE: 'O',
    INVERSE_BULLET: '@',
    SPACE: ' ',
    DOT: String.fromCharCode(250),
    SLASH: '/',
    BACKSLASH: '\\',
    EQUALS: '=',
    ASTERISK: '*',
    TREE_TOP: '^',
    TREE_TRUNK: String.fromCharCode(179),
    ROCK: String.fromCharCode(178),
    GRASS: String.fromCharCode(176),
    CACTUS: '|',
    MOUNTAIN_PEAK: '/',
    MOUNTAIN_SLOPE: '\\',
    CHECKER: String.fromCharCode(177),
    FLAG: '>'
};
function getShadeGlyph(intensity) {
    if (intensity >= 0.75)
        return GLYPH.FULL_BLOCK;
    if (intensity >= 0.5)
        return GLYPH.DARK_SHADE;
    if (intensity >= 0.25)
        return GLYPH.MEDIUM_SHADE;
    return GLYPH.LIGHT_SHADE;
}
function getBarGlyph(fill) {
    if (fill >= 0.875)
        return GLYPH.FULL_BLOCK;
    if (fill >= 0.625)
        return GLYPH.DARK_SHADE;
    if (fill >= 0.375)
        return GLYPH.MEDIUM_SHADE;
    if (fill >= 0.125)
        return GLYPH.LIGHT_SHADE;
    return GLYPH.SPACE;
}
"use strict";
var SceneComposer = (function () {
    function SceneComposer(width, height) {
        this.width = width;
        this.height = height;
        this.buffer = [];
        this.defaultAttr = makeAttr(BLACK, BG_BLACK);
        this.clear();
    }
    SceneComposer.prototype.clear = function () {
        this.buffer = [];
        for (var y = 0; y < this.height; y++) {
            var row = [];
            for (var x = 0; x < this.width; x++) {
                row.push({ char: ' ', attr: this.defaultAttr });
            }
            this.buffer.push(row);
        }
    };
    SceneComposer.prototype.setCell = function (x, y, char, attr) {
        if (x >= 0 && x < this.width && y >= 0 && y < this.height) {
            this.buffer[y][x] = { char: char, attr: attr };
        }
    };
    SceneComposer.prototype.getCell = function (x, y) {
        if (x >= 0 && x < this.width && y >= 0 && y < this.height) {
            return this.buffer[y][x];
        }
        return null;
    };
    SceneComposer.prototype.writeString = function (x, y, text, attr) {
        for (var i = 0; i < text.length; i++) {
            this.setCell(x + i, y, text.charAt(i), attr);
        }
    };
    SceneComposer.prototype.drawHLine = function (x, y, length, char, attr) {
        for (var i = 0; i < length; i++) {
            this.setCell(x + i, y, char, attr);
        }
    };
    SceneComposer.prototype.drawVLine = function (x, y, length, char, attr) {
        for (var i = 0; i < length; i++) {
            this.setCell(x, y + i, char, attr);
        }
    };
    SceneComposer.prototype.drawBox = function (x, y, w, h, attr) {
        this.setCell(x, y, GLYPH.BOX_TL, attr);
        this.setCell(x + w - 1, y, GLYPH.BOX_TR, attr);
        this.setCell(x, y + h - 1, GLYPH.BOX_BL, attr);
        this.setCell(x + w - 1, y + h - 1, GLYPH.BOX_BR, attr);
        for (var i = 1; i < w - 1; i++) {
            this.setCell(x + i, y, GLYPH.BOX_H, attr);
            this.setCell(x + i, y + h - 1, GLYPH.BOX_H, attr);
        }
        for (var j = 1; j < h - 1; j++) {
            this.setCell(x, y + j, GLYPH.BOX_V, attr);
            this.setCell(x + w - 1, y + j, GLYPH.BOX_V, attr);
        }
    };
    SceneComposer.prototype.getBuffer = function () {
        return this.buffer;
    };
    SceneComposer.prototype.getRow = function (y) {
        var row = '';
        for (var x = 0; x < this.width; x++) {
            row += this.buffer[y][x].char;
        }
        return row;
    };
    return SceneComposer;
}());
"use strict";
var RoadRenderer = (function () {
    function RoadRenderer(composer, horizonY) {
        this.composer = composer;
        this.horizonY = horizonY;
        this.roadBottom = 23;
    }
    RoadRenderer.prototype.render = function (trackPosition, cameraX, track, road) {
        var roadLength = road ? road.totalLength : 55000;
        for (var screenY = this.roadBottom; screenY > this.horizonY; screenY--) {
            var t = (this.roadBottom - screenY) / (this.roadBottom - this.horizonY);
            var distance = 1 / (1 - t * 0.95);
            var roadWidth = Math.round(40 / distance);
            var halfWidth = Math.floor(roadWidth / 2);
            var curveOffset = 0;
            if (track && typeof track.getCenterlineX === 'function') {
                curveOffset = Math.round(track.getCenterlineX(trackPosition + distance * 10) * 0.1);
            }
            var centerX = 40 + curveOffset - Math.round(cameraX * 0.5);
            this.renderScanline(screenY, centerX, halfWidth, distance, trackPosition, roadLength);
        }
    };
    RoadRenderer.prototype.renderScanline = function (y, centerX, halfWidth, distance, trackZ, roadLength) {
        var width = 80;
        var leftEdge = centerX - halfWidth;
        var rightEdge = centerX + halfWidth;
        var stripePhase = Math.floor((trackZ + distance * 5) / 15) % 2;
        var roadAttr = colorToAttr(distance < 10 ? PALETTE.ROAD_LIGHT : PALETTE.ROAD_DARK);
        var gridAttr = colorToAttr(PALETTE.ROAD_GRID);
        var worldZ = trackZ + distance * 5;
        var wrappedZ = worldZ % roadLength;
        if (wrappedZ < 0)
            wrappedZ += roadLength;
        var isFinishLine = (wrappedZ < 200) || (wrappedZ > roadLength - 200);
        for (var x = 0; x < width; x++) {
            if (x >= leftEdge && x <= rightEdge) {
                if (isFinishLine) {
                    this.renderFinishLineCell(x, y, centerX, leftEdge, rightEdge, distance);
                }
                else {
                    this.renderRoadCell(x, y, centerX, leftEdge, rightEdge, stripePhase, distance, roadAttr, gridAttr);
                }
            }
            else {
                this.renderOffroadCell(x, y, leftEdge, rightEdge, distance, trackZ);
            }
        }
    };
    RoadRenderer.prototype.renderFinishLineCell = function (x, y, centerX, leftEdge, rightEdge, distance) {
        if (x === leftEdge || x === rightEdge) {
            this.composer.setCell(x, y, GLYPH.BOX_V, colorToAttr({ fg: WHITE, bg: BG_BLACK }));
            return;
        }
        var checkerSize = Math.max(1, Math.floor(3 / distance));
        var checkerX = Math.floor((x - centerX) / checkerSize);
        var checkerY = Math.floor(y / 2);
        var isWhite = ((checkerX + checkerY) % 2) === 0;
        if (isWhite) {
            this.composer.setCell(x, y, GLYPH.FULL_BLOCK, colorToAttr({ fg: WHITE, bg: BG_LIGHTGRAY }));
        }
        else {
            this.composer.setCell(x, y, ' ', colorToAttr({ fg: BLACK, bg: BG_BLACK }));
        }
    };
    RoadRenderer.prototype.renderRoadCell = function (x, y, centerX, leftEdge, rightEdge, stripePhase, distance, roadAttr, gridAttr) {
        if (x === leftEdge || x === rightEdge) {
            this.composer.setCell(x, y, GLYPH.BOX_V, colorToAttr(PALETTE.ROAD_EDGE));
            return;
        }
        if (Math.abs(x - centerX) < 1 && stripePhase === 0) {
            this.composer.setCell(x, y, GLYPH.BOX_V, colorToAttr(PALETTE.ROAD_STRIPE));
            return;
        }
        var gridPhase = Math.floor(distance) % 3;
        if (gridPhase === 0 && distance > 5) {
            this.composer.setCell(x, y, GLYPH.BOX_H, gridAttr);
        }
        else {
            var shade = this.getShadeForDistance(distance);
            this.composer.setCell(x, y, shade, roadAttr);
        }
    };
    RoadRenderer.prototype.renderOffroadCell = function (x, y, leftEdge, rightEdge, distance, trackZ) {
        var bgAttr = makeAttr(BLACK, BG_BLACK);
        this.composer.setCell(x, y, ' ', bgAttr);
        var side = x < leftEdge ? 'left' : 'right';
        var distFromRoad = side === 'left' ? leftEdge - x : x - rightEdge;
        var scale = Math.min(1, 3 / distance);
        var gridSize = 8;
        var worldZ = Math.floor(trackZ + distance * 10);
        var gridZ = Math.floor(worldZ / gridSize);
        var anchorX = (side === 'left') ? leftEdge - 8 : rightEdge + 8;
        var seed = gridZ * 1000 + (side === 'left' ? 1 : 2);
        var rand = this.pseudoRandom(seed);
        var shouldDraw = (Math.abs(x - anchorX) < 2) &&
            (rand < 0.7) &&
            (distFromRoad > 3 && distFromRoad < 20);
        if (shouldDraw && x === anchorX) {
            this.drawSceneryObject(x, y, distance, scale, rand);
        }
        else if (distFromRoad <= 2) {
            this.composer.setCell(x, y, GLYPH.GRASS, colorToAttr(PALETTE.OFFROAD_DIRT));
        }
    };
    RoadRenderer.prototype.drawSceneryObject = function (x, y, distance, scale, rand) {
        if (rand < 0.05) {
            this.drawTree(x, y, distance, scale);
        }
        else if (rand < 0.10) {
            this.drawRock(x, y, distance, scale);
        }
        else {
            this.drawBush(x, y, distance, scale);
        }
    };
    RoadRenderer.prototype.drawTree = function (x, y, distance, _scale) {
        var leafTrunkAttr = makeAttr(LIGHTGREEN, BG_BROWN);
        var leafAttr = makeAttr(LIGHTGREEN, BG_BLACK);
        var leafDarkAttr = makeAttr(GREEN, BG_BLACK);
        var trunkAttr = makeAttr(BROWN, BG_BLACK);
        if (distance > 8) {
            this.safePut(x, y, GLYPH.UPPER_HALF, leafTrunkAttr);
        }
        else if (distance > 5) {
            this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x, y - 1, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x, y, GLYPH.UPPER_HALF, leafTrunkAttr);
        }
        else if (distance > 3) {
            this.safePut(x - 1, y - 2, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 2, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x - 1, y - 1, GLYPH.DARK_SHADE, leafDarkAttr);
            this.safePut(x, y - 1, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 1, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 1, GLYPH.DARK_SHADE, leafDarkAttr);
            this.safePut(x, y, GLYPH.UPPER_HALF, leafTrunkAttr);
            this.safePut(x + 1, y, GLYPH.UPPER_HALF, leafTrunkAttr);
        }
        else if (distance > 1.5) {
            this.safePut(x - 2, y - 3, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x - 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 3, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x - 2, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
            this.safePut(x - 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
            this.safePut(x - 1, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
            this.safePut(x, y - 1, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, trunkAttr);
            this.safePut(x, y + 1, GLYPH.UPPER_HALF, trunkAttr);
        }
        else {
            this.safePut(x - 2, y - 4, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x - 1, y - 4, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x, y - 4, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 4, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 4, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 3, y - 4, GLYPH.UPPER_HALF, leafDarkAttr);
            this.safePut(x - 2, y - 3, GLYPH.FULL_BLOCK, leafDarkAttr);
            this.safePut(x - 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 3, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 3, y - 3, GLYPH.FULL_BLOCK, leafDarkAttr);
            this.safePut(x - 1, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
            this.safePut(x, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 1, y - 2, GLYPH.FULL_BLOCK, leafAttr);
            this.safePut(x + 2, y - 2, GLYPH.DARK_SHADE, leafDarkAttr);
            this.safePut(x, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
            this.safePut(x + 1, y - 1, GLYPH.LOWER_HALF, leafDarkAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, trunkAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, trunkAttr);
            this.safePut(x, y + 1, GLYPH.UPPER_HALF, trunkAttr);
            this.safePut(x + 1, y + 1, GLYPH.UPPER_HALF, trunkAttr);
        }
    };
    RoadRenderer.prototype.drawRock = function (x, y, distance, _scale) {
        var rockAttr = makeAttr(DARKGRAY, BG_BLACK);
        var rockLightAttr = makeAttr(LIGHTGRAY, BG_BLACK);
        if (distance > 8) {
            this.safePut(x, y, GLYPH.LOWER_HALF, rockAttr);
        }
        else if (distance > 5) {
            this.safePut(x - 1, y, GLYPH.LOWER_HALF, rockAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
            this.safePut(x + 1, y, GLYPH.LOWER_HALF, rockAttr);
        }
        else if (distance > 3) {
            this.safePut(x - 1, y, GLYPH.LOWER_HALF, rockAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, rockAttr);
            this.safePut(x + 2, y, GLYPH.LOWER_HALF, rockAttr);
        }
        else if (distance > 1.5) {
            this.safePut(x - 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
            this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
            this.safePut(x, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
            this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, rockAttr);
            this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
            this.safePut(x - 2, y, GLYPH.LOWER_HALF, rockAttr);
            this.safePut(x - 1, y, GLYPH.FULL_BLOCK, rockAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, rockAttr);
            this.safePut(x + 2, y, GLYPH.LOWER_HALF, rockAttr);
        }
        else {
            this.safePut(x - 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
            this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
            this.safePut(x, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
            this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, rockLightAttr);
            this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, rockAttr);
            this.safePut(x + 3, y - 1, GLYPH.UPPER_HALF, rockAttr);
            this.safePut(x - 2, y, GLYPH.LOWER_HALF, rockAttr);
            this.safePut(x - 1, y, GLYPH.FULL_BLOCK, rockAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, rockLightAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, rockLightAttr);
            this.safePut(x + 2, y, GLYPH.FULL_BLOCK, rockAttr);
            this.safePut(x + 3, y, GLYPH.LOWER_HALF, rockAttr);
        }
    };
    RoadRenderer.prototype.drawBush = function (x, y, distance, _scale) {
        var bushAttr = makeAttr(GREEN, BG_BLACK);
        var bushLightAttr = makeAttr(LIGHTGREEN, BG_BLACK);
        if (distance > 8) {
            this.safePut(x, y, GLYPH.LOWER_HALF, bushAttr);
        }
        else if (distance > 5) {
            this.safePut(x - 1, y, GLYPH.LOWER_HALF, bushAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
            this.safePut(x + 1, y, GLYPH.LOWER_HALF, bushAttr);
        }
        else if (distance > 3) {
            this.safePut(x - 1, y, GLYPH.LOWER_HALF, bushAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, bushAttr);
            this.safePut(x + 2, y, GLYPH.LOWER_HALF, bushAttr);
        }
        else if (distance > 1.5) {
            this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, bushAttr);
            this.safePut(x, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
            this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
            this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, bushAttr);
            this.safePut(x - 2, y, GLYPH.LOWER_HALF, bushAttr);
            this.safePut(x - 1, y, GLYPH.FULL_BLOCK, bushAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, bushLightAttr);
            this.safePut(x + 2, y, GLYPH.FULL_BLOCK, bushAttr);
            this.safePut(x + 3, y, GLYPH.LOWER_HALF, bushAttr);
        }
        else {
            this.safePut(x - 2, y - 1, GLYPH.UPPER_HALF, bushAttr);
            this.safePut(x - 1, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
            this.safePut(x, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
            this.safePut(x + 1, y - 1, GLYPH.UPPER_HALF, bushLightAttr);
            this.safePut(x + 2, y - 1, GLYPH.UPPER_HALF, bushAttr);
            this.safePut(x + 3, y - 1, GLYPH.UPPER_HALF, bushAttr);
            this.safePut(x - 2, y, GLYPH.LOWER_HALF, bushAttr);
            this.safePut(x - 1, y, GLYPH.FULL_BLOCK, bushAttr);
            this.safePut(x, y, GLYPH.FULL_BLOCK, bushLightAttr);
            this.safePut(x + 1, y, GLYPH.FULL_BLOCK, bushLightAttr);
            this.safePut(x + 2, y, GLYPH.FULL_BLOCK, bushAttr);
            this.safePut(x + 3, y, GLYPH.LOWER_HALF, bushAttr);
        }
    };
    RoadRenderer.prototype.safePut = function (x, y, char, attr) {
        if (x >= 0 && x < 80 && y >= this.horizonY && y <= this.roadBottom) {
            this.composer.setCell(x, y, char, attr);
        }
    };
    RoadRenderer.prototype.pseudoRandom = function (seed) {
        var x = Math.sin(seed * 12.9898) * 43758.5453;
        return x - Math.floor(x);
    };
    RoadRenderer.prototype.getShadeForDistance = function (distance) {
        if (distance < 5)
            return GLYPH.FULL_BLOCK;
        if (distance < 15)
            return GLYPH.DARK_SHADE;
        if (distance < 30)
            return GLYPH.MEDIUM_SHADE;
        return GLYPH.LIGHT_SHADE;
    };
    return RoadRenderer;
}());
"use strict";
var ParallaxBackground = (function () {
    function ParallaxBackground(visibleWidth, horizonY) {
        this.MOUNTAIN_PARALLAX = 0.3;
        this.width = visibleWidth;
        this.height = horizonY;
        this.bufferWidth = visibleWidth * 3;
        this.scrollOffset = this.width;
        this.skyBuffer = [];
        this.mountainBuffer = [];
        this.renderBackgroundLayers();
    }
    ParallaxBackground.prototype.renderBackgroundLayers = function () {
        this.skyBuffer = [];
        this.mountainBuffer = [];
        for (var y = 0; y < this.height; y++) {
            var skyRow = [];
            var mtRow = [];
            for (var x = 0; x < this.bufferWidth; x++) {
                skyRow.push({ char: ' ', attr: makeAttr(BLACK, BG_BLACK) });
                mtRow.push({ char: ' ', attr: makeAttr(BLACK, BG_BLACK) });
            }
            this.skyBuffer.push(skyRow);
            this.mountainBuffer.push(mtRow);
        }
        this.renderSunToBuffer();
        this.renderMountainsToBuffer();
    };
    ParallaxBackground.prototype.renderSunToBuffer = function () {
        var sunCoreAttr = makeAttr(YELLOW, BG_RED);
        var sunGlowAttr = makeAttr(LIGHTRED, BG_BLACK);
        var sunCenterX = Math.floor(this.bufferWidth / 2);
        var sunY = this.height - 5;
        for (var dy = -1; dy <= 1; dy++) {
            for (var dx = -2; dx <= 2; dx++) {
                var y = sunY + dy;
                var x = sunCenterX + dx;
                if (y >= 0 && y < this.height && x >= 0 && x < this.bufferWidth) {
                    this.skyBuffer[y][x] = { char: GLYPH.FULL_BLOCK, attr: sunCoreAttr };
                }
            }
        }
        var glowOffsets = [
            { dx: -3, dy: -1 }, { dx: -3, dy: 0 }, { dx: -3, dy: 1 },
            { dx: 3, dy: -1 }, { dx: 3, dy: 0 }, { dx: 3, dy: 1 },
            { dx: -2, dy: -2 }, { dx: -1, dy: -2 }, { dx: 0, dy: -2 }, { dx: 1, dy: -2 }, { dx: 2, dy: -2 },
            { dx: -2, dy: 2 }, { dx: -1, dy: 2 }, { dx: 0, dy: 2 }, { dx: 1, dy: 2 }, { dx: 2, dy: 2 }
        ];
        for (var i = 0; i < glowOffsets.length; i++) {
            var g = glowOffsets[i];
            var y = sunY + g.dy;
            var x = sunCenterX + g.dx;
            if (y >= 0 && y < this.height && x >= 0 && x < this.bufferWidth) {
                this.skyBuffer[y][x] = { char: GLYPH.DARK_SHADE, attr: sunGlowAttr };
            }
        }
    };
    ParallaxBackground.prototype.renderMountainsToBuffer = function () {
        var mountainAttr = colorToAttr(PALETTE.MOUNTAIN);
        var highlightAttr = colorToAttr(PALETTE.MOUNTAIN_HIGHLIGHT);
        var mountainPattern = [
            { xOffset: 0, height: 4, width: 12 },
            { xOffset: 15, height: 6, width: 16 },
            { xOffset: 35, height: 3, width: 10 },
            { xOffset: 50, height: 5, width: 14 },
            { xOffset: 68, height: 4, width: 11 }
        ];
        var patternWidth = 80;
        var mountainBaseY = this.height - 1;
        for (var tileX = 0; tileX < this.bufferWidth; tileX += patternWidth) {
            for (var m = 0; m < mountainPattern.length; m++) {
                var mt = mountainPattern[m];
                var baseX = tileX + mt.xOffset;
                this.drawMountainToBuffer(baseX, mountainBaseY, mt.height, mt.width, mountainAttr, highlightAttr);
            }
        }
    };
    ParallaxBackground.prototype.drawMountainToBuffer = function (baseX, baseY, height, width, attr, highlightAttr) {
        var peakX = baseX + Math.floor(width / 2);
        for (var h = 0; h < height; h++) {
            var y = baseY - h;
            if (y < 0 || y >= this.height)
                continue;
            var halfWidth = Math.floor((height - h) * width / height / 2);
            for (var dx = -halfWidth; dx < 0; dx++) {
                var x = peakX + dx;
                if (x >= 0 && x < this.bufferWidth) {
                    this.mountainBuffer[y][x] = { char: '/', attr: attr };
                }
            }
            if (peakX >= 0 && peakX < this.bufferWidth) {
                if (h === height - 1) {
                    this.mountainBuffer[y][peakX] = { char: GLYPH.TRIANGLE_UP, attr: highlightAttr };
                }
                else {
                    this.mountainBuffer[y][peakX] = { char: GLYPH.BOX_V, attr: attr };
                }
            }
            for (var dx = 1; dx <= halfWidth; dx++) {
                var x = peakX + dx;
                if (x >= 0 && x < this.bufferWidth) {
                    this.mountainBuffer[y][x] = { char: '\\', attr: attr };
                }
            }
        }
    };
    ParallaxBackground.prototype.updateScroll = function (curvature, playerSteer, speed, dt) {
        var scrollSpeed = (curvature * 0.8 + playerSteer * 0.2) * speed * dt * 0.1;
        this.scrollOffset += scrollSpeed;
        if (this.scrollOffset < 0) {
            this.scrollOffset += this.bufferWidth;
        }
        else if (this.scrollOffset >= this.bufferWidth) {
            this.scrollOffset -= this.bufferWidth;
        }
    };
    ParallaxBackground.prototype.render = function (composer) {
        var mountainScroll = Math.floor(this.scrollOffset * this.MOUNTAIN_PARALLAX) % this.bufferWidth;
        var skyScroll = Math.floor(this.scrollOffset * 0.1) % this.bufferWidth;
        for (var y = 0; y < this.height; y++) {
            for (var x = 0; x < this.width; x++) {
                var srcX = (x + skyScroll) % this.bufferWidth;
                var cell = this.skyBuffer[y][srcX];
                if (cell.char !== ' ') {
                    composer.setCell(x, y, cell.char, cell.attr);
                }
            }
        }
        for (var y = 0; y < this.height; y++) {
            for (var x = 0; x < this.width; x++) {
                var srcX = (x + mountainScroll) % this.bufferWidth;
                var cell = this.mountainBuffer[y][srcX];
                if (cell.char !== ' ') {
                    composer.setCell(x, y, cell.char, cell.attr);
                }
            }
        }
    };
    ParallaxBackground.prototype.getScrollOffset = function () {
        return this.scrollOffset;
    };
    ParallaxBackground.prototype.resetScroll = function () {
        this.scrollOffset = this.width;
    };
    return ParallaxBackground;
}());
"use strict";
var SkylineRenderer = (function () {
    function SkylineRenderer(composer, horizonY) {
        this.composer = composer;
        this.horizonY = horizonY;
        this.parallax = new ParallaxBackground(80, horizonY);
        this.parallax.resetScroll();
        this.gridAnimPhase = 0;
    }
    SkylineRenderer.prototype.render = function (_trackPosition, curvature, playerSteer, speed, dt) {
        this.renderSkyBackground();
        if (curvature !== undefined && speed !== undefined && dt !== undefined && speed > 0) {
            this.parallax.updateScroll(curvature, playerSteer || 0, speed, dt);
        }
        if (speed !== undefined && dt !== undefined && speed > 0) {
            this.gridAnimPhase += speed * dt * 0.003;
            while (this.gridAnimPhase >= 1)
                this.gridAnimPhase -= 1;
            while (this.gridAnimPhase < 0)
                this.gridAnimPhase += 1;
        }
        this.renderSkyGrid();
        this.parallax.render(this.composer);
    };
    SkylineRenderer.prototype.renderSkyBackground = function () {
        var bgAttr = makeAttr(BLACK, BG_BLACK);
        for (var y = 0; y < this.horizonY; y++) {
            for (var x = 0; x < 80; x++) {
                this.composer.setCell(x, y, ' ', bgAttr);
            }
        }
    };
    SkylineRenderer.prototype.renderSkyGrid = function () {
        var gridAttr = colorToAttr(PALETTE.SKY_GRID);
        var glowAttr = colorToAttr(PALETTE.SKY_GRID_GLOW);
        var vanishX = 40;
        var screenWidth = 80;
        for (var y = this.horizonY - 1; y >= 1; y--) {
            var distFromHorizon = this.horizonY - y;
            var spread = distFromHorizon * 6;
            for (var offset = 0; offset <= 40; offset += 8) {
                if (offset <= spread) {
                    var leftX = vanishX - offset;
                    var rightX = vanishX + offset;
                    if (offset === 0) {
                        this.composer.setCell(vanishX, y, GLYPH.BOX_V, gridAttr);
                    }
                    else {
                        if (leftX >= 0 && leftX < screenWidth) {
                            this.composer.setCell(leftX, y, '/', glowAttr);
                        }
                        if (rightX >= 0 && rightX < screenWidth) {
                            this.composer.setCell(rightX, y, '\\', glowAttr);
                        }
                    }
                }
            }
            var lineSpacing = 3;
            var scanlinePhase = (this.gridAnimPhase * lineSpacing + (this.horizonY - y) * 0.3) % 1;
            if (scanlinePhase < 0.33) {
                var lineSpread = Math.min(spread, 39);
                for (var x = vanishX - lineSpread; x <= vanishX + lineSpread; x++) {
                    if (x >= 0 && x < screenWidth) {
                        var buffer = this.composer.getBuffer();
                        var cell = buffer[y][x];
                        if (cell.char === ' ') {
                            this.composer.setCell(x, y, GLYPH.BOX_H, glowAttr);
                        }
                    }
                }
            }
        }
    };
    return SkylineRenderer;
}());
"use strict";
var SpriteRenderer = (function () {
    function SpriteRenderer(composer, horizonY) {
        this.composer = composer;
        this.horizonY = horizonY;
        this.roadBottom = 23;
    }
    SpriteRenderer.prototype.renderPlayerVehicle = function (steerOffset) {
        var baseY = 20;
        var baseX = 38 + Math.round(steerOffset * 2);
        var bodyAttr = colorToAttr(PALETTE.PLAYER_BODY);
        var trimAttr = colorToAttr(PALETTE.PLAYER_TRIM);
        this.composer.setCell(baseX, baseY, GLYPH.LOWER_HALF, bodyAttr);
        this.composer.setCell(baseX + 1, baseY, GLYPH.FULL_BLOCK, trimAttr);
        this.composer.setCell(baseX + 2, baseY, GLYPH.LOWER_HALF, bodyAttr);
        this.composer.setCell(baseX - 1, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
        this.composer.setCell(baseX, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
        this.composer.setCell(baseX + 1, baseY + 1, GLYPH.FULL_BLOCK, trimAttr);
        this.composer.setCell(baseX + 2, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
        this.composer.setCell(baseX + 3, baseY + 1, GLYPH.FULL_BLOCK, bodyAttr);
        this.composer.setCell(baseX - 1, baseY + 2, GLYPH.UPPER_HALF, bodyAttr);
        this.composer.setCell(baseX, baseY + 2, GLYPH.LOWER_HALF, bodyAttr);
        this.composer.setCell(baseX + 1, baseY + 2, GLYPH.FULL_BLOCK, trimAttr);
        this.composer.setCell(baseX + 2, baseY + 2, GLYPH.LOWER_HALF, bodyAttr);
        this.composer.setCell(baseX + 3, baseY + 2, GLYPH.UPPER_HALF, bodyAttr);
    };
    SpriteRenderer.prototype.renderOtherVehicle = function (relativeZ, relativeX, color) {
        if (relativeZ < 5 || relativeZ > 200)
            return;
        var t = Math.max(0, Math.min(1, 1 - (relativeZ / 200)));
        var screenY = Math.round(this.horizonY + t * (this.roadBottom - this.horizonY - 3));
        var scale = t;
        var screenX = 40 + Math.round(relativeX * scale * 2);
        if (screenY < this.horizonY || screenY > this.roadBottom - 3)
            return;
        var attr = makeAttr(color, BG_BLACK);
        if (scale > 0.5) {
            this.composer.setCell(screenX, screenY, GLYPH.UPPER_HALF, attr);
            this.composer.setCell(screenX + 1, screenY, GLYPH.UPPER_HALF, attr);
            this.composer.setCell(screenX, screenY + 1, GLYPH.FULL_BLOCK, attr);
            this.composer.setCell(screenX + 1, screenY + 1, GLYPH.FULL_BLOCK, attr);
        }
        else if (scale > 0.25) {
            this.composer.setCell(screenX, screenY, GLYPH.DARK_SHADE, attr);
            this.composer.setCell(screenX, screenY + 1, GLYPH.FULL_BLOCK, attr);
        }
        else {
            this.composer.setCell(screenX, screenY, GLYPH.MEDIUM_SHADE, attr);
        }
    };
    SpriteRenderer.prototype.renderItemBox = function (relativeZ, relativeX) {
        if (relativeZ < 5 || relativeZ > 200)
            return;
        var t = Math.max(0, Math.min(1, 1 - (relativeZ / 200)));
        var screenY = Math.round(this.horizonY + t * (this.roadBottom - this.horizonY - 2));
        var screenX = 40 + Math.round(relativeX * t * 2);
        if (screenY < this.horizonY || screenY > this.roadBottom - 2)
            return;
        var attr = colorToAttr(PALETTE.ITEM_BOX);
        this.composer.setCell(screenX, screenY, '?', attr);
    };
    return SpriteRenderer;
}());
"use strict";
var HudRenderer = (function () {
    function HudRenderer(composer) {
        this.composer = composer;
    }
    HudRenderer.prototype.render = function (hudData) {
        this.renderTopBar(hudData);
        this.renderLapProgress(hudData);
        this.renderSpeedometer(hudData);
        this.renderItemSlot(hudData);
        if (hudData.countdown > 0 && hudData.raceMode === RaceMode.GRAND_PRIX) {
            this.renderCountdown(hudData.countdown);
        }
    };
    HudRenderer.prototype.renderTopBar = function (data) {
        var y = 0;
        var valueAttr = colorToAttr(PALETTE.HUD_VALUE);
        var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
        for (var x = 0; x < 80; x++) {
            this.composer.setCell(x, y, ' ', makeAttr(BLACK, BG_BLACK));
        }
        this.composer.writeString(2, y, "LAP", labelAttr);
        this.composer.writeString(6, y, data.lap + "/" + data.totalLaps, valueAttr);
        this.composer.writeString(14, y, "POS", labelAttr);
        var posSuffix = PositionIndicator.getOrdinalSuffix(data.position);
        this.composer.writeString(18, y, data.position + posSuffix, valueAttr);
        this.composer.writeString(26, y, "TIME", labelAttr);
        this.composer.writeString(31, y, LapTimer.format(data.lapTime), valueAttr);
        if (data.bestLapTime > 0) {
            this.composer.writeString(45, y, "BEST", labelAttr);
            this.composer.writeString(50, y, LapTimer.format(data.bestLapTime), valueAttr);
        }
        this.composer.writeString(66, y, "SPD", labelAttr);
        var speedStr = this.padLeft(data.speed.toString(), 3);
        this.composer.writeString(70, y, speedStr, valueAttr);
    };
    HudRenderer.prototype.renderLapProgress = function (data) {
        var y = 1;
        var barX = 2;
        var barWidth = 60;
        var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
        var filledAttr = colorToAttr({ fg: CYAN, bg: BG_BLACK });
        var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
        var finishAttr = colorToAttr({ fg: WHITE, bg: BG_MAGENTA });
        for (var x = 0; x < 80; x++) {
            this.composer.setCell(x, y, ' ', makeAttr(BLACK, BG_BLACK));
        }
        var pct = Math.floor(data.lapProgress * 100);
        var pctStr = this.padLeft(pct.toString(), 3) + "%";
        this.composer.writeString(barX, y, "TRACK", labelAttr);
        this.composer.writeString(barX + 6, y, pctStr, colorToAttr(PALETTE.HUD_VALUE));
        var barStartX = barX + 12;
        this.composer.setCell(barStartX, y, '[', labelAttr);
        var fillWidth = Math.floor(data.lapProgress * barWidth);
        for (var i = 0; i < barWidth; i++) {
            var isFinish = (i === barWidth - 1);
            var attr;
            var char;
            if (isFinish) {
                attr = finishAttr;
                char = GLYPH.CHECKER;
            }
            else if (i < fillWidth) {
                attr = filledAttr;
                char = GLYPH.FULL_BLOCK;
            }
            else {
                attr = emptyAttr;
                char = GLYPH.LIGHT_SHADE;
            }
            this.composer.setCell(barStartX + 1 + i, y, char, attr);
        }
        this.composer.setCell(barStartX + barWidth + 1, y, ']', labelAttr);
        this.composer.writeString(barStartX + barWidth + 3, y, "FINISH", finishAttr);
    };
    HudRenderer.prototype.renderSpeedometer = function (data) {
        var y = 23;
        var barX = 2;
        var barWidth = 20;
        var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
        var filledAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
        var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
        var highAttr = colorToAttr({ fg: LIGHTRED, bg: BG_BLACK });
        this.composer.writeString(barX, y, "[", labelAttr);
        var fillAmount = data.speed / data.speedMax;
        var fillWidth = Math.round(fillAmount * barWidth);
        for (var i = 0; i < barWidth; i++) {
            var attr = i < fillWidth ?
                (fillAmount > 0.8 ? highAttr : filledAttr) :
                emptyAttr;
            var char = i < fillWidth ? GLYPH.FULL_BLOCK : GLYPH.LIGHT_SHADE;
            this.composer.setCell(barX + 1 + i, y, char, attr);
        }
        this.composer.writeString(barX + barWidth + 1, y, "]", labelAttr);
    };
    HudRenderer.prototype.renderItemSlot = function (data) {
        var bottomY = 23;
        var rightEdge = 79;
        if (data.heldItem === null) {
            var emptyAttr = colorToAttr(PALETTE.HUD_LABEL);
            this.composer.writeString(rightEdge - 3, bottomY, "----", emptyAttr);
            return;
        }
        var itemType = data.heldItem.type;
        var uses = data.heldItem.uses;
        var icon = this.getItemIcon(itemType);
        var iconWidth = icon.lines[0].length;
        var iconHeight = icon.lines.length;
        var iconX = rightEdge - iconWidth + 1;
        var iconY = bottomY - iconHeight + 1;
        for (var row = 0; row < iconHeight; row++) {
            var line = icon.lines[row];
            for (var col = 0; col < line.length; col++) {
                var ch = line.charAt(col);
                if (ch !== ' ') {
                    var attr = this.getIconCharAttr(ch, icon, itemType);
                    this.composer.setCell(iconX + col, iconY + row, ch, attr);
                }
            }
        }
        if (uses > 1) {
            var countAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
            this.composer.setCell(iconX - 2, bottomY, 'x', colorToAttr(PALETTE.HUD_LABEL));
            this.composer.setCell(iconX - 1, bottomY, String(uses).charAt(0), countAttr);
        }
    };
    HudRenderer.prototype.getItemIcon = function (type) {
        switch (type) {
            case ItemType.MUSHROOM:
            case ItemType.MUSHROOM_TRIPLE:
                return {
                    lines: [
                        ' @@ ',
                        '@##@',
                        ' || '
                    ],
                    color: { fg: LIGHTRED, bg: BG_BLACK },
                    altColor: { fg: WHITE, bg: BG_BLACK }
                };
            case ItemType.MUSHROOM_GOLDEN:
                return {
                    lines: [
                        ' @@ ',
                        '@##@',
                        ' || '
                    ],
                    color: { fg: YELLOW, bg: BG_BLACK },
                    altColor: { fg: WHITE, bg: BG_BLACK }
                };
            case ItemType.GREEN_SHELL:
            case ItemType.GREEN_SHELL_TRIPLE:
                return {
                    lines: [
                        ' /^\\ ',
                        '(O O)',
                        ' \\_/ '
                    ],
                    color: { fg: LIGHTGREEN, bg: BG_BLACK }
                };
            case ItemType.RED_SHELL:
            case ItemType.RED_SHELL_TRIPLE:
            case ItemType.SHELL:
            case ItemType.SHELL_TRIPLE:
                return {
                    lines: [
                        ' /^\\ ',
                        '(O O)',
                        ' \\_/ '
                    ],
                    color: { fg: LIGHTRED, bg: BG_BLACK }
                };
            case ItemType.BLUE_SHELL:
                return {
                    lines: [
                        ' ~*~ ',
                        '<(@)>',
                        ' \\_/ '
                    ],
                    color: { fg: LIGHTBLUE, bg: BG_BLACK },
                    altColor: { fg: LIGHTCYAN, bg: BG_BLACK }
                };
            case ItemType.BANANA:
            case ItemType.BANANA_TRIPLE:
                return {
                    lines: [
                        '  /\\ ',
                        ' (  )',
                        '  \\/ '
                    ],
                    color: { fg: YELLOW, bg: BG_BLACK }
                };
            case ItemType.STAR:
                return {
                    lines: [
                        '  *  ',
                        ' *** ',
                        '*****',
                        ' * * '
                    ],
                    color: { fg: YELLOW, bg: BG_BLACK }
                };
            case ItemType.LIGHTNING:
                return {
                    lines: [
                        ' /| ',
                        '/-\' ',
                        '|/  '
                    ],
                    color: { fg: YELLOW, bg: BG_BLACK },
                    altColor: { fg: LIGHTCYAN, bg: BG_BLACK }
                };
            case ItemType.BULLET:
                return {
                    lines: [
                        ' __ ',
                        '|==>',
                        ' -- '
                    ],
                    color: { fg: WHITE, bg: BG_BLACK },
                    altColor: { fg: DARKGRAY, bg: BG_BLACK }
                };
            default:
                return {
                    lines: [
                        ' ? '
                    ],
                    color: { fg: YELLOW, bg: BG_BLACK }
                };
        }
    };
    HudRenderer.prototype.getIconCharAttr = function (ch, icon, itemType) {
        var useAlt = false;
        switch (itemType) {
            case ItemType.MUSHROOM:
            case ItemType.MUSHROOM_TRIPLE:
            case ItemType.MUSHROOM_GOLDEN:
                useAlt = (ch === '#' || ch === '|');
                break;
            case ItemType.BLUE_SHELL:
                useAlt = (ch === '<' || ch === '>' || ch === '~');
                break;
            case ItemType.LIGHTNING:
                useAlt = (Math.floor(Date.now() / 150) % 2 === 0);
                break;
            case ItemType.STAR:
                var starColors = [YELLOW, LIGHTRED, LIGHTGREEN, LIGHTCYAN, LIGHTMAGENTA, WHITE];
                var colorIdx = Math.floor(Date.now() / 100) % starColors.length;
                return makeAttr(starColors[colorIdx], BG_BLACK);
            case ItemType.BULLET:
                useAlt = (ch !== '=' && ch !== '>');
                break;
        }
        if (useAlt && icon.altColor) {
            return makeAttr(icon.altColor.fg, icon.altColor.bg);
        }
        return makeAttr(icon.color.fg, icon.color.bg);
    };
    HudRenderer.prototype.padLeft = function (str, len) {
        while (str.length < len) {
            str = ' ' + str;
        }
        return str;
    };
    HudRenderer.prototype.renderCountdown = function (countdown) {
        var countNum = Math.ceil(countdown);
        var centerX = 40;
        var topY = 8;
        var frameAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
        var poleAttr = colorToAttr({ fg: BROWN, bg: BG_BLACK });
        var redOn = countNum >= 3;
        var yellowOn = countNum === 2;
        var greenOn = countNum <= 1;
        var redAttr = redOn ? colorToAttr({ fg: LIGHTRED, bg: BG_RED }) : colorToAttr({ fg: RED, bg: BG_BLACK });
        var yellowAttr = yellowOn ? colorToAttr({ fg: YELLOW, bg: BG_BROWN }) : colorToAttr({ fg: BROWN, bg: BG_BLACK });
        var greenAttr = greenOn ? colorToAttr({ fg: LIGHTGREEN, bg: BG_GREEN }) : colorToAttr({ fg: GREEN, bg: BG_BLACK });
        var boxX = centerX - 3;
        this.composer.setCell(boxX, topY, GLYPH.DBOX_TL, frameAttr);
        for (var i = 1; i < 6; i++) {
            this.composer.setCell(boxX + i, topY, GLYPH.DBOX_H, frameAttr);
        }
        this.composer.setCell(boxX + 6, topY, GLYPH.DBOX_TR, frameAttr);
        this.composer.setCell(boxX, topY + 1, GLYPH.DBOX_V, frameAttr);
        this.composer.writeString(boxX + 1, topY + 1, " ", frameAttr);
        this.composer.setCell(boxX + 2, topY + 1, GLYPH.FULL_BLOCK, redAttr);
        this.composer.setCell(boxX + 3, topY + 1, GLYPH.FULL_BLOCK, redAttr);
        this.composer.setCell(boxX + 4, topY + 1, GLYPH.FULL_BLOCK, redAttr);
        this.composer.writeString(boxX + 5, topY + 1, " ", frameAttr);
        this.composer.setCell(boxX + 6, topY + 1, GLYPH.DBOX_V, frameAttr);
        this.composer.setCell(boxX, topY + 2, GLYPH.DBOX_V, frameAttr);
        this.composer.writeString(boxX + 1, topY + 2, "     ", frameAttr);
        this.composer.setCell(boxX + 6, topY + 2, GLYPH.DBOX_V, frameAttr);
        this.composer.setCell(boxX, topY + 3, GLYPH.DBOX_V, frameAttr);
        this.composer.writeString(boxX + 1, topY + 3, " ", frameAttr);
        this.composer.setCell(boxX + 2, topY + 3, GLYPH.FULL_BLOCK, yellowAttr);
        this.composer.setCell(boxX + 3, topY + 3, GLYPH.FULL_BLOCK, yellowAttr);
        this.composer.setCell(boxX + 4, topY + 3, GLYPH.FULL_BLOCK, yellowAttr);
        this.composer.writeString(boxX + 5, topY + 3, " ", frameAttr);
        this.composer.setCell(boxX + 6, topY + 3, GLYPH.DBOX_V, frameAttr);
        this.composer.setCell(boxX, topY + 4, GLYPH.DBOX_V, frameAttr);
        this.composer.writeString(boxX + 1, topY + 4, "     ", frameAttr);
        this.composer.setCell(boxX + 6, topY + 4, GLYPH.DBOX_V, frameAttr);
        this.composer.setCell(boxX, topY + 5, GLYPH.DBOX_V, frameAttr);
        this.composer.writeString(boxX + 1, topY + 5, " ", frameAttr);
        this.composer.setCell(boxX + 2, topY + 5, GLYPH.FULL_BLOCK, greenAttr);
        this.composer.setCell(boxX + 3, topY + 5, GLYPH.FULL_BLOCK, greenAttr);
        this.composer.setCell(boxX + 4, topY + 5, GLYPH.FULL_BLOCK, greenAttr);
        this.composer.writeString(boxX + 5, topY + 5, " ", frameAttr);
        this.composer.setCell(boxX + 6, topY + 5, GLYPH.DBOX_V, frameAttr);
        this.composer.setCell(boxX, topY + 6, GLYPH.DBOX_BL, frameAttr);
        for (var j = 1; j < 6; j++) {
            this.composer.setCell(boxX + j, topY + 6, GLYPH.DBOX_H, frameAttr);
        }
        this.composer.setCell(boxX + 6, topY + 6, GLYPH.DBOX_BR, frameAttr);
        this.composer.setCell(centerX, topY + 7, GLYPH.DBOX_V, poleAttr);
        this.composer.setCell(centerX, topY + 8, GLYPH.DBOX_V, poleAttr);
        if (greenOn && countNum <= 0) {
            var goAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
            this.composer.writeString(centerX - 2, topY + 9, "GO!!!", goAttr);
        }
    };
    return HudRenderer;
}());
"use strict";
var ANSILoader = (function () {
    function ANSILoader() {
    }
    ANSILoader.ensureGraphicLoaded = function () {
        if (!ANSILoader._graphicLoaded) {
            try {
                load('graphic.js');
                ANSILoader._graphicLoaded = true;
                logInfo("ANSILoader: graphic.js loaded successfully");
            }
            catch (e) {
                logWarning("ANSILoader: Failed to load graphic.js - " + e);
            }
        }
    };
    ANSILoader.load = function (pathOrFilename, optDirectory) {
        ANSILoader.ensureGraphicLoaded();
        var path;
        if (pathOrFilename.indexOf('/') >= 0 || pathOrFilename.indexOf('\\') >= 0) {
            path = pathOrFilename;
        }
        else {
            var dir = optDirectory || ANSILoader.defaultDirectory;
            path = dir + '/' + pathOrFilename;
        }
        try {
            if (typeof file_exists === 'function' && !file_exists(path)) {
                logWarning("ANSI file not found: " + path);
                return null;
            }
            var graphic = new Graphic(80, 1);
            graphic.auto_extend = true;
            if (!graphic.load(path)) {
                logWarning("Failed to load ANSI file via Graphic: " + path);
                return null;
            }
            var width = graphic.width;
            var height = graphic.height;
            var cells = [];
            for (var row = 0; row < height; row++) {
                cells[row] = [];
                for (var col = 0; col < width; col++) {
                    var cell = graphic.data[col][row];
                    cells[row][col] = {
                        char: cell.ch || ' ',
                        attr: cell.attr || 7
                    };
                }
            }
            logInfo("ANSILoader: Loaded " + path + " (" + width + "x" + height + " actual size)");
            return {
                width: width,
                height: height,
                cells: cells
            };
        }
        catch (e) {
            logWarning("Error loading ANSI file: " + path + " - " + e);
            return null;
        }
    };
    ANSILoader.scanDirectory = function (dirPath) {
        var dir = dirPath || ANSILoader.defaultDirectory;
        var files = [];
        try {
            if (typeof directory === 'function') {
                var glob = dir + '/*.ans';
                var allFiles = directory(glob);
                if (allFiles && allFiles.length) {
                    for (var i = 0; i < allFiles.length; i++) {
                        files.push(allFiles[i]);
                    }
                }
            }
            logInfo("ANSILoader.scanDirectory: Found " + files.length + " files in " + dir);
        }
        catch (e) {
            logWarning("Error scanning ANSI directory: " + dir + " - " + e);
        }
        return files;
    };
    ANSILoader.defaultDirectory = '/sbbs/xtrn/outrun/ansi_art';
    ANSILoader._graphicLoaded = false;
    return ANSILoader;
}());
"use strict";
var ThemeRegistry = {};
var ROADSIDE_SPRITES = {};
function registerTheme(theme) {
    ThemeRegistry[theme.name] = theme;
}
function registerRoadsideSprite(name, creator) {
    ROADSIDE_SPRITES[name] = creator;
}
function getTheme(name) {
    return ThemeRegistry[name] || null;
}
function getThemeNames() {
    var names = [];
    for (var key in ThemeRegistry) {
        if (ThemeRegistry.hasOwnProperty(key)) {
            names.push(key);
        }
    }
    return names;
}
"use strict";
var CitySprites = {
    createBuilding: function () {
        var wall = makeAttr(DARKGRAY, BG_BLACK);
        var window = makeAttr(YELLOW, BG_BLACK);
        var windowDark = makeAttr(DARKGRAY, BG_BLACK);
        var roof = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'building',
            variants: [
                [
                    [{ char: GLYPH.UPPER_HALF, attr: roof }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }],
                    [{ char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }],
                    [{ char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: windowDark }, { char: GLYPH.FULL_BLOCK, attr: wall }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }],
                    [{ char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: windowDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.UPPER_HALF, attr: roof }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }],
                    [{ char: '.', attr: windowDark }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: window }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: '.', attr: windowDark }, { char: GLYPH.FULL_BLOCK, attr: wall }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }, { char: GLYPH.FULL_BLOCK, attr: wall }]
                ]
            ]
        };
    },
    createLamppost: function () {
        var pole = makeAttr(DARKGRAY, BG_BLACK);
        var light = makeAttr(YELLOW, BG_BLACK);
        var lightBright = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'lamppost',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: light }]
                ],
                [
                    [{ char: '*', attr: light }],
                    [{ char: GLYPH.BOX_V, attr: pole }]
                ],
                [
                    [{ char: '*', attr: lightBright }, { char: '*', attr: light }],
                    [U, { char: GLYPH.BOX_V, attr: pole }],
                    [U, { char: GLYPH.BOX_V, attr: pole }]
                ],
                [
                    [{ char: GLYPH.DARK_SHADE, attr: light }, { char: '*', attr: lightBright }],
                    [U, { char: GLYPH.BOX_V, attr: pole }],
                    [U, { char: GLYPH.BOX_V, attr: pole }],
                    [U, { char: GLYPH.BOX_V, attr: pole }]
                ],
                [
                    [{ char: GLYPH.DARK_SHADE, attr: light }, { char: '*', attr: lightBright }, { char: GLYPH.DARK_SHADE, attr: light }],
                    [U, { char: GLYPH.BOX_V, attr: pole }, U],
                    [U, { char: GLYPH.BOX_V, attr: pole }, U],
                    [U, { char: GLYPH.BOX_V, attr: pole }, U],
                    [U, { char: GLYPH.BOX_V, attr: pole }, U]
                ]
            ]
        };
    },
    createSign: function () {
        var sign = makeAttr(GREEN, BG_BLACK);
        var signBright = makeAttr(LIGHTGREEN, BG_BLACK);
        var text = makeAttr(WHITE, BG_GREEN);
        var pole = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'sign',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: sign }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: GLYPH.FULL_BLOCK, attr: sign }],
                    [U, { char: GLYPH.BOX_V, attr: pole }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: '-', attr: text }, { char: GLYPH.FULL_BLOCK, attr: sign }],
                    [U, { char: GLYPH.BOX_V, attr: pole }, U]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: 'E', attr: text }, { char: 'X', attr: text }, { char: GLYPH.FULL_BLOCK, attr: sign }],
                    [{ char: GLYPH.FULL_BLOCK, attr: signBright }, { char: 'I', attr: text }, { char: 'T', attr: text }, { char: GLYPH.FULL_BLOCK, attr: signBright }],
                    [U, { char: GLYPH.BOX_V, attr: pole }, { char: GLYPH.BOX_V, attr: pole }, U]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: sign }, { char: 'E', attr: text }, { char: 'X', attr: text }, { char: 'I', attr: text }, { char: GLYPH.FULL_BLOCK, attr: sign }],
                    [{ char: GLYPH.FULL_BLOCK, attr: signBright }, { char: ' ', attr: text }, { char: '1', attr: text }, { char: ' ', attr: text }, { char: GLYPH.FULL_BLOCK, attr: signBright }],
                    [U, U, { char: GLYPH.BOX_V, attr: pole }, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('building', CitySprites.createBuilding);
registerRoadsideSprite('lamppost', CitySprites.createLamppost);
registerRoadsideSprite('sign', CitySprites.createSign);
"use strict";
var BeachSprites = {
    createPalm: function () {
        var frond = makeAttr(LIGHTGREEN, BG_BLACK);
        var frondDark = makeAttr(GREEN, BG_BLACK);
        var trunk = makeAttr(BROWN, BG_BLACK);
        var coconut = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'palm',
            variants: [
                [
                    [{ char: 'Y', attr: frond }],
                    [{ char: '|', attr: trunk }],
                    [{ char: '|', attr: trunk }]
                ],
                [
                    [{ char: '\\', attr: frondDark }, { char: '|', attr: frond }, { char: '/', attr: frondDark }],
                    [U, { char: '|', attr: trunk }, U],
                    [U, { char: '|', attr: trunk }, U],
                    [U, { char: '|', attr: trunk }, U]
                ],
                [
                    [{ char: '~', attr: frondDark }, { char: '\\', attr: frond }, { char: '|', attr: frond }, { char: '/', attr: frond }, { char: '~', attr: frondDark }],
                    [U, { char: '\\', attr: frond }, { char: 'o', attr: coconut }, { char: '/', attr: frond }, U],
                    [U, U, { char: '|', attr: trunk }, U, U],
                    [U, U, { char: ')', attr: trunk }, U, U],
                    [U, U, { char: '|', attr: trunk }, U, U]
                ],
                [
                    [{ char: '/', attr: frondDark }, { char: '~', attr: frond }, U, U, U, { char: '~', attr: frond }, { char: '\\', attr: frondDark }],
                    [U, { char: '\\', attr: frond }, { char: '_', attr: frond }, { char: 'Y', attr: frond }, { char: '_', attr: frond }, { char: '/', attr: frond }, U],
                    [U, U, { char: '\\', attr: frondDark }, { char: '|', attr: trunk }, { char: '/', attr: frondDark }, U, U],
                    [U, U, U, { char: '(', attr: trunk }, U, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, { char: ')', attr: trunk }, U, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U]
                ],
                [
                    [{ char: '/', attr: frondDark }, { char: '~', attr: frond }, { char: '~', attr: frondDark }, U, U, U, { char: '~', attr: frondDark }, { char: '~', attr: frond }, { char: '\\', attr: frondDark }],
                    [U, { char: '\\', attr: frond }, { char: '_', attr: frond }, { char: '|', attr: frond }, { char: 'o', attr: coconut }, { char: '|', attr: frond }, { char: '_', attr: frond }, { char: '/', attr: frond }, U],
                    [U, U, { char: '\\', attr: frondDark }, { char: '\\', attr: frond }, { char: '|', attr: trunk }, { char: '/', attr: frond }, { char: '/', attr: frondDark }, U, U],
                    [U, U, U, U, { char: '|', attr: trunk }, { char: ')', attr: trunk }, U, U, U],
                    [U, U, U, U, { char: '(', attr: trunk }, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, U, { char: '|', attr: trunk }, { char: ')', attr: trunk }, U, U, U],
                    [U, U, U, U, { char: '(', attr: trunk }, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, U, { char: '|', attr: trunk }, { char: ')', attr: trunk }, U, U, U],
                    [U, U, U, U, { char: '|', attr: trunk }, { char: '|', attr: trunk }, U, U, U]
                ]
            ]
        };
    },
    createUmbrella: function () {
        var stripe1 = makeAttr(LIGHTRED, BG_BLACK);
        var stripe2 = makeAttr(YELLOW, BG_BLACK);
        var pole = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'umbrella',
            variants: [
                [
                    [{ char: '/', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '\\', attr: stripe1 }],
                    [U, { char: '|', attr: pole }, U]
                ],
                [
                    [U, { char: '/', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '\\', attr: stripe1 }, U],
                    [U, U, { char: '|', attr: pole }, U, U],
                    [U, U, { char: '+', attr: pole }, U, U]
                ],
                [
                    [U, { char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }, U],
                    [U, U, U, { char: '|', attr: pole }, U, U, U],
                    [U, U, U, { char: '|', attr: pole }, U, U, U],
                    [U, U, U, { char: '+', attr: pole }, U, U, U]
                ],
                [
                    [U, { char: '_', attr: stripe1 }, { char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }, { char: '_', attr: stripe1 }, U],
                    [{ char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '|', attr: pole }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }],
                    [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
                    [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
                    [U, U, U, U, { char: '+', attr: pole }, U, U, U, U]
                ],
                [
                    [U, { char: '_', attr: stripe1 }, { char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '^', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }, { char: '_', attr: stripe1 }, U],
                    [{ char: '/', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '|', attr: pole }, { char: '=', attr: stripe1 }, { char: '=', attr: stripe2 }, { char: '=', attr: stripe1 }, { char: '\\', attr: stripe2 }],
                    [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
                    [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
                    [U, U, U, U, { char: '|', attr: pole }, U, U, U, U],
                    [U, U, U, U, { char: '+', attr: pole }, U, U, U, U]
                ]
            ]
        };
    },
    createLifeguard: function () {
        var red = makeAttr(LIGHTRED, BG_BLACK);
        var white = makeAttr(WHITE, BG_BLACK);
        var wood = makeAttr(BROWN, BG_BLACK);
        var window = makeAttr(LIGHTCYAN, BG_CYAN);
        var U = null;
        return {
            name: 'lifeguard',
            variants: [
                [
                    [{ char: '[', attr: red }, { char: ']', attr: red }],
                    [{ char: '/', attr: wood }, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, { char: '\\', attr: wood }]
                ],
                [
                    [{ char: '[', attr: red }, { char: '#', attr: white }, { char: ']', attr: red }],
                    [{ char: '/', attr: wood }, { char: '_', attr: wood }, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, U, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, U, { char: '\\', attr: wood }]
                ],
                [
                    [U, { char: '[', attr: red }, { char: '=', attr: white }, { char: ']', attr: red }, U],
                    [U, { char: '|', attr: white }, { char: ' ', attr: window }, { char: '|', attr: white }, U],
                    [{ char: '/', attr: wood }, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, U, U, U, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, U, U, U, { char: '\\', attr: wood }]
                ],
                [
                    [U, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, U],
                    [{ char: '/', attr: white }, { char: '[', attr: red }, { char: '=', attr: white }, { char: '=', attr: white }, { char: '=', attr: white }, { char: ']', attr: red }, { char: '\\', attr: white }],
                    [U, { char: '|', attr: white }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: ' ', attr: red }, { char: '|', attr: white }, U],
                    [{ char: '/', attr: wood }, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, U, U, U, U, U, { char: '\\', attr: wood }],
                    [{ char: '/', attr: wood }, U, U, U, U, U, { char: '\\', attr: wood }]
                ],
                [
                    [U, U, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, { char: '_', attr: red }, U, U],
                    [U, { char: '/', attr: white }, { char: '[', attr: red }, { char: '=', attr: white }, { char: '=', attr: white }, { char: '=', attr: white }, { char: ']', attr: red }, { char: '\\', attr: white }, U],
                    [U, { char: '|', attr: red }, { char: '_', attr: white }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: '_', attr: white }, { char: '|', attr: red }, U, U],
                    [U, { char: '|', attr: white }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: ' ', attr: red }, { char: ' ', attr: red }, { char: '|', attr: white }, U, U],
                    [{ char: '/', attr: wood }, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: wood }, U],
                    [{ char: '/', attr: wood }, U, U, U, U, U, U, { char: '\\', attr: wood }, U],
                    [{ char: '/', attr: wood }, U, U, U, U, U, U, { char: '\\', attr: wood }, U],
                    [{ char: '/', attr: wood }, U, U, U, U, U, U, { char: '\\', attr: wood }, U]
                ]
            ]
        };
    },
    createSurfboard: function () {
        var board = makeAttr(LIGHTCYAN, BG_BLACK);
        var accent = makeAttr(CYAN, BG_BLACK);
        var star = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'surfboard',
            variants: [
                [
                    [{ char: '^', attr: board }],
                    [{ char: '|', attr: board }],
                    [{ char: 'v', attr: accent }]
                ],
                [
                    [{ char: '^', attr: board }],
                    [{ char: '|', attr: board }],
                    [{ char: '|', attr: accent }],
                    [{ char: 'v', attr: board }]
                ],
                [
                    [{ char: '/', attr: board }, { char: '\\', attr: board }],
                    [{ char: '|', attr: board }, { char: '|', attr: accent }],
                    [{ char: '|', attr: accent }, { char: '|', attr: board }],
                    [{ char: '|', attr: board }, { char: '|', attr: accent }],
                    [{ char: '\\', attr: accent }, { char: '/', attr: accent }]
                ],
                [
                    [U, { char: '^', attr: board }, U],
                    [{ char: '/', attr: board }, { char: '~', attr: accent }, { char: '\\', attr: board }],
                    [{ char: '|', attr: board }, { char: '*', attr: star }, { char: '|', attr: accent }],
                    [{ char: '|', attr: accent }, { char: ' ', attr: board }, { char: '|', attr: board }],
                    [{ char: '|', attr: board }, { char: ' ', attr: board }, { char: '|', attr: accent }],
                    [{ char: '\\', attr: accent }, { char: '_', attr: board }, { char: '/', attr: accent }]
                ],
                [
                    [U, { char: '/', attr: board }, { char: '\\', attr: board }, U],
                    [{ char: '/', attr: board }, { char: '~', attr: accent }, { char: '~', attr: board }, { char: '\\', attr: board }],
                    [{ char: '|', attr: board }, { char: ' ', attr: accent }, { char: ' ', attr: accent }, { char: '|', attr: accent }],
                    [{ char: '|', attr: accent }, { char: '*', attr: star }, { char: ' ', attr: board }, { char: '|', attr: board }],
                    [{ char: '|', attr: board }, { char: ' ', attr: board }, { char: ' ', attr: board }, { char: '|', attr: accent }],
                    [{ char: '|', attr: accent }, { char: ' ', attr: board }, { char: ' ', attr: board }, { char: '|', attr: board }],
                    [{ char: '\\', attr: board }, { char: '_', attr: accent }, { char: '_', attr: accent }, { char: '/', attr: board }],
                    [U, { char: '\\', attr: accent }, { char: '/', attr: accent }, U]
                ]
            ]
        };
    },
    createTiki: function () {
        var flame = makeAttr(YELLOW, BG_BLACK);
        var flameOrange = makeAttr(LIGHTRED, BG_BLACK);
        var bamboo = makeAttr(BROWN, BG_BLACK);
        var band = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'tiki',
            variants: [
                [
                    [{ char: '*', attr: flame }],
                    [{ char: '#', attr: bamboo }],
                    [{ char: '|', attr: bamboo }]
                ],
                [
                    [U, { char: '*', attr: flame }, U],
                    [U, { char: '#', attr: bamboo }, U],
                    [U, { char: '|', attr: bamboo }, U],
                    [U, { char: '|', attr: bamboo }, U]
                ],
                [
                    [{ char: '\\', attr: flame }, { char: '*', attr: flameOrange }, { char: '/', attr: flame }],
                    [U, { char: '#', attr: bamboo }, U],
                    [U, { char: '|', attr: bamboo }, U],
                    [U, { char: '=', attr: band }, U],
                    [U, { char: '|', attr: bamboo }, U]
                ],
                [
                    [U, { char: '(', attr: flame }, { char: '*', attr: flameOrange }, { char: ')', attr: flame }, U],
                    [U, { char: '\\', attr: flameOrange }, { char: '|', attr: flame }, { char: '/', attr: flameOrange }, U],
                    [U, { char: '[', attr: bamboo }, { char: '#', attr: bamboo }, { char: ']', attr: bamboo }, U],
                    [U, U, { char: '|', attr: bamboo }, U, U],
                    [U, U, { char: '=', attr: band }, U, U],
                    [U, U, { char: '|', attr: bamboo }, U, U],
                    [U, U, { char: '|', attr: bamboo }, U, U]
                ],
                [
                    [U, { char: '(', attr: flame }, { char: '*', attr: flameOrange }, { char: ')', attr: flame }, U],
                    [U, { char: '\\', attr: flameOrange }, { char: '|', attr: flame }, { char: '/', attr: flameOrange }, U],
                    [U, { char: '[', attr: bamboo }, { char: '#', attr: bamboo }, { char: ']', attr: bamboo }, U],
                    [U, U, { char: '|', attr: bamboo }, U, U],
                    [U, U, { char: '=', attr: band }, U, U],
                    [U, U, { char: '|', attr: bamboo }, U, U],
                    [U, U, { char: '=', attr: band }, U, U],
                    [U, U, { char: '|', attr: bamboo }, U, U]
                ]
            ]
        };
    },
    createBeachhut: function () {
        var thatch = makeAttr(YELLOW, BG_BLACK);
        var thatchDark = makeAttr(BROWN, BG_BLACK);
        var wood = makeAttr(BROWN, BG_BLACK);
        var window = makeAttr(LIGHTCYAN, BG_CYAN);
        var U = null;
        return {
            name: 'beachhut',
            variants: [
                [
                    [{ char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '\\', attr: thatch }],
                    [{ char: '[', attr: wood }, { char: '#', attr: wood }, { char: ']', attr: wood }]
                ],
                [
                    [U, { char: '/', attr: thatch }, { char: '\\', attr: thatch }, U],
                    [{ char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }],
                    [{ char: '|', attr: wood }, { char: '#', attr: wood }, { char: '#', attr: wood }, { char: '|', attr: wood }]
                ],
                [
                    [U, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '\\', attr: thatch }, U],
                    [{ char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '\\', attr: thatch }],
                    [{ char: '|', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '|', attr: wood }],
                    [{ char: '|', attr: wood }, { char: ' ', attr: window }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }]
                ],
                [
                    [U, U, { char: '_', attr: thatch }, { char: '/', attr: thatch }, { char: '\\', attr: thatch }, { char: '_', attr: thatch }, U],
                    [U, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }],
                    [{ char: '/', attr: thatch }, { char: '|', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: thatch }],
                    [U, { char: '|', attr: wood }, { char: ' ', attr: window }, { char: '|', attr: wood }, { char: '/', attr: wood }, { char: '\\', attr: wood }, { char: '|', attr: wood }],
                    [U, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }, { char: '|', attr: wood }]
                ],
                [
                    [U, U, { char: '_', attr: thatch }, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }, { char: '_', attr: thatch }, U, U],
                    [U, { char: '/', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '~', attr: thatchDark }, { char: '~', attr: thatch }, { char: '\\', attr: thatch }, U],
                    [{ char: '/', attr: thatch }, { char: '|', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '=', attr: wood }, { char: '|', attr: wood }, { char: '\\', attr: thatch }],
                    [U, { char: '|', attr: wood }, { char: ' ', attr: window }, { char: ' ', attr: window }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '/', attr: wood }, { char: '\\', attr: wood }, { char: '|', attr: wood }, U],
                    [U, { char: '|', attr: wood }, { char: '_', attr: window }, { char: '_', attr: window }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }, { char: '|', attr: wood }, U],
                    [U, { char: '|', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '_', attr: wood }, { char: '|', attr: wood }, { char: ' ', attr: wood }, { char: '|', attr: wood }, { char: '|', attr: wood }, U],
                    [U, { char: '|', attr: wood }, U, U, U, U, U, U, { char: '|', attr: wood }, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('palm', BeachSprites.createPalm);
registerRoadsideSprite('umbrella', BeachSprites.createUmbrella);
registerRoadsideSprite('lifeguard', BeachSprites.createLifeguard);
registerRoadsideSprite('surfboard', BeachSprites.createSurfboard);
registerRoadsideSprite('tiki', BeachSprites.createTiki);
registerRoadsideSprite('beachhut', BeachSprites.createBeachhut);
"use strict";
var HorrorSprites = {
    createDeadTree: function () {
        var branch = makeAttr(BROWN, BG_BLACK);
        var branchDark = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'deadtree',
            variants: [
                [
                    [U, { char: 'Y', attr: branch }, U],
                    [U, { char: '|', attr: branch }, U]
                ],
                [
                    [{ char: '\\', attr: branchDark }, { char: 'Y', attr: branch }, { char: '/', attr: branchDark }],
                    [U, { char: '|', attr: branch }, U],
                    [U, { char: '|', attr: branch }, U]
                ],
                [
                    [{ char: '\\', attr: branchDark }, U, { char: 'Y', attr: branch }, U, { char: '/', attr: branchDark }],
                    [U, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, U],
                    [U, U, { char: '|', attr: branch }, U, U],
                    [U, U, { char: '|', attr: branch }, U, U]
                ],
                [
                    [{ char: '\\', attr: branchDark }, U, { char: 'Y', attr: branch }, U, { char: 'Y', attr: branch }, U, { char: '/', attr: branchDark }],
                    [U, { char: '\\', attr: branch }, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, { char: '/', attr: branch }, U],
                    [U, U, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, U, U],
                    [U, U, U, { char: '|', attr: branch }, U, U, U],
                    [U, U, U, { char: '|', attr: branch }, U, U, U]
                ],
                [
                    [{ char: '_', attr: branchDark }, U, U, U, { char: 'V', attr: branch }, U, U, U, { char: '_', attr: branchDark }],
                    [U, { char: '\\', attr: branch }, { char: 'Y', attr: branch }, U, { char: '|', attr: branch }, U, { char: 'Y', attr: branch }, { char: '/', attr: branch }, U],
                    [U, U, { char: '\\', attr: branch }, { char: '\\', attr: branch }, { char: '|', attr: branch }, { char: '/', attr: branch }, { char: '/', attr: branch }, U, U],
                    [U, U, U, U, { char: '|', attr: branch }, U, U, U, U],
                    [U, U, U, U, { char: '|', attr: branch }, U, U, U, U],
                    [U, U, U, { char: '-', attr: branch }, { char: '|', attr: branch }, { char: '-', attr: branch }, U, U, U]
                ]
            ]
        };
    },
    createGravestone: function () {
        var stone = makeAttr(LIGHTGRAY, BG_BLACK);
        var stoneDark = makeAttr(DARKGRAY, BG_BLACK);
        var stoneText = makeAttr(DARKGRAY, BG_LIGHTGRAY);
        var U = null;
        return {
            name: 'gravestone',
            variants: [
                [
                    [{ char: GLYPH.UPPER_HALF, attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: stone }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: 'R', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: 'R', attr: stoneText }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'P', attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.LOWER_HALF, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.LOWER_HALF, attr: stoneDark }]
                ],
                [
                    [U, { char: '_', attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }, { char: '_', attr: stone }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: stoneText }, { char: '+', attr: stoneText }, { char: ' ', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'R', attr: stoneText }, { char: '.', attr: stoneText }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'P', attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.LOWER_HALF, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.LOWER_HALF, attr: stoneDark }]
                ],
                [
                    [U, { char: '_', attr: stone }, { char: GLYPH.UPPER_HALF, attr: stone }, { char: '_', attr: stone }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: stoneText }, { char: '+', attr: stoneText }, { char: ' ', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'R', attr: stoneText }, { char: '.', attr: stoneText }, { char: 'I', attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: 'P', attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.MEDIUM_SHADE, attr: stoneText }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.LOWER_HALF, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.LOWER_HALF, attr: stoneDark }]
                ]
            ]
        };
    },
    createPumpkin: function () {
        var pumpkin = makeAttr(BROWN, BG_BLACK);
        var glow = makeAttr(YELLOW, BG_BROWN);
        var stem = makeAttr(GREEN, BG_BLACK);
        var face = makeAttr(YELLOW, BG_BROWN);
        var U = null;
        return {
            name: 'pumpkin',
            variants: [
                [
                    [{ char: '(', attr: glow }, { char: ')', attr: glow }],
                    [{ char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }]
                ],
                [
                    [U, { char: '|', attr: stem }, U],
                    [{ char: '(', attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: glow }],
                    [{ char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }]
                ],
                [
                    [U, U, { char: '|', attr: stem }, U, U],
                    [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
                    [{ char: '(', attr: pumpkin }, { char: '^', attr: face }, { char: 'v', attr: face }, { char: '^', attr: face }, { char: ')', attr: pumpkin }],
                    [U, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, U]
                ],
                [
                    [U, U, { char: '\\', attr: stem }, { char: '/', attr: stem }, U, U],
                    [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
                    [{ char: '(', attr: pumpkin }, { char: '^', attr: face }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: '^', attr: face }, { char: ')', attr: pumpkin }],
                    [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: 'v', attr: face }, { char: 'v', attr: face }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
                    [U, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, U]
                ],
                [
                    [U, U, { char: '\\', attr: stem }, { char: '/', attr: stem }, U, U],
                    [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
                    [{ char: '(', attr: pumpkin }, { char: '^', attr: face }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: pumpkin }, { char: '^', attr: face }, { char: ')', attr: pumpkin }],
                    [{ char: '(', attr: pumpkin }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: 'v', attr: face }, { char: 'v', attr: face }, { char: GLYPH.FULL_BLOCK, attr: glow }, { char: ')', attr: pumpkin }],
                    [U, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, { char: GLYPH.LOWER_HALF, attr: pumpkin }, U]
                ]
            ]
        };
    },
    createSkull: function () {
        var bone = makeAttr(WHITE, BG_BLACK);
        var eye = makeAttr(BLACK, BG_LIGHTGRAY);
        var stake = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'skull',
            variants: [
                [
                    [{ char: '0', attr: bone }, { char: '0', attr: bone }],
                    [U, { char: '|', attr: stake }]
                ],
                [
                    [{ char: '(', attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, { char: ')', attr: bone }],
                    [{ char: 'o', attr: eye }, { char: 'v', attr: bone }, { char: 'o', attr: eye }],
                    [U, { char: '|', attr: stake }, U]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, U],
                    [{ char: '(', attr: bone }, { char: 'o', attr: eye }, { char: 'o', attr: eye }, { char: ')', attr: bone }],
                    [U, { char: GLYPH.LOWER_HALF, attr: bone }, { char: GLYPH.LOWER_HALF, attr: bone }, U],
                    [U, { char: '|', attr: stake }, { char: '|', attr: stake }, U]
                ],
                [
                    [U, { char: '_', attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, { char: '_', attr: bone }, U],
                    [{ char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }],
                    [{ char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }],
                    [U, { char: GLYPH.LOWER_HALF, attr: bone }, { char: 'w', attr: bone }, { char: GLYPH.LOWER_HALF, attr: bone }, U],
                    [U, U, { char: '|', attr: stake }, U, U]
                ],
                [
                    [U, { char: '_', attr: bone }, { char: GLYPH.UPPER_HALF, attr: bone }, { char: '_', attr: bone }, U],
                    [{ char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }],
                    [{ char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }],
                    [U, { char: GLYPH.LOWER_HALF, attr: bone }, { char: 'w', attr: bone }, { char: GLYPH.LOWER_HALF, attr: bone }, U],
                    [U, U, { char: '|', attr: stake }, U, U]
                ]
            ]
        };
    },
    createFence: function () {
        var iron = makeAttr(DARKGRAY, BG_BLACK);
        var ironPoint = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'fence',
            variants: [
                [
                    [{ char: '|', attr: iron }, { char: '|', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '-', attr: iron }, { char: '-', attr: iron }, { char: '-', attr: iron }]
                ],
                [
                    [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
                    [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
                ],
                [
                    [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
                ],
                [
                    [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
                ],
                [
                    [{ char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }, U, { char: '^', attr: ironPoint }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '+', attr: iron }, { char: '-', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }, U, { char: '|', attr: iron }]
                ]
            ]
        };
    },
    createCandle: function () {
        var flame = makeAttr(YELLOW, BG_BLACK);
        var candle = makeAttr(WHITE, BG_BLACK);
        var holder = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'candle',
            variants: [
                [
                    [{ char: '*', attr: flame }],
                    [{ char: '|', attr: holder }]
                ],
                [
                    [U, { char: '*', attr: flame }, U],
                    [{ char: '(', attr: holder }, { char: GLYPH.FULL_BLOCK, attr: candle }, { char: ')', attr: holder }],
                    [U, { char: '|', attr: holder }, U]
                ],
                [
                    [{ char: '*', attr: flame }, U, { char: '*', attr: flame }, U, { char: '*', attr: flame }],
                    [{ char: '|', attr: candle }, U, { char: '|', attr: candle }, U, { char: '|', attr: candle }],
                    [{ char: '\\', attr: holder }, { char: '-', attr: holder }, { char: '+', attr: holder }, { char: '-', attr: holder }, { char: '/', attr: holder }],
                    [U, U, { char: '|', attr: holder }, U, U]
                ],
                [
                    [{ char: '*', attr: flame }, U, { char: '*', attr: flame }, U, { char: '*', attr: flame }],
                    [{ char: '|', attr: candle }, U, { char: '|', attr: candle }, U, { char: '|', attr: candle }],
                    [{ char: '\\', attr: holder }, { char: '-', attr: holder }, { char: '+', attr: holder }, { char: '-', attr: holder }, { char: '/', attr: holder }],
                    [U, U, { char: '|', attr: holder }, U, U]
                ],
                [
                    [{ char: '*', attr: flame }, U, { char: '*', attr: flame }, U, { char: '*', attr: flame }],
                    [{ char: '|', attr: candle }, U, { char: '|', attr: candle }, U, { char: '|', attr: candle }],
                    [{ char: '\\', attr: holder }, { char: '-', attr: holder }, { char: '+', attr: holder }, { char: '-', attr: holder }, { char: '/', attr: holder }],
                    [U, U, { char: '|', attr: holder }, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('deadtree', HorrorSprites.createDeadTree);
registerRoadsideSprite('gravestone', HorrorSprites.createGravestone);
registerRoadsideSprite('pumpkin', HorrorSprites.createPumpkin);
registerRoadsideSprite('skull', HorrorSprites.createSkull);
registerRoadsideSprite('fence', HorrorSprites.createFence);
registerRoadsideSprite('candle', HorrorSprites.createCandle);
"use strict";
var WinterSprites = {
    createSnowPine: function () {
        var snow = makeAttr(WHITE, BG_BLACK);
        var pine = makeAttr(GREEN, BG_BLACK);
        var pineDark = makeAttr(CYAN, BG_BLACK);
        var trunk = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'snowpine',
            variants: [
                [
                    [U, { char: '^', attr: snow }, U],
                    [U, { char: '|', attr: trunk }, U]
                ],
                [
                    [U, { char: '*', attr: snow }, U],
                    [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
                    [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
                    [U, { char: '|', attr: trunk }, U]
                ],
                [
                    [U, U, { char: '*', attr: snow }, U, U],
                    [U, { char: '/', attr: pine }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: '\\', attr: pine }, U],
                    [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
                    [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
                    [U, U, { char: '|', attr: trunk }, U, U]
                ],
                [
                    [U, U, U, { char: '*', attr: snow }, U, U, U],
                    [U, U, { char: '/', attr: pine }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: '\\', attr: pine }, U, U],
                    [U, { char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }, U],
                    [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
                    [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U]
                ],
                [
                    [U, U, U, { char: '*', attr: snow }, U, U, U],
                    [U, U, { char: '/', attr: pine }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: '\\', attr: pine }, U, U],
                    [U, { char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }, U],
                    [{ char: '/', attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '\\', attr: pine }],
                    [{ char: '/', attr: pineDark }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: GLYPH.FULL_BLOCK, attr: pine }, { char: '\\', attr: pineDark }],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U]
                ]
            ]
        };
    },
    createSnowman: function () {
        var snow = makeAttr(WHITE, BG_BLACK);
        var hat = makeAttr(BLACK, BG_BLACK);
        var face = makeAttr(BLACK, BG_LIGHTGRAY);
        var carrot = makeAttr(BROWN, BG_LIGHTGRAY);
        var scarf = makeAttr(LIGHTRED, BG_BLACK);
        var button = makeAttr(BLACK, BG_LIGHTGRAY);
        var arm = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'snowman',
            variants: [
                [
                    [{ char: 'o', attr: snow }, { char: 'o', attr: snow }],
                    [{ char: 'O', attr: snow }, { char: 'O', attr: snow }],
                    [{ char: 'O', attr: snow }, { char: 'O', attr: snow }]
                ],
                [
                    [U, { char: GLYPH.FULL_BLOCK, attr: hat }, U],
                    [{ char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }],
                    [{ char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }],
                    [{ char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }]
                ],
                [
                    [U, { char: '_', attr: hat }, { char: GLYPH.FULL_BLOCK, attr: hat }, { char: '_', attr: hat }, U],
                    [U, { char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }, U],
                    [{ char: '-', attr: arm }, { char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }, { char: '-', attr: arm }],
                    [U, { char: '(', attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: ')', attr: snow }, U],
                    [U, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, U]
                ],
                [
                    [U, { char: '_', attr: hat }, { char: GLYPH.FULL_BLOCK, attr: hat }, { char: '_', attr: hat }, U],
                    [U, { char: '.', attr: face }, { char: 'v', attr: carrot }, { char: '.', attr: face }, U],
                    [U, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, U],
                    [{ char: '-', attr: arm }, { char: '(', attr: snow }, { char: '*', attr: button }, { char: ')', attr: snow }, { char: '-', attr: arm }],
                    [U, { char: '(', attr: snow }, { char: '*', attr: button }, { char: ')', attr: snow }, U],
                    [U, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, U]
                ],
                [
                    [U, U, { char: '_', attr: hat }, { char: GLYPH.FULL_BLOCK, attr: hat }, { char: '_', attr: hat }, U, U],
                    [U, { char: '(', attr: snow }, { char: '.', attr: face }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: '.', attr: face }, { char: ')', attr: snow }, U],
                    [U, { char: '(', attr: snow }, { char: ' ', attr: snow }, { char: '>', attr: carrot }, { char: ' ', attr: snow }, { char: ')', attr: snow }, U],
                    [U, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, { char: '~', attr: scarf }, U],
                    [{ char: '/', attr: arm }, { char: '(', attr: snow }, { char: ' ', attr: snow }, { char: '*', attr: button }, { char: ' ', attr: snow }, { char: ')', attr: snow }, { char: '\\', attr: arm }],
                    [U, { char: '(', attr: snow }, { char: ' ', attr: snow }, { char: '*', attr: button }, { char: ' ', attr: snow }, { char: ')', attr: snow }, U],
                    [U, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }, U]
                ]
            ]
        };
    },
    createIceCrystal: function () {
        var ice = makeAttr(LIGHTCYAN, BG_BLACK);
        var iceShine = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'icecrystal',
            variants: [
                [
                    [{ char: '*', attr: iceShine }],
                    [{ char: 'V', attr: ice }]
                ],
                [
                    [U, { char: '*', attr: iceShine }, U],
                    [{ char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }],
                    [U, { char: 'V', attr: ice }, U]
                ],
                [
                    [U, U, { char: '*', attr: iceShine }, U, U],
                    [{ char: '/', attr: ice }, { char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }, { char: '\\', attr: ice }],
                    [U, { char: '/', attr: ice }, { char: '|', attr: iceShine }, { char: '\\', attr: ice }, U],
                    [U, U, { char: 'V', attr: ice }, U, U]
                ],
                [
                    [U, U, { char: '*', attr: iceShine }, U, U],
                    [{ char: '\\', attr: ice }, { char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }, { char: '/', attr: ice }],
                    [U, { char: '<', attr: ice }, { char: '+', attr: iceShine }, { char: '>', attr: ice }, U],
                    [{ char: '/', attr: ice }, { char: '\\', attr: ice }, { char: '|', attr: ice }, { char: '/', attr: ice }, { char: '\\', attr: ice }],
                    [U, U, { char: 'V', attr: ice }, U, U]
                ],
                [
                    [U, U, { char: '*', attr: iceShine }, U, U],
                    [{ char: '\\', attr: ice }, { char: '/', attr: ice }, { char: '|', attr: ice }, { char: '\\', attr: ice }, { char: '/', attr: ice }],
                    [U, { char: '<', attr: ice }, { char: '+', attr: iceShine }, { char: '>', attr: ice }, U],
                    [{ char: '/', attr: ice }, { char: '\\', attr: ice }, { char: '|', attr: ice }, { char: '/', attr: ice }, { char: '\\', attr: ice }],
                    [U, U, { char: 'V', attr: ice }, U, U]
                ]
            ]
        };
    },
    createCandyCane: function () {
        var red = makeAttr(LIGHTRED, BG_BLACK);
        var white = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'candycane',
            variants: [
                [
                    [{ char: '?', attr: red }],
                    [{ char: '|', attr: white }]
                ],
                [
                    [{ char: '_', attr: red }, { char: ')', attr: red }],
                    [{ char: '|', attr: white }, U],
                    [{ char: '|', attr: red }, U]
                ],
                [
                    [{ char: '_', attr: red }, { char: '_', attr: white }, { char: ')', attr: red }],
                    [{ char: '|', attr: white }, U, U],
                    [{ char: '|', attr: red }, U, U],
                    [{ char: '|', attr: white }, U, U]
                ],
                [
                    [{ char: '_', attr: red }, { char: '_', attr: white }, { char: ')', attr: red }],
                    [{ char: '|', attr: white }, U, { char: '/', attr: white }],
                    [{ char: '|', attr: red }, U, U],
                    [{ char: '|', attr: white }, U, U],
                    [{ char: '|', attr: red }, U, U]
                ],
                [
                    [{ char: '_', attr: red }, { char: '_', attr: white }, { char: ')', attr: red }],
                    [{ char: '|', attr: white }, U, { char: '/', attr: white }],
                    [{ char: '|', attr: red }, U, U],
                    [{ char: '|', attr: white }, U, U],
                    [{ char: '|', attr: red }, U, U]
                ]
            ]
        };
    },
    createSnowDrift: function () {
        var snow = makeAttr(WHITE, BG_BLACK);
        var snowShade = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'snowdrift',
            variants: [
                [
                    [{ char: GLYPH.LOWER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snow }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
                    [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
                    [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
                ],
                [
                    [U, U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U, U],
                    [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
                    [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
                ],
                [
                    [U, U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U, U],
                    [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
                    [{ char: GLYPH.LOWER_HALF, attr: snowShade }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.FULL_BLOCK, attr: snow }, { char: GLYPH.LOWER_HALF, attr: snowShade }]
                ]
            ]
        };
    },
    createSignpost: function () {
        var wood = makeAttr(BROWN, BG_BLACK);
        var snow = makeAttr(WHITE, BG_BLACK);
        var text = makeAttr(WHITE, BG_BROWN);
        var U = null;
        return {
            name: 'signpost',
            variants: [
                [
                    [{ char: '[', attr: wood }, { char: ']', attr: wood }],
                    [U, { char: '|', attr: wood }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }],
                    [{ char: '[', attr: wood }, { char: '>', attr: text }, { char: ']', attr: wood }],
                    [U, { char: '|', attr: wood }, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }],
                    [{ char: '[', attr: wood }, { char: '>', attr: text }, { char: '>', attr: text }, { char: ']', attr: wood }],
                    [U, { char: '|', attr: wood }, { char: '|', attr: wood }, U],
                    [U, { char: '|', attr: wood }, { char: '|', attr: wood }, U]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
                    [{ char: '[', attr: wood }, { char: 'S', attr: text }, { char: 'K', attr: text }, { char: 'I', attr: text }, { char: '>', attr: wood }],
                    [U, U, { char: '|', attr: wood }, U, U],
                    [U, U, { char: '|', attr: wood }, U, U]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, { char: GLYPH.UPPER_HALF, attr: snow }, U],
                    [{ char: '[', attr: wood }, { char: 'S', attr: text }, { char: 'K', attr: text }, { char: 'I', attr: text }, { char: '>', attr: wood }],
                    [U, U, { char: '|', attr: wood }, U, U],
                    [U, U, { char: '|', attr: wood }, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('snowpine', WinterSprites.createSnowPine);
registerRoadsideSprite('snowman', WinterSprites.createSnowman);
registerRoadsideSprite('icecrystal', WinterSprites.createIceCrystal);
registerRoadsideSprite('candycane', WinterSprites.createCandyCane);
registerRoadsideSprite('snowdrift', WinterSprites.createSnowDrift);
registerRoadsideSprite('signpost', WinterSprites.createSignpost);
"use strict";
var DesertSprites = {
    createSaguaro: function () {
        var cactus = makeAttr(GREEN, BG_BLACK);
        var cactusDark = makeAttr(CYAN, BG_BLACK);
        var U = null;
        return {
            name: 'saguaro',
            variants: [
                [
                    [{ char: '|', attr: cactus }],
                    [{ char: '|', attr: cactus }],
                    [{ char: '|', attr: cactus }]
                ],
                [
                    [U, { char: '|', attr: cactus }, U],
                    [{ char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }],
                    [{ char: '|', attr: cactusDark }, { char: '|', attr: cactus }, { char: '|', attr: cactusDark }],
                    [U, { char: '|', attr: cactus }, U]
                ],
                [
                    [U, U, { char: '(', attr: cactus }, U, U],
                    [{ char: '_', attr: cactus }, U, { char: '|', attr: cactus }, U, { char: '_', attr: cactus }],
                    [{ char: '|', attr: cactusDark }, { char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }, { char: '|', attr: cactusDark }],
                    [{ char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }],
                    [U, U, { char: '|', attr: cactus }, U, U]
                ],
                [
                    [U, U, U, { char: '(', attr: cactus }, U, U, U],
                    [{ char: '_', attr: cactus }, U, U, { char: '|', attr: cactus }, U, U, { char: '_', attr: cactus }],
                    [{ char: '|', attr: cactusDark }, U, { char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }, U, { char: '|', attr: cactusDark }],
                    [{ char: '|', attr: cactusDark }, { char: '/', attr: cactus }, U, { char: '|', attr: cactus }, U, { char: '\\', attr: cactus }, { char: '|', attr: cactusDark }],
                    [U, { char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }, U],
                    [U, U, U, { char: '|', attr: cactus }, U, U, U]
                ],
                [
                    [U, U, U, { char: '(', attr: cactus }, U, U, U],
                    [{ char: '_', attr: cactus }, U, U, { char: '|', attr: cactus }, U, U, { char: '_', attr: cactus }],
                    [{ char: '|', attr: cactusDark }, U, { char: '/', attr: cactus }, { char: '|', attr: cactus }, { char: '\\', attr: cactus }, U, { char: '|', attr: cactusDark }],
                    [{ char: '|', attr: cactusDark }, { char: '/', attr: cactus }, U, { char: '|', attr: cactus }, U, { char: '\\', attr: cactus }, { char: '|', attr: cactusDark }],
                    [U, { char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }, U],
                    [U, { char: '|', attr: cactusDark }, U, { char: '|', attr: cactus }, U, { char: '|', attr: cactusDark }, U],
                    [U, U, U, { char: '|', attr: cactus }, U, U, U]
                ]
            ]
        };
    },
    createBarrelCactus: function () {
        var cactus = makeAttr(GREEN, BG_BLACK);
        var spine = makeAttr(YELLOW, BG_BLACK);
        var flower = makeAttr(LIGHTRED, BG_BLACK);
        var U = null;
        return {
            name: 'barrel',
            variants: [
                [
                    [{ char: '(', attr: cactus }, { char: ')', attr: cactus }],
                    [{ char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }]
                ],
                [
                    [{ char: '(', attr: cactus }, { char: '*', attr: flower }, { char: ')', attr: cactus }],
                    [{ char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }]
                ],
                [
                    [U, { char: '*', attr: flower }, { char: '*', attr: flower }, U],
                    [{ char: '(', attr: cactus }, { char: '|', attr: spine }, { char: '|', attr: spine }, { char: ')', attr: cactus }],
                    [{ char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }]
                ],
                [
                    [U, U, { char: '@', attr: flower }, U, U],
                    [U, { char: '/', attr: spine }, { char: GLYPH.UPPER_HALF, attr: cactus }, { char: '\\', attr: spine }, U],
                    [{ char: '(', attr: cactus }, { char: '|', attr: spine }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: '|', attr: spine }, { char: ')', attr: cactus }],
                    [U, { char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }, U]
                ],
                [
                    [U, U, { char: '@', attr: flower }, U, U],
                    [U, { char: '/', attr: spine }, { char: GLYPH.UPPER_HALF, attr: cactus }, { char: '\\', attr: spine }, U],
                    [{ char: '(', attr: cactus }, { char: '|', attr: spine }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: '|', attr: spine }, { char: ')', attr: cactus }],
                    [U, { char: GLYPH.LOWER_HALF, attr: cactus }, { char: GLYPH.FULL_BLOCK, attr: cactus }, { char: GLYPH.LOWER_HALF, attr: cactus }, U]
                ]
            ]
        };
    },
    createTumbleweed: function () {
        var weed = makeAttr(BROWN, BG_BLACK);
        var weedLight = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'tumbleweed',
            variants: [
                [
                    [{ char: '@', attr: weed }, { char: '@', attr: weed }],
                    [{ char: '@', attr: weed }, { char: '@', attr: weed }]
                ],
                [
                    [U, { char: '@', attr: weedLight }, U],
                    [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
                    [U, { char: '@', attr: weed }, U]
                ],
                [
                    [U, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, U],
                    [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
                    [U, { char: '@', attr: weed }, { char: '@', attr: weed }, U]
                ],
                [
                    [U, { char: '~', attr: weedLight }, { char: '@', attr: weedLight }, { char: '~', attr: weedLight }, U],
                    [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
                    [{ char: '@', attr: weed }, { char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weed }, { char: '@', attr: weed }],
                    [U, { char: '~', attr: weed }, { char: '@', attr: weed }, { char: '~', attr: weed }, U]
                ],
                [
                    [U, { char: '~', attr: weedLight }, { char: '@', attr: weedLight }, { char: '~', attr: weedLight }, U],
                    [{ char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weedLight }, { char: '@', attr: weed }],
                    [{ char: '@', attr: weed }, { char: '@', attr: weed }, { char: '@', attr: weedLight }, { char: '@', attr: weed }, { char: '@', attr: weed }],
                    [U, { char: '~', attr: weed }, { char: '@', attr: weed }, { char: '~', attr: weed }, U]
                ]
            ]
        };
    },
    createCowSkull: function () {
        var bone = makeAttr(WHITE, BG_BLACK);
        var eye = makeAttr(BLACK, BG_LIGHTGRAY);
        var U = null;
        return {
            name: 'cowskull',
            variants: [
                [
                    [{ char: '\\', attr: bone }, { char: 'o', attr: bone }, { char: '/', attr: bone }],
                    [U, { char: 'V', attr: bone }, U]
                ],
                [
                    [{ char: '~', attr: bone }, U, U, { char: '~', attr: bone }],
                    [{ char: '(', attr: bone }, { char: 'o', attr: eye }, { char: 'o', attr: eye }, { char: ')', attr: bone }],
                    [U, { char: '\\', attr: bone }, { char: '/', attr: bone }, U]
                ],
                [
                    [{ char: '~', attr: bone }, { char: '_', attr: bone }, U, { char: '_', attr: bone }, { char: '~', attr: bone }],
                    [{ char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }],
                    [U, { char: '\\', attr: bone }, { char: 'v', attr: bone }, { char: '/', attr: bone }, U],
                    [U, U, { char: 'w', attr: bone }, U, U]
                ],
                [
                    [{ char: '/', attr: bone }, { char: '~', attr: bone }, { char: '_', attr: bone }, U, { char: '_', attr: bone }, { char: '~', attr: bone }, { char: '\\', attr: bone }],
                    [U, { char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }, U],
                    [U, { char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }, U],
                    [U, U, { char: '\\', attr: bone }, { char: 'w', attr: bone }, { char: '/', attr: bone }, U, U],
                    [U, U, U, { char: '|', attr: bone }, U, U, U]
                ],
                [
                    [{ char: '/', attr: bone }, { char: '~', attr: bone }, { char: '_', attr: bone }, U, { char: '_', attr: bone }, { char: '~', attr: bone }, { char: '\\', attr: bone }],
                    [U, { char: '(', attr: bone }, { char: 'O', attr: eye }, { char: ' ', attr: bone }, { char: 'O', attr: eye }, { char: ')', attr: bone }, U],
                    [U, { char: '(', attr: bone }, { char: ' ', attr: bone }, { char: 'v', attr: bone }, { char: ' ', attr: bone }, { char: ')', attr: bone }, U],
                    [U, U, { char: '\\', attr: bone }, { char: 'w', attr: bone }, { char: '/', attr: bone }, U, U],
                    [U, U, U, { char: '|', attr: bone }, U, U, U]
                ]
            ]
        };
    },
    createDesertRock: function () {
        var rock = makeAttr(BROWN, BG_BLACK);
        var rockDark = makeAttr(DARKGRAY, BG_BLACK);
        var rockLight = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'desertrock',
            variants: [
                [
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: rockLight }, U],
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
                ],
                [
                    [U, { char: '_', attr: rockLight }, { char: '_', attr: rockLight }, U],
                    [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.MEDIUM_SHADE, attr: rockDark }, { char: '\\', attr: rockDark }],
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
                ],
                [
                    [U, { char: '_', attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: '_', attr: rockLight }, U],
                    [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.MEDIUM_SHADE, attr: rockDark }, { char: '\\', attr: rockDark }],
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
                ],
                [
                    [U, { char: '_', attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: '_', attr: rockLight }, U],
                    [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.MEDIUM_SHADE, attr: rockDark }, { char: '\\', attr: rockDark }],
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.LOWER_HALF, attr: rockDark }]
                ]
            ]
        };
    },
    createWesternSign: function () {
        var wood = makeAttr(BROWN, BG_BLACK);
        var text = makeAttr(YELLOW, BG_BROWN);
        var U = null;
        return {
            name: 'westernsign',
            variants: [
                [
                    [{ char: '[', attr: wood }, { char: ']', attr: wood }],
                    [U, { char: '|', attr: wood }]
                ],
                [
                    [{ char: '[', attr: wood }, { char: '>', attr: text }, { char: ']', attr: wood }],
                    [U, { char: '|', attr: wood }, U],
                    [U, { char: '|', attr: wood }, U]
                ],
                [
                    [{ char: '[', attr: wood }, { char: 'G', attr: text }, { char: 'O', attr: text }, { char: '!', attr: text }, { char: '>', attr: wood }],
                    [U, U, { char: '|', attr: wood }, U, U],
                    [U, U, { char: '|', attr: wood }, U, U],
                    [U, U, { char: '|', attr: wood }, U, U]
                ],
                [
                    [{ char: '<', attr: wood }, { char: 'W', attr: text }, { char: 'E', attr: text }, { char: 'S', attr: text }, { char: 'T', attr: text }, { char: '>', attr: wood }],
                    [U, U, U, { char: '|', attr: wood }, U, U],
                    [U, U, U, { char: '|', attr: wood }, U, U],
                    [U, U, U, { char: '|', attr: wood }, U, U],
                    [U, U, { char: '-', attr: wood }, { char: '+', attr: wood }, { char: '-', attr: wood }, U]
                ],
                [
                    [{ char: '<', attr: wood }, { char: 'W', attr: text }, { char: 'E', attr: text }, { char: 'S', attr: text }, { char: 'T', attr: text }, { char: '>', attr: wood }],
                    [U, U, U, { char: '|', attr: wood }, U, U],
                    [U, U, U, { char: '|', attr: wood }, U, U],
                    [U, U, U, { char: '|', attr: wood }, U, U],
                    [U, U, { char: '-', attr: wood }, { char: '+', attr: wood }, { char: '-', attr: wood }, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('saguaro', DesertSprites.createSaguaro);
registerRoadsideSprite('barrel', DesertSprites.createBarrelCactus);
registerRoadsideSprite('tumbleweed', DesertSprites.createTumbleweed);
registerRoadsideSprite('cowskull', DesertSprites.createCowSkull);
registerRoadsideSprite('desertrock', DesertSprites.createDesertRock);
registerRoadsideSprite('westernsign', DesertSprites.createWesternSign);
"use strict";
var JungleSprites = {
    createJungleTree: function () {
        var leaf = makeAttr(GREEN, BG_BLACK);
        var leafBright = makeAttr(LIGHTGREEN, BG_BLACK);
        var trunk = makeAttr(BROWN, BG_BLACK);
        var trunkDark = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'jungle_tree',
            variants: [
                [
                    [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }],
                    [U, { char: '|', attr: trunk }],
                    [U, { char: '|', attr: trunk }]
                ],
                [
                    [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
                    [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
                    [U, { char: '|', attr: trunk }, U],
                    [U, { char: '|', attr: trunk }, U]
                ],
                [
                    [U, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, U],
                    [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
                    [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '|', attr: trunk }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
                    [U, U, { char: '|', attr: trunk }, U, U],
                    [U, U, { char: '|', attr: trunkDark }, U, U]
                ],
                [
                    [U, U, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, U, U],
                    [U, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, U],
                    [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
                    [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '\\', attr: trunk }, { char: '|', attr: trunk }, { char: '/', attr: trunk }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, { char: '|', attr: trunkDark }, U, U, U]
                ],
                [
                    [U, U, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, U, U],
                    [U, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, U],
                    [{ char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }],
                    [{ char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '|', attr: trunk }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }, { char: '@', attr: leaf }, { char: '@', attr: leafBright }],
                    [U, U, { char: '@', attr: leaf }, { char: '\\', attr: trunk }, { char: '|', attr: trunk }, { char: '/', attr: trunk }, { char: '@', attr: leaf }, U, U],
                    [U, U, U, U, { char: '|', attr: trunk }, U, U, U, U],
                    [U, U, U, U, { char: '|', attr: trunkDark }, U, U, U, U]
                ]
            ]
        };
    },
    createFern: function () {
        var frond = makeAttr(GREEN, BG_BLACK);
        var frondLight = makeAttr(LIGHTGREEN, BG_BLACK);
        var U = null;
        return {
            name: 'fern',
            variants: [
                [
                    [{ char: '/', attr: frond }, { char: '\\', attr: frond }],
                    [{ char: '/', attr: frondLight }, { char: '\\', attr: frondLight }]
                ],
                [
                    [{ char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }],
                    [{ char: '/', attr: frondLight }, { char: '|', attr: frond }, { char: '\\', attr: frondLight }]
                ],
                [
                    [{ char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }],
                    [{ char: '/', attr: frond }, { char: '/', attr: frondLight }, { char: '|', attr: frond }, { char: '\\', attr: frondLight }, { char: '\\', attr: frond }],
                    [U, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, U]
                ],
                [
                    [{ char: '/', attr: frond }, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, { char: '\\', attr: frond }],
                    [U, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, U],
                    [U, U, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, U, U],
                    [U, U, U, { char: '|', attr: frond }, U, U, U]
                ],
                [
                    [{ char: '/', attr: frond }, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, { char: '\\', attr: frond }],
                    [U, { char: '/', attr: frondLight }, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, { char: '\\', attr: frondLight }, U],
                    [U, U, { char: '/', attr: frond }, { char: '|', attr: frondLight }, { char: '\\', attr: frond }, U, U],
                    [U, U, U, { char: '|', attr: frond }, U, U, U]
                ]
            ]
        };
    },
    createVine: function () {
        var vine = makeAttr(GREEN, BG_BLACK);
        var vineLight = makeAttr(LIGHTGREEN, BG_BLACK);
        var flower = makeAttr(LIGHTMAGENTA, BG_BLACK);
        return {
            name: 'vine',
            variants: [
                [
                    [{ char: '|', attr: vine }],
                    [{ char: ')', attr: vineLight }],
                    [{ char: '|', attr: vine }]
                ],
                [
                    [{ char: '(', attr: vine }, { char: ')', attr: vineLight }],
                    [{ char: ')', attr: vineLight }, { char: '(', attr: vine }],
                    [{ char: '|', attr: vine }, { char: ')', attr: vineLight }],
                    [{ char: ')', attr: vineLight }, { char: '|', attr: vine }]
                ],
                [
                    [{ char: '(', attr: vineLight }, { char: '|', attr: vine }, { char: ')', attr: vineLight }],
                    [{ char: ')', attr: vine }, { char: '*', attr: flower }, { char: '(', attr: vine }],
                    [{ char: '|', attr: vineLight }, { char: ')', attr: vine }, { char: '|', attr: vineLight }],
                    [{ char: ')', attr: vine }, { char: '|', attr: vineLight }, { char: '(', attr: vine }],
                    [{ char: '|', attr: vineLight }, { char: '(', attr: vine }, { char: '|', attr: vineLight }]
                ],
                [
                    [{ char: '(', attr: vine }, { char: '(', attr: vineLight }, { char: ')', attr: vineLight }, { char: ')', attr: vine }],
                    [{ char: ')', attr: vineLight }, { char: '*', attr: flower }, { char: '*', attr: flower }, { char: '(', attr: vineLight }],
                    [{ char: '|', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: '|', attr: vine }],
                    [{ char: ')', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '(', attr: vineLight }],
                    [{ char: '(', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: ')', attr: vine }],
                    [{ char: '|', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '|', attr: vineLight }]
                ],
                [
                    [{ char: '(', attr: vine }, { char: '(', attr: vineLight }, { char: ')', attr: vineLight }, { char: ')', attr: vine }],
                    [{ char: ')', attr: vineLight }, { char: '*', attr: flower }, { char: '*', attr: flower }, { char: '(', attr: vineLight }],
                    [{ char: '|', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: '|', attr: vine }],
                    [{ char: ')', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '(', attr: vineLight }],
                    [{ char: '(', attr: vine }, { char: ')', attr: vineLight }, { char: '(', attr: vineLight }, { char: ')', attr: vine }],
                    [{ char: '|', attr: vineLight }, { char: '|', attr: vine }, { char: '|', attr: vine }, { char: '|', attr: vineLight }]
                ]
            ]
        };
    },
    createFlower: function () {
        var petal = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var petalDark = makeAttr(MAGENTA, BG_BLACK);
        var center = makeAttr(YELLOW, BG_BLACK);
        var stem = makeAttr(GREEN, BG_BLACK);
        var leaf = makeAttr(LIGHTGREEN, BG_BLACK);
        var U = null;
        return {
            name: 'flower',
            variants: [
                [
                    [{ char: '*', attr: petal }],
                    [{ char: '|', attr: stem }]
                ],
                [
                    [U, { char: '*', attr: petal }, U],
                    [{ char: '*', attr: petalDark }, { char: 'o', attr: center }, { char: '*', attr: petalDark }],
                    [U, { char: '|', attr: stem }, U]
                ],
                [
                    [U, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, U],
                    [{ char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '@', attr: center }, { char: '*', attr: petal }, { char: '*', attr: petalDark }],
                    [U, { char: '*', attr: petal }, { char: '|', attr: stem }, { char: '*', attr: petal }, U],
                    [U, { char: '/', attr: leaf }, { char: '|', attr: stem }, { char: '\\', attr: leaf }, U]
                ],
                [
                    [U, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, U],
                    [{ char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '@', attr: center }, { char: '*', attr: petal }, { char: '*', attr: petalDark }],
                    [{ char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }],
                    [U, { char: '/', attr: leaf }, { char: '|', attr: stem }, { char: '\\', attr: leaf }, U],
                    [U, U, { char: '|', attr: stem }, U, U]
                ],
                [
                    [U, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, U],
                    [{ char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '@', attr: center }, { char: '*', attr: petal }, { char: '*', attr: petalDark }],
                    [{ char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }, { char: '*', attr: petalDark }, { char: '*', attr: petal }],
                    [U, { char: '/', attr: leaf }, { char: '|', attr: stem }, { char: '\\', attr: leaf }, U],
                    [U, U, { char: '|', attr: stem }, U, U]
                ]
            ]
        };
    },
    createParrot: function () {
        var body = makeAttr(LIGHTRED, BG_BLACK);
        var wing = makeAttr(LIGHTBLUE, BG_BLACK);
        var beak = makeAttr(YELLOW, BG_BLACK);
        var tail = makeAttr(LIGHTGREEN, BG_BLACK);
        var eye = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'parrot',
            variants: [
                [
                    [{ char: '(', attr: body }, { char: '>', attr: beak }],
                    [{ char: '~', attr: tail }, { char: ')', attr: body }]
                ],
                [
                    [U, { char: '(', attr: body }, { char: '<', attr: beak }],
                    [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
                    [U, { char: '~', attr: tail }, { char: '~', attr: tail }]
                ],
                [
                    [U, { char: '(', attr: body }, { char: 'o', attr: eye }, { char: '<', attr: beak }],
                    [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
                    [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }, U],
                    [U, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }]
                ],
                [
                    [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: 'o', attr: eye }, { char: '<', attr: beak }],
                    [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
                    [{ char: '/', attr: wing }, { char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }, U],
                    [U, U, { char: '\\', attr: body }, { char: '/', attr: body }, U],
                    [U, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }]
                ],
                [
                    [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: 'o', attr: eye }, { char: '<', attr: beak }],
                    [{ char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }],
                    [{ char: '/', attr: wing }, { char: '/', attr: wing }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: body }, U],
                    [U, U, { char: '\\', attr: body }, { char: '/', attr: body }, U],
                    [U, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }, { char: '~', attr: tail }]
                ]
            ]
        };
    },
    createBanana: function () {
        var leaf = makeAttr(GREEN, BG_BLACK);
        var leafLight = makeAttr(LIGHTGREEN, BG_BLACK);
        var trunk = makeAttr(BROWN, BG_BLACK);
        var banana = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'banana',
            variants: [
                [
                    [{ char: '/', attr: leaf }, { char: '\\', attr: leaf }],
                    [U, { char: ')', attr: banana }],
                    [U, { char: '|', attr: trunk }]
                ],
                [
                    [{ char: '/', attr: leafLight }, { char: '|', attr: leaf }, { char: '\\', attr: leafLight }],
                    [{ char: '/', attr: leaf }, { char: ')', attr: banana }, { char: '\\', attr: leaf }],
                    [U, { char: '|', attr: trunk }, U],
                    [U, { char: '|', attr: trunk }, U]
                ],
                [
                    [{ char: '/', attr: leaf }, { char: '/', attr: leafLight }, { char: '|', attr: leaf }, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }],
                    [{ char: '/', attr: leafLight }, U, { char: '|', attr: trunk }, U, { char: '\\', attr: leafLight }],
                    [U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U],
                    [U, U, { char: '|', attr: trunk }, U, U],
                    [U, U, { char: '|', attr: trunk }, U, U]
                ],
                [
                    [{ char: '/', attr: leaf }, { char: '/', attr: leafLight }, U, { char: '|', attr: leaf }, U, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }],
                    [U, { char: '/', attr: leaf }, { char: '/', attr: leafLight }, { char: '|', attr: trunk }, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }, U],
                    [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
                    [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U]
                ],
                [
                    [{ char: '/', attr: leaf }, { char: '/', attr: leafLight }, U, { char: '|', attr: leaf }, U, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }],
                    [U, { char: '/', attr: leaf }, { char: '/', attr: leafLight }, { char: '|', attr: trunk }, { char: '\\', attr: leafLight }, { char: '\\', attr: leaf }, U],
                    [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
                    [U, U, { char: ')', attr: banana }, { char: ')', attr: banana }, { char: ')', attr: banana }, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U],
                    [U, U, U, { char: '|', attr: trunk }, U, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('jungle_tree', JungleSprites.createJungleTree);
registerRoadsideSprite('fern', JungleSprites.createFern);
registerRoadsideSprite('vine', JungleSprites.createVine);
registerRoadsideSprite('flower', JungleSprites.createFlower);
registerRoadsideSprite('parrot', JungleSprites.createParrot);
registerRoadsideSprite('banana', JungleSprites.createBanana);
"use strict";
var CandySprites = {
    createLollipop: function () {
        var candy1 = makeAttr(LIGHTRED, BG_BLACK);
        var candy2 = makeAttr(WHITE, BG_BLACK);
        var candy3 = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var stick = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'lollipop',
            variants: [
                [
                    [{ char: '(', attr: candy1 }, { char: ')', attr: candy2 }],
                    [U, { char: '|', attr: stick }],
                    [U, { char: '|', attr: stick }]
                ],
                [
                    [U, { char: '@', attr: candy1 }, U],
                    [{ char: '(', attr: candy2 }, { char: '@', attr: candy3 }, { char: ')', attr: candy2 }],
                    [U, { char: '|', attr: stick }, U],
                    [U, { char: '|', attr: stick }, U]
                ],
                [
                    [U, { char: '/', attr: candy1 }, { char: '-', attr: candy2 }, { char: '\\', attr: candy1 }, U],
                    [{ char: '(', attr: candy2 }, { char: '@', attr: candy3 }, { char: '@', attr: candy1 }, { char: '@', attr: candy3 }, { char: ')', attr: candy2 }],
                    [U, { char: '\\', attr: candy1 }, { char: '-', attr: candy2 }, { char: '/', attr: candy1 }, U],
                    [U, U, { char: '|', attr: stick }, U, U],
                    [U, U, { char: '|', attr: stick }, U, U]
                ],
                [
                    [U, { char: '/', attr: candy1 }, { char: '~', attr: candy2 }, { char: '~', attr: candy3 }, { char: '\\', attr: candy1 }, U],
                    [{ char: '(', attr: candy2 }, { char: '@', attr: candy3 }, { char: '@', attr: candy1 }, { char: '@', attr: candy3 }, { char: '@', attr: candy1 }, { char: ')', attr: candy2 }],
                    [{ char: '(', attr: candy1 }, { char: '@', attr: candy2 }, { char: '@', attr: candy3 }, { char: '@', attr: candy2 }, { char: '@', attr: candy3 }, { char: ')', attr: candy1 }],
                    [U, { char: '\\', attr: candy2 }, { char: '~', attr: candy1 }, { char: '~', attr: candy2 }, { char: '/', attr: candy2 }, U],
                    [U, U, U, { char: '|', attr: stick }, U, U],
                    [U, U, U, { char: '|', attr: stick }, U, U]
                ],
                [
                    [U, { char: '/', attr: candy1 }, { char: '~', attr: candy2 }, { char: '~', attr: candy3 }, { char: '\\', attr: candy1 }, U],
                    [{ char: '(', attr: candy2 }, { char: '@', attr: candy3 }, { char: '@', attr: candy1 }, { char: '@', attr: candy3 }, { char: '@', attr: candy1 }, { char: ')', attr: candy2 }],
                    [{ char: '(', attr: candy1 }, { char: '@', attr: candy2 }, { char: '@', attr: candy3 }, { char: '@', attr: candy2 }, { char: '@', attr: candy3 }, { char: ')', attr: candy1 }],
                    [U, { char: '\\', attr: candy2 }, { char: '~', attr: candy1 }, { char: '~', attr: candy2 }, { char: '/', attr: candy2 }, U],
                    [U, U, U, { char: '|', attr: stick }, U, U],
                    [U, U, U, { char: '|', attr: stick }, U, U]
                ]
            ]
        };
    },
    createCandyCane: function () {
        var red = makeAttr(LIGHTRED, BG_BLACK);
        var white = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'candy_cane',
            variants: [
                [
                    [{ char: '/', attr: red }, { char: ')', attr: white }],
                    [{ char: '|', attr: white }, U],
                    [{ char: '|', attr: red }, U]
                ],
                [
                    [U, { char: '/', attr: red }, { char: ')', attr: white }],
                    [U, { char: '|', attr: white }, U],
                    [U, { char: '|', attr: red }, U],
                    [U, { char: '|', attr: white }, U]
                ],
                [
                    [U, { char: '_', attr: red }, { char: '/', attr: white }, { char: ')', attr: red }],
                    [{ char: '(', attr: white }, { char: '|', attr: red }, U, U],
                    [U, { char: '|', attr: white }, U, U],
                    [U, { char: '|', attr: red }, U, U],
                    [U, { char: '|', attr: white }, U, U]
                ],
                [
                    [U, { char: '_', attr: white }, { char: '_', attr: red }, { char: '/', attr: white }, { char: ')', attr: red }],
                    [{ char: '(', attr: red }, { char: '_', attr: white }, { char: '|', attr: red }, U, U],
                    [U, U, { char: '|', attr: white }, U, U],
                    [U, U, { char: '|', attr: red }, U, U],
                    [U, U, { char: '|', attr: white }, U, U],
                    [U, U, { char: '|', attr: red }, U, U]
                ],
                [
                    [U, { char: '_', attr: white }, { char: '_', attr: red }, { char: '/', attr: white }, { char: ')', attr: red }],
                    [{ char: '(', attr: red }, { char: '_', attr: white }, { char: '|', attr: red }, U, U],
                    [U, U, { char: '|', attr: white }, U, U],
                    [U, U, { char: '|', attr: red }, U, U],
                    [U, U, { char: '|', attr: white }, U, U],
                    [U, U, { char: '|', attr: red }, U, U]
                ]
            ]
        };
    },
    createGummyBear: function () {
        var body = makeAttr(LIGHTGREEN, BG_BLACK);
        var bodyDark = makeAttr(GREEN, BG_BLACK);
        var eye = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'gummy_bear',
            variants: [
                [
                    [{ char: '(', attr: body }, { char: ')', attr: body }],
                    [{ char: GLYPH.LOWER_HALF, attr: bodyDark }, { char: GLYPH.LOWER_HALF, attr: bodyDark }]
                ],
                [
                    [{ char: 'o', attr: body }, U, { char: 'o', attr: body }],
                    [{ char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: bodyDark }, { char: ')', attr: body }],
                    [{ char: '/', attr: bodyDark }, U, { char: '\\', attr: bodyDark }]
                ],
                [
                    [{ char: '(', attr: body }, { char: ')', attr: body }, U, { char: '(', attr: body }, { char: ')', attr: body }],
                    [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: bodyDark }, { char: ')', attr: body }, U],
                    [U, { char: '(', attr: bodyDark }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: bodyDark }, U],
                    [U, { char: '/', attr: bodyDark }, U, { char: '\\', attr: bodyDark }, U]
                ],
                [
                    [{ char: '(', attr: body }, { char: ')', attr: body }, U, { char: '(', attr: body }, { char: ')', attr: body }],
                    [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: bodyDark }, { char: ')', attr: body }, U],
                    [U, { char: '.', attr: eye }, { char: 'u', attr: bodyDark }, { char: '.', attr: eye }, U],
                    [U, { char: '(', attr: bodyDark }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: bodyDark }, U],
                    [U, { char: '/', attr: bodyDark }, U, { char: '\\', attr: bodyDark }, U]
                ],
                [
                    [{ char: '(', attr: body }, { char: ')', attr: body }, U, { char: '(', attr: body }, { char: ')', attr: body }],
                    [U, { char: '(', attr: body }, { char: GLYPH.FULL_BLOCK, attr: bodyDark }, { char: ')', attr: body }, U],
                    [U, { char: '.', attr: eye }, { char: 'u', attr: bodyDark }, { char: '.', attr: eye }, U],
                    [U, { char: '(', attr: bodyDark }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ')', attr: bodyDark }, U],
                    [U, { char: '/', attr: bodyDark }, U, { char: '\\', attr: bodyDark }, U]
                ]
            ]
        };
    },
    createCupcake: function () {
        var frosting = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var frostingAlt = makeAttr(LIGHTCYAN, BG_BLACK);
        var wrapper = makeAttr(BROWN, BG_BLACK);
        var cherry = makeAttr(LIGHTRED, BG_BLACK);
        var U = null;
        return {
            name: 'cupcake',
            variants: [
                [
                    [{ char: GLYPH.UPPER_HALF, attr: frosting }, { char: GLYPH.UPPER_HALF, attr: frosting }],
                    [{ char: GLYPH.FULL_BLOCK, attr: wrapper }, { char: GLYPH.FULL_BLOCK, attr: wrapper }]
                ],
                [
                    [U, { char: 'o', attr: cherry }, U],
                    [{ char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }],
                    [{ char: '\\', attr: wrapper }, { char: GLYPH.FULL_BLOCK, attr: wrapper }, { char: '/', attr: wrapper }]
                ],
                [
                    [U, { char: 'o', attr: cherry }, U, U],
                    [{ char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }],
                    [{ char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }],
                    [{ char: '\\', attr: wrapper }, { char: GLYPH.FULL_BLOCK, attr: wrapper }, { char: GLYPH.FULL_BLOCK, attr: wrapper }, { char: '/', attr: wrapper }]
                ],
                [
                    [U, U, { char: 'o', attr: cherry }, U, U],
                    [U, { char: '/', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '\\', attr: frosting }, U],
                    [{ char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }],
                    [{ char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }],
                    [U, { char: '\\', attr: wrapper }, { char: GLYPH.FULL_BLOCK, attr: wrapper }, { char: '/', attr: wrapper }, U]
                ],
                [
                    [U, U, { char: 'o', attr: cherry }, U, U],
                    [U, { char: '/', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '\\', attr: frosting }, U],
                    [{ char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }],
                    [{ char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }, { char: '~', attr: frosting }, { char: '~', attr: frostingAlt }],
                    [U, { char: '\\', attr: wrapper }, { char: GLYPH.FULL_BLOCK, attr: wrapper }, { char: '/', attr: wrapper }, U]
                ]
            ]
        };
    },
    createIceCream: function () {
        var scoop1 = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var scoop2 = makeAttr(BROWN, BG_BLACK);
        var scoop3 = makeAttr(WHITE, BG_BLACK);
        var cone = makeAttr(YELLOW, BG_BLACK);
        var coneDark = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'ice_cream',
            variants: [
                [
                    [{ char: '(', attr: scoop1 }, { char: ')', attr: scoop1 }],
                    [{ char: '\\', attr: cone }, { char: '/', attr: cone }],
                    [U, { char: 'V', attr: coneDark }]
                ],
                [
                    [U, { char: '@', attr: scoop1 }, U],
                    [{ char: '(', attr: scoop2 }, { char: '@', attr: scoop3 }, { char: ')', attr: scoop2 }],
                    [U, { char: '\\', attr: cone }, { char: '/', attr: cone }],
                    [U, U, { char: 'V', attr: coneDark }]
                ],
                [
                    [U, { char: '@', attr: scoop1 }, { char: '@', attr: scoop1 }, U],
                    [{ char: '(', attr: scoop2 }, { char: '@', attr: scoop3 }, { char: '@', attr: scoop2 }, { char: ')', attr: scoop3 }],
                    [U, { char: '(', attr: scoop2 }, { char: ')', attr: scoop3 }, U],
                    [U, { char: '\\', attr: cone }, { char: '/', attr: cone }, U],
                    [U, U, { char: 'V', attr: coneDark }, U]
                ],
                [
                    [U, { char: '(', attr: scoop1 }, { char: '@', attr: scoop1 }, { char: ')', attr: scoop1 }, U],
                    [{ char: '(', attr: scoop2 }, { char: '@', attr: scoop2 }, { char: '@', attr: scoop3 }, { char: '@', attr: scoop2 }, { char: ')', attr: scoop3 }],
                    [U, { char: '(', attr: scoop3 }, { char: '@', attr: scoop2 }, { char: ')', attr: scoop3 }, U],
                    [U, { char: '\\', attr: cone }, { char: '#', attr: cone }, { char: '/', attr: cone }, U],
                    [U, U, { char: '\\', attr: coneDark }, { char: '/', attr: coneDark }, U],
                    [U, U, U, { char: 'V', attr: coneDark }, U]
                ],
                [
                    [U, { char: '(', attr: scoop1 }, { char: '@', attr: scoop1 }, { char: ')', attr: scoop1 }, U],
                    [{ char: '(', attr: scoop2 }, { char: '@', attr: scoop2 }, { char: '@', attr: scoop3 }, { char: '@', attr: scoop2 }, { char: ')', attr: scoop3 }],
                    [U, { char: '(', attr: scoop3 }, { char: '@', attr: scoop2 }, { char: ')', attr: scoop3 }, U],
                    [U, { char: '\\', attr: cone }, { char: '#', attr: cone }, { char: '/', attr: cone }, U],
                    [U, U, { char: '\\', attr: coneDark }, { char: '/', attr: coneDark }, U],
                    [U, U, U, { char: 'V', attr: coneDark }, U]
                ]
            ]
        };
    },
    createCottonCandy: function () {
        var fluff1 = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var fluff2 = makeAttr(LIGHTCYAN, BG_BLACK);
        var stick = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'cotton_candy',
            variants: [
                [
                    [{ char: '(', attr: fluff1 }, { char: ')', attr: fluff2 }],
                    [{ char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }],
                    [U, { char: '|', attr: stick }]
                ],
                [
                    [{ char: '(', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: ')', attr: fluff1 }],
                    [{ char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }],
                    [U, { char: '@', attr: fluff1 }, U],
                    [U, { char: '|', attr: stick }, U]
                ],
                [
                    [U, { char: '(', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: ')', attr: fluff1 }, U],
                    [{ char: '(', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: ')', attr: fluff2 }],
                    [{ char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }],
                    [U, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, U],
                    [U, U, { char: '|', attr: stick }, U, U]
                ],
                [
                    [U, { char: '(', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: ')', attr: fluff1 }, U],
                    [{ char: '(', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: ')', attr: fluff2 }],
                    [{ char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }],
                    [{ char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }],
                    [U, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, U],
                    [U, U, U, { char: '|', attr: stick }, U, U]
                ],
                [
                    [U, { char: '(', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: ')', attr: fluff1 }, U],
                    [{ char: '(', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: ')', attr: fluff2 }],
                    [{ char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }],
                    [{ char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }],
                    [U, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, { char: '@', attr: fluff2 }, { char: '@', attr: fluff1 }, U],
                    [U, U, U, { char: '|', attr: stick }, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('lollipop', CandySprites.createLollipop);
registerRoadsideSprite('candy_cane', CandySprites.createCandyCane);
registerRoadsideSprite('gummy_bear', CandySprites.createGummyBear);
registerRoadsideSprite('cupcake', CandySprites.createCupcake);
registerRoadsideSprite('ice_cream', CandySprites.createIceCream);
registerRoadsideSprite('cotton_candy', CandySprites.createCottonCandy);
"use strict";
var SpaceSprites = {
    createStar: function () {
        var core = makeAttr(WHITE, BG_BLACK);
        var glow = makeAttr(YELLOW, BG_BLACK);
        var glowDim = makeAttr(LIGHTCYAN, BG_BLACK);
        var U = null;
        return {
            name: 'star',
            variants: [
                [
                    [{ char: '*', attr: core }]
                ],
                [
                    [U, { char: '.', attr: glow }, U],
                    [{ char: '-', attr: glow }, { char: '*', attr: core }, { char: '-', attr: glow }],
                    [U, { char: "'", attr: glow }, U]
                ],
                [
                    [U, U, { char: '.', attr: glowDim }, U, U],
                    [U, { char: '\\', attr: glow }, { char: '|', attr: core }, { char: '/', attr: glow }, U],
                    [{ char: '-', attr: glow }, { char: '-', attr: core }, { char: '*', attr: core }, { char: '-', attr: core }, { char: '-', attr: glow }],
                    [U, { char: '/', attr: glow }, { char: '|', attr: core }, { char: '\\', attr: glow }, U],
                    [U, U, { char: "'", attr: glowDim }, U, U]
                ],
                [
                    [U, U, { char: '.', attr: glowDim }, U, U],
                    [U, { char: '\\', attr: glow }, { char: '|', attr: core }, { char: '/', attr: glow }, U],
                    [{ char: '-', attr: glow }, { char: '-', attr: core }, { char: '*', attr: core }, { char: '-', attr: core }, { char: '-', attr: glow }],
                    [U, { char: '/', attr: glow }, { char: '|', attr: core }, { char: '\\', attr: glow }, U],
                    [U, U, { char: "'", attr: glowDim }, U, U]
                ],
                [
                    [U, U, { char: '.', attr: glowDim }, U, U],
                    [U, { char: '\\', attr: glow }, { char: '|', attr: core }, { char: '/', attr: glow }, U],
                    [{ char: '-', attr: glow }, { char: '-', attr: core }, { char: '*', attr: core }, { char: '-', attr: core }, { char: '-', attr: glow }],
                    [U, { char: '/', attr: glow }, { char: '|', attr: core }, { char: '\\', attr: glow }, U],
                    [U, U, { char: "'", attr: glowDim }, U, U]
                ]
            ]
        };
    },
    createMoon: function () {
        var moonLight = makeAttr(WHITE, BG_BLACK);
        var moonMid = makeAttr(LIGHTGRAY, BG_BLACK);
        var moonDark = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'moon',
            variants: [
                [
                    [{ char: '(', attr: moonLight }, { char: ')', attr: moonMid }],
                    [{ char: '(', attr: moonMid }, { char: ')', attr: moonDark }]
                ],
                [
                    [U, { char: '_', attr: moonLight }, U],
                    [{ char: '(', attr: moonLight }, { char: ' ', attr: moonMid }, { char: ')', attr: moonMid }],
                    [U, { char: '-', attr: moonDark }, U]
                ],
                [
                    [U, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonMid }, U],
                    [{ char: '/', attr: moonLight }, { char: ' ', attr: moonLight }, { char: 'o', attr: moonMid }, { char: ' ', attr: moonMid }, { char: '\\', attr: moonMid }],
                    [{ char: '\\', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonDark }, { char: 'o', attr: moonDark }, { char: '/', attr: moonDark }],
                    [U, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, U]
                ],
                [
                    [U, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonMid }, U],
                    [{ char: '/', attr: moonLight }, { char: ' ', attr: moonLight }, { char: ' ', attr: moonLight }, { char: 'o', attr: moonMid }, { char: ' ', attr: moonMid }, { char: '\\', attr: moonMid }],
                    [{ char: '|', attr: moonLight }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonDark }, { char: '|', attr: moonDark }],
                    [{ char: '\\', attr: moonMid }, { char: ' ', attr: moonDark }, { char: 'o', attr: moonDark }, { char: ' ', attr: moonDark }, { char: ' ', attr: moonDark }, { char: '/', attr: moonDark }],
                    [U, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, U]
                ],
                [
                    [U, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonLight }, { char: '_', attr: moonMid }, U],
                    [{ char: '/', attr: moonLight }, { char: ' ', attr: moonLight }, { char: ' ', attr: moonLight }, { char: 'o', attr: moonMid }, { char: ' ', attr: moonMid }, { char: '\\', attr: moonMid }],
                    [{ char: '|', attr: moonLight }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonMid }, { char: ' ', attr: moonDark }, { char: '|', attr: moonDark }],
                    [{ char: '\\', attr: moonMid }, { char: ' ', attr: moonDark }, { char: 'o', attr: moonDark }, { char: ' ', attr: moonDark }, { char: ' ', attr: moonDark }, { char: '/', attr: moonDark }],
                    [U, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, { char: '-', attr: moonDark }, U]
                ]
            ]
        };
    },
    createPlanet: function () {
        var planet = makeAttr(LIGHTRED, BG_BLACK);
        var planetDark = makeAttr(RED, BG_BLACK);
        var ring = makeAttr(YELLOW, BG_BLACK);
        var ringDark = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'planet',
            variants: [
                [
                    [{ char: '(', attr: planet }, { char: ')', attr: planetDark }],
                    [{ char: '-', attr: ring }, { char: '-', attr: ringDark }]
                ],
                [
                    [U, { char: '(', attr: planet }, { char: ')', attr: planetDark }, U],
                    [{ char: '-', attr: ring }, { char: '-', attr: ring }, { char: '-', attr: ringDark }, { char: '-', attr: ringDark }],
                    [U, { char: '(', attr: planetDark }, { char: ')', attr: planetDark }, U]
                ],
                [
                    [U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: '\\', attr: planetDark }, U],
                    [{ char: '-', attr: ring }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }],
                    [{ char: '-', attr: ringDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }],
                    [U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U]
                ],
                [
                    [U, U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: '\\', attr: planetDark }, U, U],
                    [U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '\\', attr: planetDark }, U],
                    [{ char: '-', attr: ring }, { char: '-', attr: ring }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }, { char: '-', attr: ringDark }],
                    [U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U],
                    [U, U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U, U]
                ],
                [
                    [U, U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: '\\', attr: planetDark }, U, U],
                    [U, { char: '/', attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planet }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '\\', attr: planetDark }, U],
                    [{ char: '-', attr: ring }, { char: '-', attr: ring }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '-', attr: ringDark }, { char: '-', attr: ringDark }],
                    [U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U],
                    [U, U, { char: '\\', attr: planetDark }, { char: GLYPH.FULL_BLOCK, attr: planetDark }, { char: '/', attr: planetDark }, U, U]
                ]
            ]
        };
    },
    createComet: function () {
        var core = makeAttr(WHITE, BG_BLACK);
        var coreGlow = makeAttr(LIGHTCYAN, BG_BLACK);
        var tail1 = makeAttr(LIGHTBLUE, BG_BLACK);
        var tail2 = makeAttr(BLUE, BG_BLACK);
        var U = null;
        return {
            name: 'comet',
            variants: [
                [
                    [{ char: '-', attr: tail2 }, { char: '=', attr: tail1 }, { char: '*', attr: core }]
                ],
                [
                    [U, U, { char: '/', attr: coreGlow }, { char: '*', attr: core }],
                    [{ char: '-', attr: tail2 }, { char: '~', attr: tail1 }, { char: '\\', attr: coreGlow }, U]
                ],
                [
                    [U, U, U, { char: '/', attr: coreGlow }, { char: 'O', attr: core }, { char: ')', attr: coreGlow }],
                    [{ char: '-', attr: tail2 }, { char: '~', attr: tail2 }, { char: '=', attr: tail1 }, { char: '=', attr: coreGlow }, { char: '\\', attr: coreGlow }, U],
                    [U, U, U, { char: '~', attr: tail1 }, U, U]
                ],
                [
                    [U, U, U, U, { char: '.', attr: coreGlow }, { char: '/', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }],
                    [{ char: '-', attr: tail2 }, { char: '-', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '=', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }, U],
                    [U, U, { char: '~', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '\\', attr: coreGlow }, U, U],
                    [U, U, U, U, { char: '~', attr: tail1 }, U, U, U]
                ],
                [
                    [U, U, U, U, { char: '.', attr: coreGlow }, { char: '/', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }],
                    [{ char: '-', attr: tail2 }, { char: '-', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '=', attr: coreGlow }, { char: '@', attr: core }, { char: ')', attr: coreGlow }, U],
                    [U, U, { char: '~', attr: tail2 }, { char: '~', attr: tail1 }, { char: '=', attr: tail1 }, { char: '\\', attr: coreGlow }, U, U],
                    [U, U, U, U, { char: '~', attr: tail1 }, U, U, U]
                ]
            ]
        };
    },
    createNebula: function () {
        var gas1 = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var gas2 = makeAttr(MAGENTA, BG_BLACK);
        var gas3 = makeAttr(LIGHTBLUE, BG_BLACK);
        var gas4 = makeAttr(BLUE, BG_BLACK);
        var star = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'nebula',
            variants: [
                [
                    [{ char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }]
                ],
                [
                    [U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.LIGHT_SHADE, attr: gas2 }, U],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }],
                    [U, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, U]
                ],
                [
                    [U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
                    [{ char: GLYPH.LIGHT_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }],
                    [U, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U]
                ],
                [
                    [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, U, U],
                    [U, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
                    [{ char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }],
                    [U, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
                    [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, U, U]
                ],
                [
                    [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas1 }, U, U],
                    [U, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
                    [{ char: GLYPH.LIGHT_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: GLYPH.MEDIUM_SHADE, attr: gas1 }, { char: GLYPH.MEDIUM_SHADE, attr: gas2 }, { char: '*', attr: star }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }],
                    [U, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.MEDIUM_SHADE, attr: gas3 }, { char: GLYPH.MEDIUM_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, U],
                    [U, U, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, { char: GLYPH.LIGHT_SHADE, attr: gas3 }, { char: GLYPH.LIGHT_SHADE, attr: gas4 }, U, U]
                ]
            ]
        };
    },
    createSatellite: function () {
        var body = makeAttr(LIGHTGRAY, BG_BLACK);
        var bodyDark = makeAttr(DARKGRAY, BG_BLACK);
        var panel = makeAttr(LIGHTBLUE, BG_BLACK);
        var panelDark = makeAttr(BLUE, BG_BLACK);
        var antenna = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'satellite',
            variants: [
                [
                    [{ char: '=', attr: panel }, { char: '[', attr: body }, { char: '=', attr: panel }],
                    [U, { char: '|', attr: antenna }, U]
                ],
                [
                    [U, U, { char: '/', attr: antenna }, U, U],
                    [{ char: '=', attr: panel }, { char: '=', attr: panelDark }, { char: '[', attr: body }, { char: '=', attr: panelDark }, { char: '=', attr: panel }],
                    [U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U]
                ],
                [
                    [U, U, U, { char: '/', attr: antenna }, { char: ')', attr: antenna }, U, U],
                    [{ char: '/', attr: panel }, { char: '#', attr: panel }, { char: '\\', attr: panelDark }, { char: '[', attr: body }, { char: '/', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }],
                    [{ char: '\\', attr: panelDark }, { char: '#', attr: panelDark }, { char: '/', attr: panel }, { char: ']', attr: bodyDark }, { char: '\\', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }],
                    [U, U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U, U]
                ],
                [
                    [U, U, U, U, { char: '/', attr: antenna }, { char: ')', attr: antenna }, U, U, U],
                    [U, U, U, U, { char: '[', attr: body }, { char: ']', attr: body }, U, U, U],
                    [{ char: '/', attr: panel }, { char: '#', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }, { char: '[', attr: body }, { char: ']', attr: bodyDark }, { char: '\\', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }],
                    [{ char: '\\', attr: panelDark }, { char: '#', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }, { char: '[', attr: bodyDark }, { char: ']', attr: bodyDark }, { char: '/', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }],
                    [U, U, U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U, U]
                ],
                [
                    [U, U, U, U, { char: '/', attr: antenna }, { char: ')', attr: antenna }, U, U, U],
                    [U, U, U, U, { char: '[', attr: body }, { char: ']', attr: body }, U, U, U],
                    [{ char: '/', attr: panel }, { char: '#', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }, { char: '[', attr: body }, { char: ']', attr: bodyDark }, { char: '\\', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }],
                    [{ char: '\\', attr: panelDark }, { char: '#', attr: panelDark }, { char: '#', attr: panel }, { char: '\\', attr: panel }, { char: '[', attr: bodyDark }, { char: ']', attr: bodyDark }, { char: '/', attr: panel }, { char: '#', attr: panelDark }, { char: '/', attr: panelDark }],
                    [U, U, U, U, { char: GLYPH.LOWER_HALF, attr: bodyDark }, { char: GLYPH.LOWER_HALF, attr: bodyDark }, U, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('star', SpaceSprites.createStar);
registerRoadsideSprite('moon', SpaceSprites.createMoon);
registerRoadsideSprite('planet', SpaceSprites.createPlanet);
registerRoadsideSprite('comet', SpaceSprites.createComet);
registerRoadsideSprite('nebula', SpaceSprites.createNebula);
registerRoadsideSprite('satellite', SpaceSprites.createSatellite);
"use strict";
var CastleSprites = {
    createTower: function () {
        var stone = makeAttr(DARKGRAY, BG_BLACK);
        var stoneDark = makeAttr(BLACK, BG_BLACK);
        var stoneLight = makeAttr(LIGHTGRAY, BG_BLACK);
        var roof = makeAttr(BROWN, BG_BLACK);
        var window = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'tower',
            variants: [
                [
                    [{ char: '/', attr: roof }, { char: '\\', attr: roof }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }]
                ],
                [
                    [U, { char: '^', attr: roof }, U],
                    [{ char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
                ],
                [
                    [U, U, { char: '^', attr: roof }, U, U],
                    [U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U],
                    [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
                ],
                [
                    [U, U, U, { char: '^', attr: roof }, U, U, U],
                    [U, U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U, U],
                    [U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U],
                    [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [U, U, U, { char: '^', attr: roof }, U, U, U],
                    [U, U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U, U],
                    [U, { char: '/', attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: GLYPH.FULL_BLOCK, attr: roof }, { char: '\\', attr: roof }, U],
                    [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: ' ', attr: window }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ]
            ]
        };
    },
    createBattlement: function () {
        var stone = makeAttr(DARKGRAY, BG_BLACK);
        var stoneDark = makeAttr(BLACK, BG_BLACK);
        var stoneLight = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'battlement',
            variants: [
                [
                    [{ char: 'n', attr: stone }, U, { char: 'n', attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: 'n', attr: stoneLight }, U, { char: 'n', attr: stoneLight }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, U, U, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, U, U, U, U, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
                ],
                [
                    [{ char: 'n', attr: stoneLight }, { char: 'n', attr: stone }, U, U, U, U, { char: 'n', attr: stone }, { char: 'n', attr: stoneLight }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }]
                ]
            ]
        };
    },
    createTorch: function () {
        var flame = makeAttr(YELLOW, BG_BLACK);
        var flameOuter = makeAttr(LIGHTRED, BG_BLACK);
        var handle = makeAttr(BROWN, BG_BLACK);
        var bracket = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'torch',
            variants: [
                [
                    [{ char: '*', attr: flame }],
                    [{ char: '|', attr: handle }],
                    [{ char: '+', attr: bracket }]
                ],
                [
                    [U, { char: '*', attr: flame }, U],
                    [{ char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }],
                    [U, { char: '|', attr: handle }, U],
                    [{ char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }]
                ],
                [
                    [U, { char: '^', attr: flameOuter }, U],
                    [{ char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }],
                    [U, { char: '|', attr: handle }, U],
                    [U, { char: '|', attr: handle }, U],
                    [{ char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }]
                ],
                [
                    [U, U, { char: '^', attr: flameOuter }, U, U],
                    [U, { char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }, U],
                    [{ char: '~', attr: flame }, { char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: flame }, { char: ')', attr: flame }, { char: '~', attr: flame }],
                    [U, U, { char: '|', attr: handle }, U, U],
                    [U, U, { char: '|', attr: handle }, U, U],
                    [U, { char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }, U]
                ],
                [
                    [U, U, { char: '^', attr: flameOuter }, U, U],
                    [U, { char: '(', attr: flameOuter }, { char: '*', attr: flame }, { char: ')', attr: flameOuter }, U],
                    [{ char: '~', attr: flame }, { char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: flame }, { char: ')', attr: flame }, { char: '~', attr: flame }],
                    [U, U, { char: '|', attr: handle }, U, U],
                    [U, U, { char: '|', attr: handle }, U, U],
                    [U, { char: '-', attr: bracket }, { char: '+', attr: bracket }, { char: '-', attr: bracket }, U]
                ]
            ]
        };
    },
    createBanner: function () {
        var banner = makeAttr(LIGHTRED, BG_BLACK);
        var bannerDark = makeAttr(RED, BG_BLACK);
        var pole = makeAttr(BROWN, BG_BLACK);
        var emblem = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'banner',
            variants: [
                [
                    [{ char: '-', attr: pole }, { char: '-', attr: pole }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [{ char: '\\', attr: banner }, { char: '/', attr: bannerDark }]
                ],
                [
                    [{ char: '-', attr: pole }, { char: 'o', attr: pole }, { char: '-', attr: pole }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: '*', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [U, { char: 'V', attr: banner }, U]
                ],
                [
                    [{ char: '-', attr: pole }, { char: '-', attr: pole }, { char: 'o', attr: pole }, { char: '-', attr: pole }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: '*', attr: emblem }, { char: '*', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: banner }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [U, { char: '\\', attr: banner }, { char: '/', attr: bannerDark }, U]
                ],
                [
                    [{ char: '-', attr: pole }, { char: '-', attr: pole }, { char: 'O', attr: pole }, { char: '-', attr: pole }, { char: '-', attr: pole }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }],
                    [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: '/', attr: emblem }, { char: '*', attr: emblem }, { char: '\\', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: '\\', attr: emblem }, { char: '*', attr: emblem }, { char: '/', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: banner }],
                    [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [U, { char: '\\', attr: banner }, { char: 'V', attr: bannerDark }, { char: '/', attr: banner }, U]
                ],
                [
                    [{ char: '-', attr: pole }, { char: '-', attr: pole }, { char: 'O', attr: pole }, { char: '-', attr: pole }, { char: '-', attr: pole }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }],
                    [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: '/', attr: emblem }, { char: '*', attr: emblem }, { char: '\\', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: banner }, { char: '\\', attr: emblem }, { char: '*', attr: emblem }, { char: '/', attr: emblem }, { char: GLYPH.FULL_BLOCK, attr: banner }],
                    [{ char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }, { char: GLYPH.FULL_BLOCK, attr: banner }, { char: GLYPH.FULL_BLOCK, attr: bannerDark }],
                    [U, { char: '\\', attr: banner }, { char: 'V', attr: bannerDark }, { char: '/', attr: banner }, U]
                ]
            ]
        };
    },
    createGargoyle: function () {
        var stone = makeAttr(DARKGRAY, BG_BLACK);
        var stoneDark = makeAttr(BLACK, BG_BLACK);
        var eye = makeAttr(RED, BG_BLACK);
        var U = null;
        return {
            name: 'gargoyle',
            variants: [
                [
                    [{ char: '(', attr: stone }, { char: ')', attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [{ char: '/', attr: stone }, { char: '^', attr: stone }, { char: '\\', attr: stone }],
                    [{ char: '(', attr: stoneDark }, { char: 'v', attr: stone }, { char: ')', attr: stoneDark }],
                    [{ char: '/', attr: stone }, U, { char: '\\', attr: stone }]
                ],
                [
                    [{ char: '/', attr: stone }, { char: '^', attr: stone }, U, { char: '^', attr: stone }, { char: '\\', attr: stone }],
                    [{ char: '(', attr: stoneDark }, { char: '.', attr: eye }, { char: 'v', attr: stone }, { char: '.', attr: eye }, { char: ')', attr: stoneDark }],
                    [U, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stoneDark }, U, { char: '|', attr: stone }, U, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: stone }, { char: '^', attr: stone }, U, { char: '^', attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stone }, { char: '(', attr: stoneDark }, { char: '*', attr: eye }, { char: 'w', attr: stone }, { char: '*', attr: eye }, { char: ')', attr: stoneDark }, { char: '\\', attr: stone }],
                    [U, { char: '/', attr: stoneDark }, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '\\', attr: stone }, { char: '\\', attr: stoneDark }, U],
                    [{ char: '/', attr: stone }, U, { char: '(', attr: stoneDark }, { char: '|', attr: stone }, { char: ')', attr: stoneDark }, U, { char: '\\', attr: stone }],
                    [{ char: '/', attr: stoneDark }, U, U, { char: '|', attr: stoneDark }, U, U, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: stone }, { char: '^', attr: stone }, U, { char: '^', attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stone }, { char: '(', attr: stoneDark }, { char: '*', attr: eye }, { char: 'w', attr: stone }, { char: '*', attr: eye }, { char: ')', attr: stoneDark }, { char: '\\', attr: stone }],
                    [U, { char: '/', attr: stoneDark }, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '\\', attr: stone }, { char: '\\', attr: stoneDark }, U],
                    [{ char: '/', attr: stone }, U, { char: '(', attr: stoneDark }, { char: '|', attr: stone }, { char: ')', attr: stoneDark }, U, { char: '\\', attr: stone }],
                    [{ char: '/', attr: stoneDark }, U, U, { char: '|', attr: stoneDark }, U, U, { char: '\\', attr: stoneDark }]
                ]
            ]
        };
    },
    createPortcullis: function () {
        var iron = makeAttr(DARKGRAY, BG_BLACK);
        var ironLight = makeAttr(LIGHTGRAY, BG_BLACK);
        var stone = makeAttr(BROWN, BG_BLACK);
        return {
            name: 'portcullis',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: '^', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '#', attr: iron }, { char: '#', attr: iron }, { char: '#', attr: iron }],
                    [{ char: '#', attr: ironLight }, { char: '#', attr: ironLight }, { char: '#', attr: ironLight }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '|', attr: ironLight }, { char: '#', attr: ironLight }, { char: '#', attr: ironLight }, { char: '|', attr: ironLight }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '^', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '/', attr: stone }, { char: '^', attr: stone }, { char: '\\', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }],
                    [{ char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }, { char: '-', attr: ironLight }, { char: '+', attr: ironLight }],
                    [{ char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }, { char: '#', attr: iron }, { char: '|', attr: iron }]
                ]
            ]
        };
    }
};
registerRoadsideSprite('tower', CastleSprites.createTower);
registerRoadsideSprite('battlement', CastleSprites.createBattlement);
registerRoadsideSprite('torch', CastleSprites.createTorch);
registerRoadsideSprite('banner', CastleSprites.createBanner);
registerRoadsideSprite('gargoyle', CastleSprites.createGargoyle);
registerRoadsideSprite('portcullis', CastleSprites.createPortcullis);
"use strict";
var VillainSprites = {
    createLavaRock: function () {
        var rock = makeAttr(DARKGRAY, BG_BLACK);
        var rockDark = makeAttr(BLACK, BG_BLACK);
        var lava = makeAttr(LIGHTRED, BG_BLACK);
        var lavaGlow = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'lava_rock',
            variants: [
                [
                    [{ char: '(', attr: rock }, { char: ')', attr: rock }],
                    [{ char: GLYPH.LOWER_HALF, attr: lava }, { char: GLYPH.LOWER_HALF, attr: lava }]
                ],
                [
                    [U, { char: '^', attr: rock }, U],
                    [{ char: '(', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: ')', attr: rockDark }],
                    [{ char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }]
                ],
                [
                    [U, { char: '/', attr: rock }, { char: '^', attr: rockDark }, { char: '\\', attr: rock }, U],
                    [{ char: '/', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '\\', attr: rockDark }],
                    [{ char: '(', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '*', attr: lavaGlow }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: ')', attr: rock }],
                    [{ char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }]
                ],
                [
                    [U, U, { char: '/', attr: rock }, { char: '^', attr: rockDark }, { char: '\\', attr: rock }, U, U],
                    [U, { char: '/', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '\\', attr: rockDark }, U],
                    [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '*', attr: lavaGlow }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '\\', attr: rock }],
                    [{ char: '(', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '*', attr: lava }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: ')', attr: rockDark }],
                    [{ char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }]
                ],
                [
                    [U, U, { char: '/', attr: rock }, { char: '^', attr: rockDark }, { char: '\\', attr: rock }, U, U],
                    [U, { char: '/', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '\\', attr: rockDark }, U],
                    [{ char: '/', attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: '*', attr: lavaGlow }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '\\', attr: rock }],
                    [{ char: '(', attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: '*', attr: lava }, { char: GLYPH.FULL_BLOCK, attr: rockDark }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: ')', attr: rockDark }],
                    [{ char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }, { char: '~', attr: lava }, { char: '~', attr: lavaGlow }]
                ]
            ]
        };
    },
    createFlame: function () {
        var core = makeAttr(YELLOW, BG_BLACK);
        var mid = makeAttr(LIGHTRED, BG_BLACK);
        var outer = makeAttr(RED, BG_BLACK);
        var smoke = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'flame',
            variants: [
                [
                    [{ char: '*', attr: core }],
                    [{ char: '^', attr: mid }],
                    [{ char: '~', attr: outer }]
                ],
                [
                    [U, { char: '.', attr: smoke }, U],
                    [U, { char: '*', attr: core }, U],
                    [{ char: '(', attr: mid }, { char: '^', attr: core }, { char: ')', attr: mid }],
                    [{ char: '~', attr: outer }, { char: '^', attr: mid }, { char: '~', attr: outer }]
                ],
                [
                    [U, U, { char: '.', attr: smoke }, U, U],
                    [U, { char: '(', attr: core }, { char: '*', attr: core }, { char: ')', attr: core }, U],
                    [{ char: '(', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: ')', attr: mid }],
                    [{ char: '(', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: ')', attr: outer }],
                    [U, { char: '~', attr: outer }, { char: '^', attr: mid }, { char: '~', attr: outer }, U]
                ],
                [
                    [U, U, { char: '.', attr: smoke }, { char: '.', attr: smoke }, { char: '.', attr: smoke }, U, U],
                    [U, U, { char: '(', attr: core }, { char: '*', attr: core }, { char: ')', attr: core }, U, U],
                    [U, { char: '(', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: ')', attr: mid }, U],
                    [{ char: '(', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: ')', attr: outer }],
                    [{ char: '(', attr: outer }, { char: '^', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: '^', attr: outer }, { char: ')', attr: outer }],
                    [U, { char: '~', attr: outer }, { char: '~', attr: mid }, { char: '^', attr: mid }, { char: '~', attr: mid }, { char: '~', attr: outer }, U]
                ],
                [
                    [U, U, { char: '.', attr: smoke }, { char: '.', attr: smoke }, { char: '.', attr: smoke }, U, U],
                    [U, U, { char: '(', attr: core }, { char: '*', attr: core }, { char: ')', attr: core }, U, U],
                    [U, { char: '(', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: ')', attr: mid }, U],
                    [{ char: '(', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '*', attr: core }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: ')', attr: outer }],
                    [{ char: '(', attr: outer }, { char: '^', attr: outer }, { char: '^', attr: mid }, { char: '^', attr: core }, { char: '^', attr: mid }, { char: '^', attr: outer }, { char: ')', attr: outer }],
                    [U, { char: '~', attr: outer }, { char: '~', attr: mid }, { char: '^', attr: mid }, { char: '~', attr: mid }, { char: '~', attr: outer }, U]
                ]
            ]
        };
    },
    createSkullPile: function () {
        var skull = makeAttr(WHITE, BG_BLACK);
        var skullDark = makeAttr(LIGHTGRAY, BG_BLACK);
        var bone = makeAttr(LIGHTGRAY, BG_BLACK);
        var eye = makeAttr(RED, BG_BLACK);
        var U = null;
        return {
            name: 'skull_pile',
            variants: [
                [
                    [{ char: '(', attr: skull }, { char: ')', attr: skull }],
                    [{ char: 'o', attr: skullDark }, { char: 'o', attr: skullDark }]
                ],
                [
                    [U, { char: '(', attr: skull }, { char: ')', attr: skull }],
                    [{ char: '(', attr: skullDark }, { char: 'o', attr: eye }, { char: ')', attr: skullDark }],
                    [{ char: '-', attr: bone }, { char: '-', attr: bone }, { char: '-', attr: bone }]
                ],
                [
                    [U, { char: '/', attr: skull }, { char: 'O', attr: skull }, { char: '\\', attr: skull }, U],
                    [{ char: '/', attr: skullDark }, { char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }, { char: '\\', attr: skullDark }],
                    [{ char: '(', attr: skull }, { char: 'o', attr: skullDark }, { char: '(', attr: skull }, { char: 'o', attr: skullDark }, { char: ')', attr: skull }],
                    [{ char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }]
                ],
                [
                    [U, U, { char: '/', attr: skull }, { char: 'O', attr: skull }, { char: '\\', attr: skull }, U, U],
                    [U, { char: '/', attr: skull }, { char: '*', attr: eye }, { char: 'v', attr: skull }, { char: '*', attr: eye }, { char: '\\', attr: skull }, U],
                    [{ char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }, U, { char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }],
                    [{ char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }, { char: '-', attr: bone }, { char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }],
                    [{ char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }]
                ],
                [
                    [U, U, { char: '/', attr: skull }, { char: 'O', attr: skull }, { char: '\\', attr: skull }, U, U],
                    [U, { char: '/', attr: skull }, { char: '*', attr: eye }, { char: 'v', attr: skull }, { char: '*', attr: eye }, { char: '\\', attr: skull }, U],
                    [{ char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }, U, { char: '/', attr: skullDark }, { char: 'O', attr: skull }, { char: '\\', attr: skullDark }],
                    [{ char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }, { char: '-', attr: bone }, { char: '.', attr: eye }, { char: 'v', attr: skull }, { char: '.', attr: eye }],
                    [{ char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }, { char: '=', attr: bone }, { char: '-', attr: bone }]
                ]
            ]
        };
    },
    createChain: function () {
        var chain = makeAttr(DARKGRAY, BG_BLACK);
        var chainLight = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'chain',
            variants: [
                [
                    [{ char: 'o', attr: chain }],
                    [{ char: 'O', attr: chainLight }],
                    [{ char: 'o', attr: chain }],
                    [{ char: 'O', attr: chainLight }]
                ],
                [
                    [{ char: '(', attr: chain }, { char: ')', attr: chain }],
                    [U, { char: 'O', attr: chainLight }],
                    [{ char: '(', attr: chain }, { char: ')', attr: chain }],
                    [U, { char: 'O', attr: chainLight }],
                    [{ char: '(', attr: chain }, { char: ')', attr: chain }]
                ],
                [
                    [{ char: '/', attr: chain }, { char: 'o', attr: chainLight }, { char: '\\', attr: chain }],
                    [U, { char: 'O', attr: chain }, U],
                    [{ char: '/', attr: chainLight }, { char: 'o', attr: chain }, { char: '\\', attr: chainLight }],
                    [U, { char: 'O', attr: chainLight }, U],
                    [{ char: '/', attr: chain }, { char: 'o', attr: chainLight }, { char: '\\', attr: chain }],
                    [U, { char: 'O', attr: chain }, U]
                ],
                [
                    [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
                    [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
                    [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
                    [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
                    [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
                    [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
                    [U, { char: 'V', attr: chain }, U]
                ],
                [
                    [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
                    [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
                    [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
                    [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
                    [{ char: '/', attr: chain }, { char: 'O', attr: chainLight }, { char: '\\', attr: chain }],
                    [{ char: '\\', attr: chainLight }, { char: 'o', attr: chain }, { char: '/', attr: chainLight }],
                    [U, { char: 'V', attr: chain }, U]
                ]
            ]
        };
    },
    createSpike: function () {
        var spike = makeAttr(LIGHTGRAY, BG_BLACK);
        var spikeDark = makeAttr(DARKGRAY, BG_BLACK);
        var blood = makeAttr(RED, BG_BLACK);
        var U = null;
        return {
            name: 'spike',
            variants: [
                [
                    [{ char: '^', attr: spike }, { char: '^', attr: spike }, { char: '^', attr: spike }],
                    [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }]
                ],
                [
                    [U, { char: '^', attr: spike }, { char: '^', attr: spike }, U],
                    [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
                    [{ char: '/', attr: spikeDark }, { char: '/', attr: spike }, { char: '\\', attr: spike }, { char: '\\', attr: spikeDark }]
                ],
                [
                    [U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U],
                    [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '^', attr: spike }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
                    [{ char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spike }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }],
                    [{ char: '/', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spike }, { char: '-', attr: spike }, { char: '\\', attr: spikeDark }]
                ],
                [
                    [U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U],
                    [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
                    [{ char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }],
                    [{ char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '.', attr: blood }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }],
                    [{ char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }]
                ],
                [
                    [U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U, { char: '^', attr: spike }, U],
                    [{ char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '/', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }, { char: '|', attr: spike }, { char: '\\', attr: spikeDark }],
                    [{ char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }],
                    [{ char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }, { char: '.', attr: blood }, { char: '|', attr: spike }, { char: '|', attr: spikeDark }, { char: '|', attr: spike }],
                    [{ char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }, { char: '-', attr: spike }, { char: '-', attr: spikeDark }]
                ]
            ]
        };
    },
    createCauldron: function () {
        var pot = makeAttr(DARKGRAY, BG_BLACK);
        var potDark = makeAttr(BLACK, BG_BLACK);
        var brew = makeAttr(LIGHTGREEN, BG_BLACK);
        var bubble = makeAttr(GREEN, BG_BLACK);
        var fire = makeAttr(LIGHTRED, BG_BLACK);
        var U = null;
        return {
            name: 'cauldron',
            variants: [
                [
                    [{ char: 'o', attr: bubble }, { char: 'o', attr: bubble }],
                    [{ char: '(', attr: pot }, { char: ')', attr: pot }],
                    [{ char: '^', attr: fire }, { char: '^', attr: fire }]
                ],
                [
                    [U, { char: 'o', attr: bubble }, U],
                    [{ char: '(', attr: pot }, { char: '~', attr: brew }, { char: ')', attr: pot }],
                    [{ char: '(', attr: potDark }, { char: '_', attr: pot }, { char: ')', attr: potDark }],
                    [{ char: '^', attr: fire }, { char: '^', attr: fire }, { char: '^', attr: fire }]
                ],
                [
                    [U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U],
                    [{ char: '/', attr: pot }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '\\', attr: pot }],
                    [{ char: '|', attr: pot }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '|', attr: pot }],
                    [{ char: '\\', attr: potDark }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '/', attr: potDark }],
                    [U, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, U]
                ],
                [
                    [U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U],
                    [U, { char: '/', attr: pot }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '\\', attr: pot }, U],
                    [{ char: '/', attr: pot }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '\\', attr: pot }],
                    [{ char: '|', attr: potDark }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '|', attr: potDark }],
                    [{ char: '\\', attr: potDark }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '/', attr: potDark }],
                    [U, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, U]
                ],
                [
                    [U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U, { char: 'o', attr: bubble }, U],
                    [U, { char: '/', attr: pot }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '~', attr: brew }, { char: '\\', attr: pot }, U],
                    [{ char: '/', attr: pot }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: 'o', attr: bubble }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '\\', attr: pot }],
                    [{ char: '|', attr: potDark }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: GLYPH.FULL_BLOCK, attr: brew }, { char: '|', attr: potDark }],
                    [{ char: '\\', attr: potDark }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '_', attr: pot }, { char: '/', attr: potDark }],
                    [U, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, { char: '*', attr: fire }, { char: '^', attr: fire }, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('lava_rock', VillainSprites.createLavaRock);
registerRoadsideSprite('flame', VillainSprites.createFlame);
registerRoadsideSprite('skull_pile', VillainSprites.createSkullPile);
registerRoadsideSprite('chain', VillainSprites.createChain);
registerRoadsideSprite('spike', VillainSprites.createSpike);
registerRoadsideSprite('cauldron', VillainSprites.createCauldron);
"use strict";
var RuinsSprites = {
    createColumn: function () {
        var stone = makeAttr(YELLOW, BG_BLACK);
        var stoneDark = makeAttr(BROWN, BG_BLACK);
        var stoneLight = makeAttr(WHITE, BG_BLACK);
        return {
            name: 'column',
            variants: [
                [
                    [{ char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stoneDark }],
                    [{ char: '=', attr: stone }, { char: '=', attr: stone }]
                ],
                [
                    [{ char: '/', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '\\', attr: stoneLight }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }],
                    [{ char: '\\', attr: stoneDark }, { char: '=', attr: stone }, { char: '/', attr: stoneDark }]
                ],
                [
                    [{ char: '/', attr: stoneLight }, { char: '-', attr: stoneLight }, { char: '-', attr: stoneLight }, { char: '\\', attr: stoneLight }],
                    [{ char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '\\', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '/', attr: stoneDark }]
                ],
                [
                    [{ char: '/', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '\\', attr: stoneLight }],
                    [{ char: '|', attr: stone }, { char: '(', attr: stone }, { char: ')', attr: stone }, { char: '(', attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: ')', attr: stoneDark }, { char: '(', attr: stoneDark }, { char: ')', attr: stoneDark }, { char: '|', attr: stone }],
                    [{ char: '\\', attr: stoneDark }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '/', attr: stoneDark }]
                ],
                [
                    [{ char: '/', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '=', attr: stoneLight }, { char: '\\', attr: stoneLight }],
                    [{ char: '|', attr: stone }, { char: '(', attr: stone }, { char: ')', attr: stone }, { char: '(', attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }, { char: '|', attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: ')', attr: stoneDark }, { char: '(', attr: stoneDark }, { char: ')', attr: stoneDark }, { char: '|', attr: stone }],
                    [{ char: '\\', attr: stoneDark }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '=', attr: stone }, { char: '/', attr: stoneDark }]
                ]
            ]
        };
    },
    createStatue: function () {
        var stone = makeAttr(YELLOW, BG_BLACK);
        var stoneDark = makeAttr(BROWN, BG_BLACK);
        var gold = makeAttr(YELLOW, BG_BROWN);
        var face = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'statue',
            variants: [
                [
                    [{ char: '/', attr: gold }, { char: '\\', attr: gold }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '^', attr: gold }, U],
                    [{ char: '/', attr: gold }, { char: 'O', attr: face }, { char: '\\', attr: gold }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: gold }, { char: '^', attr: gold }, { char: '\\', attr: gold }, U],
                    [{ char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: 'O', attr: face }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }],
                    [{ char: '|', attr: stone }, { char: '.', attr: face }, { char: 'v', attr: face }, { char: '.', attr: face }, { char: '|', attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, U, { char: '/', attr: gold }, { char: '^', attr: gold }, { char: '\\', attr: gold }, U, U],
                    [U, { char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }, U],
                    [{ char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '.', attr: face }, { char: 'v', attr: face }, { char: '.', attr: face }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }, { char: '|', attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, U, { char: '/', attr: gold }, { char: '^', attr: gold }, { char: '\\', attr: gold }, U, U],
                    [U, { char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }, U],
                    [{ char: '/', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '.', attr: face }, { char: 'v', attr: face }, { char: '.', attr: face }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: gold }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }, { char: '|', attr: stone }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
                ]
            ]
        };
    },
    createObelisk: function () {
        var stone = makeAttr(YELLOW, BG_BLACK);
        var stoneDark = makeAttr(BROWN, BG_BLACK);
        var top = makeAttr(WHITE, BG_BLACK);
        var glyph = makeAttr(RED, BG_BLACK);
        var U = null;
        return {
            name: 'obelisk',
            variants: [
                [
                    [{ char: '^', attr: top }],
                    [{ char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }]
                ],
                [
                    [{ char: '/', attr: top }, { char: '\\', attr: top }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stone }, { char: '|', attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '^', attr: top }, U],
                    [{ char: '/', attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stoneDark }],
                    [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: top }, { char: '\\', attr: top }, U],
                    [{ char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }],
                    [{ char: '|', attr: stone }, { char: '@', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '@', attr: glyph }, { char: '|', attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: top }, { char: '\\', attr: top }, U],
                    [{ char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }],
                    [{ char: '|', attr: stone }, { char: '@', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '*', attr: glyph }, { char: '|', attr: stone }],
                    [{ char: '|', attr: stone }, { char: '*', attr: glyph }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '|', attr: stoneDark }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '@', attr: glyph }, { char: '|', attr: stone }],
                    [{ char: '/', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '\\', attr: stoneDark }]
                ]
            ]
        };
    },
    createSphinx: function () {
        var stone = makeAttr(YELLOW, BG_BLACK);
        var stoneDark = makeAttr(BROWN, BG_BLACK);
        var face = makeAttr(BROWN, BG_BLACK);
        var eye = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'sphinx',
            variants: [
                [
                    [{ char: '/', attr: stone }, { char: 'O', attr: face }, { char: '\\', attr: stone }],
                    [{ char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stone }, { char: 'O', attr: face }, { char: 'v', attr: face }, { char: '\\', attr: stone }],
                    [{ char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }]
                ],
                [
                    [U, { char: '/', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stone }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: '.', attr: eye }, { char: '\\', attr: stone }],
                    [{ char: '|', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '>', attr: face }, { char: '<', attr: face }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '|', attr: stoneDark }],
                    [{ char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }]
                ],
                [
                    [U, U, { char: '/', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '\\', attr: stone }, U, U],
                    [U, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '>', attr: face }, { char: 'w', attr: face }, { char: '<', attr: face }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }],
                    [{ char: '-', attr: stone }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '-', attr: stone }, { char: '-', attr: stone }],
                    [{ char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }]
                ],
                [
                    [U, U, { char: '/', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '_', attr: stone }, { char: '\\', attr: stone }, U, U],
                    [U, { char: '/', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: face }, { char: '.', attr: eye }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stone }, U],
                    [{ char: '/', attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: '>', attr: face }, { char: 'w', attr: face }, { char: '<', attr: face }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '\\', attr: stoneDark }],
                    [{ char: '-', attr: stone }, { char: '-', attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: '-', attr: stone }, { char: '-', attr: stone }],
                    [{ char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stone }, { char: '-', attr: stoneDark }, { char: '-', attr: stoneDark }]
                ]
            ]
        };
    },
    createHieroglyph: function () {
        var stone = makeAttr(YELLOW, BG_BLACK);
        var stoneDark = makeAttr(BROWN, BG_BLACK);
        var glyph1 = makeAttr(RED, BG_BLACK);
        var glyph2 = makeAttr(BLUE, BG_BLACK);
        var glyph3 = makeAttr(GREEN, BG_BLACK);
        return {
            name: 'hieroglyph',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '*', attr: glyph1 }, { char: '+', attr: glyph2 }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '@', attr: glyph1 }, { char: '+', attr: glyph2 }, { char: '~', attr: glyph3 }],
                    [{ char: '|', attr: glyph3 }, { char: '*', attr: glyph1 }, { char: 'o', attr: glyph2 }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '@', attr: glyph1 }, { char: '~', attr: glyph2 }, { char: '+', attr: glyph3 }, { char: '|', attr: glyph1 }],
                    [{ char: '|', attr: glyph2 }, { char: 'o', attr: glyph3 }, { char: '*', attr: glyph1 }, { char: '~', attr: glyph2 }],
                    [{ char: '^', attr: glyph3 }, { char: '*', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: 'v', attr: glyph3 }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: glyph1 }, { char: 'O', attr: glyph2 }, { char: '\\', attr: glyph1 }, { char: '~', attr: glyph3 }, { char: '|', attr: glyph2 }],
                    [{ char: '|', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '~', attr: glyph1 }],
                    [{ char: '^', attr: glyph3 }, { char: '~', attr: glyph1 }, { char: 'o', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: 'v', attr: glyph1 }],
                    [{ char: '|', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '|', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '|', attr: glyph2 }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }, { char: GLYPH.FULL_BLOCK, attr: stone }],
                    [{ char: '/', attr: glyph1 }, { char: 'O', attr: glyph2 }, { char: '\\', attr: glyph1 }, { char: '~', attr: glyph3 }, { char: '|', attr: glyph2 }],
                    [{ char: '|', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '~', attr: glyph1 }],
                    [{ char: '^', attr: glyph3 }, { char: '~', attr: glyph1 }, { char: 'o', attr: glyph2 }, { char: '*', attr: glyph3 }, { char: 'v', attr: glyph1 }],
                    [{ char: '|', attr: glyph1 }, { char: '@', attr: glyph2 }, { char: '|', attr: glyph3 }, { char: '+', attr: glyph1 }, { char: '|', attr: glyph2 }],
                    [{ char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }, { char: GLYPH.FULL_BLOCK, attr: stoneDark }]
                ]
            ]
        };
    },
    createScarab: function () {
        var shell = makeAttr(LIGHTBLUE, BG_BLACK);
        var shellDark = makeAttr(BLUE, BG_BLACK);
        var gold = makeAttr(YELLOW, BG_BLACK);
        var leg = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'scarab',
            variants: [
                [
                    [{ char: '(', attr: shell }, { char: ')', attr: shell }],
                    [{ char: '<', attr: leg }, { char: '>', attr: leg }]
                ],
                [
                    [{ char: '/', attr: shell }, { char: '*', attr: gold }, { char: '\\', attr: shell }],
                    [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: ')', attr: shellDark }],
                    [{ char: '<', attr: leg }, { char: '|', attr: leg }, { char: '>', attr: leg }]
                ],
                [
                    [U, { char: '/', attr: shell }, { char: '*', attr: gold }, { char: '\\', attr: shell }, U],
                    [{ char: '/', attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shell }],
                    [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: ')', attr: shellDark }],
                    [{ char: '<', attr: leg }, { char: '/', attr: leg }, { char: '|', attr: leg }, { char: '\\', attr: leg }, { char: '>', attr: leg }]
                ],
                [
                    [U, U, { char: '/', attr: shell }, { char: '@', attr: gold }, { char: '\\', attr: shell }, U, U],
                    [U, { char: '/', attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shell }, U],
                    [{ char: '/', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shellDark }],
                    [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: ')', attr: shellDark }],
                    [{ char: '<', attr: leg }, { char: '<', attr: leg }, { char: '/', attr: leg }, { char: '|', attr: leg }, { char: '\\', attr: leg }, { char: '>', attr: leg }, { char: '>', attr: leg }]
                ],
                [
                    [U, U, { char: '/', attr: shell }, { char: '@', attr: gold }, { char: '\\', attr: shell }, U, U],
                    [U, { char: '/', attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shell }, U],
                    [{ char: '/', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: '\\', attr: shellDark }],
                    [{ char: '(', attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: GLYPH.FULL_BLOCK, attr: shell }, { char: GLYPH.FULL_BLOCK, attr: shellDark }, { char: ')', attr: shellDark }],
                    [{ char: '<', attr: leg }, { char: '<', attr: leg }, { char: '/', attr: leg }, { char: '|', attr: leg }, { char: '\\', attr: leg }, { char: '>', attr: leg }, { char: '>', attr: leg }]
                ]
            ]
        };
    }
};
registerRoadsideSprite('column', RuinsSprites.createColumn);
registerRoadsideSprite('statue', RuinsSprites.createStatue);
registerRoadsideSprite('obelisk', RuinsSprites.createObelisk);
registerRoadsideSprite('sphinx', RuinsSprites.createSphinx);
registerRoadsideSprite('hieroglyph', RuinsSprites.createHieroglyph);
registerRoadsideSprite('scarab', RuinsSprites.createScarab);
"use strict";
var StadiumSprites = {
    createGrandstand: function () {
        var structure = makeAttr(DARKGRAY, BG_BLACK);
        var fans1 = makeAttr(LIGHTRED, BG_BLACK);
        var fans2 = makeAttr(YELLOW, BG_BLACK);
        var fans3 = makeAttr(LIGHTCYAN, BG_BLACK);
        var rail = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'grandstand',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.MEDIUM_SHADE, attr: fans1 }, { char: GLYPH.MEDIUM_SHADE, attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.DARK_SHADE, attr: fans3 }, { char: GLYPH.DARK_SHADE, attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: '-', attr: rail }, { char: '-', attr: rail }, { char: '-', attr: rail }, { char: '-', attr: rail }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }]
                ],
                [
                    [{ char: '/', attr: structure }, { char: GLYPH.MEDIUM_SHADE, attr: fans1 }, { char: GLYPH.MEDIUM_SHADE, attr: fans2 }, { char: GLYPH.MEDIUM_SHADE, attr: fans3 }, { char: GLYPH.MEDIUM_SHADE, attr: fans1 }, { char: GLYPH.MEDIUM_SHADE, attr: fans2 }, { char: '\\', attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: '[', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: ']', attr: rail }]
                ],
                [
                    [U, { char: '/', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '\\', attr: structure }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: '[', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: ']', attr: rail }]
                ],
                [
                    [U, { char: '/', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '_', attr: structure }, { char: '\\', attr: structure }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: 'o', attr: fans3 }, { char: 'o', attr: fans1 }, { char: 'o', attr: fans2 }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }, { char: GLYPH.FULL_BLOCK, attr: structure }],
                    [{ char: '[', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: '=', attr: rail }, { char: ']', attr: rail }]
                ]
            ]
        };
    },
    createTireStack: function () {
        var tire = makeAttr(DARKGRAY, BG_BLACK);
        var tireInner = makeAttr(BLACK, BG_BLACK);
        var paint = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'tire_stack',
            variants: [
                [
                    [{ char: 'O', attr: tire }, { char: 'O', attr: paint }],
                    [{ char: 'O', attr: paint }, { char: 'O', attr: tire }]
                ],
                [
                    [U, { char: 'O', attr: paint }, U],
                    [{ char: 'O', attr: tire }, { char: 'O', attr: paint }, { char: 'O', attr: tire }],
                    [{ char: 'O', attr: paint }, { char: 'O', attr: tire }, { char: 'O', attr: paint }]
                ],
                [
                    [U, { char: 'O', attr: paint }, { char: 'O', attr: tire }, U],
                    [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: '0', attr: tireInner }, { char: ')', attr: tire }],
                    [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: '0', attr: tireInner }, { char: ')', attr: paint }],
                    [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: '0', attr: tireInner }, { char: ')', attr: tire }]
                ],
                [
                    [U, U, { char: 'O', attr: paint }, U, U],
                    [U, { char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: tire }, U],
                    [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }],
                    [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: paint }, { char: '0', attr: tireInner }, { char: ')', attr: tire }],
                    [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }]
                ],
                [
                    [U, U, { char: 'O', attr: paint }, U, U],
                    [U, { char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: tire }, U],
                    [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }],
                    [{ char: '(', attr: tire }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: paint }, { char: '0', attr: tireInner }, { char: ')', attr: tire }],
                    [{ char: '(', attr: paint }, { char: '0', attr: tireInner }, { char: GLYPH.FULL_BLOCK, attr: tire }, { char: '0', attr: tireInner }, { char: ')', attr: paint }]
                ]
            ]
        };
    },
    createHayBale: function () {
        var hay = makeAttr(YELLOW, BG_BLACK);
        var hayDark = makeAttr(BROWN, BG_BLACK);
        var twine = makeAttr(BROWN, BG_BLACK);
        return {
            name: 'hay_bale',
            variants: [
                [
                    [{ char: '[', attr: hay }, { char: ']', attr: hay }]
                ],
                [
                    [{ char: '/', attr: hay }, { char: '~', attr: hayDark }, { char: '\\', attr: hay }],
                    [{ char: '[', attr: hay }, { char: '=', attr: twine }, { char: ']', attr: hay }]
                ],
                [
                    [{ char: '/', attr: hay }, { char: GLYPH.MEDIUM_SHADE, attr: hayDark }, { char: GLYPH.MEDIUM_SHADE, attr: hay }, { char: '\\', attr: hay }],
                    [{ char: '[', attr: hay }, { char: '=', attr: twine }, { char: '=', attr: twine }, { char: ']', attr: hay }]
                ],
                [
                    [{ char: '/', attr: hay }, { char: '~', attr: hayDark }, { char: '~', attr: hay }, { char: '~', attr: hayDark }, { char: '\\', attr: hay }],
                    [{ char: '[', attr: hay }, { char: GLYPH.MEDIUM_SHADE, attr: hayDark }, { char: '|', attr: twine }, { char: GLYPH.MEDIUM_SHADE, attr: hay }, { char: ']', attr: hay }],
                    [{ char: '\\', attr: hay }, { char: '_', attr: hayDark }, { char: '=', attr: twine }, { char: '_', attr: hay }, { char: '/', attr: hay }]
                ],
                [
                    [{ char: '/', attr: hay }, { char: '~', attr: hayDark }, { char: '~', attr: hay }, { char: '~', attr: hayDark }, { char: '\\', attr: hay }],
                    [{ char: '[', attr: hay }, { char: GLYPH.MEDIUM_SHADE, attr: hayDark }, { char: '|', attr: twine }, { char: GLYPH.MEDIUM_SHADE, attr: hay }, { char: ']', attr: hay }],
                    [{ char: '\\', attr: hay }, { char: '_', attr: hayDark }, { char: '=', attr: twine }, { char: '_', attr: hay }, { char: '/', attr: hay }]
                ]
            ]
        };
    },
    createFlagMarshal: function () {
        var uniform = makeAttr(WHITE, BG_BLACK);
        var pants = makeAttr(DARKGRAY, BG_BLACK);
        var flag = makeAttr(YELLOW, BG_BLACK);
        var flagAlt = makeAttr(LIGHTGREEN, BG_BLACK);
        var U = null;
        return {
            name: 'flag_marshal',
            variants: [
                [
                    [{ char: '\\', attr: flag }],
                    [{ char: 'o', attr: uniform }],
                    [{ char: '|', attr: pants }]
                ],
                [
                    [{ char: '~', attr: flag }, { char: '>', attr: flag }],
                    [U, { char: 'O', attr: uniform }],
                    [{ char: '/', attr: uniform }, { char: '\\', attr: uniform }],
                    [{ char: '/', attr: pants }, { char: '\\', attr: pants }]
                ],
                [
                    [{ char: '~', attr: flagAlt }, { char: '~', attr: flagAlt }, { char: '>', attr: flagAlt }],
                    [U, { char: '|', attr: uniform }, U],
                    [U, { char: 'O', attr: uniform }, U],
                    [{ char: '/', attr: uniform }, { char: '|', attr: uniform }, { char: '\\', attr: uniform }],
                    [{ char: '/', attr: pants }, U, { char: '\\', attr: pants }]
                ],
                [
                    [U, { char: '~', attr: flag }, { char: '~', attr: flag }, { char: '>', attr: flag }],
                    [U, U, { char: '|', attr: uniform }, U],
                    [U, { char: '(', attr: uniform }, { char: ')', attr: uniform }, U],
                    [U, { char: '/', attr: uniform }, { char: '\\', attr: uniform }, U],
                    [U, { char: '|', attr: pants }, { char: '|', attr: pants }, U],
                    [{ char: '/', attr: pants }, U, U, { char: '\\', attr: pants }]
                ],
                [
                    [U, { char: '~', attr: flag }, { char: '~', attr: flag }, { char: '>', attr: flag }],
                    [U, U, { char: '|', attr: uniform }, U],
                    [U, { char: '(', attr: uniform }, { char: ')', attr: uniform }, U],
                    [U, { char: '/', attr: uniform }, { char: '\\', attr: uniform }, U],
                    [U, { char: '|', attr: pants }, { char: '|', attr: pants }, U],
                    [{ char: '/', attr: pants }, U, U, { char: '\\', attr: pants }]
                ]
            ]
        };
    },
    createPitCrew: function () {
        var helmet = makeAttr(LIGHTRED, BG_BLACK);
        var suit = makeAttr(RED, BG_BLACK);
        var tool = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'pit_crew',
            variants: [
                [
                    [{ char: 'o', attr: helmet }],
                    [{ char: '#', attr: suit }],
                    [{ char: 'A', attr: suit }]
                ],
                [
                    [{ char: '(', attr: helmet }, { char: ')', attr: helmet }],
                    [{ char: '[', attr: suit }, { char: ']', attr: suit }],
                    [{ char: '/', attr: suit }, { char: '\\', attr: suit }],
                    [{ char: '|', attr: suit }, { char: '|', attr: suit }]
                ],
                [
                    [U, { char: '@', attr: helmet }, U],
                    [{ char: '/', attr: tool }, { char: '#', attr: suit }, { char: '\\', attr: suit }],
                    [{ char: '|', attr: tool }, { char: '#', attr: suit }, { char: '|', attr: suit }],
                    [U, { char: '/', attr: suit }, { char: '\\', attr: suit }],
                    [U, { char: '|', attr: suit }, { char: '|', attr: suit }]
                ],
                [
                    [U, { char: '(', attr: helmet }, { char: ')', attr: helmet }, U],
                    [{ char: '/', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, { char: '\\', attr: suit }],
                    [{ char: '|', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
                    [U, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
                    [U, { char: '/', attr: suit }, { char: '\\', attr: suit }, U],
                    [U, { char: '|', attr: suit }, { char: '|', attr: suit }, U]
                ],
                [
                    [U, { char: '(', attr: helmet }, { char: ')', attr: helmet }, U],
                    [{ char: '/', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, { char: '\\', attr: suit }],
                    [{ char: '|', attr: tool }, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
                    [U, { char: '[', attr: suit }, { char: ']', attr: suit }, U],
                    [U, { char: '/', attr: suit }, { char: '\\', attr: suit }, U],
                    [U, { char: '|', attr: suit }, { char: '|', attr: suit }, U]
                ]
            ]
        };
    },
    createBanner: function () {
        var frame = makeAttr(DARKGRAY, BG_BLACK);
        var banner1 = makeAttr(LIGHTRED, BG_RED);
        var banner2 = makeAttr(YELLOW, BG_BROWN);
        var banner3 = makeAttr(LIGHTCYAN, BG_CYAN);
        var pole = makeAttr(LIGHTGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'banner',
            variants: [
                [
                    [{ char: '[', attr: frame }, { char: '#', attr: banner1 }, { char: ']', attr: frame }],
                    [U, { char: '|', attr: pole }, U]
                ],
                [
                    [{ char: '[', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: ']', attr: frame }],
                    [U, { char: '|', attr: pole }, { char: '|', attr: pole }, U],
                    [U, { char: '|', attr: pole }, { char: '|', attr: pole }, U]
                ],
                [
                    [{ char: '/', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '\\', attr: frame }],
                    [{ char: '|', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner1 }, { char: GLYPH.FULL_BLOCK, attr: banner1 }, { char: GLYPH.FULL_BLOCK, attr: banner1 }, { char: '|', attr: frame }],
                    [U, U, { char: '|', attr: pole }, U, U],
                    [U, U, { char: '|', attr: pole }, U, U]
                ],
                [
                    [{ char: '/', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '\\', attr: frame }],
                    [{ char: '|', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: GLYPH.FULL_BLOCK, attr: banner3 }, { char: '|', attr: frame }],
                    [{ char: '\\', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '/', attr: frame }],
                    [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U],
                    [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U]
                ],
                [
                    [{ char: '/', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '-', attr: frame }, { char: '\\', attr: frame }],
                    [{ char: '|', attr: frame }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: GLYPH.FULL_BLOCK, attr: banner2 }, { char: '|', attr: frame }],
                    [{ char: '\\', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '_', attr: frame }, { char: '/', attr: frame }],
                    [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U],
                    [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U]
                ]
            ]
        };
    }
};
registerRoadsideSprite('grandstand', StadiumSprites.createGrandstand);
registerRoadsideSprite('tire_stack', StadiumSprites.createTireStack);
registerRoadsideSprite('hay_bale', StadiumSprites.createHayBale);
registerRoadsideSprite('flag_marshal', StadiumSprites.createFlagMarshal);
registerRoadsideSprite('pit_crew', StadiumSprites.createPitCrew);
registerRoadsideSprite('banner', StadiumSprites.createBanner);
"use strict";
var KaijuSprites = {
    createRubble: function () {
        var concrete = makeAttr(DARKGRAY, BG_BLACK);
        var rebar = makeAttr(BROWN, BG_BLACK);
        var dust = makeAttr(WHITE, BG_BLACK);
        var U = null;
        return {
            name: 'rubble',
            variants: [
                [
                    [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ],
                [
                    [U, { char: '.', attr: dust }, { char: '/', attr: rebar }, { char: '.', attr: dust }, U],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ],
                [
                    [U, U, { char: '.', attr: dust }, { char: '_', attr: rebar }, { char: '.', attr: dust }, U, U],
                    [U, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: rebar }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, U],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ],
                [
                    [U, U, U, { char: '.', attr: dust }, { char: '/', attr: rebar }, { char: '.', attr: dust }, U, U, U],
                    [U, U, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: '/', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: rebar }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, U, U],
                    [U, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }, U],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '[', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: ']', attr: rebar }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ]
            ]
        };
    },
    createFire: function () {
        var flame1 = makeAttr(YELLOW, BG_BLACK);
        var flame2 = makeAttr(LIGHTRED, BG_BLACK);
        var flame3 = makeAttr(RED, BG_BLACK);
        var U = null;
        return {
            name: 'fire',
            variants: [
                [
                    [{ char: '^', attr: flame1 }, { char: '^', attr: flame2 }],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }]
                ],
                [
                    [U, { char: '^', attr: flame1 }, U],
                    [{ char: '(', attr: flame2 }, { char: '*', attr: flame1 }, { char: ')', attr: flame2 }],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }]
                ],
                [
                    [U, { char: '^', attr: flame1 }, { char: '^', attr: flame1 }, { char: '^', attr: flame1 }, U],
                    [{ char: '(', attr: flame2 }, { char: '*', attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: '*', attr: flame1 }, { char: ')', attr: flame2 }],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.MEDIUM_SHADE, attr: flame2 }],
                    [U, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, U]
                ],
                [
                    [U, U, { char: '^', attr: flame1 }, { char: '^', attr: flame1 }, U, U],
                    [U, { char: '(', attr: flame1 }, { char: '*', attr: flame1 }, { char: '*', attr: flame1 }, { char: ')', attr: flame1 }, U],
                    [{ char: '(', attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: GLYPH.FULL_BLOCK, attr: flame1 }, { char: ')', attr: flame2 }],
                    [{ char: GLYPH.MEDIUM_SHADE, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.FULL_BLOCK, attr: flame2 }, { char: GLYPH.MEDIUM_SHADE, attr: flame2 }],
                    [U, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.FULL_BLOCK, attr: flame3 }, { char: GLYPH.MEDIUM_SHADE, attr: flame3 }, U]
                ]
            ]
        };
    },
    createWreckedCar: function () {
        var body = makeAttr(DARKGRAY, BG_BLACK);
        var rust = makeAttr(BROWN, BG_BLACK);
        var glass = makeAttr(LIGHTCYAN, BG_BLACK);
        var U = null;
        return {
            name: 'wrecked_car',
            variants: [
                [
                    [{ char: '_', attr: body }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: '_', attr: body }],
                    [{ char: 'o', attr: rust }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: 'o', attr: rust }]
                ],
                [
                    [U, { char: ',', attr: body }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: '.', attr: body }, U],
                    [{ char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }],
                    [{ char: 'O', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: 'O', attr: rust }]
                ],
                [
                    [U, U, { char: '_', attr: body }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: GLYPH.MEDIUM_SHADE, attr: glass }, { char: '_', attr: body }, U, U],
                    [U, { char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '|', attr: glass }, { char: '|', attr: glass }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }, U],
                    [{ char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }],
                    [{ char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }]
                ],
                [
                    [U, U, U, { char: '_', attr: body }, { char: '/', attr: glass }, { char: '\\', attr: glass }, { char: '_', attr: body }, U, U, U],
                    [U, U, { char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '|', attr: glass }, { char: '|', attr: glass }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }, U, U],
                    [U, { char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }, U],
                    [{ char: '[', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: ']', attr: body }],
                    [{ char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '-', attr: rust }, { char: '(', attr: rust }, { char: 'O', attr: rust }, { char: ')', attr: rust }]
                ]
            ]
        };
    },
    createFallenBuilding: function () {
        var concrete = makeAttr(DARKGRAY, BG_BLACK);
        var window = makeAttr(BLACK, BG_BLACK);
        var rebar = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'fallen_building',
            variants: [
                [
                    [U, { char: '/', attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: '\\', attr: concrete }, U],
                    [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ],
                [
                    [U, U, { char: '/', attr: concrete }, { char: '_', attr: rebar }, { char: '\\', attr: concrete }, U, U],
                    [U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U],
                    [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ],
                [
                    [U, U, U, { char: '/', attr: concrete }, { char: '=', attr: rebar }, { char: '\\', attr: concrete }, U, U, U],
                    [U, U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U, U],
                    [U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U],
                    [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ],
                [
                    [U, U, U, U, { char: '/', attr: concrete }, { char: '_', attr: rebar }, { char: '\\', attr: concrete }, U, U, U, U],
                    [U, U, U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U, U, U],
                    [U, U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U, U],
                    [U, { char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }, U],
                    [{ char: '/', attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.MEDIUM_SHADE, attr: window }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: '\\', attr: concrete }],
                    [{ char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }, { char: GLYPH.FULL_BLOCK, attr: concrete }]
                ]
            ]
        };
    },
    createTank: function () {
        var armor = makeAttr(GREEN, BG_BLACK);
        var turret = makeAttr(LIGHTGREEN, BG_BLACK);
        var track = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'tank',
            variants: [
                [
                    [{ char: '-', attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, U],
                    [{ char: 'O', attr: track }, { char: GLYPH.FULL_BLOCK, attr: track }, { char: GLYPH.FULL_BLOCK, attr: track }, { char: 'O', attr: track }]
                ],
                [
                    [U, { char: '-', attr: turret }, { char: '-', attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, U, U],
                    [{ char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }],
                    [{ char: 'O', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'O', attr: track }]
                ],
                [
                    [U, U, { char: '=', attr: turret }, { char: '=', attr: turret }, { char: '=', attr: turret }, U, U, U],
                    [U, { char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }, U],
                    [{ char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }],
                    [{ char: 'O', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'O', attr: track }]
                ],
                [
                    [U, U, U, { char: '=', attr: turret }, { char: '=', attr: turret }, { char: '=', attr: turret }, { char: '=', attr: turret }, U, U, U],
                    [U, U, { char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: turret }, { char: GLYPH.FULL_BLOCK, attr: turret }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }, U, U],
                    [U, { char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }, U],
                    [{ char: '[', attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: GLYPH.FULL_BLOCK, attr: armor }, { char: ']', attr: armor }],
                    [{ char: 'O', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'o', attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: GLYPH.MEDIUM_SHADE, attr: track }, { char: 'O', attr: track }]
                ]
            ]
        };
    },
    createFootprint: function () {
        var dirt = makeAttr(BROWN, BG_BLACK);
        var crater = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'monster_footprint',
            variants: [
                [
                    [{ char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }],
                    [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }]
                ],
                [
                    [U, { char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }, U],
                    [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }],
                    [U, { char: '\\', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '/', attr: crater }, U]
                ],
                [
                    [U, U, { char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }, U, U],
                    [U, { char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }, U],
                    [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }],
                    [U, { char: '\\', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '/', attr: crater }, U]
                ],
                [
                    [U, U, U, { char: '.', attr: dirt }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '.', attr: dirt }, U, U, U],
                    [U, U, { char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }, U, U],
                    [U, { char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }, U],
                    [{ char: '\\', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: ' ', attr: crater }, { char: GLYPH.DARK_SHADE, attr: crater }, { char: '/', attr: crater }],
                    [U, { char: '\\', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '_', attr: crater }, { char: '/', attr: crater }, U]
                ]
            ]
        };
    },
    createDangerSign: function () {
        var sign = makeAttr(YELLOW, BG_BLACK);
        var border = makeAttr(RED, BG_BLACK);
        var pole = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'danger_sign',
            variants: [
                [
                    [{ char: '/', attr: border }, { char: '!', attr: sign }, { char: '\\', attr: border }],
                    [U, { char: '|', attr: pole }, U]
                ],
                [
                    [U, { char: '/', attr: border }, { char: '\\', attr: border }, U],
                    [{ char: '/', attr: border }, { char: '!', attr: sign }, { char: '!', attr: sign }, { char: '\\', attr: border }],
                    [U, { char: '|', attr: pole }, { char: '|', attr: pole }, U]
                ],
                [
                    [U, U, { char: '/\\', attr: border }, U, U],
                    [U, { char: '/', attr: border }, { char: '!', attr: sign }, { char: '\\', attr: border }, U],
                    [{ char: '/', attr: border }, { char: '-', attr: border }, { char: '!', attr: sign }, { char: '-', attr: border }, { char: '\\', attr: border }],
                    [U, U, { char: '|', attr: pole }, U, U]
                ],
                [
                    [U, U, { char: '/', attr: border }, { char: '\\', attr: border }, U, U],
                    [U, { char: '/', attr: border }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: '\\', attr: border }, U],
                    [{ char: '/', attr: border }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: '!', attr: border }, { char: '!', attr: border }, { char: GLYPH.FULL_BLOCK, attr: sign }, { char: '\\', attr: border }],
                    [{ char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }, { char: '-', attr: border }],
                    [U, U, { char: '|', attr: pole }, { char: '|', attr: pole }, U, U]
                ]
            ]
        };
    },
    createFleeingPerson: function () {
        var skin = makeAttr(YELLOW, BG_BLACK);
        var clothes = makeAttr(CYAN, BG_BLACK);
        var hair = makeAttr(BROWN, BG_BLACK);
        var U = null;
        return {
            name: 'fleeing_person',
            variants: [
                [
                    [{ char: '\\', attr: skin }, { char: 'o', attr: skin }, { char: '/', attr: skin }],
                    [U, { char: 'A', attr: clothes }, U]
                ],
                [
                    [{ char: '\\', attr: skin }, { char: 'O', attr: skin }, { char: '/', attr: skin }],
                    [U, { char: '|', attr: clothes }, U],
                    [{ char: '/', attr: clothes }, U, { char: '\\', attr: clothes }]
                ],
                [
                    [{ char: '\\', attr: skin }, U, { char: 'O', attr: skin }, U, { char: '/', attr: skin }],
                    [U, { char: '\\', attr: skin }, { char: '|', attr: clothes }, { char: '/', attr: skin }, U],
                    [U, U, { char: 'Y', attr: clothes }, U, U],
                    [U, { char: '/', attr: clothes }, U, { char: '\\', attr: clothes }, U]
                ],
                [
                    [{ char: '\\', attr: skin }, U, { char: 'n', attr: hair }, U, { char: '/', attr: skin }],
                    [U, { char: '|', attr: skin }, { char: '@', attr: skin }, { char: '|', attr: skin }, U],
                    [U, U, { char: 'Y', attr: clothes }, U, U],
                    [U, U, { char: '|', attr: clothes }, U, U],
                    [U, { char: '/', attr: clothes }, U, { char: '\\', attr: clothes }, U]
                ]
            ]
        };
    },
    createBurningBarrel: function () {
        var barrel = makeAttr(BROWN, BG_BLACK);
        var flame = makeAttr(YELLOW, BG_BLACK);
        var hotFlame = makeAttr(LIGHTRED, BG_BLACK);
        var U = null;
        return {
            name: 'burning_barrel',
            variants: [
                [
                    [{ char: '^', attr: flame }, { char: '^', attr: hotFlame }],
                    [{ char: '[', attr: barrel }, { char: ']', attr: barrel }]
                ],
                [
                    [{ char: '^', attr: flame }, { char: '*', attr: hotFlame }, { char: '^', attr: flame }],
                    [{ char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: ')', attr: flame }],
                    [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }]
                ],
                [
                    [U, { char: '^', attr: flame }, { char: '^', attr: hotFlame }, U],
                    [{ char: '(', attr: flame }, { char: '*', attr: hotFlame }, { char: '*', attr: hotFlame }, { char: ')', attr: flame }],
                    [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }],
                    [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }]
                ],
                [
                    [U, { char: '^', attr: flame }, { char: '*', attr: hotFlame }, { char: '^', attr: flame }, U],
                    [{ char: '(', attr: flame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: GLYPH.FULL_BLOCK, attr: hotFlame }, { char: ')', attr: flame }],
                    [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }],
                    [{ char: '[', attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: ']', attr: barrel }],
                    [U, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, { char: GLYPH.FULL_BLOCK, attr: barrel }, U]
                ]
            ]
        };
    },
    createBrokenStreetLamp: function () {
        var pole = makeAttr(DARKGRAY, BG_BLACK);
        var spark = makeAttr(YELLOW, BG_BLACK);
        var U = null;
        return {
            name: 'broken_streetlamp',
            variants: [
                [
                    [{ char: '*', attr: spark }, U],
                    [{ char: '/', attr: pole }, U],
                    [{ char: '|', attr: pole }, U]
                ],
                [
                    [{ char: '*', attr: spark }, { char: '_', attr: pole }, U],
                    [U, { char: '/', attr: pole }, U],
                    [U, { char: '|', attr: pole }, U],
                    [U, { char: '|', attr: pole }, U]
                ],
                [
                    [{ char: '*', attr: spark }, { char: '*', attr: spark }, { char: '_', attr: pole }, U],
                    [U, U, { char: '/', attr: pole }, U],
                    [U, U, { char: '|', attr: pole }, U],
                    [U, U, { char: '|', attr: pole }, U],
                    [U, { char: '=', attr: pole }, { char: '|', attr: pole }, { char: '=', attr: pole }]
                ],
                [
                    [{ char: '*', attr: spark }, { char: '*', attr: spark }, { char: '_', attr: pole }, { char: '_', attr: pole }, U],
                    [U, U, U, { char: '/', attr: pole }, U],
                    [U, U, U, { char: '|', attr: pole }, U],
                    [U, U, U, { char: '|', attr: pole }, U],
                    [U, U, U, { char: '|', attr: pole }, U],
                    [U, { char: '=', attr: pole }, { char: '=', attr: pole }, { char: '|', attr: pole }, { char: '=', attr: pole }]
                ]
            ]
        };
    }
};
registerRoadsideSprite('rubble', KaijuSprites.createRubble);
registerRoadsideSprite('fire', KaijuSprites.createFire);
registerRoadsideSprite('wrecked_car', KaijuSprites.createWreckedCar);
registerRoadsideSprite('fallen_building', KaijuSprites.createFallenBuilding);
registerRoadsideSprite('tank', KaijuSprites.createTank);
registerRoadsideSprite('monster_footprint', KaijuSprites.createFootprint);
registerRoadsideSprite('danger_sign', KaijuSprites.createDangerSign);
registerRoadsideSprite('fleeing_person', KaijuSprites.createFleeingPerson);
registerRoadsideSprite('burning_barrel', KaijuSprites.createBurningBarrel);
registerRoadsideSprite('broken_streetlamp', KaijuSprites.createBrokenStreetLamp);
"use strict";
var UnderwaterSprites = {
    createFish: function () {
        var body1 = makeAttr(YELLOW, BG_BLUE);
        var body2 = makeAttr(LIGHTCYAN, BG_BLUE);
        var eye = makeAttr(WHITE, BG_BLUE);
        var fin = makeAttr(LIGHTRED, BG_BLUE);
        var stripe = makeAttr(WHITE, BG_BLUE);
        var U = null;
        return {
            name: 'underwater_fish',
            variants: [
                [
                    [{ char: '<', attr: body1 }, { char: '>', attr: body1 }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: fin }, U],
                    [{ char: '<', attr: body2 }, { char: GLYPH.FULL_BLOCK, attr: body2 }, { char: '>', attr: body2 }]
                ],
                [
                    [U, { char: '/', attr: fin }, { char: GLYPH.UPPER_HALF, attr: fin }, { char: '\\', attr: fin }, U],
                    [{ char: '<', attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: '|', attr: stripe }, { char: 'O', attr: eye }, { char: '>', attr: body1 }],
                    [U, { char: '\\', attr: fin }, { char: GLYPH.LOWER_HALF, attr: fin }, { char: '/', attr: fin }, U]
                ],
                [
                    [U, U, { char: '/', attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '\\', attr: fin }, U],
                    [{ char: '<', attr: body2 }, { char: '<', attr: body2 }, { char: GLYPH.FULL_BLOCK, attr: body2 }, { char: '|', attr: stripe }, { char: 'O', attr: eye }, { char: '>', attr: body2 }],
                    [U, U, { char: '\\', attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '/', attr: fin }, U]
                ],
                [
                    [U, U, { char: '/', attr: fin }, { char: '_', attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '\\', attr: fin }, U],
                    [{ char: '<', attr: body1 }, { char: '<', attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: '|', attr: stripe }, { char: 'O', attr: eye }, { char: '>', attr: body1 }],
                    [U, U, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: GLYPH.FULL_BLOCK, attr: body1 }, { char: '|', attr: stripe }, { char: GLYPH.FULL_BLOCK, attr: body1 }, U],
                    [U, U, { char: '\\', attr: fin }, { char: GLYPH.LOWER_HALF, attr: fin }, { char: GLYPH.FULL_BLOCK, attr: fin }, { char: '/', attr: fin }, U]
                ]
            ]
        };
    },
    createCoral: function () {
        var coral1 = makeAttr(LIGHTRED, BG_BLUE);
        var coral2 = makeAttr(LIGHTMAGENTA, BG_BLUE);
        var coral3 = makeAttr(YELLOW, BG_BLUE);
        var base = makeAttr(DARKGRAY, BG_BLUE);
        var U = null;
        return {
            name: 'underwater_coral',
            variants: [
                [
                    [{ char: '*', attr: coral1 }]
                ],
                [
                    [{ char: 'Y', attr: coral1 }, { char: 'Y', attr: coral2 }],
                    [{ char: '|', attr: base }, { char: '|', attr: base }]
                ],
                [
                    [{ char: '*', attr: coral1 }, { char: '*', attr: coral2 }, { char: '*', attr: coral1 }],
                    [{ char: 'Y', attr: coral1 }, { char: '|', attr: coral2 }, { char: 'Y', attr: coral1 }],
                    [U, { char: '|', attr: base }, U]
                ],
                [
                    [U, { char: '*', attr: coral1 }, { char: '*', attr: coral3 }, U],
                    [{ char: 'Y', attr: coral1 }, { char: '|', attr: coral1 }, { char: '|', attr: coral3 }, { char: 'Y', attr: coral3 }],
                    [{ char: '|', attr: coral1 }, { char: '/', attr: coral1 }, { char: '\\', attr: coral3 }, { char: '|', attr: coral3 }],
                    [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
                ],
                [
                    [U, { char: '*', attr: coral1 }, { char: '*', attr: coral2 }, { char: '*', attr: coral3 }, U],
                    [{ char: '*', attr: coral1 }, { char: 'Y', attr: coral1 }, { char: '|', attr: coral2 }, { char: 'Y', attr: coral3 }, { char: '*', attr: coral3 }],
                    [{ char: '|', attr: coral1 }, { char: '/', attr: coral1 }, { char: '|', attr: coral2 }, { char: '\\', attr: coral3 }, { char: '|', attr: coral3 }],
                    [U, { char: '|', attr: base }, { char: '|', attr: base }, { char: '|', attr: base }, U],
                    [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
                ]
            ]
        };
    },
    createSeaweed: function () {
        var leaf1 = makeAttr(LIGHTGREEN, BG_BLUE);
        var leaf2 = makeAttr(GREEN, BG_BLUE);
        var U = null;
        return {
            name: 'underwater_seaweed',
            variants: [
                [
                    [{ char: '|', attr: leaf2 }]
                ],
                [
                    [{ char: ')', attr: leaf1 }],
                    [{ char: '|', attr: leaf2 }]
                ],
                [
                    [{ char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }],
                    [{ char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }],
                    [{ char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }]
                ],
                [
                    [U, { char: '~', attr: leaf1 }, U],
                    [{ char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }, { char: '(', attr: leaf1 }],
                    [{ char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }, { char: ')', attr: leaf2 }],
                    [{ char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }]
                ],
                [
                    [U, { char: '~', attr: leaf1 }, { char: '~', attr: leaf1 }, U],
                    [{ char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }, { char: '(', attr: leaf1 }, { char: ')', attr: leaf1 }],
                    [{ char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }, { char: ')', attr: leaf2 }, { char: '(', attr: leaf2 }],
                    [{ char: '(', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: ')', attr: leaf2 }],
                    [{ char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }, { char: '|', attr: leaf2 }]
                ]
            ]
        };
    },
    createAnemone: function () {
        var tentacle1 = makeAttr(LIGHTMAGENTA, BG_BLUE);
        var tentacle2 = makeAttr(MAGENTA, BG_BLUE);
        var center = makeAttr(YELLOW, BG_BLUE);
        var fish = makeAttr(LIGHTRED, BG_BLUE);
        var base = makeAttr(BROWN, BG_BLUE);
        var U = null;
        return {
            name: 'underwater_anemone',
            variants: [
                [
                    [{ char: '*', attr: tentacle1 }]
                ],
                [
                    [{ char: ')', attr: tentacle1 }, { char: '(', attr: tentacle1 }],
                    [{ char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }]
                ],
                [
                    [{ char: ')', attr: tentacle1 }, { char: '|', attr: tentacle2 }, { char: '(', attr: tentacle1 }],
                    [{ char: '(', attr: tentacle2 }, { char: 'O', attr: center }, { char: ')', attr: tentacle2 }],
                    [U, { char: GLYPH.FULL_BLOCK, attr: base }, U]
                ],
                [
                    [{ char: '~', attr: tentacle1 }, { char: ')', attr: tentacle1 }, { char: '(', attr: tentacle1 }, { char: '~', attr: tentacle1 }],
                    [{ char: ')', attr: tentacle2 }, { char: '|', attr: tentacle2 }, { char: '|', attr: tentacle2 }, { char: '(', attr: tentacle2 }],
                    [{ char: '(', attr: tentacle2 }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: ')', attr: tentacle2 }],
                    [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
                ],
                [
                    [{ char: '~', attr: tentacle1 }, { char: ')', attr: tentacle1 }, { char: '<', attr: fish }, { char: '(', attr: tentacle1 }, { char: '~', attr: tentacle1 }],
                    [{ char: ')', attr: tentacle1 }, { char: '|', attr: tentacle2 }, { char: '>', attr: fish }, { char: '|', attr: tentacle2 }, { char: '(', attr: tentacle1 }],
                    [{ char: '(', attr: tentacle2 }, { char: ')', attr: tentacle2 }, { char: 'O', attr: center }, { char: '(', attr: tentacle2 }, { char: ')', attr: tentacle2 }],
                    [{ char: '|', attr: tentacle2 }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: GLYPH.FULL_BLOCK, attr: center }, { char: '|', attr: tentacle2 }],
                    [U, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, { char: GLYPH.FULL_BLOCK, attr: base }, U]
                ]
            ]
        };
    },
    createJellyfish: function () {
        var body = makeAttr(LIGHTMAGENTA, BG_BLUE);
        var glow = makeAttr(LIGHTCYAN, BG_BLUE);
        var tent = makeAttr(MAGENTA, BG_BLUE);
        var U = null;
        return {
            name: 'underwater_jellyfish',
            variants: [
                [
                    [{ char: 'n', attr: body }],
                    [{ char: '|', attr: tent }]
                ],
                [
                    [{ char: '/', attr: glow }, { char: '\\', attr: glow }],
                    [{ char: '(', attr: body }, { char: ')', attr: body }],
                    [{ char: '|', attr: tent }, { char: '|', attr: tent }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: glow }, U],
                    [{ char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }],
                    [{ char: '(', attr: tent }, { char: '~', attr: tent }, { char: ')', attr: tent }],
                    [{ char: '|', attr: tent }, U, { char: '|', attr: tent }]
                ],
                [
                    [U, { char: '_', attr: glow }, { char: '_', attr: glow }, U],
                    [{ char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }],
                    [{ char: '|', attr: body }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: '|', attr: body }],
                    [{ char: '(', attr: tent }, { char: '~', attr: tent }, { char: '~', attr: tent }, { char: ')', attr: tent }],
                    [{ char: '|', attr: tent }, { char: '|', attr: tent }, { char: '|', attr: tent }, { char: '|', attr: tent }]
                ],
                [
                    [U, { char: '_', attr: glow }, { char: '_', attr: glow }, { char: '_', attr: glow }, U],
                    [{ char: '/', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '\\', attr: body }],
                    [{ char: '|', attr: body }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: GLYPH.LIGHT_SHADE, attr: glow }, { char: '|', attr: body }],
                    [{ char: '\\', attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: '/', attr: body }],
                    [{ char: '(', attr: tent }, { char: '~', attr: tent }, { char: '|', attr: tent }, { char: '~', attr: tent }, { char: ')', attr: tent }],
                    [{ char: '|', attr: tent }, U, { char: '|', attr: tent }, U, { char: '|', attr: tent }]
                ]
            ]
        };
    },
    createTreasure: function () {
        var chest = makeAttr(BROWN, BG_BLUE);
        var gold = makeAttr(YELLOW, BG_BLUE);
        var shine = makeAttr(WHITE, BG_BLUE);
        var U = null;
        return {
            name: 'underwater_treasure',
            variants: [
                [
                    [{ char: '#', attr: chest }]
                ],
                [
                    [{ char: '[', attr: chest }, { char: ']', attr: chest }]
                ],
                [
                    [{ char: '$', attr: gold }, { char: '*', attr: shine }, { char: '$', attr: gold }],
                    [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }]
                ],
                [
                    [U, { char: '$', attr: gold }, { char: '$', attr: gold }, U],
                    [{ char: '/', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: chest }],
                    [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }]
                ],
                [
                    [U, { char: '$', attr: gold }, { char: '*', attr: shine }, { char: '$', attr: gold }, U],
                    [{ char: '/', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: GLYPH.FULL_BLOCK, attr: gold }, { char: '\\', attr: chest }],
                    [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: 'O', attr: gold }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }],
                    [{ char: '[', attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: GLYPH.FULL_BLOCK, attr: chest }, { char: ']', attr: chest }]
                ]
            ]
        };
    }
};
registerRoadsideSprite('underwater_fish', function () { return UnderwaterSprites.createFish(); });
registerRoadsideSprite('underwater_coral', function () { return UnderwaterSprites.createCoral(); });
registerRoadsideSprite('underwater_seaweed', function () { return UnderwaterSprites.createSeaweed(); });
registerRoadsideSprite('underwater_anemone', function () { return UnderwaterSprites.createAnemone(); });
registerRoadsideSprite('underwater_jellyfish', function () { return UnderwaterSprites.createJellyfish(); });
registerRoadsideSprite('underwater_treasure', function () { return UnderwaterSprites.createTreasure(); });
"use strict";
var NPC_VEHICLE_TYPES = ['sedan', 'truck', 'sportscar'];
var NPC_VEHICLE_COLORS = [
    { body: RED, trim: LIGHTRED, name: 'red' },
    { body: BLUE, trim: LIGHTBLUE, name: 'blue' },
    { body: GREEN, trim: LIGHTGREEN, name: 'green' },
    { body: CYAN, trim: LIGHTCYAN, name: 'cyan' },
    { body: MAGENTA, trim: LIGHTMAGENTA, name: 'magenta' },
    { body: WHITE, trim: LIGHTGRAY, name: 'white' },
    { body: BROWN, trim: YELLOW, name: 'orange' }
];
function createSedanSprite(bodyColor, trimColor) {
    var body = makeAttr(bodyColor, BG_BLACK);
    var trim = makeAttr(trimColor, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var window = makeAttr(CYAN, BG_BLACK);
    var taillight = makeAttr(LIGHTRED, BG_BLACK);
    var U = null;
    return {
        name: 'sedan',
        variants: [
            [
                [{ char: GLYPH.LOWER_HALF, attr: taillight }]
            ],
            [
                [{ char: GLYPH.LOWER_HALF, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: taillight }]
            ],
            [
                [{ char: GLYPH.UPPER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: body }],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ],
            [
                [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ],
            [
                [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }],
                [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
            ]
        ]
    };
}
function createTruckSprite(bodyColor, trimColor) {
    var body = makeAttr(bodyColor, BG_BLACK);
    var trim = makeAttr(trimColor, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var window = makeAttr(CYAN, BG_BLACK);
    var taillight = makeAttr(LIGHTRED, BG_BLACK);
    var U = null;
    return {
        name: 'truck',
        variants: [
            [
                [{ char: GLYPH.LOWER_HALF, attr: taillight }]
            ],
            [
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ],
            [
                [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: body }],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ],
            [
                [U, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, U],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }],
                [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
            ],
            [
                [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
                [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.FULL_BLOCK, attr: body }],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }],
                [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
            ]
        ]
    };
}
function createSportsCarSprite(bodyColor, trimColor) {
    var body = makeAttr(bodyColor, BG_BLACK);
    var trim = makeAttr(trimColor, BG_BLACK);
    var window = makeAttr(CYAN, BG_BLACK);
    var taillight = makeAttr(LIGHTRED, BG_BLACK);
    var U = null;
    return {
        name: 'sportscar',
        variants: [
            [
                [{ char: GLYPH.LOWER_HALF, attr: taillight }]
            ],
            [
                [{ char: GLYPH.LOWER_HALF, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: taillight }]
            ],
            [
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ],
            [
                [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ],
            [
                [U, { char: GLYPH.UPPER_HALF, attr: window }, { char: GLYPH.FULL_BLOCK, attr: window }, { char: GLYPH.UPPER_HALF, attr: window }, U],
                [{ char: GLYPH.FULL_BLOCK, attr: taillight }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: taillight }]
            ]
        ]
    };
}
var NPC_SPRITE_CACHE = {};
function getNPCSprite(vehicleType, colorIndex) {
    var color = NPC_VEHICLE_COLORS[colorIndex % NPC_VEHICLE_COLORS.length];
    var key = vehicleType + '_' + color.name;
    if (!NPC_SPRITE_CACHE[key]) {
        switch (vehicleType) {
            case 'sedan':
                NPC_SPRITE_CACHE[key] = createSedanSprite(color.body, color.trim);
                break;
            case 'truck':
                NPC_SPRITE_CACHE[key] = createTruckSprite(color.body, color.trim);
                break;
            case 'sportscar':
                NPC_SPRITE_CACHE[key] = createSportsCarSprite(color.body, color.trim);
                break;
            default:
                NPC_SPRITE_CACHE[key] = createSedanSprite(color.body, color.trim);
        }
    }
    return NPC_SPRITE_CACHE[key];
}
function getRandomNPCSprite() {
    var typeIndex = Math.floor(Math.random() * NPC_VEHICLE_TYPES.length);
    var colorIndex = Math.floor(Math.random() * NPC_VEHICLE_COLORS.length);
    var vehicleType = NPC_VEHICLE_TYPES[typeIndex];
    return {
        sprite: getNPCSprite(vehicleType, colorIndex),
        type: vehicleType,
        colorIndex: colorIndex
    };
}
"use strict";
var PLAYER_CAR_SPRITE_CACHE = {};
function createPlayerSportsCarSprite(config) {
    var body = makeAttr(config.bodyColor, BG_BLACK);
    var trim = makeAttr(config.trimColor, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
    var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
    var U = null;
    return {
        name: 'player_sports',
        brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
        variants: [
            [
                [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
                [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
                [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
            ]
        ]
    };
}
function createMuscleCarSprite(config) {
    var body = makeAttr(config.bodyColor, BG_BLACK);
    var trim = makeAttr(config.trimColor, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
    var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
    var U = null;
    return {
        name: 'player_muscle',
        brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
        variants: [
            [
                [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
                [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
                [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
            ]
        ]
    };
}
function createCompactCarSprite(config) {
    var body = makeAttr(config.bodyColor, BG_BLACK);
    var trim = makeAttr(config.trimColor, BG_BLACK);
    var wheel = makeAttr(DARKGRAY, BG_BLACK);
    var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
    var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
    var U = null;
    return {
        name: 'player_compact',
        brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
        variants: [
            [
                [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
                [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
                [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: wheel }]
            ]
        ]
    };
}
function createSuperCarSprite(config) {
    var body = makeAttr(config.bodyColor, BG_BLACK);
    var trim = makeAttr(config.trimColor, BG_BLACK);
    var wheelWell = makeAttr(DARKGRAY, BG_BLACK);
    var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
    var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
    var U = null;
    return {
        name: 'player_super',
        brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
        variants: [
            [
                [U, { char: GLYPH.UPPER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: body }, U],
                [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
                [{ char: GLYPH.LOWER_HALF, attr: wheelWell }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheelWell }]
            ]
        ]
    };
}
function createClassicCarSprite(config) {
    var body = makeAttr(config.bodyColor, BG_BLACK);
    var trim = makeAttr(config.trimColor, BG_BLACK);
    var chrome = makeAttr(LIGHTGRAY, BG_BLACK);
    var brakeLightAttr = config.brakeLightsOn ? makeAttr(config.bodyColor, BG_RED) : body;
    var brakeLightChar = config.brakeLightsOn ? GLYPH.UPPER_HALF : GLYPH.FULL_BLOCK;
    var U = null;
    return {
        name: 'player_classic',
        brakeLightCells: [{ row: 1, col: 0 }, { row: 1, col: 4 }],
        variants: [
            [
                [U, { char: GLYPH.UPPER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: body }, U],
                [{ char: brakeLightChar, attr: brakeLightAttr }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: chrome }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: brakeLightChar, attr: brakeLightAttr }],
                [{ char: GLYPH.LOWER_HALF, attr: chrome }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: chrome }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: chrome }]
            ]
        ]
    };
}
function getPlayerCarSprite(bodyStyle, colorId, brakeLightsOn) {
    var key = bodyStyle + '_' + colorId + '_' + (brakeLightsOn ? 'brake' : 'normal');
    if (!PLAYER_CAR_SPRITE_CACHE[key]) {
        var color = getCarColor(colorId);
        if (!color) {
            color = CAR_COLORS['yellow'];
        }
        var config = {
            bodyColor: color.body,
            trimColor: color.trim,
            brakeLightsOn: brakeLightsOn
        };
        switch (bodyStyle) {
            case 'muscle':
                PLAYER_CAR_SPRITE_CACHE[key] = createMuscleCarSprite(config);
                break;
            case 'compact':
                PLAYER_CAR_SPRITE_CACHE[key] = createCompactCarSprite(config);
                break;
            case 'super':
                PLAYER_CAR_SPRITE_CACHE[key] = createSuperCarSprite(config);
                break;
            case 'classic':
                PLAYER_CAR_SPRITE_CACHE[key] = createClassicCarSprite(config);
                break;
            case 'sports':
            default:
                PLAYER_CAR_SPRITE_CACHE[key] = createPlayerSportsCarSprite(config);
                break;
        }
    }
    return PLAYER_CAR_SPRITE_CACHE[key];
}
function clearPlayerCarSpriteCache() {
    PLAYER_CAR_SPRITE_CACHE = {};
}
function createCarPreviewSprite(bodyStyle, colorId) {
    return getPlayerCarSprite(bodyStyle, colorId, false);
}
"use strict";
var SynthwaveSprites = {
    createNeonPillar: function () {
        var neonCyan = makeAttr(LIGHTCYAN, BG_BLACK);
        var neonMagenta = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var glowCyan = makeAttr(CYAN, BG_BLACK);
        var glowMagenta = makeAttr(MAGENTA, BG_BLACK);
        var base = makeAttr(BLUE, BG_BLACK);
        var U = null;
        return {
            name: 'neon_pillar',
            variants: [
                [
                    [{ char: GLYPH.BOX_V, attr: neonCyan }]
                ],
                [
                    [{ char: GLYPH.BOX_V, attr: neonCyan }],
                    [{ char: GLYPH.BOX_V, attr: neonMagenta }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: neonCyan }, { char: GLYPH.UPPER_HALF, attr: glowCyan }],
                    [{ char: GLYPH.FULL_BLOCK, attr: neonMagenta }, { char: GLYPH.DARK_SHADE, attr: glowMagenta }],
                    [{ char: GLYPH.LOWER_HALF, attr: base }, { char: GLYPH.LOWER_HALF, attr: base }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: neonCyan }, U],
                    [{ char: GLYPH.DARK_SHADE, attr: glowCyan }, { char: GLYPH.FULL_BLOCK, attr: neonCyan }, { char: GLYPH.DARK_SHADE, attr: glowCyan }],
                    [{ char: GLYPH.DARK_SHADE, attr: glowMagenta }, { char: GLYPH.FULL_BLOCK, attr: neonMagenta }, { char: GLYPH.DARK_SHADE, attr: glowMagenta }],
                    [U, { char: GLYPH.LOWER_HALF, attr: base }, U]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: neonCyan }, U],
                    [{ char: GLYPH.DARK_SHADE, attr: glowCyan }, { char: GLYPH.FULL_BLOCK, attr: neonCyan }, { char: GLYPH.DARK_SHADE, attr: glowCyan }],
                    [{ char: GLYPH.DARK_SHADE, attr: glowCyan }, { char: GLYPH.FULL_BLOCK, attr: neonCyan }, { char: GLYPH.DARK_SHADE, attr: glowCyan }],
                    [{ char: GLYPH.DARK_SHADE, attr: glowMagenta }, { char: GLYPH.FULL_BLOCK, attr: neonMagenta }, { char: GLYPH.DARK_SHADE, attr: glowMagenta }],
                    [U, { char: GLYPH.LOWER_HALF, attr: base }, U]
                ]
            ]
        };
    },
    createGridPylon: function () {
        var frame = makeAttr(MAGENTA, BG_BLACK);
        var frameBright = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var accent = makeAttr(CYAN, BG_BLACK);
        var accentBright = makeAttr(LIGHTCYAN, BG_BLACK);
        var U = null;
        return {
            name: 'grid_pylon',
            variants: [
                [
                    [{ char: GLYPH.TRIANGLE_UP, attr: frame }]
                ],
                [
                    [{ char: GLYPH.TRIANGLE_UP, attr: frameBright }],
                    [{ char: GLYPH.BOX_V, attr: frame }]
                ],
                [
                    [U, { char: GLYPH.TRIANGLE_UP, attr: accentBright }, U],
                    [{ char: '/', attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: '\\', attr: frame }],
                    [{ char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }]
                ],
                [
                    [U, { char: GLYPH.TRIANGLE_UP, attr: accentBright }, U],
                    [{ char: '/', attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: '\\', attr: frame }],
                    [{ char: GLYPH.BOX_V, attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: GLYPH.BOX_V, attr: frame }],
                    [{ char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }]
                ],
                [
                    [U, U, { char: GLYPH.TRIANGLE_UP, attr: accentBright }, U, U],
                    [U, { char: '/', attr: frameBright }, { char: GLYPH.BOX_V, attr: frameBright }, { char: '\\', attr: frameBright }, U],
                    [{ char: '/', attr: frame }, { char: ' ', attr: frame }, { char: GLYPH.BOX_V, attr: frameBright }, { char: ' ', attr: frame }, { char: '\\', attr: frame }],
                    [{ char: GLYPH.BOX_V, attr: frame }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_V, attr: frame }],
                    [{ char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_CROSS, attr: accentBright }, { char: GLYPH.BOX_H, attr: accent }, { char: GLYPH.BOX_H, attr: accent }]
                ]
            ]
        };
    },
    createHoloBillboard: function () {
        var border = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var borderDim = makeAttr(MAGENTA, BG_BLACK);
        var text = makeAttr(LIGHTCYAN, BG_BLACK);
        var support = makeAttr(BLUE, BG_BLACK);
        var U = null;
        return {
            name: 'holo_billboard',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: border }, { char: GLYPH.FULL_BLOCK, attr: borderDim }]
                ],
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: border }, { char: '-', attr: text }, { char: GLYPH.FULL_BLOCK, attr: borderDim }],
                    [U, { char: GLYPH.BOX_V, attr: support }, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.UPPER_HALF, attr: borderDim }],
                    [{ char: GLYPH.BOX_V, attr: border }, { char: 'N', attr: text }, { char: 'E', attr: text }, { char: 'O', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
                    [U, U, { char: GLYPH.BOX_V, attr: support }, U, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.UPPER_HALF, attr: borderDim }],
                    [{ char: GLYPH.BOX_V, attr: border }, { char: 'N', attr: text }, { char: 'E', attr: text }, { char: 'O', attr: text }, { char: 'N', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
                    [{ char: GLYPH.LOWER_HALF, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.LOWER_HALF, attr: borderDim }],
                    [U, U, { char: GLYPH.BOX_V, attr: support }, { char: GLYPH.BOX_V, attr: support }, U, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.BOX_H, attr: border }, { char: GLYPH.UPPER_HALF, attr: borderDim }],
                    [{ char: GLYPH.BOX_V, attr: border }, { char: ' ', attr: text }, { char: 'N', attr: text }, { char: 'E', attr: text }, { char: 'O', attr: text }, { char: ' ', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
                    [{ char: GLYPH.BOX_V, attr: border }, { char: ' ', attr: text }, { char: '8', attr: text }, { char: '0', attr: text }, { char: 's', attr: text }, { char: ' ', attr: text }, { char: GLYPH.BOX_V, attr: borderDim }],
                    [{ char: GLYPH.LOWER_HALF, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.BOX_H, attr: borderDim }, { char: GLYPH.LOWER_HALF, attr: borderDim }],
                    [U, U, { char: GLYPH.BOX_V, attr: support }, U, { char: GLYPH.BOX_V, attr: support }, U, U]
                ]
            ]
        };
    },
    createNeonPalm: function () {
        var trunk = makeAttr(MAGENTA, BG_BLACK);
        var trunkBright = makeAttr(LIGHTMAGENTA, BG_BLACK);
        var frond = makeAttr(CYAN, BG_BLACK);
        var frondBright = makeAttr(LIGHTCYAN, BG_BLACK);
        var glow = makeAttr(BLUE, BG_BLACK);
        var U = null;
        return {
            name: 'neon_palm',
            variants: [
                [
                    [{ char: GLYPH.UPPER_HALF, attr: frondBright }]
                ],
                [
                    [{ char: '/', attr: frond }, { char: '\\', attr: frond }],
                    [U, { char: GLYPH.BOX_V, attr: trunk }]
                ],
                [
                    [{ char: '/', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frondBright }],
                    [U, { char: '\\', attr: frond }, { char: '/', attr: frond }, U],
                    [U, { char: GLYPH.BOX_V, attr: trunk }, { char: GLYPH.BOX_V, attr: trunkBright }, U]
                ],
                [
                    [{ char: '-', attr: glow }, { char: '/', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frondBright }, { char: '-', attr: glow }],
                    [U, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '*', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, U],
                    [U, { char: '/', attr: frond }, { char: GLYPH.BOX_V, attr: trunkBright }, { char: '\\', attr: frond }, U],
                    [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, U, U]
                ],
                [
                    [{ char: '-', attr: glow }, { char: '/', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frondBright }, { char: '-', attr: glow }],
                    [{ char: '/', attr: frond }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: ' ', attr: frond }, { char: '*', attr: frondBright }, { char: GLYPH.FULL_BLOCK, attr: frond }, { char: '\\', attr: frond }],
                    [U, { char: '\\', attr: frond }, { char: '/', attr: frond }, { char: '\\', attr: frond }, { char: '/', attr: frond }, U],
                    [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, { char: GLYPH.FULL_BLOCK, attr: trunkBright }, U, U],
                    [U, U, { char: GLYPH.LOWER_HALF, attr: trunk }, { char: GLYPH.LOWER_HALF, attr: trunk }, U, U]
                ]
            ]
        };
    },
    createLaserBeam: function () {
        var beamCore = makeAttr(WHITE, BG_BLACK);
        var beamMid = makeAttr(LIGHTCYAN, BG_BLACK);
        var beamOuter = makeAttr(CYAN, BG_BLACK);
        var glowDim = makeAttr(BLUE, BG_BLACK);
        return {
            name: 'laser_beam',
            variants: [
                [
                    [{ char: GLYPH.BOX_V, attr: beamMid }]
                ],
                [
                    [{ char: GLYPH.BOX_V, attr: beamCore }],
                    [{ char: GLYPH.BOX_V, attr: beamMid }]
                ],
                [
                    [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }],
                    [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
                    [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }]
                ],
                [
                    [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }],
                    [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
                    [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
                    [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }]
                ],
                [
                    [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }],
                    [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
                    [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamCore }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
                    [{ char: GLYPH.LIGHT_SHADE, attr: beamOuter }, { char: GLYPH.FULL_BLOCK, attr: beamMid }, { char: GLYPH.LIGHT_SHADE, attr: beamOuter }],
                    [{ char: GLYPH.DARK_SHADE, attr: glowDim }, { char: GLYPH.BOX_V, attr: beamCore }, { char: GLYPH.DARK_SHADE, attr: glowDim }]
                ]
            ]
        };
    }
};
registerRoadsideSprite('neon_pillar', SynthwaveSprites.createNeonPillar);
registerRoadsideSprite('grid_pylon', SynthwaveSprites.createGridPylon);
registerRoadsideSprite('holo_billboard', SynthwaveSprites.createHoloBillboard);
registerRoadsideSprite('neon_palm', SynthwaveSprites.createNeonPalm);
registerRoadsideSprite('laser_beam', SynthwaveSprites.createLaserBeam);
"use strict";
var SynthwaveTheme = {
    name: 'synthwave',
    description: 'Classic 80s synthwave with magenta sky, purple mountains, and setting sun',
    colors: {
        skyTop: { fg: MAGENTA, bg: BG_BLACK },
        skyMid: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        skyHorizon: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        skyGrid: { fg: MAGENTA, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        celestialCore: { fg: YELLOW, bg: BG_RED },
        celestialGlow: { fg: LIGHTRED, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: LIGHTGRAY, bg: BG_BLACK },
        sceneryPrimary: { fg: MAGENTA, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
        roadSurface: { fg: LIGHTCYAN, bg: BG_CYAN },
        roadSurfaceAlt: { fg: CYAN, bg: BG_CYAN },
        roadStripe: { fg: WHITE, bg: BG_CYAN },
        roadEdge: { fg: LIGHTMAGENTA, bg: BG_BLUE },
        roadGrid: { fg: WHITE, bg: BG_CYAN },
        shoulderPrimary: { fg: BLUE, bg: BG_BLACK },
        shoulderSecondary: { fg: MAGENTA, bg: BG_BLACK },
        itemBox: {
            border: { fg: LIGHTMAGENTA, bg: BG_BLACK },
            fill: { fg: MAGENTA, bg: BG_BLACK },
            symbol: { fg: WHITE, bg: BG_BLACK }
        },
        roadsideColors: {
            'neon_pillar': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
                tertiary: { fg: BLUE, bg: BG_BLACK }
            },
            'grid_pylon': {
                primary: { fg: MAGENTA, bg: BG_BLACK },
                secondary: { fg: CYAN, bg: BG_BLACK }
            },
            'holo_billboard': {
                primary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
                secondary: { fg: LIGHTCYAN, bg: BG_BLACK },
                tertiary: { fg: BLUE, bg: BG_BLACK }
            },
            'neon_palm': {
                primary: { fg: MAGENTA, bg: BG_BLACK },
                secondary: { fg: CYAN, bg: BG_BLACK },
                tertiary: { fg: LIGHTCYAN, bg: BG_BLACK }
            },
            'laser_beam': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: LIGHTCYAN, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'grid',
        converging: true,
        horizontal: true
    },
    background: {
        type: 'mountains',
        config: {
            count: 5,
            minHeight: 3,
            maxHeight: 6,
            parallaxSpeed: 0.1
        }
    },
    celestial: {
        type: 'sun',
        size: 3,
        positionX: 0.5,
        positionY: 0.6
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'grid',
        primary: { fg: MAGENTA, bg: BG_BLACK },
        secondary: { fg: CYAN, bg: BG_BLACK },
        pattern: {
            gridSpacing: 15,
            radialLines: 10
        }
    },
    roadside: {
        pool: [
            { sprite: 'neon_pillar', weight: 3, side: 'both' },
            { sprite: 'grid_pylon', weight: 3, side: 'both' },
            { sprite: 'holo_billboard', weight: 2, side: 'both' },
            { sprite: 'neon_palm', weight: 2, side: 'both' },
            { sprite: 'laser_beam', weight: 1, side: 'both' }
        ],
        spacing: 10,
        density: 1.0
    }
};
registerTheme(SynthwaveTheme);
"use strict";
var CityNightTheme = {
    name: 'city_night',
    description: 'Midnight city drive with skyscrapers, stars, and neon lights',
    colors: {
        skyTop: { fg: BLUE, bg: BG_BLACK },
        skyMid: { fg: BLUE, bg: BG_BLACK },
        skyHorizon: { fg: LIGHTBLUE, bg: BG_BLACK },
        skyGrid: { fg: BLUE, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTBLUE, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_CYAN },
        celestialGlow: { fg: LIGHTCYAN, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: CYAN, bg: BG_BLACK },
        sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        scenerySecondary: { fg: YELLOW, bg: BG_BLACK },
        sceneryTertiary: { fg: LIGHTRED, bg: BG_BLACK },
        roadSurface: { fg: DARKGRAY, bg: BG_LIGHTGRAY },
        roadSurfaceAlt: { fg: BLACK, bg: BG_LIGHTGRAY },
        roadStripe: { fg: YELLOW, bg: BG_LIGHTGRAY },
        roadEdge: { fg: LIGHTBLUE, bg: BG_BLUE },
        roadGrid: { fg: DARKGRAY, bg: BG_LIGHTGRAY },
        shoulderPrimary: { fg: LIGHTGRAY, bg: BG_BLUE },
        shoulderSecondary: { fg: DARKGRAY, bg: BG_BLACK },
        roadsideColors: {
            'building': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK },
                tertiary: { fg: LIGHTCYAN, bg: BG_BLACK }
            },
            'lamppost': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'sign': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'tree': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: GREEN, bg: BG_BLACK },
                tertiary: { fg: DARKGRAY, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'stars',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'skyscrapers',
        config: {
            density: 0.8,
            hasWindows: true,
            hasAntennas: true,
            parallaxSpeed: 0.15
        }
    },
    celestial: {
        type: 'moon',
        size: 2,
        positionX: 0.85,
        positionY: 0.3
    },
    stars: {
        enabled: true,
        density: 0.4,
        twinkle: true
    },
    roadside: {
        pool: [
            { sprite: 'building', weight: 4, side: 'both' },
            { sprite: 'lamppost', weight: 3, side: 'both' },
            { sprite: 'sign', weight: 2, side: 'right' },
            { sprite: 'tree', weight: 1, side: 'both' }
        ],
        spacing: 12,
        density: 1.2
    }
};
registerTheme(CityNightTheme);
"use strict";
var SunsetBeachTheme = {
    name: 'sunset_beach',
    description: 'Tropical sunset with palm trees, warm orange skies, and beach vibes',
    colors: {
        skyTop: { fg: MAGENTA, bg: BG_BLACK },
        skyMid: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        skyHorizon: { fg: LIGHTRED, bg: BG_BLACK },
        skyGrid: { fg: LIGHTRED, bg: BG_BLACK },
        skyGridGlow: { fg: YELLOW, bg: BG_BLACK },
        celestialCore: { fg: YELLOW, bg: BG_BROWN },
        celestialGlow: { fg: BROWN, bg: BG_RED },
        starBright: { fg: YELLOW, bg: BG_BLACK },
        starDim: { fg: BROWN, bg: BG_BLACK },
        sceneryPrimary: { fg: MAGENTA, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        sceneryTertiary: { fg: CYAN, bg: BG_BLACK },
        roadSurface: { fg: BROWN, bg: BG_BLACK },
        roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
        roadStripe: { fg: WHITE, bg: BG_BLACK },
        roadEdge: { fg: YELLOW, bg: BG_BLACK },
        roadGrid: { fg: BROWN, bg: BG_BLACK },
        shoulderPrimary: { fg: YELLOW, bg: BG_BLACK },
        shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
        roadsideColors: {
            'palm': {
                primary: { fg: LIGHTGREEN, bg: BG_BLACK },
                secondary: { fg: GREEN, bg: BG_BLACK },
                tertiary: { fg: BROWN, bg: BG_BLACK }
            },
            'umbrella': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'lifeguard': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK },
                tertiary: { fg: BROWN, bg: BG_BLACK }
            },
            'surfboard': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: CYAN, bg: BG_BLACK }
            },
            'tiki': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: LIGHTRED, bg: BG_BLACK },
                tertiary: { fg: BROWN, bg: BG_BLACK }
            },
            'beachhut': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'ocean',
        config: {
            count: 3,
            minHeight: 2,
            maxHeight: 4,
            parallaxSpeed: 0.2
        }
    },
    celestial: {
        type: 'sun',
        size: 4,
        positionX: 0.5,
        positionY: 0.75
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'sand',
        primary: { fg: YELLOW, bg: BG_BROWN },
        secondary: { fg: WHITE, bg: BG_BROWN }
    },
    roadside: {
        pool: [
            { sprite: 'palm', weight: 4, side: 'both' },
            { sprite: 'umbrella', weight: 3, side: 'both' },
            { sprite: 'tiki', weight: 2, side: 'both' },
            { sprite: 'surfboard', weight: 2, side: 'both' },
            { sprite: 'lifeguard', weight: 1, side: 'right' },
            { sprite: 'beachhut', weight: 1, side: 'left' }
        ],
        spacing: 55,
        density: 1.3
    }
};
registerTheme(SunsetBeachTheme);
"use strict";
var TwilightForestTheme = {
    name: 'twilight_forest',
    description: 'Enchanted forest at twilight with dual moons and dancing fireflies',
    colors: {
        skyTop: { fg: BLUE, bg: BG_BLACK },
        skyMid: { fg: CYAN, bg: BG_BLACK },
        skyHorizon: { fg: BROWN, bg: BG_BLACK },
        skyGrid: { fg: BLUE, bg: BG_BLACK },
        skyGridGlow: { fg: CYAN, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_LIGHTGRAY },
        celestialGlow: { fg: LIGHTCYAN, bg: BG_BLACK },
        starBright: { fg: YELLOW, bg: BG_BLACK },
        starDim: { fg: LIGHTGREEN, bg: BG_BLACK },
        sceneryPrimary: { fg: GREEN, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTGREEN, bg: BG_BLACK },
        sceneryTertiary: { fg: BROWN, bg: BG_BLACK },
        roadSurface: { fg: BROWN, bg: BG_BLACK },
        roadSurfaceAlt: { fg: DARKGRAY, bg: BG_BLACK },
        roadStripe: { fg: YELLOW, bg: BG_BLACK },
        roadEdge: { fg: BROWN, bg: BG_BLACK },
        roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderPrimary: { fg: GREEN, bg: BG_BLACK },
        shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
        roadsideColors: {
            'tree': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: LIGHTGREEN, bg: BG_BLACK },
                tertiary: { fg: BROWN, bg: BG_BLACK }
            },
            'rock': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'bush': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: LIGHTGREEN, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'stars',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'forest',
        config: {
            count: 8,
            minHeight: 3,
            maxHeight: 7,
            parallaxSpeed: 0.15
        }
    },
    celestial: {
        type: 'dual_moons',
        size: 3,
        positionX: 0.7,
        positionY: 0.3
    },
    stars: {
        enabled: true,
        density: 0.6,
        twinkle: true
    },
    ground: {
        type: 'grass',
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK },
        pattern: {
            grassDensity: 0.4,
            grassChars: ['"', 'v', ',', "'", '.']
        }
    },
    roadside: {
        pool: [
            { sprite: 'tree', weight: 5, side: 'both' },
            { sprite: 'bush', weight: 3, side: 'both' },
            { sprite: 'rock', weight: 1, side: 'both' }
        ],
        spacing: 45,
        density: 1.4
    }
};
registerTheme(TwilightForestTheme);
"use strict";
var HauntedHollowTheme = {
    name: 'haunted_hollow',
    description: 'Race through a haunted cemetery under a blood moon',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: DARKGRAY, bg: BG_BLACK },
        skyHorizon: { fg: MAGENTA, bg: BG_BLACK },
        skyGrid: { fg: DARKGRAY, bg: BG_BLACK },
        skyGridGlow: { fg: MAGENTA, bg: BG_BLACK },
        celestialCore: { fg: RED, bg: BG_BLACK },
        celestialGlow: { fg: LIGHTRED, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: DARKGRAY, bg: BG_BLACK },
        sceneryPrimary: { fg: BLACK, bg: BG_BLACK },
        scenerySecondary: { fg: DARKGRAY, bg: BG_BLACK },
        sceneryTertiary: { fg: MAGENTA, bg: BG_BLACK },
        roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
        roadStripe: { fg: LIGHTRED, bg: BG_BLACK },
        roadEdge: { fg: DARKGRAY, bg: BG_BLACK },
        roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderSecondary: { fg: GREEN, bg: BG_BLACK },
        itemBox: {
            border: { fg: LIGHTGREEN, bg: BG_BLACK },
            fill: { fg: GREEN, bg: BG_BLACK },
            symbol: { fg: LIGHTMAGENTA, bg: BG_BLACK }
        },
        roadsideColors: {
            'deadtree': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'gravestone': {
                primary: { fg: LIGHTGRAY, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'pumpkin': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'skull': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            },
            'fence': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'candle': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'stars',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'hills',
        config: {
            count: 6,
            minHeight: 2,
            maxHeight: 5,
            parallaxSpeed: 0.1
        }
    },
    celestial: {
        type: 'moon',
        size: 4,
        positionX: 0.5,
        positionY: 0.25
    },
    stars: {
        enabled: true,
        density: 0.3,
        twinkle: true
    },
    ground: {
        type: 'dither',
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: GREEN, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.4,
            ditherChars: ['.', ',', "'", GLYPH.LIGHT_SHADE]
        }
    },
    roadside: {
        pool: [
            { sprite: 'deadtree', weight: 4, side: 'both' },
            { sprite: 'gravestone', weight: 5, side: 'both' },
            { sprite: 'pumpkin', weight: 2, side: 'both' },
            { sprite: 'skull', weight: 1, side: 'both' },
            { sprite: 'fence', weight: 3, side: 'both' },
            { sprite: 'candle', weight: 2, side: 'both' }
        ],
        spacing: 40,
        density: 1.3
    }
};
registerTheme(HauntedHollowTheme);
"use strict";
var WinterWonderlandTheme = {
    name: 'winter_wonderland',
    description: 'Race through a magical snowy forest with sparkling ice',
    colors: {
        skyTop: { fg: BLUE, bg: BG_BLACK },
        skyMid: { fg: LIGHTCYAN, bg: BG_BLACK },
        skyHorizon: { fg: WHITE, bg: BG_BLACK },
        skyGrid: { fg: LIGHTCYAN, bg: BG_BLACK },
        skyGridGlow: { fg: WHITE, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_LIGHTGRAY },
        celestialGlow: { fg: YELLOW, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: LIGHTCYAN, bg: BG_BLACK },
        sceneryPrimary: { fg: WHITE, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTCYAN, bg: BG_BLACK },
        sceneryTertiary: { fg: LIGHTGRAY, bg: BG_BLACK },
        roadSurface: { fg: LIGHTGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: WHITE, bg: BG_BLACK },
        roadStripe: { fg: LIGHTRED, bg: BG_BLACK },
        roadEdge: { fg: WHITE, bg: BG_BLACK },
        roadGrid: { fg: LIGHTCYAN, bg: BG_BLACK },
        shoulderPrimary: { fg: WHITE, bg: BG_BLACK },
        shoulderSecondary: { fg: LIGHTCYAN, bg: BG_BLACK },
        roadsideColors: {
            'snowpine': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'snowman': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: LIGHTRED, bg: BG_BLACK }
            },
            'icecrystal': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'candycane': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'snowdrift': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'signpost': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'stars',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'mountains',
        config: {
            count: 5,
            minHeight: 4,
            maxHeight: 8,
            parallaxSpeed: 0.12
        }
    },
    celestial: {
        type: 'sun',
        size: 2,
        positionX: 0.3,
        positionY: 0.35
    },
    stars: {
        enabled: true,
        density: 0.5,
        twinkle: true
    },
    ground: {
        type: 'dither',
        primary: { fg: WHITE, bg: BG_BLACK },
        secondary: { fg: LIGHTCYAN, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.3,
            ditherChars: ['.', '*', GLYPH.LIGHT_SHADE, "'"]
        }
    },
    roadside: {
        pool: [
            { sprite: 'snowpine', weight: 5, side: 'both' },
            { sprite: 'snowman', weight: 2, side: 'both' },
            { sprite: 'icecrystal', weight: 2, side: 'both' },
            { sprite: 'candycane', weight: 1, side: 'both' },
            { sprite: 'snowdrift', weight: 3, side: 'both' },
            { sprite: 'signpost', weight: 1, side: 'both' }
        ],
        spacing: 45,
        density: 1.2
    }
};
registerTheme(WinterWonderlandTheme);
"use strict";
var CactusCanyonTheme = {
    name: 'cactus_canyon',
    description: 'Race through the scorching desert canyons of the Southwest',
    colors: {
        skyTop: { fg: BLUE, bg: BG_BLACK },
        skyMid: { fg: CYAN, bg: BG_BLACK },
        skyHorizon: { fg: YELLOW, bg: BG_BLACK },
        skyGrid: { fg: YELLOW, bg: BG_BLACK },
        skyGridGlow: { fg: BROWN, bg: BG_BLACK },
        celestialCore: { fg: YELLOW, bg: BG_BROWN },
        celestialGlow: { fg: YELLOW, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: YELLOW, bg: BG_BLACK },
        sceneryPrimary: { fg: BROWN, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTRED, bg: BG_BLACK },
        sceneryTertiary: { fg: YELLOW, bg: BG_BLACK },
        roadSurface: { fg: BROWN, bg: BG_BLACK },
        roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
        roadStripe: { fg: YELLOW, bg: BG_BLACK },
        roadEdge: { fg: BROWN, bg: BG_BLACK },
        roadGrid: { fg: BROWN, bg: BG_BLACK },
        shoulderPrimary: { fg: YELLOW, bg: BG_BLACK },
        shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
        roadsideColors: {
            'saguaro': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: CYAN, bg: BG_BLACK }
            },
            'barrel': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: LIGHTRED, bg: BG_BLACK }
            },
            'tumbleweed': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'cowskull': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'desertrock': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'westernsign': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'dunes',
        config: {
            count: 6,
            minHeight: 3,
            maxHeight: 6,
            parallaxSpeed: 0.1
        }
    },
    celestial: {
        type: 'sun',
        size: 3,
        positionX: 0.6,
        positionY: 0.3
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'sand',
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.35,
            ditherChars: ['.', ',', "'", GLYPH.LIGHT_SHADE, '~']
        }
    },
    roadside: {
        pool: [
            { sprite: 'saguaro', weight: 4, side: 'both' },
            { sprite: 'barrel', weight: 3, side: 'both' },
            { sprite: 'tumbleweed', weight: 2, side: 'both' },
            { sprite: 'cowskull', weight: 1, side: 'both' },
            { sprite: 'desertrock', weight: 3, side: 'both' },
            { sprite: 'westernsign', weight: 1, side: 'both' }
        ],
        spacing: 50,
        density: 0.9
    }
};
registerTheme(CactusCanyonTheme);
"use strict";
var TropicalJungleTheme = {
    name: 'tropical_jungle',
    description: 'Race through a dense tropical rainforest teeming with life',
    colors: {
        skyTop: { fg: LIGHTCYAN, bg: BG_BLACK },
        skyMid: { fg: GREEN, bg: BG_BLACK },
        skyHorizon: { fg: LIGHTGREEN, bg: BG_BLACK },
        skyGrid: { fg: GREEN, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
        celestialCore: { fg: YELLOW, bg: BG_GREEN },
        celestialGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: LIGHTGREEN, bg: BG_BLACK },
        sceneryPrimary: { fg: GREEN, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTGREEN, bg: BG_BLACK },
        sceneryTertiary: { fg: BROWN, bg: BG_BLACK },
        roadSurface: { fg: BROWN, bg: BG_BLACK },
        roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
        roadStripe: { fg: YELLOW, bg: BG_BLACK },
        roadEdge: { fg: GREEN, bg: BG_BLACK },
        roadGrid: { fg: BROWN, bg: BG_BLACK },
        shoulderPrimary: { fg: GREEN, bg: BG_BLACK },
        shoulderSecondary: { fg: LIGHTGREEN, bg: BG_BLACK },
        roadsideColors: {
            'jungle_tree': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            },
            'fern': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: LIGHTGREEN, bg: BG_BLACK }
            },
            'vine': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: LIGHTMAGENTA, bg: BG_BLACK }
            },
            'flower': {
                primary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'parrot': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: LIGHTBLUE, bg: BG_BLACK }
            },
            'banana': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'jungle_canopy',
        config: {
            vineCount: 8,
            hangingVines: true,
            parallaxSpeed: 0.08
        }
    },
    celestial: {
        type: 'sun',
        size: 2,
        positionX: 0.4,
        positionY: 0.2
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'jungle',
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK },
        pattern: {
            grassDensity: 0.7,
            grassChars: ['"', 'v', 'V', 'Y', ',']
        }
    },
    roadside: {
        pool: [
            { sprite: 'jungle_tree', weight: 5, side: 'both' },
            { sprite: 'fern', weight: 4, side: 'both' },
            { sprite: 'vine', weight: 3, side: 'both' },
            { sprite: 'flower', weight: 3, side: 'both' },
            { sprite: 'parrot', weight: 1, side: 'both' },
            { sprite: 'banana', weight: 2, side: 'both' }
        ],
        spacing: 35,
        density: 1.4
    }
};
registerTheme(TropicalJungleTheme);
"use strict";
var CandyLandTheme = {
    name: 'candy_land',
    description: 'Race through a magical world made entirely of sweets and candy',
    colors: {
        skyTop: { fg: LIGHTCYAN, bg: BG_MAGENTA },
        skyMid: { fg: WHITE, bg: BG_MAGENTA },
        skyHorizon: { fg: LIGHTMAGENTA, bg: BG_CYAN },
        skyGrid: { fg: WHITE, bg: BG_MAGENTA },
        skyGridGlow: { fg: LIGHTCYAN, bg: BG_MAGENTA },
        celestialCore: { fg: YELLOW, bg: BG_MAGENTA },
        celestialGlow: { fg: WHITE, bg: BG_MAGENTA },
        starBright: { fg: WHITE, bg: BG_MAGENTA },
        starDim: { fg: LIGHTCYAN, bg: BG_MAGENTA },
        sceneryPrimary: { fg: WHITE, bg: BG_MAGENTA },
        scenerySecondary: { fg: LIGHTCYAN, bg: BG_MAGENTA },
        sceneryTertiary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
        roadSurface: { fg: WHITE, bg: BG_MAGENTA },
        roadSurfaceAlt: { fg: LIGHTMAGENTA, bg: BG_MAGENTA },
        roadStripe: { fg: LIGHTCYAN, bg: BG_MAGENTA },
        roadEdge: { fg: WHITE, bg: BG_CYAN },
        roadGrid: { fg: LIGHTMAGENTA, bg: BG_MAGENTA },
        shoulderPrimary: { fg: WHITE, bg: BG_MAGENTA },
        shoulderSecondary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
        itemBox: {
            border: { fg: LIGHTRED, bg: BG_CYAN },
            fill: { fg: LIGHTMAGENTA, bg: BG_CYAN },
            symbol: { fg: WHITE, bg: BG_CYAN }
        },
        roadsideColors: {
            'lollipop': {
                primary: { fg: LIGHTRED, bg: BG_CYAN },
                secondary: { fg: WHITE, bg: BG_CYAN }
            },
            'candy_cane': {
                primary: { fg: LIGHTRED, bg: BG_CYAN },
                secondary: { fg: WHITE, bg: BG_CYAN }
            },
            'gummy_bear': {
                primary: { fg: LIGHTGREEN, bg: BG_CYAN },
                secondary: { fg: GREEN, bg: BG_CYAN }
            },
            'cupcake': {
                primary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
                secondary: { fg: LIGHTRED, bg: BG_CYAN }
            },
            'ice_cream': {
                primary: { fg: WHITE, bg: BG_CYAN },
                secondary: { fg: BROWN, bg: BG_CYAN }
            },
            'cotton_candy': {
                primary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
                secondary: { fg: WHITE, bg: BG_CYAN }
            }
        }
    },
    sky: {
        type: 'stars',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'candy_hills',
        config: {
            swirls: true,
            parallaxSpeed: 0.12
        }
    },
    celestial: {
        type: 'sun',
        size: 3,
        positionX: 0.5,
        positionY: 0.3
    },
    stars: {
        enabled: true,
        density: 0.4,
        twinkle: true
    },
    ground: {
        type: 'candy',
        primary: { fg: WHITE, bg: BG_CYAN },
        secondary: { fg: LIGHTMAGENTA, bg: BG_CYAN },
        pattern: {
            ditherDensity: 0.4,
            ditherChars: ['*', '@', '.', '~']
        }
    },
    roadside: {
        pool: [
            { sprite: 'lollipop', weight: 4, side: 'both' },
            { sprite: 'candy_cane', weight: 4, side: 'both' },
            { sprite: 'gummy_bear', weight: 2, side: 'both' },
            { sprite: 'cupcake', weight: 3, side: 'both' },
            { sprite: 'ice_cream', weight: 2, side: 'both' },
            { sprite: 'cotton_candy', weight: 3, side: 'both' }
        ],
        spacing: 40,
        density: 1.2
    }
};
registerTheme(CandyLandTheme);
"use strict";
var RainbowRoadTheme = {
    name: 'rainbow_road',
    description: 'Race across a cosmic highway through the stars and galaxies',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: BLUE, bg: BG_BLACK },
        skyHorizon: { fg: MAGENTA, bg: BG_BLACK },
        skyGrid: { fg: MAGENTA, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_BLACK },
        celestialGlow: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: CYAN, bg: BG_BLACK },
        sceneryPrimary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTCYAN, bg: BG_BLACK },
        sceneryTertiary: { fg: LIGHTBLUE, bg: BG_BLACK },
        roadSurface: { fg: LIGHTMAGENTA, bg: BG_BLACK },
        roadSurfaceAlt: { fg: LIGHTCYAN, bg: BG_BLACK },
        roadStripe: { fg: WHITE, bg: BG_BLACK },
        roadEdge: { fg: YELLOW, bg: BG_BLACK },
        roadGrid: { fg: LIGHTBLUE, bg: BG_BLACK },
        shoulderPrimary: { fg: BLUE, bg: BG_BLACK },
        shoulderSecondary: { fg: MAGENTA, bg: BG_BLACK },
        roadsideColors: {
            'star': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'moon': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'planet': {
                primary: { fg: LIGHTBLUE, bg: BG_BLACK },
                secondary: { fg: LIGHTCYAN, bg: BG_BLACK }
            },
            'comet': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'nebula': {
                primary: { fg: LIGHTMAGENTA, bg: BG_BLACK },
                secondary: { fg: MAGENTA, bg: BG_BLACK }
            },
            'satellite': {
                primary: { fg: LIGHTGRAY, bg: BG_BLACK },
                secondary: { fg: LIGHTBLUE, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'stars',
        converging: true,
        horizontal: false
    },
    background: {
        type: 'nebula',
        config: {
            cloudCount: 5,
            parallaxSpeed: 0.05
        }
    },
    celestial: {
        type: 'sun',
        size: 4,
        positionX: 0.5,
        positionY: 0.4
    },
    stars: {
        enabled: true,
        density: 1.0,
        twinkle: true
    },
    ground: {
        type: 'void',
        primary: { fg: BLUE, bg: BG_BLACK },
        secondary: { fg: WHITE, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.1,
            ditherChars: ['.', '*']
        }
    },
    roadside: {
        pool: [
            { sprite: 'star', weight: 5, side: 'both' },
            { sprite: 'moon', weight: 2, side: 'both' },
            { sprite: 'planet', weight: 3, side: 'both' },
            { sprite: 'comet', weight: 2, side: 'both' },
            { sprite: 'nebula', weight: 3, side: 'both' },
            { sprite: 'satellite', weight: 2, side: 'both' }
        ],
        spacing: 45,
        density: 1.0
    },
    road: {
        rainbow: true,
        hideEdgeMarkers: true
    }
};
registerTheme(RainbowRoadTheme);
"use strict";
var DarkCastleTheme = {
    name: 'dark_castle',
    description: 'Race through the moonlit grounds of an ancient fortress',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: DARKGRAY, bg: BG_BLACK },
        skyHorizon: { fg: BLUE, bg: BG_BLACK },
        skyGrid: { fg: DARKGRAY, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTGRAY, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_BLACK },
        celestialGlow: { fg: LIGHTGRAY, bg: BG_BLACK },
        starBright: { fg: LIGHTGRAY, bg: BG_BLACK },
        starDim: { fg: DARKGRAY, bg: BG_BLACK },
        sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        scenerySecondary: { fg: BLACK, bg: BG_BLACK },
        sceneryTertiary: { fg: BLUE, bg: BG_BLACK },
        roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
        roadStripe: { fg: LIGHTGRAY, bg: BG_BLACK },
        roadEdge: { fg: BROWN, bg: BG_BLACK },
        roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
        roadsideColors: {
            'tower': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'battlement': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: BLACK, bg: BG_BLACK }
            },
            'torch': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'banner': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'gargoyle': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'portcullis': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'castle_fortress',
        config: {
            towerCount: 5,
            hasTorches: true,
            parallaxSpeed: 0.08
        }
    },
    celestial: {
        type: 'moon',
        size: 3,
        positionX: 0.45,
        positionY: 0.25
    },
    stars: {
        enabled: true,
        density: 0.25,
        twinkle: true
    },
    ground: {
        type: 'cobblestone',
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: BLACK, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.6,
            ditherChars: ['#', '+', GLYPH.DARK_SHADE]
        }
    },
    roadside: {
        pool: [
            { sprite: 'tower', weight: 3, side: 'both' },
            { sprite: 'battlement', weight: 4, side: 'both' },
            { sprite: 'torch', weight: 5, side: 'both' },
            { sprite: 'banner', weight: 3, side: 'both' },
            { sprite: 'gargoyle', weight: 2, side: 'both' },
            { sprite: 'portcullis', weight: 2, side: 'both' }
        ],
        spacing: 35,
        density: 1.0
    }
};
registerTheme(DarkCastleTheme);
"use strict";
var VillainsLairTheme = {
    name: 'villains_lair',
    description: 'Speed through the fiery domain of a supervillain',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: RED, bg: BG_BLACK },
        skyHorizon: { fg: YELLOW, bg: BG_BLACK },
        skyGrid: { fg: RED, bg: BG_BLACK },
        skyGridGlow: { fg: YELLOW, bg: BG_BLACK },
        celestialCore: { fg: YELLOW, bg: BG_RED },
        celestialGlow: { fg: RED, bg: BG_BLACK },
        starBright: { fg: YELLOW, bg: BG_BLACK },
        starDim: { fg: RED, bg: BG_BLACK },
        sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        scenerySecondary: { fg: RED, bg: BG_BLACK },
        sceneryTertiary: { fg: YELLOW, bg: BG_BLACK },
        roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
        roadStripe: { fg: RED, bg: BG_BLACK },
        roadEdge: { fg: YELLOW, bg: BG_BLACK },
        roadGrid: { fg: RED, bg: BG_BLACK },
        shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderSecondary: { fg: RED, bg: BG_BLACK },
        roadsideColors: {
            'lava_rock': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: RED, bg: BG_BLACK }
            },
            'flame': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: RED, bg: BG_BLACK }
            },
            'skull_pile': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'chain': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: LIGHTGRAY, bg: BG_BLACK }
            },
            'spike': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: RED, bg: BG_BLACK }
            },
            'cauldron': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: LIGHTGREEN, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'volcanic',
        config: {
            lavaGlow: true,
            smokeLevel: 2,
            parallaxSpeed: 0.1
        }
    },
    celestial: {
        type: 'sun',
        size: 5,
        positionX: 0.5,
        positionY: 0.6
    },
    stars: {
        enabled: true,
        density: 0.35,
        twinkle: true
    },
    ground: {
        type: 'lava',
        primary: { fg: DARKGRAY, bg: BG_BLACK },
        secondary: { fg: YELLOW, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.5,
            ditherChars: ['*', '~', GLYPH.MEDIUM_SHADE]
        }
    },
    roadside: {
        pool: [
            { sprite: 'lava_rock', weight: 4, side: 'both' },
            { sprite: 'flame', weight: 5, side: 'both' },
            { sprite: 'skull_pile', weight: 3, side: 'both' },
            { sprite: 'chain', weight: 2, side: 'both' },
            { sprite: 'spike', weight: 3, side: 'both' },
            { sprite: 'cauldron', weight: 2, side: 'both' }
        ],
        spacing: 35,
        density: 1.1
    }
};
registerTheme(VillainsLairTheme);
"use strict";
var AncientRuinsTheme = {
    name: 'ancient_ruins',
    description: 'Discover the secrets of ancient Egypt at high speed',
    colors: {
        skyTop: { fg: CYAN, bg: BG_BLACK },
        skyMid: { fg: YELLOW, bg: BG_BLACK },
        skyHorizon: { fg: WHITE, bg: BG_BLACK },
        skyGrid: { fg: YELLOW, bg: BG_BLACK },
        skyGridGlow: { fg: WHITE, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_BROWN },
        celestialGlow: { fg: YELLOW, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: YELLOW, bg: BG_BLACK },
        sceneryPrimary: { fg: BROWN, bg: BG_BLACK },
        scenerySecondary: { fg: YELLOW, bg: BG_BLACK },
        sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
        roadSurface: { fg: BROWN, bg: BG_BLACK },
        roadSurfaceAlt: { fg: YELLOW, bg: BG_BLACK },
        roadStripe: { fg: WHITE, bg: BG_BLACK },
        roadEdge: { fg: LIGHTGRAY, bg: BG_BLACK },
        roadGrid: { fg: YELLOW, bg: BG_BLACK },
        shoulderPrimary: { fg: YELLOW, bg: BG_BLACK },
        shoulderSecondary: { fg: BROWN, bg: BG_BLACK },
        roadsideColors: {
            'column': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'statue': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            },
            'obelisk': {
                primary: { fg: LIGHTGRAY, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'sphinx': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            },
            'hieroglyph': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: LIGHTCYAN, bg: BG_BLACK }
            },
            'scarab': {
                primary: { fg: LIGHTGREEN, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'pyramids',
        config: {
            pyramidCount: 3,
            hasSphinx: true,
            parallaxSpeed: 0.06
        }
    },
    celestial: {
        type: 'sun',
        size: 4,
        positionX: 0.6,
        positionY: 0.3
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'sand',
        primary: { fg: YELLOW, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.35,
            ditherChars: ['.', "'", '~', GLYPH.LIGHT_SHADE]
        }
    },
    roadside: {
        pool: [
            { sprite: 'column', weight: 4, side: 'both' },
            { sprite: 'statue', weight: 3, side: 'both' },
            { sprite: 'obelisk', weight: 3, side: 'both' },
            { sprite: 'sphinx', weight: 2, side: 'both' },
            { sprite: 'hieroglyph', weight: 3, side: 'both' },
            { sprite: 'scarab', weight: 2, side: 'both' }
        ],
        spacing: 45,
        density: 0.9
    }
};
registerTheme(AncientRuinsTheme);
"use strict";
var ThunderStadiumTheme = {
    name: 'thunder_stadium',
    description: 'Race on packed dirt under stadium lights with roaring crowds',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: DARKGRAY, bg: BG_BLACK },
        skyHorizon: { fg: DARKGRAY, bg: BG_BLACK },
        skyGrid: { fg: YELLOW, bg: BG_BLACK },
        skyGridGlow: { fg: WHITE, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_BLACK },
        celestialGlow: { fg: YELLOW, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: YELLOW, bg: BG_BLACK },
        sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        scenerySecondary: { fg: WHITE, bg: BG_BLACK },
        sceneryTertiary: { fg: YELLOW, bg: BG_BLACK },
        roadSurface: { fg: YELLOW, bg: BG_BROWN },
        roadSurfaceAlt: { fg: BROWN, bg: BG_BROWN },
        roadStripe: { fg: WHITE, bg: BG_BROWN },
        roadEdge: { fg: YELLOW, bg: BG_BLACK },
        roadGrid: { fg: DARKGRAY, bg: BG_BROWN },
        shoulderPrimary: { fg: YELLOW, bg: BG_BROWN },
        shoulderSecondary: { fg: BROWN, bg: BG_BROWN },
        roadsideColors: {
            'grandstand': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'tire_stack': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: WHITE, bg: BG_BLACK }
            },
            'hay_bale': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            },
            'flag_marshal': {
                primary: { fg: WHITE, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'pit_crew': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: RED, bg: BG_BLACK }
            },
            'banner': {
                primary: { fg: LIGHTRED, bg: BG_RED },
                secondary: { fg: YELLOW, bg: BG_BROWN }
            }
        }
    },
    sky: {
        type: 'plain',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'stadium',
        config: {
            parallaxSpeed: 0.05
        }
    },
    celestial: {
        type: 'none',
        size: 0,
        positionX: 0.5,
        positionY: 0.5
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'dirt',
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: BROWN, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.4,
            ditherChars: ['.', "'", '`', GLYPH.LIGHT_SHADE]
        }
    },
    roadside: {
        pool: [
            { sprite: 'grandstand', weight: 3, side: 'both' },
            { sprite: 'tire_stack', weight: 4, side: 'both' },
            { sprite: 'hay_bale', weight: 4, side: 'both' },
            { sprite: 'flag_marshal', weight: 2, side: 'both' },
            { sprite: 'pit_crew', weight: 2, side: 'both' },
            { sprite: 'banner', weight: 3, side: 'both' }
        ],
        spacing: 35,
        density: 1.2
    }
};
registerTheme(ThunderStadiumTheme);
"use strict";
var GlitchState = {
    intensity: 0.3,
    intensityTarget: 0.3,
    wavePhase: 0,
    lastSpikeTime: 0,
    colorCorruptPhase: 0,
    tearOffset: 0,
    noiseRows: [],
    skyGlitchType: 0,
    skyGlitchTimer: 0,
    matrixRainDrops: [],
    roadColorGlitch: 0,
    roadGlitchTimer: 0,
    roadsideColorShift: 0,
    update: function (trackPosition, dt) {
        this.wavePhase += dt * 2;
        var baseIntensity = 0.2 + Math.sin(this.wavePhase) * 0.1;
        if (Math.random() < 0.02) {
            this.intensityTarget = 0.6 + Math.random() * 0.4;
            this.lastSpikeTime = trackPosition;
        }
        if (this.intensityTarget > baseIntensity) {
            this.intensityTarget -= dt * 0.5;
        }
        this.intensity += (this.intensityTarget - this.intensity) * dt * 5;
        if (Math.random() < this.intensity * 0.1) {
            this.colorCorruptPhase = Math.floor(Math.random() * 6);
        }
        if (Math.random() < this.intensity * 0.15) {
            this.tearOffset = Math.floor(Math.random() * 8) - 4;
        }
        else if (Math.random() < 0.3) {
            this.tearOffset = 0;
        }
        if (Math.random() < this.intensity * 0.2) {
            this.noiseRows = [];
            var numNoiseRows = Math.floor(Math.random() * 3 * this.intensity);
            for (var i = 0; i < numNoiseRows; i++) {
                this.noiseRows.push(Math.floor(Math.random() * 25));
            }
        }
        else if (Math.random() < 0.2) {
            this.noiseRows = [];
        }
        this.skyGlitchTimer -= dt;
        if (this.skyGlitchTimer <= 0) {
            if (Math.random() < this.intensity * 0.08) {
                this.skyGlitchType = Math.floor(Math.random() * 5);
                this.skyGlitchTimer = 0.5 + Math.random() * 2.0;
                if (this.skyGlitchType === 1) {
                    this.matrixRainDrops = [];
                    for (var r = 0; r < 15; r++) {
                        this.matrixRainDrops.push({
                            x: Math.floor(Math.random() * 80),
                            y: Math.floor(Math.random() * 8),
                            speed: 0.5 + Math.random() * 1.5,
                            char: String.fromCharCode(48 + Math.floor(Math.random() * 10))
                        });
                    }
                }
            }
            else {
                this.skyGlitchType = 0;
            }
        }
        if (this.skyGlitchType === 1) {
            for (var d = 0; d < this.matrixRainDrops.length; d++) {
                var drop = this.matrixRainDrops[d];
                drop.y += drop.speed * dt * 10;
                if (drop.y > 8) {
                    drop.y = 0;
                    drop.x = Math.floor(Math.random() * 80);
                    drop.char = String.fromCharCode(48 + Math.floor(Math.random() * 10));
                }
                if (Math.random() < 0.1) {
                    drop.char = String.fromCharCode(48 + Math.floor(Math.random() * 10));
                }
            }
        }
        this.roadGlitchTimer -= dt;
        if (this.roadGlitchTimer <= 0) {
            if (Math.random() < this.intensity * 0.12) {
                this.roadColorGlitch = 1 + Math.floor(Math.random() * 4);
                this.roadGlitchTimer = 0.3 + Math.random() * 1.5;
            }
            else {
                this.roadColorGlitch = 0;
            }
        }
        if (Math.random() < this.intensity * 0.15) {
            this.roadsideColorShift = Math.floor(Math.random() * 8);
        }
        else if (Math.random() < 0.1) {
            this.roadsideColorShift = 0;
        }
    },
    corruptColor: function (originalFg, originalBg) {
        if (Math.random() > this.intensity * 0.5) {
            return { fg: originalFg, bg: originalBg };
        }
        switch (this.colorCorruptPhase) {
            case 0:
                return { fg: 15 - originalFg, bg: originalBg };
            case 1:
                return { fg: originalBg & 0x07, bg: (originalFg << 4) & 0x70 };
            case 2:
                return { fg: (originalFg + 8) % 16, bg: originalBg };
            case 3:
                return { fg: LIGHTGREEN, bg: BG_BLACK };
            case 4:
                return { fg: LIGHTCYAN, bg: BG_BLACK };
            case 5:
                return { fg: LIGHTRED, bg: BG_BLACK };
            default:
                return { fg: originalFg, bg: originalBg };
        }
    },
    corruptChar: function (original) {
        if (Math.random() > this.intensity * 0.3) {
            return original;
        }
        var glitchChars = [
            GLYPH.FULL_BLOCK, GLYPH.DARK_SHADE, GLYPH.MEDIUM_SHADE, GLYPH.LIGHT_SHADE,
            '#', '@', '%', '&', '$', '!', '?', '/', '\\', '|',
            GLYPH.BOX_H, GLYPH.BOX_V, GLYPH.BOX_TR, GLYPH.BOX_TL,
            '0', '1', 'X', 'x', '>', '<', '^', 'v'
        ];
        return glitchChars[Math.floor(Math.random() * glitchChars.length)];
    },
    isNoiseRow: function (row) {
        for (var i = 0; i < this.noiseRows.length; i++) {
            if (this.noiseRows[i] === row)
                return true;
        }
        return false;
    },
    getNoiseChar: function () {
        var noiseChars = [GLYPH.LIGHT_SHADE, GLYPH.MEDIUM_SHADE, GLYPH.DARK_SHADE, ' ', '#', '%'];
        return noiseChars[Math.floor(Math.random() * noiseChars.length)];
    },
    getTearOffset: function (row) {
        if (row > 8 && row < 18 && Math.abs(this.tearOffset) > 0) {
            return this.tearOffset;
        }
        return 0;
    },
    getGlitchedRoadColor: function (baseFg, baseBg, distance) {
        if (this.roadColorGlitch === 0) {
            return { fg: baseFg, bg: baseBg };
        }
        switch (this.roadColorGlitch) {
            case 1:
                return { fg: 15 - baseFg, bg: ((baseBg >> 4) ^ 0x07) << 4 };
            case 2:
                var rainbowColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
                var colorIndex = Math.floor(distance + (this.wavePhase * 3)) % rainbowColors.length;
                return { fg: rainbowColors[colorIndex], bg: BG_BLACK };
            case 3:
                var flash = Math.floor(Date.now() / 50) % 2;
                return flash === 0 ? { fg: WHITE, bg: BG_BLACK } : { fg: BLACK, bg: BG_LIGHTGRAY };
            case 4:
                if (Math.random() < 0.3) {
                    return { fg: Math.floor(Math.random() * 16), bg: [BG_BLACK, BG_GREEN, BG_CYAN, BG_RED][Math.floor(Math.random() * 4)] };
                }
                return { fg: baseFg, bg: baseBg };
            default:
                return { fg: baseFg, bg: baseBg };
        }
    },
    getGlitchedSpriteColor: function (baseFg, baseBg) {
        if (this.roadsideColorShift === 0 || Math.random() > this.intensity * 0.7) {
            return { fg: baseFg, bg: baseBg };
        }
        var shiftedFg = (baseFg + this.roadsideColorShift) % 16;
        var shiftedBg = baseBg;
        if (Math.random() < 0.2) {
            shiftedBg = [BG_BLACK, BG_GREEN, BG_BLUE, BG_CYAN][Math.floor(Math.random() * 4)];
        }
        return { fg: shiftedFg, bg: shiftedBg };
    }
};
var GlitchTheme = {
    name: 'glitch_circuit',
    description: 'Reality is corrupted. The simulation is breaking down.',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: DARKGRAY, bg: BG_BLACK },
        skyHorizon: { fg: LIGHTGREEN, bg: BG_BLACK },
        skyGrid: { fg: GREEN, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_GREEN },
        celestialGlow: { fg: LIGHTGREEN, bg: BG_BLACK },
        starBright: { fg: WHITE, bg: BG_BLACK },
        starDim: { fg: GREEN, bg: BG_BLACK },
        sceneryPrimary: { fg: GREEN, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTGREEN, bg: BG_BLACK },
        sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
        roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: GREEN, bg: BG_BLACK },
        roadStripe: { fg: LIGHTGREEN, bg: BG_BLACK },
        roadEdge: { fg: LIGHTCYAN, bg: BG_BLACK },
        roadGrid: { fg: GREEN, bg: BG_BLACK },
        shoulderPrimary: { fg: GREEN, bg: BG_BLACK },
        shoulderSecondary: { fg: DARKGRAY, bg: BG_BLACK }
    },
    sky: {
        type: 'grid',
        gridDensity: 8,
        converging: true,
        horizontal: true
    },
    background: {
        type: 'mountains',
        config: {
            count: 5,
            minHeight: 3,
            maxHeight: 7,
            parallaxSpeed: 0.08
        }
    },
    celestial: {
        type: 'sun',
        size: 3,
        positionX: 0.5,
        positionY: 0.4
    },
    stars: {
        enabled: true,
        density: 0.3,
        twinkle: true
    },
    ground: {
        type: 'void',
        primary: { fg: GREEN, bg: BG_BLACK },
        secondary: { fg: LIGHTGREEN, bg: BG_BLACK },
        pattern: {
            gridSpacing: 4,
            gridChar: '+'
        }
    },
    roadside: {
        pool: [
            { sprite: 'building', weight: 1, side: 'both' },
            { sprite: 'lamppost', weight: 1, side: 'both' },
            { sprite: 'palm', weight: 1, side: 'both' },
            { sprite: 'umbrella', weight: 1, side: 'both' },
            { sprite: 'tree', weight: 1, side: 'both' },
            { sprite: 'gravestone', weight: 1, side: 'both' },
            { sprite: 'deadtree', weight: 1, side: 'both' },
            { sprite: 'pumpkin', weight: 1, side: 'both' },
            { sprite: 'snowpine', weight: 1, side: 'both' },
            { sprite: 'snowman', weight: 1, side: 'both' },
            { sprite: 'saguaro', weight: 1, side: 'both' },
            { sprite: 'cowskull', weight: 1, side: 'both' },
            { sprite: 'jungle_tree', weight: 1, side: 'both' },
            { sprite: 'fern', weight: 1, side: 'both' },
            { sprite: 'lollipop', weight: 1, side: 'both' },
            { sprite: 'candy_cane', weight: 1, side: 'both' },
            { sprite: 'gummy_bear', weight: 1, side: 'both' },
            { sprite: 'planet', weight: 1, side: 'both' },
            { sprite: 'satellite', weight: 1, side: 'both' },
            { sprite: 'torch', weight: 1, side: 'both' },
            { sprite: 'tower', weight: 1, side: 'both' },
            { sprite: 'flame', weight: 1, side: 'both' },
            { sprite: 'skull_pile', weight: 1, side: 'both' },
            { sprite: 'column', weight: 1, side: 'both' },
            { sprite: 'obelisk', weight: 1, side: 'both' },
            { sprite: 'sphinx', weight: 1, side: 'both' },
            { sprite: 'tire_stack', weight: 1, side: 'both' },
            { sprite: 'hay_bale', weight: 1, side: 'both' }
        ],
        spacing: 35,
        density: 1.2
    }
};
registerTheme(GlitchTheme);
"use strict";
var KaijuRampageTheme = {
    name: 'kaiju_rampage',
    description: 'Destroyed city with fires, rubble, and giant monster silhouette',
    colors: {
        skyTop: { fg: DARKGRAY, bg: BG_BLACK },
        skyMid: { fg: BROWN, bg: BG_BLACK },
        skyHorizon: { fg: LIGHTRED, bg: BG_BLACK },
        skyGrid: { fg: DARKGRAY, bg: BG_BLACK },
        skyGridGlow: { fg: BROWN, bg: BG_BLACK },
        celestialCore: { fg: LIGHTRED, bg: BG_BLACK },
        celestialGlow: { fg: YELLOW, bg: BG_BLACK },
        starBright: { fg: DARKGRAY, bg: BG_BLACK },
        starDim: { fg: BLACK, bg: BG_BLACK },
        sceneryPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        scenerySecondary: { fg: BROWN, bg: BG_BLACK },
        sceneryTertiary: { fg: LIGHTRED, bg: BG_BLACK },
        roadSurface: { fg: LIGHTGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: DARKGRAY, bg: BG_BLACK },
        roadStripe: { fg: WHITE, bg: BG_BLACK },
        roadEdge: { fg: LIGHTRED, bg: BG_BLACK },
        roadGrid: { fg: BROWN, bg: BG_BLACK },
        shoulderPrimary: { fg: BROWN, bg: BG_BLACK },
        shoulderSecondary: { fg: DARKGRAY, bg: BG_BLACK },
        roadsideColors: {
            'rubble': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'fire': {
                primary: { fg: LIGHTRED, bg: BG_BLACK },
                secondary: { fg: YELLOW, bg: BG_BLACK }
            },
            'wrecked_car': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK },
                tertiary: { fg: LIGHTRED, bg: BG_BLACK }
            },
            'fallen_building': {
                primary: { fg: DARKGRAY, bg: BG_BLACK },
                secondary: { fg: BROWN, bg: BG_BLACK }
            },
            'tank': {
                primary: { fg: GREEN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'monster_footprint': {
                primary: { fg: BROWN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'danger_sign': {
                primary: { fg: YELLOW, bg: BG_BLACK },
                secondary: { fg: LIGHTRED, bg: BG_BLACK }
            }
        }
    },
    sky: {
        type: 'gradient',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'destroyed_city',
        config: {
            parallaxSpeed: 0.15
        }
    },
    celestial: {
        type: 'monster',
        size: 5,
        positionX: 0.5,
        positionY: 0.5
    },
    stars: {
        enabled: false,
        density: 0,
        twinkle: false
    },
    ground: {
        type: 'solid',
        primary: { fg: BROWN, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK },
        pattern: {
            gridSpacing: 20,
            radialLines: 0
        }
    },
    roadside: {
        pool: [
            { sprite: 'fleeing_person', weight: 4, side: 'both' },
            { sprite: 'wrecked_car', weight: 3, side: 'both' },
            { sprite: 'fire', weight: 2, side: 'both' },
            { sprite: 'rubble', weight: 2, side: 'both' },
            { sprite: 'danger_sign', weight: 1, side: 'both' }
        ],
        spacing: 12,
        density: 0.9
    }
};
registerTheme(KaijuRampageTheme);
function getKaijuSprite(type) {
    switch (type) {
        case 'rubble': return KaijuSprites.createRubble();
        case 'fire': return KaijuSprites.createFire();
        case 'wrecked_car': return KaijuSprites.createWreckedCar();
        case 'fallen_building': return KaijuSprites.createFallenBuilding();
        case 'tank': return KaijuSprites.createTank();
        case 'monster_footprint': return KaijuSprites.createFootprint();
        case 'danger_sign': return KaijuSprites.createDangerSign();
        default: return null;
    }
}
"use strict";
var UnderwaterTheme = {
    name: 'underwater_grotto',
    description: 'Race through a magical underwater world filled with marine life',
    colors: {
        skyTop: { fg: CYAN, bg: BG_CYAN },
        skyMid: { fg: LIGHTCYAN, bg: BG_BLUE },
        skyHorizon: { fg: LIGHTBLUE, bg: BG_BLUE },
        skyGrid: { fg: LIGHTCYAN, bg: BG_BLUE },
        skyGridGlow: { fg: WHITE, bg: BG_CYAN },
        celestialCore: { fg: LIGHTMAGENTA, bg: BG_BLUE },
        celestialGlow: { fg: LIGHTCYAN, bg: BG_BLUE },
        starBright: { fg: WHITE, bg: BG_BLUE },
        starDim: { fg: LIGHTCYAN, bg: BG_BLUE },
        sceneryPrimary: { fg: LIGHTGRAY, bg: BG_BLACK },
        scenerySecondary: { fg: WHITE, bg: BG_BLUE },
        sceneryTertiary: { fg: LIGHTGREEN, bg: BG_BLUE },
        roadSurface: { fg: LIGHTBLUE, bg: BG_BLUE },
        roadSurfaceAlt: { fg: CYAN, bg: BG_CYAN },
        roadStripe: { fg: WHITE, bg: BG_BLUE },
        roadEdge: { fg: LIGHTCYAN, bg: BG_CYAN },
        roadGrid: { fg: BLUE, bg: BG_BLUE },
        shoulderPrimary: { fg: YELLOW, bg: BG_BLUE },
        shoulderSecondary: { fg: BROWN, bg: BG_CYAN },
        itemBox: {
            border: { fg: LIGHTCYAN, bg: BG_BLUE },
            fill: { fg: CYAN, bg: BG_BLUE },
            symbol: { fg: YELLOW, bg: BG_BLUE }
        },
        roadsideColors: {
            'underwater_fish': {
                primary: { fg: YELLOW, bg: BG_BLUE },
                secondary: { fg: LIGHTRED, bg: BG_BLUE }
            },
            'underwater_coral': {
                primary: { fg: LIGHTRED, bg: BG_BLUE },
                secondary: { fg: LIGHTMAGENTA, bg: BG_BLUE }
            },
            'underwater_seaweed': {
                primary: { fg: LIGHTGREEN, bg: BG_BLUE },
                secondary: { fg: GREEN, bg: BG_BLUE }
            },
            'underwater_anemone': {
                primary: { fg: LIGHTMAGENTA, bg: BG_BLUE },
                secondary: { fg: YELLOW, bg: BG_BLUE }
            },
            'underwater_jellyfish': {
                primary: { fg: LIGHTMAGENTA, bg: BG_BLUE },
                secondary: { fg: LIGHTCYAN, bg: BG_BLUE }
            },
            'underwater_treasure': {
                primary: { fg: YELLOW, bg: BG_BLUE },
                secondary: { fg: BROWN, bg: BG_BLUE }
            }
        }
    },
    sky: {
        type: 'water',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'aquarium',
        config: {
            kelp: true,
            bubbles: true,
            parallaxSpeed: 0.08
        }
    },
    celestial: {
        type: 'mermaid',
        size: 4,
        positionX: 0.5,
        positionY: 0.5
    },
    stars: {
        enabled: true,
        density: 0.3,
        twinkle: true
    },
    ground: {
        type: 'water',
        primary: { fg: LIGHTBLUE, bg: BG_BLUE },
        secondary: { fg: LIGHTCYAN, bg: BG_CYAN },
        pattern: {
            ditherDensity: 0.4,
            ditherChars: ['~', GLYPH.LIGHT_SHADE, '.', 'o']
        }
    },
    roadside: {
        pool: [
            { sprite: 'underwater_fish', weight: 4, side: 'both' },
            { sprite: 'underwater_coral', weight: 4, side: 'both' },
            { sprite: 'underwater_seaweed', weight: 3, side: 'both' },
            { sprite: 'underwater_anemone', weight: 4, side: 'both' },
            { sprite: 'underwater_jellyfish', weight: 3, side: 'both' },
            { sprite: 'underwater_treasure', weight: 1, side: 'both' }
        ],
        spacing: 35,
        density: 1.3
    }
};
registerTheme(UnderwaterTheme);
"use strict";
var ANSITunnelSprites = {
    left: [
        {
            lines: [
                '│',
                '│',
                '│',
                '│'
            ],
            colors: [LIGHTCYAN, CYAN, LIGHTCYAN, CYAN],
            width: 1,
            height: 4
        },
        {
            lines: [
                '┌┐',
                '└┘'
            ],
            colors: [CYAN, CYAN],
            width: 2,
            height: 2
        },
        {
            lines: [
                ' ◊',
                ' │',
                ' │'
            ],
            colors: [LIGHTCYAN, DARKGRAY, DARKGRAY],
            width: 2,
            height: 3
        },
        {
            lines: [
                '1',
                '0',
                '1',
                '0'
            ],
            colors: [LIGHTCYAN, CYAN, LIGHTCYAN, CYAN],
            width: 1,
            height: 4
        }
    ],
    right: [
        {
            lines: [
                '│',
                '│',
                '│',
                '│'
            ],
            colors: [LIGHTCYAN, CYAN, LIGHTCYAN, CYAN],
            width: 1,
            height: 4
        },
        {
            lines: [
                '┌┐',
                '└┘'
            ],
            colors: [CYAN, CYAN],
            width: 2,
            height: 2
        },
        {
            lines: [
                '◊ ',
                '│ ',
                '│ '
            ],
            colors: [LIGHTCYAN, DARKGRAY, DARKGRAY],
            width: 2,
            height: 3
        },
        {
            lines: [
                '0',
                '1',
                '0',
                '1'
            ],
            colors: [CYAN, LIGHTCYAN, CYAN, LIGHTCYAN],
            width: 1,
            height: 4
        }
    ],
    background: {
        layers: [
            {
                speed: 0.1,
                sprites: [
                    {
                        x: 10,
                        lines: ['·'],
                        colors: [DARKGRAY],
                        width: 1,
                        height: 1
                    },
                    {
                        x: 30,
                        lines: ['·'],
                        colors: [CYAN],
                        width: 1,
                        height: 1
                    },
                    {
                        x: 50,
                        lines: ['·'],
                        colors: [DARKGRAY],
                        width: 1,
                        height: 1
                    },
                    {
                        x: 70,
                        lines: ['·'],
                        colors: [CYAN],
                        width: 1,
                        height: 1
                    }
                ]
            }
        ]
    }
};
"use strict";
function getANSITunnelConfig() {
    return {
        directory: OUTRUN_CONFIG.ansiTunnel.directory,
        scrollSpeed: OUTRUN_CONFIG.ansiTunnel.scrollSpeed,
        mirrorSky: true,
        colorShift: true,
        maxRowsToLoad: OUTRUN_CONFIG.ansiTunnel.maxRows
    };
}
var ANSITunnelConfig = getANSITunnelConfig();
var ANSITunnelTheme = {
    name: 'ansi_tunnel',
    description: 'Race through scrolling ANSI art - a digital cyberspace tunnel',
    colors: {
        skyTop: { fg: BLACK, bg: BG_BLACK },
        skyMid: { fg: DARKGRAY, bg: BG_BLACK },
        skyHorizon: { fg: CYAN, bg: BG_BLACK },
        skyGrid: { fg: CYAN, bg: BG_BLACK },
        skyGridGlow: { fg: LIGHTCYAN, bg: BG_BLACK },
        celestialCore: { fg: WHITE, bg: BG_BLACK },
        celestialGlow: { fg: CYAN, bg: BG_BLACK },
        starBright: { fg: LIGHTCYAN, bg: BG_BLACK },
        starDim: { fg: CYAN, bg: BG_BLACK },
        sceneryPrimary: { fg: CYAN, bg: BG_BLACK },
        scenerySecondary: { fg: LIGHTCYAN, bg: BG_BLACK },
        sceneryTertiary: { fg: WHITE, bg: BG_BLACK },
        roadSurface: { fg: DARKGRAY, bg: BG_BLACK },
        roadSurfaceAlt: { fg: BLACK, bg: BG_BLACK },
        roadStripe: { fg: LIGHTCYAN, bg: BG_BLACK },
        roadEdge: { fg: CYAN, bg: BG_BLACK },
        roadGrid: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderPrimary: { fg: DARKGRAY, bg: BG_BLACK },
        shoulderSecondary: { fg: BLACK, bg: BG_BLACK },
        roadsideColors: {
            'data_beacon': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: CYAN, bg: BG_BLACK }
            },
            'data_node': {
                primary: { fg: CYAN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'signal_pole': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: DARKGRAY, bg: BG_BLACK }
            },
            'binary_pillar': {
                primary: { fg: LIGHTCYAN, bg: BG_BLACK },
                secondary: { fg: CYAN, bg: BG_BLACK }
            }
        },
        itemBox: {
            border: { fg: LIGHTCYAN, bg: BG_BLACK },
            fill: { fg: DARKGRAY, bg: BG_BLACK },
            symbol: { fg: WHITE, bg: BG_BLACK }
        }
    },
    sky: {
        type: 'ansi',
        converging: false,
        horizontal: false
    },
    background: {
        type: 'ansi',
        config: {
            parallaxSpeed: 0.0
        }
    },
    celestial: {
        type: 'none',
        size: 0,
        positionX: 0.5,
        positionY: 0.5
    },
    stars: {
        enabled: true,
        density: 0.1,
        twinkle: true
    },
    ground: {
        type: 'solid',
        primary: { fg: BLACK, bg: BG_BLACK },
        secondary: { fg: DARKGRAY, bg: BG_BLACK },
        pattern: {
            ditherDensity: 0.1,
            ditherChars: ['.', GLYPH.LIGHT_SHADE]
        }
    },
    roadside: {
        pool: [
            { sprite: 'data_beacon', weight: 3, side: 'both' },
            { sprite: 'data_node', weight: 2, side: 'both' },
            { sprite: 'signal_pole', weight: 2, side: 'both' },
            { sprite: 'binary_pillar', weight: 3, side: 'both' }
        ],
        spacing: 60,
        density: 0.5
    },
    hud: {
        speedLabel: 'Kbps',
        speedMultiplier: 0.24,
        positionPrefix: 'Node ',
        lapLabel: 'SECTOR',
        timeLabel: 'CONNECT'
    }
};
registerTheme(ANSITunnelTheme);
var ANSITunnelRenderer = (function () {
    function ANSITunnelRenderer() {
        this.combinedCells = [];
        this.combinedWidth = 80;
        this.combinedHeight = 0;
        this.scrollOffset = 0;
        this.loaded = false;
        this._renderDebugLogged = false;
        this.loadANSIFiles();
    }
    ANSITunnelRenderer.prototype.loadANSIFiles = function () {
        var files = ANSILoader.scanDirectory(ANSITunnelConfig.directory);
        if (files.length === 0) {
            logWarning("ANSITunnelRenderer: No ANSI files found in " + ANSITunnelConfig.directory);
            return;
        }
        for (var i = files.length - 1; i > 0; i--) {
            var j = Math.floor(Math.random() * (i + 1));
            var temp = files[i];
            files[i] = files[j];
            files[j] = temp;
        }
        var maxRows = ANSITunnelConfig.maxRowsToLoad || 2000;
        var filesLoaded = 0;
        logInfo("ANSITunnelRenderer: Loading ANSI files (max " + maxRows + " rows)...");
        this.combinedCells = [];
        this.combinedHeight = 0;
        for (var i = 0; i < files.length && this.combinedHeight < maxRows; i++) {
            var img = ANSILoader.load(files[i]);
            if (img && img.height > 0) {
                var rowsToAdd = Math.min(img.height, maxRows - this.combinedHeight);
                for (var row = 0; row < rowsToAdd; row++) {
                    var newRow = [];
                    for (var col = 0; col < this.combinedWidth; col++) {
                        if (col < img.width && img.cells[row] && img.cells[row][col]) {
                            var cell = img.cells[row][col];
                            var ch = cell.char;
                            var code = ch.charCodeAt(0);
                            if (ANSITunnelRenderer.CONTROL_CHARS[code]) {
                                ch = ' ';
                            }
                            newRow.push({ char: ch, attr: cell.attr });
                        }
                        else {
                            newRow.push({ char: ' ', attr: 7 });
                        }
                    }
                    this.combinedCells.push(newRow);
                    this.combinedHeight++;
                }
                filesLoaded++;
                logInfo("ANSITunnelRenderer: Loaded " + files[i].split('/').pop() + " (" + img.height + " rows, total: " + this.combinedHeight + ")");
            }
        }
        this.loaded = this.combinedHeight > 0;
        var estimatedKB = Math.round(this.combinedHeight * this.combinedWidth * 10 / 1024);
        logInfo("ANSITunnelRenderer: Loaded " + filesLoaded + "/" + files.length + " files, " + this.combinedHeight + " rows (~" + estimatedKB + "KB)");
    };
    ANSITunnelRenderer.prototype.getCell = function (row, col) {
        if (!this.loaded || this.combinedHeight === 0) {
            return { char: ' ', attr: 7 };
        }
        var wrappedRow = Math.floor(row) % this.combinedHeight;
        if (wrappedRow < 0)
            wrappedRow += this.combinedHeight;
        var clampedCol = Math.max(0, Math.min(Math.floor(col), this.combinedWidth - 1));
        if (this.combinedCells[wrappedRow] && this.combinedCells[wrappedRow][clampedCol]) {
            return this.combinedCells[wrappedRow][clampedCol];
        }
        return { char: ' ', attr: 7 };
    };
    ANSITunnelRenderer.prototype.updateScroll = function (trackZ, _trackLength) {
        if (!this.loaded || this.combinedHeight === 0)
            return;
        var scrollMultiplier = 0.5;
        this.scrollOffset = (trackZ * scrollMultiplier) % this.combinedHeight;
        if (this.scrollOffset < 0)
            this.scrollOffset += this.combinedHeight;
    };
    ANSITunnelRenderer.prototype.renderTunnel = function (skyFrame, roadFrame, horizonY, roadHeight, screenWidth, trackPosition, cameraX, road, roadLength) {
        if (!this._renderDebugLogged) {
            this._renderDebugLogged = true;
            logInfo('ANSITunnelRenderer.renderTunnel: canvas=' + this.combinedWidth + 'x' + this.combinedHeight + ' horizonY=' + horizonY + ' roadHeight=' + roadHeight);
        }
        if (!this.loaded || this.combinedHeight === 0) {
            this.renderFallback(skyFrame, roadFrame, horizonY, roadHeight, screenWidth);
            return;
        }
        var startRow = (this.combinedHeight - 1) - Math.floor(this.scrollOffset);
        if (skyFrame) {
            for (var frameY = 0; frameY < horizonY; frameY++) {
                var ansiRow = startRow + frameY;
                for (var frameX = 0; frameX < screenWidth; frameX++) {
                    var cell = this.getCell(ansiRow, frameX);
                    skyFrame.setData(frameX, frameY, cell.char, cell.attr);
                }
            }
        }
        if (roadFrame) {
            var roadBottom = roadHeight - 1;
            var blackAttr = makeAttr(BLACK, BG_BLACK);
            var accumulatedCurve = 0;
            for (var screenY = roadBottom; screenY >= 0; screenY--) {
                var ansiRow = startRow + horizonY + screenY;
                var t = (roadBottom - screenY) / Math.max(1, roadBottom);
                var distance = 1 / (1 - t * 0.95);
                var worldZ = trackPosition + distance * 5;
                var segment = road.getSegment(worldZ);
                if (segment) {
                    accumulatedCurve += segment.curve * 0.5;
                }
                var roadWidth = Math.round(40 / distance);
                var halfWidth = Math.floor(roadWidth / 2);
                var curveOffset = accumulatedCurve * distance * 0.8;
                var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
                var leftEdge = centerX - halfWidth;
                var rightEdge = centerX + halfWidth;
                var wrappedZ = worldZ % roadLength;
                if (wrappedZ < 0)
                    wrappedZ += roadLength;
                var isFinishLine = (wrappedZ < 200) || (wrappedZ > roadLength - 200);
                for (var frameX = 0; frameX < screenWidth; frameX++) {
                    var onRoad = frameX >= leftEdge && frameX <= rightEdge;
                    if (onRoad) {
                        if (isFinishLine) {
                            var checkerSize = Math.max(1, Math.floor(3 / distance));
                            var checkerX = Math.floor((frameX - centerX) / checkerSize);
                            var checkerY = Math.floor(screenY / 2);
                            var isWhite = ((checkerX + checkerY) % 2) === 0;
                            if (isWhite) {
                                roadFrame.setData(frameX, screenY, GLYPH.FULL_BLOCK, makeAttr(WHITE, BG_LIGHTGRAY));
                            }
                            else {
                                roadFrame.setData(frameX, screenY, ' ', makeAttr(BLACK, BG_BLACK));
                            }
                        }
                        else {
                            roadFrame.setData(frameX, screenY, ' ', blackAttr);
                        }
                    }
                    else {
                        var cell = this.getCell(ansiRow, frameX);
                        roadFrame.setData(frameX, screenY, cell.char, cell.attr);
                    }
                }
            }
        }
    };
    ANSITunnelRenderer.prototype.renderFallback = function (skyFrame, roadFrame, horizonY, roadHeight, screenWidth) {
        var gridAttr = makeAttr(DARKGRAY, BG_BLACK);
        if (skyFrame) {
            for (var y = 0; y < horizonY; y++) {
                var attr = y < 2 ? makeAttr(BLACK, BG_BLACK) : makeAttr(DARKGRAY, BG_BLACK);
                for (var x = 0; x < screenWidth; x++) {
                    skyFrame.setData(x, y, ' ', attr);
                }
            }
        }
        if (roadFrame) {
            for (var y = 0; y < roadHeight; y++) {
                for (var x = 0; x < screenWidth; x++) {
                    var ch = (y + x) % 4 === 0 ? '.' : ' ';
                    roadFrame.setData(x, y, ch, gridAttr);
                }
            }
        }
    };
    ANSITunnelRenderer.prototype.isLoaded = function () {
        return this.loaded;
    };
    ANSITunnelRenderer.CONTROL_CHARS = {
        0: true,
        7: true,
        8: true,
        9: true,
        10: true,
        12: true,
        13: true,
        27: true
    };
    return ANSITunnelRenderer;
}());
var ansiTunnelRenderer = null;
function getANSITunnelRenderer() {
    if (!ansiTunnelRenderer) {
        ansiTunnelRenderer = new ANSITunnelRenderer();
    }
    return ansiTunnelRenderer;
}
"use strict";
var FrameManager = (function () {
    function FrameManager(width, height, horizonY) {
        this.width = width;
        this.height = height;
        this.horizonY = horizonY;
        this.layers = [];
        this.roadsidePool = [];
        this.vehicleFrames = [];
        this.skyGridFrame = null;
        this.mountainsFrame = null;
        this.sunFrame = null;
        this.groundGridFrame = null;
        this.roadFrame = null;
        this.hudFrame = null;
        this.rootFrame = null;
    }
    FrameManager.prototype.init = function () {
        this.rootFrame = new Frame(1, 1, this.width, this.height, BG_BLACK);
        this.rootFrame.open();
        this.skyGridFrame = new Frame(1, 1, this.width, this.horizonY, BG_BLACK, this.rootFrame);
        this.skyGridFrame.open();
        this.addLayer(this.skyGridFrame, 'skyGrid', 1);
        this.sunFrame = new Frame(1, 1, this.width, this.horizonY, BG_BLACK, this.rootFrame);
        this.sunFrame.transparent = true;
        this.sunFrame.open();
        this.addLayer(this.sunFrame, 'sun', 2);
        this.mountainsFrame = new Frame(1, 1, this.width, this.horizonY, BG_BLACK, this.rootFrame);
        this.mountainsFrame.transparent = true;
        this.mountainsFrame.open();
        this.addLayer(this.mountainsFrame, 'mountains', 3);
        var roadHeight = this.height - this.horizonY;
        this.groundGridFrame = new Frame(1, this.horizonY + 1, this.width, roadHeight, BG_BLACK, this.rootFrame);
        this.groundGridFrame.open();
        this.addLayer(this.groundGridFrame, 'groundGrid', 4);
        this.roadFrame = new Frame(1, this.horizonY + 1, this.width, roadHeight, BG_BLACK, this.rootFrame);
        this.roadFrame.transparent = true;
        this.roadFrame.open();
        this.addLayer(this.roadFrame, 'road', 5);
        this.hudFrame = new Frame(1, 1, this.width, this.height, BG_BLACK, this.rootFrame);
        this.hudFrame.transparent = true;
        this.hudFrame.open();
        this.addLayer(this.hudFrame, 'hud', 100);
        this.initRoadsidePool(20);
        this.initVehicleFrames(8);
    };
    FrameManager.prototype.addLayer = function (frame, name, zIndex) {
        this.layers.push({ frame: frame, name: name, zIndex: zIndex });
    };
    FrameManager.prototype.initRoadsidePool = function (count) {
        for (var i = 0; i < count; i++) {
            var spriteFrame = new Frame(1, 1, 8, 6, BG_BLACK, this.rootFrame);
            spriteFrame.transparent = true;
            this.roadsidePool.push(spriteFrame);
        }
    };
    FrameManager.prototype.initVehicleFrames = function (count) {
        for (var i = 0; i < count; i++) {
            var vehicleFrame = new Frame(1, 1, 7, 4, BG_BLACK, this.rootFrame);
            vehicleFrame.transparent = true;
            this.vehicleFrames.push(vehicleFrame);
        }
    };
    FrameManager.prototype.getSkyGridFrame = function () {
        return this.skyGridFrame;
    };
    FrameManager.prototype.getMountainsFrame = function () {
        return this.mountainsFrame;
    };
    FrameManager.prototype.getSunFrame = function () {
        return this.sunFrame;
    };
    FrameManager.prototype.getRoadFrame = function () {
        return this.roadFrame;
    };
    FrameManager.prototype.getGroundGridFrame = function () {
        return this.groundGridFrame;
    };
    FrameManager.prototype.getHudFrame = function () {
        return this.hudFrame;
    };
    FrameManager.prototype.getRoadsideFrame = function (index) {
        if (index >= 0 && index < this.roadsidePool.length) {
            return this.roadsidePool[index];
        }
        return null;
    };
    FrameManager.prototype.getVehicleFrame = function (index) {
        if (index >= 0 && index < this.vehicleFrames.length) {
            return this.vehicleFrames[index];
        }
        return null;
    };
    FrameManager.prototype.getRoadsidePoolSize = function () {
        return this.roadsidePool.length;
    };
    FrameManager.prototype.positionRoadsideFrame = function (index, x, y, visible) {
        var frame = this.roadsidePool[index];
        if (!frame)
            return;
        if (visible) {
            frame.moveTo(x, y);
            if (!frame.is_open) {
                frame.open();
            }
        }
        else {
            if (frame.is_open) {
                frame.close();
            }
        }
    };
    FrameManager.prototype.positionVehicleFrame = function (index, x, y, visible) {
        var frame = this.vehicleFrames[index];
        if (!frame)
            return;
        if (visible) {
            frame.moveTo(x, y);
            if (!frame.is_open) {
                frame.open();
            }
            frame.top();
        }
        else {
            if (frame.is_open) {
                frame.close();
            }
        }
    };
    FrameManager.prototype.cycle = function () {
        this.rootFrame.cycle();
    };
    FrameManager.prototype.clearFrame = function (frame) {
        if (frame) {
            frame.clear();
        }
    };
    FrameManager.prototype.getRootFrame = function () {
        return this.rootFrame;
    };
    FrameManager.prototype.shutdown = function () {
        if (this.hudFrame)
            this.hudFrame.close();
        for (var i = 0; i < this.vehicleFrames.length; i++) {
            if (this.vehicleFrames[i].is_open) {
                this.vehicleFrames[i].close();
            }
        }
        for (var j = 0; j < this.roadsidePool.length; j++) {
            if (this.roadsidePool[j].is_open) {
                this.roadsidePool[j].close();
            }
        }
        if (this.roadFrame)
            this.roadFrame.close();
        if (this.sunFrame)
            this.sunFrame.close();
        if (this.mountainsFrame)
            this.mountainsFrame.close();
        if (this.skyGridFrame)
            this.skyGridFrame.close();
        this.rootFrame.close();
    };
    return FrameManager;
}());
"use strict";
var SpriteSheet = {
    createTree: function () {
        var leafTop = makeAttr(LIGHTGREEN, BG_BLACK);
        var leafDark = makeAttr(GREEN, BG_BLACK);
        var trunk = makeAttr(BROWN, BG_BLACK);
        var leafTrunk = makeAttr(LIGHTGREEN, BG_BROWN);
        var U = null;
        return {
            name: 'tree',
            variants: [
                [
                    [{ char: GLYPH.UPPER_HALF, attr: leafTrunk }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
                    [U, { char: GLYPH.UPPER_HALF, attr: leafTrunk }, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
                    [{ char: GLYPH.DARK_SHADE, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.DARK_SHADE, attr: leafDark }],
                    [U, { char: GLYPH.UPPER_HALF, attr: leafTrunk }, { char: GLYPH.UPPER_HALF, attr: leafTrunk }, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
                    [{ char: GLYPH.DARK_SHADE, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.DARK_SHADE, attr: leafDark }],
                    [U, { char: GLYPH.LOWER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.LOWER_HALF, attr: leafDark }, U],
                    [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, U, U]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.UPPER_HALF, attr: leafDark }],
                    [{ char: GLYPH.FULL_BLOCK, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafDark }],
                    [U, { char: GLYPH.DARK_SHADE, attr: leafDark }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.FULL_BLOCK, attr: leafTop }, { char: GLYPH.DARK_SHADE, attr: leafDark }, U],
                    [U, U, { char: GLYPH.FULL_BLOCK, attr: trunk }, { char: GLYPH.FULL_BLOCK, attr: trunk }, U, U],
                    [U, U, { char: GLYPH.UPPER_HALF, attr: trunk }, { char: GLYPH.UPPER_HALF, attr: trunk }, U, U]
                ]
            ]
        };
    },
    createRock: function () {
        var rock = makeAttr(DARKGRAY, BG_BLACK);
        var rockLight = makeAttr(LIGHTGRAY, BG_BLACK);
        return {
            name: 'rock',
            variants: [
                [
                    [{ char: GLYPH.LOWER_HALF, attr: rock }]
                ],
                [
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.LOWER_HALF, attr: rock }]
                ],
                [
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rock }],
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rockLight }, { char: GLYPH.UPPER_HALF, attr: rock }, { char: GLYPH.UPPER_HALF, attr: rock }],
                    [{ char: GLYPH.LOWER_HALF, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rockLight }, { char: GLYPH.FULL_BLOCK, attr: rock }, { char: GLYPH.LOWER_HALF, attr: rock }]
                ]
            ]
        };
    },
    createBush: function () {
        var bush = makeAttr(GREEN, BG_BLACK);
        var bushLight = makeAttr(LIGHTGREEN, BG_BLACK);
        var U = null;
        return {
            name: 'bush',
            variants: [
                [
                    [{ char: GLYPH.LOWER_HALF, attr: bush }]
                ],
                [
                    [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.LOWER_HALF, attr: bush }]
                ],
                [
                    [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.LOWER_HALF, attr: bush }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: bush }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bush }, U],
                    [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.LOWER_HALF, attr: bush }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: bush }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bushLight }, { char: GLYPH.UPPER_HALF, attr: bush }, { char: GLYPH.UPPER_HALF, attr: bush }],
                    [{ char: GLYPH.LOWER_HALF, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bushLight }, { char: GLYPH.FULL_BLOCK, attr: bush }, { char: GLYPH.LOWER_HALF, attr: bush }]
                ]
            ]
        };
    },
    createPlayerCar: function () {
        var body = makeAttr(YELLOW, BG_BLACK);
        var trim = makeAttr(WHITE, BG_BLACK);
        var wheel = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'player_car',
            variants: [
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }],
                    [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
                ]
            ]
        };
    },
    createAICar: function (color) {
        var body = makeAttr(color, BG_BLACK);
        var trim = makeAttr(WHITE, BG_BLACK);
        var wheel = makeAttr(DARKGRAY, BG_BLACK);
        var U = null;
        return {
            name: 'ai_car',
            variants: [
                [
                    [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }],
                    [{ char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }]
                ],
                [
                    [{ char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }],
                    [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }]
                ],
                [
                    [U, { char: GLYPH.UPPER_HALF, attr: trim }, { char: GLYPH.FULL_BLOCK, attr: trim }, { char: GLYPH.UPPER_HALF, attr: trim }, U],
                    [{ char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }, { char: GLYPH.FULL_BLOCK, attr: body }],
                    [{ char: GLYPH.LOWER_HALF, attr: wheel }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: body }, { char: GLYPH.LOWER_HALF, attr: wheel }]
                ]
            ]
        };
    }
};
function renderSpriteToFrame(frame, sprite, scaleIndex) {
    var variant = sprite.variants[scaleIndex];
    if (!variant) {
        variant = sprite.variants[sprite.variants.length - 1];
    }
    frame.clear();
    for (var row = 0; row < variant.length; row++) {
        for (var col = 0; col < variant[row].length; col++) {
            var cell = variant[row][col];
            if (cell !== null && cell !== undefined) {
                frame.setData(col, row, cell.char, cell.attr);
            }
        }
    }
}
function getSpriteSize(sprite, scaleIndex) {
    var variant = sprite.variants[scaleIndex];
    if (!variant) {
        variant = sprite.variants[sprite.variants.length - 1];
    }
    var height = variant.length;
    var width = 0;
    for (var row = 0; row < variant.length; row++) {
        if (variant[row].length > width) {
            width = variant[row].length;
        }
    }
    return { width: width, height: height };
}
registerRoadsideSprite('tree', SpriteSheet.createTree);
registerRoadsideSprite('rock', SpriteSheet.createRock);
registerRoadsideSprite('bush', SpriteSheet.createBush);
"use strict";
var FrameRenderer = (function () {
    function FrameRenderer(width, height) {
        this._currentBrakeLightsOn = false;
        this.width = width;
        this.height = height;
        this.horizonY = 8;
        this._mountainScrollOffset = 0;
        this._staticElementsDirty = true;
        this._skyGridAnimPhase = 0;
        this._fireAnimPhase = 0;
        this._currentRoad = null;
        this._currentTrackPosition = 0;
        this._currentCameraX = 0;
        this._lightningBolts = [];
        this._ansiTunnelRenderer = null;
        this._ansiDebugLogged = false;
        this._ansiFrameLogged = false;
        this.frameManager = new FrameManager(width, height, this.horizonY);
        this.composer = new SceneComposer(width, height);
        this.activeTheme = SynthwaveTheme;
        this.spriteCache = {};
    }
    FrameRenderer.prototype.setTheme = function (themeName) {
        var theme = getTheme(themeName);
        if (theme) {
            this.activeTheme = theme;
            this._staticElementsDirty = true;
            this.rebuildSpriteCache();
            if (themeName === 'ansi_tunnel') {
                this._ansiTunnelRenderer = new ANSITunnelRenderer();
                logInfo('ANSI Tunnel renderer initialized');
            }
            else {
                this._ansiTunnelRenderer = null;
            }
            if (this.frameManager.getSunFrame()) {
                this.clearStaticFrames();
                this.renderStaticElements();
            }
            logInfo('Theme changed to: ' + themeName);
        }
        else {
            logWarning('Theme not found: ' + themeName);
        }
    };
    FrameRenderer.prototype.getAvailableThemes = function () {
        return getThemeNames();
    };
    FrameRenderer.prototype.init = function () {
        load('frame.js');
        this.frameManager.init();
        this.rebuildSpriteCache();
        this.renderStaticElements();
        logInfo('FrameRenderer initialized with theme: ' + this.activeTheme.name);
    };
    FrameRenderer.prototype.rebuildSpriteCache = function () {
        this.spriteCache = {};
        var pool = this.activeTheme.roadside.pool;
        for (var i = 0; i < pool.length; i++) {
            var entry = pool[i];
            var creator = ROADSIDE_SPRITES[entry.sprite];
            if (creator) {
                this.spriteCache[entry.sprite] = creator();
            }
        }
    };
    FrameRenderer.prototype.selectFromPool = function (worldZ) {
        var pool = this.activeTheme.roadside.pool;
        var totalWeight = 0;
        for (var i = 0; i < pool.length; i++) {
            totalWeight += pool[i].weight;
        }
        var hash = (Math.floor(worldZ) * 7919) % totalWeight;
        var cumulative = 0;
        for (var j = 0; j < pool.length; j++) {
            cumulative += pool[j].weight;
            if (hash < cumulative) {
                return { sprite: pool[j].sprite, side: pool[j].side || 'both' };
            }
        }
        return { sprite: pool[0].sprite, side: pool[0].side || 'both' };
    };
    FrameRenderer.prototype.clearStaticFrames = function () {
        var sunFrame = this.frameManager.getSunFrame();
        var mtnsFrame = this.frameManager.getMountainsFrame();
        if (sunFrame)
            sunFrame.clear();
        if (mtnsFrame)
            mtnsFrame.clear();
    };
    FrameRenderer.prototype.renderStaticElements = function () {
        if (this.activeTheme.celestial.type === 'sun') {
            this.renderSun();
        }
        else if (this.activeTheme.celestial.type === 'moon') {
            this.renderMoon();
        }
        else if (this.activeTheme.celestial.type === 'dual_moons') {
            this.renderDualMoons();
        }
        else if (this.activeTheme.celestial.type === 'monster') {
            this.renderMonsterSilhouette();
        }
        else if (this.activeTheme.celestial.type === 'mermaid') {
            this.renderMermaid();
        }
        if (this.activeTheme.background.type === 'mountains') {
            this.renderMountains();
        }
        else if (this.activeTheme.background.type === 'skyscrapers') {
            this.renderSkyscrapers();
        }
        else if (this.activeTheme.background.type === 'ocean') {
            this.renderOceanIslands();
        }
        else if (this.activeTheme.background.type === 'forest') {
            this.renderForestTreeline();
        }
        else if (this.activeTheme.background.type === 'jungle_canopy') {
            this.renderJungleCanopy();
        }
        else if (this.activeTheme.background.type === 'candy_hills') {
            this.renderCandyHills();
        }
        else if (this.activeTheme.background.type === 'nebula') {
            this.renderNebula();
        }
        else if (this.activeTheme.background.type === 'castle_fortress') {
            this.renderCastleFortress();
        }
        else if (this.activeTheme.background.type === 'volcanic') {
            this.renderVolcanic();
        }
        else if (this.activeTheme.background.type === 'pyramids') {
            this.renderPyramids();
        }
        else if (this.activeTheme.background.type === 'dunes') {
            this.renderDunes();
        }
        else if (this.activeTheme.background.type === 'stadium') {
            this.renderStadium();
        }
        else if (this.activeTheme.background.type === 'destroyed_city') {
            this.renderDestroyedCity();
        }
        else if (this.activeTheme.background.type === 'underwater') {
            this.renderUnderwaterBackground();
        }
        else if (this.activeTheme.background.type === 'aquarium') {
            this.renderAquariumBackground();
        }
        else if (this.activeTheme.background.type === 'ansi') {
            this.renderANSITunnelStatic();
        }
        this._staticElementsDirty = false;
        logDebug('Static elements rendered, dirty=' + this._staticElementsDirty);
    };
    FrameRenderer.prototype.beginFrame = function () {
    };
    FrameRenderer.prototype.renderSky = function (trackPosition, curvature, playerSteer, speed, dt) {
        if (this.activeTheme.name === 'glitch_circuit' && typeof GlitchState !== 'undefined') {
            GlitchState.update(trackPosition, dt || 0.016);
        }
        this._fireAnimPhase += (dt || 0.016) * 8;
        if (this.activeTheme.sky.type === 'grid') {
            this.renderSkyGrid(speed || 0, dt || 0);
        }
        else if (this.activeTheme.sky.type === 'stars') {
            this.renderSkyStars(trackPosition);
        }
        else if (this.activeTheme.sky.type === 'gradient') {
            this.renderSkyGradient(trackPosition);
        }
        else if (this.activeTheme.sky.type === 'water') {
            this.renderSkyWater(trackPosition, speed || 0, dt || 0);
        }
        else if (this.activeTheme.sky.type === 'ansi') {
            if (this._ansiTunnelRenderer && this._currentRoad) {
                this._ansiTunnelRenderer.updateScroll(trackPosition, this._currentRoad.totalLength);
            }
        }
        if (this.activeTheme.background.type === 'ocean') {
            this.renderOceanWaves(trackPosition);
        }
        if (curvature !== undefined && playerSteer !== undefined && speed !== undefined && dt !== undefined && speed > 0) {
            this.updateParallax(curvature, playerSteer, speed, dt);
        }
    };
    FrameRenderer.prototype.renderRoad = function (trackPosition, cameraX, _track, road) {
        this._currentRoad = road;
        this._currentTrackPosition = trackPosition;
        this._currentCameraX = cameraX;
        var bgType = this.activeTheme.background ? this.activeTheme.background.type : 'none';
        if (!this._ansiDebugLogged) {
            this._ansiDebugLogged = true;
            logInfo('renderRoad check: bgType=' + bgType + ' hasRenderer=' + (this._ansiTunnelRenderer ? 'yes' : 'no') + ' theme=' + this.activeTheme.name);
        }
        if (this._ansiTunnelRenderer && bgType === 'ansi') {
            this._ansiTunnelRenderer.updateScroll(trackPosition, road.totalLength);
            var skyFrame = this.frameManager.getSkyGridFrame();
            var roadFrame = this.frameManager.getRoadFrame();
            var roadFrameHeight = this.height - this.horizonY;
            var ansiRoadHeight = roadFrameHeight;
            if (!this._ansiFrameLogged) {
                this._ansiFrameLogged = true;
                logInfo('ANSI frames: sky=' + (skyFrame ? 'ok' : 'null') + ' road=' + (roadFrame ? 'ok' : 'null') + ' horizonY=' + this.horizonY + ' roadFrameHeight=' + roadFrameHeight + ' ansiRoadHeight=' + ansiRoadHeight);
            }
            if (roadFrame) {
                roadFrame.clear();
            }
            this._ansiTunnelRenderer.renderTunnel(skyFrame, roadFrame, this.horizonY, ansiRoadHeight, this.width, trackPosition, cameraX, road, road.totalLength);
            this.renderANSIRoadStripes(trackPosition, cameraX, road, ansiRoadHeight);
            var roadsideObjects = this.buildRoadsideObjects(trackPosition, cameraX, road);
            this.renderRoadsideSprites(roadsideObjects);
            return;
        }
        if (this.activeTheme.ground) {
            if (this.activeTheme.ground.type === 'grid') {
                this.renderHolodeckFloor(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'lava') {
                this.renderLavaGround(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'candy') {
                this.renderCandyGround(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'void') {
                this.renderVoidGround(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'cobblestone') {
                this.renderCobblestoneGround(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'jungle') {
                this.renderJungleGround(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'dirt') {
                this.renderDirtGround(trackPosition);
            }
            else if (this.activeTheme.ground.type === 'water') {
                this.renderWaterGround(trackPosition);
            }
        }
        this.renderRoadSurface(trackPosition, cameraX, road);
        var roadsideObjects = this.buildRoadsideObjects(trackPosition, cameraX, road);
        this.renderRoadsideSprites(roadsideObjects);
    };
    FrameRenderer.prototype.buildRoadsideObjects = function (trackPosition, cameraX, road) {
        var objects = [];
        var roadHeight = this.height - this.horizonY;
        var viewDistanceWorld = 100;
        var startZ = trackPosition;
        var endZ = trackPosition + viewDistanceWorld;
        var spacing = this.activeTheme.roadside.spacing;
        var firstObjectZ = Math.ceil(startZ / spacing) * spacing;
        for (var worldZ = firstObjectZ; worldZ < endZ; worldZ += spacing) {
            var selection = this.selectFromPool(worldZ);
            var spriteType = selection.sprite;
            var allowedSide = selection.side;
            var worldZInt = Math.floor(worldZ);
            var relativeZ = worldZ - trackPosition;
            if (relativeZ <= 0)
                continue;
            var distance = relativeZ / 5;
            if (distance < 1 || distance > 20)
                continue;
            var t = 1 - (1 / distance);
            var screenY = Math.round(this.horizonY + roadHeight * (1 - t));
            if (screenY <= this.horizonY || screenY >= this.height)
                continue;
            var accumulatedCurve = 0;
            for (var z = trackPosition; z < worldZ; z += 5) {
                var seg = road.getSegment(z);
                if (seg)
                    accumulatedCurve += seg.curve * 0.5;
            }
            var curveOffset = accumulatedCurve * distance * 0.8;
            var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
            var roadHalfWidth = Math.round(20 / distance);
            var leftEdge = centerX - roadHalfWidth;
            var rightEdge = centerX + roadHalfWidth;
            var edgeOffset = Math.round(15 / distance) + 3;
            var leftX = leftEdge - edgeOffset;
            var rightX = rightEdge + edgeOffset;
            var preferredSide = (Math.floor(worldZ / spacing) % 2 === 0) ? 'left' : 'right';
            if (allowedSide === 'left' || (allowedSide === 'both' && preferredSide === 'left')) {
                if (leftX >= 0) {
                    objects.push({ x: leftX, y: screenY, distance: distance, type: spriteType });
                }
            }
            if (allowedSide === 'right' || (allowedSide === 'both' && preferredSide === 'right')) {
                if (rightX < 80) {
                    objects.push({ x: rightX, y: screenY, distance: distance, type: spriteType });
                }
            }
            if (this.activeTheme.roadside.density > 1.0 && (worldZInt % 2 === 0)) {
                if (allowedSide === 'both' || allowedSide === 'right') {
                    if (preferredSide === 'left' && rightX < 80) {
                        objects.push({ x: rightX, y: screenY, distance: distance, type: spriteType });
                    }
                }
                if (allowedSide === 'both' || allowedSide === 'left') {
                    if (preferredSide === 'right' && leftX >= 0) {
                        objects.push({ x: leftX, y: screenY, distance: distance, type: spriteType });
                    }
                }
            }
        }
        objects.sort(function (a, b) { return b.distance - a.distance; });
        return objects;
    };
    FrameRenderer.prototype.renderEntities = function (playerVehicle, vehicles, items, projectiles) {
        this.renderItemBoxes(playerVehicle, items);
        if (projectiles && projectiles.length > 0) {
            this.renderProjectiles(playerVehicle, projectiles);
        }
        this.renderNPCVehicles(playerVehicle, vehicles);
        var v = playerVehicle;
        this.renderPlayerVehicle(playerVehicle.playerX, playerVehicle.flashTimer > 0, playerVehicle.boostTimer > 0, v.hasEffect ? v.hasEffect(ItemType.STAR) : false, v.hasEffect ? v.hasEffect(ItemType.BULLET) : false, v.hasEffect ? v.hasEffect(ItemType.LIGHTNING) : false, v.carId || 'sports', v.carColorId || 'yellow', this._currentBrakeLightsOn);
    };
    FrameRenderer.prototype.renderProjectiles = function (playerVehicle, projectiles) {
        var frame = this.frameManager.getRoadFrame();
        if (!frame)
            return;
        var visualHorizonY = 5;
        var roadBottom = this.height - 4;
        var roadHeight = roadBottom - visualHorizonY;
        var greenSprites = [
            ['.'],
            ['o'],
            ['(O)'],
            ['_/O\\_', ' \\O/']
        ];
        var redSprites = [
            ['.'],
            ['o'],
            ['(O)'],
            ['_/O\\_', ' \\O/']
        ];
        var blueSprites = [
            ['*'],
            ['@'],
            ['<@>'],
            ['~/~@~\\~', ' ~\\@/~']
        ];
        var bananaSprites = [
            ['.'],
            ['o'],
            ['(o)'],
            [' /\\\\ ', '(__)']
        ];
        for (var i = 0; i < projectiles.length; i++) {
            var projectile = projectiles[i];
            if (projectile.isDestroyed)
                continue;
            var isBanana = projectile.speed === 0;
            var distZ = projectile.trackZ - playerVehicle.trackZ;
            if (distZ < -5 || distZ > 600)
                continue;
            var maxViewDist = 500;
            var normalizedDist = Math.max(0.01, distZ / maxViewDist);
            var t = Math.max(0, Math.min(1, 1 - normalizedDist));
            var screenY = Math.round(visualHorizonY + t * roadHeight);
            var curveOffset = 0;
            if (this._currentRoad && distZ > 0) {
                var projWorldZ = this._currentTrackPosition + distZ;
                var seg = this._currentRoad.getSegment(projWorldZ);
                if (seg) {
                    curveOffset = seg.curve * t * 15;
                }
            }
            var perspectiveScale = t * t;
            var relativeX = projectile.playerX - playerVehicle.playerX;
            var screenX = Math.round(40 + curveOffset + relativeX * perspectiveScale * 25 - this._currentCameraX * 0.5);
            var screenProgress = (screenY - visualHorizonY) / roadHeight;
            var scaleIndex;
            if (screenProgress < 0.08) {
                scaleIndex = 0;
            }
            else if (screenProgress < 0.20) {
                scaleIndex = 1;
            }
            else if (screenProgress < 0.40) {
                scaleIndex = 2;
            }
            else {
                scaleIndex = 3;
            }
            var sprites;
            var attr;
            if (isBanana) {
                sprites = bananaSprites;
                attr = makeAttr(YELLOW, BG_BLACK);
            }
            else {
                var shell = projectile;
                if (shell.shellType === ShellType.GREEN) {
                    sprites = greenSprites;
                    attr = makeAttr(LIGHTGREEN, BG_BLACK);
                }
                else if (shell.shellType === ShellType.RED) {
                    sprites = redSprites;
                    attr = makeAttr(LIGHTRED, BG_BLACK);
                }
                else {
                    sprites = blueSprites;
                    attr = makeAttr(LIGHTBLUE, BG_BLACK);
                }
            }
            var spriteLines = sprites[scaleIndex];
            var spriteWidth = spriteLines[0].length;
            var spriteHeight = spriteLines.length;
            var startX = screenX - Math.floor(spriteWidth / 2);
            var startY = screenY - (spriteHeight - 1);
            for (var ly = 0; ly < spriteHeight; ly++) {
                var line = spriteLines[ly];
                var drawY = startY + ly;
                if (drawY < visualHorizonY || drawY >= roadBottom)
                    continue;
                for (var lx = 0; lx < line.length; lx++) {
                    var ch = line.charAt(lx);
                    if (ch === ' ')
                        continue;
                    var drawX = startX + lx;
                    if (drawX < 0 || drawX >= 80)
                        continue;
                    frame.setData(drawX, drawY, ch, attr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderItemBoxes = function (playerVehicle, items) {
        var frame = this.frameManager.getRoadFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var boxColors = colors.itemBox || {
            border: { fg: YELLOW, bg: BG_BLACK },
            fill: { fg: BROWN, bg: BG_BLACK },
            symbol: { fg: WHITE, bg: BG_BLACK }
        };
        var borderAttr = makeAttr(boxColors.border.fg, boxColors.border.bg);
        var fillAttr = makeAttr(boxColors.fill.fg, boxColors.fill.bg);
        var symbolAttr = makeAttr(boxColors.symbol.fg, boxColors.symbol.bg);
        var visualHorizonY = 5;
        var roadBottom = this.height - 4;
        var itemsToRender = [];
        for (var p = 0; p < items.length; p++) {
            var pItem = items[p];
            if (pItem.isBeingDestroyed() && pItem.pickedUpByPlayer) {
                this.renderPlayerPickupEffect(frame, pItem.destructionTimer, pItem.destructionStartTime, symbolAttr);
            }
        }
        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            if (!item.isAvailable() && !item.isBeingDestroyed())
                continue;
            if (item.isBeingDestroyed() && item.pickedUpByPlayer)
                continue;
            var relativeZ = item.z - playerVehicle.trackZ;
            var relativeX = item.x - (playerVehicle.playerX * 20);
            if (relativeZ < 5 || relativeZ > 300)
                continue;
            var maxViewDist = 300;
            var normalizedDist = Math.max(0.01, relativeZ / maxViewDist);
            var t = Math.max(0, Math.min(1, 1 - normalizedDist));
            var screenY = Math.round(visualHorizonY + t * (roadBottom - visualHorizonY));
            var curveOffset = 0;
            if (this._currentRoad && relativeZ > 0) {
                var itemWorldZ = this._currentTrackPosition + relativeZ;
                var seg = this._currentRoad.getSegment(itemWorldZ);
                if (seg) {
                    curveOffset = seg.curve * t * 15;
                }
            }
            var xScale = t * 1.5;
            var screenX = Math.round(40 + curveOffset + relativeX * xScale - this._currentCameraX * 0.5);
            if (screenY < visualHorizonY - 2 || screenY >= this.height)
                continue;
            if (screenX < -3 || screenX >= this.width + 3)
                continue;
            itemsToRender.push({ item: item, screenY: screenY, screenX: screenX, t: t });
        }
        itemsToRender.sort(function (a, b) { return a.screenY - b.screenY; });
        for (var j = 0; j < itemsToRender.length; j++) {
            var data = itemsToRender[j];
            var item = data.item;
            var screenX = data.screenX;
            var screenY = data.screenY;
            var t = data.t;
            var pulse = Math.floor(Date.now() / 150) % 4;
            if (item.isBeingDestroyed()) {
                this.renderItemBoxDestruction(frame, screenX, screenY, t, item.destructionTimer, item.destructionStartTime);
                continue;
            }
            if (t > 0.5) {
                this.renderItemBoxLarge(frame, screenX, screenY, borderAttr, fillAttr, symbolAttr, pulse);
            }
            else if (t > 0.25) {
                this.renderItemBoxMedium(frame, screenX, screenY, borderAttr, symbolAttr, pulse);
            }
            else if (t > 0.1) {
                this.renderItemBoxSmall(frame, screenX, screenY, borderAttr, symbolAttr, pulse);
            }
            else {
                if (screenX >= 0 && screenX < this.width && screenY >= 0 && screenY < this.height) {
                    var tinyAttr = (pulse % 2 === 0) ? borderAttr : symbolAttr;
                    frame.setData(screenX, screenY, '.', tinyAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderItemBoxLarge = function (frame, cx, cy, borderAttr, _fillAttr, symbolAttr, pulse) {
        var TL = GLYPH.DBOX_TL;
        var TR = GLYPH.DBOX_TR;
        var BL = GLYPH.DBOX_BL;
        var BR = GLYPH.DBOX_BR;
        var H = GLYPH.DBOX_H;
        var V = GLYPH.DBOX_V;
        var activeBorderAttr = (pulse === 0 || pulse === 2) ? borderAttr :
            makeAttr(WHITE, borderAttr & 0xF0);
        var left = cx - 1;
        var right = cx + 1;
        var top = cy - 1;
        var bottom = cy + 1;
        if (top >= 0 && top < this.height) {
            if (left >= 0 && left < this.width)
                frame.setData(left, top, TL, activeBorderAttr);
            if (cx >= 0 && cx < this.width)
                frame.setData(cx, top, H, activeBorderAttr);
            if (right >= 0 && right < this.width)
                frame.setData(right, top, TR, activeBorderAttr);
        }
        if (cy >= 0 && cy < this.height) {
            if (left >= 0 && left < this.width)
                frame.setData(left, cy, V, activeBorderAttr);
            if (cx >= 0 && cx < this.width) {
                var qAttr = (pulse % 2 === 0) ? symbolAttr : makeAttr(YELLOW, symbolAttr & 0xF0);
                frame.setData(cx, cy, '?', qAttr);
            }
            if (right >= 0 && right < this.width)
                frame.setData(right, cy, V, activeBorderAttr);
        }
        if (bottom >= 0 && bottom < this.height) {
            if (left >= 0 && left < this.width)
                frame.setData(left, bottom, BL, activeBorderAttr);
            if (cx >= 0 && cx < this.width)
                frame.setData(cx, bottom, H, activeBorderAttr);
            if (right >= 0 && right < this.width)
                frame.setData(right, bottom, BR, activeBorderAttr);
        }
    };
    FrameRenderer.prototype.renderItemBoxMedium = function (frame, cx, cy, borderAttr, symbolAttr, pulse) {
        var left = cx - 1;
        var right = cx + 1;
        var activeBorderAttr = (pulse % 2 === 0) ? borderAttr : makeAttr(WHITE, borderAttr & 0xF0);
        if (cy >= 0 && cy < this.height) {
            if (left >= 0 && left < this.width)
                frame.setData(left, cy, '[', activeBorderAttr);
            if (cx >= 0 && cx < this.width) {
                var qAttr = (pulse % 2 === 0) ? symbolAttr : makeAttr(YELLOW, symbolAttr & 0xF0);
                frame.setData(cx, cy, '?', qAttr);
            }
            if (right >= 0 && right < this.width)
                frame.setData(right, cy, ']', activeBorderAttr);
        }
        var bottom = cy + 1;
        if (bottom >= 0 && bottom < this.height) {
            if (cx >= 0 && cx < this.width)
                frame.setData(cx, bottom, GLYPH.LOWER_HALF, makeAttr(DARKGRAY, BG_BLACK));
        }
    };
    FrameRenderer.prototype.renderItemBoxSmall = function (frame, cx, cy, borderAttr, symbolAttr, pulse) {
        if (cx < 0 || cx >= this.width || cy < 0 || cy >= this.height)
            return;
        var char = (pulse % 2 === 0) ? '?' : GLYPH.FULL_BLOCK;
        var attr = (pulse % 2 === 0) ? symbolAttr : borderAttr;
        frame.setData(cx, cy, char, attr);
    };
    FrameRenderer.prototype.renderItemBoxDestruction = function (frame, cx, cy, t, destructionTimer, startTime) {
        var totalDuration = 0.4;
        var progress = 1 - (destructionTimer / totalDuration);
        progress = Math.max(0, Math.min(1, progress));
        var colors = [YELLOW, WHITE, LIGHTCYAN, LIGHTMAGENTA, LIGHTRED];
        var colorIndex = Math.floor((Date.now() - startTime) / 50) % colors.length;
        var explosionAttr = makeAttr(colors[colorIndex], BG_BLACK);
        var sparkAttr = makeAttr(YELLOW, BG_BLACK);
        var scale = t > 0.5 ? 3 : (t > 0.25 ? 2 : 1);
        var spread = Math.floor(progress * scale * 2) + 1;
        var chars = ['*', '+', GLYPH.LIGHT_SHADE, '.', GLYPH.MEDIUM_SHADE];
        if (progress < 0.5) {
            if (cx >= 0 && cx < this.width && cy >= 0 && cy < this.height) {
                frame.setData(cx, cy, chars[0], explosionAttr);
            }
        }
        var particleCount = Math.floor(progress * 8);
        for (var i = 0; i < particleCount; i++) {
            var angle = (i / 8) * Math.PI * 2;
            var dist = spread * (0.5 + progress * 0.5);
            var px = Math.round(cx + Math.cos(angle) * dist);
            var py = Math.round(cy + Math.sin(angle) * dist * 0.5);
            if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
                var charIdx = (i + Math.floor(progress * 3)) % chars.length;
                var attr = (i % 2 === 0) ? explosionAttr : sparkAttr;
                frame.setData(px, py, chars[charIdx], attr);
            }
        }
        if (progress > 0.3 && progress < 0.8) {
            var sparkles = [[0, 0], [-1, 0], [1, 0], [0, -1], [0, 1]];
            for (var s = 0; s < sparkles.length; s++) {
                var sx = cx + sparkles[s][0];
                var sy = cy + sparkles[s][1];
                if (sx >= 0 && sx < this.width && sy >= 0 && sy < this.height) {
                    var sparkChar = (Date.now() % 100 < 50) ? '*' : '+';
                    frame.setData(sx, sy, sparkChar, sparkAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderPlayerPickupEffect = function (frame, destructionTimer, startTime, themeAttr) {
        var totalDuration = 0.5;
        var progress = 1 - (destructionTimer / totalDuration);
        progress = Math.max(0, Math.min(1, progress));
        var cx = Math.floor(this.width / 2);
        var cy = this.height - 5;
        var colors = [WHITE, YELLOW, LIGHTCYAN, LIGHTMAGENTA, YELLOW];
        var colorIndex = Math.floor((Date.now() - startTime) / 40) % colors.length;
        var burstAttr = makeAttr(colors[colorIndex], BG_BLACK);
        var sparkAttr = makeAttr(YELLOW, BG_BLACK);
        var flashChars = ['*', '+', GLYPH.LIGHT_SHADE, '!', '?', GLYPH.MEDIUM_SHADE, GLYPH.FULL_BLOCK];
        if (progress < 0.3) {
            var flashSize = Math.floor(3 + progress * 6);
            for (var fy = -1; fy <= 1; fy++) {
                for (var fx = -flashSize; fx <= flashSize; fx++) {
                    var px = cx + fx;
                    var py = cy + fy;
                    if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
                        var dist = Math.abs(fx) + Math.abs(fy);
                        var charIdx = dist % flashChars.length;
                        var attr = (dist % 2 === 0) ? burstAttr : themeAttr;
                        frame.setData(px, py, flashChars[charIdx], attr);
                    }
                }
            }
        }
        if (progress > 0.1 && progress < 0.7) {
            var ringProgress = (progress - 0.1) / 0.5;
            var ringRadius = 2 + ringProgress * 10;
            var numParticles = 12;
            for (var i = 0; i < numParticles; i++) {
                var angle = (i / numParticles) * Math.PI * 2;
                var px = Math.round(cx + Math.cos(angle) * ringRadius);
                var py = Math.round(cy + Math.sin(angle) * ringRadius * 0.4);
                if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
                    var charIdx = (i + Math.floor(progress * 5)) % flashChars.length;
                    var attr = (i % 3 === 0) ? burstAttr : (i % 3 === 1) ? sparkAttr : themeAttr;
                    frame.setData(px, py, flashChars[charIdx], attr);
                }
            }
        }
        if (progress > 0.2) {
            var riseProgress = (progress - 0.2) / 0.8;
            var numSparkles = 8;
            for (var s = 0; s < numSparkles; s++) {
                var sx = cx + (s - numSparkles / 2) * 2;
                var sy = cy - Math.floor(riseProgress * (4 + (s % 3) * 2));
                if (sx >= 0 && sx < this.width && sy >= 0 && sy < this.height) {
                    var sparkleChar = (Date.now() % 80 < 40) ? '*' : '+';
                    var attr = (s % 2 === 0) ? burstAttr : sparkAttr;
                    frame.setData(sx, sy, sparkleChar, attr);
                }
            }
        }
        if (progress < 0.4) {
            var textY = cy - 3;
            if (progress < 0.15) {
                if (cx >= 0 && cx < this.width && textY >= 0 && textY < this.height) {
                    frame.setData(cx, textY, '!', burstAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderNPCVehicles = function (playerVehicle, vehicles) {
        var visibleNPCs = [];
        var roadLength = this._currentRoad ? this._currentRoad.totalLength : 10000;
        for (var i = 0; i < vehicles.length; i++) {
            var v = vehicles[i];
            if (!v.isNPC)
                continue;
            var rawDiff = v.trackZ - playerVehicle.trackZ;
            var relativeZ = rawDiff;
            if (rawDiff > roadLength / 2) {
                relativeZ = rawDiff - roadLength;
            }
            else if (rawDiff < -roadLength / 2) {
                relativeZ = rawDiff + roadLength;
            }
            var relativeX = v.playerX - playerVehicle.playerX;
            if (relativeZ > -10 && relativeZ < 600) {
                visibleNPCs.push({ vehicle: v, relativeZ: relativeZ, relativeX: relativeX });
            }
        }
        visibleNPCs.sort(function (a, b) { return b.relativeZ - a.relativeZ; });
        for (var j = 0; j < visibleNPCs.length; j++) {
            this.renderNPCVehicle(visibleNPCs[j].vehicle, visibleNPCs[j].relativeZ, visibleNPCs[j].relativeX);
        }
    };
    FrameRenderer.prototype.renderNPCVehicle = function (vehicle, relativeZ, relativeX) {
        var sprite = getNPCSprite(vehicle.npcType, vehicle.npcColorIndex);
        var maxViewDist = 500;
        var t;
        if (relativeZ >= 0) {
            var normalizedDist = Math.min(1, relativeZ / maxViewDist);
            t = 1 - normalizedDist;
        }
        else {
            t = 1 + Math.abs(relativeZ) / 50;
        }
        var visualHorizonY = 5;
        var roadBottom = this.height - 4;
        var screenY = Math.round(visualHorizonY + t * (roadBottom - visualHorizonY));
        var curveOffset = 0;
        if (this._currentRoad && relativeZ > 0) {
            var npcWorldZ = this._currentTrackPosition + relativeZ;
            var seg = this._currentRoad.getSegment(npcWorldZ);
            if (seg) {
                curveOffset = seg.curve * t * 15;
            }
        }
        var perspectiveScale = t * t;
        var screenX = Math.round(40 + curveOffset + relativeX * perspectiveScale * 25 - this._currentCameraX * 0.5);
        var roadHeight = roadBottom - visualHorizonY;
        var screenProgress = (screenY - visualHorizonY) / roadHeight;
        var scaleIndex;
        if (screenProgress < 0.04) {
            scaleIndex = 0;
        }
        else if (screenProgress < 0.10) {
            scaleIndex = 1;
        }
        else if (screenProgress < 0.20) {
            scaleIndex = 2;
        }
        else if (screenProgress < 0.35) {
            scaleIndex = 3;
        }
        else {
            scaleIndex = 4;
        }
        scaleIndex = Math.min(scaleIndex, sprite.variants.length - 1);
        var size = getSpriteSize(sprite, scaleIndex);
        screenX -= Math.floor(size.width / 2);
        var variant = sprite.variants[scaleIndex];
        var frame = this.frameManager.getRoadFrame();
        if (!frame)
            return;
        var isFlashing = vehicle.flashTimer > 0;
        var flashAttr = makeAttr(LIGHTRED, BG_BLACK);
        var hasLightning = vehicle.hasEffect(ItemType.LIGHTNING);
        var hasStar = vehicle.hasEffect(ItemType.STAR);
        var hasBullet = vehicle.hasEffect(ItemType.BULLET);
        if (hasLightning) {
            var lightningCycle = Math.floor(Date.now() / 150) % 2;
            flashAttr = lightningCycle === 0 ? makeAttr(CYAN, BG_BLACK) : makeAttr(LIGHTCYAN, BG_BLUE);
            isFlashing = true;
        }
        if (hasStar) {
            var starColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTMAGENTA, WHITE];
            var starIndex = Math.floor(Date.now() / 80) % starColors.length;
            flashAttr = makeAttr(starColors[starIndex], BG_BLACK);
            isFlashing = true;
        }
        if (hasBullet) {
            var bulletCycle = Math.floor(Date.now() / 100) % 2;
            flashAttr = bulletCycle === 0 ? makeAttr(WHITE, BG_BLACK) : makeAttr(YELLOW, BG_BLACK);
            isFlashing = true;
        }
        var visualHorizon = 5;
        for (var row = 0; row < variant.length; row++) {
            for (var col = 0; col < variant[row].length; col++) {
                var cell = variant[row][col];
                if (cell !== null && cell !== undefined) {
                    var drawX = screenX + col;
                    var drawY = screenY + row;
                    if (drawX >= 0 && drawX < this.width && drawY >= visualHorizon && drawY < this.height - 1) {
                        var attr = isFlashing && (Math.floor(Date.now() / 100) % 2 === 0) ? flashAttr : cell.attr;
                        frame.setData(drawX, drawY, cell.char, attr);
                    }
                }
            }
        }
    };
    FrameRenderer.prototype.endFrame = function () {
        if (this.activeTheme.name === 'glitch_circuit' && typeof GlitchState !== 'undefined') {
            this.applyGlitchEffects();
        }
        this.renderLightningBolts();
        this.cycle();
    };
    FrameRenderer.prototype.getComposer = function () {
        return this.composer;
    };
    FrameRenderer.prototype.applyGlitchEffects = function () {
        var roadFrame = this.frameManager.getRoadFrame();
        var groundFrame = this.frameManager.getGroundGridFrame();
        if (GlitchState.intensity > 0.2 && roadFrame) {
            for (var row = 0; row < this.height - this.horizonY; row++) {
                if (GlitchState.isNoiseRow(row)) {
                    var noiseAttr = makeAttr(Math.random() < 0.5 ? GREEN : LIGHTGREEN, BG_BLACK);
                    for (var x = 0; x < this.width; x++) {
                        if (Math.random() < 0.7) {
                            roadFrame.setData(x, row, GlitchState.getNoiseChar(), noiseAttr);
                        }
                    }
                }
            }
        }
        if (GlitchState.intensity > 0.4 && groundFrame) {
            var corruptRows = Math.floor(GlitchState.intensity * 3);
            for (var cr = 0; cr < corruptRows; cr++) {
                var corruptY = Math.floor(Math.random() * (this.height - this.horizonY));
                var corruptX = Math.floor(Math.random() * this.width);
                var corruptW = Math.floor(Math.random() * 20) + 5;
                var corruptedColor = GlitchState.corruptColor(GREEN, BG_BLACK);
                var corruptAttr = makeAttr(corruptedColor.fg, corruptedColor.bg);
                for (var cx = corruptX; cx < corruptX + corruptW && cx < this.width; cx++) {
                    groundFrame.setData(cx, corruptY, GlitchState.corruptChar('#'), corruptAttr);
                }
            }
        }
        if (Math.abs(GlitchState.tearOffset) > 0 && roadFrame) {
            var tearY = 5 + Math.floor(Math.random() * 8);
            var offset = GlitchState.tearOffset;
            var tearAttr = makeAttr(LIGHTCYAN, BG_BLACK);
            if (offset > 0) {
                for (var tx = 0; tx < Math.abs(offset); tx++) {
                    roadFrame.setData(tx, tearY, '>', tearAttr);
                }
            }
            else {
                for (var tx2 = this.width - Math.abs(offset); tx2 < this.width; tx2++) {
                    roadFrame.setData(tx2, tearY, '<', tearAttr);
                }
            }
        }
        if (GlitchState.intensity > 0.6 && roadFrame) {
            var numCorruptions = Math.floor(GlitchState.intensity * 10);
            for (var i = 0; i < numCorruptions; i++) {
                var rx = Math.floor(Math.random() * this.width);
                var ry = Math.floor(Math.random() * (this.height - this.horizonY));
                var glitchAttr = makeAttr([LIGHTGREEN, LIGHTCYAN, LIGHTRED, WHITE][Math.floor(Math.random() * 4)], BG_BLACK);
                roadFrame.setData(rx, ry, GlitchState.corruptChar('X'), glitchAttr);
            }
        }
        this.applySkyGlitchEffects();
    };
    FrameRenderer.prototype.applySkyGlitchEffects = function () {
        var mountainsFrame = this.frameManager.getMountainsFrame();
        var skyGridFrame = this.frameManager.getSkyGridFrame();
        switch (GlitchState.skyGlitchType) {
            case 1:
                if (skyGridFrame) {
                    var rainAttr = makeAttr(LIGHTGREEN, BG_BLACK);
                    var rainDimAttr = makeAttr(GREEN, BG_BLACK);
                    for (var d = 0; d < GlitchState.matrixRainDrops.length; d++) {
                        var drop = GlitchState.matrixRainDrops[d];
                        var dropY = Math.floor(drop.y);
                        if (dropY >= 0 && dropY < this.horizonY && drop.x >= 0 && drop.x < this.width) {
                            skyGridFrame.setData(drop.x, dropY, drop.char, rainAttr);
                            if (dropY > 0) {
                                skyGridFrame.setData(drop.x, dropY - 1, drop.char, rainDimAttr);
                            }
                        }
                    }
                }
                break;
            case 2:
                if (mountainsFrame) {
                    var binaryAttr = makeAttr(LIGHTGREEN, BG_BLACK);
                    var numBinary = 15 + Math.floor(GlitchState.intensity * 20);
                    for (var b = 0; b < numBinary; b++) {
                        var bx = Math.floor(Math.random() * this.width);
                        var by = Math.floor(Math.random() * this.horizonY);
                        var binaryChar = Math.random() < 0.5 ? '0' : '1';
                        mountainsFrame.setData(bx, by, binaryChar, binaryAttr);
                    }
                }
                break;
            case 3:
                if (skyGridFrame) {
                    var bsodBgAttr = makeAttr(WHITE, BG_BLUE);
                    var bsodTextAttr = makeAttr(WHITE, BG_BLUE);
                    var bsodHeight = 3 + Math.floor(Math.random() * 3);
                    var bsodY = Math.floor(Math.random() * (this.horizonY - bsodHeight));
                    for (var by2 = bsodY; by2 < bsodY + bsodHeight && by2 < this.horizonY; by2++) {
                        for (var bx2 = 10; bx2 < this.width - 10; bx2++) {
                            skyGridFrame.setData(bx2, by2, ' ', bsodBgAttr);
                        }
                    }
                    var errorMsgs = ['FATAL_ERROR', 'MEMORY_CORRUPT', 'REALITY.SYS', 'STACK_OVERFLOW', '0x0000DEAD'];
                    var msg = errorMsgs[Math.floor(Math.random() * errorMsgs.length)];
                    var msgX = Math.floor((this.width - msg.length) / 2);
                    for (var c = 0; c < msg.length; c++) {
                        if (msgX + c >= 0 && msgX + c < this.width) {
                            skyGridFrame.setData(msgX + c, bsodY + 1, msg[c], bsodTextAttr);
                        }
                    }
                }
                break;
            case 4:
                if (mountainsFrame) {
                    var staticColors = [LIGHTCYAN, LIGHTMAGENTA, YELLOW, WHITE];
                    var numStatic = 20 + Math.floor(GlitchState.intensity * 30);
                    for (var s = 0; s < numStatic; s++) {
                        var sx = Math.floor(Math.random() * this.width);
                        var sy = Math.floor(Math.random() * this.horizonY);
                        var staticAttr = makeAttr(staticColors[Math.floor(Math.random() * staticColors.length)], BG_BLACK);
                        var staticChars = [GLYPH.FULL_BLOCK, GLYPH.DARK_SHADE, GLYPH.MEDIUM_SHADE, '#', '%'];
                        mountainsFrame.setData(sx, sy, staticChars[Math.floor(Math.random() * staticChars.length)], staticAttr);
                    }
                }
                break;
        }
        if (GlitchState.intensity > 0.3 && mountainsFrame && Math.random() < 0.3) {
            var corruptX = Math.floor(Math.random() * this.width);
            var corruptY = Math.floor(Math.random() * this.horizonY);
            var corruptW = 3 + Math.floor(Math.random() * 8);
            var corruptH = 1 + Math.floor(Math.random() * 3);
            var corruptColors = [LIGHTGREEN, GREEN, LIGHTCYAN, CYAN];
            var corruptAttr2 = makeAttr(corruptColors[Math.floor(Math.random() * corruptColors.length)], BG_BLACK);
            for (var cy = corruptY; cy < corruptY + corruptH && cy < this.horizonY; cy++) {
                for (var cx = corruptX; cx < corruptX + corruptW && cx < this.width; cx++) {
                    if (Math.random() < 0.6) {
                        var glitchChars = ['/', '\\', '|', '-', '+', '#', '0', '1'];
                        mountainsFrame.setData(cx, cy, glitchChars[Math.floor(Math.random() * glitchChars.length)], corruptAttr2);
                    }
                }
            }
        }
        if (GlitchState.intensity > 0.5 && Math.random() < 0.2) {
            var sunFrame = this.frameManager.getSunFrame();
            if (sunFrame) {
                var flickerAttr = makeAttr(Math.random() < 0.5 ? BLACK : LIGHTGREEN, Math.random() < 0.3 ? BG_GREEN : BG_BLACK);
                for (var sf = 0; sf < 3; sf++) {
                    var sfx = Math.floor(Math.random() * 8) + 36;
                    var sfy = Math.floor(Math.random() * 4) + 2;
                    sunFrame.setData(sfx, sfy, GlitchState.getNoiseChar(), flickerAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderSun = function () {
        var sunFrame = this.frameManager.getSunFrame();
        if (!sunFrame)
            return;
        var colors = this.activeTheme.colors;
        var sunCoreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
        var sunGlowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
        var celestial = this.activeTheme.celestial;
        var sunX = Math.floor(this.width * celestial.positionX) - 3;
        var sunY = Math.floor(this.horizonY * celestial.positionY);
        var size = celestial.size;
        var coreWidth = size + 2;
        var coreHeight = Math.max(1, size);
        for (var dy = 0; dy < coreHeight; dy++) {
            for (var dx = 0; dx < coreWidth; dx++) {
                sunFrame.setData(sunX + dx, sunY + dy, GLYPH.FULL_BLOCK, sunCoreAttr);
            }
        }
        var glowChar = GLYPH.DARK_SHADE;
        for (var x = sunX - 1; x <= sunX + coreWidth; x++) {
            sunFrame.setData(x, sunY - 1, glowChar, sunGlowAttr);
        }
        for (var x = sunX - 1; x <= sunX + coreWidth; x++) {
            sunFrame.setData(x, sunY + coreHeight, glowChar, sunGlowAttr);
        }
        for (var dy = 0; dy < coreHeight; dy++) {
            sunFrame.setData(sunX - 1, sunY + dy, glowChar, sunGlowAttr);
            sunFrame.setData(sunX + coreWidth, sunY + dy, glowChar, sunGlowAttr);
        }
    };
    FrameRenderer.prototype.renderMoon = function () {
        var moonFrame = this.frameManager.getSunFrame();
        if (!moonFrame)
            return;
        var colors = this.activeTheme.colors;
        var moonCoreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
        var moonGlowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
        var moonGlowDimAttr = makeAttr(CYAN, BG_BLACK);
        var celestial = this.activeTheme.celestial;
        var moonX = Math.floor(this.width * celestial.positionX);
        var moonY = Math.max(1, Math.floor((this.horizonY - 2) * celestial.positionY));
        moonFrame.setData(moonX, moonY, ')', moonCoreAttr);
        moonFrame.setData(moonX, moonY + 1, ')', moonCoreAttr);
        moonFrame.setData(moonX + 1, moonY, ')', moonCoreAttr);
        moonFrame.setData(moonX + 1, moonY + 1, ')', moonCoreAttr);
        moonFrame.setData(moonX - 1, moonY, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX - 1, moonY + 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX + 2, moonY, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX + 2, moonY + 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX, moonY - 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX + 1, moonY - 1, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX, moonY + 2, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX + 1, moonY + 2, GLYPH.MEDIUM_SHADE, moonGlowAttr);
        moonFrame.setData(moonX - 2, moonY, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX - 2, moonY + 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX + 3, moonY, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX + 3, moonY + 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX - 1, moonY - 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX + 2, moonY - 1, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX - 1, moonY + 2, GLYPH.DARK_SHADE, moonGlowDimAttr);
        moonFrame.setData(moonX + 2, moonY + 2, GLYPH.DARK_SHADE, moonGlowDimAttr);
    };
    FrameRenderer.prototype.renderDualMoons = function () {
        var moonFrame = this.frameManager.getSunFrame();
        if (!moonFrame)
            return;
        var colors = this.activeTheme.colors;
        var moonCoreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
        var moonGlowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
        var celestial = this.activeTheme.celestial;
        var moon1X = Math.floor(this.width * celestial.positionX);
        var moon1Y = Math.max(1, Math.floor((this.horizonY - 3) * celestial.positionY));
        moonFrame.setData(moon1X, moon1Y, '(', moonCoreAttr);
        moonFrame.setData(moon1X + 1, moon1Y, ')', moonCoreAttr);
        moonFrame.setData(moon1X, moon1Y + 1, '(', moonCoreAttr);
        moonFrame.setData(moon1X + 1, moon1Y + 1, ')', moonCoreAttr);
        moonFrame.setData(moon1X - 1, moon1Y, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X + 2, moon1Y, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X - 1, moon1Y + 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X + 2, moon1Y + 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X, moon1Y - 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X + 1, moon1Y - 1, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X, moon1Y + 2, GLYPH.LIGHT_SHADE, moonGlowAttr);
        moonFrame.setData(moon1X + 1, moon1Y + 2, GLYPH.LIGHT_SHADE, moonGlowAttr);
        var moon2X = Math.floor(this.width * 0.25);
        var moon2Y = Math.max(1, Math.floor((this.horizonY - 2) * 0.2));
        var moon2Attr = makeAttr(LIGHTCYAN, BG_CYAN);
        var moon2GlowAttr = makeAttr(CYAN, BG_BLACK);
        moonFrame.setData(moon2X, moon2Y, ')', moon2Attr);
        moonFrame.setData(moon2X - 1, moon2Y, GLYPH.LIGHT_SHADE, moon2GlowAttr);
        moonFrame.setData(moon2X + 1, moon2Y, GLYPH.LIGHT_SHADE, moon2GlowAttr);
        moonFrame.setData(moon2X, moon2Y - 1, GLYPH.LIGHT_SHADE, moon2GlowAttr);
        moonFrame.setData(moon2X, moon2Y + 1, GLYPH.LIGHT_SHADE, moon2GlowAttr);
    };
    FrameRenderer.prototype.renderMonsterSilhouette = function () {
        var frame = this.frameManager.getSunFrame();
        if (!frame)
            return;
        var baseY = this.horizonY;
        var mothraX = 24;
        var godzillaX = 56;
        var mothBody = makeAttr(BROWN, BG_BLACK);
        var mothWing = makeAttr(YELLOW, BG_BLACK);
        var mothWingLight = makeAttr(WHITE, BG_BROWN);
        var mothEye = makeAttr(LIGHTCYAN, BG_CYAN);
        frame.setData(mothraX - 3, baseY - 8, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 3, baseY - 8, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX - 2, baseY - 7, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 2, baseY - 7, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX - 2, baseY - 6, GLYPH.FULL_BLOCK, mothEye);
        frame.setData(mothraX - 1, baseY - 6, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX, baseY - 6, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 1, baseY - 6, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 2, baseY - 6, GLYPH.FULL_BLOCK, mothEye);
        for (var wx = mothraX - 12; wx <= mothraX - 3; wx++) {
            frame.setData(wx, baseY - 5, GLYPH.FULL_BLOCK, mothWing);
        }
        for (var wx = mothraX - 11; wx <= mothraX - 3; wx++) {
            frame.setData(wx, baseY - 4, GLYPH.FULL_BLOCK, (wx > mothraX - 9) ? mothWingLight : mothWing);
        }
        for (var wx = mothraX - 10; wx <= mothraX - 3; wx++) {
            frame.setData(wx, baseY - 3, GLYPH.FULL_BLOCK, (wx > mothraX - 8) ? mothWingLight : mothWing);
        }
        for (var wx = mothraX + 3; wx <= mothraX + 12; wx++) {
            frame.setData(wx, baseY - 5, GLYPH.FULL_BLOCK, mothWing);
        }
        for (var wx = mothraX + 3; wx <= mothraX + 11; wx++) {
            frame.setData(wx, baseY - 4, GLYPH.FULL_BLOCK, (wx < mothraX + 9) ? mothWingLight : mothWing);
        }
        for (var wx = mothraX + 3; wx <= mothraX + 10; wx++) {
            frame.setData(wx, baseY - 3, GLYPH.FULL_BLOCK, (wx < mothraX + 8) ? mothWingLight : mothWing);
        }
        frame.setData(mothraX - 2, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX - 1, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 1, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 2, baseY - 5, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX - 1, baseY - 4, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX, baseY - 4, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 1, baseY - 4, GLYPH.FULL_BLOCK, mothBody);
        for (var wx = mothraX - 8; wx <= mothraX - 2; wx++) {
            frame.setData(wx, baseY - 2, GLYPH.FULL_BLOCK, mothWing);
        }
        for (var wx = mothraX - 6; wx <= mothraX - 2; wx++) {
            frame.setData(wx, baseY - 1, GLYPH.FULL_BLOCK, mothWing);
        }
        for (var wx = mothraX + 2; wx <= mothraX + 8; wx++) {
            frame.setData(wx, baseY - 2, GLYPH.FULL_BLOCK, mothWing);
        }
        for (var wx = mothraX + 2; wx <= mothraX + 6; wx++) {
            frame.setData(wx, baseY - 1, GLYPH.FULL_BLOCK, mothWing);
        }
        frame.setData(mothraX - 1, baseY - 3, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX, baseY - 3, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX + 1, baseY - 3, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX, baseY - 2, GLYPH.FULL_BLOCK, mothBody);
        frame.setData(mothraX, baseY - 1, GLYPH.FULL_BLOCK, mothBody);
        var godzBody = makeAttr(GREEN, BG_BLACK);
        var godzLight = makeAttr(LIGHTGREEN, BG_BLACK);
        var godzEye = makeAttr(LIGHTRED, BG_RED);
        var godzSpine = makeAttr(LIGHTCYAN, BG_CYAN);
        var godzBreath = makeAttr(LIGHTCYAN, BG_BLACK);
        frame.setData(godzillaX + 1, baseY - 10, GLYPH.FULL_BLOCK, godzSpine);
        frame.setData(godzillaX + 2, baseY - 9, GLYPH.FULL_BLOCK, godzSpine);
        frame.setData(godzillaX + 3, baseY - 10, GLYPH.FULL_BLOCK, godzSpine);
        frame.setData(godzillaX + 4, baseY - 9, GLYPH.FULL_BLOCK, godzSpine);
        frame.setData(godzillaX - 3, baseY - 9, GLYPH.UPPER_HALF, godzLight);
        frame.setData(godzillaX - 2, baseY - 9, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 1, baseY - 9, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX, baseY - 9, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX + 1, baseY - 9, GLYPH.UPPER_HALF, godzBody);
        frame.setData(godzillaX - 4, baseY - 8, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 3, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 2, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 1, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX, baseY - 8, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX + 1, baseY - 8, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 5, baseY - 7, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 4, baseY - 7, GLYPH.FULL_BLOCK, godzEye);
        frame.setData(godzillaX - 3, baseY - 7, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 2, baseY - 7, GLYPH.FULL_BLOCK, godzEye);
        frame.setData(godzillaX - 1, baseY - 7, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX, baseY - 7, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 1, baseY - 7, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 7, baseY - 6, GLYPH.LEFT_HALF, godzBody);
        frame.setData(godzillaX - 6, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 5, baseY - 6, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 4, baseY - 6, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 3, baseY - 6, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 2, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 1, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX, baseY - 6, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 8, baseY - 6, GLYPH.FULL_BLOCK, godzBreath);
        frame.setData(godzillaX - 9, baseY - 6, GLYPH.FULL_BLOCK, godzBreath);
        frame.setData(godzillaX - 10, baseY - 6, GLYPH.FULL_BLOCK, godzSpine);
        frame.setData(godzillaX - 11, baseY - 6, GLYPH.MEDIUM_SHADE, godzBreath);
        frame.setData(godzillaX - 10, baseY - 5, GLYPH.LIGHT_SHADE, godzBreath);
        frame.setData(godzillaX - 10, baseY - 7, GLYPH.LIGHT_SHADE, godzBreath);
        frame.setData(godzillaX - 3, baseY - 5, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 2, baseY - 5, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 1, baseY - 5, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX, baseY - 5, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX + 1, baseY - 5, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 2, baseY - 5, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 4, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 3, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 2, baseY - 4, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 1, baseY - 4, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX, baseY - 4, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX + 1, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 2, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 3, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 6, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 5, baseY - 4, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 3, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 2, baseY - 3, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 1, baseY - 3, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX, baseY - 3, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX + 1, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 2, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 3, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 2, baseY - 2, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX - 3, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX - 2, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 1, baseY - 2, GLYPH.FULL_BLOCK, godzLight);
        frame.setData(godzillaX + 2, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 1, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 2, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 3, baseY - 3, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 4, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 5, baseY - 2, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 6, baseY - 1, GLYPH.FULL_BLOCK, godzBody);
        frame.setData(godzillaX + 7, baseY - 1, GLYPH.RIGHT_HALF, godzBody);
    };
    FrameRenderer.prototype.renderMermaid = function () {
        var frame = this.frameManager.getSunFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var coreAttr = makeAttr(colors.celestialCore.fg, colors.celestialCore.bg);
        var glowAttr = makeAttr(colors.celestialGlow.fg, colors.celestialGlow.bg);
        var hairAttr = makeAttr(LIGHTMAGENTA, BG_BLUE);
        var tailAttr = makeAttr(LIGHTCYAN, BG_BLUE);
        var tailGlowAttr = makeAttr(CYAN, BG_BLUE);
        var posX = Math.floor(this.width * (this.activeTheme.celestial.positionX || 0.7));
        var posY = Math.floor(this.horizonY * (this.activeTheme.celestial.positionY || 0.4));
        frame.setData(posX - 5, posY - 2, '~', hairAttr);
        frame.setData(posX - 4, posY - 2, '~', hairAttr);
        frame.setData(posX - 3, posY - 1, '~', hairAttr);
        frame.setData(posX - 4, posY - 1, '~', hairAttr);
        frame.setData(posX - 5, posY, '~', hairAttr);
        frame.setData(posX - 4, posY, '~', hairAttr);
        frame.setData(posX - 3, posY, '~', hairAttr);
        frame.setData(posX - 2, posY - 1, '(', coreAttr);
        frame.setData(posX - 1, posY - 1, GLYPH.FULL_BLOCK, coreAttr);
        frame.setData(posX, posY - 1, ')', coreAttr);
        frame.setData(posX - 1, posY, GLYPH.FULL_BLOCK, coreAttr);
        frame.setData(posX, posY, GLYPH.FULL_BLOCK, coreAttr);
        frame.setData(posX + 1, posY, '\\', coreAttr);
        frame.setData(posX, posY + 1, GLYPH.FULL_BLOCK, glowAttr);
        frame.setData(posX + 1, posY + 1, GLYPH.FULL_BLOCK, tailAttr);
        frame.setData(posX + 2, posY + 1, GLYPH.FULL_BLOCK, tailAttr);
        frame.setData(posX + 3, posY + 1, GLYPH.FULL_BLOCK, tailAttr);
        frame.setData(posX + 4, posY, GLYPH.FULL_BLOCK, tailAttr);
        frame.setData(posX + 5, posY, GLYPH.FULL_BLOCK, tailAttr);
        frame.setData(posX + 6, posY - 1, GLYPH.FULL_BLOCK, tailAttr);
        frame.setData(posX + 7, posY - 2, '/', tailGlowAttr);
        frame.setData(posX + 7, posY - 1, GLYPH.FULL_BLOCK, tailGlowAttr);
        frame.setData(posX + 7, posY, '\\', tailGlowAttr);
        frame.setData(posX + 8, posY - 2, '/', tailGlowAttr);
        frame.setData(posX + 8, posY, '\\', tailGlowAttr);
        frame.setData(posX - 2, posY - 2, GLYPH.LIGHT_SHADE, glowAttr);
        frame.setData(posX + 1, posY - 2, GLYPH.LIGHT_SHADE, glowAttr);
        frame.setData(posX - 2, posY + 1, GLYPH.LIGHT_SHADE, glowAttr);
        frame.setData(posX + 2, posY + 2, GLYPH.LIGHT_SHADE, glowAttr);
    };
    FrameRenderer.prototype.renderUnderwaterBackground = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var rockAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var rockLightAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var kelpAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var coralAttr = makeAttr(LIGHTMAGENTA, BG_BLUE);
        var coralYellowAttr = makeAttr(YELLOW, BG_BLUE);
        for (var y = 0; y < this.horizonY; y++) {
            var wallWidth = 6 - Math.floor(y * 0.5);
            for (var x = 0; x < wallWidth && x < 10; x++) {
                var char = (x === wallWidth - 1) ? GLYPH.RIGHT_HALF : GLYPH.FULL_BLOCK;
                var attr = ((x + y) % 3 === 0) ? rockLightAttr : rockAttr;
                frame.setData(x, y, char, attr);
            }
        }
        for (var y = 0; y < this.horizonY; y++) {
            var wallWidth = 5 - Math.floor(y * 0.4);
            for (var x = 0; x < wallWidth && x < 8; x++) {
                var rx = this.width - 1 - x;
                var char = (x === wallWidth - 1) ? GLYPH.LEFT_HALF : GLYPH.FULL_BLOCK;
                var attr = ((x + y) % 3 === 0) ? rockLightAttr : rockAttr;
                frame.setData(rx, y, char, attr);
            }
        }
        var kelpPositions = [12, 18, 25, 55, 62, 68];
        for (var i = 0; i < kelpPositions.length; i++) {
            var kx = kelpPositions[i];
            var kelpHeight = 3 + (i % 3);
            for (var ky = 0; ky < kelpHeight; ky++) {
                var y = this.horizonY - 1 - ky;
                if (y >= 0) {
                    var kchar = (ky % 2 === 0) ? ')' : '(';
                    frame.setData(kx, y, kchar, kelpAttr);
                    if (i % 2 === 0 && kx + 1 < this.width) {
                        frame.setData(kx + 1, y, (ky % 2 === 0) ? '(' : ')', kelpAttr);
                    }
                }
            }
        }
        var coralPositions = [8, 22, 35, 48, 58, 72];
        for (var i = 0; i < coralPositions.length; i++) {
            var cx = coralPositions[i];
            var y = this.horizonY - 1;
            var attr = (i % 2 === 0) ? coralAttr : coralYellowAttr;
            frame.setData(cx, y, '*', attr);
            frame.setData(cx + 1, y, 'Y', attr);
            frame.setData(cx + 2, y, '*', attr);
            if (y - 1 >= 0) {
                frame.setData(cx + 1, y - 1, '^', attr);
            }
        }
        var bubblePositions = [[30, 2], [45, 3], [50, 1], [15, 4], [65, 2]];
        for (var i = 0; i < bubblePositions.length; i++) {
            var bx = bubblePositions[i][0];
            var by = bubblePositions[i][1];
            frame.setData(bx, by, 'o', makeAttr(WHITE, BG_BLUE));
        }
    };
    FrameRenderer.prototype.renderAquariumBackground = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var glassFrameAttr = makeAttr(LIGHTGRAY, BG_BLACK);
        var glassHighlightAttr = makeAttr(WHITE, BG_BLACK);
        var glassDarkAttr = makeAttr(DARKGRAY, BG_BLACK);
        var waterAttr = makeAttr(CYAN, BG_BLUE);
        var waterDeepAttr = makeAttr(BLUE, BG_BLUE);
        var coralAttr = makeAttr(LIGHTRED, BG_BLUE);
        var coralYellowAttr = makeAttr(YELLOW, BG_BLUE);
        var plantAttr = makeAttr(LIGHTGREEN, BG_BLUE);
        var sandAttr = makeAttr(YELLOW, BG_BLUE);
        var rockAttr = makeAttr(DARKGRAY, BG_BLUE);
        var mermaidSkinAttr = makeAttr(LIGHTMAGENTA, BG_BLUE);
        var mermaidHairAttr = makeAttr(LIGHTRED, BG_BLUE);
        var mermaidTailAttr = makeAttr(LIGHTCYAN, BG_BLUE);
        var mermaidTailGlowAttr = makeAttr(CYAN, BG_BLUE);
        for (var y = 0; y < this.horizonY; y++) {
            frame.setData(0, y, GLYPH.FULL_BLOCK, glassDarkAttr);
            frame.setData(1, y, GLYPH.FULL_BLOCK, glassFrameAttr);
            frame.setData(2, y, GLYPH.LIGHT_SHADE, glassHighlightAttr);
            for (var x = 3; x < 7; x++) {
                frame.setData(x, y, GLYPH.FULL_BLOCK, glassFrameAttr);
            }
            frame.setData(7, y, GLYPH.RIGHT_HALF, glassFrameAttr);
        }
        for (var y = 0; y < this.horizonY; y++) {
            frame.setData(72, y, GLYPH.LEFT_HALF, glassFrameAttr);
            for (var x = 73; x < 77; x++) {
                frame.setData(x, y, GLYPH.FULL_BLOCK, glassFrameAttr);
            }
            frame.setData(77, y, GLYPH.LIGHT_SHADE, glassHighlightAttr);
            frame.setData(78, y, GLYPH.FULL_BLOCK, glassFrameAttr);
            frame.setData(79, y, GLYPH.FULL_BLOCK, glassDarkAttr);
        }
        for (var y = 0; y < this.horizonY; y++) {
            for (var x = 8; x < 72; x++) {
                var attr = (y < 4) ? waterAttr : waterDeepAttr;
                frame.setData(x, y, ' ', attr);
            }
        }
        var bottomY = this.horizonY - 1;
        for (var x = 8; x < 72; x++) {
            var sandChar = (x % 3 === 0) ? GLYPH.LOWER_HALF : GLYPH.LIGHT_SHADE;
            frame.setData(x, bottomY, sandChar, sandAttr);
        }
        var rockPositions = [12, 25, 45, 62];
        for (var i = 0; i < rockPositions.length; i++) {
            var rx = rockPositions[i];
            frame.setData(rx, bottomY, GLYPH.FULL_BLOCK, rockAttr);
            frame.setData(rx + 1, bottomY, GLYPH.FULL_BLOCK, rockAttr);
            if (bottomY - 1 >= 0) {
                frame.setData(rx, bottomY - 1, GLYPH.LOWER_HALF, rockAttr);
            }
        }
        var coralPositions = [15, 32, 50, 65];
        for (var i = 0; i < coralPositions.length; i++) {
            var cx = coralPositions[i];
            var attr = (i % 2 === 0) ? coralAttr : coralYellowAttr;
            frame.setData(cx, bottomY, 'Y', attr);
            frame.setData(cx + 1, bottomY, '*', attr);
            frame.setData(cx + 2, bottomY, 'Y', attr);
            if (bottomY - 1 >= 0) {
                frame.setData(cx + 1, bottomY - 1, '^', attr);
            }
            if (bottomY - 2 >= 0) {
                frame.setData(cx + 1, bottomY - 2, '*', attr);
            }
        }
        var kelpLeft = [10, 14, 18];
        var kelpRight = [58, 62, 68];
        var kelpPositions = kelpLeft.concat(kelpRight);
        for (var i = 0; i < kelpPositions.length; i++) {
            var kx = kelpPositions[i];
            for (var ky = 0; ky < 4; ky++) {
                var y = bottomY - 1 - ky;
                if (y >= 0) {
                    var kchar = (ky % 2 === 0) ? ')' : '(';
                    frame.setData(kx, y, kchar, plantAttr);
                }
            }
        }
        var mermaidX = 38;
        var mermaidY = 5;
        frame.setData(mermaidX - 6, mermaidY - 1, '~', mermaidHairAttr);
        frame.setData(mermaidX - 5, mermaidY - 1, '~', mermaidHairAttr);
        frame.setData(mermaidX - 4, mermaidY - 1, '~', mermaidHairAttr);
        frame.setData(mermaidX - 5, mermaidY, '~', mermaidHairAttr);
        frame.setData(mermaidX - 4, mermaidY, '~', mermaidHairAttr);
        frame.setData(mermaidX - 3, mermaidY, '~', mermaidHairAttr);
        frame.setData(mermaidX - 4, mermaidY + 1, '~', mermaidHairAttr);
        frame.setData(mermaidX - 3, mermaidY + 1, '~', mermaidHairAttr);
        frame.setData(mermaidX - 2, mermaidY, '(', mermaidSkinAttr);
        frame.setData(mermaidX - 1, mermaidY, GLYPH.FULL_BLOCK, mermaidSkinAttr);
        frame.setData(mermaidX, mermaidY, ')', mermaidSkinAttr);
        frame.setData(mermaidX - 1, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidSkinAttr);
        frame.setData(mermaidX, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidSkinAttr);
        frame.setData(mermaidX + 1, mermaidY + 1, '\\', mermaidSkinAttr);
        frame.setData(mermaidX, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 1, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 2, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 3, mermaidY + 2, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 4, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 5, mermaidY + 1, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 6, mermaidY, GLYPH.FULL_BLOCK, mermaidTailAttr);
        frame.setData(mermaidX + 7, mermaidY - 1, '/', mermaidTailGlowAttr);
        frame.setData(mermaidX + 7, mermaidY, GLYPH.FULL_BLOCK, mermaidTailGlowAttr);
        frame.setData(mermaidX + 7, mermaidY + 1, '\\', mermaidTailGlowAttr);
        frame.setData(mermaidX + 8, mermaidY - 1, '/', mermaidTailGlowAttr);
        frame.setData(mermaidX + 8, mermaidY + 1, '\\', mermaidTailGlowAttr);
        var fishAttr = makeAttr(YELLOW, BG_BLUE);
        frame.setData(mermaidX - 8, mermaidY + 3, '<', fishAttr);
        frame.setData(mermaidX - 7, mermaidY + 3, '>', fishAttr);
        frame.setData(mermaidX + 12, mermaidY - 2, '<', makeAttr(LIGHTRED, BG_BLUE));
        frame.setData(mermaidX + 13, mermaidY - 2, '>', makeAttr(LIGHTRED, BG_BLUE));
        frame.setData(mermaidX + 2, mermaidY - 2, 'o', makeAttr(WHITE, BG_BLUE));
        frame.setData(mermaidX + 3, mermaidY - 3, '.', makeAttr(WHITE, BG_BLUE));
        frame.setData(20, 3, 'o', makeAttr(WHITE, BG_BLUE));
        frame.setData(55, 2, 'o', makeAttr(WHITE, BG_BLUE));
    };
    FrameRenderer.prototype.renderANSITunnelStatic = function () {
        var mtnsFrame = this.frameManager.getMountainsFrame();
        if (mtnsFrame) {
            mtnsFrame.clear();
        }
        var sunFrame = this.frameManager.getSunFrame();
        if (sunFrame) {
            sunFrame.clear();
        }
    };
    FrameRenderer.prototype.renderMountains = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var mountainAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var highlightAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var mountains = [
            { x: 5, height: 4, width: 12 },
            { x: 20, height: 6, width: 16 },
            { x: 42, height: 5, width: 14 },
            { x: 60, height: 4, width: 10 },
            { x: 72, height: 3, width: 8 }
        ];
        for (var i = 0; i < mountains.length; i++) {
            this.drawMountainToFrame(frame, mountains[i].x, this.horizonY - 1, mountains[i].height, mountains[i].width, mountainAttr, highlightAttr);
        }
    };
    FrameRenderer.prototype.renderSkyscrapers = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var wallAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var windowAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var antennaAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var buildings = [
            { x: 2, height: 5, width: 6 },
            { x: 10, height: 7, width: 4 },
            { x: 16, height: 4, width: 5 },
            { x: 23, height: 6, width: 7 },
            { x: 32, height: 8, width: 5 },
            { x: 39, height: 5, width: 6 },
            { x: 47, height: 7, width: 4 },
            { x: 53, height: 4, width: 5 },
            { x: 60, height: 6, width: 6 },
            { x: 68, height: 5, width: 5 },
            { x: 75, height: 4, width: 4 }
        ];
        for (var i = 0; i < buildings.length; i++) {
            this.drawBuildingToFrame(frame, buildings[i].x, this.horizonY - 1, buildings[i].height, buildings[i].width, wallAttr, windowAttr, antennaAttr);
        }
    };
    FrameRenderer.prototype.drawBuildingToFrame = function (frame, baseX, baseY, height, width, wallAttr, windowAttr, antennaAttr) {
        for (var h = 0; h < height; h++) {
            var y = baseY - h;
            if (y < 0)
                continue;
            for (var dx = 0; dx < width; dx++) {
                var x = baseX + dx;
                if (x >= 0 && x < this.width) {
                    var isWindow = (dx > 0 && dx < width - 1 && h > 0 && h < height - 1);
                    var isLit = ((dx + h) % 3 === 0);
                    if (isWindow && isLit) {
                        frame.setData(x, y, '.', windowAttr);
                    }
                    else {
                        frame.setData(x, y, GLYPH.FULL_BLOCK, wallAttr);
                    }
                }
            }
        }
        if (width >= 5 && height >= 5) {
            var antennaX = baseX + Math.floor(width / 2);
            var antennaY = baseY - height;
            if (antennaY >= 0) {
                frame.setData(antennaX, antennaY, '|', antennaAttr);
                frame.setData(antennaX, antennaY - 1, '*', antennaAttr);
            }
        }
    };
    FrameRenderer.prototype.renderOceanIslands = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var islandAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var highlightAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var islands = [
            { x: 15, height: 2, width: 8 },
            { x: 55, height: 3, width: 12 },
        ];
        for (var i = 0; i < islands.length; i++) {
            this.drawMountainToFrame(frame, islands[i].x, this.horizonY - 1, islands[i].height, islands[i].width, islandAttr, highlightAttr);
        }
    };
    FrameRenderer.prototype.renderForestTreeline = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var treeAttr = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var topAttr = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var trunkAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var treeChars = ['^', GLYPH.TRIANGLE_UP, 'A', '*'];
        for (var x = 0; x < this.width; x++) {
            var hash = (x * 17 + 5) % 13;
            var treeHeight = 2 + (hash % 4);
            var treeType = hash % treeChars.length;
            for (var h = 0; h < treeHeight; h++) {
                var y = this.horizonY - 1 - h;
                if (y < 0)
                    continue;
                if (h === treeHeight - 1) {
                    frame.setData(x, y, treeChars[treeType], topAttr);
                }
                else if (h === 0 && treeHeight >= 3) {
                    frame.setData(x, y, '|', trunkAttr);
                }
                else {
                    var bodyChar = (h === treeHeight - 2) ? GLYPH.TRIANGLE_UP : GLYPH.MEDIUM_SHADE;
                    frame.setData(x, y, bodyChar, treeAttr);
                }
            }
            if ((x * 7) % 11 === 0) {
                x++;
            }
        }
    };
    FrameRenderer.prototype.renderOceanWaves = function (trackPosition) {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var waveAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var foamAttr = makeAttr(WHITE, BG_BLACK);
        var wavePhase = Math.floor(trackPosition / 8) % 4;
        for (var row = 0; row < 2; row++) {
            var y = this.horizonY - row;
            if (y < 0)
                continue;
            for (var x = 0; x < this.width; x++) {
                if ((x >= 15 && x <= 23) || (x >= 55 && x <= 67)) {
                    if (row === 0)
                        continue;
                }
                var wave1 = Math.sin((x + wavePhase * 3) * 0.3);
                var wave2 = Math.sin((x - wavePhase * 2 + 10) * 0.5);
                var combined = wave1 + wave2 * 0.5;
                var char;
                var attr;
                if (combined > 0.8) {
                    char = (wavePhase % 2 === 0) ? '~' : '^';
                    attr = foamAttr;
                }
                else if (combined > 0.2) {
                    char = '~';
                    attr = waveAttr;
                }
                else if (combined > -0.3) {
                    char = (row === 0) ? '-' : '~';
                    attr = waveAttr;
                }
                else {
                    char = '_';
                    attr = waveAttr;
                }
                var sparkle = ((x * 17 + wavePhase * 31) % 23) === 0;
                if (sparkle && row === 0) {
                    char = '*';
                    attr = foamAttr;
                }
                frame.setData(x, y, char, attr);
            }
        }
    };
    FrameRenderer.prototype.renderJungleCanopy = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var leafDark = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var leafLight = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var vineAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var canopyHeight = Math.min(this.horizonY - 1, 7);
        for (var x = 0; x < this.width; x++) {
            var hash = (x * 23 + 7) % 17;
            var topY = this.horizonY - canopyHeight + (hash % 3);
            for (var y = topY; y < this.horizonY; y++) {
                var depth = y - topY;
                var leafChar;
                if (depth === 0) {
                    leafChar = ((x + hash) % 3 === 0) ? '@' : ((x % 2 === 0) ? 'O' : 'o');
                    frame.setData(x, y, leafChar, leafLight);
                }
                else if (depth < 2) {
                    leafChar = ((x + depth) % 2 === 0) ? GLYPH.MEDIUM_SHADE : GLYPH.DARK_SHADE;
                    frame.setData(x, y, leafChar, leafDark);
                }
                else {
                    if ((x * 13 + depth * 7) % 5 !== 0) {
                        leafChar = GLYPH.LIGHT_SHADE;
                        frame.setData(x, y, leafChar, leafDark);
                    }
                }
            }
        }
        for (var vineX = 3; vineX < this.width - 3; vineX += 7 + ((vineX * 11) % 5)) {
            var vineLength = 2 + ((vineX * 7) % 4);
            var vineStartY = this.horizonY - canopyHeight + 2;
            for (var vy = 0; vy < vineLength && vineStartY + vy < this.horizonY; vy++) {
                var vineChar = (vy === vineLength - 1) ? ')' : '|';
                frame.setData(vineX, vineStartY + vy, vineChar, vineAttr);
            }
        }
        for (var fx = 5; fx < this.width - 5; fx += 11 + ((fx * 3) % 7)) {
            var fy = this.horizonY - canopyHeight + 1 + ((fx * 5) % 2);
            if (fy < this.horizonY - 1) {
                frame.setData(fx, fy, '*', makeAttr(LIGHTMAGENTA, BG_BLACK));
            }
        }
    };
    FrameRenderer.prototype.renderCandyHills = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var hill1 = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var hill2 = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var sparkle = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var hills = [
            { centerX: 12, height: 4, width: 20, color: hill1 },
            { centerX: 35, height: 5, width: 24, color: hill2 },
            { centerX: 58, height: 3, width: 18, color: hill1 },
            { centerX: 75, height: 4, width: 16, color: hill2 }
        ];
        for (var i = 0; i < hills.length; i++) {
            var hill = hills[i];
            for (var dx = -hill.width / 2; dx <= hill.width / 2; dx++) {
                var x = Math.floor(hill.centerX + dx);
                if (x < 0 || x >= this.width)
                    continue;
                var t = dx / (hill.width / 2);
                var hillHeight = Math.round(hill.height * Math.cos(t * Math.PI / 2));
                for (var h = 0; h < hillHeight; h++) {
                    var y = this.horizonY - 1 - h;
                    if (y < 0)
                        continue;
                    var char;
                    if (h === hillHeight - 1) {
                        char = (Math.abs(dx) < hill.width / 4) ? '@' : 'o';
                    }
                    else {
                        char = ((x + h) % 3 === 0) ? '~' : GLYPH.MEDIUM_SHADE;
                    }
                    frame.setData(x, y, char, hill.color);
                }
            }
        }
        for (var sx = 2; sx < this.width - 2; sx += 5 + ((sx * 7) % 4)) {
            var sy = this.horizonY - 2 - ((sx * 3) % 3);
            if (sy > 0) {
                frame.setData(sx, sy, '*', sparkle);
            }
        }
    };
    FrameRenderer.prototype.renderNebula = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var nebula1 = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var nebula2 = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var starAttr = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var clouds = [
            { x: 5, y: this.horizonY - 5, w: 15, h: 4 },
            { x: 25, y: this.horizonY - 7, w: 20, h: 5 },
            { x: 50, y: this.horizonY - 4, w: 12, h: 3 },
            { x: 65, y: this.horizonY - 6, w: 14, h: 4 }
        ];
        for (var c = 0; c < clouds.length; c++) {
            var cloud = clouds[c];
            var cloudAttr = (c % 2 === 0) ? nebula1 : nebula2;
            for (var cy = 0; cy < cloud.h; cy++) {
                for (var cx = 0; cx < cloud.w; cx++) {
                    var px = cloud.x + cx;
                    var py = cloud.y + cy;
                    if (px < 0 || px >= this.width || py < 0 || py >= this.horizonY)
                        continue;
                    var distFromCenter = Math.abs(cx - cloud.w / 2) / (cloud.w / 2) +
                        Math.abs(cy - cloud.h / 2) / (cloud.h / 2);
                    var density = 1 - distFromCenter * 0.6;
                    var hash = (px * 17 + py * 31) % 100;
                    if (hash < density * 80) {
                        var char;
                        if (density > 0.7) {
                            char = GLYPH.MEDIUM_SHADE;
                        }
                        else if (density > 0.4) {
                            char = GLYPH.LIGHT_SHADE;
                        }
                        else {
                            char = '.';
                        }
                        frame.setData(px, py, char, cloudAttr);
                    }
                }
            }
        }
        for (var sx = 1; sx < this.width - 1; sx += 8 + ((sx * 5) % 6)) {
            var sy = ((sx * 13) % (this.horizonY - 2)) + 1;
            if (sy > 0 && sy < this.horizonY - 1) {
                frame.setData(sx, sy, '*', starAttr);
            }
        }
    };
    FrameRenderer.prototype.renderCastleFortress = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var stone = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var window = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var torch = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var sections = [
            { type: 'tower', x: 5, h: 7, w: 5 },
            { type: 'wall', x: 10, h: 4, w: 12 },
            { type: 'tower', x: 22, h: 8, w: 6 },
            { type: 'wall', x: 28, h: 4, w: 15 },
            { type: 'tower', x: 43, h: 6, w: 5 },
            { type: 'gate', x: 48, h: 5, w: 8 },
            { type: 'tower', x: 56, h: 7, w: 5 },
            { type: 'wall', x: 61, h: 4, w: 10 },
            { type: 'tower', x: 71, h: 5, w: 5 }
        ];
        for (var i = 0; i < sections.length; i++) {
            var sec = sections[i];
            var baseY = this.horizonY - 1;
            if (sec.type === 'tower') {
                for (var h = 0; h < sec.h; h++) {
                    var y = baseY - h;
                    if (y < 0)
                        continue;
                    for (var dx = 0; dx < sec.w; dx++) {
                        var x = sec.x + dx;
                        if (x >= this.width)
                            continue;
                        if (h === sec.h - 1) {
                            frame.setData(x, y, (dx % 2 === 0) ? GLYPH.FULL_BLOCK : ' ', stone);
                        }
                        else if (h === sec.h - 2 && dx === Math.floor(sec.w / 2)) {
                            frame.setData(x, y, '.', window);
                        }
                        else if (h === 1 && dx === Math.floor(sec.w / 2)) {
                            frame.setData(x, y, '*', torch);
                        }
                        else {
                            frame.setData(x, y, GLYPH.FULL_BLOCK, stone);
                        }
                    }
                }
                var roofX = sec.x + Math.floor(sec.w / 2);
                frame.setData(roofX, baseY - sec.h, GLYPH.TRIANGLE_UP, stone);
            }
            else if (sec.type === 'wall') {
                for (var h = 0; h < sec.h; h++) {
                    var y = baseY - h;
                    if (y < 0)
                        continue;
                    for (var dx = 0; dx < sec.w; dx++) {
                        var x = sec.x + dx;
                        if (x >= this.width)
                            continue;
                        if (h === sec.h - 1) {
                            frame.setData(x, y, (dx % 3 === 0) ? GLYPH.FULL_BLOCK : ' ', stone);
                        }
                        else {
                            frame.setData(x, y, GLYPH.MEDIUM_SHADE, stone);
                        }
                    }
                }
            }
            else if (sec.type === 'gate') {
                for (var h = 0; h < sec.h; h++) {
                    var y = baseY - h;
                    if (y < 0)
                        continue;
                    for (var dx = 0; dx < sec.w; dx++) {
                        var x = sec.x + dx;
                        if (x >= this.width)
                            continue;
                        var inArch = (dx > 1 && dx < sec.w - 2 && h < sec.h - 2);
                        if (inArch) {
                            frame.setData(x, y, (h % 2 === 0) ? '-' : '#', stone);
                        }
                        else {
                            frame.setData(x, y, GLYPH.FULL_BLOCK, stone);
                        }
                    }
                }
            }
        }
    };
    FrameRenderer.prototype.renderVolcanic = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var rock = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var lava = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var smoke = makeAttr(colors.sceneryTertiary.fg, colors.sceneryTertiary.bg);
        var peaks = [
            { x: 8, h: 5, w: 14, hasLava: false },
            { x: 28, h: 8, w: 20, hasLava: true },
            { x: 55, h: 6, w: 16, hasLava: true },
            { x: 72, h: 4, w: 10, hasLava: false }
        ];
        for (var i = 0; i < peaks.length; i++) {
            var peak = peaks[i];
            var peakX = peak.x + Math.floor(peak.w / 2);
            for (var h = 0; h < peak.h; h++) {
                var y = this.horizonY - 1 - h;
                if (y < 0)
                    continue;
                var rowWidth = Math.floor((peak.h - h) * peak.w / peak.h / 2);
                for (var dx = -rowWidth; dx <= rowWidth; dx++) {
                    var x = peakX + dx;
                    if (x < 0 || x >= this.width)
                        continue;
                    var edgeDist = Math.abs(dx) - rowWidth + 1;
                    if (edgeDist >= 0 && ((x * 7 + h * 3) % 3 === 0)) {
                        continue;
                    }
                    var char;
                    if (h === peak.h - 1 && peak.hasLava) {
                        char = '^';
                        frame.setData(x, y, char, lava);
                    }
                    else if (dx < 0) {
                        char = '/';
                        frame.setData(x, y, char, rock);
                    }
                    else if (dx > 0) {
                        char = '\\';
                        frame.setData(x, y, char, rock);
                    }
                    else {
                        char = GLYPH.BOX_V;
                        frame.setData(x, y, char, rock);
                    }
                }
            }
            if (peak.hasLava) {
                var craterY = this.horizonY - peak.h;
                if (craterY >= 0) {
                    frame.setData(peakX - 1, craterY, '*', lava);
                    frame.setData(peakX, craterY, GLYPH.FULL_BLOCK, lava);
                    frame.setData(peakX + 1, craterY, '*', lava);
                }
                for (var sy = 1; sy <= 2; sy++) {
                    var smokeY = craterY - sy;
                    if (smokeY >= 0) {
                        frame.setData(peakX + (sy % 2), smokeY, '~', smoke);
                        frame.setData(peakX - (sy % 2), smokeY, '~', smoke);
                    }
                }
            }
        }
        for (var lx = 0; lx < this.width; lx += 12 + ((lx * 5) % 7)) {
            var ly = this.horizonY - 1;
            frame.setData(lx, ly, '~', lava);
            if (lx + 1 < this.width)
                frame.setData(lx + 1, ly, '*', lava);
        }
    };
    FrameRenderer.prototype.renderPyramids = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var stone = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var gold = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var pyramids = [
            { x: 15, h: 6 },
            { x: 40, h: 8 },
            { x: 62, h: 5 }
        ];
        for (var i = 0; i < pyramids.length; i++) {
            var pyr = pyramids[i];
            var baseY = this.horizonY - 1;
            for (var h = 0; h < pyr.h; h++) {
                var y = baseY - h;
                if (y < 0)
                    continue;
                var halfWidth = pyr.h - h - 1;
                for (var dx = -halfWidth; dx <= halfWidth; dx++) {
                    var x = pyr.x + dx;
                    if (x < 0 || x >= this.width)
                        continue;
                    var char;
                    var attr = stone;
                    if (h === pyr.h - 1) {
                        char = GLYPH.TRIANGLE_UP;
                        attr = gold;
                    }
                    else if (dx === -halfWidth) {
                        char = '/';
                    }
                    else if (dx === halfWidth) {
                        char = '\\';
                    }
                    else {
                        char = ((h + dx) % 4 === 0) ? '-' : GLYPH.LIGHT_SHADE;
                    }
                    frame.setData(x, y, char, attr);
                }
            }
        }
        var sphinxX = 2;
        var sphinxY = this.horizonY - 1;
        frame.setData(sphinxX, sphinxY, GLYPH.MEDIUM_SHADE, stone);
        frame.setData(sphinxX + 1, sphinxY, GLYPH.MEDIUM_SHADE, stone);
        frame.setData(sphinxX + 2, sphinxY, GLYPH.MEDIUM_SHADE, stone);
        frame.setData(sphinxX + 3, sphinxY, '_', stone);
        frame.setData(sphinxX, sphinxY - 1, ')', stone);
        frame.setData(sphinxX + 1, sphinxY - 1, GLYPH.FULL_BLOCK, stone);
        frame.setData(sphinxX + 1, sphinxY - 2, GLYPH.TRIANGLE_UP, gold);
        var obeliskPositions = [28, 52, 75];
        for (var oi = 0; oi < obeliskPositions.length; oi++) {
            var ox = obeliskPositions[oi];
            frame.setData(ox, this.horizonY - 1, '|', stone);
            frame.setData(ox, this.horizonY - 2, '|', stone);
            frame.setData(ox, this.horizonY - 3, GLYPH.TRIANGLE_UP, gold);
        }
    };
    FrameRenderer.prototype.renderDunes = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var sandLight = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var sandDark = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        for (var x = 0; x < this.width; x++) {
            var wave1 = Math.sin(x * 0.08) * 2;
            var wave2 = Math.sin(x * 0.15 + 1) * 1.5;
            var wave3 = Math.sin(x * 0.04 + 2) * 2.5;
            var duneHeight = Math.round(2 + wave1 + wave2 + wave3);
            for (var h = 0; h < duneHeight; h++) {
                var y = this.horizonY - 1 - h;
                if (y < 0)
                    continue;
                var char;
                var attr = (h % 2 === 0) ? sandLight : sandDark;
                if (h === duneHeight - 1) {
                    char = '~';
                }
                else {
                    char = GLYPH.LIGHT_SHADE;
                }
                frame.setData(x, y, char, attr);
            }
        }
    };
    FrameRenderer.prototype.renderStadium = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var colors = this.activeTheme.colors;
        var structure = makeAttr(colors.sceneryPrimary.fg, colors.sceneryPrimary.bg);
        var lights = makeAttr(colors.scenerySecondary.fg, colors.scenerySecondary.bg);
        var crowd1 = makeAttr(LIGHTRED, BG_BLACK);
        var crowd2 = makeAttr(YELLOW, BG_BLACK);
        var crowd3 = makeAttr(LIGHTCYAN, BG_BLACK);
        var crowdColors = [crowd1, crowd2, crowd3];
        this.drawGrandstand(frame, 0, 18, structure, crowdColors);
        this.drawGrandstand(frame, 62, 18, structure, crowdColors);
        this.drawScoreboard(frame, 30, lights, structure);
        this.drawFloodlight(frame, 20, lights, structure);
        this.drawFloodlight(frame, 59, lights, structure);
        for (var beamX = 22; beamX < 28; beamX++) {
            for (var beamY = 4; beamY < this.horizonY - 2; beamY += 3) {
                frame.setData(beamX, beamY, '|', makeAttr(YELLOW, BG_BLACK));
            }
        }
        for (var beamX2 = 52; beamX2 < 58; beamX2++) {
            for (var beamY2 = 4; beamY2 < this.horizonY - 2; beamY2 += 3) {
                frame.setData(beamX2, beamY2, '|', makeAttr(YELLOW, BG_BLACK));
            }
        }
    };
    FrameRenderer.prototype.drawGrandstand = function (frame, startX, width, structure, crowdColors) {
        var standHeight = 6;
        for (var tier = 0; tier < standHeight; tier++) {
            var y = this.horizonY - 1 - tier;
            if (y < 0)
                continue;
            var tierWidth = width - tier * 2;
            var tierStart = startX + tier;
            for (var x = tierStart; x < tierStart + tierWidth && x < this.width; x++) {
                if (x < 0)
                    continue;
                if (tier === 0) {
                    frame.setData(x, y, '=', structure);
                }
                else if (x === tierStart || x === tierStart + tierWidth - 1) {
                    frame.setData(x, y, GLYPH.FULL_BLOCK, structure);
                }
                else {
                    var colorIdx = (x * 3 + tier * 7) % crowdColors.length;
                    var crowdChars = ['o', 'O', '@', 'o', '*'];
                    var charIdx = (x * 5 + tier * 11) % crowdChars.length;
                    frame.setData(x, y, crowdChars[charIdx], crowdColors[colorIdx]);
                }
            }
        }
        var roofY = this.horizonY - 1 - standHeight;
        if (roofY >= 0) {
            for (var rx = startX + standHeight; rx < startX + width - standHeight && rx < this.width; rx++) {
                if (rx >= 0) {
                    frame.setData(rx, roofY, '_', structure);
                }
            }
        }
    };
    FrameRenderer.prototype.drawScoreboard = function (frame, centerX, lights, structure) {
        var boardWidth = 20;
        var boardHeight = 4;
        var boardTop = 1;
        var startX = centerX - Math.floor(boardWidth / 2);
        for (var y = boardTop; y < boardTop + boardHeight; y++) {
            for (var x = startX; x < startX + boardWidth; x++) {
                if (x < 0 || x >= this.width)
                    continue;
                if (y === boardTop || y === boardTop + boardHeight - 1) {
                    frame.setData(x, y, '-', structure);
                }
                else if (x === startX || x === startX + boardWidth - 1) {
                    frame.setData(x, y, '|', structure);
                }
                else {
                    var displayChars = [GLYPH.FULL_BLOCK, GLYPH.DARK_SHADE, GLYPH.MEDIUM_SHADE];
                    var charIdx = (x + y * 3) % displayChars.length;
                    frame.setData(x, y, displayChars[charIdx], lights);
                }
            }
        }
        var poleY1 = boardTop + boardHeight;
        var poleY2 = this.horizonY - 1;
        for (var py = poleY1; py < poleY2; py++) {
            frame.setData(startX + 3, py, '|', structure);
            frame.setData(startX + boardWidth - 4, py, '|', structure);
        }
    };
    FrameRenderer.prototype.drawFloodlight = function (frame, centerX, lights, structure) {
        for (var y = 0; y < this.horizonY - 1; y++) {
            frame.setData(centerX, y, '|', structure);
        }
        var lightBank = ['[', '*', '*', '*', ']'];
        for (var i = 0; i < lightBank.length; i++) {
            var lx = centerX - 2 + i;
            if (lx >= 0 && lx < this.width) {
                frame.setData(lx, 0, lightBank[i], lights);
            }
        }
        for (var i2 = 0; i2 < 3; i2++) {
            var lx2 = centerX - 1 + i2;
            if (lx2 >= 0 && lx2 < this.width) {
                frame.setData(lx2, 1, '*', lights);
            }
        }
    };
    FrameRenderer.prototype.renderDestroyedCity = function () {
        var frame = this.frameManager.getMountainsFrame();
        if (!frame)
            return;
        var fire = makeAttr(LIGHTRED, BG_BLACK);
        var fireGlow = makeAttr(YELLOW, BG_BLACK);
        var heli = makeAttr(GREEN, BG_BLACK);
        var tracer = makeAttr(YELLOW, BG_BLACK);
        this.drawSimpleHelicopter(frame, 70, 3, heli);
        frame.setData(67, 4, '-', tracer);
        frame.setData(65, 4, '-', tracer);
        frame.setData(63, 4, '-', tracer);
        frame.setData(61, 5, '*', fire);
        this.drawSimpleHelicopter(frame, 75, 6, heli);
        this.drawSimpleBuilding(frame, 2, 6, 3, true);
        this.drawSimpleBuilding(frame, 7, 8, 3, false);
        this.drawSimpleBuilding(frame, 12, 5, 3, true);
        this.drawSimpleBuilding(frame, 62, 7, 3, true);
        this.drawSimpleBuilding(frame, 68, 9, 4, true);
        frame.setData(70, this.horizonY - 10, '^', fireGlow);
        frame.setData(70, this.horizonY - 11, '*', fire);
        this.drawSimpleBuilding(frame, 74, 6, 3, false);
        frame.setData(5, this.horizonY - 1, '^', fireGlow);
        frame.setData(5, this.horizonY - 2, '*', fire);
        frame.setData(72, this.horizonY - 1, '^', fireGlow);
        frame.setData(72, this.horizonY - 2, '*', fire);
    };
    FrameRenderer.prototype.drawSimpleHelicopter = function (frame, x, y, attr) {
        frame.setData(x - 1, y - 1, '-', attr);
        frame.setData(x, y - 1, '+', attr);
        frame.setData(x + 1, y - 1, '-', attr);
        frame.setData(x - 1, y, '<', attr);
        frame.setData(x, y, GLYPH.FULL_BLOCK, attr);
        frame.setData(x + 1, y, GLYPH.FULL_BLOCK, attr);
        frame.setData(x + 2, y, '-', attr);
        frame.setData(x + 3, y, '>', attr);
    };
    FrameRenderer.prototype.drawSimpleBuilding = function (frame, x, height, width, damaged) {
        var building = makeAttr(DARKGRAY, BG_BLACK);
        var window = makeAttr(BLACK, BG_BLACK);
        var baseY = this.horizonY - 1;
        for (var h = 0; h < height; h++) {
            var y = baseY - h;
            if (y < 0)
                continue;
            if (damaged && h >= height - 1) {
                for (var w = 0; w < width; w++) {
                    if ((x + w) % 2 === 0) {
                        frame.setData(x + w, y, GLYPH.DARK_SHADE, building);
                    }
                }
            }
            else {
                for (var w = 0; w < width; w++) {
                    if (h % 2 === 1 && w > 0 && w < width - 1) {
                        frame.setData(x + w, y, GLYPH.MEDIUM_SHADE, window);
                    }
                    else {
                        frame.setData(x + w, y, GLYPH.FULL_BLOCK, building);
                    }
                }
            }
        }
    };
    FrameRenderer.prototype.drawMountainToFrame = function (frame, baseX, baseY, height, width, attr, highlightAttr) {
        var peakX = baseX + Math.floor(width / 2);
        for (var h = 0; h < height; h++) {
            var y = baseY - h;
            if (y < 0)
                continue;
            var halfWidth = Math.floor((height - h) * width / height / 2);
            for (var dx = -halfWidth; dx <= halfWidth; dx++) {
                var x = peakX + dx;
                if (x >= 0 && x < this.width) {
                    if (dx < 0) {
                        frame.setData(x, y, '/', attr);
                    }
                    else if (dx > 0) {
                        frame.setData(x, y, '\\', attr);
                    }
                    else {
                        if (h === height - 1) {
                            frame.setData(x, y, GLYPH.TRIANGLE_UP, highlightAttr);
                        }
                        else {
                            frame.setData(x, y, GLYPH.BOX_V, attr);
                        }
                    }
                }
            }
        }
    };
    FrameRenderer.prototype.renderSkyGrid = function (speed, dt) {
        var frame = this.frameManager.getSkyGridFrame();
        if (!frame)
            return;
        frame.clear();
        if (speed > 1) {
            this._skyGridAnimPhase -= speed * dt * 0.004;
            while (this._skyGridAnimPhase < 0)
                this._skyGridAnimPhase += 1;
            while (this._skyGridAnimPhase >= 1)
                this._skyGridAnimPhase -= 1;
        }
        var colors = this.activeTheme.colors;
        var gridAttr = makeAttr(colors.skyGrid.fg, colors.skyGrid.bg);
        var glowAttr = makeAttr(colors.skyGridGlow.fg, colors.skyGridGlow.bg);
        var vanishX = 40;
        for (var y = this.horizonY - 1; y >= 1; y--) {
            var distFromHorizon = this.horizonY - y;
            var spread = distFromHorizon * 6;
            if (this.activeTheme.sky.converging) {
                for (var offset = 0; offset <= 40; offset += 8) {
                    if (offset <= spread) {
                        if (offset === 0) {
                            frame.setData(vanishX, y, GLYPH.BOX_V, gridAttr);
                        }
                        else {
                            var leftX = vanishX - offset;
                            var rightX = vanishX + offset;
                            if (leftX >= 0 && leftX < this.width)
                                frame.setData(leftX, y, '/', glowAttr);
                            if (rightX >= 0 && rightX < this.width)
                                frame.setData(rightX, y, '\\', glowAttr);
                        }
                    }
                }
            }
            if (this.activeTheme.sky.horizontal) {
                var scanlinePhase = (this._skyGridAnimPhase + distFromHorizon * 0.25) % 1;
                if (scanlinePhase < 0)
                    scanlinePhase += 1;
                if (scanlinePhase < 0.33) {
                    var lineSpread = Math.min(spread, 39);
                    for (var x = vanishX - lineSpread; x <= vanishX + lineSpread; x++) {
                        if (x >= 0 && x < this.width) {
                            frame.setData(x, y, GLYPH.BOX_H, glowAttr);
                        }
                    }
                }
            }
        }
    };
    FrameRenderer.prototype.renderSkyStars = function (trackPosition) {
        var frame = this.frameManager.getSkyGridFrame();
        if (!frame)
            return;
        frame.clear();
        var colors = this.activeTheme.colors;
        var brightAttr = makeAttr(colors.starBright.fg, colors.starBright.bg);
        var dimAttr = makeAttr(colors.starDim.fg, colors.starDim.bg);
        var density = this.activeTheme.stars.density;
        var numStars = Math.floor(this.width * this.horizonY * density * 0.15);
        var parallaxOffset = Math.round(this._mountainScrollOffset * 0.1);
        var twinklePhase = this.activeTheme.stars.twinkle ? Math.floor(trackPosition / 30) : 0;
        for (var i = 0; i < numStars; i++) {
            var baseX = (i * 17 + 5) % this.width;
            var x = (baseX + parallaxOffset + this.width) % this.width;
            var y = (i * 13 + 3) % (this.horizonY - 1);
            var twinkleState = (i + twinklePhase) % 5;
            var isBright = (i % 3 === 0) ? (twinkleState !== 0) : (twinkleState === 0);
            var char = isBright ? '*' : '.';
            if (x >= 0 && x < this.width && y >= 0 && y < this.horizonY) {
                frame.setData(x, y, char, isBright ? brightAttr : dimAttr);
            }
        }
    };
    FrameRenderer.prototype.renderSkyGradient = function (trackPosition) {
        var frame = this.frameManager.getSkyGridFrame();
        if (!frame)
            return;
        frame.clear();
        var colors = this.activeTheme.colors;
        var highAttr = makeAttr(colors.skyTop.fg, colors.skyTop.bg);
        var midAttr = makeAttr(colors.skyMid.fg, colors.skyMid.bg);
        var lowAttr = makeAttr(colors.skyHorizon.fg, colors.skyHorizon.bg);
        var cloudOffset = Math.floor(trackPosition / 50) % this.width;
        var topZone = Math.floor(this.horizonY * 0.35);
        var midZone = Math.floor(this.horizonY * 0.7);
        for (var y = 0; y < this.horizonY; y++) {
            var attr;
            var chars;
            if (y < topZone) {
                attr = highAttr;
                chars = [' ', ' ', ' ', '.', ' '];
            }
            else if (y < midZone) {
                attr = midAttr;
                chars = [' ', '~', ' ', ' ', '-'];
            }
            else {
                attr = lowAttr;
                chars = ['~', '-', '~', ' ', '='];
            }
            for (var x = 0; x < this.width; x++) {
                var hash = ((x + cloudOffset) * 31 + y * 17) % 37;
                var charIndex = hash % chars.length;
                var char = chars[charIndex];
                frame.setData(x, y, char, attr);
            }
        }
    };
    FrameRenderer.prototype.renderSkyWater = function (trackPosition, speed, dt) {
        var frame = this.frameManager.getSkyGridFrame();
        if (!frame)
            return;
        frame.clear();
        if (speed > 1) {
            this._skyGridAnimPhase += dt * 2.0;
            while (this._skyGridAnimPhase >= 1)
                this._skyGridAnimPhase -= 1;
        }
        var deepAttr = makeAttr(BLUE, BG_BLUE);
        var midAttr = makeAttr(CYAN, BG_BLUE);
        var surfaceAttr = makeAttr(LIGHTCYAN, BG_CYAN);
        var waveAttr = makeAttr(WHITE, BG_CYAN);
        var bubbleAttr = makeAttr(WHITE, BG_BLUE);
        var lightrayAttr = makeAttr(LIGHTCYAN, BG_BLUE);
        var fish1Attr = makeAttr(YELLOW, BG_BLUE);
        var fish2Attr = makeAttr(LIGHTRED, BG_BLUE);
        var fish3Attr = makeAttr(LIGHTGREEN, BG_BLUE);
        var surfaceZone = 3;
        var lightZone = Math.floor(this.horizonY * 0.5);
        for (var y = 0; y < this.horizonY; y++) {
            var attr;
            var baseChars;
            if (y < surfaceZone) {
                attr = surfaceAttr;
                baseChars = ['~', '~', GLYPH.UPPER_HALF, '~', GLYPH.LOWER_HALF, '~'];
            }
            else if (y < lightZone) {
                attr = midAttr;
                baseChars = ['~', ' ', GLYPH.LIGHT_SHADE, ' ', '~', ' '];
            }
            else {
                attr = deepAttr;
                baseChars = [' ', ' ', GLYPH.LIGHT_SHADE, ' ', ' ', ' ', ' ', ' '];
            }
            var waveOffset = Math.floor(this._skyGridAnimPhase * 20 + y * 0.5);
            for (var x = 0; x < this.width; x++) {
                var charIndex = (x + waveOffset) % baseChars.length;
                var char = baseChars[charIndex];
                var cellAttr = attr;
                if (y < surfaceZone) {
                    var wavePhase = Math.sin((x + trackPosition * 0.1 + this._skyGridAnimPhase * 10) * 0.3);
                    if (wavePhase > 0.7) {
                        cellAttr = waveAttr;
                        char = GLYPH.FULL_BLOCK;
                    }
                }
                frame.setData(x, y, char, cellAttr);
            }
        }
        var numRays = 5;
        for (var i = 0; i < numRays; i++) {
            var rayX = 10 + i * 15;
            var rayWobble = Math.sin(this._skyGridAnimPhase * 6.28 + i * 2) * 2;
            var actualX = Math.floor(rayX + rayWobble);
            for (var ry = surfaceZone; ry < lightZone; ry++) {
                if (actualX >= 0 && actualX < this.width) {
                    var fade = 1 - (ry - surfaceZone) / (lightZone - surfaceZone);
                    if (Math.random() < fade * 0.6) {
                        frame.setData(actualX, ry, GLYPH.LIGHT_SHADE, lightrayAttr);
                    }
                }
            }
        }
        var fishPhase = trackPosition * 0.02 + this._skyGridAnimPhase * 5;
        var fish1X = Math.floor((fishPhase * 30) % (this.width + 10)) - 5;
        var fish1Y = 5 + Math.floor(Math.sin(fishPhase * 2) * 2);
        if (fish1X >= 0 && fish1X < this.width - 3 && fish1Y >= 0 && fish1Y < this.horizonY) {
            frame.setData(fish1X, fish1Y, '<', fish1Attr);
            frame.setData(fish1X + 1, fish1Y, GLYPH.FULL_BLOCK, fish1Attr);
            frame.setData(fish1X + 2, fish1Y, '>', fish1Attr);
        }
        var fish2X = this.width - Math.floor((fishPhase * 25) % (this.width + 8));
        var fish2Y = 8 + Math.floor(Math.sin(fishPhase * 1.5 + 1) * 2);
        if (fish2X >= 2 && fish2X < this.width && fish2Y >= 0 && fish2Y < this.horizonY) {
            frame.setData(fish2X, fish2Y, '<', fish2Attr);
            frame.setData(fish2X - 1, fish2Y, GLYPH.FULL_BLOCK, fish2Attr);
            frame.setData(fish2X - 2, fish2Y, '>', fish2Attr);
        }
        for (var f = 0; f < 4; f++) {
            var schoolX = Math.floor(((fishPhase + f * 0.7) * 20) % this.width);
            var schoolY = 3 + f * 2;
            if (schoolY < this.horizonY - 2) {
                frame.setData(schoolX, schoolY, '<', fish3Attr);
                frame.setData((schoolX + 1) % this.width, schoolY, '>', fish3Attr);
            }
        }
        var bubblePositions = [8, 22, 38, 55, 72];
        for (var i = 0; i < bubblePositions.length; i++) {
            var bubbleBaseX = bubblePositions[i];
            var bubbleY = this.horizonY - 2 - Math.floor((trackPosition * 0.5 + i * 7) % (this.horizonY - 2));
            var wobble = Math.sin(trackPosition * 0.1 + i * 3) * 1.5;
            var bx = Math.floor(bubbleBaseX + wobble);
            if (bubbleY >= 0 && bubbleY < this.horizonY && bx >= 0 && bx < this.width) {
                frame.setData(bx, bubbleY, 'o', bubbleAttr);
                if (bubbleY + 2 < this.horizonY) {
                    frame.setData(bx, bubbleY + 2, '.', bubbleAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.updateParallax = function (curvature, steer, speed, dt) {
        var scrollAmount = (curvature * 0.8 + steer * 0.3) * speed * dt * 0.15;
        this._mountainScrollOffset += scrollAmount;
        if (this._mountainScrollOffset > 80)
            this._mountainScrollOffset -= 80;
        if (this._mountainScrollOffset < -80)
            this._mountainScrollOffset += 80;
    };
    FrameRenderer.prototype.renderHolodeckFloor = function (trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
        var frameHeight = this.height - this.horizonY;
        var vanishX = Math.floor(this.width / 2);
        var radialSpacing = 6;
        for (var y = 0; y < frameHeight - 1; y++) {
            var distFromHorizon = y + 1;
            var spread = distFromHorizon * 6;
            frame.setData(vanishX, y, GLYPH.BOX_V, primaryAttr);
            for (var offset = radialSpacing; offset <= spread; offset += radialSpacing) {
                var leftX = vanishX - offset;
                var rightX = vanishX + offset;
                if (leftX >= 0) {
                    frame.setData(leftX, y, '/', primaryAttr);
                }
                if (rightX < this.width) {
                    frame.setData(rightX, y, '\\', primaryAttr);
                }
            }
            var linePhase = Math.floor(trackPosition / 40 + distFromHorizon * 1.5) % 5;
            if (linePhase === 0) {
                for (var x = 0; x < this.width; x++) {
                    var distFromVanish = Math.abs(x - vanishX);
                    var isOnRadial = (distFromVanish === 0) || (distFromVanish <= spread && (distFromVanish % radialSpacing) === 0);
                    frame.setData(x, y, isOnRadial ? '+' : GLYPH.BOX_H, primaryAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderLavaGround = function (_trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var frameHeight = this.height - this.horizonY;
        var firePhase = this._fireAnimPhase;
        var blackAttr = makeAttr(BLACK, BG_BLACK);
        var darkRockAttr = makeAttr(DARKGRAY, BG_BLACK);
        var hotRockAttr = makeAttr(RED, BG_BLACK);
        var lavaAttr = makeAttr(LIGHTRED, BG_RED);
        var lavaGlowAttr = makeAttr(YELLOW, BG_RED);
        var lavaBrightAttr = makeAttr(WHITE, BG_RED);
        var fireAttr = makeAttr(YELLOW, BG_BLACK);
        var fireBrightAttr = makeAttr(WHITE, BG_RED);
        var emberAttr = makeAttr(LIGHTRED, BG_BLACK);
        for (var y = 0; y < frameHeight - 1; y++) {
            var depthFactor = y / frameHeight;
            for (var x = 0; x < this.width; x++) {
                var river1 = Math.sin((x * 0.15) + (y * 0.3) + firePhase * 0.7) * 1.5;
                var river2 = Math.sin((x * 0.1) - (y * 0.2) + firePhase * 0.5 + 2) * 1.2;
                var riverIntensity = river1 + river2;
                var flameBase = Math.sin(x * 0.25 + firePhase * 1.5) +
                    Math.sin(x * 0.4 - firePhase * 2.0) * 0.5;
                var flameFlicker = Math.sin(x * 0.8 + y * 0.5 + firePhase * 4) * 0.5;
                var flameIntensity = flameBase + flameFlicker;
                var emberSeed = (x * 7919 + y * 104729) % 1000;
                var emberPhase = Math.sin(firePhase * 3 + emberSeed * 0.01);
                var isEmber = emberSeed < 30 && emberPhase > 0.7;
                var poolCenterX1 = 15 + Math.sin(firePhase * 0.3) * 3;
                var poolCenterX2 = 55 + Math.sin(firePhase * 0.4 + 1) * 4;
                var poolCenterX3 = 35 + Math.sin(firePhase * 0.2 + 2) * 2;
                var dist1 = Math.sqrt(Math.pow(x - poolCenterX1, 2) + Math.pow(y - 8, 2));
                var dist2 = Math.sqrt(Math.pow(x - poolCenterX2, 2) + Math.pow(y - 12, 2));
                var dist3 = Math.sqrt(Math.pow(x - poolCenterX3, 2) + Math.pow(y - 5, 2));
                var poolBubble = Math.sin(firePhase * 2 + x * 0.3 + y * 0.2);
                var inPool1 = dist1 < 6 + poolBubble;
                var inPool2 = dist2 < 5 + poolBubble * 0.8;
                var inPool3 = dist3 < 4 + poolBubble * 0.6;
                var char;
                var attr;
                if (isEmber && depthFactor > 0.3) {
                    char = '*';
                    attr = emberAttr;
                }
                else if (inPool1 || inPool2 || inPool3) {
                    var poolDist = inPool1 ? dist1 : (inPool2 ? dist2 : dist3);
                    var bubblePhase = Math.sin(firePhase * 3 + poolDist * 0.5);
                    if (bubblePhase > 0.7) {
                        char = 'O';
                        attr = lavaBrightAttr;
                    }
                    else if (bubblePhase > 0.3) {
                        char = GLYPH.FULL_BLOCK;
                        attr = lavaGlowAttr;
                    }
                    else if (bubblePhase > -0.2) {
                        char = GLYPH.MEDIUM_SHADE;
                        attr = lavaAttr;
                    }
                    else {
                        char = '~';
                        attr = lavaAttr;
                    }
                }
                else if (riverIntensity > 1.5) {
                    char = GLYPH.FULL_BLOCK;
                    attr = lavaGlowAttr;
                }
                else if (riverIntensity > 0.8) {
                    char = '~';
                    attr = lavaAttr;
                }
                else if (riverIntensity > 0.3) {
                    char = GLYPH.LIGHT_SHADE;
                    attr = hotRockAttr;
                }
                else if (flameIntensity > 1.2 && depthFactor < 0.6) {
                    char = '^';
                    attr = fireBrightAttr;
                }
                else if (flameIntensity > 0.6 && depthFactor < 0.7) {
                    var flameChar = ((x + Math.floor(firePhase * 5)) % 3 === 0) ? '^' : '*';
                    char = flameChar;
                    attr = fireAttr;
                }
                else if (flameIntensity > 0.2 && depthFactor < 0.8) {
                    char = GLYPH.MEDIUM_SHADE;
                    attr = emberAttr;
                }
                else if (Math.random() < 0.02 && depthFactor > 0.4) {
                    char = '.';
                    attr = emberAttr;
                }
                else {
                    var rockPattern = ((x * 3 + y * 7) % 5);
                    if (rockPattern === 0) {
                        char = GLYPH.DARK_SHADE;
                        attr = darkRockAttr;
                    }
                    else if (rockPattern === 1) {
                        char = GLYPH.MEDIUM_SHADE;
                        attr = blackAttr;
                    }
                    else {
                        char = ' ';
                        attr = blackAttr;
                    }
                }
                frame.setData(x, y, char, attr);
            }
        }
    };
    FrameRenderer.prototype.renderWaterGround = function (_trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var frameHeight = this.height - this.horizonY;
        var waterPhase = this._fireAnimPhase;
        var deepBlueAttr = makeAttr(BLUE, BG_BLUE);
        var mediumBlueAttr = makeAttr(LIGHTBLUE, BG_BLUE);
        var lightBlueAttr = makeAttr(LIGHTCYAN, BG_BLUE);
        var cyanAttr = makeAttr(CYAN, BG_CYAN);
        var whiteAttr = makeAttr(WHITE, BG_BLUE);
        var sandAttr = makeAttr(YELLOW, BG_BLUE);
        var sandDarkAttr = makeAttr(BROWN, BG_BLUE);
        var seaweedAttr = makeAttr(GREEN, BG_BLUE);
        var bubbleAttr = makeAttr(WHITE, BG_CYAN);
        for (var y = 0; y < frameHeight - 1; y++) {
            var depthFactor = y / frameHeight;
            for (var x = 0; x < this.width; x++) {
                var caustic1 = Math.sin((x * 0.2) + (y * 0.15) + waterPhase * 0.8);
                var caustic2 = Math.sin((x * 0.15) - (y * 0.1) + waterPhase * 0.6 + 1.5);
                var causticIntensity = (caustic1 + caustic2) / 2;
                var currentWave = Math.sin(y * 0.3 + waterPhase * 1.2) * 0.5;
                var currentIntensity = Math.sin((x + waterPhase * 3) * 0.1 + currentWave);
                var bubbleSeed = (x * 7919 + y * 104729) % 1000;
                var bubblePhase = Math.sin(waterPhase * 2 - y * 0.3 + bubbleSeed * 0.01);
                var isBubble = bubbleSeed < 20 && bubblePhase > 0.8 && depthFactor > 0.2;
                var sandNoise = Math.sin(x * 0.4 + y * 0.2) + Math.sin(x * 0.2 - y * 0.3);
                var isSandy = sandNoise > 0.8 && depthFactor > 0.5;
                var seaweedSeed = (x * 31 + y * 17) % 100;
                var seaweedWave = Math.sin(waterPhase * 1.5 + x * 0.5);
                var isSeaweed = seaweedSeed < 8 && depthFactor > 0.3 && seaweedWave > 0.3;
                var char;
                var attr;
                if (isBubble) {
                    char = 'o';
                    attr = bubbleAttr;
                }
                else if (isSeaweed) {
                    var seaweedChar = seaweedWave > 0.6 ? ')' : '(';
                    char = seaweedChar;
                    attr = seaweedAttr;
                }
                else if (isSandy) {
                    var sandRipple = Math.sin(x * 0.5 + waterPhase * 0.5);
                    if (sandRipple > 0.5) {
                        char = '~';
                        attr = sandAttr;
                    }
                    else if (sandRipple > 0) {
                        char = '.';
                        attr = sandDarkAttr;
                    }
                    else {
                        char = ',';
                        attr = sandAttr;
                    }
                }
                else if (causticIntensity > 0.7) {
                    char = GLYPH.LIGHT_SHADE;
                    attr = whiteAttr;
                }
                else if (causticIntensity > 0.3) {
                    char = '~';
                    attr = lightBlueAttr;
                }
                else if (currentIntensity > 0.6) {
                    char = '~';
                    attr = cyanAttr;
                }
                else if (currentIntensity > 0.2) {
                    char = '~';
                    attr = mediumBlueAttr;
                }
                else if (currentIntensity > -0.2) {
                    char = GLYPH.MEDIUM_SHADE;
                    attr = mediumBlueAttr;
                }
                else {
                    char = GLYPH.DARK_SHADE;
                    attr = deepBlueAttr;
                }
                frame.setData(x, y, char, attr);
            }
        }
    };
    FrameRenderer.prototype.renderCandyGround = function (trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var candy1 = makeAttr(ground.primary.fg, ground.primary.bg);
        var candy2 = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var frameHeight = this.height - this.horizonY;
        var sparklePhase = Math.floor(trackPosition / 30) % 6;
        for (var y = 0; y < frameHeight - 1; y++) {
            for (var x = 0; x < this.width; x++) {
                var swirl = Math.sin(x * 0.2 + y * 0.3) + Math.cos(x * 0.15 - y * 0.25);
                var char;
                var attr;
                if (swirl > 0.5) {
                    char = '@';
                    attr = candy1;
                }
                else if (swirl > -0.5) {
                    char = GLYPH.LIGHT_SHADE;
                    attr = candy2;
                }
                else {
                    char = '.';
                    attr = candy1;
                }
                var isSprinkle = ((x * 7 + y * 13 + sparklePhase) % 17) === 0;
                if (isSprinkle) {
                    var sprinkleColors = [LIGHTRED, LIGHTGREEN, LIGHTCYAN, YELLOW, LIGHTMAGENTA];
                    var colorIdx = (x + y) % sprinkleColors.length;
                    char = '*';
                    attr = makeAttr(sprinkleColors[colorIdx], BG_BLACK);
                }
                frame.setData(x, y, char, attr);
            }
        }
    };
    FrameRenderer.prototype.renderVoidGround = function (trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var voidAttr = makeAttr(ground.primary.fg, ground.primary.bg);
        var starAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var frameHeight = this.height - this.horizonY;
        var twinklePhase = Math.floor(trackPosition / 15) % 4;
        for (var y = 0; y < frameHeight - 1; y++) {
            for (var x = 0; x < this.width; x++) {
                var hash = (x * 31 + y * 17) % 100;
                if (hash < 3) {
                    var twinkle = ((x + y + twinklePhase) % 4) === 0;
                    frame.setData(x, y, twinkle ? '*' : '+', starAttr);
                }
                else if (hash < 8) {
                    frame.setData(x, y, '.', voidAttr);
                }
                else if (hash < 15) {
                    frame.setData(x, y, GLYPH.LIGHT_SHADE, voidAttr);
                }
            }
        }
        var vanishX = Math.floor(this.width / 2);
        var colors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
        for (var y = 0; y < frameHeight - 1; y++) {
            var distFromHorizon = y + 1;
            for (var ci = 0; ci < colors.length; ci++) {
                var offset = (ci - 2.5) * 8;
                var lineX = Math.floor(vanishX + offset * distFromHorizon / 5);
                if (lineX >= 0 && lineX < this.width) {
                    var glowPhase = (trackPosition / 10 + ci) % colors.length;
                    var colorIdx = Math.floor(glowPhase + ci) % colors.length;
                    frame.setData(lineX, y, GLYPH.BOX_V, makeAttr(colors[colorIdx], BG_BLACK));
                }
            }
        }
    };
    FrameRenderer.prototype.renderCobblestoneGround = function (trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var stone = makeAttr(ground.primary.fg, ground.primary.bg);
        var mortar = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var frameHeight = this.height - this.horizonY;
        for (var y = 0; y < frameHeight - 1; y++) {
            for (var x = 0; x < this.width; x++) {
                var stoneSize = 3;
                var inMortar = ((x % stoneSize) === 0) || (((y + Math.floor(x / stoneSize)) % 2 === 0) && ((y % stoneSize) === 0));
                if (inMortar) {
                    frame.setData(x, y, '+', mortar);
                }
                else {
                    var texture = ((x * 7 + y * 11) % 5);
                    var char = (texture === 0) ? GLYPH.MEDIUM_SHADE : GLYPH.DARK_SHADE;
                    frame.setData(x, y, char, stone);
                }
            }
        }
        var puddlePhase = Math.floor(trackPosition / 50) % 10;
        for (var py = frameHeight / 2; py < frameHeight - 1; py += 4) {
            var px = (py * 13 + puddlePhase * 7) % this.width;
            frame.setData(px, py, '~', makeAttr(DARKGRAY, BG_BLACK));
        }
    };
    FrameRenderer.prototype.renderJungleGround = function (trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var leaf = makeAttr(ground.primary.fg, ground.primary.bg);
        var dirt = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var frameHeight = this.height - this.horizonY;
        var rustlePhase = Math.floor(trackPosition / 25) % 4;
        for (var y = 0; y < frameHeight - 1; y++) {
            for (var x = 0; x < this.width; x++) {
                var hash = (x * 23 + y * 19) % 20;
                var char;
                var attr;
                if (hash < 8) {
                    var leafChars = ['"', 'v', 'V', 'Y', 'y'];
                    char = leafChars[(x + y + rustlePhase) % leafChars.length];
                    attr = leaf;
                }
                else if (hash < 12) {
                    char = GLYPH.LIGHT_SHADE;
                    attr = dirt;
                }
                else if (hash < 15) {
                    char = ',';
                    attr = makeAttr(BROWN, BG_BLACK);
                }
                else {
                    char = GLYPH.MEDIUM_SHADE;
                    attr = leaf;
                }
                frame.setData(x, y, char, attr);
            }
        }
        for (var mx = 5; mx < this.width - 5; mx += 15 + ((mx * 3) % 7)) {
            var my = (mx * 7) % (frameHeight - 2) + 1;
            frame.setData(mx, my, 'o', makeAttr(LIGHTRED, BG_BLACK));
        }
    };
    FrameRenderer.prototype.renderDirtGround = function (trackPosition) {
        var frame = this.frameManager.getGroundGridFrame();
        if (!frame)
            return;
        var ground = this.activeTheme.ground;
        if (!ground)
            return;
        frame.clear();
        var dirt = makeAttr(ground.primary.fg, ground.primary.bg);
        var dirtDark = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var tireTrack = makeAttr(DARKGRAY, BG_BLACK);
        var frameHeight = this.height - this.horizonY;
        var dustPhase = Math.floor(trackPosition / 20) % 8;
        for (var y = 0; y < frameHeight - 1; y++) {
            for (var x = 0; x < this.width; x++) {
                var hash = (x * 37 + y * 23 + dustPhase) % 25;
                var char;
                var attr;
                var inTireTrack = (x >= 5 && x <= 12) || (x >= this.width - 12 && x <= this.width - 5);
                if (inTireTrack && y > 2 && ((y + dustPhase) % 3 === 0)) {
                    char = '=';
                    attr = tireTrack;
                }
                else if (hash < 8) {
                    char = GLYPH.MEDIUM_SHADE;
                    attr = dirt;
                }
                else if (hash < 14) {
                    char = GLYPH.DARK_SHADE;
                    attr = dirtDark;
                }
                else if (hash < 17) {
                    char = '.';
                    attr = makeAttr(LIGHTGRAY, BG_BLACK);
                }
                else if (hash < 20) {
                    char = GLYPH.LIGHT_SHADE;
                    attr = dirt;
                }
                else {
                    var clumps = ['`', "'", ','];
                    char = clumps[(x + y) % clumps.length];
                    attr = dirtDark;
                }
                frame.setData(x, y, char, attr);
            }
        }
        for (var px = 8; px < this.width - 8; px += 20 + ((px * 7) % 10)) {
            var py = (px * 5) % (frameHeight - 3) + 1;
            var pw = 3 + (px % 3);
            for (var pox = 0; pox < pw; pox++) {
                if (px + pox < this.width) {
                    frame.setData(px + pox, py, '~', makeAttr(DARKGRAY, BG_BLACK));
                }
            }
        }
    };
    FrameRenderer.prototype.renderANSIRoadStripes = function (trackPosition, cameraX, road, roadHeight) {
        var frame = this.frameManager.getRoadFrame();
        if (!frame)
            return;
        var roadBottom = roadHeight ? roadHeight - 1 : this.height - this.horizonY - 4;
        var accumulatedCurve = 0;
        for (var screenY = roadBottom; screenY >= 0; screenY--) {
            var t = (roadBottom - screenY) / Math.max(1, roadBottom);
            var distance = 1 / (1 - t * 0.95);
            var worldZ = trackPosition + distance * 5;
            var segment = road.getSegment(worldZ);
            if (segment) {
                accumulatedCurve += segment.curve * 0.5;
            }
            var roadWidth = Math.round(40 / distance);
            var halfWidth = Math.floor(roadWidth / 2);
            var curveOffset = accumulatedCurve * distance * 0.8;
            var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
            var stripePhase = Math.floor((trackPosition + distance * 5) / 15) % 2;
            if (stripePhase === 0 && halfWidth > 2) {
                frame.setData(centerX, screenY, '-', makeAttr(YELLOW, BG_BLACK));
            }
            var leftEdge = centerX - halfWidth;
            var rightEdge = centerX + halfWidth;
            if (leftEdge >= 0 && leftEdge < this.width) {
                frame.setData(leftEdge, screenY, '|', makeAttr(WHITE, BG_BLACK));
            }
            if (rightEdge >= 0 && rightEdge < this.width) {
                frame.setData(rightEdge, screenY, '|', makeAttr(WHITE, BG_BLACK));
            }
        }
    };
    FrameRenderer.prototype.renderRoadSurface = function (trackPosition, cameraX, road) {
        var frame = this.frameManager.getRoadFrame();
        if (!frame)
            return;
        frame.clear();
        var roadBottom = this.height - this.horizonY - 1;
        var roadLength = road.totalLength;
        var accumulatedCurve = 0;
        for (var screenY = roadBottom; screenY >= 0; screenY--) {
            var t = (roadBottom - screenY) / roadBottom;
            var distance = 1 / (1 - t * 0.95);
            var worldZ = trackPosition + distance * 5;
            var segment = road.getSegment(worldZ);
            if (segment) {
                accumulatedCurve += segment.curve * 0.5;
            }
            var roadWidth = Math.round(40 / distance);
            var halfWidth = Math.floor(roadWidth / 2);
            var curveOffset = accumulatedCurve * distance * 0.8;
            var centerX = 40 + Math.round(curveOffset) - Math.round(cameraX * 0.5);
            var leftEdge = centerX - halfWidth;
            var rightEdge = centerX + halfWidth;
            var stripePhase = Math.floor((trackPosition + distance * 5) / 15) % 2;
            var wrappedZ = worldZ % roadLength;
            if (wrappedZ < 0)
                wrappedZ += roadLength;
            var isFinishLine = (wrappedZ < 200) || (wrappedZ > roadLength - 200);
            this.renderRoadScanline(frame, screenY, centerX, leftEdge, rightEdge, distance, stripePhase, isFinishLine, accumulatedCurve, worldZ);
        }
    };
    FrameRenderer.prototype.renderRoadScanline = function (frame, y, centerX, leftEdge, rightEdge, distance, stripePhase, isFinishLine, curve, worldZ) {
        var colors = this.activeTheme.colors;
        var baseSurfaceFg = distance < 10 ? colors.roadSurfaceAlt.fg : colors.roadSurface.fg;
        var baseSurfaceBg = distance < 10 ? colors.roadSurfaceAlt.bg : colors.roadSurface.bg;
        var baseGridFg = colors.roadGrid.fg;
        var baseGridBg = colors.roadGrid.bg;
        var baseEdgeFg = colors.roadEdge.fg;
        var baseEdgeBg = colors.roadEdge.bg;
        var baseStripeFg = colors.roadStripe.fg;
        var baseStripeBg = colors.roadStripe.bg;
        var baseShoulderFg = colors.shoulderPrimary.fg;
        var baseShoulderBg = colors.shoulderPrimary.bg;
        var isRainbowRoad = this.activeTheme.road && this.activeTheme.road.rainbow;
        if (isRainbowRoad) {
            var rainbowColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
            var trackPos = worldZ || 0;
            var colorIndex = Math.floor(trackPos * 0.02) % rainbowColors.length;
            var nextColorIndex = (colorIndex + 1) % rainbowColors.length;
            baseSurfaceFg = rainbowColors[colorIndex];
            baseSurfaceBg = BG_BLACK;
            baseGridFg = rainbowColors[nextColorIndex];
            baseGridBg = BG_BLACK;
            baseEdgeFg = rainbowColors[colorIndex];
            baseEdgeBg = BG_BLACK;
            baseStripeFg = WHITE;
            baseStripeBg = BG_BLACK;
            baseShoulderFg = rainbowColors[(colorIndex + 2) % rainbowColors.length];
            baseShoulderBg = BG_BLACK;
        }
        if (this.activeTheme.name === 'glitch_circuit' && typeof GlitchState !== 'undefined' && GlitchState.roadColorGlitch !== 0) {
            var surfaceGlitch = GlitchState.getGlitchedRoadColor(baseSurfaceFg, baseSurfaceBg, distance);
            baseSurfaceFg = surfaceGlitch.fg;
            baseSurfaceBg = surfaceGlitch.bg;
            var gridGlitch = GlitchState.getGlitchedRoadColor(baseGridFg, baseGridBg, distance);
            baseGridFg = gridGlitch.fg;
            baseGridBg = gridGlitch.bg;
            var edgeGlitch = GlitchState.getGlitchedRoadColor(baseEdgeFg, baseEdgeBg, distance);
            baseEdgeFg = edgeGlitch.fg;
            baseEdgeBg = edgeGlitch.bg;
            var stripeGlitch = GlitchState.getGlitchedRoadColor(baseStripeFg, baseStripeBg, distance);
            baseStripeFg = stripeGlitch.fg;
            baseStripeBg = stripeGlitch.bg;
            var shoulderGlitch = GlitchState.getGlitchedRoadColor(baseShoulderFg, baseShoulderBg, distance);
            baseShoulderFg = shoulderGlitch.fg;
            baseShoulderBg = shoulderGlitch.bg;
        }
        var roadAttr = makeAttr(baseSurfaceFg, baseSurfaceBg);
        var gridAttr = makeAttr(baseGridFg, baseGridBg);
        var edgeAttr = makeAttr(baseEdgeFg, baseEdgeBg);
        var stripeAttr = makeAttr(baseStripeFg, baseStripeBg);
        var shoulderAttr = makeAttr(baseShoulderFg, baseShoulderBg);
        var hideEdgeMarkers = this.activeTheme.road && this.activeTheme.road.hideEdgeMarkers;
        for (var x = 0; x < this.width; x++) {
            if (x >= leftEdge && x <= rightEdge) {
                if (isFinishLine) {
                    this.renderFinishCell(frame, x, y, centerX, leftEdge, rightEdge, distance);
                }
                else if ((x === leftEdge || x === rightEdge) && !hideEdgeMarkers) {
                    frame.setData(x, y, GLYPH.BOX_V, edgeAttr);
                }
                else if (Math.abs(x - centerX) < 1 && stripePhase === 0) {
                    frame.setData(x, y, GLYPH.BOX_V, stripeAttr);
                }
                else {
                    var gridPhase = Math.floor(distance) % 3;
                    if (gridPhase === 0 && distance > 5) {
                        frame.setData(x, y, GLYPH.BOX_H, gridAttr);
                    }
                    else {
                        if (isRainbowRoad) {
                            frame.setData(x, y, GLYPH.FULL_BLOCK, roadAttr);
                        }
                        else {
                            frame.setData(x, y, ' ', roadAttr);
                        }
                    }
                }
            }
            else {
                var distFromRoad = (x < leftEdge) ? leftEdge - x : x - rightEdge;
                this.renderGroundCell(frame, x, y, distFromRoad, distance, leftEdge, rightEdge, shoulderAttr, curve || 0);
            }
        }
    };
    FrameRenderer.prototype.renderFinishCell = function (frame, x, y, centerX, leftEdge, rightEdge, distance) {
        if (x === leftEdge || x === rightEdge) {
            frame.setData(x, y, GLYPH.BOX_V, makeAttr(WHITE, BG_BLACK));
            return;
        }
        var checkerSize = Math.max(1, Math.floor(3 / distance));
        var checkerX = Math.floor((x - centerX) / checkerSize);
        var checkerY = Math.floor(y / 2);
        var isWhite = ((checkerX + checkerY) % 2) === 0;
        if (isWhite) {
            frame.setData(x, y, GLYPH.FULL_BLOCK, makeAttr(WHITE, BG_LIGHTGRAY));
        }
        else {
            frame.setData(x, y, ' ', makeAttr(BLACK, BG_BLACK));
        }
    };
    FrameRenderer.prototype.renderGroundCell = function (frame, x, y, distFromRoad, distance, _leftEdge, _rightEdge, shoulderAttr, _curve) {
        var ground = this.activeTheme.ground;
        if (!ground) {
            if (distFromRoad <= 2) {
                frame.setData(x, y, GLYPH.GRASS, shoulderAttr);
            }
            return;
        }
        switch (ground.type) {
            case 'grid':
            case 'lava':
            case 'candy':
            case 'void':
            case 'cobblestone':
            case 'jungle':
            case 'dirt':
            case 'water':
                return;
            case 'dither':
                this.renderDitherGround(frame, x, y, distFromRoad, distance, ground);
                break;
            case 'grass':
                this.renderGrassGround(frame, x, y, distFromRoad, distance, ground);
                break;
            case 'sand':
                this.renderSandGround(frame, x, y, distFromRoad, distance, ground);
                break;
            case 'solid':
            default:
                if (distFromRoad <= 2) {
                    frame.setData(x, y, GLYPH.GRASS, shoulderAttr);
                }
                else {
                    var solidAttr = makeAttr(ground.primary.fg, ground.primary.bg);
                    frame.setData(x, y, ' ', solidAttr);
                }
                break;
        }
    };
    FrameRenderer.prototype.renderDitherGround = function (frame, x, y, _distFromRoad, distance, ground) {
        var pattern = ground.pattern || {};
        var density = pattern.ditherDensity || 0.3;
        var chars = pattern.ditherChars || ['.', ',', "'"];
        var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
        var secondaryAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var hash = (x * 31 + y * 17 + Math.floor(distance)) % 100;
        var normalized = hash / 100;
        if (normalized < density) {
            var charIndex = hash % chars.length;
            frame.setData(x, y, chars[charIndex], secondaryAttr);
        }
        else {
            frame.setData(x, y, ' ', primaryAttr);
        }
    };
    FrameRenderer.prototype.renderGrassGround = function (frame, x, y, _distFromRoad, distance, ground) {
        var pattern = ground.pattern || {};
        var density = pattern.grassDensity || 0.4;
        var chars = pattern.grassChars || ['"', "'", ',', '.'];
        var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
        var secondaryAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var hash = (x * 23 + y * 41 + Math.floor(distance * 2)) % 100;
        var normalized = hash / 100;
        if (normalized < density) {
            var charIndex = hash % chars.length;
            var attr = ((x + y) % 3 === 0) ? secondaryAttr : primaryAttr;
            frame.setData(x, y, chars[charIndex], attr);
        }
        else {
            frame.setData(x, y, ' ', primaryAttr);
        }
    };
    FrameRenderer.prototype.renderSandGround = function (frame, x, y, _distFromRoad, distance, ground) {
        var primaryAttr = makeAttr(ground.primary.fg, ground.primary.bg);
        var secondaryAttr = makeAttr(ground.secondary.fg, ground.secondary.bg);
        var hash = (x * 17 + y * 29 + Math.floor(distance)) % 100;
        if (hash < 15) {
            frame.setData(x, y, '~', secondaryAttr);
        }
        else if (hash < 25) {
            frame.setData(x, y, '.', primaryAttr);
        }
        else {
            frame.setData(x, y, ' ', primaryAttr);
        }
    };
    FrameRenderer.prototype.renderRoadsideSprites = function (objects) {
        objects.sort(function (a, b) { return b.distance - a.distance; });
        var poolSize = this.frameManager.getRoadsidePoolSize();
        var used = 0;
        var applyGlitch = this.activeTheme.name === 'glitch_circuit' &&
            typeof GlitchState !== 'undefined' &&
            GlitchState.roadsideColorShift !== 0;
        for (var i = 0; i < objects.length && used < poolSize; i++) {
            var obj = objects[i];
            var spriteFrame = this.frameManager.getRoadsideFrame(used);
            if (!spriteFrame)
                continue;
            var sprite = this.spriteCache[obj.type];
            if (!sprite) {
                var pool = this.activeTheme.roadside.pool;
                if (pool.length > 0) {
                    sprite = this.spriteCache[pool[0].sprite];
                }
                if (!sprite)
                    continue;
            }
            var scaleIndex = this.getScaleForDistance(obj.distance);
            renderSpriteToFrame(spriteFrame, sprite, scaleIndex);
            if (applyGlitch) {
                this.applyGlitchToSpriteFrame(spriteFrame, sprite, scaleIndex);
            }
            var size = getSpriteSize(sprite, scaleIndex);
            var frameX = Math.round(obj.x - size.width / 2);
            var frameY = Math.round(obj.y - size.height + 1);
            this.frameManager.positionRoadsideFrame(used, frameX, frameY, true);
            used++;
        }
        for (var j = used; j < poolSize; j++) {
            this.frameManager.positionRoadsideFrame(j, 0, 0, false);
        }
    };
    FrameRenderer.prototype.applyGlitchToSpriteFrame = function (frame, sprite, scaleIndex) {
        var variant = sprite.variants[scaleIndex];
        if (!variant)
            variant = sprite.variants[sprite.variants.length - 1];
        for (var row = 0; row < variant.length; row++) {
            for (var col = 0; col < variant[row].length; col++) {
                var cell = variant[row][col];
                if (cell !== null && cell !== undefined) {
                    var fg = cell.attr & 0x0F;
                    var bg = cell.attr & 0xF0;
                    var glitched = GlitchState.getGlitchedSpriteColor(fg, bg);
                    var newAttr = makeAttr(glitched.fg, glitched.bg);
                    var char = cell.char;
                    if (Math.random() < GlitchState.intensity * 0.15) {
                        char = GlitchState.corruptChar(char);
                    }
                    frame.setData(col, row, char, newAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.getScaleForDistance = function (distance) {
        if (distance > 8)
            return 0;
        if (distance > 5)
            return 1;
        if (distance > 3)
            return 2;
        if (distance > 1.5)
            return 3;
        return 4;
    };
    FrameRenderer.prototype.setBrakeLightState = function (brakeLightsOn) {
        this._currentBrakeLightsOn = brakeLightsOn;
    };
    FrameRenderer.prototype.renderPlayerVehicle = function (playerX, isFlashing, isBoosting, hasStar, hasBullet, hasLightning, carId, carColorId, brakeLightsOn) {
        var frame = this.frameManager.getVehicleFrame(0);
        if (!frame)
            return;
        var effectiveCarId = carId || 'sports';
        var effectiveColorId = carColorId || 'yellow';
        var effectiveBrake = brakeLightsOn !== undefined ? brakeLightsOn : this._currentBrakeLightsOn;
        var carDef = getCarDefinition(effectiveCarId);
        var bodyStyle = carDef ? carDef.bodyStyle : 'sports';
        var sprite = getPlayerCarSprite(bodyStyle, effectiveColorId, effectiveBrake);
        renderSpriteToFrame(frame, sprite, 0);
        var now = Date.now();
        var effectFlashColor = getEffectFlashColor(effectiveColorId);
        if (hasStar) {
            var starColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
            var colorIndex = Math.floor(now / 60) % starColors.length;
            var starColor = starColors[colorIndex];
            var starAttr = makeAttr(starColor, BG_BLACK);
            for (var y = 0; y < 3; y++) {
                for (var x = 0; x < 5; x++) {
                    var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
                    if (cell) {
                        frame.setData(x, y, cell.char, starAttr);
                    }
                }
            }
        }
        else if (hasBullet) {
            var bulletColor = (Math.floor(now / 40) % 2 === 0) ? WHITE : effectFlashColor;
            var bulletAttr = makeAttr(bulletColor, BG_BLACK);
            for (var y = 0; y < 3; y++) {
                for (var x = 0; x < 5; x++) {
                    var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
                    if (cell) {
                        frame.setData(x, y, cell.char, bulletAttr);
                    }
                }
            }
        }
        else if (hasLightning) {
            var lightningColor = (Math.floor(now / 120) % 3 === 0) ? BLUE :
                (Math.floor(now / 120) % 3 === 1) ? LIGHTCYAN : CYAN;
            var lightningAttr = makeAttr(lightningColor, BG_BLACK);
            for (var y = 0; y < 3; y++) {
                for (var x = 0; x < 5; x++) {
                    var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
                    if (cell) {
                        frame.setData(x, y, cell.char, lightningAttr);
                    }
                }
            }
        }
        else if (isFlashing) {
            var flashColor = (Math.floor(now / 100) % 2 === 0) ? WHITE : LIGHTRED;
            var flashAttr = makeAttr(flashColor, BG_BLACK);
            for (var y = 0; y < 3; y++) {
                for (var x = 0; x < 5; x++) {
                    var cell = sprite.variants[0][y] ? sprite.variants[0][y][x] : null;
                    if (cell) {
                        frame.setData(x, y, cell.char, flashAttr);
                    }
                }
            }
        }
        else if (isBoosting) {
            var boostColor = (Math.floor(now / 80) % 2 === 0) ? LIGHTCYAN : effectFlashColor;
            var boostAttr = makeAttr(boostColor, BG_BLACK);
            for (var bx = 0; bx < 5; bx++) {
                var cell = sprite.variants[0][2] ? sprite.variants[0][2][bx] : null;
                if (cell) {
                    frame.setData(bx, 2, cell.char, boostAttr);
                }
            }
        }
        var screenX = 40 + Math.round(playerX * 5) - 2;
        var screenY = this.height - 3;
        this.frameManager.positionVehicleFrame(0, screenX, screenY, true);
    };
    FrameRenderer.prototype.renderHud = function (hudData) {
        var frame = this.frameManager.getHudFrame();
        if (!frame)
            return;
        frame.clear();
        var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
        var valueAttr = colorToAttr(PALETTE.HUD_VALUE);
        var hudConfig = this.activeTheme.hud;
        var timeLabel = (hudConfig && hudConfig.timeLabel) ? hudConfig.timeLabel : 'TIME';
        var lapLabel = (hudConfig && hudConfig.lapLabel) ? hudConfig.lapLabel : 'LAP';
        var positionPrefix = (hudConfig && hudConfig.positionPrefix) ? hudConfig.positionPrefix : '';
        var speedMultiplier = (hudConfig && hudConfig.speedMultiplier) ? hudConfig.speedMultiplier : 1;
        var speedSuffix = (hudConfig && hudConfig.speedLabel) ? ' ' + hudConfig.speedLabel : '';
        this.writeStringToFrame(frame, 35, 0, timeLabel, labelAttr);
        this.writeStringToFrame(frame, 35 + timeLabel.length + 1, 0, LapTimer.format(hudData.lapTime), valueAttr);
        var bottomY = this.height - 1;
        var posStr = positionPrefix + hudData.position + PositionIndicator.getOrdinalSuffix(hudData.position);
        this.writeStringToFrame(frame, 0, bottomY - 1, posStr, valueAttr);
        this.renderLapProgressBar(frame, hudData.lap, hudData.totalLaps, hudData.lapProgress, 0, bottomY, 16, lapLabel);
        var displaySpeed = Math.round(hudData.speed * speedMultiplier);
        var maxDisplaySpeed = Math.round(300 * speedMultiplier);
        var speedDisplay = displaySpeed > maxDisplaySpeed ? maxDisplaySpeed + '+' : this.padLeft(displaySpeed.toString(), 3);
        if (speedSuffix && displaySpeed <= maxDisplaySpeed) {
            speedDisplay = displaySpeed.toFixed(1) + speedSuffix;
        }
        var speedAttr = displaySpeed > maxDisplaySpeed ? colorToAttr({ fg: LIGHTRED, bg: BG_BLACK }) : valueAttr;
        var speedX = 79 - speedDisplay.length;
        this.writeStringToFrame(frame, speedX - 12, bottomY, speedDisplay, speedAttr);
        this.renderSpeedometerBarCompact(frame, hudData.speed, hudData.speedMax, 67, bottomY, 11);
        this.renderItemSlotWithIcon(frame, hudData.heldItem);
        if (hudData.countdown > 0 && hudData.raceMode === RaceMode.GRAND_PRIX) {
            this.renderStoplight(frame, hudData.countdown);
        }
    };
    FrameRenderer.prototype.renderItemSlotWithIcon = function (frame, heldItem) {
        var slotLeft = 67;
        var slotRight = 79;
        var slotTop = 19;
        var slotBottom = 21;
        var slotHeight = 3;
        var slotWidth = slotRight - slotLeft + 1;
        var separatorAttr = makeAttr(DARKGRAY, BG_BLACK);
        frame.setData(slotLeft, slotTop, GLYPH.BOX_VD_HD, separatorAttr);
        for (var row = slotTop + 1; row < slotBottom; row++) {
            frame.setData(slotLeft, row, GLYPH.BOX_V, separatorAttr);
        }
        frame.setData(slotLeft, slotBottom, GLYPH.BOX_VD_HU, separatorAttr);
        if (heldItem === null) {
            return;
        }
        var itemType = heldItem.type;
        var uses = heldItem.uses;
        var isActivated = heldItem.activated;
        var icon = this.getItemIconCP437(itemType);
        var iconWidth = icon.cells[0].length;
        var availableWidth = slotWidth - 1;
        var iconX = slotLeft + 1 + Math.floor((availableWidth - iconWidth) / 2);
        if (uses > 1) {
            iconX = slotLeft + 3;
            var countAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
            frame.setData(slotLeft + 1, slotTop + 1, String(uses).charAt(0), countAttr);
        }
        for (var row = 0; row < icon.cells.length && row < slotHeight; row++) {
            var cellRow = icon.cells[row];
            for (var col = 0; col < cellRow.length; col++) {
                var cellData = cellRow[col];
                if (cellData.char !== ' ') {
                    var attr = this.getIconAttrCP437(cellData.attr, itemType, isActivated);
                    frame.setData(iconX + col, slotTop + row, cellData.char, attr);
                }
            }
        }
    };
    FrameRenderer.prototype.getItemIconCP437 = function (itemType) {
        var FB = GLYPH.FULL_BLOCK;
        var LH = GLYPH.LOWER_HALF;
        var UH = GLYPH.UPPER_HALF;
        var DS = GLYPH.DARK_SHADE;
        var LS = GLYPH.LIGHT_SHADE;
        var __ = ' ';
        function cell(ch, fg, bg) {
            return { char: ch, attr: makeAttr(fg, bg) };
        }
        var X = cell(__, BLACK, BG_BLACK);
        switch (itemType) {
            case ItemType.MUSHROOM:
            case ItemType.MUSHROOM_TRIPLE:
                return {
                    cells: [
                        [X, cell(LH, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(LH, LIGHTRED, BG_BLACK), X],
                        [cell(FB, LIGHTRED, BG_BLACK), cell('o', WHITE, BG_RED), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell('o', WHITE, BG_RED), cell(FB, LIGHTRED, BG_BLACK)],
                        [X, X, cell(FB, WHITE, BG_BLACK), cell(FB, WHITE, BG_BLACK), X, X]
                    ],
                    mainColor: LIGHTRED
                };
            case ItemType.MUSHROOM_GOLDEN:
                return {
                    cells: [
                        [X, cell(LH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(LH, YELLOW, BG_BLACK), X],
                        [cell(FB, YELLOW, BG_BLACK), cell('*', WHITE, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell('*', WHITE, BG_BLACK), cell(FB, YELLOW, BG_BLACK)],
                        [X, X, cell(FB, WHITE, BG_BLACK), cell(FB, WHITE, BG_BLACK), X, X]
                    ],
                    mainColor: YELLOW
                };
            case ItemType.GREEN_SHELL:
            case ItemType.GREEN_SHELL_TRIPLE:
                return {
                    cells: [
                        [X, cell(LH, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(LH, LIGHTGREEN, BG_BLACK), X],
                        [cell(FB, LIGHTGREEN, BG_BLACK), cell(LS, YELLOW, BG_GREEN), cell(FB, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(LS, YELLOW, BG_GREEN), cell(FB, LIGHTGREEN, BG_BLACK)],
                        [X, cell(UH, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(FB, LIGHTGREEN, BG_BLACK), cell(UH, LIGHTGREEN, BG_BLACK), X]
                    ],
                    mainColor: LIGHTGREEN
                };
            case ItemType.RED_SHELL:
            case ItemType.RED_SHELL_TRIPLE:
            case ItemType.SHELL:
            case ItemType.SHELL_TRIPLE:
                return {
                    cells: [
                        [X, cell(LH, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(LH, LIGHTRED, BG_BLACK), X],
                        [cell(FB, LIGHTRED, BG_BLACK), cell(LS, YELLOW, BG_RED), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(LS, YELLOW, BG_RED), cell(FB, LIGHTRED, BG_BLACK)],
                        [X, cell(UH, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(FB, LIGHTRED, BG_BLACK), cell(UH, LIGHTRED, BG_BLACK), X]
                    ],
                    mainColor: LIGHTRED
                };
            case ItemType.BLUE_SHELL:
                return {
                    cells: [
                        [cell(LS, LIGHTCYAN, BG_BLACK), cell(LH, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(LH, LIGHTCYAN, BG_BLACK), cell(LS, LIGHTCYAN, BG_BLACK)],
                        [cell(UH, WHITE, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(DS, WHITE, BG_CYAN), cell(DS, WHITE, BG_CYAN), cell(FB, LIGHTCYAN, BG_BLACK), cell(UH, WHITE, BG_BLACK)],
                        [X, cell(UH, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(FB, LIGHTCYAN, BG_BLACK), cell(UH, LIGHTCYAN, BG_BLACK), X]
                    ],
                    mainColor: LIGHTCYAN,
                    altColor: WHITE
                };
            case ItemType.BANANA:
            case ItemType.BANANA_TRIPLE:
                return {
                    cells: [
                        [X, cell(LH, GREEN, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(LH, GREEN, BG_BLACK), X],
                        [cell(FB, YELLOW, BG_BLACK), cell(UH, YELLOW, BG_BLACK), X, X, cell(UH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK)],
                        [cell(UH, YELLOW, BG_BLACK), X, X, X, X, cell(UH, YELLOW, BG_BLACK)]
                    ],
                    mainColor: YELLOW
                };
            case ItemType.STAR:
                return {
                    cells: [
                        [X, X, cell(LH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(LH, YELLOW, BG_BLACK), X, X],
                        [cell(UH, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(FB, YELLOW, BG_BLACK), cell(UH, YELLOW, BG_BLACK)],
                        [X, X, cell(UH, YELLOW, BG_BLACK), X, cell(UH, YELLOW, BG_BLACK), X, X]
                    ],
                    mainColor: YELLOW
                };
            case ItemType.LIGHTNING:
                return {
                    cells: [
                        [X, cell('/', LIGHTCYAN, BG_BLACK), cell('\\', LIGHTCYAN, BG_BLACK), X, X],
                        [cell('/', LIGHTCYAN, BG_BLACK), cell('-', YELLOW, BG_BLACK), cell('-', YELLOW, BG_BLACK), cell('\\', LIGHTCYAN, BG_BLACK), X],
                        [X, X, X, cell('\\', LIGHTCYAN, BG_BLACK), cell('/', LIGHTCYAN, BG_BLACK)]
                    ],
                    mainColor: YELLOW,
                    altColor: LIGHTCYAN
                };
            case ItemType.BULLET:
                return {
                    cells: [
                        [cell(LH, DARKGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(LH, DARKGRAY, BG_BLACK)],
                        [cell(FB, DARKGRAY, BG_BLACK), cell('O', WHITE, BG_BLACK), cell('O', WHITE, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(UH, LIGHTGRAY, BG_BLACK)],
                        [cell(UH, DARKGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(FB, LIGHTGRAY, BG_BLACK), cell(UH, DARKGRAY, BG_BLACK)]
                    ],
                    mainColor: LIGHTGRAY,
                    altColor: WHITE
                };
            default:
                return {
                    cells: [
                        [X, X, cell('?', YELLOW, BG_BLACK), X, X],
                        [X, cell('?', YELLOW, BG_BLACK), cell('?', YELLOW, BG_BLACK), cell('?', YELLOW, BG_BLACK), X],
                        [X, X, cell('?', YELLOW, BG_BLACK), X, X]
                    ],
                    mainColor: YELLOW
                };
        }
    };
    FrameRenderer.prototype.getIconAttrCP437 = function (baseAttr, itemType, isActivated) {
        switch (itemType) {
            case ItemType.STAR:
                if (isActivated) {
                    var starColors = [YELLOW, LIGHTRED, LIGHTGREEN, LIGHTCYAN, LIGHTMAGENTA, WHITE];
                    var colorIdx = Math.floor(Date.now() / 100) % starColors.length;
                    var bg = (baseAttr >> 4) & 0x07;
                    return makeAttr(starColors[colorIdx], bg << 4);
                }
                break;
            case ItemType.LIGHTNING:
                if (isActivated && (Math.floor(Date.now() / 150) % 2 === 0)) {
                    var bg2 = (baseAttr >> 4) & 0x07;
                    return makeAttr(LIGHTCYAN, bg2 << 4);
                }
                break;
        }
        return baseAttr;
    };
    FrameRenderer.prototype.renderStoplight = function (frame, countdown) {
        var countNum = Math.ceil(countdown);
        var centerX = 40;
        var topY = 3;
        var frameAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
        var redOn = countNum >= 3;
        var yellowOn = countNum === 2;
        var greenOn = countNum === 1;
        var redAttr = redOn ? colorToAttr({ fg: LIGHTRED, bg: BG_RED }) : colorToAttr({ fg: RED, bg: BG_BLACK });
        var yellowAttr = yellowOn ? colorToAttr({ fg: YELLOW, bg: BG_BROWN }) : colorToAttr({ fg: BROWN, bg: BG_BLACK });
        var greenAttr = greenOn ? colorToAttr({ fg: LIGHTGREEN, bg: BG_GREEN }) : colorToAttr({ fg: GREEN, bg: BG_BLACK });
        var boxX = centerX - 7;
        frame.setData(boxX, topY, GLYPH.DBOX_TL, frameAttr);
        for (var i = 1; i < 14; i++) {
            frame.setData(boxX + i, topY, GLYPH.DBOX_H, frameAttr);
        }
        frame.setData(boxX + 14, topY, GLYPH.DBOX_TR, frameAttr);
        frame.setData(boxX, topY + 1, GLYPH.DBOX_V, frameAttr);
        frame.setData(boxX + 1, topY + 1, GLYPH.FULL_BLOCK, redAttr);
        frame.setData(boxX + 2, topY + 1, GLYPH.FULL_BLOCK, redAttr);
        frame.setData(boxX + 3, topY + 1, GLYPH.FULL_BLOCK, redAttr);
        frame.setData(boxX + 4, topY + 1, GLYPH.DBOX_V, frameAttr);
        frame.setData(boxX + 5, topY + 1, GLYPH.FULL_BLOCK, yellowAttr);
        frame.setData(boxX + 6, topY + 1, GLYPH.FULL_BLOCK, yellowAttr);
        frame.setData(boxX + 7, topY + 1, GLYPH.FULL_BLOCK, yellowAttr);
        frame.setData(boxX + 8, topY + 1, GLYPH.DBOX_V, frameAttr);
        frame.setData(boxX + 9, topY + 1, GLYPH.FULL_BLOCK, greenAttr);
        frame.setData(boxX + 10, topY + 1, GLYPH.FULL_BLOCK, greenAttr);
        frame.setData(boxX + 11, topY + 1, GLYPH.FULL_BLOCK, greenAttr);
        frame.setData(boxX + 12, topY + 1, ' ', frameAttr);
        frame.setData(boxX + 13, topY + 1, countNum.toString(), colorToAttr({ fg: WHITE, bg: BG_BLACK }));
        frame.setData(boxX + 14, topY + 1, GLYPH.DBOX_V, frameAttr);
        frame.setData(boxX, topY + 2, GLYPH.DBOX_BL, frameAttr);
        for (var j = 1; j < 14; j++) {
            frame.setData(boxX + j, topY + 2, GLYPH.DBOX_H, frameAttr);
        }
        frame.setData(boxX + 14, topY + 2, GLYPH.DBOX_BR, frameAttr);
    };
    FrameRenderer.prototype.renderLapProgressBar = function (frame, lap, totalLaps, progress, x, y, width, label) {
        var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
        frame.setData(x, y, '[', labelAttr);
        frame.setData(x + width + 1, y, ']', labelAttr);
        var fillWidth = Math.round(progress * width);
        var lapLabel = label || 'LAP';
        var lapText = lapLabel + ' ' + lap + '/' + totalLaps;
        var textStart = Math.floor((width - lapText.length) / 2);
        for (var i = 0; i < width; i++) {
            var isFilled = i < fillWidth;
            var bg = isFilled ? BG_BLUE : BG_BLACK;
            var textIndex = i - textStart;
            if (textIndex >= 0 && textIndex < lapText.length) {
                var attr = colorToAttr({ fg: YELLOW, bg: bg });
                frame.setData(x + 1 + i, y, lapText.charAt(textIndex), attr);
            }
            else {
                if (isFilled) {
                    var filledAttr = colorToAttr({ fg: LIGHTBLUE, bg: BG_BLUE });
                    frame.setData(x + 1 + i, y, ' ', filledAttr);
                }
                else {
                    var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
                    frame.setData(x + 1 + i, y, GLYPH.LIGHT_SHADE, emptyAttr);
                }
            }
        }
    };
    FrameRenderer.prototype.renderSpeedometerBarCompact = function (frame, speed, maxSpeed, x, y, width) {
        var labelAttr = colorToAttr(PALETTE.HUD_LABEL);
        var filledAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
        var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
        var highAttr = colorToAttr({ fg: LIGHTRED, bg: BG_BLACK });
        var boostAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
        frame.setData(x, y, '[', labelAttr);
        var fillAmount = Math.min(1.0, speed / maxSpeed);
        var isBoost = speed > maxSpeed;
        var fillWidth = Math.round(fillAmount * width);
        for (var i = 0; i < width; i++) {
            var attr;
            if (i < fillWidth) {
                if (isBoost) {
                    attr = boostAttr;
                }
                else if (fillAmount > 0.8) {
                    attr = highAttr;
                }
                else {
                    attr = filledAttr;
                }
            }
            else {
                attr = emptyAttr;
            }
            var char = (i < fillWidth) ? GLYPH.FULL_BLOCK : GLYPH.LIGHT_SHADE;
            frame.setData(x + 1 + i, y, char, attr);
        }
        frame.setData(x + width + 1, y, ']', labelAttr);
    };
    FrameRenderer.prototype.writeStringToFrame = function (frame, x, y, str, attr) {
        for (var i = 0; i < str.length; i++) {
            frame.setData(x + i, y, str.charAt(i), attr);
        }
    };
    FrameRenderer.prototype.padLeft = function (str, len) {
        while (str.length < len) {
            str = ' ' + str;
        }
        return str;
    };
    FrameRenderer.prototype.triggerLightningBolt = function (targetX, targetY) {
        var x = targetX !== undefined ? targetX : 40;
        var y = targetY !== undefined ? targetY : this.height - 3;
        this._lightningBolts.push({
            x: x,
            startTime: Date.now(),
            targetY: y
        });
    };
    FrameRenderer.prototype.triggerLightningStrike = function (hitCount) {
        this.triggerLightningBolt(40, this.height - 3);
        var additionalBolts = Math.min(hitCount, 4);
        for (var i = 0; i < additionalBolts; i++) {
            var randomX = 10 + Math.floor(Math.random() * 60);
            var randomY = 8 + Math.floor(Math.random() * 10);
            this.triggerLightningBolt(randomX, randomY);
        }
    };
    FrameRenderer.prototype.renderLightningBolts = function () {
        if (this._lightningBolts.length === 0)
            return;
        var now = Date.now();
        var roadFrame = this.frameManager.getRoadFrame();
        if (!roadFrame)
            return;
        var boltDuration = 300;
        var flashCycleMs = 40;
        for (var i = this._lightningBolts.length - 1; i >= 0; i--) {
            var bolt = this._lightningBolts[i];
            var elapsed = now - bolt.startTime;
            if (elapsed > boltDuration) {
                this._lightningBolts.splice(i, 1);
                continue;
            }
            var progress = elapsed / boltDuration;
            var startY = 1;
            var currentEndY = Math.floor(startY + (bolt.targetY - startY) * Math.min(1, progress * 2));
            var colorPhase = Math.floor(now / flashCycleMs) % 3;
            var boltColor = colorPhase === 0 ? WHITE : (colorPhase === 1 ? YELLOW : LIGHTCYAN);
            var boltAttr = makeAttr(boltColor, BG_BLACK);
            var brightAttr = makeAttr(WHITE, BG_BLUE);
            var x = bolt.x;
            for (var y = startY; y <= currentEndY; y++) {
                var jitter = (y % 3 === 0) ? ((y % 6 === 0) ? 1 : -1) : 0;
                var drawX = x + jitter;
                var char = jitter > 0 ? '\\' : (jitter < 0 ? '/' : '|');
                var attr = (y === currentEndY && progress < 0.7) ? brightAttr : boltAttr;
                if (drawX >= 0 && drawX < this.width && y >= 0 && y < this.height - 1) {
                    roadFrame.setData(drawX, y, char, attr);
                }
                x = drawX;
            }
            if (progress > 0.3 && progress < 0.8) {
                var flashRadius = 2;
                var impactAttr = makeAttr(WHITE, BG_CYAN);
                for (var fx = -flashRadius; fx <= flashRadius; fx++) {
                    var impactX = bolt.x + fx;
                    if (impactX >= 0 && impactX < this.width && bolt.targetY < this.height - 1) {
                        roadFrame.setData(impactX, bolt.targetY, '*', impactAttr);
                    }
                }
            }
        }
    };
    FrameRenderer.prototype.cycle = function () {
        this.frameManager.cycle();
    };
    FrameRenderer.prototype.getRootFrame = function () {
        return this.frameManager ? this.frameManager.getRootFrame() : null;
    };
    FrameRenderer.prototype.getHudFrame = function () {
        return this.frameManager ? this.frameManager.getHudFrame() : null;
    };
    FrameRenderer.prototype.shutdown = function () {
        this.frameManager.shutdown();
        console.clear(BG_BLACK, false);
    };
    return FrameRenderer;
}());
"use strict";
var Renderer = (function () {
    function Renderer(width, height) {
        this.width = width;
        this.height = height;
        this.horizonY = 8;
        this.frame = null;
        this.useFrame = false;
        this.composer = new SceneComposer(width, height);
        this.skylineRenderer = new SkylineRenderer(this.composer, this.horizonY);
        this.roadRenderer = new RoadRenderer(this.composer, this.horizonY);
        this.spriteRenderer = new SpriteRenderer(this.composer, this.horizonY);
        this.hudRenderer = new HudRenderer(this.composer);
    }
    Renderer.prototype.init = function () {
        try {
            load("frame.js");
            this.frame = new Frame(1, 1, this.width, this.height, BG_BLACK);
            this.frame.open();
            this.useFrame = true;
            logInfo("Renderer: Using frame.js");
        }
        catch (e) {
            logWarning("Renderer: frame.js not available, using direct console");
            this.useFrame = false;
        }
        console.clear(BG_BLACK, false);
    };
    Renderer.prototype.beginFrame = function () {
        this.composer.clear();
    };
    Renderer.prototype.getComposer = function () {
        return this.composer;
    };
    Renderer.prototype.renderSky = function (trackPosition, curvature, playerSteer, speed, dt) {
        this.skylineRenderer.render(trackPosition, curvature, playerSteer, speed, dt);
    };
    Renderer.prototype.renderRoad = function (trackPosition, cameraX, track, road) {
        this.roadRenderer.render(trackPosition, cameraX, track, road);
    };
    Renderer.prototype.renderEntities = function (playerVehicle, vehicles, items) {
        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            if (!item.isAvailable())
                continue;
            var relZ = item.z - playerVehicle.z;
            var relX = item.x - playerVehicle.x;
            this.spriteRenderer.renderItemBox(relZ, relX);
        }
        var sortedVehicles = vehicles.slice();
        sortedVehicles.sort(function (a, b) {
            return (b.z - playerVehicle.z) - (a.z - playerVehicle.z);
        });
        for (var j = 0; j < sortedVehicles.length; j++) {
            var v = sortedVehicles[j];
            if (v.id === playerVehicle.id)
                continue;
            var relZ = v.z - playerVehicle.z;
            var relX = v.x - playerVehicle.x;
            this.spriteRenderer.renderOtherVehicle(relZ, relX, v.color);
        }
        this.spriteRenderer.renderPlayerVehicle(playerVehicle.x);
    };
    Renderer.prototype.renderHud = function (hudData) {
        this.hudRenderer.render(hudData);
    };
    Renderer.prototype.endFrame = function () {
        var buffer = this.composer.getBuffer();
        if (this.useFrame && this.frame) {
            for (var y = 0; y < this.height; y++) {
                for (var x = 0; x < this.width; x++) {
                    var cell = buffer[y][x];
                    this.frame.setData(x + 1, y + 1, cell.char, cell.attr);
                }
            }
            this.frame.draw();
        }
        else {
            console.home();
            for (var y = 0; y < this.height; y++) {
                var line = '';
                for (var x = 0; x < this.width; x++) {
                    line += buffer[y][x].char;
                }
                console.print(line);
                if (y < this.height - 1) {
                    console.print("\r\n");
                }
            }
        }
    };
    Renderer.prototype.getRootFrame = function () {
        return this.frame;
    };
    Renderer.prototype.shutdown = function () {
        if (this.frame) {
            this.frame.close();
            this.frame = null;
        }
        console.attributes = LIGHTGRAY;
        console.clear(BG_BLACK, false);
    };
    return Renderer;
}());
"use strict";
var RaceMode;
(function (RaceMode) {
    RaceMode["TIME_TRIAL"] = "time_trial";
    RaceMode["GRAND_PRIX"] = "grand_prix";
})(RaceMode || (RaceMode = {}));
function createInitialState(track, trackDef, road, playerVehicle, raceMode) {
    return {
        track: track,
        trackDefinition: trackDef,
        road: road,
        vehicles: [playerVehicle],
        playerVehicle: playerVehicle,
        items: [],
        time: 0,
        racing: false,
        finished: false,
        cameraX: 0,
        raceMode: raceMode || RaceMode.TIME_TRIAL,
        lapStartTime: 0,
        bestLapTime: -1,
        lapTimes: [],
        raceResults: [],
        countdown: 3,
        raceStarted: false
    };
}
"use strict";
var PhysicsSystem = (function () {
    function PhysicsSystem() {
    }
    PhysicsSystem.prototype.init = function (_state) {
    };
    PhysicsSystem.prototype.update = function (state, dt) {
        for (var i = 0; i < state.vehicles.length; i++) {
            var vehicle = state.vehicles[i];
            if (!vehicle.active)
                continue;
            var intent = { accelerate: 0, steer: 0, useItem: false };
            if (vehicle.driver) {
                intent = vehicle.driver.update(vehicle, state.track, dt);
            }
            vehicle.updatePhysics(state.road, intent, dt);
        }
    };
    return PhysicsSystem;
}());
var RaceSystem = (function () {
    function RaceSystem() {
        this.lastTrackZ = {};
    }
    RaceSystem.prototype.init = function (state) {
        for (var i = 0; i < state.vehicles.length; i++) {
            var vehicle = state.vehicles[i];
            this.lastTrackZ[vehicle.id] = vehicle.trackZ || 0;
        }
    };
    RaceSystem.prototype.update = function (state, _dt) {
        if (state.finished)
            return;
        var roadLength = state.road.totalLength;
        for (var i = 0; i < state.vehicles.length; i++) {
            var vehicle = state.vehicles[i];
            if (vehicle.isCrashed || !vehicle.active)
                continue;
            var lastZ = this.lastTrackZ[vehicle.id] || 0;
            var currentZ = vehicle.trackZ || 0;
            var crossedFinishLine = (lastZ > roadLength * 0.75 && currentZ < roadLength * 0.25);
            if (crossedFinishLine) {
                vehicle.lap++;
                debugLog.info("LAP COMPLETE! Vehicle " + vehicle.id + " now on lap " + vehicle.lap + "/" + state.track.laps);
                debugLog.info("  lastZ=" + lastZ.toFixed(1) + " currentZ=" + currentZ.toFixed(1) + " roadLength=" + roadLength);
                if (!vehicle.isNPC) {
                    var lapTime = state.time - state.lapStartTime;
                    state.lapTimes.push(lapTime);
                    if (state.bestLapTime < 0 || lapTime < state.bestLapTime) {
                        state.bestLapTime = lapTime;
                    }
                    state.lapStartTime = state.time;
                    debugLog.info("  Lap time: " + lapTime.toFixed(2) + " (best: " + state.bestLapTime.toFixed(2) + ")");
                }
                if (!vehicle.isNPC && vehicle.lap > state.track.laps) {
                    state.finished = true;
                    state.racing = false;
                    debugLog.info("RACE FINISHED! Final time: " + state.time.toFixed(2));
                }
            }
            this.lastTrackZ[vehicle.id] = currentZ;
        }
        PositionIndicator.calculatePositions(state.vehicles);
    };
    return RaceSystem;
}());
"use strict";
var CUP_POINTS = [15, 12, 10, 8, 6, 5, 4, 3, 2, 1];
function getPointsForPosition(position) {
    if (position < 1 || position > CUP_POINTS.length)
        return 0;
    return CUP_POINTS[position - 1];
}
var CupManager = (function () {
    function CupManager() {
        this.state = null;
    }
    CupManager.prototype.startCup = function (cupDef, racerNames) {
        var standings = [];
        standings.push({
            id: 1,
            name: 'YOU',
            isPlayer: true,
            points: 0,
            raceResults: []
        });
        for (var i = 0; i < racerNames.length; i++) {
            standings.push({
                id: i + 2,
                name: racerNames[i],
                isPlayer: false,
                points: 0,
                raceResults: []
            });
        }
        this.state = {
            definition: cupDef,
            currentRaceIndex: 0,
            standings: standings,
            raceResults: [],
            totalTime: 0,
            totalBestLaps: 0,
            isComplete: false
        };
    };
    CupManager.prototype.getState = function () {
        return this.state;
    };
    CupManager.prototype.getCurrentTrackId = function () {
        if (!this.state)
            return null;
        if (this.state.currentRaceIndex >= this.state.definition.trackIds.length)
            return null;
        return this.state.definition.trackIds[this.state.currentRaceIndex];
    };
    CupManager.prototype.getCurrentRaceNumber = function () {
        if (!this.state)
            return 0;
        return this.state.currentRaceIndex + 1;
    };
    CupManager.prototype.getTotalRaces = function () {
        if (!this.state)
            return 0;
        return this.state.definition.trackIds.length;
    };
    CupManager.prototype.recordRaceResult = function (trackId, trackName, racerPositions, playerTime, playerBestLap) {
        if (!this.state)
            return;
        var positions = {};
        for (var i = 0; i < racerPositions.length; i++) {
            var rp = racerPositions[i];
            positions[rp.id] = rp.position;
        }
        var result = {
            trackId: trackId,
            trackName: trackName,
            positions: positions,
            playerTime: playerTime,
            playerBestLap: playerBestLap
        };
        this.state.raceResults.push(result);
        for (var j = 0; j < this.state.standings.length; j++) {
            var standing = this.state.standings[j];
            var position = positions[standing.id] || 8;
            standing.raceResults.push(position);
            standing.points += getPointsForPosition(position);
        }
        this.state.totalTime += playerTime;
        this.state.totalBestLaps += playerBestLap;
        this.state.standings.sort(function (a, b) {
            return b.points - a.points;
        });
        this.state.currentRaceIndex++;
        if (this.state.currentRaceIndex >= this.state.definition.trackIds.length) {
            this.state.isComplete = true;
        }
    };
    CupManager.prototype.getStandings = function () {
        if (!this.state)
            return [];
        return this.state.standings.slice();
    };
    CupManager.prototype.getPlayerCupPosition = function () {
        if (!this.state)
            return 0;
        for (var i = 0; i < this.state.standings.length; i++) {
            if (this.state.standings[i].isPlayer) {
                return i + 1;
            }
        }
        return 0;
    };
    CupManager.prototype.isCupComplete = function () {
        return this.state ? this.state.isComplete : false;
    };
    CupManager.prototype.didPlayerWin = function () {
        if (!this.state || !this.state.isComplete)
            return false;
        return this.state.standings[0].isPlayer;
    };
    CupManager.prototype.getPlayerPoints = function () {
        if (!this.state)
            return 0;
        for (var i = 0; i < this.state.standings.length; i++) {
            if (this.state.standings[i].isPlayer) {
                return this.state.standings[i].points;
            }
        }
        return 0;
    };
    CupManager.prototype.clear = function () {
        this.state = null;
    };
    CupManager.CUPS = [
        {
            id: 'neon_cup',
            name: 'Neon Cup',
            trackIds: ['neon_coast', 'twilight_forest', 'sunset_beach'],
            description: 'A tour through the neon-lit night'
        },
        {
            id: 'nature_cup',
            name: 'Nature Cup',
            trackIds: ['twilight_forest', 'sunset_beach', 'cactus_canyon', 'tropical_jungle'],
            description: 'Race through nature\'s finest'
        },
        {
            id: 'spooky_cup',
            name: 'Spooky Cup',
            trackIds: ['haunted_hollow', 'dark_castle', 'ancient_ruins'],
            description: 'Face your fears on the track'
        },
        {
            id: 'fantasy_cup',
            name: 'Fantasy Cup',
            trackIds: ['candy_land', 'rainbow_road', 'dark_castle', 'kaiju_rampage'],
            description: 'A journey through imagination'
        },
        {
            id: 'grand_prix',
            name: 'Grand Prix',
            trackIds: ['neon_coast', 'sunset_beach', 'twilight_forest', 'haunted_hollow',
                'cactus_canyon', 'tropical_jungle', 'rainbow_road', 'thunder_stadium'],
            description: 'The ultimate challenge - 8 tracks!'
        }
    ];
    return CupManager;
}());
"use strict";
var DEFAULT_CONFIG = {
    screenWidth: 80,
    screenHeight: 24,
    tickRate: 60,
    maxTicksPerFrame: 5
};
var Game = (function () {
    function Game(config, highScoreManager) {
        this.config = config || DEFAULT_CONFIG;
        this.running = false;
        this.paused = false;
        this.clock = new Clock();
        this.timestep = new FixedTimestep({
            tickRate: this.config.tickRate,
            maxTicksPerFrame: this.config.maxTicksPerFrame
        });
        this.inputMap = new InputMap();
        this.controls = new Controls(this.inputMap);
        this.renderer = new FrameRenderer(this.config.screenWidth, this.config.screenHeight);
        this.trackLoader = new TrackLoader();
        this.hud = new Hud();
        this.physicsSystem = new PhysicsSystem();
        this.raceSystem = new RaceSystem();
        this.itemSystem = new ItemSystem();
        this.highScoreManager = highScoreManager || null;
        this.state = null;
    }
    Game.prototype.initWithTrack = function (trackDef, raceMode, carSelection) {
        logInfo("Game.initWithTrack(): " + trackDef.name + " mode: " + (raceMode || RaceMode.GRAND_PRIX));
        var mode = raceMode || RaceMode.GRAND_PRIX;
        this.renderer.init();
        var themeMapping = {
            'synthwave': 'synthwave',
            'midnight_city': 'city_night',
            'beach_paradise': 'sunset_beach',
            'forest_night': 'twilight_forest',
            'haunted_hollow': 'haunted_hollow',
            'winter_wonderland': 'winter_wonderland',
            'cactus_canyon': 'cactus_canyon',
            'tropical_jungle': 'tropical_jungle',
            'candy_land': 'candy_land',
            'rainbow_road': 'rainbow_road',
            'dark_castle': 'dark_castle',
            'villains_lair': 'villains_lair',
            'ancient_ruins': 'ancient_ruins',
            'thunder_stadium': 'thunder_stadium',
            'glitch_circuit': 'glitch_circuit',
            'kaiju_rampage': 'kaiju_rampage',
            'underwater_grotto': 'underwater_grotto',
            'ansi_tunnel': 'ansi_tunnel'
        };
        var themeName = themeMapping[trackDef.themeId] || 'synthwave';
        if (this.renderer.setTheme) {
            this.renderer.setTheme(themeName);
        }
        var road = buildRoadFromDefinition(trackDef);
        var track = this.trackLoader.load("neon_coast_01");
        track.laps = trackDef.laps;
        track.name = trackDef.name;
        var selectedCarId = carSelection ? carSelection.carId : 'sports';
        var selectedColorId = carSelection ? carSelection.colorId : 'yellow';
        applyCarStats(selectedCarId);
        var playerVehicle = new Vehicle();
        playerVehicle.driver = new HumanDriver(this.controls);
        playerVehicle.isNPC = false;
        playerVehicle.carId = selectedCarId;
        playerVehicle.carColorId = selectedColorId;
        var carColor = getCarColor(selectedColorId);
        playerVehicle.color = carColor ? carColor.body : YELLOW;
        this.state = createInitialState(track, trackDef, road, playerVehicle, mode);
        if (mode === RaceMode.GRAND_PRIX) {
            this.spawnRacers(7, road);
            this.positionOnStartingGrid(road);
            for (var i = 0; i < this.state.vehicles.length; i++) {
                var v = this.state.vehicles[i];
                var drv = v.driver;
                if (drv && drv.setCanMove) {
                    drv.setCanMove(false);
                }
            }
        }
        else {
            var npcCount = trackDef.npcCount !== undefined ? trackDef.npcCount : 5;
            this.spawnNPCs(npcCount, road);
            playerVehicle.trackZ = 0;
            playerVehicle.playerX = 0;
        }
        this.physicsSystem.init(this.state);
        this.raceSystem.init(this.state);
        this.itemSystem.initFromTrack(track, road);
        var renderer = this.renderer;
        this.itemSystem.setCallbacks({
            onLightningStrike: function (hitCount) {
                if (renderer.triggerLightningStrike) {
                    renderer.triggerLightningStrike(hitCount);
                }
            }
        });
        this.hud.init(this.state.time);
        this.running = true;
        this.state.racing = false;
        debugLog.info("Game initialized with track: " + trackDef.name);
        debugLog.info("  Race mode: " + mode);
        debugLog.info("  Road segments: " + road.segments.length);
        debugLog.info("  Road length: " + road.totalLength);
        debugLog.info("  Laps: " + road.laps);
        debugLog.info("  Total racers: " + this.state.vehicles.length);
    };
    Game.prototype.init = function () {
        logInfo("Game.init()");
        var defaultTrack = getTrackDefinition('test_oval');
        if (defaultTrack) {
            this.initWithTrack(defaultTrack, RaceMode.GRAND_PRIX);
        }
        else {
            this.initWithTrack({
                id: 'fallback',
                name: 'Fallback Track',
                description: 'Default fallback',
                difficulty: 1,
                laps: 2,
                themeId: 'synthwave',
                estimatedLapTime: 30,
                sections: [
                    { type: 'straight', length: 15 },
                    { type: 'curve', length: 15, curve: 0.5 },
                    { type: 'straight', length: 15 },
                    { type: 'curve', length: 15, curve: 0.5 }
                ]
            });
        }
    };
    Game.prototype.run = function () {
        debugLog.info("Entering game loop");
        this.clock.reset();
        var frameCount = 0;
        var lastLogTime = 0;
        while (this.running) {
            var deltaMs = this.clock.getDelta();
            frameCount++;
            this.processInput();
            if (!this.paused && this.state) {
                var ticks = this.timestep.update(deltaMs);
                for (var i = 0; i < ticks; i++) {
                    this.tick(this.timestep.getDt());
                }
                this.controls.endFrame();
                if (this.state.time - lastLogTime >= 1.0) {
                    debugLog.logVehicle(this.state.playerVehicle);
                    lastLogTime = this.state.time;
                }
                if (this.state.finished && this.state.racing === false) {
                    debugLog.info("Race complete! Final time: " + this.state.time.toFixed(2));
                    this.showGameOverScreen();
                    this.running = false;
                }
            }
            this.render();
            mswait(1);
        }
    };
    Game.prototype.processInput = function () {
        var now = this.clock.now();
        var key;
        while ((key = console.inkey(K_NONE, 0)) !== '') {
            this.controls.handleKey(key, now);
        }
        this.controls.update(now);
        if (this.controls.wasJustPressed(GameAction.QUIT)) {
            debugLog.info("QUIT action triggered - exiting game loop");
            this.running = false;
            this.controls.endFrame();
            return;
        }
        if (this.controls.wasJustPressed(GameAction.PAUSE)) {
            this.togglePause();
            this.controls.endFrame();
            return;
        }
    };
    Game.prototype.tick = function (dt) {
        if (!this.state)
            return;
        if (!this.state.raceStarted && this.state.raceMode === RaceMode.GRAND_PRIX) {
            this.state.countdown -= dt;
            if (this.state.countdown <= 0) {
                this.state.raceStarted = true;
                this.state.racing = true;
                this.state.countdown = 0;
                this.state.time = 0;
                this.state.lapStartTime = 0;
                this.hud.init(0);
                for (var i = 0; i < this.state.vehicles.length; i++) {
                    var vehicle = this.state.vehicles[i];
                    var driver = vehicle.driver;
                    if (driver && driver.setCanMove) {
                        driver.setCanMove(true);
                    }
                }
                debugLog.info("Race started! GO!");
            }
            return;
        }
        this.state.time += dt;
        this.physicsSystem.update(this.state, dt);
        this.raceSystem.update(this.state, dt);
        if (this.state.raceMode !== RaceMode.GRAND_PRIX) {
            this.activateDormantNPCs();
            this.applyNPCPacing();
        }
        this.itemSystem.update(dt, this.state.vehicles, this.state.road.totalLength);
        this.itemSystem.checkPickups(this.state.vehicles, this.state.road);
        if (this.controls.consumeJustPressed(GameAction.USE_ITEM)) {
            var fireBackward = this.controls.getLastAccelAction() < 0;
            var currentSpeed = this.state.playerVehicle.speed;
            var currentAccel = this.controls.getAcceleration();
            this.itemSystem.useItem(this.state.playerVehicle, this.state.vehicles, fireBackward);
            if (currentAccel >= 0 && this.state.playerVehicle.speed < currentSpeed) {
                this.state.playerVehicle.speed = currentSpeed;
            }
        }
        for (var i = 0; i < this.state.vehicles.length; i++) {
            var vehicle = this.state.vehicles[i];
            if (vehicle === this.state.playerVehicle)
                continue;
            if (!vehicle.isRacer)
                continue;
            if (!vehicle.driver)
                continue;
            var intent = vehicle.driver.update(vehicle, this.state.track, dt);
            if (intent.useItem && vehicle.heldItem !== null) {
                this.itemSystem.useItem(vehicle, this.state.vehicles);
            }
        }
        Collision.processVehicleCollisions(this.state.vehicles);
        if (this.state.raceMode !== RaceMode.GRAND_PRIX) {
            this.checkNPCRespawn();
        }
        this.state.cameraX = this.state.playerVehicle.x;
    };
    Game.prototype.showGameOverScreen = function () {
        if (!this.state)
            return;
        debugLog.info("Showing game over screen, waiting for ENTER...");
        var player = this.state.playerVehicle;
        var finalPosition = player.racePosition;
        var finalTime = this.state.time;
        var bestLap = this.state.bestLapTime > 0 ? this.state.bestLapTime : 0;
        var trackTimePosition = 0;
        var lapTimePosition = 0;
        if (this.highScoreManager && this.state.trackDefinition) {
            var trackId = this.state.trackDefinition.id;
            trackTimePosition = this.highScoreManager.checkQualification(HighScoreType.TRACK_TIME, trackId, finalTime);
            if (bestLap > 0) {
                lapTimePosition = this.highScoreManager.checkQualification(HighScoreType.LAP_TIME, trackId, bestLap);
            }
            var playerName = "Player";
            try {
                if (typeof user !== 'undefined' && user && user.alias) {
                    playerName = user.alias;
                }
            }
            catch (e) {
            }
            if (trackTimePosition > 0) {
                this.highScoreManager.submitScore(HighScoreType.TRACK_TIME, trackId, playerName, finalTime, this.state.track.name);
                logInfo("NEW HIGH SCORE! Track time #" + trackTimePosition + ": " + finalTime.toFixed(2));
            }
            if (lapTimePosition > 0) {
                this.highScoreManager.submitScore(HighScoreType.LAP_TIME, trackId, playerName, bestLap, this.state.track.name);
                logInfo("NEW HIGH SCORE! Lap time #" + lapTimePosition + ": " + bestLap.toFixed(2));
            }
        }
        this.renderResultsScreen(finalPosition, finalTime, bestLap);
        while (true) {
            var key = console.inkey(K_NONE, 100);
            if (key === '\r' || key === '\n') {
                debugLog.info("ENTER pressed, exiting game over screen");
                break;
            }
        }
        if (this.highScoreManager && this.state.trackDefinition && (trackTimePosition > 0 || lapTimePosition > 0)) {
            var trackId = this.state.trackDefinition.id;
            showTwoColumnHighScores(trackId, this.state.track.name, this.highScoreManager, trackTimePosition, lapTimePosition);
        }
    };
    Game.prototype.renderResultsScreen = function (position, totalTime, bestLap) {
        console.clear(BG_BLACK, false);
        var titleAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
        var labelAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
        var valueAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
        var boxAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
        var promptAttr = colorToAttr({ fg: LIGHTMAGENTA, bg: BG_BLACK });
        var viewWidth = 80;
        var viewHeight = 24;
        var boxWidth = 40;
        var boxHeight = 12;
        var boxX = Math.floor((viewWidth - boxWidth) / 2);
        var topY = Math.floor((viewHeight - boxHeight) / 2);
        console.gotoxy(boxX + 1, topY + 1);
        console.attributes = boxAttr;
        console.print(GLYPH.DBOX_TL);
        for (var i = 1; i < boxWidth - 1; i++) {
            console.print(GLYPH.DBOX_H);
        }
        console.print(GLYPH.DBOX_TR);
        for (var j = 1; j < boxHeight - 1; j++) {
            console.gotoxy(boxX + 1, topY + 1 + j);
            console.print(GLYPH.DBOX_V);
            for (var i = 1; i < boxWidth - 1; i++) {
                console.print(' ');
            }
            console.print(GLYPH.DBOX_V);
        }
        console.gotoxy(boxX + 1, topY + boxHeight);
        console.print(GLYPH.DBOX_BL);
        for (var i = 1; i < boxWidth - 1; i++) {
            console.print(GLYPH.DBOX_H);
        }
        console.print(GLYPH.DBOX_BR);
        var title = "=== RACE COMPLETE ===";
        console.gotoxy(boxX + 1 + Math.floor((boxWidth - title.length) / 2), topY + 3);
        console.attributes = titleAttr;
        console.print(title);
        var posSuffix = PositionIndicator.getOrdinalSuffix(position);
        console.gotoxy(boxX + 5, topY + 5);
        console.attributes = labelAttr;
        console.print("FINAL POSITION:");
        console.gotoxy(boxX + 23, topY + 5);
        console.attributes = valueAttr;
        console.print(position + posSuffix);
        console.gotoxy(boxX + 5, topY + 6);
        console.attributes = labelAttr;
        console.print("TOTAL TIME:");
        console.gotoxy(boxX + 23, topY + 6);
        console.attributes = valueAttr;
        console.print(LapTimer.format(totalTime));
        console.gotoxy(boxX + 5, topY + 7);
        console.attributes = labelAttr;
        console.print("BEST LAP:");
        console.gotoxy(boxX + 23, topY + 7);
        console.attributes = valueAttr;
        console.print(bestLap > 0 ? LapTimer.format(bestLap) : "--:--.--");
        console.gotoxy(boxX + 5, topY + 9);
        console.attributes = labelAttr;
        console.print("TRACK:");
        console.gotoxy(boxX + 23, topY + 9);
        console.attributes = valueAttr;
        console.print(this.state.track.name);
        var prompt = "Press ENTER to continue";
        console.gotoxy(boxX + 1 + Math.floor((boxWidth - prompt.length) / 2), topY + 11);
        console.attributes = promptAttr;
        console.print(prompt);
    };
    Game.prototype.activateDormantNPCs = function () {
        if (!this.state)
            return;
        var playerZ = this.state.playerVehicle.trackZ;
        var roadLength = this.state.road.totalLength;
        for (var i = 0; i < this.state.vehicles.length; i++) {
            var npc = this.state.vehicles[i];
            if (!npc.isNPC || npc.isRacer)
                continue;
            var driver = npc.driver;
            if (driver.isActive())
                continue;
            var dist = npc.trackZ - playerZ;
            if (dist < 0)
                dist += roadLength;
            if (dist < driver.getActivationRange()) {
                driver.activate();
                debugLog.info("NPC activated at distance " + dist.toFixed(0));
            }
        }
    };
    Game.prototype.checkNPCRespawn = function () {
        if (!this.state)
            return;
        var playerZ = this.state.playerVehicle.trackZ;
        var roadLength = this.state.road.totalLength;
        var respawnDistance = 100;
        var npcs = [];
        for (var i = 0; i < this.state.vehicles.length; i++) {
            if (this.state.vehicles[i].isNPC && !this.state.vehicles[i].isRacer) {
                npcs.push(this.state.vehicles[i]);
            }
        }
        if (npcs.length === 0)
            return;
        var idealSpacing = roadLength / npcs.length;
        var respawnCount = 0;
        for (var j = 0; j < npcs.length; j++) {
            var npc = npcs[j];
            var distBehind = playerZ - npc.trackZ;
            if (distBehind > respawnDistance) {
                var slotOffset = idealSpacing * (respawnCount + 1);
                var newZ = (playerZ + slotOffset) % roadLength;
                npc.trackZ = newZ;
                npc.z = newZ;
                var driver = npc.driver;
                driver.deactivate();
                npc.speed = 0;
                var laneChoice = Math.random();
                if (laneChoice < 0.4) {
                    npc.playerX = -0.35 + (Math.random() - 0.5) * 0.2;
                }
                else if (laneChoice < 0.8) {
                    npc.playerX = 0.35 + (Math.random() - 0.5) * 0.2;
                }
                else {
                    npc.playerX = (Math.random() - 0.5) * 0.3;
                }
                npc.isCrashed = false;
                npc.crashTimer = 0;
                npc.flashTimer = 0;
                respawnCount++;
            }
        }
    };
    Game.prototype.applyNPCPacing = function () {
        if (!this.state)
            return;
        var playerZ = this.state.playerVehicle.trackZ;
        var roadLength = this.state.road.totalLength;
        for (var i = 0; i < this.state.vehicles.length; i++) {
            var npc = this.state.vehicles[i];
            if (!npc.isNPC)
                continue;
            var driver = npc.driver;
            if (!driver.isActive())
                continue;
            var distance = npc.trackZ - playerZ;
            if (distance < 0)
                distance += roadLength;
            var commuterBaseSpeed = VEHICLE_PHYSICS.MAX_SPEED * driver.getSpeedFactor();
            if (distance < 100) {
                var slowFactor = 0.7 + (distance / 100) * 0.3;
                npc.speed = commuterBaseSpeed * slowFactor;
            }
            else {
                npc.speed = commuterBaseSpeed;
            }
        }
    };
    Game.prototype.render = function () {
        if (!this.state)
            return;
        var trackZ = this.state.playerVehicle.z;
        var vehicle = this.state.playerVehicle;
        var road = this.state.road;
        var curvature = road.getCurvature(trackZ);
        var playerSteer = vehicle.playerX;
        var speed = this.paused ? 0 : vehicle.speed;
        var dt = 1.0 / this.config.tickRate;
        var accel = this.controls.getAcceleration();
        var brakeLightsOn = accel <= 0;
        if (this.renderer.setBrakeLightState) {
            this.renderer.setBrakeLightState(brakeLightsOn);
        }
        this.renderer.beginFrame();
        this.renderer.renderSky(trackZ, curvature, playerSteer, speed, dt);
        this.renderer.renderRoad(trackZ, this.state.cameraX, this.state.track, this.state.road);
        this.renderer.renderEntities(this.state.playerVehicle, this.state.vehicles, this.itemSystem.getItemBoxes(), this.itemSystem.getProjectiles());
        var hudData = this.hud.compute(this.state.playerVehicle, this.state.track, this.state.road, this.state.vehicles, this.state.time, this.state.countdown, this.state.raceMode);
        this.renderer.renderHud(hudData);
        if (this.paused) {
            this.renderPauseOverlay();
        }
        this.renderer.endFrame();
    };
    Game.prototype.renderPauseOverlay = function () {
        var pausedArt = [
            ' ####   ###  #   # ### #### ####  ',
            ' #   # #   # #   # #   #    #   # ',
            ' ####  ##### #   # ### #### #   # ',
            ' #     #   # #   #   # #    #   # ',
            ' #     #   #  ###  ### #### ####  '
        ];
        var rainbowColors = [LIGHTRED, YELLOW, LIGHTGREEN, LIGHTCYAN, LIGHTBLUE, LIGHTMAGENTA];
        var colorIndex = Math.floor((system.timer * 8)) % rainbowColors.length;
        var textColor = rainbowColors[colorIndex];
        var attr = makeAttr(textColor, BG_BLACK);
        var textWidth = pausedArt[0].length;
        var textHeight = pausedArt.length;
        var startX = Math.floor((80 - textWidth) / 2);
        var startY = Math.floor((24 - textHeight) / 2);
        var hudFrame = this.renderer.getHudFrame ? this.renderer.getHudFrame() : null;
        if (!hudFrame)
            return;
        var boxPadding = 2;
        var boxAttr = makeAttr(DARKGRAY, BG_BLACK);
        for (var by = startY - boxPadding; by < startY + textHeight + boxPadding; by++) {
            for (var bx = startX - boxPadding; bx < startX + textWidth + boxPadding; bx++) {
                if (bx >= 0 && bx < 80 && by >= 0 && by < 24) {
                    hudFrame.setData(bx + 1, by + 1, GLYPH.MEDIUM_SHADE, boxAttr);
                }
            }
        }
        for (var row = 0; row < textHeight; row++) {
            var line = pausedArt[row];
            for (var col = 0; col < line.length; col++) {
                var ch = line.charAt(col);
                var x = startX + col;
                var y = startY + row;
                if (x >= 0 && x < 80 && y >= 0 && y < 24) {
                    if (ch !== ' ') {
                        hudFrame.setData(x + 1, y + 1, GLYPH.FULL_BLOCK, attr);
                    }
                }
            }
        }
        var resumeText = 'Press P to resume';
        var resumeX = Math.floor((80 - resumeText.length) / 2);
        var resumeY = startY + textHeight + 2;
        var resumeAttr = makeAttr(WHITE, BG_BLACK);
        for (var i = 0; i < resumeText.length; i++) {
            if (resumeX + i >= 0 && resumeX + i < 80 && resumeY >= 0 && resumeY < 24) {
                hudFrame.setData(resumeX + i + 1, resumeY + 1, resumeText.charAt(i), resumeAttr);
            }
        }
    };
    Game.prototype.togglePause = function () {
        this.paused = !this.paused;
        if (!this.paused) {
            this.clock.reset();
            this.timestep.reset();
        }
        logInfo("Game " + (this.paused ? "paused" : "resumed"));
    };
    Game.prototype.spawnRacers = function (count, _road) {
        if (!this.state)
            return;
        var racerColors = [
            { body: LIGHTRED, highlight: WHITE },
            { body: LIGHTBLUE, highlight: LIGHTCYAN },
            { body: LIGHTGREEN, highlight: WHITE },
            { body: LIGHTMAGENTA, highlight: WHITE },
            { body: LIGHTCYAN, highlight: WHITE },
            { body: WHITE, highlight: LIGHTGRAY },
            { body: BROWN, highlight: YELLOW }
        ];
        var skillLevels = [0.82, 0.75, 0.58, 0.52, 0.42, 0.38, 0.35];
        for (var i = 0; i < count && i < racerColors.length; i++) {
            var racer = new Vehicle();
            var skill = skillLevels[i] || 0.6;
            racer.driver = new RacerDriver(skill);
            racer.isNPC = true;
            racer.isRacer = true;
            var typeIndex = Math.floor(Math.random() * NPC_VEHICLE_TYPES.length);
            racer.npcType = NPC_VEHICLE_TYPES[typeIndex];
            var colorPalette = racerColors[i];
            racer.color = colorPalette.body;
            racer.npcColorIndex = i;
            racer.trackZ = 0;
            racer.z = 0;
            racer.playerX = 0;
            this.state.vehicles.push(racer);
        }
        debugLog.info("Spawned " + count + " CPU racers for Grand Prix");
    };
    Game.prototype.positionOnStartingGrid = function (_road) {
        if (!this.state)
            return;
        var vehicles = this.state.vehicles;
        var gridRowSpacing = 20;
        var gridColSpacing = 0.5;
        var playerIdx = -1;
        for (var p = 0; p < vehicles.length; p++) {
            if (!vehicles[p].isNPC) {
                playerIdx = p;
                break;
            }
        }
        var numAICars = vehicles.length - 1;
        var totalGridLength = Math.ceil(numAICars / 2) * gridRowSpacing;
        var aiSlot = 0;
        for (var v = 0; v < vehicles.length; v++) {
            var vehicle = vehicles[v];
            if (v === playerIdx) {
                vehicle.trackZ = totalGridLength + gridRowSpacing;
                vehicle.z = vehicle.trackZ;
                vehicle.playerX = 0;
            }
            else {
                var row = Math.floor(aiSlot / 2);
                var col = aiSlot % 2;
                vehicle.trackZ = totalGridLength - (row * gridRowSpacing);
                vehicle.z = vehicle.trackZ;
                vehicle.playerX = (col === 0) ? -gridColSpacing : gridColSpacing;
                aiSlot++;
            }
            vehicle.speed = 0;
            vehicle.lap = 1;
            vehicle.checkpoint = 0;
            vehicle.racePosition = 1;
            debugLog.info("Grid slot " + v + " (NPC=" + vehicle.isNPC + "): trackZ=" + vehicle.trackZ.toFixed(0) + " X=" + vehicle.playerX.toFixed(2));
        }
        debugLog.info("Positioned " + vehicles.length + " vehicles on starting grid (player at front)");
    };
    Game.prototype.spawnNPCs = function (count, road) {
        if (!this.state)
            return;
        var roadLength = road.totalLength;
        var spacing = roadLength / count;
        for (var i = 0; i < count; i++) {
            var npc = new Vehicle();
            npc.driver = new CommuterDriver();
            npc.isNPC = true;
            var typeIndex = Math.floor(Math.random() * NPC_VEHICLE_TYPES.length);
            npc.npcType = NPC_VEHICLE_TYPES[typeIndex];
            npc.npcColorIndex = Math.floor(Math.random() * NPC_VEHICLE_COLORS.length);
            var colorPalette = NPC_VEHICLE_COLORS[npc.npcColorIndex];
            npc.color = colorPalette.body;
            var baseZ = spacing * i;
            var jitter = spacing * 0.2 * (Math.random() - 0.5);
            npc.trackZ = (baseZ + jitter + roadLength) % roadLength;
            npc.z = npc.trackZ;
            var laneOffset = (i % 2 === 0) ? -0.3 : 0.3;
            npc.playerX = laneOffset + (Math.random() - 0.5) * 0.4;
            this.state.vehicles.push(npc);
        }
        debugLog.info("Spawned " + count + " NPC commuters");
    };
    Game.prototype.isRunning = function () {
        return this.running;
    };
    Game.prototype.getFinalRaceResults = function () {
        if (!this.state || !this.state.finished)
            return null;
        var positions = [];
        positions.push({
            id: 1,
            position: this.state.playerVehicle.racePosition
        });
        var aiId = 2;
        for (var i = 0; i < this.state.vehicles.length; i++) {
            var v = this.state.vehicles[i];
            if (v !== this.state.playerVehicle && v.isRacer) {
                positions.push({
                    id: aiId++,
                    position: v.racePosition
                });
            }
        }
        return {
            positions: positions,
            playerTime: this.state.time,
            playerBestLap: this.state.bestLapTime > 0 ? this.state.bestLapTime : this.state.time / this.state.track.laps
        };
    };
    Game.prototype.shutdown = function () {
        logInfo("Game.shutdown()");
        this.renderer.shutdown();
        this.controls.clearAll();
    };
    return Game;
}());
"use strict";
var CIRCUITS = [
    {
        id: 'retro_cup',
        name: 'RETRO CUP',
        icon: [
            '  ____  ',
            ' /    \\ ',
            '|  RC  |',
            ' \\____/ ',
            '   ||   '
        ],
        color: LIGHTCYAN,
        trackIds: ['neon_coast', 'downtown_dash', 'sunset_beach', 'twilight_grove'],
        description: 'Classic retro vibes'
    },
    {
        id: 'nature_cup',
        name: 'NATURE CUP',
        icon: [
            '   /\\   ',
            '  /  \\  ',
            ' /    \\ ',
            '/______\\',
            '   NC   '
        ],
        color: LIGHTGREEN,
        trackIds: ['winter_wonderland', 'cactus_canyon', 'jungle_run', 'sugar_rush'],
        description: 'Wild natural courses'
    },
    {
        id: 'dark_cup',
        name: 'DARK CUP',
        icon: [
            ' _\\||/_ ',
            '  \\||/  ',
            '  /||\\ ',
            ' /_||\\_',
            '   DC   '
        ],
        color: LIGHTMAGENTA,
        trackIds: ['haunted_hollow', 'fortress_rally', 'inferno_speedway', 'mermaid_lagoon'],
        description: 'Dangerous & mysterious'
    },
    {
        id: 'special_cup',
        name: 'SPECIAL CUP',
        icon: [
            '   **   ',
            '  *  *  ',
            ' * SC * ',
            '  *  *  ',
            '   **   '
        ],
        color: YELLOW,
        trackIds: ['celestial_circuit', 'data_highway', 'glitch_circuit', 'kaiju_rampage'],
        description: 'Ultimate challenge'
    }
];
var LEFT_PANEL_WIDTH = 22;
var RIGHT_PANEL_START = 24;
var SCREEN_WIDTH = 80;
function showTrackSelector(highScoreManager) {
    var state = {
        mode: 'circuit',
        circuitIndex: 0,
        trackIndex: 0
    };
    console.clear(LIGHTGRAY, false);
    drawSelectorUI(state, highScoreManager);
    while (true) {
        var key = console.inkey(K_UPPER, 100);
        if (key === '')
            continue;
        var needsRedraw = false;
        if (state.mode === 'circuit') {
            if (key === KEY_UP || key === 'W' || key === '8') {
                state.circuitIndex = (state.circuitIndex - 1 + CIRCUITS.length) % CIRCUITS.length;
                needsRedraw = true;
            }
            else if (key === KEY_DOWN || key === 'S' || key === '2') {
                state.circuitIndex = (state.circuitIndex + 1) % CIRCUITS.length;
                needsRedraw = true;
            }
            else if (key === '\r' || key === '\n' || key === ' ' || key === KEY_RIGHT || key === 'D' || key === '6') {
                state.mode = 'tracks';
                state.trackIndex = 0;
                needsRedraw = true;
            }
            else if (key === 'H' && highScoreManager) {
                var circuit = CIRCUITS[state.circuitIndex];
                showHighScoreList(HighScoreType.CIRCUIT_TIME, circuit.id, '=== CIRCUIT HIGH SCORES ===', circuit.name, highScoreManager);
                needsRedraw = true;
            }
            else if (key === 'Q' || key === KEY_ESC) {
                return { selected: false, track: null };
            }
            else if (key === '?') {
                var secretResult = showSecretTracksMenu();
                if (secretResult.selected && secretResult.track) {
                    return secretResult;
                }
                needsRedraw = true;
            }
        }
        else {
            if (key === KEY_UP || key === 'W' || key === '8') {
                state.trackIndex--;
                if (state.trackIndex < 0)
                    state.trackIndex = 4;
                needsRedraw = true;
            }
            else if (key === KEY_DOWN || key === 'S' || key === '2') {
                state.trackIndex++;
                if (state.trackIndex > 4)
                    state.trackIndex = 0;
                needsRedraw = true;
            }
            else if (key === '\r' || key === '\n' || key === ' ') {
                var circuit = CIRCUITS[state.circuitIndex];
                if (state.trackIndex === 4) {
                    var circuitTracks = getCircuitTracks(circuit);
                    return {
                        selected: true,
                        track: circuitTracks[0],
                        isCircuitMode: true,
                        circuitTracks: circuitTracks,
                        circuitId: circuit.id,
                        circuitName: circuit.name
                    };
                }
                else {
                    var trackDef = getTrackDefinition(circuit.trackIds[state.trackIndex]);
                    if (trackDef) {
                        return {
                            selected: true,
                            track: trackDef,
                            isCircuitMode: false,
                            circuitTracks: null
                        };
                    }
                }
            }
            else if (key === KEY_LEFT || key === 'A' || key === '4' || key === KEY_ESC) {
                state.mode = 'circuit';
                needsRedraw = true;
            }
            else if (key === 'H' && highScoreManager) {
                var circuit = CIRCUITS[state.circuitIndex];
                if (state.trackIndex === 4) {
                    showHighScoreList(HighScoreType.CIRCUIT_TIME, circuit.id, '=== CIRCUIT HIGH SCORES ===', circuit.name, highScoreManager);
                }
                else {
                    var trackId = circuit.trackIds[state.trackIndex];
                    var trackDef = getTrackDefinition(trackId);
                    if (trackDef) {
                        showTwoColumnHighScores(trackId, trackDef.name, highScoreManager, 0, 0);
                    }
                }
                needsRedraw = true;
            }
            else if (key === 'Q') {
                return { selected: false, track: null };
            }
            else if (key === '?') {
                var secretResult = showSecretTracksMenu();
                if (secretResult.selected && secretResult.track) {
                    return secretResult;
                }
                needsRedraw = true;
            }
        }
        if (needsRedraw) {
            console.clear(LIGHTGRAY, false);
            drawSelectorUI(state, highScoreManager);
        }
    }
}
function getCircuitTracks(circuit) {
    var tracks = [];
    for (var i = 0; i < circuit.trackIds.length; i++) {
        var track = getTrackDefinition(circuit.trackIds[i]);
        if (track)
            tracks.push(track);
    }
    return tracks;
}
function getSecretTracks() {
    var secrets = [];
    var allTracks = getAllTracks();
    for (var i = 0; i < allTracks.length; i++) {
        if (allTracks[i].hidden) {
            secrets.push(allTracks[i]);
        }
    }
    return secrets;
}
function showSecretTracksMenu() {
    var secretTracks = getSecretTracks();
    if (secretTracks.length === 0) {
        console.clear(LIGHTGRAY, false);
        console.gotoxy(1, 10);
        console.attributes = LIGHTMAGENTA;
        console.print('       ' + GLYPH.DBOX_TL + repeatChar(GLYPH.DBOX_H, 44) + GLYPH.DBOX_TR + '\r\n');
        console.print('       ' + GLYPH.DBOX_V + '         NO SECRETS FOUND... YET          ' + GLYPH.DBOX_V + '\r\n');
        console.print('       ' + GLYPH.DBOX_V + '                                          ' + GLYPH.DBOX_V + '\r\n');
        console.print('       ' + GLYPH.DBOX_V + '  Keep racing to unlock hidden tracks!    ' + GLYPH.DBOX_V + '\r\n');
        console.print('       ' + GLYPH.DBOX_BL + repeatChar(GLYPH.DBOX_H, 44) + GLYPH.DBOX_BR + '\r\n');
        console.print('\r\n');
        console.attributes = DARKGRAY;
        console.print('                 Press any key to return...\r\n');
        console.inkey(K_NONE, 5000);
        return { selected: false, track: null };
    }
    var selectedIndex = 0;
    while (true) {
        console.clear(LIGHTGRAY, false);
        console.gotoxy(1, 1);
        console.attributes = LIGHTRED;
        console.print(repeatChar(GLYPH.DBOX_H, 79) + '\r\n');
        console.attributes = YELLOW;
        console.print('                     #### SECRET TRACKS ####\r\n');
        console.attributes = LIGHTRED;
        console.print(repeatChar(GLYPH.DBOX_H, 79) + '\r\n');
        console.print('\r\n');
        console.attributes = DARKGRAY;
        console.print('  These tracks are hidden from the main menu. Shh, it\'s a secret!\r\n\r\n');
        for (var i = 0; i < secretTracks.length; i++) {
            var track = secretTracks[i];
            console.gotoxy(5, 9 + i * 2);
            if (i === selectedIndex) {
                console.attributes = LIGHTCYAN;
                console.print('>>  ');
            }
            else {
                console.attributes = DARKGRAY;
                console.print('    ');
            }
            console.attributes = i === selectedIndex ? WHITE : LIGHTGRAY;
            console.print(track.name);
            console.attributes = DARKGRAY;
            console.print(' - ');
            console.print(track.description);
            console.gotoxy(65, 9 + i * 2);
            console.attributes = YELLOW;
            console.print(renderDifficultyStars(track.difficulty));
        }
        console.gotoxy(5, 20);
        console.attributes = DARKGRAY;
        console.print(repeatChar(GLYPH.BOX_H, 68) + '\r\n');
        console.gotoxy(5, 21);
        console.attributes = CYAN;
        console.print('[');
        console.attributes = WHITE;
        console.print('W/S');
        console.attributes = CYAN;
        console.print('] Select   [');
        console.attributes = WHITE;
        console.print('ENTER');
        console.attributes = CYAN;
        console.print('] Race   [');
        console.attributes = WHITE;
        console.print('ESC/Q');
        console.attributes = CYAN;
        console.print('] Back');
        var key = console.inkey(K_UPPER, 100);
        if (key === '')
            continue;
        if (key === KEY_UP || key === 'W' || key === '8') {
            selectedIndex = (selectedIndex - 1 + secretTracks.length) % secretTracks.length;
        }
        else if (key === KEY_DOWN || key === 'S' || key === '2') {
            selectedIndex = (selectedIndex + 1) % secretTracks.length;
        }
        else if (key === '\r' || key === '\n' || key === ' ') {
            return {
                selected: true,
                track: secretTracks[selectedIndex],
                isCircuitMode: false,
                circuitTracks: null
            };
        }
        else if (key === 'Q' || key === KEY_ESC) {
            return { selected: false, track: null };
        }
    }
}
function drawSelectorUI(state, highScoreManager) {
    drawHeader();
    drawLeftPanel(state);
    drawRightPanel(state, highScoreManager);
    drawControls(state);
}
function drawHeader() {
    console.gotoxy(1, 1);
    console.attributes = LIGHTMAGENTA;
    console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
    console.gotoxy(1, 2);
    console.attributes = LIGHTCYAN;
    var title = '  SELECT YOUR RACE  ';
    var padding = Math.floor((SCREEN_WIDTH - title.length) / 2);
    console.print(repeatChar(' ', padding) + title);
    console.gotoxy(1, 3);
    console.attributes = LIGHTMAGENTA;
    console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
}
function drawLeftPanel(state) {
    var y = 5;
    for (var sy = 4; sy <= 22; sy++) {
        console.gotoxy(LEFT_PANEL_WIDTH, sy);
        console.attributes = DARKGRAY;
        console.print(GLYPH.BOX_V);
    }
    if (state.mode === 'circuit') {
        drawCircuitSelector(state, y);
    }
    else {
        drawTrackList(state, y);
    }
}
function drawCircuitSelector(state, startY) {
    console.gotoxy(2, startY);
    console.attributes = WHITE;
    console.print('SELECT CIRCUIT');
    for (var i = 0; i < CIRCUITS.length; i++) {
        var circuit = CIRCUITS[i];
        var isSelected = (i === state.circuitIndex);
        var baseY = startY + 2 + (i * 4);
        console.gotoxy(1, baseY + 1);
        if (isSelected) {
            console.attributes = circuit.color;
            console.print(GLYPH.TRIANGLE_RIGHT);
        }
        else {
            console.print(' ');
        }
        console.gotoxy(3, baseY);
        console.attributes = isSelected ? circuit.color : DARKGRAY;
        var shortName = circuit.name.substring(0, 10);
        console.print(shortName);
        console.gotoxy(3, baseY + 1);
        console.attributes = isSelected ? WHITE : DARKGRAY;
        console.print(circuit.icon[2].substring(0, 8));
        console.gotoxy(3, baseY + 2);
        console.attributes = isSelected ? LIGHTGRAY : DARKGRAY;
        console.print('4 tracks');
    }
}
function drawTrackList(state, startY) {
    var circuit = CIRCUITS[state.circuitIndex];
    console.gotoxy(2, startY);
    console.attributes = DARKGRAY;
    console.print(GLYPH.TRIANGLE_LEFT + ' ');
    console.attributes = circuit.color;
    console.print(circuit.name.substring(0, 14));
    console.gotoxy(2, startY + 1);
    console.attributes = DARKGRAY;
    console.print(repeatChar(GLYPH.BOX_H, LEFT_PANEL_WIDTH - 4));
    for (var i = 0; i < circuit.trackIds.length; i++) {
        var track = getTrackDefinition(circuit.trackIds[i]);
        if (!track)
            continue;
        var isSelected = (i === state.trackIndex);
        var y = startY + 3 + (i * 3);
        console.gotoxy(1, y);
        if (isSelected) {
            console.attributes = circuit.color;
            console.print(GLYPH.TRIANGLE_RIGHT);
        }
        else {
            console.print(' ');
        }
        console.gotoxy(3, y);
        console.attributes = isSelected ? WHITE : LIGHTGRAY;
        console.print((i + 1) + '.');
        console.gotoxy(6, y);
        console.attributes = isSelected ? circuit.color : CYAN;
        var name = track.name.substring(0, 13);
        console.print(name);
        console.gotoxy(3, y + 1);
        console.attributes = isSelected ? YELLOW : BROWN;
        console.print(renderDifficultyStars(track.difficulty));
    }
    var playY = startY + 3 + (4 * 3);
    var isPlaySelected = (state.trackIndex === 4);
    console.gotoxy(1, playY);
    if (isPlaySelected) {
        console.attributes = LIGHTGREEN;
        console.print(GLYPH.TRIANGLE_RIGHT);
    }
    else {
        console.print(' ');
    }
    console.gotoxy(3, playY);
    console.attributes = isPlaySelected ? LIGHTGREEN : GREEN;
    console.print(GLYPH.TRIANGLE_RIGHT + ' PLAY CUP');
}
function drawRightPanel(state, highScoreManager) {
    var circuit = CIRCUITS[state.circuitIndex];
    console.gotoxy(RIGHT_PANEL_START, 5);
    console.attributes = WHITE;
    if (state.mode === 'circuit' || (state.mode === 'tracks' && state.trackIndex === 4)) {
        console.print('CIRCUIT INFO');
        console.gotoxy(RIGHT_PANEL_START, 6);
        console.attributes = DARKGRAY;
        console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH - RIGHT_PANEL_START - 1));
        drawCircuitInfo(circuit, highScoreManager);
    }
    else {
        var track = getTrackDefinition(circuit.trackIds[state.trackIndex]);
        if (track) {
            console.print('TRACK INFO');
            console.gotoxy(RIGHT_PANEL_START, 6);
            console.attributes = DARKGRAY;
            console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH - RIGHT_PANEL_START - 1));
            drawTrackInfo(track, circuit.color, highScoreManager);
            drawTrackRoute(track);
        }
    }
}
function drawCircuitInfo(circuit, _highScoreManager) {
    var y = 8;
    console.gotoxy(RIGHT_PANEL_START, y);
    console.attributes = circuit.color;
    console.print(circuit.name);
    for (var iconLine = 0; iconLine < circuit.icon.length; iconLine++) {
        console.gotoxy(RIGHT_PANEL_START + 20, y - 1 + iconLine);
        console.attributes = circuit.color;
        console.print(circuit.icon[iconLine]);
    }
    console.gotoxy(RIGHT_PANEL_START, y + 1);
    console.attributes = LIGHTGRAY;
    console.print(circuit.description);
    console.gotoxy(RIGHT_PANEL_START, y + 3);
    console.attributes = WHITE;
    console.print('Tracks in this circuit:');
    var totalLaps = 0;
    var totalTime = 0;
    for (var i = 0; i < circuit.trackIds.length; i++) {
        var track = getTrackDefinition(circuit.trackIds[i]);
        if (!track)
            continue;
        console.gotoxy(RIGHT_PANEL_START + 2, y + 5 + i);
        console.attributes = CYAN;
        console.print((i + 1) + '. ' + padRight(track.name, 22));
        console.attributes = YELLOW;
        console.print(renderDifficultyStars(track.difficulty));
        totalLaps += track.laps;
        totalTime += track.estimatedLapTime * track.laps;
    }
    console.gotoxy(RIGHT_PANEL_START, y + 11);
    console.attributes = DARKGRAY;
    console.print(repeatChar(GLYPH.BOX_H, 40));
    console.gotoxy(RIGHT_PANEL_START, y + 12);
    console.attributes = LIGHTGRAY;
    console.print('Total Races: ');
    console.attributes = WHITE;
    console.print(circuit.trackIds.length.toString());
    console.gotoxy(RIGHT_PANEL_START + 20, y + 12);
    console.attributes = LIGHTGRAY;
    console.print('Total Laps: ');
    console.attributes = WHITE;
    console.print(totalLaps.toString());
    console.gotoxy(RIGHT_PANEL_START, y + 13);
    console.attributes = LIGHTGRAY;
    console.print('Est. Time: ');
    console.attributes = WHITE;
    console.print(formatTime(totalTime));
}
function drawTrackInfo(track, _accentColor, highScoreManager) {
    var theme = getTrackTheme(track);
    console.gotoxy(RIGHT_PANEL_START, 8);
    console.attributes = theme.road.edge.fg;
    console.print(track.name);
    console.attributes = YELLOW;
    console.print('  ' + renderDifficultyStars(track.difficulty));
    console.gotoxy(RIGHT_PANEL_START, 9);
    console.attributes = DARKGRAY;
    console.print(track.laps + ' laps');
    if (highScoreManager) {
        var trackTimeScore = highScoreManager.getTopScore(HighScoreType.TRACK_TIME, track.id);
        var lapTimeScore = highScoreManager.getTopScore(HighScoreType.LAP_TIME, track.id);
        if (trackTimeScore || lapTimeScore) {
            console.gotoxy(RIGHT_PANEL_START, 10);
            console.attributes = DARKGRAY;
            console.print('High Scores:');
            if (trackTimeScore) {
                console.gotoxy(RIGHT_PANEL_START + 2, 11);
                console.attributes = LIGHTGRAY;
                console.print('Track: ');
                console.attributes = LIGHTGREEN;
                console.print(LapTimer.format(trackTimeScore.time) + ' ');
                console.attributes = DARKGRAY;
                console.print('(' + trackTimeScore.playerName + ')');
            }
            if (lapTimeScore) {
                console.gotoxy(RIGHT_PANEL_START + 2, 12);
                console.attributes = LIGHTGRAY;
                console.print('Lap:   ');
                console.attributes = LIGHTGREEN;
                console.print(LapTimer.format(lapTimeScore.time) + ' ');
                console.attributes = DARKGRAY;
                console.print('(' + lapTimeScore.playerName + ')');
            }
        }
    }
    drawThemedTrackMap(track, theme);
}
function drawThemedTrackMap(track, theme) {
    var mapX = RIGHT_PANEL_START;
    var mapY = 11;
    var mapWidth = SCREEN_WIDTH - RIGHT_PANEL_START - 1;
    var mapHeight = 13;
    var pixelWidth = mapWidth - 2;
    var pixelHeight = (mapHeight - 1) * 2;
    var points = generateTrackLoop(track, pixelWidth, pixelHeight);
    var pixels = createPixelGrid(pixelWidth, pixelHeight);
    rasterizeTrack(points, pixels, pixelWidth, pixelHeight);
    markInfield(pixels, pixelWidth, pixelHeight);
    renderHalfBlockMap(pixels, mapX + 2, mapY + 1, pixelWidth, pixelHeight, theme);
    drawStartFinishHiRes(points, mapX + 2, mapY + 1, pixelWidth, pixelHeight, theme);
}
function createPixelGrid(width, height) {
    var grid = [];
    for (var y = 0; y < height; y++) {
        grid[y] = [];
        for (var x = 0; x < width; x++) {
            grid[y][x] = 0;
        }
    }
    return grid;
}
function markInfield(pixels, width, height) {
    var OUTSIDE_MARKER = -1;
    var queue = [];
    for (var x = 0; x < width; x++) {
        if (pixels[0][x] === 0) {
            pixels[0][x] = OUTSIDE_MARKER;
            queue.push({ x: x, y: 0 });
        }
        if (pixels[height - 1][x] === 0) {
            pixels[height - 1][x] = OUTSIDE_MARKER;
            queue.push({ x: x, y: height - 1 });
        }
    }
    for (var y = 0; y < height; y++) {
        if (pixels[y][0] === 0) {
            pixels[y][0] = OUTSIDE_MARKER;
            queue.push({ x: 0, y: y });
        }
        if (pixels[y][width - 1] === 0) {
            pixels[y][width - 1] = OUTSIDE_MARKER;
            queue.push({ x: width - 1, y: y });
        }
    }
    while (queue.length > 0) {
        var p = queue.shift();
        if (!p)
            break;
        var neighbors = [
            { x: p.x - 1, y: p.y },
            { x: p.x + 1, y: p.y },
            { x: p.x, y: p.y - 1 },
            { x: p.x, y: p.y + 1 }
        ];
        for (var i = 0; i < neighbors.length; i++) {
            var n = neighbors[i];
            if (n.x >= 0 && n.x < width && n.y >= 0 && n.y < height) {
                if (pixels[n.y][n.x] === 0) {
                    pixels[n.y][n.x] = OUTSIDE_MARKER;
                    queue.push(n);
                }
            }
        }
    }
    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            if (pixels[y][x] === OUTSIDE_MARKER) {
                pixels[y][x] = 0;
            }
            else if (pixels[y][x] === 0) {
                pixels[y][x] = 3;
            }
        }
    }
}
function rasterizeTrack(points, pixels, width, height) {
    if (points.length < 2)
        return;
    var roadWidth = 2;
    var edgeWidth = 1;
    for (var i = 0; i < points.length - 1; i++) {
        var x1 = points[i].x;
        var y1 = points[i].y;
        var x2 = points[i + 1].x;
        var y2 = points[i + 1].y;
        var dx = x2 - x1;
        var dy = y2 - y1;
        var len = Math.sqrt(dx * dx + dy * dy);
        if (len === 0)
            continue;
        var perpX = -dy / len;
        var perpY = dx / len;
        var steps = Math.max(Math.ceil(len), 1);
        for (var s = 0; s <= steps; s++) {
            var t = s / steps;
            var cx = x1 + dx * t;
            var cy = y1 + dy * t;
            for (var w = -(roadWidth + edgeWidth); w <= (roadWidth + edgeWidth); w++) {
                var px = Math.floor(cx + perpX * w);
                var py = Math.floor(cy + perpY * w);
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    if (Math.abs(w) <= roadWidth) {
                        pixels[py][px] = 2;
                    }
                    else if (pixels[py][px] === 0) {
                        pixels[py][px] = 1;
                    }
                }
            }
        }
    }
}
function renderHalfBlockMap(pixels, screenX, screenY, width, height, theme) {
    var charRows = Math.floor(height / 2);
    function getPixelColor(pixelType) {
        switch (pixelType) {
            case 0: return theme.offroad.groundColor.fg;
            case 1: return theme.road.edge.fg;
            case 2: return theme.road.surface.fg;
            case 3: return BG_BLACK;
            default: return theme.offroad.groundColor.fg;
        }
    }
    function isFilledPixel(pixelType) {
        return pixelType === 1 || pixelType === 2;
    }
    for (var charRow = 0; charRow < charRows; charRow++) {
        var topPixelY = charRow * 2;
        var bottomPixelY = charRow * 2 + 1;
        for (var col = 0; col < width; col++) {
            var topPixel = (topPixelY < height) ? pixels[topPixelY][col] : 0;
            var bottomPixel = (bottomPixelY < height) ? pixels[bottomPixelY][col] : 0;
            console.gotoxy(screenX + col, screenY + charRow);
            var topFilled = isFilledPixel(topPixel);
            var bottomFilled = isFilledPixel(bottomPixel);
            if (topFilled && bottomFilled) {
                var pixelType = (topPixel === 2 || bottomPixel === 2) ? 2 : 1;
                console.attributes = getPixelColor(pixelType);
                console.print(GLYPH.FULL_BLOCK);
            }
            else if (topFilled) {
                var topColor = getPixelColor(topPixel);
                var bottomColor = getPixelColor(bottomPixel);
                console.attributes = topColor + (bottomColor << 4);
                console.print(GLYPH.UPPER_HALF);
            }
            else if (bottomFilled) {
                var topColor = getPixelColor(topPixel);
                var bottomColor = getPixelColor(bottomPixel);
                console.attributes = bottomColor + (topColor << 4);
                console.print(GLYPH.LOWER_HALF);
            }
            else {
                if (topPixel === 3 || bottomPixel === 3) {
                    console.attributes = BLACK;
                    console.print(' ');
                }
                else {
                    console.attributes = theme.offroad.groundColor.fg;
                    console.print(GLYPH.LIGHT_SHADE);
                }
            }
        }
    }
}
function generateTrackLoop(track, width, height) {
    var sections = track.sections;
    var difficulty = track.difficulty || 3;
    var trackId = track.id || '';
    var totalLength = 0;
    var totalCurve = 0;
    var hasSCurves = false;
    var sharpestCurve = 0;
    if (sections && sections.length > 0) {
        for (var i = 0; i < sections.length; i++) {
            var section = sections[i];
            totalLength += section.length || 0;
            if (section.type === 'curve') {
                totalCurve += Math.abs(section.curve || 0) * (section.length || 1);
                if (Math.abs(section.curve || 0) > sharpestCurve) {
                    sharpestCurve = Math.abs(section.curve || 0);
                }
            }
            if (section.type === 's_curve')
                hasSCurves = true;
        }
    }
    var points = [];
    var cx = width / 2;
    var cy = height / 2;
    var rx = (width - 8) / 2;
    var ry = (height - 6) / 2;
    var numPoints = 80;
    var complexity = Math.min(difficulty, 5);
    var wobbleFreq = 2 + complexity;
    var wobbleAmp = 0.05 + (sharpestCurve * 0.15);
    if (trackId.indexOf('figure') >= 0 || (hasSCurves && difficulty >= 4)) {
        for (var i = 0; i <= numPoints; i++) {
            var t = (i / numPoints) * Math.PI * 2;
            var fx = Math.sin(t);
            var fy = Math.sin(t) * Math.cos(t);
            points.push({
                x: cx + fx * rx,
                y: cy + fy * ry * 1.5
            });
        }
    }
    else {
        for (var i = 0; i <= numPoints; i++) {
            var angle = (i / numPoints) * Math.PI * 2;
            var wobble = Math.sin(angle * wobbleFreq) * wobbleAmp;
            var wobble2 = Math.cos(angle * (wobbleFreq + 1)) * wobbleAmp * 0.5;
            points.push({
                x: cx + Math.cos(angle) * rx * (1 + wobble),
                y: cy + Math.sin(angle) * ry * (1 + wobble2)
            });
        }
    }
    return points;
}
function generatePathFromSections(sections) {
    var totalIntendedCurve = 0;
    for (var i = 0; i < sections.length; i++) {
        var section = sections[i];
        var segmentCount = section.length || 10;
        switch (section.type) {
            case 'curve':
                totalIntendedCurve += (section.curve || 0) * segmentCount;
                break;
            case 'ease_in':
                totalIntendedCurve += (section.targetCurve || 0.5) * segmentCount * 0.5;
                break;
            case 'ease_out':
                totalIntendedCurve += (section.targetCurve || 0.5) * segmentCount * 0.5;
                break;
            case 's_curve':
                break;
        }
    }
    var curveScale = 0.1;
    if (Math.abs(totalIntendedCurve) > 0.1) {
        curveScale = (Math.PI * 2) / Math.abs(totalIntendedCurve);
    }
    var direction = totalIntendedCurve >= 0 ? 1 : -1;
    curveScale = Math.abs(curveScale) * direction;
    var points = [];
    var x = 0;
    var y = 0;
    var heading = 0;
    var currentCurve = 0;
    var stepSize = 1.0;
    points.push({ x: x, y: y });
    for (var i = 0; i < sections.length; i++) {
        var section = sections[i];
        var segmentCount = section.length || 10;
        switch (section.type) {
            case 'straight':
                for (var s = 0; s < segmentCount; s++) {
                    x += Math.cos(heading) * stepSize;
                    y += Math.sin(heading) * stepSize;
                    points.push({ x: x, y: y });
                }
                currentCurve = 0;
                break;
            case 'curve':
                var curvature = (section.curve || 0) * curveScale;
                for (var s = 0; s < segmentCount; s++) {
                    heading += curvature;
                    x += Math.cos(heading) * stepSize;
                    y += Math.sin(heading) * stepSize;
                    points.push({ x: x, y: y });
                }
                currentCurve = section.curve || 0;
                break;
            case 'ease_in':
                var targetCurve = (section.targetCurve || 0.5) * curveScale;
                for (var s = 0; s < segmentCount; s++) {
                    var t = s / segmentCount;
                    var easedCurve = currentCurve * curveScale + (targetCurve - currentCurve * curveScale) * t;
                    heading += easedCurve;
                    x += Math.cos(heading) * stepSize;
                    y += Math.sin(heading) * stepSize;
                    points.push({ x: x, y: y });
                }
                currentCurve = section.targetCurve || 0.5;
                break;
            case 'ease_out':
                var startCurve = currentCurve * curveScale;
                for (var s = 0; s < segmentCount; s++) {
                    var t = s / segmentCount;
                    var easedCurve = startCurve * (1 - t);
                    heading += easedCurve;
                    x += Math.cos(heading) * stepSize;
                    y += Math.sin(heading) * stepSize;
                    points.push({ x: x, y: y });
                }
                currentCurve = 0;
                break;
            case 's_curve':
                var halfLen = Math.floor(segmentCount / 2);
                var sCurve = 0.06 * curveScale;
                for (var s = 0; s < halfLen; s++) {
                    heading += sCurve;
                    x += Math.cos(heading) * stepSize;
                    y += Math.sin(heading) * stepSize;
                    points.push({ x: x, y: y });
                }
                for (var s = 0; s < halfLen; s++) {
                    heading -= sCurve;
                    x += Math.cos(heading) * stepSize;
                    y += Math.sin(heading) * stepSize;
                    points.push({ x: x, y: y });
                }
                break;
        }
    }
    var startX = points[0].x;
    var startY = points[0].y;
    var endX = points[points.length - 1].x;
    var endY = points[points.length - 1].y;
    var closeSteps = 15;
    for (var s = 1; s <= closeSteps; s++) {
        var t = s / closeSteps;
        var smoothT = t * t * (3 - 2 * t);
        points.push({
            x: endX + (startX - endX) * smoothT,
            y: endY + (startY - endY) * smoothT
        });
    }
    return points;
}
function normalizeAndCenterPath(points, width, height) {
    if (points.length === 0)
        return points;
    var minX = points[0].x, maxX = points[0].x;
    var minY = points[0].y, maxY = points[0].y;
    for (var i = 1; i < points.length; i++) {
        if (points[i].x < minX)
            minX = points[i].x;
        if (points[i].x > maxX)
            maxX = points[i].x;
        if (points[i].y < minY)
            minY = points[i].y;
        if (points[i].y > maxY)
            maxY = points[i].y;
    }
    var pathWidth = maxX - minX;
    var pathHeight = maxY - minY;
    var padding = 3;
    var availWidth = width - padding * 2;
    var availHeight = height - padding * 2;
    var scaleX = pathWidth > 0 ? availWidth / pathWidth : 1;
    var scaleY = pathHeight > 0 ? availHeight / pathHeight : 1;
    var scale = Math.min(scaleX, scaleY);
    var scaledWidth = pathWidth * scale;
    var scaledHeight = pathHeight * scale;
    var offsetX = padding + (availWidth - scaledWidth) / 2;
    var offsetY = padding + (availHeight - scaledHeight) / 2;
    var result = [];
    for (var i = 0; i < points.length; i++) {
        result.push({
            x: (points[i].x - minX) * scale + offsetX,
            y: (points[i].y - minY) * scale + offsetY
        });
    }
    return result;
}
function generateOvalTrack(width, height) {
    var points = [];
    var cx = width / 2;
    var cy = height / 2;
    var rx = (width - 8) / 2;
    var ry = (height - 4) / 2;
    var numPoints = 60;
    for (var i = 0; i <= numPoints; i++) {
        var angle = (i / numPoints) * Math.PI * 2;
        points.push({
            x: cx + Math.cos(angle) * rx,
            y: cy + Math.sin(angle) * ry
        });
    }
    return points;
}
function drawTrackSurface(_points, _x, _y, _width, _height, _theme) {
}
function drawStartFinishHiRes(points, screenX, screenY, _pixelWidth, pixelHeight, theme) {
    if (points.length < 2)
        return;
    var startPixelX = Math.floor(points[0].x);
    var startPixelY = Math.floor(points[0].y);
    var charX = startPixelX;
    var charY = Math.floor(startPixelY / 2);
    var maxCharY = Math.floor(pixelHeight / 2) - 1;
    charY = Math.max(0, Math.min(maxCharY, charY));
    console.gotoxy(screenX + charX, screenY + charY);
    console.attributes = WHITE;
    console.print(GLYPH.CHECKER);
    console.gotoxy(screenX + charX + 1, screenY + charY);
    console.attributes = theme.sun.color.fg;
    console.print('S');
}
function drawStartFinish(_points, _x, _y, _width, _height, _theme) {
}
function drawTrackRoute(_track) {
}
function drawControls(state) {
    console.gotoxy(1, 23);
    console.attributes = LIGHTMAGENTA;
    console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
    console.gotoxy(1, 24);
    console.attributes = LIGHTGRAY;
    if (state.mode === 'circuit') {
        console.print('  ');
        console.attributes = WHITE;
        console.print(String.fromCharCode(24) + '/' + String.fromCharCode(25));
        console.attributes = LIGHTGRAY;
        console.print(' Select  ');
        console.attributes = WHITE;
        console.print('ENTER');
        console.attributes = LIGHTGRAY;
        console.print(' Open  ');
        console.attributes = WHITE;
        console.print('H');
        console.attributes = LIGHTGRAY;
        console.print(' Scores  ');
        console.attributes = WHITE;
        console.print('Q');
        console.attributes = LIGHTGRAY;
        console.print(' Quit  ');
        console.attributes = DARKGRAY;
        console.print('?');
        console.attributes = DARKGRAY;
        console.print(' ???');
    }
    else {
        console.print('  ');
        console.attributes = WHITE;
        console.print(String.fromCharCode(24) + '/' + String.fromCharCode(25));
        console.attributes = LIGHTGRAY;
        console.print(' Select  ');
        console.attributes = WHITE;
        console.print('ENTER');
        console.attributes = LIGHTGRAY;
        console.print(' Race  ');
        console.attributes = WHITE;
        console.print('H');
        console.attributes = LIGHTGRAY;
        console.print(' Scores  ');
        console.attributes = WHITE;
        console.print(String.fromCharCode(27));
        console.attributes = LIGHTGRAY;
        console.print(' Back  ');
        console.attributes = WHITE;
        console.print('Q');
        console.attributes = LIGHTGRAY;
        console.print(' Quit  ');
        console.attributes = DARKGRAY;
        console.print('?');
        console.attributes = DARKGRAY;
        console.print(' ???');
    }
    console.gotoxy(1, 25);
    console.attributes = LIGHTMAGENTA;
    console.print(repeatChar(GLYPH.BOX_H, SCREEN_WIDTH));
}
function repeatChar(char, count) {
    var result = '';
    for (var i = 0; i < count; i++) {
        result += char;
    }
    return result;
}
function formatTime(seconds) {
    var mins = Math.floor(seconds / 60);
    var secs = Math.floor(seconds % 60);
    return mins + ':' + (secs < 10 ? '0' : '') + secs;
}
function padRight(str, len) {
    while (str.length < len) {
        str += ' ';
    }
    return str.substring(0, len);
}
"use strict";
var CarSelector = {
    show: function (composer) {
        var cars = getAllCars();
        var selectedCarIndex = 0;
        var selectedColorIndex = 0;
        for (var i = 0; i < cars.length; i++) {
            if (cars[i].unlocked) {
                selectedCarIndex = i;
                break;
            }
        }
        var running = true;
        var confirmed = false;
        while (running) {
            var currentCar = cars[selectedCarIndex];
            var availableColors = [];
            for (var c = 0; c < currentCar.availableColors.length; c++) {
                var col = getCarColor(currentCar.availableColors[c]);
                if (col)
                    availableColors.push(col);
            }
            if (selectedColorIndex >= availableColors.length) {
                selectedColorIndex = 0;
            }
            var currentColor = availableColors[selectedColorIndex];
            this.render(composer, cars, selectedCarIndex, currentColor, selectedColorIndex, availableColors.length);
            var key = console.inkey(K_UPPER, 100);
            if (key === '')
                continue;
            switch (key) {
                case 'W':
                case KEY_UP:
                    selectedCarIndex--;
                    if (selectedCarIndex < 0)
                        selectedCarIndex = cars.length - 1;
                    var attempts = 0;
                    while (!cars[selectedCarIndex].unlocked && attempts < cars.length) {
                        selectedCarIndex--;
                        if (selectedCarIndex < 0)
                            selectedCarIndex = cars.length - 1;
                        attempts++;
                    }
                    selectedColorIndex = 0;
                    break;
                case 'S':
                case KEY_DOWN:
                    selectedCarIndex++;
                    if (selectedCarIndex >= cars.length)
                        selectedCarIndex = 0;
                    var attempts = 0;
                    while (!cars[selectedCarIndex].unlocked && attempts < cars.length) {
                        selectedCarIndex++;
                        if (selectedCarIndex >= cars.length)
                            selectedCarIndex = 0;
                        attempts++;
                    }
                    selectedColorIndex = 0;
                    break;
                case 'A':
                case KEY_LEFT:
                    selectedColorIndex--;
                    if (selectedColorIndex < 0)
                        selectedColorIndex = availableColors.length - 1;
                    break;
                case 'D':
                case KEY_RIGHT:
                    selectedColorIndex++;
                    if (selectedColorIndex >= availableColors.length)
                        selectedColorIndex = 0;
                    break;
                case '\r':
                case '\n':
                case ' ':
                    if (currentCar.unlocked) {
                        confirmed = true;
                        running = false;
                    }
                    break;
                case 'Q':
                case '\x1b':
                    running = false;
                    break;
            }
        }
        if (confirmed) {
            var car = cars[selectedCarIndex];
            var colors = [];
            for (var c = 0; c < car.availableColors.length; c++) {
                var col = getCarColor(car.availableColors[c]);
                if (col)
                    colors.push(col);
            }
            return {
                carId: car.id,
                colorId: colors[selectedColorIndex].id,
                confirmed: true
            };
        }
        else {
            return {
                carId: 'sports',
                colorId: 'yellow',
                confirmed: false
            };
        }
    },
    render: function (composer, cars, selectedIndex, currentColor, colorIndex, totalColors) {
        composer.clear();
        var width = 80;
        var titleAttr = makeAttr(LIGHTCYAN, BG_BLACK);
        var title = '=== SELECT YOUR VEHICLE ===';
        composer.writeString(Math.floor((width - title.length) / 2), 1, title, titleAttr);
        var instructAttr = makeAttr(DARKGRAY, BG_BLACK);
        composer.writeString(5, 22, 'W/S: Car  A/D: Color  ENTER: Select  Q: Cancel', instructAttr);
        var listX = 3;
        var listY = 4;
        var listAttr = makeAttr(LIGHTGRAY, BG_BLACK);
        var selectedAttr = makeAttr(WHITE, BG_BLUE);
        var lockedAttr = makeAttr(DARKGRAY, BG_BLACK);
        for (var i = 0; i < cars.length; i++) {
            var car = cars[i];
            var y = listY + i * 2;
            var attr = (i === selectedIndex) ? selectedAttr : (car.unlocked ? listAttr : lockedAttr);
            var indicator = (i === selectedIndex) ? '>' : ' ';
            composer.writeString(listX, y, indicator, attr);
            var name = car.unlocked ? car.name : '??? LOCKED ???';
            composer.writeString(listX + 2, y, name, attr);
            if (!car.unlocked) {
                composer.writeString(listX + 2, y + 1, car.unlockHint || 'Complete challenges', lockedAttr);
            }
        }
        var detailX = 40;
        var detailY = 4;
        var currentCar = cars[selectedIndex];
        if (currentCar.unlocked) {
            var nameAttr = makeAttr(YELLOW, BG_BLACK);
            composer.writeString(detailX, detailY, currentCar.name, nameAttr);
            var descAttr = makeAttr(LIGHTGRAY, BG_BLACK);
            composer.writeString(detailX, detailY + 1, currentCar.description, descAttr);
            var statsY = detailY + 3;
            var statLabelAttr = makeAttr(CYAN, BG_BLACK);
            var statBarAttr = makeAttr(LIGHTGREEN, BG_BLACK);
            var statBarEmptyAttr = makeAttr(DARKGRAY, BG_BLACK);
            composer.writeString(detailX, statsY, 'TOP SPEED:', statLabelAttr);
            this.renderStatBar(composer, detailX + 11, statsY, currentCar.stats.topSpeed, statBarAttr, statBarEmptyAttr);
            composer.writeString(detailX, statsY + 1, 'ACCEL:', statLabelAttr);
            this.renderStatBar(composer, detailX + 11, statsY + 1, currentCar.stats.acceleration, statBarAttr, statBarEmptyAttr);
            composer.writeString(detailX, statsY + 2, 'HANDLING:', statLabelAttr);
            this.renderStatBar(composer, detailX + 11, statsY + 2, currentCar.stats.handling, statBarAttr, statBarEmptyAttr);
            var colorY = statsY + 5;
            var colorLabelAttr = makeAttr(LIGHTMAGENTA, BG_BLACK);
            composer.writeString(detailX, colorY, 'COLOR: < ' + currentColor.name + ' >', colorLabelAttr);
            composer.writeString(detailX, colorY + 1, '(' + (colorIndex + 1) + '/' + totalColors + ')', instructAttr);
            var previewY = colorY + 3;
            this.renderCarPreview(composer, detailX + 8, previewY, currentCar.bodyStyle, currentColor);
        }
        else {
            var lockedMsgAttr = makeAttr(RED, BG_BLACK);
            composer.writeString(detailX, detailY, 'VEHICLE LOCKED', lockedMsgAttr);
            composer.writeString(detailX, detailY + 2, currentCar.unlockHint || 'Complete challenges to unlock', lockedAttr);
        }
        this.outputToConsole(composer);
    },
    outputToConsole: function (composer) {
        console.clear(BG_BLACK, false);
        var buffer = composer.getBuffer();
        for (var y = 0; y < buffer.length; y++) {
            console.gotoxy(1, y + 1);
            for (var x = 0; x < buffer[y].length; x++) {
                var cell = buffer[y][x];
                console.attributes = cell.attr;
                console.print(cell.char);
            }
        }
    },
    renderStatBar: function (composer, x, y, value, filledAttr, emptyAttr) {
        var filled = Math.round(value * 5);
        filled = Math.max(1, Math.min(10, filled));
        for (var i = 0; i < 10; i++) {
            var char = (i < filled) ? GLYPH.FULL_BLOCK : GLYPH.LIGHT_SHADE;
            var attr = (i < filled) ? filledAttr : emptyAttr;
            composer.setCell(x + i, y, char, attr);
        }
    },
    renderCarPreview: function (composer, x, y, bodyStyle, color) {
        var sprite = getPlayerCarSprite(bodyStyle, color.id, false);
        var variant = sprite.variants[0];
        for (var row = 0; row < variant.length; row++) {
            for (var col = 0; col < variant[row].length; col++) {
                var cell = variant[row][col];
                if (cell) {
                    composer.setCell(x + col, y + row, cell.char, cell.attr);
                }
            }
        }
        var labelAttr = makeAttr(DARKGRAY, BG_BLACK);
        composer.writeString(x - 2, y + 3, '(preview)', labelAttr);
    }
};
"use strict";
function showCupStandings(cupManager, isPreRace) {
    var state = cupManager.getState();
    if (!state)
        return;
    var screenWidth = 80;
    var screenHeight = 24;
    console.clear(BG_BLACK, false);
    console.attributes = WHITE | BG_BLACK;
    var title = "=== " + state.definition.name.toUpperCase() + " ===";
    console.gotoxy(Math.floor((screenWidth - title.length) / 2), 2);
    console.attributes = YELLOW | BG_BLACK;
    console.print(title);
    var raceInfo;
    if (isPreRace) {
        if (cupManager.getCurrentRaceNumber() === 1) {
            raceInfo = "Race " + cupManager.getCurrentRaceNumber() + " of " + cupManager.getTotalRaces();
        }
        else {
            raceInfo = "Next: Race " + cupManager.getCurrentRaceNumber() + " of " + cupManager.getTotalRaces();
        }
    }
    else {
        if (state.isComplete) {
            raceInfo = "FINAL STANDINGS";
        }
        else {
            raceInfo = "After Race " + (cupManager.getCurrentRaceNumber() - 1) + " of " + cupManager.getTotalRaces();
        }
    }
    console.gotoxy(Math.floor((screenWidth - raceInfo.length) / 2), 4);
    console.attributes = LIGHTCYAN | BG_BLACK;
    console.print(raceInfo);
    if (isPreRace) {
        var trackId = cupManager.getCurrentTrackId();
        if (trackId) {
            var trackName = getTrackDisplayName(trackId);
            console.gotoxy(Math.floor((screenWidth - trackName.length) / 2), 5);
            console.attributes = WHITE | BG_BLACK;
            console.print(trackName);
        }
    }
    var standings = cupManager.getStandings();
    var tableTop = 7;
    var tableWidth = 50;
    var tableLeft = Math.floor((screenWidth - tableWidth) / 2);
    console.gotoxy(tableLeft, tableTop);
    console.attributes = LIGHTGRAY | BG_BLACK;
    console.print("POS  RACER                    POINTS");
    console.gotoxy(tableLeft, tableTop + 1);
    console.print("------------------------------------");
    for (var i = 0; i < standings.length; i++) {
        var s = standings[i];
        var row = tableTop + 2 + i;
        console.gotoxy(tableLeft, row);
        var posStr = PositionIndicator.getOrdinalSuffix(i + 1);
        posStr = (i + 1) + posStr;
        while (posStr.length < 4)
            posStr += ' ';
        if (s.isPlayer) {
            if (i === 0) {
                console.attributes = LIGHTGREEN | BG_BLACK;
            }
            else if (i < 3) {
                console.attributes = YELLOW | BG_BLACK;
            }
            else {
                console.attributes = LIGHTCYAN | BG_BLACK;
            }
        }
        else {
            if (i === 0) {
                console.attributes = WHITE | BG_BLACK;
            }
            else {
                console.attributes = LIGHTGRAY | BG_BLACK;
            }
        }
        console.print(posStr + " ");
        var name = s.isPlayer ? "YOU" : s.name;
        while (name.length < 24)
            name += ' ';
        console.print(name);
        var pointsStr = s.points.toString();
        while (pointsStr.length < 4)
            pointsStr = ' ' + pointsStr;
        console.print(pointsStr);
        if (s.raceResults.length > 0) {
            console.attributes = DARKGRAY | BG_BLACK;
            console.print("  [");
            for (var r = 0; r < s.raceResults.length; r++) {
                if (r > 0)
                    console.print(",");
                var racePos = s.raceResults[r];
                if (racePos <= 3) {
                    console.attributes = LIGHTGREEN | BG_BLACK;
                }
                else if (racePos <= 5) {
                    console.attributes = YELLOW | BG_BLACK;
                }
                else {
                    console.attributes = LIGHTGRAY | BG_BLACK;
                }
                console.print(racePos.toString());
            }
            console.attributes = DARKGRAY | BG_BLACK;
            console.print("]");
        }
    }
    var refRow = tableTop + 2 + standings.length + 2;
    console.gotoxy(tableLeft, refRow);
    console.attributes = DARKGRAY | BG_BLACK;
    console.print("Points: 1st=15 2nd=12 3rd=10 4th=8 5th=6...");
    var prompt = isPreRace ? "Press ENTER to start race" : "Press ENTER to continue";
    if (state.isComplete) {
        prompt = "Press ENTER to see results";
    }
    console.gotoxy(Math.floor((screenWidth - prompt.length) / 2), screenHeight - 3);
    console.attributes = LIGHTMAGENTA | BG_BLACK;
    console.print(prompt);
    waitForEnter();
}
function getTrackDisplayName(trackId) {
    var parts = trackId.split('_');
    var name = '';
    for (var i = 0; i < parts.length; i++) {
        if (i > 0)
            name += ' ';
        var part = parts[i];
        name += part.charAt(0).toUpperCase() + part.slice(1);
    }
    return name;
}
function waitForEnter() {
    while (true) {
        var key = console.inkey(K_NONE, 100);
        if (key === '\r' || key === '\n')
            break;
        if (key === 'q' || key === 'Q')
            break;
    }
}
function showWinnersCircle(cupManager) {
    var state = cupManager.getState();
    if (!state)
        return;
    var screenWidth = 80;
    var playerWon = cupManager.didPlayerWin();
    var playerPos = cupManager.getPlayerCupPosition();
    console.clear(BG_BLACK, false);
    if (playerWon) {
        console.attributes = YELLOW | BG_BLACK;
        var trophy = [
            "    ___________",
            "   '._==_==_=_.'",
            "   .-\\:      /-.",
            "  | (|:.     |) |",
            "   '-|:.     |-'",
            "     \\::.    /",
            "      '::. .'",
            "        ) (",
            "      _.' '._",
            "     '-------'"
        ];
        var trophyTop = 3;
        for (var t = 0; t < trophy.length; t++) {
            console.gotoxy(Math.floor((screenWidth - trophy[t].length) / 2), trophyTop + t);
            console.print(trophy[t]);
        }
        var winTitle = "=== CHAMPION! ===";
        console.gotoxy(Math.floor((screenWidth - winTitle.length) / 2), trophyTop + trophy.length + 2);
        console.attributes = LIGHTGREEN | BG_BLACK;
        console.print(winTitle);
        var cupName = state.definition.name + " Winner!";
        console.gotoxy(Math.floor((screenWidth - cupName.length) / 2), trophyTop + trophy.length + 4);
        console.attributes = WHITE | BG_BLACK;
        console.print(cupName);
    }
    else {
        var posStr = playerPos + PositionIndicator.getOrdinalSuffix(playerPos);
        var resultTitle = "=== " + posStr + " PLACE ===";
        console.gotoxy(Math.floor((screenWidth - resultTitle.length) / 2), 5);
        if (playerPos <= 3) {
            console.attributes = YELLOW | BG_BLACK;
        }
        else {
            console.attributes = LIGHTGRAY | BG_BLACK;
        }
        console.print(resultTitle);
        var cupName2 = state.definition.name + " Complete";
        console.gotoxy(Math.floor((screenWidth - cupName2.length) / 2), 7);
        console.attributes = WHITE | BG_BLACK;
        console.print(cupName2);
        if (playerPos <= 3) {
            var podiumMsg = "You made the podium!";
            console.gotoxy(Math.floor((screenWidth - podiumMsg.length) / 2), 9);
            console.attributes = LIGHTCYAN | BG_BLACK;
            console.print(podiumMsg);
        }
        else {
            var tryAgain = "Better luck next time!";
            console.gotoxy(Math.floor((screenWidth - tryAgain.length) / 2), 9);
            console.attributes = LIGHTGRAY | BG_BLACK;
            console.print(tryAgain);
        }
    }
    var statsTop = 16;
    console.gotoxy(Math.floor((screenWidth - 30) / 2), statsTop);
    console.attributes = LIGHTGRAY | BG_BLACK;
    console.print("Total Points: " + cupManager.getPlayerPoints());
    console.gotoxy(Math.floor((screenWidth - 30) / 2), statsTop + 1);
    console.print("Circuit Time: " + formatCupTime(state.totalTime));
    console.gotoxy(Math.floor((screenWidth - 30) / 2), statsTop + 2);
    console.print("Best Laps Sum: " + formatCupTime(state.totalBestLaps));
    var prompt = "Press ENTER to continue";
    console.gotoxy(Math.floor((screenWidth - prompt.length) / 2), 22);
    console.attributes = LIGHTMAGENTA | BG_BLACK;
    console.print(prompt);
    waitForEnter();
}
function formatCupTime(seconds) {
    var mins = Math.floor(seconds / 60);
    var secs = seconds % 60;
    var minsStr = mins.toString();
    var secsStr = secs.toFixed(2);
    if (secs < 10)
        secsStr = '0' + secsStr;
    return minsStr + ':' + secsStr;
}
"use strict";
if (typeof console === 'undefined' || console === null) {
    print("ERROR: OutRun ANSI must be run from a Synchronet BBS terminal session.");
    print("This game cannot run directly with jsexec.");
    print("");
    print("To test, you need to either:");
    print("  1. Run from an actual BBS login session");
    print("  2. Use a terminal emulator that connects to your BBS");
    exit(1);
}
function showTitleScreen() {
    console.clear(BG_BLACK, false);
    var titleFile = "";
    var assetsDir = js.exec_dir + "assets/";
    var f = new File(assetsDir + "title.bin");
    if (f.exists) {
        titleFile = assetsDir + "title.bin";
    }
    else {
        f = new File(assetsDir + "title.ans");
        if (f.exists) {
            titleFile = assetsDir + "title.ans";
        }
    }
    if (titleFile !== "") {
        try {
            load('frame.js');
            var titleFrame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK);
            titleFrame.open();
            titleFrame.load(titleFile);
            titleFrame.draw();
            titleFrame.close();
        }
        catch (e) {
            logError("Error loading custom title: " + e);
        }
    }
    else {
        console.attributes = LIGHTMAGENTA;
        var title = [
            "",
            "     .d88b.  db    db d888888b d8888b. db    db d8b   db",
            "    .8P  Y8. 88    88 `~~88~~' 88  `8D 88    88 888o  88",
            "    88    88 88    88    88    88oobY' 88    88 88V8o 88",
            "    88    88 88    88    88    88`8b   88    88 88 V8o88",
            "    `8b  d8' 88b  d88    88    88 `88. 88b  d88 88  V888",
            "     `Y88P'  ~Y8888P'    YP    88   YD ~Y8888P' VP   V8P",
            ""
        ];
        for (var i = 0; i < title.length; i++) {
            console.print(title[i] + "\r\n");
        }
        console.attributes = CYAN;
        console.print("              =========================================\r\n");
        console.attributes = LIGHTCYAN;
        console.print("                   A N S I   S Y N T H W A V E\r\n");
        console.print("                        R A C E R\r\n");
        console.attributes = CYAN;
        console.print("              =========================================\r\n");
        console.print("\r\n");
        console.attributes = DARKGRAY;
        console.print("         /     |     \\         /     |     \\\r\n");
        console.print("        /      |      \\       /      |      \\\r\n");
        console.attributes = CYAN;
        console.print("    ===/=======+=======\\=====/=======+=======\\===\r\n");
        console.attributes = DARKGRAY;
        console.print("      /        |        \\   /        |        \\\r\n");
        console.print("\r\n");
        console.attributes = WHITE;
        console.print("                    Controls:\r\n");
        console.attributes = LIGHTGRAY;
        console.print("        W/Up = Accelerate   A/Left = Steer Left\r\n");
        console.print("        S/Dn = Brake        D/Right = Steer Right\r\n");
        console.print("        SPACE = Use Item    P = Pause\r\n");
        console.print("\r\n");
        console.attributes = YELLOW;
        console.print("              Press any key to start racing...\r\n");
        console.print("                     Q to quit\r\n");
        console.print("\r\n");
        console.attributes = DARKGRAY;
        console.print("     Version 0.1.0 (Iteration 0) - Bootstrap Build\r\n");
        console.attributes = LIGHTGRAY;
    }
}
function showExitScreen() {
    console.clear(BG_BLACK, false);
    var exitFile = "";
    var assetsDir = js.exec_dir + "assets/";
    var f = new File(assetsDir + "exit.bin");
    if (f.exists) {
        exitFile = assetsDir + "exit.bin";
    }
    else {
        f = new File(assetsDir + "exit.ans");
        if (f.exists) {
            exitFile = assetsDir + "exit.ans";
        }
    }
    if (exitFile !== "") {
        try {
            load('frame.js');
            var exitFrame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK);
            exitFrame.open();
            exitFrame.load(exitFile);
            exitFrame.draw();
            exitFrame.close();
        }
        catch (e) {
            logError("Error loading custom exit screen: " + e);
            console.attributes = LIGHTGRAY;
            console.print("Thanks for playing SynthKart!\r\n");
        }
    }
    else {
        console.attributes = LIGHTGRAY;
        console.print("Thanks for playing SynthKart!\r\n");
    }
    console.pause();
}
function waitForTitleInput() {
    while (true) {
        var key = console.inkey(K_UPPER, 1000);
        if (key !== '') {
            if (key === 'Q') {
                return false;
            }
            return true;
        }
    }
}
function showRaceEndScreen() {
    console.clear(BG_BLACK, false);
    console.attributes = LIGHTMAGENTA;
    console.print("\r\n\r\n");
    console.print("  ========================================\r\n");
    console.attributes = LIGHTCYAN;
    console.print("             RACE COMPLETE!\r\n");
    console.attributes = LIGHTMAGENTA;
    console.print("  ========================================\r\n");
    console.print("\r\n");
    console.attributes = LIGHTGRAY;
    console.print("         Thanks for racing!\r\n");
    console.print("\r\n");
    console.attributes = YELLOW;
    console.print("    Press any key to continue...\r\n");
    console.print("\r\n");
    console.attributes = LIGHTGRAY;
    console.inkey(K_NONE, 30000);
}
function main() {
    debugLog.separator("GAME START");
    debugLog.info("Entering main()");
    load('json-db.js');
    var highScoreManager = new HighScoreManager();
    var cupManager = new CupManager();
    try {
        var keepPlaying = true;
        while (keepPlaying) {
            debugLog.info("Showing title screen");
            showTitleScreen();
            if (!waitForTitleInput()) {
                debugLog.info("User quit from title screen");
                keepPlaying = false;
                break;
            }
            debugLog.info("Showing track selector");
            var trackSelection = showTrackSelector(highScoreManager);
            if (!trackSelection.selected || !trackSelection.track) {
                debugLog.info("User went back from track selection");
                continue;
            }
            debugLog.info("Selected track: " + trackSelection.track.name);
            debugLog.info("Showing car selector");
            var carComposer = new SceneComposer(80, 24);
            var carSelection = CarSelector.show(carComposer);
            if (!carSelection.confirmed) {
                debugLog.info("User cancelled car selection");
                continue;
            }
            debugLog.info("Selected car: " + carSelection.carId + " color: " + carSelection.colorId);
            if (trackSelection.isCircuitMode && trackSelection.circuitTracks) {
                runCupMode(trackSelection.circuitTracks, cupManager, highScoreManager, trackSelection.circuitId || 'custom_cup', trackSelection.circuitName || 'Circuit Cup', carSelection);
            }
            else {
                runSingleRace(trackSelection.track, highScoreManager, carSelection);
            }
            debugLog.info("Returning to splash screen");
        }
        showExitScreen();
    }
    catch (e) {
        debugLog.separator("FATAL ERROR");
        debugLog.exception("Uncaught exception in main()", e);
        console.clear(BG_BLACK, false);
        console.attributes = LIGHTRED;
        console.print("An error occurred: " + e + "\r\n");
        console.attributes = LIGHTGRAY;
        console.print("\r\nError details written to: outrun_debug.log\r\n");
        console.print("Press any key to exit...\r\n");
        console.inkey(K_NONE);
    }
    finally {
        console.attributes = LIGHTGRAY;
    }
}
function runSingleRace(track, highScoreManager, carSelection) {
    debugLog.separator("GAME INIT");
    var game = new Game(undefined, highScoreManager);
    game.initWithTrack(track, undefined, carSelection ? { carId: carSelection.carId, colorId: carSelection.colorId } : undefined);
    debugLog.separator("GAME LOOP");
    debugLog.info("Entering game loop");
    game.run();
    debugLog.separator("GAME END");
    debugLog.info("Game loop ended");
    game.shutdown();
    showRaceEndScreen();
}
function runCupMode(tracks, cupManager, highScoreManager, circuitId, circuitName, carSelection) {
    debugLog.separator("CUP MODE START");
    debugLog.info("Starting cup with " + tracks.length + " tracks: " + circuitId);
    var aiNames = ['MAX', 'LUNA', 'BLAZE', 'NOVA', 'TURBO', 'DASH', 'FLASH'];
    var cupDef = {
        id: circuitId,
        name: circuitName,
        trackIds: [],
        description: circuitName + ' circuit'
    };
    for (var t = 0; t < tracks.length; t++) {
        cupDef.trackIds.push(tracks[t].id);
    }
    cupManager.startCup(cupDef, aiNames);
    showCupStandings(cupManager, true);
    while (!cupManager.isCupComplete()) {
        var trackId = cupManager.getCurrentTrackId();
        if (!trackId)
            break;
        var track = getTrackDefinitionForCup(tracks, trackId);
        if (!track) {
            debugLog.error("Track not found in cup: " + trackId);
            break;
        }
        debugLog.info("Cup race " + cupManager.getCurrentRaceNumber() + ": " + track.name);
        var game = new Game(undefined, highScoreManager);
        game.initWithTrack(track, undefined, carSelection ? { carId: carSelection.carId, colorId: carSelection.colorId } : undefined);
        game.run();
        var raceResults = game.getFinalRaceResults();
        game.shutdown();
        if (raceResults) {
            cupManager.recordRaceResult(track.id, track.name, raceResults.positions, raceResults.playerTime, raceResults.playerBestLap);
        }
        if (!cupManager.isCupComplete()) {
            showCupStandings(cupManager, true);
        }
    }
    showCupStandings(cupManager, false);
    showWinnersCircle(cupManager);
    var cupState = cupManager.getState();
    if (cupState && highScoreManager) {
        var position = highScoreManager.checkQualification(HighScoreType.CIRCUIT_TIME, cupDef.id, cupState.totalTime);
        if (position > 0) {
            var playerName = "Player";
            try {
                if (typeof user !== 'undefined' && user && user.alias) {
                    playerName = user.alias;
                }
            }
            catch (e) {
            }
            highScoreManager.submitScore(HighScoreType.CIRCUIT_TIME, cupDef.id, playerName, cupState.totalTime, undefined, cupDef.name);
            showHighScoreList(HighScoreType.CIRCUIT_TIME, cupDef.id, "=== CIRCUIT HIGH SCORES ===", cupDef.name, highScoreManager, position);
        }
    }
    cupManager.clear();
    debugLog.separator("CUP MODE END");
}
function getTrackDefinitionForCup(tracks, trackId) {
    for (var i = 0; i < tracks.length; i++) {
        if (tracks[i].id === trackId) {
            return tracks[i];
        }
    }
    return null;
}
main();
