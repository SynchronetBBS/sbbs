/**
 * Logging utilities.
 * Wraps Synchronet's log() function with convenience methods.
 */

enum LogLevel {
  DEBUG = 7,
  INFO = 6,
  NOTICE = 5,
  WARNING = 4,
  ERROR = 3,
  CRITICAL = 2,
  ALERT = 1,
  EMERGENCY = 0
}

var currentLogLevel: LogLevel = LogLevel.INFO;

function setLogLevel(level: LogLevel): void {
  currentLogLevel = level;
}

function logMessage(level: LogLevel, message: string): void {
  if (level <= currentLogLevel) {
    // Use Synchronet's log function if available
    if (typeof log !== 'undefined') {
      log(level, "[OutRun] " + message);
    }
  }
}

function logDebug(message: string): void {
  logMessage(LogLevel.DEBUG, message);
}

function logInfo(message: string): void {
  logMessage(LogLevel.INFO, message);
}

function logWarning(message: string): void {
  logMessage(LogLevel.WARNING, message);
}

function logError(message: string): void {
  logMessage(LogLevel.ERROR, message);
}
