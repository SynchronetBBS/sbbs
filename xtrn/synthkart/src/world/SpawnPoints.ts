/**
 * SpawnPoints - Handles vehicle spawn positioning.
 */

class SpawnPointManager {
  private track: ITrack;

  constructor(track: ITrack) {
    this.track = track;
  }

  /**
   * Get spawn position for a vehicle by grid position (0-indexed).
   */
  getSpawnPosition(gridPosition: number): { x: number; z: number } {
    if (gridPosition < this.track.spawnPoints.length) {
      var sp = this.track.spawnPoints[gridPosition];
      return { x: sp.x, z: sp.z };
    }

    // Generate additional spawn points if needed
    var row = Math.floor(gridPosition / 2);
    var col = gridPosition % 2;
    return {
      x: col === 0 ? -5 : 5,
      z: -(row + 1) * 15
    };
  }

  /**
   * Place a vehicle at a spawn point.
   */
  placeVehicle(vehicle: IVehicle, gridPosition: number): void {
    var pos = this.getSpawnPosition(gridPosition);
    vehicle.x = pos.x;
    vehicle.z = pos.z;
    vehicle.rotation = 0;
    vehicle.speed = 0;
  }

  /**
   * Place multiple vehicles on the starting grid.
   */
  placeVehicles(vehicles: IVehicle[]): void {
    for (var i = 0; i < vehicles.length; i++) {
      this.placeVehicle(vehicles[i], i);
    }
  }
}
