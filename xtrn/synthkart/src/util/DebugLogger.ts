/**
 * DebugLogger - File-based logging for debugging.
 * 
 * Writes to a log file in the game directory, overwriting on each run.
 * Flushes after each write to ensure we capture crashes.
 */

var DEBUG_LOG_FILE = "outrun_debug.log";

/**
 * Repeat a string N times (SpiderMonkey 1.8.5 compatible).
 */
function repeatString(str: string, count: number): string {
  var result = "";
  for (var i = 0; i < count; i++) {
    result += str;
  }
  return result;
}

class DebugLogger {
  private file: any;  // Synchronet File object
  private enabled: boolean;
  private startTime: number;
  private logPath: string;

  constructor() {
    this.file = null;
    this.enabled = false;
    this.startTime = 0;
    this.logPath = "";
  }

  /**
   * Initialize the logger - call this early in startup.
   */
  init(): boolean {
    try {
      // Log file goes in script directory (using relative path)
      this.logPath = DEBUG_LOG_FILE;
      this.startTime = Date.now();

      // Debug: Print where we're trying to write
      if (typeof console !== 'undefined' && console.print) {
        console.print("DEBUG: Log path = " + this.logPath + "\r\n");
      }

      // Open file for writing (overwrites existing)
      this.file = new File(this.logPath);
      if (!this.file.open("w")) {
        // Can't open file - disable logging
        if (typeof console !== 'undefined' && console.print) {
          console.print("DEBUG: Failed to open log file!\r\n");
        }
        this.enabled = false;
        return false;
      }

      this.enabled = true;

      // Write header
      this.writeRaw(repeatString("=", 60));
      this.writeRaw("OutRun ANSI Debug Log");
      this.writeRaw("Started: " + new Date().toISOString());
      this.writeRaw("Log file: " + this.logPath);
      this.writeRaw(repeatString("=", 60));
      this.writeRaw("");

      return true;
    } catch (e) {
      this.enabled = false;
      return false;
    }
  }

  /**
   * Write raw text to log (no timestamp).
   */
  private writeRaw(text: string): void {
    if (!this.enabled || !this.file) return;
    try {
      this.file.writeln(text);
      this.file.flush();  // Flush immediately to capture crashes
    } catch (e) {
      // Silently fail
    }
  }

  /**
   * Get elapsed time since start in seconds.
   */
  private getElapsed(): string {
    var elapsed = (Date.now() - this.startTime) / 1000;
    return elapsed.toFixed(3);
  }

  /**
   * Format and write a log entry.
   */
  private write(level: string, message: string): void {
    if (!this.enabled || !this.file) return;
    var timestamp = "[" + this.getElapsed() + "s]";
    var line = timestamp + " " + level + " " + message;
    this.writeRaw(line);
  }

  /**
   * Log debug message (verbose).
   */
  debug(message: string): void {
    this.write("[DEBUG]", message);
  }

  /**
   * Log info message.
   */
  info(message: string): void {
    this.write("[INFO ]", message);
  }

  /**
   * Log warning message.
   */
  warn(message: string): void {
    this.write("[WARN ]", message);
  }

  /**
   * Log error message.
   */
  error(message: string): void {
    this.write("[ERROR]", message);
  }

  /**
   * Log an exception/error object with stack trace if available.
   */
  exception(message: string, error: any): void {
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
  }

  /**
   * Log game state for debugging.
   */
  logState(label: string, state: any): void {
    this.write("[STATE]", label);
    try {
      var json = JSON.stringify(state, null, 2);
      var lines = json.split("\n");
      for (var i = 0; i < lines.length; i++) {
        this.writeRaw("  " + lines[i]);
      }
    } catch (e) {
      this.writeRaw("  (could not serialize state)");
    }
  }

  /**
   * Log vehicle state.
   */
  logVehicle(vehicle: IVehicle): void {
    this.debug("Vehicle: playerX=" + vehicle.playerX.toFixed(3) + 
               " trackZ=" + vehicle.trackZ.toFixed(1) +
               " speed=" + vehicle.speed.toFixed(1) +
               " offRoad=" + vehicle.isOffRoad +
               " crashed=" + vehicle.isCrashed +
               " lap=" + vehicle.lap);
  }

  /**
   * Log input state.
   */
  logInput(key: string, action: number): void {
    var keyStr = key;
    if (key === " ") keyStr = "SPACE";
    else if (key === "\r") keyStr = "ENTER";
    else if (key.charCodeAt(0) < 32) keyStr = "0x" + key.charCodeAt(0).toString(16);
    
    this.debug("Input: key='" + keyStr + "' action=" + action);
  }

  /**
   * Log a separator line.
   */
  separator(label?: string): void {
    if (label) {
      this.writeRaw("");
      this.writeRaw("--- " + label + " " + repeatString("-", 50 - label.length));
    } else {
      this.writeRaw(repeatString("-", 60));
    }
  }

  /**
   * Close the log file.
   */
  close(): void {
    if (this.file) {
      this.writeRaw("");
      this.writeRaw(repeatString("=", 60));
      this.writeRaw("Log ended: " + new Date().toISOString());
      this.writeRaw(repeatString("=", 60));
      try {
        this.file.close();
      } catch (e) {
        // Ignore
      }
      this.file = null;
    }
    this.enabled = false;
  }
}

// Global debug logger instance
var debugLog = new DebugLogger();
