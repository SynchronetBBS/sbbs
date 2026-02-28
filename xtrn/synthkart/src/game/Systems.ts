/**
 * Systems - Game system interfaces and utilities.
 */

interface ISystem {
  init(state: GameState): void;
  update(state: GameState, dt: number): void;
}

/**
 * Physics system - updates vehicle physics using pseudo-3D model.
 */
class PhysicsSystem implements ISystem {
  init(_state: GameState): void {
    // Nothing to initialize
  }

  update(state: GameState, dt: number): void {
    for (var i = 0; i < state.vehicles.length; i++) {
      var vehicle = state.vehicles[i];
      if (!vehicle.active) continue;

      // Get driver intent
      var intent: DriverIntent = { accelerate: 0, steer: 0, useItem: false };
      if (vehicle.driver) {
        intent = vehicle.driver.update(vehicle, state.track, dt);
      }

      // Update vehicle physics with road data
      vehicle.updatePhysics(state.road, intent, dt);
    }
  }
}

/**
 * Race system - tracks lap progress and positions using trackZ.
 */
class RaceSystem implements ISystem {
  // Track previous Z position to detect lap boundary crossing
  private lastTrackZ: { [vehicleId: number]: number };

  constructor() {
    this.lastTrackZ = {};
  }

  init(state: GameState): void {
    // Initialize Z tracking for each vehicle
    for (var i = 0; i < state.vehicles.length; i++) {
      var vehicle = state.vehicles[i];
      this.lastTrackZ[vehicle.id] = vehicle.trackZ || 0;
    }
  }

  update(state: GameState, _dt: number): void {
    // Don't process if race is already finished
    if (state.finished) return;

    var roadLength = state.road.totalLength;

    for (var i = 0; i < state.vehicles.length; i++) {
      var vehicle = state.vehicles[i];
      
      // Skip crashed or inactive vehicles
      if (vehicle.isCrashed || !vehicle.active) continue;

      var lastZ = this.lastTrackZ[vehicle.id] || 0;
      var currentZ = vehicle.trackZ || 0;

      // Detect lap completion: when trackZ wraps from near end back to near start
      // This happens when currentZ < lastZ by a large margin (more than half the track)
      // This means we crossed the start/finish line going forward
      var crossedFinishLine = (lastZ > roadLength * 0.75 && currentZ < roadLength * 0.25);
      
      if (crossedFinishLine) {
        vehicle.lap++;
        debugLog.info("LAP COMPLETE! Vehicle " + vehicle.id + " now on lap " + vehicle.lap + "/" + state.track.laps);
        debugLog.info("  lastZ=" + lastZ.toFixed(1) + " currentZ=" + currentZ.toFixed(1) + " roadLength=" + roadLength);

        // Track lap time for player
        if (!vehicle.isNPC) {
          var lapTime = state.time - state.lapStartTime;
          state.lapTimes.push(lapTime);
          
          // Update best lap time
          if (state.bestLapTime < 0 || lapTime < state.bestLapTime) {
            state.bestLapTime = lapTime;
          }
          
          // Reset lap start time for next lap
          state.lapStartTime = state.time;
          
          debugLog.info("  Lap time: " + lapTime.toFixed(2) + " (best: " + state.bestLapTime.toFixed(2) + ")");
        }

        // Check if race is complete (only for player, not NPCs)
        if (!vehicle.isNPC && vehicle.lap > state.track.laps) {
          state.finished = true;
          state.racing = false;
          debugLog.info("RACE FINISHED! Final time: " + state.time.toFixed(2));
        }
      }

      // Update last position
      this.lastTrackZ[vehicle.id] = currentZ;
    }

    // Update race positions
    PositionIndicator.calculatePositions(state.vehicles);
  }
}
