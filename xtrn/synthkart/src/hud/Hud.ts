/**
 * HUD data computation.
 * Calculates values to display; actual rendering is in HudRenderer.
 */

interface HudData {
  speed: number;
  speedMax: number;
  lap: number;
  totalLaps: number;
  lapProgress: number;  // 0.0 to 1.0 - how far through current lap
  position: number;
  totalRacers: number;
  lapTime: number;
  bestLapTime: number;
  totalTime: number;
  heldItem: HeldItemData | null;  // Held item with type and uses count
  raceFinished: boolean;
  countdown: number;       // Seconds until race starts (0 = started)
  raceMode: RaceMode;      // Current race mode
}

class Hud {
  private startTime: number;
  private lapStartTime: number;
  private bestLapTime: number;

  constructor() {
    this.startTime = 0;
    this.lapStartTime = 0;
    this.bestLapTime = Infinity;
  }

  /**
   * Initialize HUD for race start.
   */
  init(currentTime: number): void {
    this.startTime = currentTime;
    this.lapStartTime = currentTime;
    this.bestLapTime = Infinity;
  }

  /**
   * Called when a new lap starts.
   */
  onLapComplete(currentTime: number): void {
    var lapTime = currentTime - this.lapStartTime;
    if (lapTime < this.bestLapTime) {
      this.bestLapTime = lapTime;
    }
    this.lapStartTime = currentTime;
  }

  /**
   * Compute HUD data from game state.
   */
  compute(vehicle: IVehicle, track: ITrack, road: Road, vehicles: IVehicle[], currentTime: number, countdown?: number, raceMode?: RaceMode): HudData {
    // Calculate lap progress (0.0 to 1.0)
    var lapProgress = 0;
    if (road.totalLength > 0) {
      lapProgress = (vehicle.trackZ % road.totalLength) / road.totalLength;
      if (lapProgress < 0) lapProgress += 1.0;
    }

    // Count only actual racers (not commuter NPCs)
    var racers = vehicles.filter(function(v) { return !v.isNPC || v.isRacer; });
    
    // During countdown, show 0:00.00 for times
    // Also ensure we never show negative times
    var isCountdown = (countdown !== undefined && countdown > 0);
    var lapTime = currentTime - this.lapStartTime;
    var totalTime = currentTime - this.startTime;
    
    // Clamp to zero - never show negative
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
  }

  /**
   * Format time as M:SS.mm
   */
  static formatTime(seconds: number): string {
    var mins = Math.floor(seconds / 60);
    var secs = seconds % 60;
    var secStr = secs < 10 ? "0" + secs.toFixed(2) : secs.toFixed(2);
    return mins + ":" + secStr;
  }
}
