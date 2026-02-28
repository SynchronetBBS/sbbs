/**
 * LapTimer - Lap time tracking and display.
 */

interface LapTimeData {
  currentLap: number;
  totalLaps: number;
  currentLapTime: number;
  bestLapTime: number | null;
  lapTimes: number[];
  totalTime: number;
}

class LapTimer {
  private lapTimes: number[];
  private lapStartTime: number;
  private raceStartTime: number;
  private bestLapTime: number | null;

  constructor() {
    this.lapTimes = [];
    this.lapStartTime = 0;
    this.raceStartTime = 0;
    this.bestLapTime = null;
  }

  /**
   * Start the race timer.
   */
  start(currentTime: number): void {
    this.raceStartTime = currentTime;
    this.lapStartTime = currentTime;
    this.lapTimes = [];
    this.bestLapTime = null;
  }

  /**
   * Record a completed lap.
   */
  completeLap(currentTime: number): number {
    var lapTime = currentTime - this.lapStartTime;
    this.lapTimes.push(lapTime);

    if (this.bestLapTime === null || lapTime < this.bestLapTime) {
      this.bestLapTime = lapTime;
    }

    this.lapStartTime = currentTime;
    return lapTime;
  }

  /**
   * Get current lap time data.
   */
  getData(currentTime: number, currentLap: number, totalLaps: number): LapTimeData {
    return {
      currentLap: currentLap,
      totalLaps: totalLaps,
      currentLapTime: currentTime - this.lapStartTime,
      bestLapTime: this.bestLapTime,
      lapTimes: this.lapTimes.slice(),
      totalTime: currentTime - this.raceStartTime
    };
  }

  /**
   * Format time for display (M:SS.mm)
   */
  static format(seconds: number): string {
    if (seconds === null || seconds === undefined) return "--:--.--";

    var mins = Math.floor(seconds / 60);
    var secs = Math.floor(seconds % 60);
    var ms = Math.floor((seconds % 1) * 100);

    var secStr = secs < 10 ? "0" + secs : "" + secs;
    var msStr = ms < 10 ? "0" + ms : "" + ms;

    return mins + ":" + secStr + "." + msStr;
  }
}
