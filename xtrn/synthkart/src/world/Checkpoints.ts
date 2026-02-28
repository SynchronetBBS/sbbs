/**
 * Checkpoints - Checkpoint tracking for lap validation.
 */

class CheckpointTracker {
  private track: ITrack;
  private vehicleCheckpoints: { [vehicleId: number]: number };

  constructor(track: ITrack) {
    this.track = track;
    this.vehicleCheckpoints = {};
  }

  /**
   * Initialize tracking for a vehicle.
   */
  initVehicle(vehicleId: number): void {
    this.vehicleCheckpoints[vehicleId] = 0;
  }

  /**
   * Check if vehicle has passed next checkpoint.
   * Returns true if checkpoint was crossed.
   */
  checkProgress(vehicle: IVehicle): boolean {
    var currentCheckpoint = this.vehicleCheckpoints[vehicle.id] || 0;
    var nextCheckpoint = this.track.checkpoints[currentCheckpoint];

    if (!nextCheckpoint) {
      // All checkpoints passed
      return false;
    }

    // Check if vehicle has passed the checkpoint Z position
    var wrappedZ = vehicle.z % this.track.length;
    if (wrappedZ >= nextCheckpoint.z) {
      this.vehicleCheckpoints[vehicle.id] = currentCheckpoint + 1;
      vehicle.checkpoint = currentCheckpoint + 1;

      // Check for lap completion
      if (currentCheckpoint + 1 >= this.track.checkpoints.length) {
        this.vehicleCheckpoints[vehicle.id] = 0;
        return true; // Lap completed
      }
    }

    return false;
  }

  /**
   * Get current checkpoint index for a vehicle.
   */
  getCurrentCheckpoint(vehicleId: number): number {
    return this.vehicleCheckpoints[vehicleId] || 0;
  }

  /**
   * Reset all checkpoint progress.
   */
  reset(): void {
    this.vehicleCheckpoints = {};
  }
}
